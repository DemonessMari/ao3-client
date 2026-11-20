#pragma once

#include <QObject>
#include <QSettings>
#include <QNetworkCookieJar>
#include <QNetworkCookie>
#include "workmodel.h"

class LoginManager : public QNetworkCookieJar {
    Q_OBJECT
public:
    explicit LoginManager(QObject *parent = nullptr, const QString &cookieFile = {});

    bool isLoggedIn() const;
    QString username() const;
    UserInfo userInfo() const;

    void setUserInfo(const UserInfo &user);
    bool setCookiesFromUrl(const QList<QNetworkCookie> &cookies, const QUrl &url) override;
    void logout();
    void checkSession();

    QList<QNetworkCookie> cookies() const;

    void setAuthenticityToken(const QString &token);
    QString authenticityToken() const;

signals:
    void loginSuccessful(const UserInfo &user);
    void loginFailed(const QString &reason);
    void loggedOut();
    void sessionChecked(bool valid, const UserInfo &user);

private:
    void saveCookies();
    void loadCookies();

    UserInfo m_user;
    QString m_authToken;
    QString m_cookieFile;
};
