#pragma once
#include <QWidget>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QVariantAnimation>
#include <functional>
#include <algorithm>
#include <cmath>
#include <QImage>
#include <QPainterPath>
#include <QTextBlock>
#include <QTextLayout>

class BookView : public QWidget {
public:
    explicit BookView(QWidget *parent = nullptr) : QWidget(parent), animation(this) {
        setFocusPolicy(Qt::StrongFocus);
        animation.setDuration(420);
        connect(&animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) { phase = v.toDouble(); update(); });
        connect(&animation, &QVariantAnimation::finished, this, [this] { phase = 1; update(); });
    }
    std::function<void(double)> positionChanged;
    std::function<void()> centerClicked;
    std::function<void(int)> chapterRequested;
    std::function<void()> pageChanged;
    bool animated = true;
    int page() const { return current; }
    int pages() const { return doc ? qMax(1, doc->pageCount()) : 1; }
    int anchor() const {
        if(!doc) return 0;
        const double top=current*doc->pageSize().height();
        for(auto block=doc->begin();block.isValid();block=block.next()) {
            const auto rect=doc->documentLayout()->blockBoundingRect(block); const auto layout=block.layout();
            for(int i=0;layout && i<layout->lineCount();++i) { const auto line=layout->lineAt(i); if(rect.top()+line.y()+line.height()>top) return block.position()+line.textStart(); }
        }
        return qMax(0,doc->characterCount()-1);
    }
    void restoreAnchor(int offset) {
        if(!doc || offset<0) return;
        const auto block=doc->findBlock(qMin(offset,doc->characterCount()-1)); if(!block.isValid()) return;
        const auto layout=block.layout(); const auto line=layout?layout->lineForTextPosition(offset-block.position()):QTextLine();
        const double y=doc->documentLayout()->blockBoundingRect(block).top()+(line.isValid()?line.y():0);
        current=qBound(0,int(y/doc->pageSize().height()),pages()-1); update(); if(pageChanged) pageChanged();
    }
    double fraction() const { return pages() > 1 ? double(current) / (pages()-1) : 0; }
    void setContent(const QTextDocument *source, double fraction, QColor bg, QColor fg) {
        animation.stop(); phase = 1; delete doc; doc = source->clone(this);
        background = bg; foreground = fg; doc->setDocumentMargin(24);
        doc->setPageSize(QSizeF(qMax(100,width()), qMax(100,height())));
        current = qRound(qBound(0.0,fraction,1.0)*(pages()-1)); update(); if(pageChanged) pageChanged();
    }
    void turn(int delta) {
        const int target = qBound(0,current+delta,pages()-1); if(target == current) { if(chapterRequested) chapterRequested(delta); return; }
        animation.stop(); previous = current; current = target;
        if(animated) { leaf=QImage(size(),QImage::Format_ARGB32_Premultiplied); leaf.fill(background); QPainter snapshot(&leaf); drawPage(snapshot,current>previous?previous:current); }
        if(animated) { animation.setStartValue(0.0); animation.setEndValue(1.0); animation.start(); } else { phase=1; update(); }
        if(positionChanged) positionChanged(fraction());
        if(pageChanged) pageChanged();
    }
protected:
    void resizeEvent(QResizeEvent *) override {
        animation.stop(); phase=1; const int saved=anchor(); if(doc) { doc->setPageSize(QSizeF(qMax(100,width()), qMax(100,height()))); restoreAnchor(saved); } update(); if(pageChanged) pageChanged();
    }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this); painter.fillRect(rect(),background); if(!doc) return;
        const bool flipping = animation.state() != QAbstractAnimation::Stopped;
        painter.setClipRect(rect());
        if(!flipping) { drawPage(painter,current); return; }
        const double p=current>previous?phase:1-phase;
        drawPage(painter,current>previous?current:previous);
        if(p>=.9999) return;
        if(p<=.0001) { painter.drawImage(0,0,leaf); return; }
        const double w=width(),h=height(),angle=.30*std::sin(3.14159265*p),ca=std::cos(angle),sa=std::sin(angle);
        const double span=w*ca+h*sa,tall=h*ca+w*sa,minX=(w-span)/2,minY=(h-tall)/2;
        const double radius=qMax(.001,span*.12*std::sin(3.14159265*p)),crease=minX+span*(1-p)-3.14159265*radius/2;
        struct Point { double x,z,normal; };
        const auto curled=[&](double x) { const double u=x-crease;
            if(u<=0) return Point{x,0,1};
            if(u<3.14159265*radius) { const double a=u/radius; return Point{crease+radius*std::sin(a),radius*(1-std::cos(a)),std::cos(a)}; }
            return Point{crease-(u-3.14159265*radius),2*radius,-1}; };
        const auto flat=[&](double x,double y) { return QPointF(w/2+(x-w/2)*ca-(y-h/2)*sa,h/2+(x-w/2)*sa+(y-h/2)*ca); };
        const auto projected=[&](double x,double y) { const Point v=curled(x); const QPointF point=flat(v.x,y); const double scale=1+v.z/(span*8); return QPointF(w/2+(point.x()-w/2)*scale,h/2+(point.y()-h/2)*scale); };
        const auto quad=[&](double x0,double x1,bool project) { QPolygonF result; for(const auto &xy:QPolygonF{QPointF(x0,minY),QPointF(x1,minY),QPointF(x1,minY+tall),QPointF(x0,minY+tall)}) result.append(project?projected(xy.x(),xy.y()):flat(xy.x(),xy.y())); return result; };
        const double tangent=crease+3.14159265*radius/2; const QPointF edge=projected(tangent,h/2),vector=QPointF(ca,sa)*(span*.035),end=edge+vector;
        QLinearGradient shadow(edge,end); shadow.setColorAt(0,QColor(0,0,0,32)); shadow.setColorAt(1,Qt::transparent);
        painter.setPen(Qt::NoPen); painter.setBrush(shadow); painter.drawPolygon(QPolygonF{projected(tangent,minY),projected(tangent,minY)+vector,projected(tangent,minY+tall)+vector,projected(tangent,minY+tall)});
        struct Strip { QTransform transform; QPainterPath clip; double normal; };
        QVector<Strip> strips;
        QPainterPath paperMask,reverseMask; paperMask.setFillRule(Qt::WindingFill); reverseMask.setFillRule(Qt::WindingFill);
        const auto clipPage=[&](QPolygonF polygon) {
            for(int side=0;side<4 && !polygon.isEmpty();++side) {
                QPolygonF output; const int axis=side/2; const double bound=side%2==0?0:(axis==0?w:h);
                const auto coordinate=[&](QPointF point) { return axis==0?point.x():point.y(); };
                const auto inside=[&](QPointF point) { return side%2==0?coordinate(point)>=bound:coordinate(point)<=bound; };
                QPointF previous=polygon.last();
                for(const QPointF current:polygon) {
                    if(inside(previous)!=inside(current)) { const double t=(bound-coordinate(previous))/(coordinate(current)-coordinate(previous)); output.append(previous+(current-previous)*t); }
                    if(inside(current)) output.append(current);
                    previous=current;
                }
                polygon=output;
            }
            return polygon;
        };
        const auto addStrip=[&](double x0,double x1,double normal) {
            const QPolygonF source=quad(x0,x1,false),target=quad(x0,x1,true); QTransform transform;
            if(!QTransform::quadToQuad(source,target,transform)) return;
            const QPolygonF clipped=clipPage(source); if(clipped.size()<3) return;
            QPainterPath clip; clip.addPolygon(clipped); clip.closeSubpath();
            QPolygonF visible=transform.map(clipped); double area=0;
            for(int i=0;i<visible.size();++i) { const auto a=visible[i],b=visible[(i+1)%visible.size()]; area+=a.x()*b.y()-b.x()*a.y(); }
            if(area<0) std::reverse(visible.begin(),visible.end());
            paperMask.addPolygon(visible); paperMask.closeSubpath(); if(normal<=0) { reverseMask.addPolygon(visible); reverseMask.closeSubpath(); } strips.append({transform,clip,normal});
        };
        if(crease>minX) addStrip(minX,crease,1);
        QVector<double> stops; for(int i=0;i<=64;++i) stops.append(crease+(minX+span-crease)*i/64);
        for(double stop:{tangent,crease+3.14159265*radius}) if(stop>=crease && stop<=minX+span) stops.append(stop);
        std::sort(stops.begin(),stops.end()); for(int i=0;i+1<stops.size();++i) if(stops[i+1]>stops[i]) addStrip(stops[i],stops[i+1],(curled(stops[i]).normal+curled(stops[i+1]).normal)/2);
        // One continuous opaque sheet under all mesh strips prevents seam bleed.
        QColor opaquePaper=background; opaquePaper.setAlpha(255);
        painter.fillPath(paperMask,opaquePaper); lastPaperMask=paperMask;
        for(const auto &strip:strips) {
            if(strip.normal<=0) continue;
            painter.save(); painter.setTransform(strip.transform,true); painter.setClipPath(strip.clip,Qt::IntersectClip);
            if(strip.normal>0) { painter.fillRect(rect(),opaquePaper); painter.drawImage(0,0,leaf); if(strip.normal<1) painter.fillRect(rect(),QColor(0,0,0,qRound((1-strip.normal)*.24*255))); }
            else { const double mix=.08+.18*(1-std::abs(strip.normal)); QColor paper;
                paper.setRgbF(background.redF()*(1-mix)+foreground.redF()*mix,background.greenF()*(1-mix)+foreground.greenF()*mix,background.blueF()*(1-mix)+foreground.blueF()*mix); painter.fillRect(rect(),paper); }
            painter.restore();
        }
        const auto shade=[&](double mix) { return QColor::fromRgbF(background.redF()*(1-mix)+foreground.redF()*mix,background.greenF()*(1-mix)+foreground.greenF()*mix,background.blueF()*(1-mix)+foreground.blueF()*mix,1); };
        QLinearGradient reverse(flat(crease,h/2),flat(crease+radius,h/2)); reverse.setColorAt(0,shade(.04)); reverse.setColorAt(1,shade(.14));
        painter.setPen(QPen(QBrush(reverse),2)); painter.setBrush(reverse); painter.drawPath(reverseMask);
    }

    void wheelEvent(QWheelEvent *e) override { if(e->angleDelta().y()) turn(e->angleDelta().y()<0?1:-1); e->accept(); }
    void mouseReleaseEvent(QMouseEvent *e) override { if(e->position().x()<width()*.3) turn(-1); else if(e->position().x()>width()*.7) turn(1); else if(centerClicked) centerClicked(); }
    void keyPressEvent(QKeyEvent *e) override { if(e->key()==Qt::Key_Right || e->key()==Qt::Key_PageDown || e->key()==Qt::Key_Space) turn(1); else if(e->key()==Qt::Key_Left || e->key()==Qt::Key_PageUp) turn(-1); else QWidget::keyPressEvent(e); }
private:
    friend class FunctionalTest;
    void drawPage(QPainter &painter,int index) {
        painter.save(); painter.fillRect(rect(),background); painter.translate(0,-index*doc->pageSize().height());
        QAbstractTextDocumentLayout::PaintContext context; context.palette.setColor(QPalette::Text,foreground);
        context.clip=QRectF(0,index*doc->pageSize().height(),width(),height()); doc->documentLayout()->draw(&painter,context); painter.restore();
    }
    QPainterPath lastPaperMask;
    QImage leaf;
    QTextDocument *doc = nullptr;
    QVariantAnimation animation;
    int current=0,previous=0; double phase=1;
    QColor background=Qt::white,foreground=Qt::black;
};
