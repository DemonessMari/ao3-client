#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "workmodel.h"
#include "searchfilters.h"
#include "loginmanager.h"

class AO3Client : public QObject {
    Q_OBJECT
    friend class FunctionalTest;
public:
    explicit AO3Client(LoginManager *loginManager, QObject *parent = nullptr);

    void search(const QString &query, int page = 1, const SearchFilters &filters = {});
    void fetchSearchOptions();
    void fetchTagSuggestions(const QString &field, const QString &type, const QString &term, quint64 revision);
    void fetchWorkDetails(const QString &workId);
    void fetchWorkContent(const QString &workId);
    void downloadWork(const QString &workId);
    void login(const QString &login, const QString &password);
    void checkSession();
    void logout();
    void fetchDashboard();
    void fetchUserWorks(int page = 1);
    void fetchAuthorProfile(const QString &username);
    void fetchAuthorWorks(const QString &username, int page = 1, bool bookmarks = false);

    LoginManager *loginManager() const;

signals:
    void tagSuggestionsFetched(const QString &field, quint64 revision, const QStringList &suggestions, const QString &error);
    void searchOptionsFetched(const SearchOptions &options);
    void searchCompleted(const SearchResult &result);
    void workDetailsFetched(const Work &work);
    void workChaptersFetched(const QString &workId, const QString &title, const QVector<WorkChapter> &chapters);
    void workContentFetched(const QString &workId, const QString &title, const QString &contentHtml);
    void workDownloaded(const QString &workId, const QString &fullHtml);
    void errorOccurred(const QString &error);
    void loginSuccessful(const UserInfo &user);
    void loginFailed(const QString &reason);
    void sessionChecked(bool valid, const UserInfo &user);
    void loggedOut();
    void userWorksFetched(const SearchResult &result);
    void authorProfileFetched(const AuthorProfile &profile);
    void authorWorksFetched(const QString &username, bool bookmarks, const SearchResult &result);

private:
    void verifySession(bool loginAttempt, quint64 generation);
    QNetworkRequest makeRequest(const QUrl &url) const;
    SearchResult parseSearchResults(const QString &html, const QString &query, int page);
    Work parseWorkDetails(const QString &html, const QString &workId);
    QString extractWorkContent(const QString &html);
    QString extractCsrfToken(const QString &html);

    QNetworkAccessManager *m_manager;
    LoginManager *m_loginManager;
    QString m_userAgent;
    quint64 m_profileGeneration = 0;
    quint64 m_authGeneration = 0;
    quint64 m_listingGeneration = 0;
};
