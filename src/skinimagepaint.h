#pragma once
#include "appskin.h"
#include <QPainter>
#include <QPainterPath>
#include <QImageReader>
#include <QBuffer>
#include <QCache>
#include <QCryptographicHash>

inline QImage decodedSkinImage(const QJsonObject &asset) {
    static QCache<QByteArray,QImage> cache(16*1024);
    const auto data=asset.value("data").toString().toLatin1();const auto key=QCryptographicHash::hash(data,QCryptographicHash::Sha256);
    if(auto *image=cache.object(key))return *image;
    auto bytes=skinImageBytes(asset);QBuffer buffer(&bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);auto size=reader.size();QImage image;
    if(size.width()>0 && size.height()>0 && size.width()<=4096 && size.height()<=4096 && qint64(size.width())*size.height()<=2097152){if(size.width()>1024 || size.height()>1024)reader.setScaledSize(size.scaled(1024,1024,Qt::KeepAspectRatio).expandedTo(QSize(1,1)));image=reader.read();}
    cache.insert(key,new QImage(image),qMax(1,int(image.sizeInBytes()/1024)));return image;
}
inline double safeSkinOpacity(const QString &background,const QString &text,double requested) {
    const QColor bg(background);
    auto safe=[&](double opacity){for(int edge:{0,255}){const auto blend=QColor(qRound(bg.red()*(1-opacity)+edge*opacity),qRound(bg.green()*(1-opacity)+edge*opacity),qRound(bg.blue()*(1-opacity)+edge*opacity)).name();if(AppSkin::contrast(blend,text)<4.5)return false;}return true;};
    requested=std::floor(requested*255)/255;
    if(safe(requested))return requested;double low=0,high=requested;for(int i=0;i<18;++i){const double mid=(low+high)/2;if(safe(mid))low=mid;else high=mid;}return std::floor(low*255)/255;
}
inline int skinBannerHeight(const AppSkin &skin,double scale=1) {for(const auto &v:skin.layers){const auto l=v.toObject();if(l.value("target")=="header" && l.value("placement")=="banner")return qRound(l.value("height").toInt()*scale);}return 0;}
inline void drawSkinLayers(QPainter &p,const AppSkin &skin,const QString &target,const QRectF &bounds,const QString &background,const QString &foreground,double scale=1,bool banner=false) {
    if(bounds.width()<=0 || bounds.height()<=0)return;
    p.save();p.setClipRect(bounds,Qt::IntersectClip);p.setRenderHint(QPainter::SmoothPixmapTransform);
    int count=0;for(const auto &v:skin.layers){const auto l=v.toObject();if(l.value("target")==target && l.value("placement")!="banner")++count;}
    double budget=safeSkinOpacity(background,foreground,1);if(target=="window")budget=qMin(budget,safeSkinOpacity(background,skin.accent,1));
    const double layerLimit=std::floor(budget/qMax(1,count)*255)/255;
    for(const auto &placement:banner?QStringList{"banner"}:QStringList{"background","overlay"})for(const auto &v:skin.layers){
        const auto layer=v.toObject();if(layer.value("target")!=target || layer.value("placement")!=placement)continue;
        const auto image=decodedSkinImage(skin.images.value(layer.value("image").toString()).toObject());if(image.isNull())continue;
        double opacity=layer.value("opacity").toDouble();if(!banner){opacity=safeSkinOpacity(background,foreground,opacity);if(target=="window")opacity=qMin(opacity,safeSkinOpacity(background,skin.accent,opacity));opacity=qMin(opacity,layerLimit);}p.setOpacity(opacity);
        const auto fit=layer.value("fit").toString();double width=layer.value("width").toInt()*scale,height=layer.value("height").toInt()*scale;
        const double dx=layer.value("offsetX").toInt()*scale,dy=layer.value("offsetY").toInt()*scale;
        auto paint=[&](const QRectF &rect){QRectF source(image.rect()),destination=rect;if(fit=="cover"){const double factor=qMax(rect.width()/image.width(),rect.height()/image.height());source=QRectF((image.width()-rect.width()/factor)/2,(image.height()-rect.height()/factor)/2,rect.width()/factor,rect.height()/factor);}else if(fit=="contain"){const auto size=QSizeF(image.size()).scaled(rect.size(),Qt::KeepAspectRatio);destination=QRectF(rect.center()-QPointF(size.width()/2,size.height()/2),size);}p.drawImage(destination,image,source);};
        if(banner){paint(bounds);continue;}
        if(fit=="tile"){
            const double count=std::ceil(bounds.width()/width+2)*std::ceil(bounds.height()/height+2);if(count>1024){const double factor=std::sqrt(count/1024);width*=factor;height*=factor;}
            const double x=bounds.left()+std::fmod(dx,width)-width,y=bounds.top()+std::fmod(dy,height)-height;
            for(double yy=y;yy<bounds.bottom();yy+=height)for(double xx=x;xx<bounds.right();xx+=width)p.drawImage(QRectF(xx,yy,width,height),image);continue;
        }
        const auto anchor=layer.value("anchor").toString();double x=bounds.left(),y=bounds.top();if(anchor.contains("right"))x=bounds.right()-width;else if(anchor=="center")x=bounds.center().x()-width/2;if(anchor.contains("bottom"))y=bounds.bottom()-height;else if(anchor=="center")y=bounds.center().y()-height/2;
        paint(QRectF(x+dx,y+dy,width,height));
    }
    p.restore();
}
