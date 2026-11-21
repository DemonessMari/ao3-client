#pragma once
#include <QString>
#include <QUrl>

struct TextStyleSpan {
    int start = 0, end = 0;
    bool bold = false, italic = false, underline = false, strike = false, code = false;
    QString url;
};
inline QString readingLink(const QString &target, const QString &workId = {}) {
    if (target.trimmed().isEmpty()) return {};
    const QUrl base("https://archiveofourown.org/" + (workId.isEmpty() ? QString() : "works/" + workId));
    const auto url = base.resolved(QUrl(target.trimmed()));
    if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http") || url.host().isEmpty() || !url.userInfo().isEmpty()) return {};
    return url.toString();
}
