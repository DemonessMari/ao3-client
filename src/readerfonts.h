#pragma once
#include <algorithm>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QMap>
#include <QRawFont>
#include <QSaveFile>
#include <QStandardPaths>
#include <QRegularExpression>

struct ReaderFont {
    QString id, name, family, style;
    int registration=-1;
};
inline QString readerFontDirectory() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/fonts"; }
inline QMap<QString,ReaderFont> &loadedReaderFonts() { static QMap<QString,ReaderFont> fonts;return fonts; }
inline bool readerFontId(const QString &id) { return QRegularExpression("^font:[0-9a-f]{64}$").match(id).hasMatch(); }
inline ReaderFont loadReaderFont(const QByteArray &bytes) {
    if(bytes.size()<12 || bytes.size()>20*1024*1024 || (bytes.left(4)!=QByteArray::fromHex("00010000") && bytes.left(4)!="OTTO"))return {};
    QRawFont raw(bytes,16,QFont::PreferDefaultHinting);if(!raw.isValid())return {};
    const int registration=-1;
    const auto family=raw.familyName(),style=raw.styleName();
    return {"font:"+QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()),family+(style.isEmpty()||style=="Regular"?QString():" · "+style),family,style,registration};
}
inline QList<ReaderFont> readerFonts() {
    QList<ReaderFont> result;QDir directory(readerFontDirectory());
    for(const auto &name:directory.entryList({"*.font"},QDir::Files,QDir::Name)) {
        const QString id="font:"+name.chopped(5);if(!readerFontId(id))continue;
        const auto path=directory.filePath(name);auto &cache=loadedReaderFonts();
        if(!cache.contains(path)) {QFile file(path);if(!file.open(QIODevice::ReadOnly))continue;const auto bytes=file.read(20*1024*1024+1);auto font=loadReaderFont(bytes);if(font.id!=id){if(font.registration>=0)QFontDatabase::removeApplicationFont(font.registration);continue;}cache.insert(path,font);}
        result.append(cache.value(path));if(result.size()>=20)break;
    }
    std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){return a.name.localeAwareCompare(b.name)<0;});return result;
}
inline QString importReaderFont(const QByteArray &bytes,QString &error) {
    if(bytes.size()>20*1024*1024){error="Choose a font smaller than 20 MB.";return {};}
    const auto id="font:"+QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
    const auto fonts=readerFonts();for(const auto &font:fonts)if(font.id==id)return id;
    if(fonts.size()>=20){error="Remove an imported font before adding another (20 maximum).";return {};}
    auto font=loadReaderFont(bytes);if(font.id.isEmpty()){error="Choose a valid TrueType (.ttf) or OpenType (.otf) font.";return {};}
    QDir directory(readerFontDirectory());const auto path=directory.filePath(font.id.mid(5)+".font");QSaveFile file(path);
    if(!directory.mkpath(".") || !file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()){if(font.registration>=0)QFontDatabase::removeApplicationFont(font.registration);error="Could not save the font. Check storage and permissions.";return {};}
    loadedReaderFonts().insert(path,font);return font.id;
}
inline bool removeReaderFont(const QString &id) {
    if(!readerFontId(id))return false;
    const auto path=QDir(readerFontDirectory()).filePath(id.mid(5)+".font");
    if(!QFile::remove(path))return false;
    auto &cache=loadedReaderFonts();
    if(cache.contains(path)){const auto font=cache.take(path);if(font.registration>=0)QFontDatabase::removeApplicationFont(font.registration);}
    return true;
}
inline QFont readerFont(const QString &id) {
    if(readerFontId(id))for(const auto &font:readerFonts())if(font.id==id){
        const auto path=QDir(readerFontDirectory()).filePath(id.mid(5)+".font");auto &loaded=loadedReaderFonts()[path];
        if(loaded.registration<0)loaded.registration=QFontDatabase::addApplicationFont(path);
        if(loaded.registration<0)break;
        QFont result(font.family);if(font.style!="Regular" && font.style!="Normal" && font.style!="Book")result.setStyleName(font.style);return result;}
    QFont result(id=="sans-serif"?"sans-serif":"serif");result.setStyleHint(id=="sans-serif"?QFont::SansSerif:QFont::Serif);return result;
}
