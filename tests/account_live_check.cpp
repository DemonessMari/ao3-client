#include "accountform.h"
#include "nativetools.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
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
#include "archivepages.h"
#include "profileavatar.h"
#include "accountbutton.h"
int main(int argc,char **argv) {
    QApplication app(argc,argv);QStandardPaths::setTestModeEnabled(true);
    if(argc<2 || argc>5 || (argc>3 && QString::fromLocal8Bit(argv[3])!="--profile" && QString::fromLocal8Bit(argv[3])!="--prepare-bookmark" && QString::fromLocal8Bit(argv[3])!="--forms" && QString::fromLocal8Bit(argv[3])!="--tools")){QTextStream(stderr)<<"Pass the native cookie file, optional settings file, and optional --profile [render-path] or --prepare-bookmark or --forms or --tools.\n";return 2;}
    const auto agent=argc>=3?QSettings(QString::fromLocal8Bit(argv[2]),QSettings::IniFormat).value("network/userAgent","AO3Client/3.0 (Unofficial Desktop Reader)").toString():QString("AO3Client/3.0 (Unofficial Desktop Reader)");
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
    if(argc>3 && QString::fromLocal8Bit(argv[3])=="--tools") {
        const QVector<QPair<QString,QString>> paths={{"work editor","/works/new"},{"collection editor","/collections/new"},{"password editor","/users/"+user+"/change_password"},{"email editor","/users/"+user+"/change_email"},{"username editor","/users/"+user+"/change_username"},{"pseud editor","/users/"+user+"/pseuds/"+user+"/edit"},{"draft list","/users/"+user+"/works/drafts"},{"bulk work list","/users/"+user+"/works/show_multiple"},{"invitations","/users/"+user+"/invitations"},{"collections","/collections"}};
        bool valid=true;
        for(const auto &item:paths){QEventLoop pause;QTimer::singleShot(1500,&pause,&QEventLoop::quit);pause.exec();const auto html=get(item.second);if(html.isEmpty())return 77;const auto page=NativeTools::parse(html,item.second,user);const bool expected=item.first.endsWith("editor");
            QTextStream(stdout)<<item.first<<": "<<page.forms.size()<<" native form(s); "<<(page.error.isEmpty()?QString("page verified"):page.error)<<"; no changes submitted.\n";
            if(!page.error.isEmpty() || (expected && page.forms.isEmpty())) {
                valid=false;ArchivePages::Document doc(html);QStringList names;
                for(int n:doc.select({},{}))if(QStringList{"input","select","textarea"}.contains(doc.list[n].tag) && !doc.list[n].attrs.contains("disabled")){auto name=doc.list[n].attrs.value("name");if(!name.isEmpty() && !NativeTools::fieldAllowed(name)){name.replace(QRegularExpression("[0-9]+"),"#");if(!names.contains(name))names<<name;}}
                QTextStream(stdout)<<"Unsupported control names: "<<names.join(", ")<<"\n";
            }
        }
        return valid?0:1;
    }
    if(argc>3 && QString::fromLocal8Bit(argv[3])=="--forms") {
        for(const auto &kind:QStringList{"profile","preferences","pseud","externalBookmark"}) {
            QEventLoop pause;QTimer::singleShot(1000,&pause,&QEventLoop::quit);pause.exec();
            const auto html=get(AccountForms::source(kind,user));if(html.isEmpty())return 77;
            const auto form=AccountForms::parse(html,kind,user);
            if(!form.error.isEmpty()){QTextStream(stdout)<<kind<<": form unavailable: "<<form.error<<"\n";return 1;}
            QTextStream(stdout)<<kind<<": parsed "<<form.fields.size()<<" editable fields; no changes submitted.\n";
        }
        for(const auto &kind:QStringList{"pseuds","blocked","muted","media"}) {
            QEventLoop pause;QTimer::singleShot(1000,&pause,&QEventLoop::quit);pause.exec();
            const auto html=get(ArchivePages::route(kind,user));if(html.isEmpty())return 77;
            const auto page=ArchivePages::parse(html,kind,user,{},1);
            if(!page.error.isEmpty()){QTextStream(stdout)<<kind<<": list unavailable: "<<page.error<<"\n";return 1;}
            QTextStream(stdout)<<kind<<": list structure verified; no changes submitted.\n";
        }
        return 0;
    }
    if(argc>3 && QString::fromLocal8Bit(argv[3])=="--profile") {
        const auto avatar=profileAvatar(get("/users/"+user+"/profile"));
        if(avatar.url.isEmpty()){QTextStream(stdout)<<"Profile returned no supported avatar URL. Live image check unavailable.\n";return 77;}
        QUrl url(avatar.url);QImage image;
        for(int redirects=0;redirects<=3;++redirects){
            QNetworkRequest request(url);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setRawHeader("User-Agent",agent.toUtf8());request.setRawHeader("Accept","image/png,image/jpeg,image/gif");request.setTransferTimeout(30000);
            if(url.host()!="archiveofourown.org"){request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,QNetworkRequest::Manual);request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,QNetworkRequest::Manual);}
            auto *reply=network.get(request);reply->setReadBufferSize(2*1024*1024+1);QByteArray bytes;QEventLoop loop;QTimer timeout;timeout.setSingleShot(true);
            QObject::connect(reply,&QNetworkReply::readyRead,&loop,[&]{bytes+=reply->readAll();if(bytes.size()>2*1024*1024)reply->abort();});QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);QObject::connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);timeout.start(35000);loop.exec();
            if(!reply->isFinished())reply->abort();bytes+=reply->readAll();const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto next=url.resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());const bool ok=reply->error()==QNetworkReply::NoError;delete reply;
            if(ok && status>=300 && status<400 && redirects<3 && avatarUrlAllowed(next,true)){url=next;continue;}
            if(ok && status==200)image=decodeProfileAvatar(bytes);else QTextStream(stdout)<<"Avatar request unavailable (HTTP "<<status<<").\n";break;
        }
        if(image.isNull()){QTextStream(stdout)<<"Live avatar could not be decoded.\n";return 77;}
        AccountButton button;button.setFixedSize(88,88);button.setProperty("accountAvatar",true);button.setProperty("accountImage",image);button.show();app.processEvents();
        if(argc==5 && !button.grab().save(QString::fromLocal8Bit(argv[4])))return 1;
        QTextStream(stdout)<<"Actual account avatar decoded and rendered in the circular button. No account writes performed.\n";return 0;
    }
    const auto results=get("/works/search?work_search%5Bquery%5D=sherlock");if(!siteIdentityMatches(results,user))return 1;
    const auto work=QRegularExpression("href=[\"']/works/([0-9]+)[\"']").match(results).captured(1);if(work.isEmpty())return 1;
    const auto html=get("/works/"+work+"?view_adult=true");if(!siteIdentityMatches(html,user))return 1;
    const auto form=parseArchiveBookmark(html,work);
    if(!form.error.isEmpty()){QTextStream(stdout)<<form.error<<"\n";return 1;}
    if(argc>3 && QString::fromLocal8Bit(argv[3])=="--prepare-bookmark") {
        QTextStream(stdout)<<"Private bookmark candidate: "<<QJsonDocument(QJsonObject{{"work_url","https://archiveofourown.org/works/"+work},{"has_existing_bookmark",!form.id.isEmpty()},{"can_create",form.id.isEmpty()}}).toJson(QJsonDocument::Compact)<<"\n";return 0;
    }
    for(const auto &kind:QStringList{"bookmarks","subscriptions","inbox"}) {
        const auto page=ArchivePages::parse(get("/users/"+user+"/"+kind),kind,user,{},1);
        if(!page.error.isEmpty()){QTextStream(stdout)<<"Native "<<kind<<" parser failed: "<<page.error<<"\n";return 1;}
        QTextStream(stdout)<<"Native "<<kind<<" list validated ("<<page.entries.size()<<" entries on the first page).\n";
    }
    QTextStream(stdout)<<"Native session confirmed. Live AO3 bookmark form validated; "<<form.pseuds.size()<<" pseud choice(s). No account writes performed.\n";
    return 0;
}
