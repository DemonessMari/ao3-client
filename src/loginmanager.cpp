#include "loginmanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QDataStream>
#include <QFile>
#include <QSaveFile>
#include <QDateTime>

LoginManager::LoginManager(QObject *parent, const QString &cookieFile)
    : QNetworkCookieJar(parent)
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    m_cookieFile = cookieFile.isEmpty() ? dataDir + QStringLiteral("/cookies.dat") : cookieFile;
    loadCookies();
}

bool LoginManager::isLoggedIn() const
{
    return m_user.isLoggedIn;
}

QString LoginManager::username() const
{
    return m_user.username;
}

UserInfo LoginManager::userInfo() const
{
    return m_user;
}

void LoginManager::setUserInfo(const UserInfo &user)
{
    m_user = user;
    saveCookies();
}

bool LoginManager::setCookiesFromUrl(const QList<QNetworkCookie> &cookies, const QUrl &url)
{
    const bool accepted = QNetworkCookieJar::setCookiesFromUrl(cookies, url);
    if (accepted) saveCookies();
    return accepted;
}

void LoginManager::logout()
{
    m_user = UserInfo();
    m_authToken.clear();
    setAllCookies({});
    saveCookies();
    emit loggedOut();
}

void LoginManager::checkSession()
{
    // Only the server response can establish authentication; a cookie cannot.
    emit sessionChecked(m_user.isLoggedIn, m_user);
}

QList<QNetworkCookie> LoginManager::cookies() const
{
    return allCookies();
}

void LoginManager::setAuthenticityToken(const QString &token)
{
    m_authToken = token;
}

QString LoginManager::authenticityToken() const
{
    return m_authToken;
}

void LoginManager::saveCookies()
{
    QList<QNetworkCookie> cookies = allCookies();
    QSaveFile file(m_cookieFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QDataStream out(&file);
        out.setVersion(QDataStream::Qt_6_0);
        out << qint32(cookies.size());
        for (const auto &cookie : cookies) {
            out << cookie.toRawForm();
        }
        if (out.status() == QDataStream::Ok) file.commit();
    }
}

void LoginManager::loadCookies()
{
    QFile file(m_cookieFile);
    if (!file.open(QIODevice::ReadOnly))
        return;

    if (file.size() > 1024 * 1024) return;
    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_6_0);
    qint32 count = 0;
    in >> count;
    if (count < 0 || count > 10000) return;
    QList<QNetworkCookie> cookies;
    for (int i = 0; i < count; ++i) {
        QByteArray raw;
        in >> raw;
        if (in.status() != QDataStream::Ok) return;
        for (const auto &cookie : QNetworkCookie::parseCookies(raw)) {
            if (cookie.isSessionCookie() || cookie.expirationDate() > QDateTime::currentDateTimeUtc())
                cookies.append(cookie);
        }
    }
    setAllCookies(cookies);
}
