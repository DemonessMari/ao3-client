#pragma once
#include "readinglibrary.h"
#include <QSettings>
#include <QSet>
#include <QUuid>
#include <cmath>
class LibraryBackup {
public:
    static QJsonObject snapshot(ReadingLibrary library=ReadingLibrary(),bool includeDownloads=true) {
        QJsonArray works; QSettings settings;
        for(const auto &value:library.entries()) {
            const auto entry=value.toObject(); const auto work=ReadingLibrary::work(entry); QJsonArray chapters,marks,positions;
            for(const auto &chapter:includeDownloads?library.load(work.id):QVector<WorkChapter>{}) chapters.append(ReadingLibrary::chapterMetadata(chapter));
            for(const auto &value:settings.value("bookmarks/"+work.id).toList()) { const auto mark=value.toMap(); marks.append(QJsonObject{{"id",mark.value("id").toString()},{"title",mark.value("title").toString()},{"chapter",mark.value("chapter").toInt()},{"fraction",mark.value("position").toDouble()}}); }
            const QString prefix="readingProgress/"+work.id+"/positions/";
            for(const auto &key:settings.allKeys()) if(key.startsWith(prefix)) positions.append(QJsonObject{{"chapter",key.mid(prefix.size()).toInt()},{"fraction",settings.value(key).toDouble()}});
            works.append(QJsonObject{{"work",ReadingLibrary::metadata(work)},{"favorite",entry.value("favorite").toBool()},{"chapters",chapters},{"bookmarks",marks},{"progress",QJsonObject{{"chapter",settings.value("readingProgress/"+work.id+"/chapter",0).toInt()},{"positions",positions}}}});
        }
        return {{"format","ao3-client-library"},{"version",1},{"works",works}};
    }
    static bool validate(const QJsonObject &root,QString *error=nullptr) {
        auto fail=[&](const QString &text) { if(error) *error=text; return false; };
        if(root.value("format")!="ao3-client-library" || root.value("version").toInt()!=1 || !root.value("works").isArray()) return fail("Unsupported library backup");
        const auto works=root.value("works").toArray(); if(works.size()>10000) return fail("Backup contains too many works"); QSet<QString> ids;
        for(const auto &value:works) {
            const auto entry=value.toObject(); const auto id=entry.value("work").toObject().value("id").toString();
            if(!QRegularExpression("^\\d+$").match(id).hasMatch() || ids.contains(id)) return fail("Invalid or duplicate work ID");
            ids.insert(id);
            if(!entry.value("chapters").isArray() || !entry.value("bookmarks").isArray()) return fail("Missing chapters or bookmarks");
            for(const auto &c:entry.value("chapters").toArray()) if(c.toObject().value("text").toString().trimmed().isEmpty()) return fail("Backup contains an empty chapter");
            for(const auto &m:entry.value("bookmarks").toArray()) { const auto mark=m.toObject(); const double fraction=mark.value("fraction").toDouble(-1); if(mark.value("title").toString().trimmed().isEmpty() || mark.value("chapter").toInt(-1)<0 || !std::isfinite(fraction) || fraction<0 || fraction>1) return fail("Invalid reading bookmark"); }
            const auto progress=entry.value("progress").toObject(); if(progress.value("chapter").toInt(-1)<0) return fail("Invalid chapter position");
            for(const auto &p:progress.value("positions").toArray()) { const auto position=p.toObject(); const double fraction=position.value("fraction").toDouble(-1); if(position.value("chapter").toInt(-1)<0 || !std::isfinite(fraction) || fraction<0 || fraction>1) return fail("Invalid reading position"); }
        }
        return true;
    }
    static bool merge(const QJsonObject &root,QString *error=nullptr,ReadingLibrary library=ReadingLibrary()) {
        if(!validate(root,error)) return false;
        QSettings settings;
        for(const auto &value:root.value("works").toArray()) {
            const auto entry=value.toObject(); const auto work=ReadingLibrary::work(entry); QVector<WorkChapter> chapters;
            for(const auto &c:entry.value("chapters").toArray()) chapters.append(ReadingLibrary::readChapter(c.toObject()));
            if(!library.remember(work) || (!chapters.isEmpty() && library.load(work.id).isEmpty() && !library.saveOffline(work,chapters)) || (entry.value("favorite").toBool() && !library.favorite(work.id,true))) { if(error) *error="Could not save imported library. Check available storage."; return false; }
            auto marks=settings.value("bookmarks/"+work.id).toList();
            for(const auto &m:entry.value("bookmarks").toArray()) { const auto mark=m.toObject(); bool exists=false; for(const auto &v:marks) { const auto old=v.toMap(); if(old.value("title").toString()==mark.value("title").toString() && old.value("chapter").toInt()==mark.value("chapter").toInt() && qAbs(old.value("position").toDouble()-mark.value("fraction").toDouble())<.00001) exists=true; } if(!exists) marks.append(QVariantMap{{"id",mark.value("id").toString()},{"title",mark.value("title").toString()},{"chapter",mark.value("chapter").toInt()},{"position",mark.value("fraction").toDouble()}}); }
            settings.setValue("bookmarks/"+work.id,marks);
            const auto progress=entry.value("progress").toObject(); const QString prefix="readingProgress/"+work.id+"/";
            if(!settings.contains(prefix+"chapter")) settings.setValue(prefix+"chapter",progress.value("chapter").toInt());
            for(const auto &v:progress.value("positions").toArray()) { const auto p=v.toObject(); const QString key=prefix+"positions/"+QString::number(p.value("chapter").toInt()); if(!settings.contains(key)) settings.setValue(key,p.value("fraction").toDouble()); }
        }
        settings.sync(); if(settings.status()!=QSettings::NoError) { if(error) *error="Could not save reading positions"; return false; } return true;
    }
};
