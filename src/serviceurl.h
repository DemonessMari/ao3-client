#pragma once
#include <QUrl>
inline bool validTranslationService(const QUrl &url) {
    const auto host=url.host().toLower(); return !host.isEmpty() && url.userInfo().isEmpty() && (url.scheme()=="https" || (url.scheme()=="http" && (host=="localhost" || host=="127.0.0.1" || host=="::1")));
}
