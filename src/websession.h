#pragma once
#include <QNetworkCookie>
#include <QFile>
#include <QDateTime>
#include <QRegularExpression>
inline bool validWebsiteSessionIdentity(const QString &user,const QString &agent) {
    return QRegularExpression("^[A-Za-z0-9_]+$").match(user).hasMatch() && !agent.isEmpty() && agent.size()<=512 && !agent.contains('\r') && !agent.contains('\n');
}
inline QList<QNetworkCookie> websiteSessionCookies(const QList<QNetworkCookie> &cookies) {
    QList<QNetworkCookie> out;
    for(auto cookie:cookies) {
        auto domain=cookie.domain().toLower();if(domain.startsWith('.'))domain.remove(0,1);
        if(domain!="archiveofourown.org" || (!cookie.isSessionCookie() && cookie.expirationDate()<=QDateTime::currentDateTimeUtc()))continue;
        cookie.setSecure(true);out.append(cookie);
    }
    return out;
}

inline QString websiteSessionIdentityScript() {
    QFile file(":/web/session-identity.js");if(!file.open(QIODevice::ReadOnly))return {};
    return QString::fromUtf8(file.readAll());
}
