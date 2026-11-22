#pragma once

#include <QMainWindow>
#include <QTextEdit>
#include <QDockWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QSplitter>
#include <QLabel>
#include <QCheckBox>
#include <QStatusBar>
#include <QStackedWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "ao3client.h"
#include "workdetailwidget.h"
#include "logindialog.h"
#include "dashboardwidget.h"
#include "workmodel.h"
#include "readerwidget.h"

class ArchiveWeb;
class MainWindow : public QMainWindow {
    Q_OBJECT
    friend class FunctionalTest;
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void performSearch();
    void onSearchCompleted(const SearchResult &result);
    void onWorkSelected(int index);
    void onError(const QString &error);
    void onLoginClicked();
    void onLoginRequested(const QString &login, const QString &password);
    void onLoginSuccessful(const UserInfo &user);
    void onLoginFailed(const QString &reason);
    void onSessionChecked(bool valid, const UserInfo &user);
    void onLogout();
    void goToPage(int page);
    void showSearchView(int page = 1);
    void showDashboard();
    void showUserWorks();
    void showLoginView();

private:
    void setupUI();
    void openArchiveWebsite(const QUrl &url);
    ArchiveWeb *m_archiveWebsite=nullptr;
    int m_archiveWebsiteReturn=0;
    void editArchiveBookmark(const QString &workId,bool reload=false);
    void submitArchiveBookmark(bool deleting=false);
    void clearArchiveBookmark();
    QWidget *m_archiveBookmarkPage=nullptr,*m_archiveBookmarkFields=nullptr;
    QTextEdit *m_archiveBookmarkNotes=nullptr;
    QLineEdit *m_archiveBookmarkTags=nullptr,*m_archiveBookmarkCollections=nullptr;
    QComboBox *m_archiveBookmarkPseud=nullptr;
    QCheckBox *m_archiveBookmarkPrivate=nullptr,*m_archiveBookmarkRec=nullptr;
    QPushButton *m_archiveBookmarkSave=nullptr,*m_archiveBookmarkDelete=nullptr,*m_archiveBookmarkReload=nullptr;
    QLabel *m_archiveBookmarkStatus=nullptr;
    ArchiveBookmarkForm m_archiveBookmark;
    QString m_archiveBookmarkWork;
    int m_archiveBookmarkReturn=0;
    bool m_archiveBookmarkSending=false,m_archiveBookmarkUncertain=false;
    void showComments(const QString &workId,int page=1);
    void composeComment(const QString &replyId={});
    QWidget *m_commentEditor=nullptr;
    QTextEdit *m_commentDraft=nullptr;
    QLineEdit *m_commentName=nullptr,*m_commentEmail=nullptr;
    QComboBox *m_commentPseud=nullptr;
    QPushButton *m_commentPost=nullptr,*m_commentClose=nullptr,*m_commentNew=nullptr;
    QLabel *m_commentNotice=nullptr;
    CommentForm m_commentForm;
    QString m_commentContext;
    bool m_commentSending=false,m_commentUncertain=false;
    QWidget *m_commentsPage=nullptr;
    QTextBrowser *m_commentsText=nullptr;
    QPushButton *m_commentsPrevious=nullptr,*m_commentsNext=nullptr;
    QLabel *m_commentsNumber=nullptr;
    QString m_commentsWork;
    int m_commentsCurrent=1,m_commentsReturn=0;

    bool navigateBack();
    void showLibrary();
    void showProfile(const QString &authorUrl);
    void showAuthorListing(bool bookmarks);
    QWidget *m_profilePage = nullptr;
    QTextBrowser *m_profileText = nullptr;
    QLabel *m_profileAvatar = nullptr;
    QString m_profileUsername;
    QString m_listingAuthor;
    bool m_listingBookmarks = false;
    void refreshLibrary();
    void openLibrarySelection();
    void openLibraryEntry(const QJsonObject &entry);
    QWidget *m_libraryPage = nullptr;
    QListWidget *m_libraryList = nullptr;
    QComboBox *m_libraryFilter = nullptr;
    QLineEdit *m_librarySearch = nullptr;
    QWidget *m_mobileFilters = nullptr;
    QWidget *m_mobileDetails = nullptr;
    int m_beforeFilters = 0;
    int m_beforeReader = 2;
    QLabel *m_mobileStatus = nullptr;
    QString m_pendingReadId;
    void openReader(const Work &work, const QVector<WorkChapter> &chapters);
    void closeReader();
    void toggleReaderFullscreen();
    void populateWorkList();
    void resizeWorkItems();
    bool m_workResizeScheduled = false;
    void setLoggedInState(bool loggedIn);

    LoginManager *m_loginManager;
    AO3Client *m_client;

    // Header
    QWidget *m_headerBar;
    QLabel *m_logoLabel;
    QLineEdit *m_searchInput;
    QPushButton *m_searchButton;
    QPushButton *m_filterButton;
    SearchFilterPanel *m_filterPanel;
    QDockWidget *m_filterDock;
    SearchFilters m_currentFilters;
    bool m_filterChoicesLoaded = false;
    bool m_defaultLanguageApplied = false;
    QPushButton *m_loginButton;
    QLabel *m_userGreeting;

    // Nav
    QWidget *m_navBar;
    QPushButton *m_navHome;
    QPushButton *m_navSearch;
    QPushButton *m_navWorks;
    QPushButton *m_navLogout;

    // Content
    QStackedWidget *m_contentStack;
    ReaderWidget *m_reader;
    bool m_readerFiltersVisible = false;
    bool m_readerWasFullscreen = false;

    // Views
    DashboardWidget *m_dashboardView;
    LoginDialog *m_loginView;

    // Search view (splitter)
    QSplitter *m_searchSplitter;
    QListWidget *m_workList;
    WorkDetailWidget *m_detailWidget;

    // Pagination
    QWidget *m_paginationWidget;
    QPushButton *m_prevButton;
    QPushButton *m_nextButton;
    QLabel *m_pageLabel;

    SearchResult m_currentResult;
    int m_currentPage;
    QString m_currentQuery;
    bool m_showingUserWorks = false;
};
