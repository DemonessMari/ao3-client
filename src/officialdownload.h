#pragma once
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>
#include <QUrl>
#include <QSet>
#include <QRegularExpression>

struct OfficialDownload { QString format; QUrl url; };
inline QString officialDownloadFormat(const QUrl &url, const QString &workId) {
    if (!url.isValid() || url.scheme() != "https" || url.host() != "archiveofourown.org" ||
        !url.userInfo().isEmpty() || (url.port(-1) != -1 && url.port() != 443) ||
        !QRegularExpression("^[0-9]+$").match(workId).hasMatch()) return {};
    const QString path = url.path();
    if (!path.startsWith("/downloads/" + workId + "/") || path.mid(QString("/downloads/" + workId + "/").size()).contains('/')) return {};
    const QString format = path.section('.', -1).toLower();
    return QStringList{"epub", "pdf", "html", "mobi", "azw3"}.contains(format) ? format : QString();
}
inline QVector<OfficialDownload> officialDownloads(const QString &html, const QString &workId) {
    QTextDocument doc; doc.setHtml(html);
    QVector<OfficialDownload> result; QSet<QString> seen;
    for (auto block=doc.begin(); block.isValid(); block=block.next()) for (auto it=block.begin(); !it.atEnd(); ++it) {
        const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(it.fragment().charFormat().anchorHref()));
        const auto format=officialDownloadFormat(url,workId);
        if (!format.isEmpty() && !seen.contains(format)) { result.append({format,url}); seen.insert(format); }
    }
    return result;
}
