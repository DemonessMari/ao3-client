#pragma once
#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QPalette>
#include <cmath>

class QWidget;
struct AppSkin {
    QString name="Light", background="#fff8f5", surface="#ffffff", text="#292723", accent="#920b19", onAccent="#ffffff";
    QByteArray json() const { return QJsonDocument(QJsonObject{{"format","ao3-client-skin"},{"version",1},{"name",name},{"background",background},{"surface",surface},{"text",text},{"accent",accent},{"onAccent",onAccent}}).toJson(); }
    static AppSkin preset(int index) {
        if(index==1)return {"Dark","#191b1e","#25272b","#e7e1d8","#ffb3b9","#350009"};
        if(index==2)return {"Sepia","#f1e3c7","#fff2d9","#292723","#843d21","#ffffff"};
        return {};
    }
    static double luminance(const QString &hex) {
        const QColor c(hex); auto f=[](double v){return v<=0.04045?v/12.92:std::pow((v+0.055)/1.055,2.4);};
        return .2126*f(c.redF())+.7152*f(c.greenF())+.0722*f(c.blueF());
    }
    static double contrast(const QString &a,const QString &b) {double x=luminance(a),y=luminance(b);return (qMax(x,y)+.05)/(qMin(x,y)+.05);}
    static bool parse(const QByteArray &bytes,AppSkin &skin,QString &error) {
        if(bytes.size()>16384){error="Skin files must be smaller than 16 KB.";return false;}
        QJsonParseError e;const auto doc=QJsonDocument::fromJson(bytes,&e);const auto o=doc.object();
        if(e.error!=QJsonParseError::NoError || !doc.isObject() || o.value("format").toString()!="ao3-client-skin" || o.value("version").toDouble()!=1){error="Choose an AO3 Client skin JSON file (version 1).";return false;}
        const QStringList keys={"format","version","name","background","surface","text","accent","onAccent"};
        for(auto it=o.begin();it!=o.end();++it)if(!keys.contains(it.key())){error="Unknown skin field: "+it.key();return false;}
        AppSkin candidate;candidate.name=o.value("name").toString().trimmed();
        if(candidate.name.isEmpty() || candidate.name.size()>60 || candidate.name.contains(QRegularExpression("[\\x00-\\x1f\\x7f]"))){error="Give the skin a name of 1–60 characters.";return false;}
        for(auto pair:{qMakePair(QString("background"),&candidate.background),qMakePair(QString("surface"),&candidate.surface),qMakePair(QString("text"),&candidate.text),qMakePair(QString("accent"),&candidate.accent),qMakePair(QString("onAccent"),&candidate.onAccent)}){
            const auto value=o.value(pair.first).toString();if(!QRegularExpression("^#[0-9a-fA-F]{6}$").match(value).hasMatch()){error="Use opaque #RRGGBB colors for "+pair.first+".";return false;}*pair.second=value.toLower();
        }
        if(contrast(candidate.text,candidate.background)<4.5 || contrast(candidate.text,candidate.surface)<4.5 || contrast(candidate.onAccent,candidate.accent)<4.5 || contrast(candidate.accent,candidate.background)<4.5 || contrast(candidate.accent,candidate.surface)<4.5){error="Increase contrast: text and accents need 4.5:1 against their backgrounds.";return false;}
        skin=candidate;error.clear();return true;
    }
    QPalette palette() const {
        QPalette p;const QColor bg(background),base(surface),fg(text),highlight(accent),on(onAccent);
        for(auto group:{QPalette::Active,QPalette::Inactive,QPalette::Disabled}){
            for(auto role:{QPalette::WindowText,QPalette::Text,QPalette::ButtonText,QPalette::PlaceholderText,QPalette::ToolTipText})p.setColor(group,role,fg);
            for(auto role:{QPalette::Window,QPalette::Button,QPalette::AlternateBase})p.setColor(group,role,bg);
            p.setColor(group,QPalette::Base,base);p.setColor(group,QPalette::ToolTipBase,base);p.setColor(group,QPalette::Highlight,highlight);p.setColor(group,QPalette::HighlightedText,on);p.setColor(group,QPalette::Link,highlight);p.setColor(group,QPalette::LinkVisited,highlight);
            p.setColor(group,QPalette::Mid, QColor::fromRgbF((bg.redF()+fg.redF())/2,(bg.greenF()+fg.greenF())/2,(bg.blueF()+fg.blueF())/2));
        }
        return p;
    }
};
inline bool customAppSkin(AppSkin &skin) {QString error;return AppSkin::parse(QSettings().value("appearance/skin").toByteArray(),skin,error);}
void applyAppSkin();
void editAppSkin(QWidget *parent);
