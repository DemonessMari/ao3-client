#pragma once
#include <QLayout>
#include <QWidgetItem>

class FlowLayout final : public QLayout {
public:
    explicit FlowLayout(QWidget *parent):QLayout(parent) {}
    ~FlowLayout() override {while(auto *item=takeAt(0))delete item;}
    void addItem(QLayoutItem *item) override {items.append(item);}
    int count() const override {return items.size();}
    QLayoutItem *itemAt(int index) const override {return items.value(index);}
    QLayoutItem *takeAt(int index) override {return index>=0 && index<items.size()?items.takeAt(index):nullptr;}
    Qt::Orientations expandingDirections() const override {return {};}
    bool hasHeightForWidth() const override {return true;}
    int heightForWidth(int width) const override {return arrange(QRect(0,0,width,0),false);}
    QSize sizeHint() const override {return minimumSize();}
    QSize minimumSize() const override {
        QSize size;for(auto *item:items)if(!item->isEmpty())size=size.expandedTo(item->minimumSize());
        const auto margins=contentsMargins();return size+QSize(margins.left()+margins.right(),margins.top()+margins.bottom());
    }
    void setGeometry(const QRect &rect) override {QLayout::setGeometry(rect);arrange(rect,true);}
private:
    QList<QLayoutItem *> items;
    int arrange(const QRect &rect,bool apply) const {
        const auto margins=contentsMargins();const auto area=rect.marginsRemoved(margins);
        int x=area.x(),y=area.y(),line=0;const int gap=qMax(0,spacing());
        for(auto *item:items) {
            if(item->isEmpty())continue;
            const auto size=item->sizeHint();
            if(line>0 && x+size.width()>area.right()+1){x=area.x();y+=line+gap;line=0;}
            if(apply)item->setGeometry(QRect(QPoint(x,y),size));
            x+=size.width()+gap;line=qMax(line,size.height());
        }
        return y-rect.y()+line+margins.bottom();
    }
};
