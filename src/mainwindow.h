#pragma once

#include <QMainWindow>
#include <QDockWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QSplitter>
#include <QLabel>
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
    bool navigateBack();
    void showLibrary();
    void showProfile(const QString &authorUrl);
    void showAuthorListing(bool bookmarks);
    QWidget *m_profilePage = nullptr;
    QTextBrowser *m_profileText = nullptr;
    QString m_profileUsername;
    QString m_listingAuthor;
    bool m_listingBookmarks = false;
    void refreshLibrary();
    void openLibrarySelection();
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
