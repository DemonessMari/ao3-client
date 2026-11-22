#pragma once
#include <QWidget>
#include <QUrl>
#include <QNetworkCookie>
class QWebEngineView;
class QLabel;
inline bool archiveWebUrl(const QUrl &url) {
    return url.isValid() && url.scheme()=="https" && url.host()=="archiveofourown.org" && url.port(443)==443 && url.userInfo().isEmpty();
}
class ArchiveWeb : public QWidget {
    Q_OBJECT
public:
    explicit ArchiveWeb(QWidget *parent=nullptr);
    void open(const QUrl &url);
    bool back();
signals:
    void closeRequested();
    void sessionRequested(const QList<QNetworkCookie> &cookies,const QString &agent,const QString &expectedUser);
    void message(const QString &text);
private:
    QList<QNetworkCookie> m_cookies;
    QWebEngineView *m_view;
    QLabel *m_status;
};
