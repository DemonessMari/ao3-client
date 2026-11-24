#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSize>
#include <QtEndian>
#include <cmath>

inline constexpr int skinMaxFileBytes=4*1024*1024;
inline QSize skinImageSize(const QByteArray &data) {
    auto byte=[&](int i){return quint8(data[i]);};
    if(data.size()>=33 && data.left(8)==QByteArray::fromHex("89504e470d0a1a0a") && data.mid(12,4)=="IHDR" && data.mid(data.size()-8,4)=="IEND")return QSize(qFromBigEndian<quint32>(data.constData()+16),qFromBigEndian<quint32>(data.constData()+20));
    if(data.size()<12 || byte(0)!=255 || byte(1)!=216 || byte(data.size()-2)!=255 || byte(data.size()-1)!=217)return {};
    int p=2;
    while(p+3<data.size()){
        if(byte(p++)!=255)return {};while(p<data.size() && byte(p)==255)++p;if(p>=data.size())return {};
        const int marker=byte(p++);if(marker==218 || marker==217)break;if(marker==1 || (marker>=208 && marker<=215))continue;
        if(p+2>data.size())return {};const int length=(byte(p)<<8)|byte(p+1);if(length<2 || p+length>data.size())return {};
        if((marker>=192 && marker<=195)||(marker>=197 && marker<=199)||(marker>=201 && marker<=203)||(marker>=205 && marker<=207)){if(length<8)return {};return QSize((byte(p+5)<<8)|byte(p+6),(byte(p+3)<<8)|byte(p+4));}p+=length;
    }
    return {};
}
inline QByteArray skinImageBytes(const QJsonObject &asset) {
    const auto data=asset.value("data").toString();const auto comma=data.indexOf(',');return comma>=0?QByteArray::fromBase64(data.mid(comma+1).toLatin1(),QByteArray::AbortOnBase64DecodingErrors):QByteArray();
}
inline bool validateSkinImages(const QJsonObject &root,QString &error) {
    auto fail=[&](const QString &message){error=message;return false;};
    if(!root.value("images").isObject() || !root.value("layers").isArray())return fail("Images and layers are required in version 4.");
    const auto images=root.value("images").toObject();const auto layers=root.value("layers").toArray();
    if(images.size()>4 || layers.size()>8)return fail("Use at most four images and eight layers.");
    for(auto it=images.begin();it!=images.end();++it){
        if(!QRegularExpression("^[a-z][a-z0-9_-]{0,31}$").match(it.key()).hasMatch() || !it.value().isObject())return fail("Invalid image name or object.");
        const auto asset=it.value().toObject();for(auto field=asset.begin();field!=asset.end();++field)if(!QStringList{"data","credit","source","license"}.contains(field.key()))return fail("Unknown image field: "+field.key());
        for(const auto &key:QStringList{"credit","source","license"})if(!asset.value(key).isString() || asset.value(key).toString().size()>1024)return fail("Image credits must be text up to 1024 characters.");
        const auto data=asset.value("data").toString();if(!QRegularExpression("^data:image/(png|jpeg);base64,[A-Za-z0-9+/]+={0,2}$").match(data).hasMatch())return fail("Use embedded PNG or JPEG images.");
        const auto bytes=skinImageBytes(asset);const auto size=skinImageSize(bytes);
        if(bytes.isEmpty() || bytes.size()>512*1024 || size.width()<=0 || size.height()<=0 || size.width()>4096 || size.height()>4096 || qint64(size.width())*size.height()>2097152)return fail("Each image must be valid, at most 512 KB and two megapixels.");
        if(data.startsWith("data:image/png") != bytes.startsWith(QByteArray::fromHex("89504e47")))return fail("Image type does not match its contents.");
    }
    int banners=0;
    for(const auto &item:layers){
        if(!item.isObject())return fail("Layers must be objects.");const auto layer=item.toObject();
        for(auto f=layer.begin();f!=layer.end();++f)if(!QStringList{"image","target","placement","fit","anchor","width","height","opacity","offsetX","offsetY"}.contains(f.key()))return fail("Unknown layer field: "+f.key());
        if(!images.contains(layer.value("image").toString()))return fail("A layer refers to a missing image.");
        if(!QStringList{"window","header","cards"}.contains(layer.value("target").toString()) || !QStringList{"background","overlay","banner"}.contains(layer.value("placement").toString()) || !QStringList{"contain","cover","tile"}.contains(layer.value("fit").toString()) || !QStringList{"top-left","top-right","bottom-left","bottom-right","center"}.contains(layer.value("anchor").toString()))return fail("Invalid layer target, placement, fit or anchor.");
        if(layer.value("placement")=="banner" && (layer.value("target")!="header" || ++banners>1 || layer.value("fit")=="tile"))return fail("Use one non-tiled header banner.");
        for(const auto &key:QStringList{"width","height","opacity","offsetX","offsetY"}){
            const auto value=layer.value(key);const double n=value.toDouble(-99999);const double low=key=="opacity"?0:key.startsWith("offset")?-256:16;const double high=key=="opacity"?1:key.startsWith("offset")?256:key=="height" && layer.value("placement")=="banner"?240:1024;
            if(!value.isDouble() || !std::isfinite(n) || n<low || n>high || (key!="opacity" && n!=std::floor(n)))return fail("Layer size, position or opacity is outside its allowed range.");
        }
    }
    return true;
}
