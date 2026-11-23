#include "archiveweb.h"
#include "websession.h"
#include <QApplication>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineHistory>
#include <QWebEngineLoadingInfo>
#include <QWebEngineNewWindowRequest>
#include <QWebEngineDownloadRequest>
#include <QWebEngineCookieStore>
#include <QDesktopServices>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QStandardPaths>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QMenu>

namespace {
class ArchivePage:public QWebEnginePage {
public:
    ArchivePage(QWebEngineProfile *profile,QObject *parent):QWebEnginePage(profile,parent) {}
protected:
    bool acceptNavigationRequest(const QUrl &url,NavigationType type,bool mainFrame) override {
        if(!mainFrame)return url.scheme()=="https" || url.scheme()=="about";
        if(archiveWebUrl(url))return true;
        if(type==NavigationTypeLinkClicked && (url.scheme()=="https" || url.scheme()=="http"))QDesktopServices::openUrl(url);
        return false;
    }
};
QWebEngineProfile *archiveProfile() {
    static QWebEngineProfile *profile=nullptr;
    if(!profile) {
        const auto root=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/ao3-web";
        QDir().mkpath(root);QFile::setPermissions(root,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
        profile=new QWebEngineProfile("ao3-web",qApp);profile->setPersistentStoragePath(root);profile->setCachePath(root+"/cache");profile->setHttpCacheMaximumSize(16*1024*1024);profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    }
    return profile;
}
}
ArchiveWeb::ArchiveWeb(QWidget *parent):QWidget(parent) {
    auto *layout=new QVBoxLayout(this);auto *nav=new QHBoxLayout;layout->addLayout(nav);
    auto *close=new QPushButton("Close",this),*previous=new QPushButton("Back",this),*next=new QPushButton("Forward",this),*reload=new QPushButton("Reload",this),*more=new QPushButton("More",this);
    for(auto *button:{close,previous,next,reload})nav->addWidget(button);
    nav->addStretch();nav->addWidget(more);
    auto *menu=new QMenu(more);more->setMenu(menu);auto *home=menu->addAction("AO3 home");auto *browser=menu->addAction("Open in browser");auto *clear=menu->addAction("Clear website sign-in");auto *use=menu->addAction("Use website sign-in in reader");
    m_status=new QLabel("AO3 website · separate sign-in",this);m_status->setWordWrap(true);layout->addWidget(m_status);
    m_view=new QWebEngineView(this);auto *profile=archiveProfile();auto *page=new ArchivePage(profile,m_view);m_view->setPage(page);layout->addWidget(m_view,1);
    connect(profile->cookieStore(),&QWebEngineCookieStore::cookieAdded,this,[this](const QNetworkCookie &cookie){
        const auto accepted=websiteSessionCookies({cookie});if(accepted.isEmpty())return;
        for(int i=m_cookies.size()-1;i>=0;--i)if(m_cookies[i].hasSameIdentifier(cookie))m_cookies.removeAt(i);
        m_cookies.append(accepted.first());
    });
    connect(profile->cookieStore(),&QWebEngineCookieStore::cookieRemoved,this,[this](const QNetworkCookie &cookie){for(int i=m_cookies.size()-1;i>=0;--i)if(m_cookies[i].hasSameIdentifier(cookie))m_cookies.removeAt(i);});
    profile->cookieStore()->loadAllCookies();
    connect(use,&QAction::triggered,this,[this,page,profile]{
        if(!archiveWebUrl(m_view->url()))return;
        page->runJavaScript(websiteSessionIdentityScript(),[this,profile](const QVariant &value){
            const auto user=value.toString();if(user.isEmpty()){emit message("Sign in to AO3 in this view first.");return;}
            emit sessionRequested(m_cookies,profile->httpUserAgent(),user);
        });
    });
    auto *settings=page->settings();settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls,false);settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls,false);settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard,false);settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows,false);settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled,false);
    connect(close,&QPushButton::clicked,this,&ArchiveWeb::closeRequested);connect(previous,&QPushButton::clicked,m_view,&QWebEngineView::back);connect(next,&QPushButton::clicked,m_view,&QWebEngineView::forward);connect(reload,&QPushButton::clicked,m_view,&QWebEngineView::reload);connect(home,&QAction::triggered,this,[this]{open(QUrl("https://archiveofourown.org/"));});
    connect(browser,&QAction::triggered,this,[this]{if(archiveWebUrl(m_view->url()))QDesktopServices::openUrl(m_view->url());});
    connect(page,&QWebEnginePage::renderProcessTerminated,this,[this](QWebEnginePage::RenderProcessTerminationStatus,int){const QString text="The website renderer stopped. Reload or close this view.";m_status->setProperty("loadError",text);m_status->setText(text);});
    connect(m_view,&QWebEngineView::loadStarted,this,[this]{m_status->setProperty("loadError",QString());m_status->setText("Loading AO3…");});
    connect(m_view,&QWebEngineView::loadFinished,this,[this,previous,next](bool ok){previous->setEnabled(m_view->history()->canGoBack());next->setEnabled(m_view->history()->canGoForward());m_status->setText(ok?"AO3 website · separate sign-in · "+m_view->url().toDisplayString(QUrl::RemoveQuery|QUrl::RemoveFragment):m_status->property("loadError").toString().isEmpty()?QString("AO3 could not load. Check your connection or reload."):m_status->property("loadError").toString());});
    connect(page,&QWebEnginePage::loadingChanged,this,[this](const QWebEngineLoadingInfo &info){
        if(info.status()!=QWebEngineLoadingInfo::LoadFailedStatus)return;
        QString text;
        if(info.errorDomain()==QWebEngineLoadingInfo::HttpStatusCodeDomain && info.errorCode()==403)text="AO3 may require human verification. Complete any check on this page.";
        else if(info.errorDomain()==QWebEngineLoadingInfo::HttpStatusCodeDomain && info.errorCode()==429)text="AO3 rate limit reached. Try again later.";
        if(!text.isEmpty()){m_status->setProperty("loadError",text);m_status->setText(text);}
    });
    connect(clear,&QAction::triggered,this,[this,profile] {
        if(QMessageBox::question(this,"Clear website sign-in?","Clear the embedded AO3 website’s cookies and cache? Native sign-in is separate.")!=QMessageBox::Yes)return;
        profile->cookieStore()->deleteAllCookies();profile->clearAllVisitedLinks();profile->clearHttpCache();m_view->history()->clear();open(QUrl("https://archiveofourown.org/users/logout"));
    });
    connect(page,&QWebEnginePage::newWindowRequested,this,[this](QWebEngineNewWindowRequest &request){const auto url=request.requestedUrl();if(archiveWebUrl(url))open(url);else if(url.scheme()=="https" || url.scheme()=="http")QDesktopServices::openUrl(url);});
    connect(profile,&QWebEngineProfile::downloadRequested,this,[this,page](QWebEngineDownloadRequest *download) {
        if(download->page()!=page)return;
        if(!archiveWebUrl(download->url())){download->cancel();emit message("Only AO3 downloads can be saved from this view.");return;}
        const auto target=QFileDialog::getSaveFileName(this,"Save AO3 download",QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).filePath(QFileInfo(download->downloadFileName()).fileName()));
        if(target.isEmpty()){download->cancel();return;}
        download->setDownloadDirectory(QFileInfo(target).absolutePath());download->setDownloadFileName(QFileInfo(target).fileName());
        connect(download,&QWebEngineDownloadRequest::stateChanged,this,[this,download](QWebEngineDownloadRequest::DownloadState state){if(state==QWebEngineDownloadRequest::DownloadCompleted)emit message("Download saved");else if(state==QWebEngineDownloadRequest::DownloadInterrupted)emit message("Download failed: "+download->interruptReasonString());});m_status->setProperty("loadError","AO3 download started");m_status->setText("AO3 download started");download->accept();
    });
}
void ArchiveWeb::open(const QUrl &url) {if(archiveWebUrl(url))m_view->setUrl(url);else emit message("This address is outside AO3.");}
bool ArchiveWeb::back() {if(!m_view->history()->canGoBack())return false;m_view->back();return true;}
