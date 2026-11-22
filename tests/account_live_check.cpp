#include <QApplication>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QTextStream>
#include "loginmanager.h"
#include "kudos.h"
#include "archivebookmark.h"
int main(int argc,char **argv) {
    QApplication app(argc,argv);QStandardPaths::setTestModeEnabled(true);
    if(argc<2 || argc>3){QTextStream(stderr)<<"Pass the native cookie file for a read-only check.\n";return 2;}
    const auto agent=argc==3?QSettings(QString::fromLocal8Bit(argv[2]),QSettings::IniFormat).value("network/userAgent","AO3Client/3.0 (Unofficial Desktop Reader)").toString():QString("AO3Client/3.0 (Unofficial Desktop Reader)");
    QTemporaryDir directory;const auto copy=directory.filePath("cookies.dat");
    if(!QFile::copy(QString::fromLocal8Bit(argv[1]),copy))return 2;
    QFile::setPermissions(copy,QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    QNetworkAccessManager network;auto *jar=new LoginManager(nullptr,copy);network.setCookieJar(jar);
    auto get=[&](const QString &path)->QString {
        QNetworkRequest request(QUrl("https://archiveofourown.org"+path));request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::SameOriginRedirectPolicy);request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::AlwaysNetwork);request.setRawHeader("User-Agent",agent.toUtf8());request.setRawHeader("Cache-Control","no-cache");request.setTransferTimeout(30000);
        auto *reply=network.get(request);QEventLoop loop;QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);QTimer::singleShot(35000,&loop,&QEventLoop::quit);loop.exec();
        const bool ok=reply->isFinished() && reply->error()==QNetworkReply::NoError;const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(!reply->isFinished())reply->abort();
        const auto html=ok?QString::fromUtf8(reply->readAll()):QString();delete reply;
        if(!ok)QTextStream(stdout)<<"AO3 request failed (HTTP "<<status<<").\n";
        return html;
    };
    const auto home=get("/");QString user;const auto nodes=CommentHtml::nodes(home);
    for(int i=0;i<nodes.size();++i)if(nodes[i].tag=="a") {
        bool greeting=false;for(int p=nodes[i].parent;p>=0;p=nodes[p].parent)if(nodes[p].attrs.value("id")=="greeting")greeting=true;
        const auto path=QUrl("https://archiveofourown.org").resolved(QUrl(nodes[i].attrs.value("href"))).path();const auto match=QRegularExpression("^/users/([A-Za-z0-9_]+)$").match(path);
        if(greeting && match.hasMatch() && match.captured(1)!="login" && match.captured(1)!="logout")user=match.captured(1);
    }
    if(user.isEmpty() || !siteIdentityMatches(home,user)){QTextStream(stdout)<<"No confirmed native account session is available. No account writes performed.\n";return 77;}
    const auto results=get("/works/search?work_search%5Bquery%5D=sherlock");if(!siteIdentityMatches(results,user))return 1;
    const auto work=QRegularExpression("href=[\"']/works/([0-9]+)[\"']").match(results).captured(1);if(work.isEmpty())return 1;
    const auto html=get("/works/"+work+"?view_adult=true");if(!siteIdentityMatches(html,user))return 1;
    const auto form=parseArchiveBookmark(html,work);
    if(!form.error.isEmpty()){QTextStream(stdout)<<form.error<<"\n";return 1;}
    QTextStream(stdout)<<"Native session confirmed. Live AO3 bookmark form validated; "<<form.pseuds.size()<<" pseud choice(s). No account writes performed.\n";
    return 0;
}
