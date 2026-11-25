#include "formattedreader.h"
#include "formattedwork.h"
#include "readinglibrary.h"
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEnginePermission>
#include <QWebEngineDownloadRequest>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineHttpRequest>
#include <functional>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestInterceptor>
#include <QDialog>
#include <QApplication>
#include <QSpinBox>
#include <QRandomGenerator>
#include <QLabel>
#include <QTextBrowser>
#include "readerfonts.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QSettings>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHostInfo>
#include <QHostAddress>
#include <QEventLoop>
#include <QTimer>
#include <QProgressDialog>
#include <QImageReader>
#include <QBuffer>
#include <QTemporaryDir>
#include <atomic>

namespace {
bool publicImage(const QUrl &url);
class ReadingRequests final:public QWebEngineUrlRequestInterceptor {
public:using QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor;QUrl document;std::atomic_bool external{false};
    void interceptRequest(QWebEngineUrlRequestInfo &info)override {
        const auto type=info.resourceType();
        if(info.requestUrl().adjusted(QUrl::RemoveFragment)==document || info.requestUrl().scheme()=="data" || info.requestUrl().scheme()=="about")return;
        if(external && (info.requestMethod()=="GET" || info.requestMethod()=="HEAD") && (type==QWebEngineUrlRequestInfo::ResourceTypeStylesheet || type==QWebEngineUrlRequestInfo::ResourceTypeImage || type==QWebEngineUrlRequestInfo::ResourceTypeFontResource || type==QWebEngineUrlRequestInfo::ResourceTypeMedia) && publicImage(info.requestUrl())){info.setHttpHeader("Cookie","");return;}
        info.block(true);
    }
};
class ReadingPage final:public QWebEnginePage {
public:using QWebEnginePage::QWebEnginePage;QUrl document;QSet<QUrl> players;std::function<void(const QUrl &)> player;
    bool acceptNavigationRequest(const QUrl &url,NavigationType type,bool main)override {
        if(type==NavigationTypeLinkClicked){if(players.contains(url) && player){player(url);return false;}if(main && url.adjusted(QUrl::RemoveFragment)==document && url.hasFragment())return true;if(url.scheme()=="https" || url.scheme()=="http")QDesktopServices::openUrl(url);return false;}
        return url.adjusted(QUrl::RemoveFragment)==document || url.scheme()=="data" || url.scheme()=="about";
    }
};
class PlayerRequests final:public QWebEngineUrlRequestInterceptor {
public:using QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor;
    void interceptRequest(QWebEngineUrlRequestInfo &info)override {
        const auto url=info.requestUrl();if(url.scheme()=="data" || url.scheme()=="blob" || url.scheme()=="about")return;
        if(url.host()=="archiveofourown.org" || url.host().endsWith(".archiveofourown.org") || !publicImage(url))info.block(true);
    }
};
class PlayerPage final:public QWebEnginePage {
public:using QWebEnginePage::QWebEnginePage;
    bool acceptNavigationRequest(const QUrl &url,NavigationType,bool)override {
        return url.scheme()=="about" || url.scheme()=="data" || (url.scheme()=="https" && url.userInfo().isEmpty() && url.host()!="archiveofourown.org" && !url.host().endsWith(".archiveofourown.org"));
    }
};
void showEmbeddedPlayer(QWidget *parent,const QUrl &url) {
    if(url.scheme()!="https" || url.host().isEmpty() || !url.userInfo().isEmpty() || url.host()=="archiveofourown.org" || url.host().endsWith(".archiveofourown.org"))return;
    QDialog dialog(parent);dialog.setWindowTitle("Embedded player — "+url.host());dialog.resize(1050,800);QVBoxLayout layout(&dialog);
    QWidget header;QHBoxLayout controls(&header);controls.setContentsMargins(0,0,0,0);QPushButton close("Close player"),browser("Open in browser"),load("Load player");controls.addWidget(&close);controls.addStretch();controls.addWidget(&browser);layout.addWidget(&header);
    QLabel notice("Load this player without your AO3 sign-in?");notice.setWordWrap(true);layout.addWidget(&notice);layout.addWidget(&load);
    PlayerRequests requests;QWebEngineProfile profile;profile.setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);profile.setUrlRequestInterceptor(&requests);
    QWebEngineView view;auto *page=new PlayerPage(&profile,&view);view.setPage(page);view.settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,true);view.settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls,false);view.settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls,false);view.settings()->setAttribute(QWebEngineSettings::PluginsEnabled,false);view.settings()->setAttribute(QWebEngineSettings::PdfViewerEnabled,false);view.settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture,true);view.settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled,true);layout.addWidget(&view,1);view.hide();
    QObject::connect(page,&QWebEnginePage::permissionRequested,&dialog,[](QWebEnginePermission permission){permission.deny();});
    QObject::connect(&profile,&QWebEngineProfile::downloadRequested,&dialog,[](QWebEngineDownloadRequest *request){request->cancel();});
    QObject::connect(page,&QWebEnginePage::fullScreenRequested,&dialog,[&](QWebEngineFullScreenRequest request){request.accept();if(request.toggleOn()){header.hide();notice.hide();layout.setContentsMargins(0,0,0,0);dialog.showFullScreen();}else{header.show();layout.setContentsMargins(9,9,9,9);dialog.showNormal();}});
    QObject::connect(&close,&QPushButton::clicked,&dialog,&QDialog::reject);QObject::connect(&browser,&QPushButton::clicked,&dialog,[url]{QDesktopServices::openUrl(url);});
    QObject::connect(&load,&QPushButton::clicked,&dialog,[&]{load.hide();notice.setText(url.host());view.show();QWebEngineHttpRequest request(url);request.setHeader("Referer","https://archiveofourown.org/");view.load(request);});
    QObject::connect(&view,&QWebEngineView::loadFinished,&dialog,[&](bool ok){if(!ok)notice.setText("Player unavailable.");});dialog.exec();
}

bool publicImage(const QUrl &url) {
    if(url.scheme()!="https" || url.host().isEmpty() || url.port(443)!=443 || !url.userInfo().isEmpty())return false;
    const auto host=url.host().toLower();if(host=="localhost" || host.endsWith(".localhost") || host.endsWith(".local"))return false;
    const auto resolved=QHostInfo::fromName(host);if(resolved.error()!=QHostInfo::NoError || resolved.addresses().isEmpty())return false;
    for(const auto &a:resolved.addresses()) {
        if(a.isLoopback() || a.isLinkLocal() || a.isMulticast() || a.isNull())return false;
        bool mapped=false;const auto ipv4=a.toIPv4Address(&mapped);
        if(a.protocol()==QAbstractSocket::IPv4Protocol || mapped){const auto v=ipv4;if((v>>24)==0 || (v>>24)==10 || (v>>24)==127 || (v>>16)==0xa9fe || (v>>20)==0xac1 || (v>>16)==0xc0a8 || (v>>22)==0x191 || (v>>24)>=224)return false;}
        else {const auto v=a.toIPv6Address();if((v[0]&0xfe)==0xfc)return false;}
    }
    return true;
}
QByteArray fetchImage(QNetworkAccessManager &network,QUrl url,QProgressDialog &progress,QString &error) {
    for(int redirect=0;redirect<4;++redirect) {
        if(progress.wasCanceled()){error="Image download canceled. Your previous copy was kept.";return {};}
        if(!publicImage(url)){error="Only public HTTPS images can be downloaded.";return {};}
        QNetworkRequest request(url);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setRawHeader("User-Agent","AO3Client (Offline image download)");
        auto *reply=network.get(request);QEventLoop loop;QTimer timeout;timeout.setSingleShot(true);timeout.start(20000);QByteArray bytes;bool tooLarge=false;
        QObject::connect(reply,&QIODevice::readyRead,&loop,[&]{bytes+=reply->readAll();if(bytes.size()>4*1024*1024){tooLarge=true;reply->abort();}});
        QObject::connect(&progress,&QProgressDialog::canceled,reply,&QNetworkReply::abort);
        QObject::connect(&timeout,&QTimer::timeout,reply,&QNetworkReply::abort);QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);loop.exec();bytes+=reply->readAll();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto next=url.resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());const auto success=reply->error()==QNetworkReply::NoError;reply->deleteLater();
        if(tooLarge || bytes.size()>4*1024*1024){error="Image exceeds 4 MB. Your previous copy was kept.";return {};}
        if(status>=300 && status<400){url=next;continue;}
        if(!success || status!=200){error=QString("Image download failed (HTTP %1). Your previous copy was kept.").arg(status);return {};}
        return bytes;
    }
    error="Too many image redirects.";return {};
}
}
FormattedPosition showFormattedReader(QWidget *parent,const Work &work,const QVector<WorkChapter> &chapters,int initial,bool persistProgress) {
    if(chapters.isEmpty())return {};
    QDialog dialog(parent);dialog.setWindowTitle(work.title+" — Original formatting");dialog.resize(1000,850);
    QVBoxLayout layout(&dialog);QHBoxLayout controls;QPushButton previous("Previous chapter"),next("Next chapter");QComboBox chapter;QCheckBox workSkin("Work skin"),external("External content");workSkin.setChecked(QSettings().value("reader/formattedWorkSkin",true).toBool());
    for(const auto &entry:chapters)chapter.addItem(entry.title);
    chapter.setCurrentIndex(qBound(0,initial,int(chapters.size())-1));controls.addWidget(&previous);controls.addWidget(&chapter,1);controls.addWidget(&next);controls.addWidget(&workSkin);controls.addWidget(&external);layout.addLayout(&controls);
    QHBoxLayout appearance;QLabel sizeLabel("Text size");QSpinBox fontSize;fontSize.setRange(11,32);fontSize.setValue(QSettings().value("reader/fontSize",15).toInt());appearance.addWidget(&sizeLabel);appearance.addWidget(&fontSize);appearance.addStretch();layout.addLayout(&appearance);
    const auto family=QSettings().value("reader/fontFamily","serif").toString();QByteArray fontBytes;if(readerFontId(family)){QFile file(readerFontDirectory()+"/"+family.mid(5)+".font");if(file.open(QIODevice::ReadOnly))fontBytes=file.read(20*1024*1024+1);}
    double fraction=0;bool restoring=true;quint64 renderRevision=0;const auto progressKey="formattedProgress/"+work.id+"/";
    QTemporaryDir temporary;const auto documentUrl=QUrl::fromLocalFile(temporary.filePath("chapter.html"));
    ReadingRequests requests;requests.document=documentUrl;QWebEngineProfile profile;profile.setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);profile.setUrlRequestInterceptor(&requests);
    QWebEngineView view;auto *page=new ReadingPage(&profile,&view);page->document=documentUrl;page->player=[&dialog](const QUrl &target){QTimer::singleShot(0,&dialog,[&dialog,target]{showEmbeddedPlayer(&dialog,target);});};view.setPage(page);view.settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,false);view.settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls,false);view.settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls,false);view.settings()->setAttribute(QWebEngineSettings::PluginsEnabled,false);view.settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture,true);view.settings()->setAttribute(QWebEngineSettings::PdfViewerEnabled,false);layout.addWidget(&view,1);
    QDialogButtonBox buttons(QDialogButtonBox::Close);layout.addWidget(&buttons);QObject::connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    QObject::connect(page,&QWebEnginePage::scrollPositionChanged,&dialog,[&](const QPointF &position){if(restoring)return;const auto range=page->contentsSize().height()-view.height();if(range>0){fraction=qBound(0.0,position.y()/range,1.0);if(persistProgress)QSettings().setValue(progressKey+QString::number(chapter.currentIndex()),fraction);}});
    QObject::connect(&view,&QWebEngineView::loadFinished,&dialog,[&](bool ok){if(ok){const auto revision=renderRevision;QTimer::singleShot(150,&dialog,[&,revision]{if(revision==renderRevision)restoring=false;});}});
    auto render=[&]{
        restoring=true;++renderRevision;const int i=chapter.currentIndex();previous.setEnabled(i>0);next.setEnabled(i+1<chapters.size());const auto &c=chapters[i];page->players=readingEmbedTargets(c.html);
        fraction=persistProgress?QSettings().value(progressKey+QString::number(i),QSettings().value("readingProgress/"+work.id+"/positions/"+QString::number(i),0.0)).toDouble():0.0;
        auto *reader=parent?parent->findChild<QTextBrowser *>():nullptr;const auto palette=reader?reader->palette():parent?parent->palette():QApplication::palette();
        auto html=formattedDocument(c.html.isEmpty()?c.text.toHtmlEscaped().replace("\n","<br>"):c.html,c.workCss,palette.color(reader?QPalette::Base:QPalette::Window).name(),palette.color(reader?QPalette::Text:QPalette::WindowText).name(),fontSize.value()*4/3,QSettings().value("reader/lineSpacing",160).toDouble()/100,QSettings().value("reader/margin",24).toInt(),workSkin.isChecked(),fontBytes,family,external.isChecked());
        const QRegularExpression blocks("<(?:p|h[1-6]|li|tr|br)\\b",QRegularExpression::CaseInsensitiveOption);auto matches=blocks.globalMatch(html,html.indexOf("<body>"));QVector<int> starts;while(matches.hasNext())starts.append(matches.next().capturedStart());
        const auto index=qBound(0,qRound(fraction*qMax(0,int(starts.size())-1)),qMax(0,int(starts.size())-1));auto location=documentUrl;
        if(fraction>0 && !starts.isEmpty()){const auto id="ao3-client-reading-position-"+QString::number(QRandomGenerator::global()->generate64(),16);html.insert(starts[index],"<span id=\""+QString(id)+"\"></span>");location.setFragment(id);}
        QSaveFile file(documentUrl.toLocalFile());if(!file.open(QIODevice::WriteOnly))return;file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);file.write(html.toUtf8());if(file.commit())view.load(location);
    };
    QObject::connect(&external,&QCheckBox::toggled,&dialog,[&](bool enabled){if(enabled && QMessageBox::question(&dialog,"Load external content?","Images, styles and media will contact their hosts. Continue?")!=QMessageBox::Yes){external.setChecked(false);return;}requests.external=enabled;view.settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls,enabled);render();});
    QObject::connect(&chapter,&QComboBox::currentIndexChanged,&dialog,[&]{render();});QObject::connect(&workSkin,&QCheckBox::toggled,&dialog,[&]{QSettings().setValue("reader/formattedWorkSkin",workSkin.isChecked());render();});QObject::connect(&fontSize,&QSpinBox::valueChanged,&dialog,[&]{QSettings().setValue("reader/fontSize",fontSize.value());render();});QObject::connect(&previous,&QPushButton::clicked,&dialog,[&]{chapter.setCurrentIndex(chapter.currentIndex()-1);});QObject::connect(&next,&QPushButton::clicked,&dialog,[&]{chapter.setCurrentIndex(chapter.currentIndex()+1);});render();dialog.exec();
    return {chapter.currentIndex(),fraction};
}
bool saveFormattedImages(QWidget *parent,QVector<WorkChapter> &chapters,QString &error) {
    auto saved=chapters;QMap<QString,QString> cache;qsizetype total=0,htmlTotal=0;QNetworkAccessManager network;QProgressDialog progress("Saving images…","Cancel",0,0,parent);progress.setWindowModality(Qt::WindowModal);progress.setMinimumDuration(0);progress.show();
    const QRegularExpression images("<img\\b[^>]*>",QRegularExpression::CaseInsensitiveOption);
    for(auto &chapter:saved) {
        const QString source=staticReadingHtml(chapter.html);auto matches=images.globalMatch(source);QVector<QPair<QPair<int,int>,QString>> changes;
        while(matches.hasNext()) {
            const auto m=matches.next();const auto nodes=CommentHtml::nodes(m.captured());if(nodes.isEmpty())continue;const auto src=nodes[0].attrs.value("src");if(src.startsWith("data:image/",Qt::CaseInsensitive))continue;
            const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(src));const auto key=url.toString();
            if(!cache.contains(key)) {
                if(cache.size()>=100){error="A work can save up to 100 images.";return false;}
                const auto bytes=fetchImage(network,url,progress,error);if(bytes.isEmpty())return false;
                QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);const auto format=QString::fromLatin1(QImageReader::imageFormat(&buffer)).toLower();
                if(!QStringList{"png","jpeg","jpg","gif","webp"}.contains(format)){error="Unsupported image format. Use PNG, JPEG, GIF or WebP.";return false;}
                total+=bytes.size();if(total>24*1024*1024){error="Images exceed the 24 MB offline limit.";return false;}
                cache[key]="data:image/"+(format=="jpg"?QString("jpeg"):format)+";base64,"+QString::fromLatin1(bytes.toBase64());
            }
            auto attrs=nodes[0].attrs;attrs["src"]=cache[key];QString tag="<img";for(auto it=attrs.begin();it!=attrs.end();++it)tag+=" "+it.key()+"=\""+it.value().toHtmlEscaped()+"\"";tag+=">";changes.prepend({{int(m.capturedStart()),int(m.capturedLength())},tag});
        }
        chapter.html=source;for(const auto &change:changes)chapter.html.replace(change.first.first,change.first.second,change.second);
        htmlTotal+=chapter.html.toUtf8().size();if(htmlTotal>48*1024*1024){error="Formatted content exceeds the 48 MB offline limit. Your previous copy was kept.";return false;}
    }
    chapters=saved;return true;
}
