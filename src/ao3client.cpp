#include "discoverysearch.h"
#include "profileavatar.h"
#include <memory>
#include "ao3client.h"
#include "accountreadings.h"
#include "workcontent.h"
#include <QRegularExpression>
#include <QUrlQuery>
#include <QUrl>
#include <QDateTime>
#include <QNetworkCookie>
#include <QTextDocument>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>

void AO3Client::fetchOfficialDownloads(const QString &workId) {
    if (!QRegularExpression("^[0-9]+$").match(workId).hasMatch()) return;
    auto *reply=m_manager->get(makeRequest(QUrl("https://archiveofourown.org/works/"+workId+"?view_adult=true")));
    connect(reply,&QNetworkReply::finished,this,[this,reply,workId] {
        reply->deleteLater();
        if(reply->error()!=QNetworkReply::NoError) { emit errorOccurred("Could not fetch download formats: "+reply->errorString()); return; }
        emit officialDownloadsFetched(workId,officialDownloads(QString::fromUtf8(reply->readAll()),workId));
    });
}

void AO3Client::saveOfficialDownload(const QString &workId,const QUrl &url,const QString &path) {
    const auto format=officialDownloadFormat(url,workId);
    if(format.isEmpty()) { emit errorOccurred("Invalid AO3 download link"); return; }
    auto request=makeRequest(url); request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto *reply=m_manager->get(request);
    connect(reply,&QNetworkReply::downloadProgress,this,[reply](qint64 received,qint64 total) { if(received>64*1024*1024 || total>64*1024*1024) { reply->setProperty("oversized",true); reply->abort(); } });
    connect(reply,&QNetworkReply::finished,this,[this,reply,path,format] {
        reply->deleteLater(); const auto data=reply->readAll(); const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto type=reply->header(QNetworkRequest::ContentTypeHeader).toString();
        if(reply->error()!=QNetworkReply::NoError || status!=200 || data.isEmpty() || data.size()>64*1024*1024 || (format!="html" && type.contains("text/html"))) {
            emit errorOccurred(reply->property("oversized").toBool()?"Download exceeds 64 MB":"AO3 did not return the requested file. Try again later."); return;
        }
        if(format=="html" && !QRegularExpression(QStringLiteral("<div[^>]*\\bid\\s*=\\s*['\"]chapters['\"]"),QRegularExpression::CaseInsensitiveOption).match(QString::fromUtf8(data)).hasMatch()) { emit errorOccurred("AO3 did not return an HTML download"); return; }
        if((format=="pdf" && !data.startsWith("%PDF-")) || (format=="epub" && !data.startsWith("PK")) || ((format=="mobi" || format=="azw3") && data.mid(60,8)!="BOOKMOBI")) { emit errorOccurred("AO3 returned an invalid download"); return; }
        QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit()) { emit errorOccurred("Could not save download"); return; }
        emit officialDownloadSaved(path);
    });
}

namespace {
QString ao3NetworkError(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(status==429)return QStringLiteral("AO3 rate limit reached (HTTP 429). Wait before trying again.");
    if(status==404)return QStringLiteral("AO3 returned HTTP 404 for %1. Check the same work in the website view.").arg(reply->url().path());
    if (status == 418 || status == 503) {
        const QString body = QString::fromUtf8(reply->peek(65536));
        if (body.contains(QStringLiteral("archive is down for maintenance"), Qt::CaseInsensitive))
            return QStringLiteral("AO3 is down for maintenance (HTTP %1). Check www.otwstatus.org and try again later.").arg(status);
    }
    if (status == 418)
        return QStringLiteral("AO3 temporarily refused this request (HTTP 418). Check www.otwstatus.org before trying again.");
    return reply->errorString();
}
bool ao3RequestFailed(QNetworkReply *reply)
{
    return reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() >= 400;
}
}

AO3Client::AO3Client(LoginManager *loginManager, QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_sessionManager(new QNetworkAccessManager(this))
    , m_loginManager(loginManager)
    , m_userAgent(QStringLiteral("AO3Client/3.0 (Unofficial Desktop Reader)"))
{
#ifdef Q_OS_ANDROID
    m_userAgent = QStringLiteral("AO3Client/1.0 (Qt Android Client)");
#endif
    m_userAgent=QSettings().value("network/userAgent",m_userAgent).toString();
    m_manager->setCookieJar(m_loginManager);
}

QNetworkRequest AO3Client::makeRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", m_userAgent.toUtf8());
    request.setRawHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    request.setTransferTimeout(30000);
    return request;
}

LoginManager *AO3Client::loginManager() const
{
    return m_loginManager;
}

void AO3Client::login(const QString &login, const QString &password)
{
    m_kudosStatus.clear();
    m_workAccountPending.clear();
    const quint64 generation = ++m_authGeneration;
    QNetworkRequest request = makeRequest(QUrl(QStringLiteral("https://archiveofourown.org/users/login")));

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, login, password, generation]() {
        reply->deleteLater();
        if (generation != m_authGeneration) return;
        if (ao3RequestFailed(reply)) {
            emit loginFailed(QStringLiteral("Could not reach login page: %1").arg(ao3NetworkError(reply)));
            return;
        }

        QString html = QString::fromUtf8(reply->readAll());
        QString token = extractCsrfToken(html);

        if (token.isEmpty()) {
            emit loginFailed(QStringLiteral("Could not find authenticity token"));
            return;
        }

        m_loginManager->setAuthenticityToken(token);

        QUrl postUrl(QStringLiteral("https://archiveofourown.org/users/login"));
        QNetworkRequest postRequest = makeRequest(postUrl);
        postRequest.setRawHeader("User-Agent", m_userAgent.toUtf8());
        postRequest.setRawHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        postRequest.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
        postRequest.setRawHeader("Referer", "https://archiveofourown.org/users/login");

        QByteArray postData;
        postData += "authenticity_token=" + QUrl::toPercentEncoding(token);
        postData += "&user%5Blogin%5D=" + QUrl::toPercentEncoding(login);
        postData += "&user%5Bpassword%5D=" + QUrl::toPercentEncoding(password);
        postData += "&user%5Bremember_me%5D=1";
        postData += "&commit=Sign+in";

        QNetworkReply *postReply = m_manager->post(postRequest, postData);
        connect(postReply, &QNetworkReply::finished, this, [this, postReply, generation]() {
            postReply->deleteLater();
            if (generation != m_authGeneration) return;
            if (postReply->error() != QNetworkReply::NoError &&
                postReply->error() != QNetworkReply::ContentAccessDenied) {
                emit loginFailed(QStringLiteral("Login request failed: %1").arg(ao3NetworkError(postReply)));
                return;
            }
            // Confirm the authenticated identity with a fresh server request.
            verifySession(true, generation);
        });
    });
}

void AO3Client::checkSession()
{
    verifySession(false, m_authGeneration);
}

void AO3Client::logout()
{
    m_kudosStatus.clear();
    m_workAccountPending.clear();
    ++m_commentsGeneration;
    ++m_authGeneration;
    ++m_listingGeneration;
    ++m_sessionImportGeneration;
    for(auto *reply:m_sessionManager->findChildren<QNetworkReply *>())if(!reply->isFinished())reply->abort();
    for (auto *reply : m_manager->findChildren<QNetworkReply *>()) {
        if (!reply->isFinished()) reply->abort();
    }
    m_loginManager->logout();
    emit loggedOut();
}

void AO3Client::giveKudos(const QString &id) {
    if(!QRegularExpression("^[0-9]+$").match(id).hasMatch() || m_kudosStatus.contains(id)) return;
    const auto generation=m_authGeneration;
    const auto expected=m_loginManager->isLoggedIn()?m_loginManager->username():QString();
    const QUrl source("https://archiveofourown.org/works/"+id+"?view_adult=true");
    m_kudosStatus[id]="Sending kudos…"; emit kudosChanged(id,m_kudosStatus[id],{});
    auto fresh=makeRequest(source);fresh.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::AlwaysNetwork);fresh.setRawHeader("Cache-Control","no-cache");
    auto *reply=m_manager->get(fresh);
    connect(reply,&QNetworkReply::finished,this,[this,reply,id,generation,expected,source] {
        reply->deleteLater(); if(generation!=m_authGeneration)return;
        const auto html=QString::fromUtf8(reply->readAll());
        const auto fields=parseKudosForm(html,id);
        if(reply->error()!=QNetworkReply::NoError || fields.isEmpty() || !siteIdentityMatches(html,expected)) {
            m_kudosStatus.remove(id); emit kudosChanged(id,{},"Kudos could not be sent. Check your sign-in and whether kudos are available for this work."); return;
        }
        QByteArray body;for(auto it=fields.cbegin();it!=fields.cend();++it) { if(!body.isEmpty())body+='&';body+=QUrl::toPercentEncoding(it.key())+'='+QUrl::toPercentEncoding(it.value()); }
        auto request=makeRequest(QUrl("https://archiveofourown.org/kudos"));
        request.setRawHeader("Content-Type","application/x-www-form-urlencoded"); request.setRawHeader("Referer",source.toEncoded());
        auto *posted=m_manager->post(request,body);
        connect(posted,&QNetworkReply::finished,this,[this,posted,id,generation] {
            posted->deleteLater();if(generation!=m_authGeneration)return;
            auto result=posted->error()==QNetworkReply::NoError?parseKudosResult(QString::fromUtf8(posted->readAll())):KudosResult{"Could not confirm kudos. Check this work on AO3.",false,true};
            m_kudosStatus[id]=result.success?"Kudos given":result.uncertain?"Check kudos on AO3":"Kudos unavailable";
            emit kudosChanged(id,m_kudosStatus[id],result.message);
        });
    });
}

void AO3Client::verifySession(bool loginAttempt, quint64 generation)
{
    auto request = makeRequest(QUrl(QStringLiteral("https://archiveofourown.org/")));
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setRawHeader("Cache-Control", "no-cache");
    auto *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, loginAttempt, generation]() {
        reply->deleteLater();
        if (generation != m_authGeneration) return;
        if (ao3RequestFailed(reply)) {
            const QString message = QStringLiteral("Could not verify session: %1").arg(ao3NetworkError(reply));
            if (loginAttempt) emit loginFailed(message);
            else emit errorOccurred(message);
            return;
        }
        const QString html = QString::fromUtf8(reply->readAll());
        const auto identity=parseSiteIdentity(html);
        UserInfo user;
        if(identity.logout && !identity.user.isEmpty())user={identity.user,"https://archiveofourown.org/users/"+identity.user,true};
        if(!user.isLoggedIn && (!identity.login || identity.logout)) {
            const auto message = QStringLiteral("Could not verify session. Try again when AO3 is available.");
            if (loginAttempt) emit loginFailed(message);
            else emit errorOccurred(message);
            return;
        }
        m_loginManager->setUserInfo(user);
        if (loginAttempt) {
            if (user.isLoggedIn) emit loginSuccessful(user);
            else emit loginFailed(QStringLiteral("Sign-in could not be confirmed. Check your username and password."));
        } else {
            emit sessionChecked(user.isLoggedIn, user);
        }
    });
}

void AO3Client::search(const QString &query, int page, const SearchFilters &filters)
{
    const quint64 generation = ++m_listingGeneration;
    QUrl url(QStringLiteral("https://archiveofourown.org/works/search"));
    QUrlQuery urlQuery;
    const bool excludeSingle=filters.value("single_chapter").contains("exclude");
    const auto effectiveQuery=excludeSingle?(query.trimmed().isEmpty()?QString():"("+query+") AND ")+"-expected_number_of_chapters:1":query;
    urlQuery.addQueryItem(QStringLiteral("work_search[query]"), effectiveQuery);
    for (auto it = filters.cbegin(); it != filters.cend(); ++it) {
        QString key = it.key();
        if(key=="single_chapter" && excludeSingle)continue;
        const bool array = key.endsWith(QStringLiteral("[]"));
        if (array) key.chop(2);
        for (const auto &value : it.value())
            if (!value.isEmpty()) urlQuery.addQueryItem(QStringLiteral("work_search[%1]%2").arg(key, array ? QStringLiteral("[]") : QString()), value);
    }
    urlQuery.addQueryItem(QStringLiteral("page"), QString::number(page));
    url.setQuery(urlQuery);

    QNetworkRequest request = makeRequest(url);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, query, page, generation]() {
        reply->deleteLater();
        if (generation != m_listingGeneration) return;
        if (ao3RequestFailed(reply)) {
            SearchResult result;
            result.errorMessage = ao3NetworkError(reply);
            emit searchCompleted(result);
            return;
        }
        QString html = QString::fromUtf8(reply->readAll());
        SearchResult result = parseSearchResults(html, query, page);
        emit searchCompleted(result);
    });
}

void AO3Client::searchDiscovery(const QString &kind,const QMap<QString,QString> &values,int page)
{
    const auto url=DiscoverySearch::url(kind,values,page);if(url.isEmpty())return;
    page=qBound(1,page,1000000);const auto revision=++m_listingGeneration;const auto auth=m_authGeneration;
    auto *reply=m_manager->get(makeRequest(url));
    connect(reply,&QNetworkReply::finished,this,[this,reply,kind,page,revision,auth]{
        reply->deleteLater();if(revision!=m_listingGeneration || auth!=m_authGeneration)return;
        ArchivePage result;if(ao3RequestFailed(reply))result.error=ao3NetworkError(reply);else result=DiscoverySearch::parse(QString::fromUtf8(reply->readAll()),kind,page);
        emit archivePageFetched("search:"+kind,{},result);
    });
}

void AO3Client::fetchWorkSeries(const QString &workId)
{
    if(!QRegularExpression("^[0-9]+$").match(workId).hasMatch())return;
    const auto auth=m_authGeneration;
    auto *reply=m_manager->get(makeRequest(QUrl("https://archiveofourown.org/works/"+workId+"?view_adult=true")));
    connect(reply,&QNetworkReply::finished,this,[this,reply,workId,auth] {
        reply->deleteLater();if(auth!=m_authGeneration)return;
        WorkSeries result;
        if(ao3RequestFailed(reply))result.error=ao3NetworkError(reply);
        else result=parseWorkSeries(QString::fromUtf8(reply->readAll()));
        emit workSeriesFetched(workId,result);
    });
}

void AO3Client::fetchWorkDetails(const QString &workId)
{
    if(!QRegularExpression("^[0-9]+$").match(workId).hasMatch())return;
    const auto auth=m_authGeneration;
    QNetworkRequest request = makeRequest(
        QStringLiteral("https://archiveofourown.org/works/%1?view_adult=true").arg(workId));
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, workId, auth]() {
        reply->deleteLater();
        if(auth!=m_authGeneration)return;
        if (ao3RequestFailed(reply)) {
            emit errorOccurred(ao3NetworkError(reply));
            return;
        }
        QString html = QString::fromUtf8(reply->readAll());
        Work work = parseWorkDetails(html, workId);
        if(work.title.isEmpty()){emit errorOccurred("AO3 did not return this work. Check whether it loads on the website.");return;}
        emit workDetailsFetched(work);
    });
}

void AO3Client::fetchWorkContent(const QString &workId)
{
    QNetworkRequest request = makeRequest(
        QStringLiteral("https://archiveofourown.org/works/%1?view_adult=true&view_full_work=true").arg(workId));
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, workId]() {
        reply->deleteLater();
        if (ao3RequestFailed(reply)) {
            emit errorOccurred(QStringLiteral("Failed to fetch work: %1").arg(ao3NetworkError(reply)));
            return;
        }
        QString html = QString::fromUtf8(reply->readAll());
        QString title;
        QRegularExpression trx(QStringLiteral(R"regex(<h2\s+class="title[^"]*"[^>]*>(.*?)</h2>)regex"),
                               QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch tm = trx.match(html);
        if (tm.hasMatch()) {
            title = tm.captured(1).trimmed();
            QTextDocument decodedTitle;
            decodedTitle.setHtml(title);
            title = decodedTitle.toPlainText().trimmed();
        }

        QString content = extractWorkContent(html);
        if (content.isEmpty()) {
            emit errorOccurred(QStringLiteral("AO3 returned no readable chapter text. The work may require sign-in or be unavailable."));
            return;
        }
        emit workContentFetched(workId, title, content);
        emit workChaptersFetched(workId, title, extractWorkChapters(html));
    });
}

void AO3Client::downloadWork(const QString &workId)
{
    QNetworkRequest request = makeRequest(
        QStringLiteral("https://archiveofourown.org/works/%1?view_adult=true&view_full_work=true").arg(workId));
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, workId]() {
        reply->deleteLater();
        if (ao3RequestFailed(reply)) {
            emit errorOccurred(QStringLiteral("Download failed: %1").arg(ao3NetworkError(reply)));
            return;
        }
        emit workDownloaded(workId, QString::fromUtf8(reply->readAll()));
    });
}

void AO3Client::fetchDashboard()
{
    UserInfo info = m_loginManager->userInfo();
    if (!info.isLoggedIn || info.dashboardUrl.isEmpty()) {
        emit errorOccurred(QStringLiteral("Not logged in"));
        return;
    }

    QNetworkRequest request = makeRequest(info.dashboardUrl);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (ao3RequestFailed(reply)) {
            emit errorOccurred(ao3NetworkError(reply));
            return;
        }
    });
}

void AO3Client::fetchArchivePage(const QString &kind,const QString &id,int page)
{
    const auto user=m_loginManager->username();const auto route=ArchivePages::route(kind,user,id);
    if(route.isEmpty() || (kind!="series" && !m_loginManager->isLoggedIn())){emit errorOccurred("Sign in to open this AO3 list.");return;}
    page=qBound(1,page,1000000);const auto generation=++m_listingGeneration;const auto auth=m_authGeneration;
    auto *reply=m_manager->get(makeRequest(QUrl("https://archiveofourown.org"+route+"?page="+QString::number(page))));
    connect(reply,&QNetworkReply::finished,this,[this,reply,kind,id,page,user,generation,auth]{reply->deleteLater();if(auth!=m_authGeneration || generation!=m_listingGeneration || (kind!="series" && (!m_loginManager->isLoggedIn() || user!=m_loginManager->username())))return;
        ArchivePage out;if(ao3RequestFailed(reply))out.error=ao3NetworkError(reply);else out=ArchivePages::parse(QString::fromUtf8(reply->readAll()),kind,user,id,page);emit archivePageFetched(kind,id,out);
    });
}
void AO3Client::performArchiveAction(const QString &kind,const QString &id,int page,const QString &entry,const QString &label,const QStringList &selection)
{
    if(!QStringList{"subscriptions","inbox","history"}.contains(kind) || !m_loginManager->isLoggedIn())return;
    if(kind=="history" && label=="Clear history")page=1;
    const auto user=m_loginManager->username();const auto route=ArchivePages::route(kind,user,id);if(route.isEmpty())return;
    const auto key=kind=="history"?kind:kind+":"+entry;if(m_listMutations.contains(key))return;m_listMutations.insert(key);const auto auth=m_authGeneration;
    const QUrl source("https://archiveofourown.org"+route+(kind=="history" && label=="Clear history"?"/confirm_clear":"?page="+QString::number(qBound(1,page,1000000))));
    auto *fresh=m_manager->get(makeRequest(source));
    connect(fresh,&QNetworkReply::finished,this,[this,fresh,source,kind,id,page,entry,label,user,key,auth,selection]{
        fresh->deleteLater();if(auth!=m_authGeneration){m_listMutations.remove(key);return;}
        const auto freshHtml=QString::fromUtf8(fresh->readAll());PageAction action;if(ao3RequestFailed(fresh))action.error=ao3NetworkError(fresh);else action=ArchivePages::action(freshHtml,kind,user,id,page,entry,label);
        QStringList selectedKeys;
        if(!selection.isEmpty() && action.error.isEmpty()) {
            if(kind!="inbox" || selection.size()>100)action.error="Invalid inbox selection.";
            else {const auto current=ArchivePages::parse(freshHtml,kind,user,id,page);for(const auto &selected:selection){const ArchiveEntry *found=nullptr;for(const auto &item:current.entries)if(item.id==selected){found=&item;break;}if(!found || !found->actions.contains(label)){action.error="The selected entries changed. Refresh your inbox.";break;}selectedKeys.append(found->selection);}if(action.error.isEmpty())action.fields.remove("inbox_comments[]");}
        }
        if(!action.error.isEmpty()){m_listMutations.remove(key);emit archiveActionFinished(action.error,false);return;}
        QByteArray data;for(auto it=action.fields.cbegin();it!=action.fields.cend();++it){if(!data.isEmpty())data+='&';data+=QUrl::toPercentEncoding(it.key())+'='+QUrl::toPercentEncoding(it.value());}
        for(const auto &selected:selectedKeys){if(!data.isEmpty())data+='&';data+=QUrl::toPercentEncoding("inbox_comments[]")+'='+QUrl::toPercentEncoding(selected);}
        auto request=makeRequest(QUrl("https://archiveofourown.org"+action.path));request.setRawHeader("Content-Type","application/x-www-form-urlencoded");request.setRawHeader("Referer",source.toEncoded());
        auto *posted=m_manager->post(request,data);
        connect(posted,&QNetworkReply::finished,this,[this,posted,kind,id,page,entry,label,user,key,auth,selection]{
            posted->deleteLater();m_listMutations.remove(key);if(auth!=m_authGeneration)return;
            ArchivePage out;bool confirmed=false;if(!ao3RequestFailed(posted)){out=ArchivePages::parse(QString::fromUtf8(posted->readAll()),kind,user,id,page);if(out.error.isEmpty()){const ArchiveEntry *found=nullptr;for(const auto &e:out.entries)if(e.id==entry){found=&e;break;}confirmed=label=="Clear history"?out.entries.isEmpty():(label=="Unsubscribe" || label=="Delete from inbox" || label=="Delete from history")?!found:label=="Mark read"?found && !found->unread:label=="Mark unread" && found && found->unread;}}
            if(confirmed && !selection.isEmpty())for(const auto &selected:selection){const ArchiveEntry *found=nullptr;for(const auto &item:out.entries)if(item.id==selected){found=&item;break;}confirmed &= label=="Delete from inbox"?!found:label=="Mark read"?found && !found->unread:label=="Mark unread" && found && found->unread;}
            if(confirmed){emit archivePageFetched(kind,id,out);emit archiveActionFinished("Change confirmed by AO3.",false);}else emit archiveActionFinished("Could not confirm the change. Refresh the AO3 list before trying again.",true);
        });
    });
}

void AO3Client::fetchAccountReadings(bool later,int page)
{
    const auto info=m_loginManager->userInfo();
    if(!info.isLoggedIn || !QRegularExpression("^[A-Za-z0-9_]+$").match(info.username).hasMatch()){emit errorOccurred("Sign in to view your AO3 reading lists.");return;}
    const auto generation=++m_listingGeneration;const auto auth=m_authGeneration;page=qBound(1,page,1000000);
    QUrl url("https://archiveofourown.org/users/"+info.username+"/readings");QUrlQuery query;query.addQueryItem("page",QString::number(page));if(later)query.addQueryItem("show","to-read");url.setQuery(query);
    auto *reply=m_manager->get(makeRequest(url));
    connect(reply,&QNetworkReply::finished,this,[this,reply,later,page,info,generation,auth] {
        reply->deleteLater();if(generation!=m_listingGeneration || auth!=m_authGeneration || !m_loginManager->isLoggedIn() || m_loginManager->username()!=info.username)return;
        SearchResult result;
        if(ao3RequestFailed(reply))result.errorMessage=ao3NetworkError(reply);
        else {
            const auto document=accountReadingDocument(QString::fromUtf8(reply->readAll()),info.username,page);
            if(!document.error.isEmpty())result.errorMessage=document.error;
            else {result=parseSearchResults(document.listingHtml,{},page);result.totalPages=document.pages;result.unavailableWorks=document.unavailable;for(auto &work:result.works)work.readingInfo=document.activity.value(work.id);}
        }
        emit accountReadingsFetched(later,result);
    });
}

void AO3Client::fetchUserWorks(int page)
{
    const quint64 generation = ++m_listingGeneration;
    UserInfo info = m_loginManager->userInfo();
    if (!info.isLoggedIn || info.dashboardUrl.isEmpty()) {
        emit errorOccurred(QStringLiteral("Not logged in"));
        return;
    }

    QUrl url(info.dashboardUrl + QStringLiteral("/works"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page"), QString::number(page));
    url.setQuery(query);

    QNetworkRequest request = makeRequest(url);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, page, generation]() {
        reply->deleteLater();
        if (generation != m_listingGeneration) return;
        if (ao3RequestFailed(reply)) {
            SearchResult result;
            result.errorMessage = ao3NetworkError(reply);
            emit userWorksFetched(result);
            return;
        }
        QString html = QString::fromUtf8(reply->readAll());
        SearchResult result = parseSearchResults(html, QString(), page);
        emit userWorksFetched(result);
    });
}

namespace {
QString attribute(const QString &attributes, const QString &name)
{
    const QRegularExpression rx(QStringLiteral(R"regex(\b%1\s*=\s*(["'])(.*?)\1)regex").arg(name),
                                QRegularExpression::DotMatchesEverythingOption);
    return rx.match(attributes).captured(2);
}

bool hasClass(const QString &attributes, const QString &name)
{
    return attribute(attributes, QStringLiteral("class"))
        .split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).contains(name);
}

QString plainHtml(const QString &html)
{
    QTextDocument document;
    document.setHtml(html);
    return document.toPlainText().trimmed();
}

QString element(const QString &html, const QString &tag, const QString &className)
{
    const QRegularExpression rx(QStringLiteral("<%1\\b([^>]*)>(.*?)</%1>").arg(tag),
                                QRegularExpression::DotMatchesEverythingOption);
    auto matches = rx.globalMatch(html);
    while (matches.hasNext()) {
        const auto match = matches.next();
        if (hasClass(match.captured(1), className)) return match.captured(2);
    }
    return {};
}

int statistic(const QString &html, const QString &name)
{
    QString text = plainHtml(element(html, QStringLiteral("dd"), name));
    text.remove(QRegularExpression(QStringLiteral("[,\\s]")));
    return text.toInt();
}
}

SearchResult AO3Client::parseSearchResults(const QString &html, const QString &, int page)
{
    SearchResult result;
    result.currentPage = qMax(page, 1);
    result.totalPages = result.currentPage;
    const QRegularExpression listTags(QStringLiteral("<(/?)li\\b([^>]*)>"));
    auto items = listTags.globalMatch(html);
    int depth = 0;
    int workDepth = -1;
    int workStart = -1;
    QString workId;
    while (items.hasNext()) {
        const auto item = items.next();
        if (item.captured(1).isEmpty()) {
            ++depth;
            const QString id = attribute(item.captured(2), QStringLiteral("id"));
            if (workStart < 0 && ((hasClass(item.captured(2), QStringLiteral("work")) && QRegularExpression(QStringLiteral("^work_\\d+$")).match(id).hasMatch()) ||
                (hasClass(item.captured(2), QStringLiteral("bookmark")) && QRegularExpression(QStringLiteral("^bookmark_\\d+$")).match(id).hasMatch()))) {
                workId = id.startsWith("work_") ? id.mid(5) : QString();
                workStart = item.capturedEnd();
                workDepth = depth;
            }
            continue;
        }
        const bool completeWork = workStart >= 0 && depth == workDepth;
        depth = qMax(0, depth - 1);
        if (!completeWork) continue;
        const QString workHtml = html.mid(workStart, item.capturedStart() - workStart);
        workStart = -1;
        Work work;
        work.id = workId;
        const QString heading = element(workHtml, QStringLiteral("h4"), QStringLiteral("heading"));
        const QRegularExpression links(QStringLiteral("<a\\b([^>]*)>(.*?)</a>"),
                                       QRegularExpression::DotMatchesEverythingOption);
        auto anchors = links.globalMatch(heading);
        QStringList authors;
        while (anchors.hasNext()) {
            const auto anchor = anchors.next();
            const QString href = attribute(anchor.captured(1), QStringLiteral("href"));
            if (QRegularExpression("^/works/[0-9]+$").match(href).hasMatch() && (work.id.isEmpty() || href == QStringLiteral("/works/%1").arg(work.id))) {
                work.id = href.section('/', 2, 2); work.title = plainHtml(anchor.captured(2));
            }
            if (attribute(anchor.captured(1), QStringLiteral("rel")) == QStringLiteral("author")) {
                authors.append(plainHtml(anchor.captured(2)));
                if (work.authorUrl.isEmpty()) work.authorUrl = href;
            }
        }
        work.author = authors.join(QStringLiteral(", "));
        work.summary = plainHtml(element(workHtml, QStringLiteral("blockquote"), QStringLiteral("summary")));
        work.fandom = plainHtml(element(workHtml, QStringLiteral("h5"), QStringLiteral("fandoms")));
        const QRegularExpression spans(QStringLiteral("<span\\b([^>]*)>"));
        auto badges = spans.globalMatch(workHtml);
        while (badges.hasNext()) {
            const auto attrs = badges.next().captured(1);
            const QString title = plainHtml(attribute(attrs, QStringLiteral("title")));
            if (hasClass(attrs, QStringLiteral("rating"))) work.rating = title;
            if (hasClass(attrs, QStringLiteral("warnings"))) work.warnings = title;
            if (hasClass(attrs, QStringLiteral("category"))) work.category = title;
        }
        const QString stats = element(workHtml, QStringLiteral("dl"), QStringLiteral("stats"));
        work.wordCount = statistic(stats, QStringLiteral("words"));
        work.commentCount = statistic(stats, QStringLiteral("comments"));
        work.kudosCount = statistic(stats, QStringLiteral("kudos"));
        work.hitCount = statistic(stats, QStringLiteral("hits"));
        work.language = plainHtml(element(stats, QStringLiteral("dd"), QStringLiteral("language")));
        const QStringList chapters = plainHtml(element(stats, QStringLiteral("dd"), QStringLiteral("chapters"))).split('/');
        if (chapters.size() == 2) {
            work.chaptersWritten = chapters[0].trimmed().toInt();
            work.chaptersTotal = chapters[1].trimmed() == QStringLiteral("?") ? -1 : chapters[1].trimmed().toInt();
            work.complete = work.chaptersTotal > 0 && work.chaptersWritten == work.chaptersTotal;
        }
        const QRegularExpression tags(QStringLiteral("<li\\b([^>]*)>\\s*<a\\b[^>]*>(.*?)</a>"),
                                     QRegularExpression::DotMatchesEverythingOption);
        auto tagItems = tags.globalMatch(workHtml);
        while (tagItems.hasNext()) {
            const auto tag = tagItems.next();
            const QString text = plainHtml(tag.captured(2));
            if (hasClass(tag.captured(1), QStringLiteral("freeforms"))) work.tags.append(text);
            if (hasClass(tag.captured(1), QStringLiteral("relationships"))) work.relationships.append(text);
            if (hasClass(tag.captured(1), QStringLiteral("characters"))) work.characters.append(text);
            if (hasClass(tag.captured(1), QStringLiteral("warnings")) && work.warnings.isEmpty()) work.warnings = text;
        }
        anchors = links.globalMatch(workHtml);
        while (anchors.hasNext()) {
            const auto anchor = anchors.next();
            const QString href = attribute(anchor.captured(1), QStringLiteral("href"));
            if (href == QStringLiteral("/works/%1/bookmarks").arg(work.id))
                work.bookmarksUrl = QStringLiteral("https://archiveofourown.org") + href;
        }
        if (!work.id.isEmpty()) result.works.append(work);
    }
    const QString pagination = element(html, QStringLiteral("ol"), QStringLiteral("pagination"));
    const QString current = plainHtml(element(pagination, QStringLiteral("em"), QStringLiteral("current")));
    if (current.toInt() > 0) result.currentPage = current.toInt();
    result.totalPages = result.currentPage;
    const QRegularExpression links(QStringLiteral("<a\\b([^>]*)>(.*?)</a>"), QRegularExpression::DotMatchesEverythingOption);
    auto anchors = links.globalMatch(pagination);
    while (anchors.hasNext()) {
        const auto anchor = anchors.next();
        const QUrl url(plainHtml(attribute(anchor.captured(1), QStringLiteral("href"))));
        const int number = QUrlQuery(url).queryItemValue(QStringLiteral("page")).toInt();
        result.totalPages = qMax(result.totalPages, number);
    }
    QString total = plainHtml(element(html, QStringLiteral("span"), QStringLiteral("estimated-count")));
    if (total.isEmpty()) {
        const QString heading = plainHtml(element(html, QStringLiteral("h2"), QStringLiteral("heading")));
        const QRegularExpression count(QStringLiteral("([\\d,]+)\\s+[Ww]orks?\\b"));
        total = count.match(heading).captured(1);
    }
    if (total.isEmpty()) {
        const QString heading = plainHtml(element(html, QStringLiteral("h3"), QStringLiteral("heading")));
        const QRegularExpression found(QStringLiteral("([\\d,]+)\\s+Found\\b"));
        total = found.match(heading).captured(1);
    }
    result.totalResults = total.remove(',').toInt();
    if (!result.totalResults) result.totalResults = result.works.size();
    // A login/challenge/error document must not be presented as an empty result set.
    const QRegularExpression listing(QStringLiteral("<ol\\b([^>]*)>"));
    auto lists = listing.globalMatch(html);
    bool foundListing = false;
    while (lists.hasNext()) { const auto attrs = lists.next().captured(1); foundListing |= hasClass(attrs, QStringLiteral("work")) || hasClass(attrs, QStringLiteral("bookmark")); }
    const bool noResults = plainHtml(element(html, QStringLiteral("h2"), QStringLiteral("heading"))) == QStringLiteral("Search Results")
        && html.contains(QStringLiteral("No results found."));
    if (result.works.isEmpty() && !foundListing && total.isEmpty() && !noResults)
        result.errorMessage = QStringLiteral("AO3 did not return a work listing. Please try again.");
    return result;
}

Work AO3Client::parseWorkDetails(const QString &html, const QString &workId)
{
    Work work;
    work.id = workId;

    const auto parts=CommentHtml::nodes(html);
    QStringList authors;
    for(int i=0;i<parts.size();++i) {
        const auto &node=parts[i];bool preface=false,skin=false,story=false,byline=false;
        for(int p=i;p>=0;p=parts[p].parent){preface|=CommentHtml::hasClass(parts[p],"preface");skin|=parts[p].attrs.value("id")=="workskin";story|=CommentHtml::hasClass(parts[p],"userstuff");byline|=CommentHtml::hasClass(parts[p],"byline");}
        if(!skin || !preface || story)continue;
        const auto text=CommentHtml::text(html.mid(node.begin,node.end-node.begin));
        if(node.tag=="h2" && CommentHtml::hasClass(node,"title") && work.title.isEmpty())work.title=text;
        if(byline && node.tag=="a" && node.attrs.value("rel").split(' ').contains("author")) {
            authors.append(text);if(work.authorUrl.isEmpty())work.authorUrl=node.attrs.value("href");
        }
    }
    work.author=authors.isEmpty()?QString("Anonymous"):authors.join(", ");

    QRegularExpression summaryRx(QStringLiteral(R"regex(<div\s+class="summary"[^>]*>.*?<blockquote[^>]*>(.*?)</blockquote>)regex"),
                                 QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch sm = summaryRx.match(html);
    if (sm.hasMatch()) {
        QString s = sm.captured(1).trimmed();
        s.replace(QRegularExpression(QStringLiteral("<[^>]*>")), QStringLiteral(""));
        s.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
        work.summary = s.trimmed();
    }

    QRegularExpression statsRx(QStringLiteral(R"regex(<dl\s+class="stats"[^>]*>(.*?)</dl>)regex"),
                               QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch stm = statsRx.match(html);
    if (stm.hasMatch()) {
        QString statsHtml = stm.captured(1);
        QRegularExpression wordsRx(QStringLiteral(R"regex(<dd[^>]*class="words"[^>]*>(.*?)</dd>)regex"));
        QRegularExpressionMatch wm = wordsRx.match(statsHtml);
        if (wm.hasMatch())
            work.wordCount = wm.captured(1).remove(',').toInt();

        QRegularExpression chaptersRx(QStringLiteral(R"regex(<dd[^>]*class="chapters"[^>]*>(.*?)</dd>)regex"));
        QRegularExpressionMatch cm = chaptersRx.match(statsHtml);
        if (cm.hasMatch()) {
            QString ch = cm.captured(1).trimmed();
            ch.replace(QRegularExpression(QStringLiteral("<[^>]*>")), QStringLiteral(""));
            if (ch.contains('/')) {
                QStringList parts = ch.split('/');
                work.chaptersWritten = parts[0].toInt();
                work.chaptersTotal = parts[1] == QStringLiteral("?") ? -1 : parts[1].toInt();
                if (work.chaptersTotal > 0 && parts[0] == parts[1])
                    work.complete = true;
            }
        }

        QRegularExpression commentsRx(QStringLiteral(R"regex(<dd[^>]*class="comments"[^>]*>(.*?)</dd>)regex"));
        QRegularExpressionMatch comM = commentsRx.match(statsHtml);
        if (comM.hasMatch())
            work.commentCount = comM.captured(1).remove(',').toInt();

        QRegularExpression kudosRx(QStringLiteral(R"regex(<dd[^>]*class="kudos"[^>]*>(.*?)</dd>)regex"));
        QRegularExpressionMatch kM = kudosRx.match(statsHtml);
        if (kM.hasMatch())
            work.kudosCount = kM.captured(1).remove(',').toInt();

        QRegularExpression hitsRx(QStringLiteral(R"regex(<dd[^>]*class="hits"[^>]*>(.*?)</dd>)regex"));
        QRegularExpressionMatch hM = hitsRx.match(statsHtml);
        if (hM.hasMatch())
            work.hitCount = hM.captured(1).remove(',').toInt();
    }

    QRegularExpression ratingRx(QStringLiteral(R"regex(<span[^>]*class="[^"]*rating[^"]*"[^>]*title="([^"]*)"[^>]*>)regex"));
    QRegularExpressionMatch rm = ratingRx.match(html);
    if (rm.hasMatch())
        work.rating = rm.captured(1).trimmed();

    return work;
}

QString AO3Client::extractWorkContent(const QString &html)
{
    return extractWorkText(html);
}

QString AO3Client::extractCsrfToken(const QString &html)
{
    QRegularExpression tokenRx(QStringLiteral(R"regex(<input\s+(?:[^>]*\s+)?name="authenticity_token"\s+(?:[^>]*\s+)?value="([^"]+)"\s*/?>)regex"),
                               QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch m = tokenRx.match(html);
    if (m.hasMatch())
        return m.captured(1);

    QRegularExpression tokenRx2(QStringLiteral(R"regex(<meta\s+name="csrf-token"\s+content="([^"]+)"\s*/?>)regex"),
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch m2 = tokenRx2.match(html);
    if (m2.hasMatch())
        return m2.captured(1);

    return {};
}

void AO3Client::fetchSearchOptions()
{
    auto *reply = m_manager->get(makeRequest(QUrl(QStringLiteral("https://archiveofourown.org/works/search?edit_search=true"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        SearchOptions result;
        if (ao3RequestFailed(reply)) {
            result.error = QStringLiteral("Could not load AO3 choices: %1. You can retry.").arg(ao3NetworkError(reply));
            emit searchOptionsFetched(result); return;
        }
        const QString html = QString::fromUtf8(reply->readAll());
        const QRegularExpression selects(QStringLiteral("<select\\b([^>]*)>(.*?)</select>"), QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpression options(QStringLiteral("<option\\b([^>]*)>(.*?)</option>"), QRegularExpression::DotMatchesEverythingOption);
        auto groups = selects.globalMatch(html);
        while (groups.hasNext()) {
            const auto group = groups.next();
            const QString name = attribute(group.captured(1), QStringLiteral("name"));
            QString key;
            if (name == QStringLiteral("work_search[rating_ids]")) key = QStringLiteral("rating_ids");
            if (name == QStringLiteral("work_search[language_id]")) key = QStringLiteral("language_id");
            if (key.isEmpty()) continue;
            auto choices = options.globalMatch(group.captured(2));
            while (choices.hasNext()) {
                const auto choice = choices.next();
                const QString value = attribute(choice.captured(1), QStringLiteral("value"));
                if (!value.isEmpty()) result.choices[key].append({plainHtml(choice.captured(2)), value});
            }
        }
        const QRegularExpression inputs(QStringLiteral("<input\\b([^>]*)>"));
        const QRegularExpression labels(QStringLiteral("<label\\b([^>]*)>(.*?)</label>"), QRegularExpression::DotMatchesEverythingOption);
        QMap<QString, QString> names;
        auto label = labels.globalMatch(html);
        while (label.hasNext()) { const auto item = label.next(); names[attribute(item.captured(1), QStringLiteral("for"))] = plainHtml(item.captured(2)); }
        auto boxes = inputs.globalMatch(html);
        while (boxes.hasNext()) {
            const auto attrs = boxes.next().captured(1);
            const QString name = attribute(attrs, QStringLiteral("name"));
            QString key;
            if (name == QStringLiteral("work_search[archive_warning_ids][]")) key = QStringLiteral("archive_warning_ids[]");
            if (name == QStringLiteral("work_search[category_ids][]")) key = QStringLiteral("category_ids[]");
            const QString value = attribute(attrs, QStringLiteral("value"));
            const QString text = names.value(attribute(attrs, QStringLiteral("id")));
            if (!key.isEmpty() && !value.isEmpty() && !text.isEmpty()) result.choices[key].append({text, value});
        }
        if (result.choices.value(QStringLiteral("rating_ids")).isEmpty() || result.choices.value(QStringLiteral("language_id")).isEmpty())
            result.error = QStringLiteral("AO3 did not return its filter choices. Please retry.");
        emit searchOptionsFetched(result);
    });
}

void AO3Client::fetchTagSuggestions(const QString &field, const QString &type, const QString &term, quint64 revision)
{
    if (!QStringList{QStringLiteral("fandom"), QStringLiteral("character"), QStringLiteral("relationship"), QStringLiteral("freeform"), QStringLiteral("tag")}.contains(type) || term.trimmed().size() < 2) return;
    QUrl url(QStringLiteral("https://archiveofourown.org/autocomplete/%1").arg(type));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("term"), term);
    url.setQuery(query);
    auto request = makeRequest(url);
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(10000);
    auto *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, field, revision] {
        reply->deleteLater();
        QStringList suggestions;
        QString error;
        if (ao3RequestFailed(reply)) error = ao3NetworkError(reply);
        else {
            const auto document = QJsonDocument::fromJson(reply->readAll());
            if (!document.isArray()) error = QStringLiteral("AO3 returned an unexpected response");
            else for (const auto &value : document.array()) {
                const QString name = value.isString() ? value.toString() : value.toObject().value(QStringLiteral("name")).toString();
                if (!name.isEmpty() && !suggestions.contains(name)) suggestions.append(name);
                if (suggestions.size() == 20) break;
            }
        }
        emit tagSuggestionsFetched(field, revision, suggestions, error);
    });
}

void AO3Client::fetchAuthorProfile(const QString &username)
{
    const quint64 generation = ++m_profileGeneration;
    if (!QRegularExpression("^[A-Za-z0-9_]+$").match(username).hasMatch()) { AuthorProfile profile; profile.error = "Invalid AO3 username"; emit authorProfileFetched(profile); return; }
    auto *reply = m_manager->get(makeRequest(QUrl("https://archiveofourown.org/users/" + username + "/profile")));
    connect(reply, &QNetworkReply::finished, this, [this, reply, username, generation] {
        reply->deleteLater(); if (generation != m_profileGeneration) return;
        AuthorProfile profile; profile.username = username;
        if (ao3RequestFailed(reply)) profile.error = ao3NetworkError(reply);
        else {
            const QString html = QString::fromUtf8(reply->readAll());
            auto divs = QRegularExpression("<div\\b([^>]*)>").globalMatch(html); int start = -1;
            while(divs.hasNext()) { const auto div = divs.next(); if(hasClass(div.captured(1), "user") && hasClass(div.captured(1), "profile")) { start = div.capturedEnd(); break; } }
            if(start < 0) profile.error = "AO3 did not return this user's public profile.";
            else { const auto avatar=profileAvatar(html);profile.avatarUrl=avatar.url;profile.avatarAlt=avatar.alt;const QString body = html.mid(start); profile.title = plainHtml(element(body, "h3", "heading")); profile.details = plainHtml(element(body, "dl", "meta")); profile.biography = plainHtml(element(body, "blockquote", "userstuff")); }
        }
        emit authorProfileFetched(profile);
    });
}
void AO3Client::fetchAuthorWorks(const QString &username, int page, bool bookmarks)
{
    const quint64 generation = ++m_listingGeneration;
    if (!QRegularExpression("^[A-Za-z0-9_]+$").match(username).hasMatch()) { SearchResult result; result.errorMessage = "Invalid AO3 username"; emit authorWorksFetched(username, bookmarks, result); return; }
    QUrl url("https://archiveofourown.org/users/" + username + (bookmarks ? "/bookmarks" : "/works")); QUrlQuery query; query.addQueryItem("page", QString::number(qMax(1, page))); url.setQuery(query);
    auto *reply = m_manager->get(makeRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, username, page, bookmarks, generation] {
        reply->deleteLater(); if (generation != m_listingGeneration) return;
        SearchResult result; if(ao3RequestFailed(reply)) result.errorMessage = ao3NetworkError(reply); else result = parseSearchResults(QString::fromUtf8(reply->readAll()), {}, page);
        emit authorWorksFetched(username, bookmarks, result);
    });
}

void AO3Client::fetchComments(const QString &workId,int page) {
    if(!QRegularExpression("^(?:[0-9]+|comment:[0-9]+)$").match(workId).hasMatch() || page<1) {emit errorOccurred("Invalid comments request");return;}
    const auto generation=m_authGeneration; const auto revision=++m_commentsGeneration;
    const auto route=workId.startsWith("comment:")?"/comments/"+workId.mid(8):"/works/"+workId+"/comments";
    auto *reply=m_manager->get(makeRequest(QUrl("https://archiveofourown.org"+route+"?page="+QString::number(page))));
    connect(reply,&QNetworkReply::finished,this,[this,reply,workId,page,generation,revision] {
        reply->deleteLater(); if(generation!=m_authGeneration || revision!=m_commentsGeneration)return;
        if(ao3RequestFailed(reply)) {emit errorOccurred(ao3NetworkError(reply));return;}
        const auto html=QString::fromUtf8(reply->readAll());auto result=parseCommentPage(html,page);ArchivePages::Document document(html);
        for(auto &comment:result.comments){comment.actions=commentControls(document,comment.id).keys();for(int i=0;i<document.list.size();++i)if(document.list[i].attrs.value("id")=="navigation_for_comment_"+comment.id && document.trusted(i)){comment.canReply=false;for(int j:document.select("li",{},i))if(document.list[j].attrs.value("id")=="add_comment_reply_link_"+comment.id)comment.canReply=true;break;}}
        if(!result.error.isEmpty())emit errorOccurred(result.error);else emit commentsFetched(workId,result);
    });
}

void AO3Client::fetchCommentEdit(const QString &context,const QString &id)
{
    if(!m_loginManager->isLoggedIn() || !QRegularExpression("^[0-9]+$").match(id).hasMatch())return;
    const auto user=m_loginManager->username();const auto auth=m_authGeneration;const QUrl source("https://archiveofourown.org/comments/"+id+"/edit");
    auto *reply=m_manager->get(makeRequest(source));connect(reply,&QNetworkReply::finished,this,[this,reply,context,id,user,auth,source]{reply->deleteLater();if(auth!=m_authGeneration)return;CommentForm form;
        if(ao3RequestFailed(reply))form.error=ao3NetworkError(reply);else {const auto html=QString::fromUtf8(reply->readAll());if(!siteIdentityMatches(html,user))form.error="Your AO3 session changed. Check native sign-in.";else {form=parseCommentForm(html);if(!form.editing || ArchivePages::path(form.action)!="/comments/"+id)form.error="AO3 did not return this comment's edit form.";}}
        form.context=id;form.source=source.toString();emit commentFormFetched(context,form);
    });
}
void AO3Client::manageComment(const QString &context,const QString &id,const QString &label)
{
    if(!m_loginManager->isLoggedIn() || !QRegularExpression("^[0-9]+$").match(id).hasMatch() || label=="Edit")return;
    const auto user=m_loginManager->username();const auto auth=m_authGeneration;const QUrl source("https://archiveofourown.org/comments/"+id);
    auto submit=[this,context,user,auth,source](const CommentControl &control){
        if(control.path.isEmpty()){emit commentSubmitted(context,{{},"AO3 did not return the confirmation form.",false});return;}
        QByteArray body;for(auto it=control.fields.cbegin();it!=control.fields.cend();++it){if(!body.isEmpty())body+='&';body+=QUrl::toPercentEncoding(it.key())+'='+QUrl::toPercentEncoding(it.value());}
        auto request=makeRequest(QUrl("https://archiveofourown.org"+control.path));request.setRawHeader("Content-Type","application/x-www-form-urlencoded");request.setRawHeader("Referer",source.toEncoded());auto *posted=m_manager->post(request,body);
        connect(posted,&QNetworkReply::finished,this,[this,posted,context,user,auth]{posted->deleteLater();if(auth!=m_authGeneration)return;CommentSubmission result;result.uncertain=true;result.error="Could not confirm the change. Refresh comments before trying again.";if(!ao3RequestFailed(posted)){const auto html=QString::fromUtf8(posted->readAll());if(siteIdentityMatches(html,user))result=parseCommentSubmission(html,true);}emit commentSubmitted(context,result);});
    };
    auto *reply=m_manager->get(makeRequest(source));connect(reply,&QNetworkReply::finished,this,[this,reply,context,id,label,user,auth,submit]{reply->deleteLater();if(auth!=m_authGeneration)return;
        if(ao3RequestFailed(reply)){emit commentSubmitted(context,{{},ao3NetworkError(reply),false});return;}const auto html=QString::fromUtf8(reply->readAll());if(!siteIdentityMatches(html,user)){emit commentSubmitted(context,{{},"Your AO3 session changed. Check native sign-in.",false});return;}
        const auto controls=commentControls(ArchivePages::Document(html),id);if(!controls.contains(label)){emit commentSubmitted(context,{{},"AO3 no longer permits this action. Refresh comments.",false});return;}const auto control=controls.value(label);
        if(label!="Delete"){submit(control);return;}
        auto *confirmation=m_manager->get(makeRequest(QUrl("https://archiveofourown.org"+control.path)));connect(confirmation,&QNetworkReply::finished,this,[this,confirmation,context,id,user,auth,submit]{confirmation->deleteLater();if(auth!=m_authGeneration)return;if(ao3RequestFailed(confirmation)){emit commentSubmitted(context,{{},ao3NetworkError(confirmation),false});return;}const auto html=QString::fromUtf8(confirmation->readAll());if(!siteIdentityMatches(html,user)){emit commentSubmitted(context,{{},"Your AO3 session changed.",false});return;}submit(commentDeleteControl(html,id));});
    });
}

void AO3Client::fetchCommentForm(const QString &workId,const QString &replyId) {
    if(!QRegularExpression("^(?:[0-9]+|comment:[0-9]+)$").match(workId).hasMatch() || (!replyId.isEmpty() && !QRegularExpression("^\\d+$").match(replyId).hasMatch())) {emit errorOccurred("Invalid comment target");return;}
    const auto generation=m_authGeneration;const auto revision=++m_commentFormGeneration;
    const auto parent=replyId.isEmpty() && workId.startsWith("comment:")?workId.mid(8):replyId;
    const auto path=parent.isEmpty()?QString("/works/%1/comments/new").arg(workId):QString("/comments/%1/comments/new").arg(parent);
    const auto url=QUrl("https://archiveofourown.org"+path);auto *reply=m_manager->get(makeRequest(url));
    connect(reply,&QNetworkReply::finished,this,[this,reply,workId,parent,generation,url,revision] {
        reply->deleteLater();if(generation!=m_authGeneration || revision!=m_commentFormGeneration)return;
        CommentForm form;form.context=parent;
        if(ao3RequestFailed(reply))form.error=ao3NetworkError(reply);
        else {form=parseCommentForm(QString::fromUtf8(reply->readAll()));form.context=parent;form.source=url.toString();}
        emit commentFormFetched(workId,form);
    });
}
void AO3Client::submitComment(const QString &workId,const CommentForm &form,const QString &text,const QString &pseud,const QString &guestName,const QString &guestEmail) {
    if(!validCommentAction(form.action) || form.hidden.value("authenticity_token").isEmpty() || !form.error.isEmpty() || text.trimmed().isEmpty() || (form.guest && (guestName.trimmed().isEmpty() || guestEmail.trimmed().isEmpty()))) {emit commentSubmitted(workId,{{},"Complete the comment form",false});return;}
    auto fields=form.hidden;fields["comment[comment_content]"]=text;
    if(!pseud.isEmpty()) {bool allowed=false;for(const auto &p:form.pseuds)if(p.first==pseud)allowed=true;if(!allowed){emit commentSubmitted(workId,{{},"Choose a pseud from the form",false});return;}fields["comment[pseud_id]"]=pseud;}
    if(form.guest) {fields["comment[name]"]=guestName.trimmed();fields["comment[email]"]=guestEmail.trimmed();}
    QByteArray body;for(auto it=fields.cbegin();it!=fields.cend();++it) {if(!body.isEmpty())body+='&';body+=QUrl::toPercentEncoding(it.key())+'='+QUrl::toPercentEncoding(it.value());}
    auto request=makeRequest(QUrl("https://archiveofourown.org").resolved(QUrl(form.action)));request.setRawHeader("Content-Type","application/x-www-form-urlencoded");request.setRawHeader("Referer",form.source.toUtf8());
    const auto generation=m_authGeneration;auto *reply=m_manager->post(request,body);
    connect(reply,&QNetworkReply::finished,this,[this,reply,workId,generation] {
        reply->deleteLater();if(generation!=m_authGeneration)return;
        if(ao3RequestFailed(reply))emit commentSubmitted(workId,{{},"Could not confirm whether the comment was posted. Refresh comments before trying again.",true});
        else emit commentSubmitted(workId,parseCommentSubmission(QString::fromUtf8(reply->readAll())));
    });
}

void AO3Client::setMarkedForLater(const QString &id,bool later) {
    if(!m_loginManager->isLoggedIn() || !QRegularExpression("^[0-9]+$").match(id).hasMatch() || m_workAccountPending.contains(id))return;
    const auto generation=m_authGeneration;const auto user=m_loginManager->username();
    const QString action=later?"mark_for_later":"mark_as_read";
    const QUrl source("https://archiveofourown.org/works/"+id+"?view_adult=true");
    m_workAccountPending.insert(id);
    auto finish=[this,id,generation](const QString &message) { if(generation!=m_authGeneration)return;m_workAccountPending.remove(id);emit accountActionFinished(id,message); };
    auto request=makeRequest(source);request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::AlwaysNetwork);request.setRawHeader("Cache-Control","no-cache");
    auto *reply=m_manager->get(request);
    connect(reply,&QNetworkReply::finished,this,[this,reply,id,later,action,generation,user,source,finish] {
        reply->deleteLater();if(generation!=m_authGeneration)return;
        const auto html=QString::fromUtf8(reply->readAll());const auto form=parseReadingAction(html,id);
        if(reply->error()!=QNetworkReply::NoError || !siteIdentityMatches(html,user) || form.action.isEmpty()) {finish("Reading history is unavailable. Check your sign-in and AO3 history preferences.");return;}
        const QString success=later?"Marked for later on AO3":"Marked as read on AO3";
        if(form.action!=action) {finish(success);return;}
        QByteArray body;for(auto it=form.fields.cbegin();it!=form.fields.cend();++it) {if(!body.isEmpty())body+='&';body+=QUrl::toPercentEncoding(it.key())+'='+QUrl::toPercentEncoding(it.value());}
        auto post=makeRequest(QUrl("https://archiveofourown.org/works/"+id+"/"+action));post.setRawHeader("Content-Type","application/x-www-form-urlencoded");post.setRawHeader("Referer",source.toEncoded());
        auto *posted=m_manager->post(post,body);
        connect(posted,&QNetworkReply::finished,this,[posted,id,action,user,success,finish] {
            posted->deleteLater();const auto html=QString::fromUtf8(posted->readAll());const auto result=parseReadingAction(html,id);
            finish(posted->error()==QNetworkReply::NoError && siteIdentityMatches(html,user) && !result.action.isEmpty() && result.action!=action?success:QString("Could not confirm the change. Check your Marked for Later list on AO3."));
        });
    });
}

void AO3Client::entitySubscription(const QString &type,const QString &target,int desired)
{
    if(!m_loginManager->isLoggedIn() || !QStringList{"User","Series"}.contains(type) || desired< -1 || desired>1 || !(type=="Series"?QRegularExpression("^[0-9]+$"):QRegularExpression("^[A-Za-z0-9_]+$")).match(target).hasMatch())return;
    const auto user=m_loginManager->username();const auto auth=m_authGeneration;const auto key=type+":"+target+":"+QString::number(auth);if(m_entityPending.contains(key))return;
    const auto revision=++m_entityRevisions[key];if(desired>=0)m_entityPending.insert(key);
    const QUrl source("https://archiveofourown.org"+(type=="Series"?QString("/series/"):QString("/users/"))+target);
    auto finish=[this,type,target,desired,key,revision,auth](bool available,bool subscribed,const QString &message){if(desired>=0)m_entityPending.remove(key);if(auth==m_authGeneration && revision==m_entityRevisions.value(key))emit entitySubscriptionFetched(type,target,available,subscribed,message);};
    auto *reply=m_manager->get(makeRequest(source));connect(reply,&QNetworkReply::finished,this,[this,reply,type,target,desired,user,auth,source,finish]{
        reply->deleteLater();if(auth!=m_authGeneration){finish(false,false,{});return;}
        const auto html=QString::fromUtf8(reply->readAll());const auto form=parseWorkSubscription(html,type=="Series"?target:QString(),user,type);
        if(ao3RequestFailed(reply) || !siteIdentityMatches(html,user) || form.path.isEmpty()){finish(false,false,desired<0?QString():QString("Subscription unavailable. Check native sign-in and this page on AO3."));return;}
        if(desired<0 || form.subscribed==bool(desired)){finish(true,form.subscribed,{});return;}
        QByteArray body;for(auto it=form.fields.cbegin();it!=form.fields.cend();++it){if(!body.isEmpty())body+='&';body+=QUrl::toPercentEncoding(it.key())+'='+QUrl::toPercentEncoding(it.value());}
        auto request=makeRequest(QUrl("https://archiveofourown.org"+form.path));request.setRawHeader("Content-Type","application/x-www-form-urlencoded");request.setRawHeader("Referer",source.toEncoded());auto *posted=m_manager->post(request,body);
        connect(posted,&QNetworkReply::finished,this,[this,posted,type,desired,user,auth,form,finish]{posted->deleteLater();if(auth!=m_authGeneration){finish(false,false,{});return;}const auto html=QString::fromUtf8(posted->readAll());const auto updated=parseWorkSubscription(html,form.fields.value("subscription[subscribable_id]"),user,type);const bool confirmed=!ao3RequestFailed(posted) && siteIdentityMatches(html,user) && !updated.path.isEmpty() && updated.subscribed==bool(desired);finish(confirmed,bool(desired),confirmed?QString("Subscription confirmed by AO3."):QString("Could not confirm the subscription change. Check subscriptions on AO3 before retrying."));});
    });
}

void AO3Client::setWorkSubscribed(const QString &id,bool subscribed) {
    if(!m_loginManager->isLoggedIn() || !QRegularExpression("^[0-9]+$").match(id).hasMatch() || m_workAccountPending.contains(id))return;
    const auto generation=m_authGeneration;const auto user=m_loginManager->username();
    const QUrl source("https://archiveofourown.org/works/"+id+"?view_adult=true");
    m_workAccountPending.insert(id);
    auto finish=[this,id,generation](const QString &message) {if(generation!=m_authGeneration)return;m_workAccountPending.remove(id);emit accountActionFinished(id,message);};
    auto request=makeRequest(source);request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::AlwaysNetwork);request.setRawHeader("Cache-Control","no-cache");
    auto *reply=m_manager->get(request);
    connect(reply,&QNetworkReply::finished,this,[this,reply,id,subscribed,generation,user,source,finish] {
        reply->deleteLater();if(generation!=m_authGeneration)return;
        const auto html=QString::fromUtf8(reply->readAll());const auto form=parseWorkSubscription(html,id,user);
        if(reply->error()!=QNetworkReply::NoError || !siteIdentityMatches(html,user) || form.path.isEmpty()) {finish("Subscription unavailable. Check your AO3 sign-in and access to this work.");return;}
        const QString success=subscribed?"Subscribed to this work on AO3":"Unsubscribed from this work on AO3";
        if(form.subscribed==subscribed) {finish(success);return;}
        QByteArray body;for(auto it=form.fields.cbegin();it!=form.fields.cend();++it) {if(!body.isEmpty())body+='&';body+=QUrl::toPercentEncoding(it.key())+'='+QUrl::toPercentEncoding(it.value());}
        auto post=makeRequest(QUrl("https://archiveofourown.org"+form.path));post.setRawHeader("Content-Type","application/x-www-form-urlencoded");post.setRawHeader("Referer",source.toEncoded());
        auto *posted=m_manager->post(post,body);
        connect(posted,&QNetworkReply::finished,this,[posted,id,subscribed,user,success,finish] {
            posted->deleteLater();const auto html=QString::fromUtf8(posted->readAll());const auto result=parseWorkSubscription(html,id,user);
            finish(posted->error()==QNetworkReply::NoError && siteIdentityMatches(html,user) && !result.path.isEmpty() && result.subscribed==subscribed?success:QString("Could not confirm the change. Check your subscriptions on AO3."));
        });
    });
}

void AO3Client::fetchArchiveBookmark(const QString &id) {
    if(!m_loginManager->isLoggedIn() || !QRegularExpression("^[0-9]+$").match(id).hasMatch())return;
    const auto auth=m_authGeneration,revision=++m_bookmarkGeneration;const auto user=m_loginManager->username();
    auto request=makeRequest(QUrl("https://archiveofourown.org/works/"+id+"?view_adult=true"));request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::AlwaysNetwork);request.setRawHeader("Cache-Control","no-cache");
    auto *reply=m_manager->get(request);connect(reply,&QNetworkReply::finished,this,[this,reply,id,auth,revision,user] {
        reply->deleteLater();if(auth!=m_authGeneration || revision!=m_bookmarkGeneration)return;
        const auto html=QString::fromUtf8(reply->readAll());auto form=parseArchiveBookmark(html,id);
        if(reply->error()!=QNetworkReply::NoError || !siteIdentityMatches(html,user))form.error="Could not load your AO3 bookmark. Check your sign-in and connection.";
        emit archiveBookmarkFetched(form);
    });
}
void AO3Client::saveArchiveBookmark(const ArchiveBookmarkForm &draft,bool deleting) {
    const auto id=draft.workId;
    if(!m_loginManager->isLoggedIn() || !QRegularExpression("^[0-9]+$").match(id).hasMatch() || m_workAccountPending.contains(id))return;
    const auto auth=m_authGeneration;const auto user=m_loginManager->username();
    m_workAccountPending.insert(id);
    const QUrl source("https://archiveofourown.org/works/"+id+"?view_adult=true");
    auto finish=[this,id,auth](const ArchiveFormResult &result) {if(auth!=m_authGeneration)return;m_workAccountPending.remove(id);emit archiveBookmarkSaved(id,result);};
    auto send=[this,id,auth,user,source,finish,deleting,draft](const QString &path,const QMap<QString,QString> &fields) {
        QByteArray body;for(auto it=fields.cbegin();it!=fields.cend();++it){if(!body.isEmpty())body+='&';body+=QUrl::toPercentEncoding(it.key())+'='+QUrl::toPercentEncoding(it.value());}
        auto req=makeRequest(QUrl("https://archiveofourown.org"+path));req.setRawHeader("Content-Type","application/x-www-form-urlencoded");req.setRawHeader("Referer",source.toEncoded());
        auto *reply=m_manager->post(req,body);connect(reply,&QNetworkReply::finished,this,[this,reply,id,auth,user,source,finish,deleting,draft] {
            reply->deleteLater();if(auth!=m_authGeneration)return;
            const auto html=QString::fromUtf8(reply->readAll());
            ArchiveFormResult unknown{"Could not confirm the change. Check your AO3 bookmarks before trying again.",false,true};
            auto result=reply->error()==QNetworkReply::NoError && siteIdentityMatches(html,user)?archiveFormResult(html):unknown;
            if(!result.success){finish(result);return;}
            if(!deleting){finish(archiveBookmarkRecord(html,id,draft.id)?result:unknown);return;}
            auto req=makeRequest(source);req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::AlwaysNetwork);req.setRawHeader("Cache-Control","no-cache");auto *check=m_manager->get(req);
            connect(check,&QNetworkReply::finished,this,[check,id,user,result,unknown,finish]{check->deleteLater();const auto html=QString::fromUtf8(check->readAll());const auto form=parseArchiveBookmark(html,id);finish(check->error()==QNetworkReply::NoError && siteIdentityMatches(html,user) && form.error.isEmpty() && form.id.isEmpty()?result:unknown);});
        });
    };
    auto req=makeRequest(source);req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::AlwaysNetwork);req.setRawHeader("Cache-Control","no-cache");auto *reply=m_manager->get(req);
    connect(reply,&QNetworkReply::finished,this,[this,reply,id,draft,deleting,auth,user,finish,send] {
        reply->deleteLater();if(auth!=m_authGeneration)return;
        const auto html=QString::fromUtf8(reply->readAll());const auto fresh=parseArchiveBookmark(html,id);
        if(reply->error()!=QNetworkReply::NoError || !siteIdentityMatches(html,user) || !fresh.error.isEmpty() || fresh.id!=draft.id) {finish({"Your bookmark or session changed. Reload the bookmark before saving.",false,false});return;}
        if(!deleting) {
            bool valid=false;for(const auto &p:fresh.pseuds)if(p.first==draft.pseud)valid=true;
            if(!valid){finish({"This pseud is unavailable. Reload the bookmark.",false,false});return;}
            send(fresh.path,archiveBookmarkFields(fresh,draft));return;
        }
        if(fresh.id.isEmpty()){finish({"No AO3 bookmark to delete.",false,false});return;}
        auto req=makeRequest(QUrl("https://archiveofourown.org/bookmarks/"+fresh.id+"/confirm_delete"));req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::AlwaysNetwork);
        auto *confirmation=m_manager->get(req);connect(confirmation,&QNetworkReply::finished,this,[this,confirmation,fresh,auth,user,finish,send] {
            confirmation->deleteLater();if(auth!=m_authGeneration)return;
            const auto html=QString::fromUtf8(confirmation->readAll());const auto fields=parseBookmarkDelete(html,fresh.id);
            if(confirmation->error()!=QNetworkReply::NoError || !siteIdentityMatches(html,user) || fields.isEmpty()){finish({"Could not load AO3's delete confirmation.",false,false});return;}
            send("/bookmarks/"+fresh.id,fields);
        });
    });
}

void AO3Client::useWebsiteSession(const QList<QNetworkCookie> &cookies,const QString &agent,const QString &expectedUser) {
    const auto accepted=websiteSessionCookies(cookies);
    if(!validWebsiteSessionIdentity(expectedUser,agent) || accepted.isEmpty()){emit errorOccurred("Sign in on the AO3 website before importing its session.");return;}
    const auto revision=++m_sessionImportGeneration,auth=m_authGeneration;
    for(auto *reply:m_sessionManager->findChildren<QNetworkReply *>())if(!reply->isFinished())reply->abort();
    const QUrl source("https://archiveofourown.org/");auto *jar=new QNetworkCookieJar;jar->setCookiesFromUrl(accepted,source);
    m_sessionManager->setCookieJar(jar);
    auto request=makeRequest(source);request.setRawHeader("User-Agent",agent.toUtf8());request.setRawHeader("Cache-Control","no-cache");request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::AlwaysNetwork);
    auto *reply=m_sessionManager->get(request);connect(reply,&QNetworkReply::finished,this,[this,reply,jar,source,expectedUser,agent,revision,auth]{
        reply->deleteLater();if(revision!=m_sessionImportGeneration || auth!=m_authGeneration)return;
        if(reply->error()!=QNetworkReply::NoError || !siteIdentityMatches(QString::fromUtf8(reply->readAll()),expectedUser)){emit errorOccurred("AO3 could not verify this website session for the native reader. Your existing reader sign-in was kept.");return;}
        const auto verified=jar->cookiesForUrl(source);
        logout();m_userAgent=agent;QSettings().setValue("network/userAgent",agent);m_loginManager->setCookiesFromUrl(verified,source);
        UserInfo user{expectedUser,"https://archiveofourown.org/users/"+expectedUser,true};m_loginManager->setUserInfo(user);emit loginSuccessful(user);
    });
}

void AO3Client::fetchProfileAvatar(const QString &username,const QUrl &url) {
    const auto revision=++m_avatarRevision;
    if(!QRegularExpression("^[A-Za-z0-9_]+$").match(username).hasMatch() || !avatarUrlAllowed(url)){emit profileAvatarFetched(username,{});return;}
    loadProfileAvatar(username,url,revision,m_authGeneration,0);
}
void AO3Client::loadProfileAvatar(const QString &username,const QUrl &url,quint64 revision,quint64 auth,int redirects) {
    auto request=makeRequest(url);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setRawHeader("Accept","image/png,image/jpeg,image/gif");if(url.host()!="archiveofourown.org"){request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,QNetworkRequest::Manual);request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,QNetworkRequest::Manual);}auto *reply=m_manager->get(request);reply->setReadBufferSize(2*1024*1024+1);auto data=std::make_shared<QByteArray>();
    connect(reply,&QNetworkReply::readyRead,this,[reply,data]{data->append(reply->readAll());if(data->size()>2*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,data,username,url,revision,auth,redirects]{
        reply->deleteLater();if(revision!=m_avatarRevision || auth!=m_authGeneration)return;
        data->append(reply->readAll());const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(status>=300 && status<400 && reply->error()==QNetworkReply::NoError){const auto next=url.resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());if(redirects<3 && avatarUrlAllowed(next,true)){loadProfileAvatar(username,next,revision,auth,redirects+1);return;}}
        const auto image=reply->error()==QNetworkReply::NoError && status==200?decodeProfileAvatar(*data):QImage();emit profileAvatarFetched(username,image);
    });
}
