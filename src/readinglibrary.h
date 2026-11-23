#pragma once
#include "workmodel.h"
#include "formattedwork.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QDateTime>
#include <QJsonParseError>

class ReadingLibrary {
public:
    explicit ReadingLibrary(QString root = {}) : m_root(root.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/reading-library" : root) {}
    static QJsonObject metadata(const Work &work) {
        return {{"id", work.id}, {"title", work.title}, {"author", work.author}, {"authorUrl", work.authorUrl}, {"fandom", work.fandom}, {"summary", work.summary}, {"language", work.language}, {"rating",work.rating}, {"warnings",work.warnings}};
    }
    static Work work(const QJsonObject &entry) {
        const auto data = entry.value("work").toObject();
        Work result; result.id = data.value("id").toString(); result.title = data.value("title").toString(); result.author = data.value("author").toString(); result.authorUrl = data.value("authorUrl").toString(); result.fandom = data.value("fandom").toString(); result.summary = data.value("summary").toString(); result.language = data.value("language").toString(); result.rating=data.value("rating").toString(); result.warnings=data.value("warnings").toString(); return result;
    }
    QJsonArray entries() const {
        QFile file(m_root + "/index.json");
        if (!file.open(QIODevice::ReadOnly)) return {};
        QJsonParseError error; const auto doc=QJsonDocument::fromJson(file.readAll(),&error); file.close();
        if(error.error==QJsonParseError::NoError && doc.isObject() && doc.object().value("entries").isArray()) return actualEntries(doc.object().value("entries").toArray());
        // Preserve the damaged index and recover from a previous index or cache metadata.
        if(!file.rename(m_root+"/index.corrupt-"+QString::number(QDateTime::currentMSecsSinceEpoch())+".json")) return {};
        QFile previous(m_root+"/index.previous.json");
        if(previous.open(QIODevice::ReadOnly)) { const auto old=QJsonDocument::fromJson(previous.readAll()).object().value("entries"); if(old.isArray()) { restoreIndex(old.toArray()); return actualEntries(old.toArray()); } }
        QJsonArray recovered; for(const auto &name:QDir(m_root).entryList({"*.json"},QDir::Files)) {
            if(!QRegularExpression("^\\d+\\.json$").match(name).hasMatch()) continue;
            QFile saved(m_root+"/"+name); if(saved.open(QIODevice::ReadOnly)) { const auto data=QJsonDocument::fromJson(saved.readAll()).object(); if(valid(data.value("work").toObject().value("id").toString())) recovered.append(QJsonObject{{"work",data.value("work")},{"offline",true}}); }
        }
        restoreIndex(recovered); return recovered;
    }
    bool remember(const Work &work) {
        if (!valid(work.id)) return false;
        auto list = entries(); QJsonArray updated; QJsonObject entry;
        for (const auto &value : list) { const auto item = value.toObject(); if (ReadingLibrary::work(item).id == work.id) entry = item; else updated.append(item); }
        entry["work"] = metadata(work); entry["openedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        entry["offline"] = QFile::exists(cachePath(work.id)); updated.prepend(entry);
        return write(m_root + "/index.json", {{"entries", updated}});
    }
    static QJsonObject chapterMetadata(const WorkChapter &chapter) {
        QJsonArray styles;
        for (const auto &s:chapter.styles) styles.append(QJsonObject{{"start",s.start},{"end",s.end},{"bold",s.bold},{"italic",s.italic},{"underline",s.underline},{"strike",s.strike},{"code",s.code},{"url",s.url}});
        return {{"title",chapter.title},{"text",chapter.text},{"notesBefore",chapter.notesBefore},{"notesAfter",chapter.notesAfter},{"styles",styles},{"html",chapter.html},{"workCss",chapter.workCss}};
    }
    static WorkChapter readChapter(const QJsonObject &chapter) {
        WorkChapter result{chapter.value("title").toString(),chapter.value("text").toString(),chapter.value("notesBefore").toString(),chapter.value("notesAfter").toString()};
        for (const auto &value:chapter.value("styles").toArray()) {
            const auto s=value.toObject(); TextStyleSpan span;
            span.start=s.value("start").toInt(-1); span.end=s.value("end").toInt(-1);
            if (span.start<0 || span.end<=span.start || span.end>result.text.size()) continue;
            span.bold=s.value("bold").toBool(); span.italic=s.value("italic").toBool(); span.underline=s.value("underline").toBool(); span.strike=s.value("strike").toBool(); span.code=s.value("code").toBool();
            if (!readingLink(s.value("url").toString()).isEmpty()) span.url=s.value("url").toString();
            result.styles.append(span);
        }
        result.html=staticReadingHtml(chapter.value("html").toString());
        result.workCss=chapter.value("workCss").toString().left(262144);
        return result;
    }
    bool saveOffline(const Work &work, const QVector<WorkChapter> &chapters) {
        if (!valid(work.id) || chapters.isEmpty()) return false;
        QJsonArray content; for (const auto &chapter : chapters) content.append(chapterMetadata(chapter));
        return write(cachePath(work.id), {{"work", metadata(work)}, {"chapters", content}}) && remember(work);
    }
    QVector<WorkChapter> load(const QString &id) const {
        if (!valid(id)) return {};
        QFile file(cachePath(id)); if (!file.open(QIODevice::ReadOnly)) return {};
        const auto list = QJsonDocument::fromJson(file.readAll()).object().value("chapters").toArray();
        QVector<WorkChapter> result;
        for (const auto &value : list) { const auto item = value.toObject(); if(item.value("text").toString().trimmed().isEmpty()) return {}; result.append(readChapter(item)); }
        return result;
    }
    bool removeOffline(const QString &id) {
        if (!valid(id)) return false;
        if (QFile::exists(cachePath(id)) && !QFile::remove(cachePath(id))) return false;
        return update(id, "offline", false);
    }
    bool favorite(const QString &id, bool value) { return update(id, "favorite", value); }
private:
    QJsonArray actualEntries(const QJsonArray &list) const {
        QJsonArray result; for(const auto &value:list) { auto entry=value.toObject(); const auto id=work(entry).id; if(!valid(id)) continue; entry["offline"]=QFile::exists(cachePath(id)); result.append(entry); } return result;
    }
    void restoreIndex(const QJsonArray &entries) const { QSaveFile file(m_root+"/index.json"); if(file.open(QIODevice::WriteOnly)) { file.setPermissions(QFile::ReadOwner|QFile::WriteOwner); file.write(QJsonDocument(QJsonObject{{"entries",entries}}).toJson(QJsonDocument::Compact)); file.commit(); } }
    static bool valid(const QString &id) { return QRegularExpression("^\\d+$").match(id).hasMatch(); }
    QString cachePath(const QString &id) const { return m_root + "/" + id + ".json"; }
    bool update(const QString &id, const QString &key, bool value) {
        QJsonArray changed; for (const auto &item : entries()) { auto entry = item.toObject(); if (work(entry).id == id) entry[key] = value; changed.append(entry); }
        return write(m_root + "/index.json", {{"entries", changed}});
    }
    bool write(const QString &path, const QJsonObject &object) {
        if (!QDir().mkpath(m_root)) return false;
        if(path.endsWith("/index.json") && QFile::exists(path)) { QFile old(path); if(old.open(QIODevice::ReadOnly)) { const auto bytes=old.readAll(); if(QJsonDocument::fromJson(bytes).object().value("entries").isArray()) { QSaveFile backup(m_root+"/index.previous.json"); if(backup.open(QIODevice::WriteOnly)) { backup.setPermissions(QFile::ReadOwner|QFile::WriteOwner); backup.write(bytes); backup.commit(); } } } }
        QSaveFile file(path); if (!file.open(QIODevice::WriteOnly)) return false;
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
        return file.write(bytes) == bytes.size() && file.commit();
    }
    QString m_root;
};
