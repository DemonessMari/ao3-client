#pragma once

#include <QObject>
#include <QSet>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "workmodel.h"
#include "commentpage.h"
#include "searchfilters.h"
#include "loginmanager.h"
#include "officialdownload.h"
#include "kudos.h"
#include "readingaction.h"
#include "subscription.h"
#include "archivebookmark.h"
#include "websession.h"
#include <QImage>
#include "seriesnavigation.h"
#include "archivepages.h"
#include "commentmanagement.h"

class AO3Client : public QObject {
    Q_OBJECT
    friend class FunctionalTest;
public:
    explicit AO3Client(LoginManager *loginManager, QObject *parent = nullptr);

    void search(const QString &query, int page = 1, const SearchFilters &filters = {});
    void fetchSearchOptions();
    void cancelListing(){++m_listingGeneration;}
    void searchDiscovery(const QString &kind,const QMap<QString,QString> &values,int page=1);
    void fetchTagSuggestions(const QString &field, const QString &type, const QString &term, quint64 revision);
    void fetchWorkSeries(const QString &workId);
    void fetchWorkDetails(const QString &workId);
    void fetchWorkContent(const QString &workId);
    void downloadWork(const QString &workId);
    bool workAccountPending(const QString &id) const {return m_workAccountPending.contains(id);}
    void fetchArchiveBookmark(const QString &workId);
    void saveArchiveBookmark(const ArchiveBookmarkForm &draft,bool deleting=false);
    void entitySubscription(const QString &type,const QString &target,int desired=-1);
    void setWorkSubscribed(const QString &workId,bool subscribed);
    void setMarkedForLater(const QString &workId,bool later);
    void giveKudos(const QString &workId);
    QString kudosStatus(const QString &workId) const { return m_kudosStatus.value(workId); }
    void fetchOfficialDownloads(const QString &workId);
    void saveOfficialDownload(const QString &workId, const QUrl &url, const QString &path);
    void useWebsiteSession(const QList<QNetworkCookie> &cookies,const QString &agent,const QString &expectedUser);
    void login(const QString &login, const QString &password);
    void checkSession();
    void logout();
    void fetchDashboard();
    void fetchCommentEdit(const QString &context,const QString &id);
    void manageComment(const QString &context,const QString &id,const QString &label);
    void fetchCommentForm(const QString &workId,const QString &replyId={});
    void submitComment(const QString &workId,const CommentForm &form,const QString &text,const QString &pseud={},const QString &guestName={},const QString &guestEmail={});
    void fetchComments(const QString &workId,int page=1);
    void fetchUserWorks(int page = 1);
    void fetchAccountReadings(bool later,int page=1);
    void fetchArchivePage(const QString &kind,const QString &id={},int page=1);
    void performArchiveAction(const QString &kind,const QString &id,int page,const QString &entry,const QString &label,const QStringList &selection={});
    void fetchProfileAvatar(const QString &username,const QUrl &url);
    void fetchAuthorProfile(const QString &username);
    void fetchAuthorWorks(const QString &username, int page = 1, bool bookmarks = false);

    LoginManager *loginManager() const;

signals:
    void archiveBookmarkFetched(const ArchiveBookmarkForm &form);
    void archiveBookmarkSaved(const QString &workId,const ArchiveFormResult &result);
    void accountActionFinished(const QString &workId,const QString &message);
    void kudosChanged(const QString &workId,const QString &status,const QString &message);
    void officialDownloadsFetched(const QString &workId, const QVector<OfficialDownload> &downloads);
    void officialDownloadSaved(const QString &path);
    void tagSuggestionsFetched(const QString &field, quint64 revision, const QStringList &suggestions, const QString &error);
    void searchOptionsFetched(const SearchOptions &options);
    void searchCompleted(const SearchResult &result);
    void workSeriesFetched(const QString &workId,const WorkSeries &series);
    void workDetailsFetched(const Work &work);
    void workChaptersFetched(const QString &workId, const QString &title, const QVector<WorkChapter> &chapters);
    void workContentFetched(const QString &workId, const QString &title, const QString &contentHtml);
    void workDownloaded(const QString &workId, const QString &fullHtml);
    void commentFormFetched(const QString &workId,const CommentForm &form);
    void commentSubmitted(const QString &workId,const CommentSubmission &result);
    void commentsFetched(const QString &workId,const CommentPage &page);
    void errorOccurred(const QString &error);
    void loginSuccessful(const UserInfo &user);
    void loginFailed(const QString &reason);
    void sessionChecked(bool valid, const UserInfo &user);
    void loggedOut();
    void entitySubscriptionFetched(const QString &type,const QString &target,bool available,bool subscribed,const QString &message);
    void archivePageFetched(const QString &kind,const QString &id,const ArchivePage &page);
    void archiveActionFinished(const QString &message,bool uncertain);
    void accountReadingsFetched(bool later,const SearchResult &result);
    void userWorksFetched(const SearchResult &result);
    void profileAvatarFetched(const QString &username,const QImage &image);
    void authorProfileFetched(const AuthorProfile &profile);
    void authorWorksFetched(const QString &username, bool bookmarks, const SearchResult &result);

private:
    void loadProfileAvatar(const QString &username,const QUrl &url,quint64 revision,quint64 auth,int redirects);
    quint64 m_avatarRevision=0;
    void verifySession(bool loginAttempt, quint64 generation);
    QNetworkRequest makeRequest(const QUrl &url) const;
    SearchResult parseSearchResults(const QString &html, const QString &query, int page);
    Work parseWorkDetails(const QString &html, const QString &workId);
    QString extractWorkContent(const QString &html);
    QString extractCsrfToken(const QString &html);

    QNetworkAccessManager *m_manager;
    QNetworkAccessManager *m_sessionManager;
    quint64 m_sessionImportGeneration=0;
    LoginManager *m_loginManager;
    QString m_userAgent;
    quint64 m_bookmarkGeneration=0;
    quint64 m_commentsGeneration = 0;
    quint64 m_commentFormGeneration = 0;
    quint64 m_profileGeneration = 0;
    quint64 m_authGeneration = 0;
    quint64 m_listingGeneration = 0;
    QMap<QString,QString> m_kudosStatus;
    QSet<QString> m_workAccountPending;
    QSet<QString> m_listMutations;
    QSet<QString> m_entityPending;
    QHash<QString,quint64> m_entityRevisions;
};
