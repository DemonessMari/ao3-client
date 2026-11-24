#include <QUuid>
#include "taglineedit.h"
#include "discoverysearch.h"
#include "mainwindow.h"
#include "accountbutton.h"
#ifdef AO3_WEBENGINE
#include "archiveweb.h"
#endif
#include "readinglibrary.h"
#include "librarybackup.h"
#include <QSaveFile>
#include <QInputDialog>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDesktopServices>
#include <QComboBox>
#include "mobileui.h"
#include "mobilestack.h"
#include "readabletheme.h"
#include "appskin.h"
#include <QGuiApplication>
#include <QInputMethod>
#include <QMenu>
#include <QGridLayout>
#include <QKeyEvent>
#include <QScroller>
#include "workcontent.h"
#include "workitemwidget.h"
#include <QApplication>
#include <QShortcut>
#include <QKeySequence>
#include <QFrame>
#include <QIcon>
#include <QTimer>
#include <QLocale>
#include <QPixmap>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QStyle>
#include <QSettings>
#include <QFormLayout>
#include <QEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_loginManager(new LoginManager(this))
    , m_client(new AO3Client(m_loginManager, this))
    , m_currentPage(1)
{
    setupUI();
    m_defaultLanguageApplied = true;
    if (QSettings().value(QStringLiteral("reader/filterLanguage"), false).toBool()) {
        auto filters = m_filterPanel->filters();
        filters[QStringLiteral("language_id")] = {QSettings().value(QStringLiteral("reader/language"), QStringLiteral("en")).toString()};
        m_filterPanel->setFilters(filters);
    }


    connect(m_client, &AO3Client::searchOptionsFetched, this, [this](const SearchOptions &options) {
        m_filterChoicesLoaded = options.error.isEmpty();
        m_filterPanel->setOptions(options);
        if (options.error.isEmpty() && !m_defaultLanguageApplied) {
            m_defaultLanguageApplied = true;
            if (QSettings().value(QStringLiteral("reader/filterLanguage"), false).toBool()) {
                auto filters = m_filterPanel->filters();
                filters[QStringLiteral("language_id")] = {QSettings().value(QStringLiteral("reader/language"), QStringLiteral("en")).toString()};
                m_filterPanel->setFilters(filters);
            }
        }
    });
    connect(m_client, &AO3Client::tagSuggestionsFetched, m_filterPanel, &SearchFilterPanel::showTagSuggestions);
    connect(m_filterPanel, &SearchFilterPanel::tagSuggestionsRequested, m_client, &AO3Client::fetchTagSuggestions);
    connect(m_client, &AO3Client::searchCompleted, this, &MainWindow::onSearchCompleted);
    connect(m_client, &AO3Client::userWorksFetched, this, [this](const SearchResult &result) {
        if (m_showingUserWorks) onSearchCompleted(result);
    });
    connect(m_client, &AO3Client::errorOccurred, this, &MainWindow::onError);
    connect(m_loginView,&LoginDialog::websiteSignInRequested,this,[this]{openArchiveWebsite(QUrl("https://archiveofourown.org/users/login"));});
    connect(m_client, &AO3Client::loginSuccessful, this, &MainWindow::onLoginSuccessful);
    connect(m_client, &AO3Client::loginFailed, this, &MainWindow::onLoginFailed);
    connect(m_client, &AO3Client::sessionChecked, this, &MainWindow::onSessionChecked);
    connect(m_client, &AO3Client::workContentFetched, this, [this](const QString &workId, const QString &, const QString &text) {
        if (m_detailWidget->currentWork().id != workId) return;
        if (text.isEmpty()) {
            m_detailWidget->showContent(QStringLiteral("(could not extract work content)"));
            statusBar()->showMessage(QStringLiteral("Failed to extract content"));
        } else {
            m_detailWidget->showContent(text);
            statusBar()->showMessage(QStringLiteral("Loaded %1 characters").arg(text.size()));
        }
    });

    connect(m_client, &AO3Client::workChaptersFetched, this, [this](const QString &id, const QString &title, const QVector<WorkChapter> &chapters) {
        if (m_detailWidget->currentWork().id != id || chapters.isEmpty() || m_pendingReadId != id) return;
        m_pendingReadId.clear();
        Work work = m_detailWidget->currentWork();
        if (!title.isEmpty()) work.title = title;
        openReader(work, chapters);
    });
    connect(m_detailWidget,&WorkDetailWidget::seriesRequested,this,&MainWindow::showSeries);
    connect(m_reader,&ReaderWidget::seriesRequested,this,&MainWindow::showSeries);
    connect(m_client,&AO3Client::workSeriesFetched,this,[this](const QString &id,const WorkSeries &result) {
        if(id!=m_seriesWork || m_contentStack->currentWidget()!=m_seriesPage)return;
        while(auto *item=m_seriesLayout->takeAt(1)){if(item->widget())item->widget()->deleteLater();delete item;}
        auto *message=new QLabel(result.error.isEmpty()?(result.entries.isEmpty()?"This work is not part of a series.":"Series"):result.error,m_seriesPage);
        message->setWordWrap(true);message->setTextFormat(Qt::PlainText);m_seriesLayout->addWidget(message);
        for(const auto &entry:result.entries) {
            auto *label=new QLabel(entry.position,m_seriesPage);label->setWordWrap(true);label->setTextFormat(Qt::PlainText);m_seriesLayout->addWidget(label);
            auto *site=new QPushButton("View series",m_seriesPage);m_seriesLayout->addWidget(site);
            connect(site,&QPushButton::clicked,this,[this,entry]{showArchiveList("series",entry.id);});
            for(const auto &choice:QVector<QPair<QString,QString>>{{"Previous work",entry.previous},{"Next work",entry.next}}) {
                auto *button=new QPushButton(choice.first,m_seriesPage);button->setEnabled(!choice.second.isEmpty());m_seriesLayout->addWidget(button);
                connect(button,&QPushButton::clicked,this,[this,choice]{m_seriesTarget=choice.second;statusBar()->showMessage("Loading work details...");m_client->fetchWorkDetails(choice.second);});
            }
        }
        m_seriesLayout->addStretch();
    });
    connect(m_client,&AO3Client::workDetailsFetched,this,[this](const Work &work) {
        if(work.id!=m_seriesTarget || m_contentStack->currentWidget()!=m_seriesPage)return;
        m_seriesTarget.clear();m_pendingReadId.clear();
        m_detailWidget->showWork(work);m_contentStack->setCurrentIndex(2);statusBar()->clearMessage();
    });
    connect(m_detailWidget, &WorkDetailWidget::readRequested, this, [this](const QString &workId) {
        m_pendingReadId = workId;
        QGuiApplication::inputMethod()->hide();
        statusBar()->showMessage(QStringLiteral("Loading work content..."));
        m_client->fetchWorkContent(workId);
    });

    connect(m_client, &AO3Client::workDownloaded, this, [this](const QString &workId, const QString &html) {
        Q_UNUSED(workId)

        QRegularExpression trx(QStringLiteral(R"regex(<h2\s+class="title[^"]*"[^>]*>(.*?)</h2>)regex"),
                               QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch tm = trx.match(html);
        QString title = tm.hasMatch() ? tm.captured(1).trimmed() : QString();
        title.replace(QRegularExpression(QStringLiteral("<[^>]*>")), QString());

        QString author;
        QRegularExpression arx(QStringLiteral(R"regex(<a\s+rel="author"[^>]*href="[^"]*"[^>]*>(.*?)</a>)regex"),
                               QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch am = arx.match(html);
        if (am.hasMatch()) author = am.captured(1).trimmed();

        QString cleanTitle = QString(title).replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral("_"));
        QString cleanAuthor = QString(author).replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral("_"));

        const QString plainText = extractWorkText(html);
        if (plainText.isEmpty()) {
            statusBar()->showMessage(QStringLiteral("Could not extract work content; nothing was saved"));
            return;
        }

        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Work"),
                                                     QStringLiteral("%1 - %2.txt").arg(cleanTitle, cleanAuthor),
                                                     QStringLiteral("Text Files (*.txt)"));
        if (path.isEmpty()) return;

        QString fullText = QStringLiteral("%1\nby %2\n\n%3\n\n%4")
                               .arg(title, author, QString(50, QChar('=')),
                                    plainText.isEmpty() ? QStringLiteral("(no content could be extracted)") : plainText);

        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out.setEncoding(QStringConverter::Utf8);
            out << fullText;
            file.close();
            statusBar()->showMessage(QStringLiteral("Saved to %1").arg(path));
        } else {
            statusBar()->showMessage(QStringLiteral("Could not write file"));
        }
    });

    connect(m_detailWidget,&WorkDetailWidget::archiveRequested,this,&MainWindow::openArchiveWebsite);
    connect(m_reader,&ReaderWidget::archiveRequested,this,&MainWindow::openArchiveWebsite);
    connect(m_detailWidget, &WorkDetailWidget::profileRequested, this, &MainWindow::showProfile);
    auto accountAction=[this](const QString &id) {
        if(!m_loginManager->isLoggedIn()) {statusBar()->showMessage("Sign in to use AO3 account actions");return;}
        bool ok=false;const QStringList choices{"AO3 bookmark","Mark for later","Mark as read","Subscribe to work","Unsubscribe from work"};
        const auto choice=QInputDialog::getItem(this,"AO3 account","Action",choices,0,false,&ok);
        if(!ok)return;
        if(choice==choices[0])editArchiveBookmark(id);
        else if(choices.indexOf(choice)<3)m_client->setMarkedForLater(id,choice==choices[1]);
        else m_client->setWorkSubscribed(id,choice==choices[3]);
    };
    connect(m_detailWidget,&WorkDetailWidget::readingActionRequested,this,accountAction);
    connect(m_reader,&ReaderWidget::readingActionRequested,this,accountAction);
    connect(m_client,&AO3Client::accountActionFinished,this,[this](const QString &,const QString &message) {statusBar()->showMessage(message);});
    connect(m_detailWidget,&WorkDetailWidget::kudosRequested,m_client,&AO3Client::giveKudos);
    connect(m_detailWidget,&WorkDetailWidget::workShown,this,[this](const QString &id) { m_detailWidget->setKudosStatus(m_client->kudosStatus(id),!m_loginManager->isLoggedIn()); });
    connect(m_client,&AO3Client::kudosChanged,this,[this](const QString &id,const QString &status,const QString &message) {
        if(m_detailWidget->currentWork().id==id) m_detailWidget->setKudosStatus(status,!m_loginManager->isLoggedIn());
        if(m_reader->workId()==id) m_reader->setKudosStatus(status,!m_loginManager->isLoggedIn());
        if(!message.isEmpty())statusBar()->showMessage(message);
    });
    connect(m_client,&AO3Client::authorProfileFetched,this,[this](const AuthorProfile &profile){
        if(m_loginManager->isLoggedIn() && profile.username==m_loginManager->username() && profile.username!=m_profileUsername && profile.error.isEmpty() && !profile.avatarUrl.isEmpty())m_client->fetchProfileAvatar(profile.username,QUrl(profile.avatarUrl));
    });
    connect(m_client,&AO3Client::profileAvatarFetched,this,[this](const QString &username,const QImage &image){
        if(m_loginManager->isLoggedIn() && username==m_loginManager->username() && !image.isNull()){m_loginButton->setText({});m_loginButton->setProperty("accountImage",image);m_loginButton->update();}
    });
    connect(m_client, &AO3Client::authorProfileFetched, this, [this](const AuthorProfile &profile) {
        if(profile.username != m_profileUsername) return;
        m_profileAvatar->setAccessibleName(profile.avatarAlt.isEmpty()?"Profile picture for "+profile.username:profile.avatarAlt);
        if(profile.error.isEmpty() && !profile.avatarUrl.isEmpty())m_client->fetchProfileAvatar(profile.username,QUrl(profile.avatarUrl));
        m_profileText->setPlainText(profile.error.isEmpty() ? profile.username + "\n\n" + profile.title + "\n\n" + profile.details + "\n\n" + (profile.biography.isEmpty() ? "No public biography provided." : profile.biography) : profile.error);
    });
    connect(m_client,&AO3Client::profileAvatarFetched,this,[this](const QString &username,const QImage &image){if(username==m_profileUsername && !image.isNull()){m_profileAvatar->setText({});m_profileAvatar->setPixmap(QPixmap::fromImage(image).scaled(72,72,Qt::KeepAspectRatio,Qt::SmoothTransformation));}});
    connect(m_client, &AO3Client::authorWorksFetched, this, [this](const QString &username, bool bookmarks, const SearchResult &result) {
        if(username == m_listingAuthor && bookmarks == m_listingBookmarks) onSearchCompleted(result);
    });
    connect(m_client,&AO3Client::entitySubscriptionFetched,this,[this](const QString &type,const QString &target,bool available,bool subscribed,const QString &message){
        if(type=="User" && target==m_profileUsername){m_profileSubscription->setVisible(available);m_profileSubscription->setEnabled(available);m_profileSubscription->setText(subscribed?"Unsubscribe":"Subscribe to author");m_profileSubscription->setProperty("subscribed",subscribed);}
        if(type=="Series" && m_listsKind=="series" && target==m_listsId){m_listsData.canSubscribe=available;m_listsData.subscribed=subscribed;if(m_contentStack->currentWidget()==m_listsPage)renderArchiveList(m_listsData);}
        if(!message.isEmpty())statusBar()->showMessage(message);
    });
    connect(m_client,&AO3Client::archivePageFetched,this,[this](const QString &kind,const QString &id,const ArchivePage &page){if(kind==m_listsKind && id==m_listsId && m_contentStack->currentWidget()==m_listsPage){const auto canSubscribe=m_listsData.canSubscribe,subscribed=m_listsData.subscribed;m_listsSelected.clear();m_listsData=page;m_listsData.canSubscribe=canSubscribe;m_listsData.subscribed=subscribed;renderArchiveList(m_listsData);}});
    connect(m_client,&AO3Client::archiveActionFinished,this,[this](const QString &message,bool uncertain){m_listsUncertain=uncertain;statusBar()->showMessage(message);if(m_listsPage && m_contentStack->currentWidget()==m_listsPage)renderArchiveList(m_listsData);});
    connect(m_client,&AO3Client::workDetailsFetched,this,[this](const Work &work){if(work.id==m_listsWorkTarget && m_contentStack->currentWidget()==m_listsPage){m_listsWorkTarget.clear();m_pendingReadId.clear();m_detailsReturn=m_contentStack->indexOf(m_listsPage);m_detailWidget->showWork(work);m_contentStack->setCurrentIndex(2);}});
    connect(m_client,&AO3Client::accountReadingsFetched,this,[this](bool later,const SearchResult &result) {
        if(m_readingMode==(later?"later":"history"))onSearchCompleted(result);
    });
    connect(m_detailWidget, &WorkDetailWidget::downloadRequested, this, [this](const Work &work) {
        statusBar()->showMessage(QStringLiteral("Fetching download formats…"));
        m_client->fetchOfficialDownloads(work.id);
    });
    connect(m_client,&AO3Client::officialDownloadsFetched,this,[this](const QString &id,const QVector<OfficialDownload> &downloads) {
        if(m_detailWidget->currentWork().id!=id) return;
        QStringList choices; for(const auto &item:downloads) choices << item.format.toUpper();
        choices << "Text export (client)";
        bool ok=false; const auto choice=QInputDialog::getItem(this,"Download","Format",choices,0,false,&ok);
        if(!ok) { statusBar()->clearMessage(); return; }
        const int index=choices.indexOf(choice);
        if(index==downloads.size()) { m_client->downloadWork(id); return; }
        const auto item=downloads[index];
        const auto path=QFileDialog::getSaveFileName(this,"Save AO3 download",QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)+"/"+item.url.fileName(),"*."+item.format);
        if(path.isEmpty()) return;
        statusBar()->showMessage("Downloading…"); m_client->saveOfficialDownload(id,item.url,path);
    });
    connect(m_client,&AO3Client::officialDownloadSaved,this,[this](const QString &path) { statusBar()->showMessage("Saved to "+path); });

    connect(m_searchInput, &QLineEdit::returnPressed, this, &MainWindow::performSearch);

    statusBar()->showMessage(QStringLiteral("Ready"));

    QTimer::singleShot(0, this, [this]() {
        m_client->checkSession();

    });
}

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("Archive of Our Own"));
    resize(1440, 900);
    setWindowIcon(QIcon(QStringLiteral(":/icons/ao3.png")));

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_headerBar = new QWidget(centralWidget);m_headerBar->setProperty("skinRole","header");
    m_headerBar->setMinimumHeight(72);
    m_headerBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_headerBar->setStyleSheet(QStringLiteral("background: palette(window);"));
    auto *headerLayout = new QGridLayout(m_headerBar);
    headerLayout->setContentsMargins(16, 8, 16, 8);
    headerLayout->setSpacing(12);

    m_logoLabel = new QLabel(m_headerBar);
    m_logoLabel->setPixmap(QPixmap(QStringLiteral(":/icons/ao3.png")).scaledToHeight(56, Qt::SmoothTransformation));
    m_logoLabel->setStyleSheet(QStringLiteral("background: transparent;"));
    m_logoLabel->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(m_logoLabel, 0, 0);

    m_searchInput = new QLineEdit(m_headerBar);
    m_searchInput->setPlaceholderText(QStringLiteral("Search works..."));
    m_searchInput->setMinimumHeight(32);
    m_searchInput->setMinimumWidth(280);
    m_searchInput->setMaximumWidth(450);
    m_searchInput->setStyleSheet(QStringLiteral(
        "QLineEdit { border: 1px solid palette(mid); border-radius: 4px; padding: 2px 10px; background: palette(base); color: palette(text); font-size: 10pt; }"
    ));
    headerLayout->addWidget(m_searchInput, 0, 1);

    m_searchButton = new QPushButton(QStringLiteral("Search"), m_headerBar);
    m_searchButton->setMinimumHeight(32);
    m_searchButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: palette(highlight); color: palette(highlighted-text); border: none; border-radius: 4px; padding: 2px 18px; font-weight: bold; font-size: 10pt; }"
        "QPushButton:hover { background: palette(highlight); }"
    ));
    connect(m_searchButton, &QPushButton::clicked, this, &MainWindow::performSearch);
    headerLayout->addWidget(m_searchButton, 0, 2);
    m_filterPanel = new SearchFilterPanel(this);
    m_filterButton = new QPushButton(QStringLiteral("Filters"), m_headerBar);
    m_filterButton->setMinimumHeight(32);
    m_filterButton->setStyleSheet(m_searchButton->styleSheet());
    headerLayout->addWidget(m_filterButton, mobileUi() ? 1 : 0, mobileUi() ? 1 : 3);
    m_filterDock = new QDockWidget(QStringLiteral("Search filters"), this);
    m_filterDock->setObjectName(QStringLiteral("searchFiltersDock"));
    m_filterDock->setFeatures(QDockWidget::DockWidgetClosable);
    m_filterDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_filterDock->setWidget(m_filterPanel);
    addDockWidget(Qt::LeftDockWidgetArea, m_filterDock);
    m_filterButton->setCheckable(true);
    m_filterButton->setChecked(false);
    m_filterDock->hide();
    connect(m_filterButton, &QPushButton::clicked, this, [this](bool checked) {
        if (!mobileUi()) { m_filterDock->setVisible(checked); return; }
        m_pendingReadId.clear();
        if (checked) { m_beforeFilters = m_contentStack->currentIndex(); m_contentStack->setCurrentWidget(m_mobileFilters); }
        else m_contentStack->setCurrentIndex(m_beforeFilters);
    });
    connect(m_filterDock, &QDockWidget::visibilityChanged, m_filterButton, &QPushButton::setChecked);
    connect(m_filterPanel, &SearchFilterPanel::searchRequested, this, [this] {
        m_currentFilters = m_filterPanel->filters();
        m_currentQuery = m_searchInput->text().trimmed();
        m_currentPage = 1;
        showSearchView(1);
    });


    auto *headerSpacer = new QWidget(m_headerBar);
    headerSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    headerLayout->addWidget(headerSpacer, 0, 4);
    headerLayout->setColumnStretch(mobileUi() ? 1 : 4, 1);

    m_userGreeting = new QLabel(m_headerBar);
    m_userGreeting->setStyleSheet(QStringLiteral("color: palette(text); font-size: 9pt; background: transparent;"));
    m_userGreeting->hide();
    headerLayout->addWidget(m_userGreeting, 0, 5);

    m_loginButton = new AccountButton(QStringLiteral("Log In"), m_headerBar);
    m_loginButton->setMinimumHeight(32);
    m_loginButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: palette(button); color: palette(button-text); border: 1px solid palette(mid); border-radius: 4px; padding: 2px 18px; font-size: 10pt; }"
        "QPushButton:hover { background: palette(base); }"
    ));
    connect(m_loginButton, &QPushButton::clicked, this, &MainWindow::onLoginClicked);
    headerLayout->addWidget(m_loginButton, mobileUi() ? 1 : 0, mobileUi() ? 2 : 6);
    if (mobileUi()) {
        m_logoLabel->hide(); headerSpacer->hide();
        m_searchInput->setMinimumWidth(0); m_searchInput->setMaximumWidth(QWIDGETSIZE_MAX);
        m_searchButton->setMinimumHeight(44); m_filterButton->setMinimumHeight(44); m_loginButton->setMinimumHeight(44);
        headerLayout->setContentsMargins(8, 6, 8, 6); headerLayout->setSpacing(6);
    }

    if(!mobileUi()) {
        m_headerBar->setProperty("skinEdgeInsets",QVariantList{16,12,16,12});
        delete headerLayout;delete headerSpacer;headerLayout=new QGridLayout(m_headerBar);headerLayout->setContentsMargins(16,12,16,12);headerLayout->setHorizontalSpacing(12);
        auto *identity=new QWidget(m_headerBar);identity->setObjectName("headerIdentity");auto *identityLayout=new QHBoxLayout(identity);identityLayout->setContentsMargins(0,0,0,0);identityLayout->setSpacing(8);identityLayout->addWidget(m_logoLabel);
        auto *name=new QLabel("Archive of Our Own",identity);name->setObjectName("appName");auto font=name->font();font.setPointSize(13);font.setBold(true);name->setFont(font);name->setWordWrap(false);identityLayout->addWidget(name);
        auto *searchGroup=new QWidget(m_headerBar);searchGroup->setObjectName("headerSearch");auto *searchLayout=new QHBoxLayout(searchGroup);searchLayout->setContentsMargins(0,0,0,0);searchLayout->setSpacing(8);m_searchInput->setMinimumWidth(180);searchLayout->addWidget(m_searchInput,1);searchLayout->addWidget(m_searchButton);searchLayout->addWidget(m_filterButton);searchGroup->setMaximumWidth(640);
        headerLayout->addWidget(identity,0,0,Qt::AlignLeft);headerLayout->addWidget(searchGroup,0,1);headerLayout->addWidget(m_loginButton,0,2,Qt::AlignRight);m_loginButton->setMaximumWidth(190);
        const int balance=identity->sizeHint().width();headerLayout->setColumnMinimumWidth(0,balance);headerLayout->setColumnMinimumWidth(2,balance);headerLayout->setColumnStretch(0,1);headerLayout->setColumnStretch(1,2);headerLayout->setColumnStretch(2,1);
    }
    mainLayout->addWidget(m_headerBar);

    m_navBar = new QWidget(centralWidget);
    m_navBar->setObjectName(QStringLiteral("navigationBar"));
    m_navBar->setStyleSheet(QStringLiteral("QWidget#navigationBar { background: transparent; }"));
    auto *navLayout = new QHBoxLayout(m_navBar);
    navLayout->setContentsMargins(0, 0, 0, 0);
    navLayout->setSpacing(8);

    auto makeNavBtn = [](const QString &text) {
        auto *btn = new QPushButton(text);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; color: palette(text); padding: 8px 14px; border: none; font-size: 9pt; font-weight: bold; }"
            "QPushButton:hover { color: palette(highlight); background: palette(button); }"
        ));
        return btn;
    };

    m_navHome = makeNavBtn(QStringLiteral("Home"));
    m_navSearch = makeNavBtn(QStringLiteral("Search"));
    m_navWorks = makeNavBtn(QStringLiteral("My Works"));
    m_navLogout = makeNavBtn(QStringLiteral("Sign Out"));
    m_navLogout->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: palette(highlight); padding: 8px 14px; border: none; font-size: 9pt; font-weight: bold; }"
        "QPushButton:hover { background: palette(button); }"
    ));

    connect(m_navWorks, &QPushButton::clicked, this, &MainWindow::showUserWorks);
    connect(m_navHome, &QPushButton::clicked, this, &MainWindow::showDashboard);
    connect(m_navSearch, &QPushButton::clicked, this, [this]() {
        m_pendingReadId.clear();
        if (m_contentStack->currentWidget() == m_reader) closeReader();
        m_contentStack->setCurrentIndex(2);
    });
    connect(m_navLogout, &QPushButton::clicked, this, &MainWindow::onLogout);

    navLayout->addWidget(m_navHome);
    if (!mobileUi()) {
        auto *library = makeNavBtn(QStringLiteral("Library"));
        navLayout->addWidget(library); connect(library, &QPushButton::clicked, this, &MainWindow::showLibrary);
    }
    navLayout->addWidget(m_navSearch);
    navLayout->addWidget(m_navWorks);
    m_navWorks->hide();

    navLayout->addStretch();
    navLayout->addWidget(m_navLogout);
    auto *readingSettings = makeNavBtn(QStringLiteral("Reading settings"));
    connect(readingSettings, &QPushButton::clicked, this, [this] { m_reader->showSettings(); });
    navLayout->addWidget(readingSettings);
    auto *online=makeNavBtn("AO3 website"); auto *onlineMenu=new QMenu(online); online->setMenu(onlineMenu); navLayout->addWidget(online);
    onlineMenu->addAction("AO3 website",this,[this] { openArchiveWebsite(QUrl("https://archiveofourown.org/")); });
    for(const auto &item:QVector<QPair<QString,QString>>{{"Profile pictures","pseuds"},{"My profile",""},{"My AO3 bookmarks","bookmarks"},{"My subscriptions","subscriptions"},{"Reading history","readings"},{"Marked for later","readings?show=to-read"},{"Inbox","inbox"},{"Skins","skins"},{"Account preferences","preferences"},{"Drafts","works/drafts"}}) {
        onlineMenu->addAction(item.first,this,[this,item] { const QString user=m_loginManager->username(); openArchiveWebsite(QUrl(user.isEmpty()?"https://archiveofourown.org/users/login":"https://archiveofourown.org/users/"+QString::fromLatin1(QUrl::toPercentEncoding(user))+"/"+item.second)); });
    }
    if (!mobileUi()) { auto *profile = makeNavBtn("My profile"); navLayout->addWidget(profile); connect(profile, &QPushButton::clicked, this, [this] { if(m_loginManager->isLoggedIn()) showProfile("/users/" + m_loginManager->username()); else showLoginView(); }); }


    m_navLogout->hide();

    if (mobileUi()) {
        readingSettings->setText(QStringLiteral("Settings"));
        m_navWorks->setText(QStringLiteral("My works")); m_navWorks->show();
        navLayout->setContentsMargins(4, 4, 4, 4); navLayout->setSpacing(0);
        delete navLayout->takeAt(3);
        for (auto *button : {m_navHome, m_navSearch, m_navWorks, readingSettings}) {
            button->setCheckable(true); button->setMinimumWidth(0); button->setMinimumHeight(48);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            button->setStyleSheet(QStringLiteral(
                "QPushButton { background: palette(base); color: palette(text); border: none; border-radius: 8px; padding: 8px 2px; font-size: 10pt; }"
                "QPushButton:checked { background: palette(button); color: palette(highlight); font-weight: bold; }"));
        }
        readingSettings->setCheckable(false);
    } else {
        headerLayout->addWidget(m_navBar,1,0,1,3);
        headerLayout->setVerticalSpacing(6);
        headerLayout->setSizeConstraint(QLayout::SetNoConstraint);
        m_headerBar->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Minimum);
        m_headerBar->installEventFilter(this);
        QTimer::singleShot(0,this,&MainWindow::layoutDesktopHeader);
    }

    m_contentStack = mobileUi() ? new MobileStack(centralWidget) : new QStackedWidget(centralWidget);
    m_contentStack->setObjectName(QStringLiteral("contentPages"));m_contentStack->setProperty("skinLayerTarget","window");
    m_contentStack->setStyleSheet(QStringLiteral("QStackedWidget#contentPages { background: palette(base); }"));

    // Dashboard view (0)
    m_dashboardView = new DashboardWidget(this);
    m_contentStack->addWidget(m_dashboardView);
    connect(m_dashboardView, &DashboardWidget::browseRequested, this, [this] {
        m_contentStack->setCurrentIndex(2); m_searchInput->setFocus();
    });

    connect(m_dashboardView,&DashboardWidget::libraryRequested,this,&MainWindow::showLibrary);
    connect(m_dashboardView,&DashboardWidget::skinRequested,this,[this]{editAppSkin(this);});
    connect(m_dashboardView,&DashboardWidget::workRequested,this,[this](const QString &id){for(const auto &value:ReadingLibrary().entries()){const auto entry=value.toObject();if(ReadingLibrary::work(entry).id==id){openLibraryEntry(entry);break;}}});

    // Login view (1)
    m_loginView = new LoginDialog(this);
    if (mobileUi()) {
        auto *loginScroll = new QScrollArea(m_contentStack); loginScroll->setWidgetResizable(true);
        loginScroll->setFrameShape(QFrame::NoFrame); loginScroll->setWidget(m_loginView);
        m_contentStack->addWidget(loginScroll);
    } else m_contentStack->addWidget(m_loginView);
    connect(m_loginView, &LoginDialog::loginRequested, this, &MainWindow::onLoginRequested);
    connect(m_loginView, &LoginDialog::cancelled, this, [this]() {
        showDashboard();
    });

    // Search view (2) — splitter with work list + detail
    auto *searchView = new QWidget(this);
    auto *searchViewLayout = new QVBoxLayout(searchView);
    searchViewLayout->setContentsMargins(0, 0, 0, 0);
    searchViewLayout->setSpacing(0);
    if (mobileUi()) {
        auto *searchBar = new QWidget(searchView);
        auto *searchRow = new QHBoxLayout(searchBar); searchRow->setContentsMargins(12, 8, 12, 8);
        m_searchInput->setParent(searchBar); m_searchButton->setParent(searchBar);
        m_searchInput->setMinimumHeight(44); m_searchInput->setPlaceholderText(QStringLiteral("Title, author, or keywords"));
        m_searchButton->setText(QStringLiteral("Go"));
        searchRow->addWidget(m_searchInput, 1); searchRow->addWidget(m_searchButton);
        searchViewLayout->addWidget(searchBar);
        auto *tools = new QHBoxLayout; tools->setContentsMargins(12, 0, 12, 8);
        auto *heading = new QLabel(QStringLiteral("Search"), searchView);
        heading->setStyleSheet(QStringLiteral("font-size: 12pt; font-weight: bold; color: palette(text);"));
        tools->addWidget(heading, 1);
        m_filterButton->setParent(searchView); m_filterButton->setMinimumHeight(44);
        tools->addWidget(m_filterButton); searchViewLayout->addLayout(tools);
        m_headerBar->setMinimumHeight(0); m_headerBar->setMaximumHeight(60);
        auto *brand = new QLabel(QStringLiteral("AO3 Client"), m_headerBar);
        brand->setStyleSheet(QStringLiteral("color: palette(text); font-size: 17pt; font-weight: bold; background: transparent;"));
        headerLayout->removeWidget(m_loginButton); headerLayout->addWidget(brand, 0, 1);
        headerLayout->addWidget(m_loginButton, 0, 2);
        m_loginButton->setText(QStringLiteral("Sign in"));
    }


    auto *searchModes=new QWidget(searchView);auto *modes=new QHBoxLayout(searchModes);modes->setContentsMargins(16,8,16,8);
    modes->addWidget(new QLabel("Search:",searchModes));
    for(const auto &mode:QVector<QPair<QString,QString>>{{"Works",""},{"People","people"},{"Tags","tags"},{"Bookmarks","bookmarks"}}) {
        auto *button=new QPushButton(mode.first,searchModes);button->setObjectName("searchMode_"+mode.second);modes->addWidget(button);
        connect(button,&QPushButton::clicked,this,[this,mode]{if(mode.second.isEmpty()){m_searchInput->setFocus();return;}m_discoveryEditing=true;m_listsFilter.clear();showArchiveList("search:"+mode.second,{},0);});
    }
    modes->addStretch();searchViewLayout->addWidget(searchModes);
    m_searchSplitter = new QSplitter(Qt::Horizontal, searchView);
    m_searchSplitter->setHandleWidth(1);
    m_searchSplitter->setStyleSheet(QStringLiteral("QSplitter::handle { background: palette(mid); }"));

    m_workList = new QListWidget(m_searchSplitter);
    m_workList->setMinimumWidth(280);
    m_workList->viewport()->installEventFilter(this);
    m_workList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_workList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_workList->setStyleSheet(QStringLiteral(
        "QListWidget { border: none; background: palette(base); }"
        "QListWidget::item { border-bottom: 1px solid palette(button); padding: 0; }"
        "QListWidget::item:selected { background: palette(button); }"
    ));

    if (!mobileUi()) connect(m_workList, &QListWidget::currentRowChanged, this, &MainWindow::onWorkSelected);

    auto *detailScroll = new QScrollArea(m_searchSplitter);
    detailScroll->setWidgetResizable(true);
    if (mobileUi()) detailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detailScroll->setFrameShape(QFrame::NoFrame);
    m_detailWidget = new WorkDetailWidget(detailScroll);
    detailScroll->setWidget(m_detailWidget);
    m_searchSplitter->addWidget(m_workList);
    m_searchSplitter->addWidget(detailScroll);
    m_searchSplitter->setStretchFactor(0, 3);
    m_searchSplitter->setStretchFactor(1, 4);

    searchViewLayout->addWidget(m_searchSplitter, 1);

    // Pagination bar
    m_paginationWidget = new QWidget(searchView);
    m_paginationWidget->setStyleSheet(QStringLiteral(
        "background: palette(base); border-top: 1px solid palette(mid); padding: 6px;"
    ));
    auto *pageLayout = new QHBoxLayout(m_paginationWidget);
    pageLayout->setContentsMargins(12, 6, 12, 6);

    m_prevButton = new QPushButton(QStringLiteral("Previous"), m_paginationWidget);
    m_prevButton->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    m_prevButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: palette(highlight); border: 1px solid palette(mid); border-radius: 3px; padding: 4px 14px; background: palette(base); font-size: 9pt; }"
        "QPushButton:hover { background: palette(button); }"
        "QPushButton:disabled { color: palette(mid); }"
    ));
    m_pageLabel = new QLabel(m_paginationWidget);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    m_pageLabel->setStyleSheet(QStringLiteral("color: palette(text); font-size: 9pt;"));
    m_nextButton = new QPushButton(QStringLiteral("Next"), m_paginationWidget);
    m_nextButton->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    m_nextButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: palette(highlight); border: 1px solid palette(mid); border-radius: 3px; padding: 4px 14px; background: palette(base); font-size: 9pt; }"
        "QPushButton:hover { background: palette(button); }"
        "QPushButton:disabled { color: palette(mid); }"
    ));

    pageLayout->addWidget(m_prevButton);
    pageLayout->addWidget(m_pageLabel, 1);
    pageLayout->addWidget(m_nextButton);
    m_paginationWidget->hide();

    connect(m_prevButton, &QPushButton::clicked, this, [this]() { goToPage(m_currentPage - 1); });
    connect(m_nextButton, &QPushButton::clicked, this, [this]() { goToPage(m_currentPage + 1); });

    searchViewLayout->addWidget(m_paginationWidget);
    m_contentStack->addWidget(searchView);

    m_reader = new ReaderWidget(this);
    m_contentStack->addWidget(m_reader);
    connect(m_reader, &ReaderWidget::backRequested, this, &MainWindow::closeReader);
    connect(m_reader,&ReaderWidget::kudosRequested,m_client,&AO3Client::giveKudos);
    connect(m_reader,&ReaderWidget::workOpened,this,[this](const QString &id) {m_reader->setKudosStatus(m_client->kudosStatus(id),!m_loginManager->isLoggedIn());});
    connect(m_reader, &ReaderWidget::fullscreenRequested, this, &MainWindow::toggleReaderFullscreen);
    connect(m_reader, &ReaderWidget::preferencesChanged, this, [this] {
        auto filters = m_filterPanel->filters();
        if (QSettings().value(QStringLiteral("reader/filterLanguage"), false).toBool())
            filters[QStringLiteral("language_id")] = {QSettings().value(QStringLiteral("reader/language"), QStringLiteral("en")).toString()};
        else filters.remove(QStringLiteral("language_id"));
        m_filterPanel->setFilters(filters);
    });
    auto *fullscreen = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fullscreen, &QShortcut::activated, this, &MainWindow::toggleReaderFullscreen);
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, this, [this] {
        if (m_contentStack->currentWidget() == m_reader && m_reader->closeOptions()) return;
        if (m_contentStack->currentWidget() == m_reader && isFullScreen()) { showNormal(); m_reader->setFullscreen(false); }
    });
    if (mobileUi()) {
        m_filterDock->hide(); removeDockWidget(m_filterDock);
        m_filterPanel->setParent(nullptr);
        m_mobileFilters = new QWidget(m_contentStack);
        auto *filtersLayout = new QVBoxLayout(m_mobileFilters);
        auto *back = new QPushButton(QStringLiteral("Back to browsing"), m_mobileFilters);
        back->setMinimumHeight(44);
        filtersLayout->addWidget(back); filtersLayout->addWidget(m_filterPanel, 1);
        m_contentStack->addWidget(m_mobileFilters);
        connect(back, &QPushButton::clicked, this, [this] { navigateBack(); });
        m_filterButton->setChecked(false);
        connect(m_contentStack, &QStackedWidget::currentChanged, this, [this] {
            m_filterButton->setChecked(m_contentStack->currentWidget() == m_mobileFilters);
        });
        m_mobileDetails = new QWidget(m_contentStack);
        auto *detailsLayout = new QVBoxLayout(m_mobileDetails);
        auto *results = new QPushButton(QStringLiteral("Back to results"), m_mobileDetails);
        results->setMinimumHeight(44); detailsLayout->addWidget(results);
        detailScroll->setParent(m_mobileDetails); detailsLayout->addWidget(detailScroll, 1);
        m_contentStack->addWidget(m_mobileDetails);
        connect(results, &QPushButton::clicked, this, [this] { m_contentStack->setCurrentIndex(2); });
        // A tap opens details, including when the same result is selected again.
        connect(m_workList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) { onWorkSelected(m_workList->row(item)); });
        QScroller::grabGesture(m_workList->viewport(), QScroller::TouchGesture);
        statusBar()->setSizeGripEnabled(false);
        m_prevButton->setText(QStringLiteral("Back"));
        m_prevButton->setMinimumHeight(44); m_nextButton->setMinimumHeight(44);
    }
    m_libraryPage = new QWidget(m_contentStack);
    auto *libraryLayout = new QVBoxLayout(m_libraryPage);
    auto *libraryHeading = new QLabel(QStringLiteral("Library"), m_libraryPage);
    libraryHeading->setStyleSheet(QStringLiteral("font-size: 18pt; font-weight: bold; color: palette(text);")); libraryLayout->addWidget(libraryHeading);
    m_libraryFilter = new QComboBox(m_libraryPage); m_libraryFilter->addItems({"Recent reads", "Offline saved", "Favorites"}); libraryLayout->addWidget(m_libraryFilter);
    m_librarySearch=new QLineEdit(m_libraryPage); m_librarySearch->setPlaceholderText("Search library by title or author"); libraryLayout->addWidget(m_librarySearch); connect(m_librarySearch,&QLineEdit::textChanged,this,[this] { refreshLibrary(); });
    auto *backupRow=new QHBoxLayout; auto *backup=new QPushButton("Export library backup",m_libraryPage); auto *restore=new QPushButton("Import library backup",m_libraryPage); backupRow->addWidget(backup); backupRow->addWidget(restore); libraryLayout->addLayout(backupRow);
    connect(backup,&QPushButton::clicked,this,[this] {
        m_reader->saveProgress(); const auto path=QFileDialog::getSaveFileName(this,"Export library backup","ao3-library-backup.json","JSON files (*.json)"); if(path.isEmpty()) return;
        const bool includeDownloads=QMessageBox::question(this,"Backup contents","Include downloaded chapter text? Choose No for a compact reading-data backup (maximum backup size: 100 MB).",QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes)==QMessageBox::Yes;
        const auto data=QJsonDocument(LibraryBackup::snapshot(ReadingLibrary(),includeDownloads)).toJson(QJsonDocument::Compact); if(data.size()>100*1024*1024) { onError("Backup exceeds 100 MB. Export without downloads and save large fics separately as text."); return; } QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit()) { onError("Could not save backup. Check storage and permissions."); return; }
        statusBar()->showMessage("Library backup exported. It contains reading history and downloaded text; keep it private.");
    });
    connect(restore,&QPushButton::clicked,this,[this] {
        const auto path=QFileDialog::getOpenFileName(this,"Import library backup",{},"JSON files (*.json)"); if(path.isEmpty()) return;
        if(QMessageBox::question(this,"Import library backup?","Merge the backup into your library? Existing downloads and reading positions are kept.")!=QMessageBox::Yes) return;
        QFile file(path); if(file.size()>100*1024*1024 || !file.open(QIODevice::ReadOnly)) { onError("Could not read backup (maximum 100 MB)"); return; }
        QString error; if(!LibraryBackup::merge(QJsonDocument::fromJson(file.readAll()).object(),&error)) { onError(error); return; }
        refreshLibrary(); statusBar()->showMessage("Library backup imported. Existing downloads and reading positions were kept.");
    });
    m_libraryList = new QListWidget(m_libraryPage); m_libraryList->setWordWrap(true); libraryLayout->addWidget(m_libraryList, 1);
    auto *libraryActions = new QHBoxLayout;
    auto *openSaved = new QPushButton("Open / continue", m_libraryPage); auto *favorite = new QPushButton("Toggle favorite", m_libraryPage); auto *removeSaved = new QPushButton("Remove offline copy", m_libraryPage);
    libraryActions->addWidget(openSaved); libraryActions->addWidget(favorite); libraryActions->addWidget(removeSaved); libraryLayout->addLayout(libraryActions);
    m_contentStack->addWidget(m_libraryPage);
    connect(m_libraryFilter, &QComboBox::currentIndexChanged, this, [this] { refreshLibrary(); });
    connect(openSaved, &QPushButton::clicked, this, &MainWindow::openLibrarySelection);
    connect(m_libraryList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { openLibrarySelection(); });
    connect(favorite, &QPushButton::clicked, this, [this] {
        auto *item = m_libraryList->currentItem(); if (!item) return;
        const auto entry = QJsonObject::fromVariantMap(item->data(Qt::UserRole).toMap());
        if (!ReadingLibrary().favorite(ReadingLibrary::work(entry).id, !entry.value("favorite").toBool())) onError("Could not update favorite");
        refreshLibrary();
    });
    connect(removeSaved, &QPushButton::clicked, this, [this] {
        auto *item = m_libraryList->currentItem(); if (!item) return;
        const auto entry = QJsonObject::fromVariantMap(item->data(Qt::UserRole).toMap());
        if (!ReadingLibrary().removeOffline(ReadingLibrary::work(entry).id)) onError("Could not remove offline copy");
        refreshLibrary();
    });
    m_profilePage = new QWidget(m_contentStack); auto *profileLayout = new QVBoxLayout(m_profilePage);
    m_profileAvatar=new QLabel(m_profilePage);m_profileAvatar->setObjectName("profileAvatar");m_profileAvatar->setFixedSize(80,80);m_profileAvatar->setAlignment(Qt::AlignCenter);auto avatarFont=m_profileAvatar->font();avatarFont.setPointSize(28);m_profileAvatar->setFont(avatarFont);profileLayout->addWidget(m_profileAvatar);
    auto *profileActions = new QHBoxLayout; auto *authorWorks = new QPushButton("Author's works", m_profilePage); auto *authorBookmarks = new QPushButton("Public AO3 bookmarks", m_profilePage);
    m_profileSubscription=new QPushButton("Subscribe to author",m_profilePage);m_profileSubscription->hide();profileActions->addWidget(m_profileSubscription);connect(m_profileSubscription,&QPushButton::clicked,this,[this]{m_profileSubscription->setEnabled(false);m_client->entitySubscription("User",m_profileUsername,!m_profileSubscription->property("subscribed").toBool());});
    profileActions->addWidget(authorWorks); profileActions->addWidget(authorBookmarks); profileLayout->addLayout(profileActions);
    m_profileText = new QTextBrowser(m_profilePage); profileLayout->addWidget(m_profileText, 1); m_contentStack->addWidget(m_profilePage);
    connect(authorWorks, &QPushButton::clicked, this, [this] { showAuthorListing(false); }); connect(authorBookmarks, &QPushButton::clicked, this, [this] { showAuthorListing(true); });
    m_commentsPage=new QWidget(m_contentStack); auto *commentsLayout=new QVBoxLayout(m_commentsPage);
    auto *commentsNavigation=new QHBoxLayout; auto *commentsBack=new QPushButton("Back",m_commentsPage); commentsNavigation->addWidget(commentsBack);
    commentsNavigation->addWidget(new QLabel("Comments",m_commentsPage),1);
    auto *commentsRefresh=new QPushButton("Refresh",m_commentsPage); commentsNavigation->addWidget(commentsRefresh); commentsLayout->addLayout(commentsNavigation);
    m_commentsText=new QTextBrowser(m_commentsPage); m_commentsText->setObjectName("commentsText"); m_commentsText->setOpenLinks(false); commentsLayout->addWidget(m_commentsText,1);
    auto *commentPages=new QHBoxLayout; m_commentsPrevious=new QPushButton("Previous",m_commentsPage);m_commentsNext=new QPushButton("Next",m_commentsPage);m_commentsNumber=new QLabel(m_commentsPage);
    commentPages->addWidget(m_commentsPrevious);commentPages->addWidget(m_commentsNumber,1,Qt::AlignCenter);commentPages->addWidget(m_commentsNext);commentsLayout->addLayout(commentPages);m_contentStack->addWidget(m_commentsPage);
    m_commentNew=new QPushButton("Add comment",m_commentsPage);commentsNavigation->addWidget(m_commentNew);
    m_commentEditor=new QWidget(m_commentsPage);auto *editorLayout=new QVBoxLayout(m_commentEditor);
    m_commentNotice=new QLabel(m_commentEditor);m_commentNotice->setWordWrap(true);editorLayout->addWidget(m_commentNotice);
    m_commentPseud=new QComboBox(m_commentEditor);editorLayout->addWidget(m_commentPseud);
    m_commentName=new QLineEdit(m_commentEditor);m_commentName->setPlaceholderText("Name");editorLayout->addWidget(m_commentName);
    m_commentEmail=new QLineEdit(m_commentEditor);m_commentEmail->setPlaceholderText("Email (not displayed)");editorLayout->addWidget(m_commentEmail);
    m_commentDraft=new QTextEdit(m_commentEditor);m_commentDraft->setAcceptRichText(false);m_commentDraft->setPlaceholderText("Comment");m_commentDraft->setMaximumHeight(160);editorLayout->addWidget(m_commentDraft);
    auto *editorActions=new QHBoxLayout;m_commentPost=new QPushButton("Post",m_commentEditor);m_commentClose=new QPushButton("Close",m_commentEditor);editorActions->addWidget(m_commentPost);editorActions->addWidget(m_commentClose);editorLayout->addLayout(editorActions);
    commentsLayout->insertWidget(1,m_commentEditor);m_commentEditor->hide();
    connect(m_commentNew,&QPushButton::clicked,this,[this] {composeComment();});
    connect(m_commentClose,&QPushButton::clicked,this,[this] {if(!m_commentSending)m_commentEditor->hide();});
    connect(m_commentsText,&QTextBrowser::anchorClicked,this,[this](const QUrl &url) {if(url.scheme()=="reply")composeComment(url.path());else if(url.scheme()=="manage"){const auto path=url.path(QUrl::FullyDecoded);manageComment(path.section('/',0,0),path.section('/',1));}});
    connect(m_client,&AO3Client::commentFormFetched,this,[this](const QString &id,const CommentForm &form) {
        if(id!=m_commentsWork || form.context!=m_commentContext)return;
        if(!form.error.isEmpty()) {onError(form.error);return;}
        m_commentForm=form;if(form.editing)m_commentDraft->setPlainText(form.initialText);m_commentNotice->setText((form.editing?QString("Edit comment"):form.context.isEmpty()?QString("New comment"):QString("Reply to comment #%1").arg(form.context))+"\n"+form.notice);m_commentPseud->clear();
        for(const auto &p:form.pseuds)m_commentPseud->addItem(p.second,p.first);
        m_commentPseud->setCurrentIndex(qMax(0,m_commentPseud->findData(form.hidden.value("comment[pseud_id]"))));m_commentPseud->setVisible(form.pseuds.size()>1);
        m_commentName->setVisible(form.guest);m_commentEmail->setVisible(form.guest);m_commentPost->setText(form.editing?"Save changes":"Post");m_commentPost->setEnabled(!m_commentUncertain);
        m_commentEditor->show();m_commentDraft->setFocus();
    });
    connect(m_commentPost,&QPushButton::clicked,this,[this] {
        if(m_commentSending || m_commentUncertain || m_commentDraft->toPlainText().trimmed().isEmpty())return;
        m_commentSending=true;m_commentEditor->setEnabled(false);m_commentNew->setEnabled(false);
        m_client->submitComment(m_commentsWork,m_commentForm,m_commentDraft->toPlainText(),m_commentPseud->currentData().toString(),m_commentName->text(),m_commentEmail->text());
    });
    connect(m_client,&AO3Client::commentSubmitted,this,[this](const QString &id,const CommentSubmission &result) {
        if(id!=m_commentsWork)return;
        m_commentSending=false;m_commentEditor->setEnabled(true);m_commentUncertain=result.uncertain;m_commentPost->setEnabled(!m_commentUncertain);m_commentNew->setEnabled(!m_commentUncertain);
        if(!result.error.isEmpty()) {m_commentDeletion.clear();m_commentNotice->setText(result.error);return;}
        m_commentDraft->clear();m_commentEditor->hide();statusBar()->showMessage(result.notice);if(m_contentStack->currentWidget()==m_commentsPage && !m_commentDeletion.isEmpty() && id=="comment:"+m_commentDeletion){m_commentDeletion.clear();m_contentStack->setCurrentIndex(m_commentsReturn);if(m_contentStack->currentWidget()==m_listsPage)m_client->fetchArchivePage(m_listsKind,m_listsId,m_listsData.page);return;}m_commentDeletion.clear();if(m_contentStack->currentWidget()==m_commentsPage)showComments(id,m_commentsCurrent);
    });
    connect(commentsBack,&QPushButton::clicked,this,[this] {m_contentStack->setCurrentIndex(m_commentsReturn);});
    connect(commentsRefresh,&QPushButton::clicked,this,[this] {showComments(m_commentsWork,m_commentsCurrent);});
    connect(m_commentsPrevious,&QPushButton::clicked,this,[this] {showComments(m_commentsWork,m_commentsCurrent-1);});
    connect(m_commentsNext,&QPushButton::clicked,this,[this] {showComments(m_commentsWork,m_commentsCurrent+1);});
    connect(m_detailWidget,&WorkDetailWidget::commentsRequested,this,[this](const QString &id){showComments(id);});
    connect(m_reader,&ReaderWidget::commentsRequested,this,[this](const QString &id){showComments(id);});
    connect(m_client,&AO3Client::commentsFetched,this,[this](const QString &id,const CommentPage &page) {
        if(id!=m_commentsWork)return;
        m_commentUncertain=false;m_commentPost->setEnabled(true);m_commentNew->setEnabled(!m_commentSending);
        m_commentsCurrent=page.page;m_commentsNumber->setText(QString("%1 / %2").arg(page.page).arg(page.pages));
        m_commentsPrevious->setEnabled(page.page>1);m_commentsNext->setEnabled(page.page<page.pages);
        QString html;
        for(const auto &comment:page.comments){html+=QString("<div style='margin-left:%1px'><h3>%2</h3><p>%3</p><p>%4</p>").arg(qMin(comment.depth,4)*16).arg(comment.author.toHtmlEscaped(),comment.date.toHtmlEscaped(),QString(comment.text).toHtmlEscaped().replace("\n","<br>"));if(comment.canReply)html+="<a href='reply:"+comment.id+"'>Reply</a> ";for(const auto &action:comment.actions)html+="<a href='manage:"+comment.id+"/"+QString::fromLatin1(QUrl::toPercentEncoding(action))+"'>"+action.toHtmlEscaped()+"</a> ";html+="</div><hr>";}
        m_commentsText->setHtml(html.isEmpty()?"No comments.":html);
    });

    m_archiveBookmarkPage=new QWidget(m_contentStack);auto *bookmarkLayout=new QVBoxLayout(m_archiveBookmarkPage);
    auto *bookmarkBack=new QPushButton("Back",m_archiveBookmarkPage);bookmarkLayout->addWidget(bookmarkBack,0,Qt::AlignLeft);
    m_archiveBookmarkStatus=new QLabel(m_archiveBookmarkPage);m_archiveBookmarkStatus->setWordWrap(true);bookmarkLayout->addWidget(m_archiveBookmarkStatus);
    auto *bookmarkScroll=new QScrollArea(m_archiveBookmarkPage);bookmarkScroll->setWidgetResizable(true);bookmarkLayout->addWidget(bookmarkScroll,1);
    m_archiveBookmarkFields=new QWidget;auto *bookmarkFields=new QFormLayout(m_archiveBookmarkFields);
    m_archiveBookmarkPseud=new QComboBox(m_archiveBookmarkFields);bookmarkFields->addRow("Pseud",m_archiveBookmarkPseud);
    m_archiveBookmarkNotes=new QTextEdit(m_archiveBookmarkFields);m_archiveBookmarkNotes->setAcceptRichText(false);m_archiveBookmarkNotes->setMinimumHeight(180);bookmarkFields->addRow("Notes (HTML allowed)",m_archiveBookmarkNotes);
    m_archiveBookmarkTags=new QLineEdit(m_archiveBookmarkFields);bookmarkFields->addRow("Tags, comma separated",m_archiveBookmarkTags);
    m_archiveBookmarkCollections=new QLineEdit(m_archiveBookmarkFields);bookmarkFields->addRow("Collections, comma separated",m_archiveBookmarkCollections);
    m_archiveBookmarkPrivate=new QCheckBox("Private bookmark",m_archiveBookmarkFields);bookmarkFields->addRow(m_archiveBookmarkPrivate);
    m_archiveBookmarkRec=new QCheckBox("Recommendation",m_archiveBookmarkFields);bookmarkFields->addRow(m_archiveBookmarkRec);bookmarkScroll->setWidget(m_archiveBookmarkFields);
    auto *bookmarkButtons=new QHBoxLayout;bookmarkLayout->addLayout(bookmarkButtons);
    m_archiveBookmarkSave=new QPushButton("Save to AO3",m_archiveBookmarkPage);bookmarkButtons->addWidget(m_archiveBookmarkSave);
    m_archiveBookmarkDelete=new QPushButton("Delete AO3 bookmark",m_archiveBookmarkPage);bookmarkButtons->addWidget(m_archiveBookmarkDelete);
    m_archiveBookmarkReload=new QPushButton("Reload from AO3",m_archiveBookmarkPage);bookmarkButtons->addWidget(m_archiveBookmarkReload);bookmarkButtons->addStretch();
    m_contentStack->addWidget(m_archiveBookmarkPage);
    connect(bookmarkBack,&QPushButton::clicked,this,[this] {m_contentStack->setCurrentIndex(m_archiveBookmarkReturn);});
    connect(m_archiveBookmarkSave,&QPushButton::clicked,this,[this] {submitArchiveBookmark();});
    connect(m_archiveBookmarkDelete,&QPushButton::clicked,this,[this] {if(QMessageBox::question(this,"Delete AO3 bookmark?","Delete your account bookmark, including its notes and tags?")==QMessageBox::Yes)submitArchiveBookmark(true);});
    connect(m_archiveBookmarkReload,&QPushButton::clicked,this,[this] {if(!m_archiveBookmarkSending && QMessageBox::question(this,"Reload bookmark?","Replace this draft with the bookmark saved on AO3?")==QMessageBox::Yes)editArchiveBookmark(m_archiveBookmarkWork,true);});
    connect(m_client,&AO3Client::archiveBookmarkFetched,this,[this](const ArchiveBookmarkForm &form) {
        if(form.workId!=m_archiveBookmarkWork)return;
        m_archiveBookmarkStatus->setText(form.error.isEmpty()?(form.id.isEmpty()?"New AO3 bookmark":"Edit AO3 bookmark"):form.error);
        if(!form.error.isEmpty())return;
        m_archiveBookmark=form;m_archiveBookmarkUncertain=false;m_archiveBookmarkFields->setEnabled(true);m_archiveBookmarkSave->setEnabled(true);m_archiveBookmarkDelete->setEnabled(true);m_archiveBookmarkDelete->setVisible(!form.id.isEmpty());
        m_archiveBookmarkPseud->clear();for(const auto &p:form.pseuds)m_archiveBookmarkPseud->addItem(p.second,p.first);m_archiveBookmarkPseud->setCurrentIndex(m_archiveBookmarkPseud->findData(form.pseud));if(auto *fields=qobject_cast<QFormLayout *>(m_archiveBookmarkFields->layout()))fields->setRowVisible(m_archiveBookmarkPseud,form.pseuds.size()>1);
        m_archiveBookmarkNotes->setPlainText(form.notes);m_archiveBookmarkTags->setText(form.tags);m_archiveBookmarkCollections->setText(form.collections);m_archiveBookmarkPrivate->setChecked(form.privateBookmark);m_archiveBookmarkRec->setChecked(form.recommendation);
    });
    connect(m_client,&AO3Client::archiveBookmarkSaved,this,[this](const QString &id,const ArchiveFormResult &result) {
        if(id!=m_archiveBookmarkWork)return;
        m_archiveBookmarkSending=false;m_archiveBookmarkUncertain=result.uncertain;m_archiveBookmarkFields->setEnabled(true);m_archiveBookmarkSave->setEnabled(!result.uncertain);m_archiveBookmarkDelete->setEnabled(!result.uncertain);m_archiveBookmarkReload->setEnabled(true);m_archiveBookmarkStatus->setText(result.message);
        if(result.success){if(m_contentStack->currentWidget()==m_archiveBookmarkPage)m_contentStack->setCurrentIndex(m_archiveBookmarkReturn);clearArchiveBookmark();statusBar()->showMessage(result.message);}
    });
    mainLayout->addWidget(m_contentStack, 1);
    if (mobileUi()) {
        m_mobileStatus = new QLabel(centralWidget); m_mobileStatus->setWordWrap(true);
        m_mobileStatus->setMinimumWidth(0); m_mobileStatus->setMaximumHeight(96);
        m_mobileStatus->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        m_mobileStatus->setStyleSheet(QStringLiteral("color: palette(text); background: palette(base); padding: 6px 12px; font-size: 10pt;"));
        mainLayout->addWidget(m_mobileStatus); mainLayout->addWidget(m_navBar);
        connect(statusBar(), &QStatusBar::messageChanged, m_mobileStatus, &QLabel::setText);
        connect(m_contentStack, &QStackedWidget::currentChanged, this, [this](int index) {
            m_navHome->setChecked(index == 0); m_navSearch->setChecked(index == 2 || index >= 4);
            m_navWorks->setChecked(index == 2 && m_showingUserWorks);
            if (m_showingUserWorks && index == 2) m_navSearch->setChecked(false);
        });
        statusBar()->hide();
    }


    // Status bar
    statusBar()->setStyleSheet(QStringLiteral(
        "QStatusBar { background: palette(button); border-top: 1px solid palette(mid); color: palette(text); font-size: 8pt; }"
    ));

    setCentralWidget(centralWidget);
    if (!mobileUi()) resizeDocks({m_filterDock}, {320}, Qt::Horizontal);
    showDashboard();
    if (mobileUi()) applyReadableTheme(this);
}

void MainWindow::performSearch()
{
    QGuiApplication::inputMethod()->hide();
    const QString query = m_searchInput->text().trimmed();
    const SearchFilters filters = m_filterPanel->filters();
    if (query.isEmpty() && filters.isEmpty())
        return;
    m_currentQuery = query;
    m_currentFilters = filters;
    m_currentPage = 1;
    showSearchView(1);
}

void MainWindow::showSearchView(int page)
{
    m_readingMode.clear();m_listingAuthor.clear();
    m_pendingReadId.clear();
    if (m_contentStack->currentWidget() == m_reader) closeReader();
    m_showingUserWorks = false;
    m_contentStack->setCurrentIndex(2);
    m_paginationWidget->hide();
    if (page == 1 || !m_currentQuery.isEmpty()) {
        m_workList->clear();
        m_detailWidget->clear();
        m_currentResult = SearchResult();
        statusBar()->showMessage(QStringLiteral("Searching..."));
        m_client->search(m_currentQuery, page, m_currentFilters);
    }
}

void MainWindow::onSearchCompleted(const SearchResult &result)
{
    if (!result.errorMessage.isEmpty()) {
        m_currentResult = SearchResult();
        m_workList->clear();
        m_detailWidget->clear();
        m_paginationWidget->hide();
        statusBar()->showMessage(QStringLiteral("Error: %1").arg(result.errorMessage));
        return;
    }

    m_currentResult = result;
    m_currentPage = result.currentPage;
    populateWorkList();

    if (result.works.isEmpty()) {
        if(!m_readingMode.isEmpty()){statusBar()->showMessage(result.unavailableWorks.isEmpty()?"No works in this AO3 list.":"This list contains unavailable works.");return;}
        statusBar()->showMessage(m_showingUserWorks ? QStringLiteral("You have no works to display.") : QStringLiteral("No works found. Try another search."));
        return;
    }
    statusBar()->showMessage(
        (m_readingMode.isEmpty()?QStringLiteral("Found %1 works (page %2 of %3)"):m_readingMode=="later"?QStringLiteral("Marked for Later: %1 works on page %2 of %3"):QStringLiteral("AO3 history: %1 works on page %2 of %3"))
            .arg(QLocale().toString(result.totalResults))
            .arg(result.currentPage)
            .arg(result.totalPages)
    );
}

void MainWindow::onWorkSelected(int index)
{
    if (index < 0 || index >= m_currentResult.works.size()) {
        m_detailWidget->clear();
        return;
    }
    m_detailsReturn=-1;m_detailWidget->showWork(m_currentResult.works[index]);
    if (mobileUi()) m_contentStack->setCurrentWidget(m_mobileDetails);
}

void MainWindow::onError(const QString &error)
{
    statusBar()->showMessage(QStringLiteral("Error: %1").arg(error));
    if(m_commentsText && m_contentStack->currentWidget()==m_commentsPage && m_commentsText->toPlainText()=="Loading comments...")m_commentsText->setPlainText(error);
}

void MainWindow::onLoginClicked()
{
    if(m_loginManager->isLoggedIn()) {
        QMenu menu(this);menu.addSection(m_loginManager->username());
        auto *profile=menu.addAction("My profile");auto *works=menu.addAction("My works");auto *later=menu.addAction("Marked for Later");auto *history=menu.addAction("AO3 reading history");auto *bookmarks=menu.addAction("My AO3 bookmarks");auto *subscriptions=menu.addAction("Subscriptions");auto *inbox=menu.addAction("Inbox");auto *verify=menu.addAction("Check native sign-in");menu.addSeparator();auto *logout=menu.addAction("Sign out");
        auto *selected=menu.exec(m_loginButton->mapToGlobal(QPoint(0,m_loginButton->height())));
        if(selected==profile)showProfile("/users/"+m_loginManager->username());
        else if(selected==bookmarks)showArchiveList("bookmarks");else if(selected==subscriptions)showArchiveList("subscriptions");else if(selected==inbox)showArchiveList("inbox");else if(selected==later)showAccountReadings(true);else if(selected==history)showArchiveList("history");else if(selected==works)showUserWorks();else if(selected==verify){statusBar()->showMessage("Checking native sign-in...");m_client->checkSession();}else if(selected==logout)onLogout();
        return;
    }
    showLoginView();
}

void MainWindow::onLoginRequested(const QString &login, const QString &password)
{
    m_loginView->setStatus(QStringLiteral("Signing in..."));
    m_client->login(login, password);
}

void MainWindow::onLoginSuccessful(const UserInfo &user)
{
    clearArchiveBookmark();m_reader->saveProgress();m_reader->clear();m_showingUserWorks=false;m_readingMode.clear();m_listingAuthor.clear();m_currentResult={};m_workList->clear();m_detailWidget->clear();m_paginationWidget->hide();m_profileText->clear();m_profileAvatar->clear();m_profileUsername.clear();
    m_commentSending=false;m_commentUncertain=false;m_commentEditor->setEnabled(true);m_commentEditor->hide();m_commentDraft->clear();m_commentEmail->clear();m_commentsWork.clear();
    m_loginView->clearInputs();
    setLoggedInState(true);
    showDashboard();
    statusBar()->showMessage(QStringLiteral("Signed in as %1").arg(user.username));
}

void MainWindow::onLoginFailed(const QString &reason)
{
    m_loginView->setStatus(reason);
}

void MainWindow::onSessionChecked(bool valid, const UserInfo &user)
{
    if(statusBar()->currentMessage()=="Checking native sign-in...")statusBar()->showMessage(valid?"AO3 verified the native sign-in as "+user.username:"AO3 reports that the native session is signed out.");
    if (valid) {
        setLoggedInState(true);
        m_dashboardView->showUserInfo(user);
    } else {
        setLoggedInState(false);
        m_dashboardView->showNoUser();
        if (m_showingUserWorks) {
            m_showingUserWorks = false;
            m_currentResult = SearchResult();
            m_workList->clear();
            m_detailWidget->clear();
            m_paginationWidget->hide();
            showDashboard();
        }
    }
}

void MainWindow::onLogout()
{
    if (m_contentStack->currentWidget() == m_reader) closeReader();
    m_reader->clear();
    m_client->logout();
    clearArchiveBookmark();
    m_commentSending=false;m_commentUncertain=false;m_commentEditor->setEnabled(true);m_commentEditor->hide();m_commentDraft->clear();m_commentEmail->clear();m_commentsWork.clear();
    m_showingUserWorks = false;
    m_currentResult = SearchResult();
    m_workList->clear();
    m_detailWidget->clear();
    m_paginationWidget->hide();
    setLoggedInState(false);
    statusBar()->showMessage(QStringLiteral("Signed out"));
    showDashboard();
}

void MainWindow::goToPage(int page)
{
    if (page < 1 || page > m_currentResult.totalPages)
        return;
    m_currentPage = page;
    m_workList->clear();
    m_detailWidget->clear();
    statusBar()->showMessage(QStringLiteral("Loading page %1...").arg(page));
    if(!m_readingMode.isEmpty())m_client->fetchAccountReadings(m_readingMode=="later",page);
    else if (!m_listingAuthor.isEmpty()) m_client->fetchAuthorWorks(m_listingAuthor, page, m_listingBookmarks);
    else if (m_showingUserWorks) m_client->fetchUserWorks(page);
    else m_client->search(m_currentQuery, page, m_currentFilters);
}

void MainWindow::showAccountReadings(bool later)
{
    if(!m_loginManager->isLoggedIn()){showLoginView();return;}
    if(m_contentStack->currentWidget()==m_reader)closeReader();
    m_listingAuthor.clear();m_readingMode=later?"later":"history";m_showingUserWorks=false;m_pendingReadId.clear();
    m_currentResult={};m_currentPage=1;m_workList->clear();m_detailWidget->clear();m_paginationWidget->hide();m_contentStack->setCurrentIndex(2);
    statusBar()->showMessage(later?"Loading Marked for Later...":"Loading AO3 reading history...");
    m_client->fetchAccountReadings(later);
}

void MainWindow::showUserWorks()
{
    m_readingMode.clear();m_listingAuthor.clear();
    if (m_contentStack->currentWidget() == m_reader) closeReader();
    if (!m_loginManager->isLoggedIn()) {
        showLoginView();
        return;
    }
    m_showingUserWorks = true;
    m_currentPage = 1;
    m_currentResult = SearchResult();
    m_workList->clear();
    m_detailWidget->clear();
    m_paginationWidget->hide();
    m_contentStack->setCurrentIndex(2);
    statusBar()->showMessage(QStringLiteral("Loading your works..."));
    m_client->fetchUserWorks();
}

void MainWindow::showDashboard()
{
    m_dashboardView->refreshLibrary();
    m_pendingReadId.clear();
    if (m_contentStack->currentWidget() == m_reader) closeReader();
    if (m_loginManager->isLoggedIn()) {
        m_dashboardView->showUserInfo(m_loginManager->userInfo());
    } else {
        m_dashboardView->showNoUser();
    }
    m_contentStack->setCurrentIndex(0);
}

void MainWindow::showLoginView()
{
    m_pendingReadId.clear();
    if (m_contentStack->currentWidget() == m_reader) closeReader();
    m_loginView->clearInputs();
    m_contentStack->setCurrentIndex(1);
}

void MainWindow::setLoggedInState(bool loggedIn)
{
    if(!loggedIn){m_listsData={};m_listsKind.clear();m_listsWorkTarget.clear();if(m_listsPage)renderArchiveList(m_listsData);}
    if(!loggedIn && !m_readingMode.isEmpty()){m_readingMode.clear();m_currentResult={};m_workList->clear();m_detailWidget->clear();m_paginationWidget->hide();}

    if(!loggedIn)clearArchiveBookmark();
    m_detailWidget->setKudosStatus(m_client->kudosStatus(m_detailWidget->currentWork().id),!loggedIn);
    m_reader->setKudosStatus(m_client->kudosStatus(m_reader->workId()),!loggedIn);
    m_navWorks->setVisible(mobileUi() || loggedIn);
    m_navLogout->setVisible(!mobileUi() && loggedIn);

    if (loggedIn) {
        QString name = m_loginManager->userInfo().username;
        m_loginButton->setAccessibleName("Account: "+name);
        m_loginButton->setProperty("accountAvatar",true);const int avatarSide=qRound(44*resolvedAppSkin(false).uiScale);m_loginButton->setFixedSize(avatarSide,avatarSide);
        if(m_accountPictureUser!=name){m_loginButton->setIcon({});m_loginButton->setProperty("accountImage",QVariant());m_loginButton->setText(name.left(1).toUpper());m_accountPictureUser=name;m_client->fetchAuthorProfile(name);}
        m_loginButton->setToolTip(name);
        m_loginButton->setEnabled(true);
        m_userGreeting->setText(QString());
        m_userGreeting->hide();
    } else {
        m_accountPictureUser.clear();m_loginButton->setProperty("accountAvatar",false);m_loginButton->setProperty("accountImage",QVariant());m_loginButton->setIcon({});m_loginButton->setMinimumSize(0,32);m_loginButton->setMaximumSize(190,QWIDGETSIZE_MAX);m_loginButton->setAccessibleName("Sign in");
        m_loginButton->setText(mobileUi() ? QStringLiteral("Sign in") : QStringLiteral("Log In"));
        m_loginButton->setEnabled(true);
        m_userGreeting->hide();
    }
}

void MainWindow::populateWorkList()
{
    m_workList->blockSignals(true);
    m_workList->clear();
    m_workList->blockSignals(false);

    m_pageLabel->setText(
        QStringLiteral("Page %1 of %2")
            .arg(m_currentResult.currentPage)
            .arg(m_currentResult.totalPages)
    );
    m_paginationWidget->setVisible(m_currentResult.totalPages > 1);
    m_prevButton->setEnabled(m_currentResult.currentPage > 1);
    m_nextButton->setEnabled(m_currentResult.currentPage < m_currentResult.totalPages);

    for (int i = 0; i < m_currentResult.works.size(); ++i) {
        const Work &w = m_currentResult.works[i];
        auto *item = new QListWidgetItem();
        item->setData(Qt::UserRole, i);
        int h = 80;
        if (!w.fandom.isEmpty()) h += 16;
        if (!w.summary.isEmpty()) h += qMin(60, w.summary.length() / 3);
        h += 16;
        item->setSizeHint(QSize(0, qMax(h, 110)));
        auto *widget = new WorkItemWidget(w);
        m_workList->addItem(item);
        m_workList->setItemWidget(item, widget);
    }

    for(const auto &info:m_currentResult.unavailableWorks){auto *item=new QListWidgetItem(info,m_workList);item->setFlags(Qt::ItemIsEnabled);item->setSizeHint(QSize(0,64));}
    resizeWorkItems();
    if (!mobileUi() && !m_currentResult.works.isEmpty()) {
        m_workList->setCurrentRow(0);
    }
}

void MainWindow::openReader(const Work &work, const QVector<WorkChapter> &chapters)
{
    if (m_contentStack->currentWidget() != m_reader) {
        m_beforeReader = (mobileUi() || m_contentStack->currentWidget() == m_libraryPage) ? m_contentStack->currentIndex() : 2;
        m_readerFiltersVisible = !m_filterDock->isHidden();
        m_readerWasFullscreen = isFullScreen();
    }
    m_reader->openWork(work, chapters);
    m_filterDock->hide(); m_headerBar->hide(); m_navBar->hide(); statusBar()->hide();
    if (m_mobileStatus) m_mobileStatus->hide();
    m_contentStack->setCurrentWidget(m_reader);
    m_reader->setFullscreen(isFullScreen());
}
void MainWindow::closeReader()
{
    m_reader->saveProgress();
    m_reader->cancelTranslation();
    if (!m_readerWasFullscreen && isFullScreen()) showNormal();
    m_contentStack->setCurrentIndex(m_beforeReader);
    m_headerBar->show(); m_navBar->show(); statusBar()->setVisible(!mobileUi());
    if (m_mobileStatus) m_mobileStatus->show();
    m_filterDock->setVisible(!mobileUi() && m_readerFiltersVisible);
}
void MainWindow::toggleReaderFullscreen()
{
    if (m_contentStack->currentWidget() != m_reader) return;
    if (isFullScreen()) showNormal(); else showFullScreen();
    m_reader->setFullscreen(isFullScreen());
}

void MainWindow::layoutDesktopHeader()
{
    if(mobileUi())return;
    auto *grid=qobject_cast<QGridLayout *>(m_headerBar->layout());
    auto *identity=m_headerBar->findChild<QWidget *>("headerIdentity");
    auto *search=m_headerBar->findChild<QWidget *>("headerSearch");
    if(!grid || !identity || !search)return;
    const int wing=qMax(identity->sizeHint().width(),m_loginButton->sizeHint().width());
    const auto margins=grid->contentsMargins();
    const bool stacked=m_headerBar->width()<2*wing+search->minimumSizeHint().width()+margins.left()+margins.right()+2*grid->horizontalSpacing();
    int row,column,rowSpan,columnSpan;
    grid->getItemPosition(grid->indexOf(search),&row,&column,&rowSpan,&columnSpan);
    const int wanted=stacked?1:0;
    if(row!=wanted){grid->removeWidget(search);grid->addWidget(search,wanted,stacked?0:1,1,stacked?3:1,stacked?Qt::AlignHCenter:Qt::Alignment{});grid->removeWidget(m_navBar);grid->addWidget(m_navBar,stacked?2:1,0,1,3);}
    for(int column:{0,2}){
        const int minimum=stacked?0:wing;
        if(grid->columnMinimumWidth(column)!=minimum)grid->setColumnMinimumWidth(column,minimum);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if(watched==m_headerBar && (event->type()==QEvent::Resize || event->type()==QEvent::FontChange))layoutDesktopHeader();
    if (m_workList && watched == m_workList->viewport() && event->type() == QEvent::Resize && !m_workResizeScheduled) {
        m_workResizeScheduled = true;
        QTimer::singleShot(0, this, [this] { resizeWorkItems(); m_workResizeScheduled = false; });
    }
    return QMainWindow::eventFilter(watched, event);
}
void MainWindow::resizeWorkItems()
{
    const int width = qMax(200, m_workList->viewport()->width() - 4);
    for (int i = 0; i < m_workList->count(); ++i) {
        auto *item = m_workList->item(i);
        auto *widget = m_workList->itemWidget(item);
        if (!widget || !widget->layout()) continue;
        widget->setFixedWidth(width);
        auto *layout = widget->layout();
        const int height = layout->hasHeightForWidth() ? layout->totalHeightForWidth(width) : layout->sizeHint().height();
        item->setSizeHint(QSize(width, qMax(140, height + 8)));
    }
}

bool MainWindow::navigateBack()
{
    if(m_listsPage && m_contentStack->currentWidget()==m_listsPage){m_listsWorkTarget.clear();closeArchiveList();return true;}
    if(m_seriesPage && m_contentStack->currentWidget()==m_seriesPage){m_seriesTarget.clear();m_contentStack->setCurrentIndex(m_seriesReturn);return true;}
    m_pendingReadId.clear();
    QGuiApplication::inputMethod()->hide();
#ifdef AO3_WEBENGINE
    if(m_archiveWebsite && m_contentStack->currentWidget()==m_archiveWebsite) {if(!m_archiveWebsite->back())m_contentStack->setCurrentIndex(m_archiveWebsiteReturn);return true;}
#endif
    if (m_contentStack->currentWidget() == m_archiveBookmarkPage) {m_contentStack->setCurrentIndex(m_archiveBookmarkReturn);return true;}
    if (m_contentStack->currentWidget() == m_commentsPage) {m_contentStack->setCurrentIndex(m_commentsReturn);return true;}
    if (m_contentStack->currentWidget() == m_reader && m_reader->closeOptions()) return true;
    if (m_contentStack->currentWidget() == m_reader && isFullScreen()) {
        showNormal(); m_reader->setFullscreen(false); return true;
    }
    if (m_contentStack->currentWidget() == m_reader) { closeReader(); return true; }
    if (m_contentStack->currentWidget() == m_mobileFilters) { m_contentStack->setCurrentIndex(m_beforeFilters); return true; }
    if (m_contentStack->currentWidget() == m_mobileDetails) { m_contentStack->setCurrentIndex(2); return true; }
    if(m_contentStack->currentWidget()==m_profilePage){m_contentStack->setCurrentIndex(m_profileReturn);return true;}
    if(m_contentStack->currentIndex()==2 && m_detailsReturn>=0){const int target=m_detailsReturn;m_detailsReturn=-1;m_contentStack->setCurrentIndex(target);return true;}
    if (m_contentStack->currentIndex() != 0) { showDashboard(); return true; }
    return false;
}
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (mobileUi() && event->key() == Qt::Key_Back) {
        if (QGuiApplication::inputMethod()->isVisible()) {
            QGuiApplication::inputMethod()->hide(); event->accept(); return;
        }
        if (navigateBack()) { event->accept(); return; }
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::showLibrary()
{
    if (m_contentStack->currentWidget() == m_reader) closeReader();
    m_pendingReadId.clear(); refreshLibrary(); m_contentStack->setCurrentWidget(m_libraryPage);
}
void MainWindow::refreshLibrary()
{
    if (!m_libraryList) return;
    m_libraryList->clear();
    for (const auto &value : ReadingLibrary().entries()) {
        const auto entry = value.toObject(); const auto work = ReadingLibrary::work(entry);
        if(m_librarySearch && !m_librarySearch->text().trimmed().isEmpty() && !(work.title+" "+work.author).contains(m_librarySearch->text().trimmed(),Qt::CaseInsensitive)) continue;
        if (m_libraryFilter->currentIndex() == 1 && !entry.value("offline").toBool()) continue;
        if (m_libraryFilter->currentIndex() == 2 && !entry.value("favorite").toBool()) continue;
        auto *item = new QListWidgetItem(QStringLiteral("%1\nby %2 · %3%4").arg(work.title, work.author, entry.value("offline").toBool() ? "Saved offline" : "Online", entry.value("favorite").toBool() ? " · Favorite" : ""), m_libraryList);
        item->setData(Qt::UserRole, entry.toVariantMap()); item->setSizeHint(QSize(0, 72));
    }
    statusBar()->showMessage(QStringLiteral("%1 works in this view").arg(m_libraryList->count()));
}
void MainWindow::openLibrarySelection()
{
    auto *item = m_libraryList->currentItem(); if (!item) return;
    openLibraryEntry(QJsonObject::fromVariantMap(item->data(Qt::UserRole).toMap()));
}
void MainWindow::openLibraryEntry(const QJsonObject &entry)
{
    const auto work = ReadingLibrary::work(entry);
    m_detailsReturn=-1;m_detailWidget->showWork(work);
    const auto chapters = ReadingLibrary().load(work.id);
    if (!chapters.isEmpty()) { openReader(work, chapters); return; }
    m_pendingReadId = work.id; statusBar()->showMessage("Loading fic from AO3..."); m_client->fetchWorkContent(work.id);
}

void MainWindow::showProfile(const QString &authorUrl)
{
    if(m_contentStack->currentWidget() == m_reader) closeReader();
    m_pendingReadId.clear();
    const QStringList path = QUrl(authorUrl).path().split('/', Qt::SkipEmptyParts);
    if(path.size() < 2 || path[0] != "users") { onError("This work has no public author profile."); return; }
    if(m_contentStack->currentWidget()!=m_profilePage)m_profileReturn=m_contentStack->currentIndex();
    m_profileUsername = path[1];m_profileAvatar->clear();m_profileAvatar->setText(m_profileUsername.left(1).toUpper());m_profileAvatar->setAccessibleName("Profile picture for "+m_profileUsername); m_profileText->setPlainText("Loading profile..."); m_contentStack->setCurrentWidget(m_profilePage); m_client->fetchAuthorProfile(m_profileUsername);m_profileSubscription->hide();m_client->entitySubscription("User",m_profileUsername);
}
void MainWindow::showAuthorListing(bool bookmarks)
{
    if(m_profileUsername.isEmpty()) return;
    m_readingMode.clear();m_listingAuthor = m_profileUsername; m_listingBookmarks = bookmarks; m_showingUserWorks = false;
    m_currentResult = {}; m_currentPage = 1; m_workList->clear(); m_detailWidget->clear(); m_paginationWidget->hide(); m_contentStack->setCurrentIndex(2);
    statusBar()->showMessage(m_listingAuthor + (bookmarks ? " · Public AO3 bookmarks" : " · Works")); m_client->fetchAuthorWorks(m_listingAuthor, 1, bookmarks);
}

void MainWindow::showArchiveList(const QString &kind,const QString &id,int page)
{
    if(kind!="series" && !kind.startsWith("search:") && !m_loginManager->isLoggedIn()){showLoginView();return;}
    if(m_contentStack->currentWidget()==m_reader)m_reader->saveProgress();
    if(!m_listsPage){auto *scroll=new QScrollArea(m_contentStack);scroll->setWidgetResizable(true);m_listsPage=scroll;auto *body=new QWidget(scroll);m_listsLayout=new QVBoxLayout(body);m_listsLayout->setAlignment(Qt::AlignTop);scroll->setWidget(body);m_contentStack->addWidget(scroll);}
    if(m_contentStack->currentWidget()!=m_listsPage){m_listsReturn=m_contentStack->currentIndex();m_listsOrigin=m_detailWidget->currentWork();m_listsTrail.clear();}
    m_listsKind=kind;m_listsId=id;m_listsUncertain=false;m_listsWorkTarget.clear();m_contentStack->setCurrentWidget(m_listsPage);m_listsData={};m_listsData.title="Loading...";
    if(kind.startsWith("search:")) {
        const auto mode=kind.mid(7);m_listsData.page=page;m_listsData.pages=qMax(1,page);
        if(page==0){m_client->cancelListing();m_listsData.title=DiscoverySearch::title(mode);}
        renderArchiveList(m_listsData);if(page>0)m_client->searchDiscovery(mode,m_discoveryApplied.value(mode),page);return;
    }
    renderArchiveList(m_listsData);m_client->fetchArchivePage(kind,id,page);if(kind=="series")m_client->entitySubscription("Series",id);
}
void MainWindow::openArchiveTarget(const QString &target)
{
    const QUrl link(target);
    if(link.path()=="/works/search") {
        for(const auto &field:QStringList{"fandom_names","character_names","relationship_names","freeform_names"}) {
            const auto tag=QUrlQuery(link).queryItemValue("work_search["+field+"]");
            if(!tag.isEmpty()){m_searchInput->clear();m_filterPanel->setFilters({{field,{tag}}});performSearch();break;}
        }return;
    }
    const auto work=QRegularExpression("^/works/([0-9]+)$").match(target);const auto series=QRegularExpression("^/series/([0-9]+)$").match(target);const auto comment=QRegularExpression("^/comments/([0-9]+)$").match(target);
    if(work.hasMatch()){m_listsWorkTarget=work.captured(1);m_client->fetchWorkDetails(m_listsWorkTarget);}
    else if(series.hasMatch()){m_listsTrail.append({{m_listsKind,m_listsId},m_listsData.page});showArchiveList("series",series.captured(1));}
    else if(target.startsWith("/users/"))showProfile(target);
    else if(comment.hasMatch())showComments("comment:"+comment.captured(1));
    else if(!target.isEmpty())openArchiveWebsite(QUrl("https://archiveofourown.org"+target));
}
void MainWindow::closeArchiveList()
{
    if(!m_listsTrail.isEmpty()){const auto previous=m_listsTrail.takeLast();showArchiveList(previous.first.first,previous.first.second,previous.second);}else {if(m_listsReturn==2 && !m_listsOrigin.id.isEmpty())m_detailWidget->showWork(m_listsOrigin);m_detailsReturn=-1;m_contentStack->setCurrentIndex(m_listsReturn);}
}
void MainWindow::renderArchiveList(const ArchivePage &page)
{
    while(auto *item=m_listsLayout->takeAt(0)){if(item->widget())item->widget()->deleteLater();delete item;}
    auto *back=new QPushButton("Back",m_listsPage);m_listsLayout->addWidget(back);connect(back,&QPushButton::clicked,this,&MainWindow::closeArchiveList);
    if(m_listsKind.startsWith("search:")) {
        const auto kind=m_listsKind.mid(7);
        auto *heading=new QLabel(DiscoverySearch::title(kind),m_listsPage);m_listsLayout->addWidget(heading);
        auto *edit=new QPushButton(m_discoveryEditing?"Hide search options":"Edit search",m_listsPage);m_listsLayout->addWidget(edit);
        connect(edit,&QPushButton::clicked,this,[this]{m_discoveryEditing=!m_discoveryEditing;renderArchiveList(m_listsData);});
        if(m_discoveryEditing) {
            auto *box=new QWidget(m_listsPage);auto *form=new QFormLayout(box);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);form->setVerticalSpacing(12);
            for(const auto &field:DiscoverySearch::fields(kind)) {
                if(field.choices.isEmpty()) {
                    const auto suggestion=field.key=="fandom" || field.key=="fandoms"?QString("fandom"):field.key=="other_tag_names" || field.key=="other_bookmark_tag_names"?QString("tag"):QString();
                    QLineEdit *input;
                    if(suggestion.isEmpty())input=new QLineEdit(box);
                    else {
                        auto *tags=new TagLineEdit(suggestion,box);input=tags;const auto key="discovery:"+kind+":"+field.key+":"+QUuid::createUuid().toString(QUuid::WithoutBraces);
                        connect(tags,&TagLineEdit::suggestionsRequested,this,[this,key](const QString &type,const QString &term,quint64 revision){m_client->fetchTagSuggestions(key,type,term,revision);});
                        connect(m_client,&AO3Client::tagSuggestionsFetched,tags,[tags,key](const QString &field,quint64 revision,const QStringList &values,const QString &error){if(field==key)tags->showSuggestions(revision,values,error);});
                    }
                    input->setObjectName("discovery_"+field.key);input->setText(m_discoveryDraft.value(kind).value(field.key));form->addRow(field.label,input);
                    connect(input,&QLineEdit::textChanged,this,[this,kind,field](const QString &v){m_discoveryDraft[kind][field.key]=v;});
                    connect(input,&QLineEdit::returnPressed,this,[this,kind]{m_discoveryApplied[kind]=m_discoveryDraft.value(kind);m_discoveryEditing=false;m_listsFilter.clear();showArchiveList("search:"+kind,{},1);});
                } else {
                    auto *input=new QComboBox(box);input->setObjectName("discovery_"+field.key);input->addItem(field.key=="sort_column" && kind=="bookmarks"?"Best match":"Any","");for(const auto &choice:field.choices)input->addItem(choice.first,choice.second);
                    input->setCurrentIndex(qMax(0,input->findData(m_discoveryDraft.value(kind).value(field.key))));form->addRow(field.label,input);
                    connect(input,&QComboBox::currentIndexChanged,this,[this,kind,field,input]{m_discoveryDraft[kind][field.key]=input->currentData().toString();});
                }
            }
            m_listsLayout->addWidget(box);
            auto *search=new QPushButton("Search",m_listsPage);search->setObjectName("discoverySearch");m_listsLayout->addWidget(search);
            connect(search,&QPushButton::clicked,this,[this,kind]{m_discoveryApplied[kind]=m_discoveryDraft.value(kind);m_discoveryEditing=false;m_listsFilter.clear();showArchiveList("search:"+kind,{},1);});
            auto *reset=new QPushButton("Reset options",m_listsPage);m_listsLayout->addWidget(reset);connect(reset,&QPushButton::clicked,this,[this,kind]{m_discoveryDraft[kind].clear();renderArchiveList(m_listsData);});
        }
        if(page.page==0){m_listsLayout->addStretch();return;}
    }
    auto *refresh=new QPushButton("Refresh",m_listsPage);m_listsLayout->addWidget(refresh);connect(refresh,&QPushButton::clicked,this,[this,page]{showArchiveList(m_listsKind,m_listsId,page.page);});
    auto text=[this](const QString &value){auto *label=new QLabel(value,m_listsPage);label->setTextFormat(Qt::PlainText);label->setWordWrap(true);label->setTextInteractionFlags(Qt::TextSelectableByMouse);m_listsLayout->addWidget(label);};if(!m_listsKind.startsWith("search:") || !page.error.isEmpty() || page.title=="Loading...")text(page.error.isEmpty()?page.title:page.error);
    auto *filter=new QLineEdit(m_listsPage);filter->setPlaceholderText("Filter this page");filter->setText(m_listsFilter);m_listsLayout->addWidget(filter);
    connect(filter,&QLineEdit::textChanged,this,[this](const QString &value){m_listsFilter=value;});
    if(m_listsKind=="history") {
        auto *clear=new QPushButton("Clear entire AO3 history",m_listsPage);clear->setEnabled(!m_listsUncertain && page.error.isEmpty() && !page.entries.isEmpty());m_listsLayout->addWidget(clear);
        connect(clear,&QPushButton::clicked,this,[this]{if(QMessageBox::question(this,"Clear AO3 history?","Permanently erase your AO3 reading history and Marked for Later list?")!=QMessageBox::Yes)return;m_listsUncertain=true;renderArchiveList(m_listsData);m_client->performArchiveAction("history",{},1,{},"Clear history");});
    }
    if(m_listsKind=="inbox") {
        auto *count=new QLabel(QString::number(m_listsSelected.size())+" selected",m_listsPage);m_listsLayout->addWidget(count);
        auto *select=new QPushButton("Select visible",m_listsPage);auto *clear=new QPushButton("Clear selection",m_listsPage);m_listsLayout->addWidget(select);m_listsLayout->addWidget(clear);
        connect(select,&QPushButton::clicked,this,[this,page,filter]{for(const auto &entry:page.entries)if(!entry.selection.isEmpty() && (entry.title+"\n"+entry.body).contains(filter->text(),Qt::CaseInsensitive))m_listsSelected.insert(entry.id);renderArchiveList(m_listsData);});
        connect(clear,&QPushButton::clicked,this,[this]{m_listsSelected.clear();renderArchiveList(m_listsData);});
        for(const auto &action:QStringList{"Mark read","Mark unread","Delete from inbox"}) {
            auto *button=new QPushButton(action+" selected",m_listsPage);button->setEnabled(!m_listsUncertain && !m_listsSelected.isEmpty());m_listsLayout->addWidget(button);
            connect(button,&QPushButton::clicked,this,[this,page,action]{QStringList selected;for(const auto &entry:page.entries)if(m_listsSelected.contains(entry.id) && entry.actions.contains(action))selected.append(entry.id);if(selected.isEmpty())return;if(action=="Delete from inbox" && QMessageBox::question(this,"Delete from inbox?",QString("Remove %1 selected comments from your inbox? The comments themselves remain on AO3.").arg(selected.size()))!=QMessageBox::Yes)return;m_listsUncertain=true;renderArchiveList(m_listsData);m_client->performArchiveAction("inbox",{},page.page,selected.first(),action,selected);});
        }
    }
    if(!page.description.isEmpty())text(page.description);
    if(page.entries.isEmpty() && page.error.isEmpty() && page.title!="Loading...")text("No entries.");
    if(m_listsKind=="series" && page.canSubscribe){auto *subscribe=new QPushButton(page.subscribed?"Unsubscribe":"Subscribe to series",m_listsPage);m_listsLayout->addWidget(subscribe);connect(subscribe,&QPushButton::clicked,this,[this,page,subscribe]{subscribe->setEnabled(false);m_client->entitySubscription("Series",m_listsId,!page.subscribed);});}
    for(const auto &entry:page.entries){
        const int start=m_listsLayout->count();
        if(m_listsKind=="inbox" && !entry.selection.isEmpty()){auto *choose=new QCheckBox("Select comment",m_listsPage);choose->setChecked(m_listsSelected.contains(entry.id));m_listsLayout->addWidget(choose);connect(choose,&QCheckBox::toggled,this,[this,entry](bool checked){if(checked)m_listsSelected.insert(entry.id);else m_listsSelected.remove(entry.id);QTimer::singleShot(0,this,[this]{renderArchiveList(m_listsData);});});}
        text(entry.title);if(entry.unread)text("Unread");if(!entry.body.isEmpty() && entry.body!=entry.title)text(entry.body);
        if(!entry.target.isEmpty()){auto *open=new QPushButton(entry.target.startsWith("/comments/")?"Open thread":entry.target.startsWith("/bookmarks/")?"Open on AO3":"Open",m_listsPage);m_listsLayout->addWidget(open);connect(open,&QPushButton::clicked,this,[this,entry]{openArchiveTarget(entry.target);});}
        for(const auto &action:entry.actions){auto *button=new QPushButton(action,m_listsPage);button->setEnabled(!m_listsUncertain);m_listsLayout->addWidget(button);connect(button,&QPushButton::clicked,this,[this,entry,action,page]{if((action=="Unsubscribe" || action=="Delete from inbox" || action=="Delete from history") && QMessageBox::question(this,action+"?",action=="Delete from history"?entry.title+"\n\nRemove this entry from your AO3 history and Marked for Later list?":entry.title)!=QMessageBox::Yes)return;m_listsUncertain=true;renderArchiveList(m_listsData);m_client->performArchiveAction(m_listsKind,m_listsId,page.page,entry.id,action);});}
        QList<QWidget *> entryWidgets;for(int i=start;i<m_listsLayout->count();++i)if(auto *widget=m_listsLayout->itemAt(i)->widget())entryWidgets.append(widget);
        auto applyFilter=[entry,entryWidgets](const QString &value){const bool visible=(entry.title+"\n"+entry.body).contains(value,Qt::CaseInsensitive);for(auto *widget:entryWidgets)widget->setVisible(visible);};applyFilter(filter->text());connect(filter,&QLineEdit::textChanged,this,applyFilter);
    }
    for(int delta:{-1,1}){auto *button=new QPushButton(delta<0?"Previous":"Next",m_listsPage);button->setEnabled(delta<0?page.page>1:page.page<page.pages);m_listsLayout->addWidget(button);connect(button,&QPushButton::clicked,this,[this,page,delta]{showArchiveList(m_listsKind,m_listsId,page.page+delta);});}text(QString::number(page.page)+" / "+QString::number(page.pages));m_listsLayout->addStretch();
}

void MainWindow::showSeries(const QString &id)
{
    if(m_contentStack->currentWidget()==m_reader)m_reader->saveProgress();
    if(!m_seriesPage) {
        auto *scroll=new QScrollArea(m_contentStack);scroll->setWidgetResizable(true);m_seriesPage=scroll;
        auto *body=new QWidget(scroll);m_seriesLayout=new QVBoxLayout(body);m_seriesLayout->setAlignment(Qt::AlignTop);scroll->setWidget(body);
        auto *back=new QPushButton("Back",body);m_seriesLayout->addWidget(back);
        connect(back,&QPushButton::clicked,this,[this]{m_seriesTarget.clear();m_contentStack->setCurrentIndex(m_seriesReturn);});
        m_contentStack->addWidget(m_seriesPage);
    }
    while(auto *item=m_seriesLayout->takeAt(1)){if(item->widget())item->widget()->deleteLater();delete item;}
    m_seriesLayout->addWidget(new QLabel("Loading series...",m_seriesPage));
    m_seriesReturn=m_contentStack->currentIndex();m_seriesWork=id;m_seriesTarget.clear();
    m_contentStack->setCurrentWidget(m_seriesPage);m_client->fetchWorkSeries(id);
}

void MainWindow::showComments(const QString &workId,int page) {
    if(m_contentStack->currentWidget()!=m_commentsPage)m_commentsReturn=m_contentStack->currentIndex();
    if(m_commentSending)return;
    if(workId!=m_commentsWork) {m_commentDraft->clear();m_commentEditor->hide();m_commentUncertain=false;}
    m_commentsWork=workId;m_commentsCurrent=qMax(1,page);
    m_commentsPrevious->setEnabled(false);m_commentsNext->setEnabled(false);m_commentsText->setPlainText("Loading comments...");
    m_contentStack->setCurrentWidget(m_commentsPage);m_client->fetchComments(workId,m_commentsCurrent);
}

void MainWindow::manageComment(const QString &id,const QString &action)
{
    if(m_commentSending || m_commentUncertain)return;
    if(action=="Edit"){m_commentContext=id;m_client->fetchCommentEdit(m_commentsWork,id);return;}
    if((action=="Delete" || action=="Mark as spam") && QMessageBox::question(this,action+" comment?",action=="Delete"?"Permanently delete this comment?":"Mark this comment as spam?")!=QMessageBox::Yes)return;
    m_commentDeletion=action=="Delete"?id:QString();m_commentSending=true;m_commentNew->setEnabled(false);m_client->manageComment(m_commentsWork,id,action);
}
void MainWindow::composeComment(const QString &replyId) {
    if(m_commentSending || m_commentUncertain)return;
    m_commentContext=replyId.isEmpty() && m_commentsWork.startsWith("comment:")?m_commentsWork.mid(8):replyId;m_client->fetchCommentForm(m_commentsWork,replyId);
}

void MainWindow::clearArchiveBookmark() {
    m_archiveBookmark={};m_archiveBookmarkWork.clear();m_archiveBookmarkSending=false;m_archiveBookmarkUncertain=false;
    m_archiveBookmarkNotes->clear();m_archiveBookmarkTags->clear();m_archiveBookmarkCollections->clear();m_archiveBookmarkPseud->clear();m_archiveBookmarkPrivate->setChecked(false);m_archiveBookmarkRec->setChecked(false);
}
void MainWindow::editArchiveBookmark(const QString &id,bool reload) {
    if(!m_loginManager->isLoggedIn() || m_archiveBookmarkSending)return;
    if(m_contentStack->currentWidget()!=m_archiveBookmarkPage)m_archiveBookmarkReturn=m_contentStack->currentIndex();
    if(!reload && m_archiveBookmarkWork==id && !m_archiveBookmark.path.isEmpty()){m_contentStack->setCurrentWidget(m_archiveBookmarkPage);return;}
    clearArchiveBookmark();m_archiveBookmarkWork=id;m_archiveBookmarkStatus->setText("Loading AO3 bookmark…");m_archiveBookmarkFields->setEnabled(false);m_archiveBookmarkSave->setEnabled(false);m_archiveBookmarkDelete->setEnabled(false);m_archiveBookmarkDelete->hide();m_archiveBookmarkReload->setEnabled(true);
    m_contentStack->setCurrentWidget(m_archiveBookmarkPage);m_client->fetchArchiveBookmark(id);
}
void MainWindow::submitArchiveBookmark(bool deleting) {
    if(m_archiveBookmarkSending || m_archiveBookmarkUncertain || !m_archiveBookmark.error.isEmpty() || m_archiveBookmark.path.isEmpty())return;
    if(m_client->workAccountPending(m_archiveBookmark.workId)){m_archiveBookmarkStatus->setText("Another AO3 account request is still running. Try again when it finishes.");return;}
    auto draft=m_archiveBookmark;draft.notes=m_archiveBookmarkNotes->toPlainText();draft.tags=m_archiveBookmarkTags->text();draft.collections=m_archiveBookmarkCollections->text();draft.pseud=m_archiveBookmarkPseud->currentData().toString();draft.privateBookmark=m_archiveBookmarkPrivate->isChecked();draft.recommendation=m_archiveBookmarkRec->isChecked();
    m_archiveBookmarkSending=true;m_archiveBookmarkFields->setEnabled(false);m_archiveBookmarkSave->setEnabled(false);m_archiveBookmarkDelete->setEnabled(false);m_archiveBookmarkReload->setEnabled(false);m_archiveBookmarkStatus->setText(deleting?"Deleting AO3 bookmark…":"Saving AO3 bookmark…");m_client->saveArchiveBookmark(draft,deleting);
}

void MainWindow::openArchiveWebsite(const QUrl &url) {
#ifdef AO3_WEBENGINE
    if(!archiveWebUrl(url)){statusBar()->showMessage("This address is outside AO3.");return;}
    if(!m_archiveWebsite) {
        m_archiveWebsite=new ArchiveWeb(m_contentStack);m_contentStack->addWidget(m_archiveWebsite);
        connect(m_archiveWebsite,&ArchiveWeb::closeRequested,this,[this]{m_contentStack->setCurrentIndex(m_archiveWebsiteReturn);});
        connect(m_archiveWebsite,&ArchiveWeb::sessionRequested,this,[this](const QList<QNetworkCookie> &cookies,const QString &agent,const QString &user){statusBar()->showMessage("Checking website sign-in for the native reader…");m_client->useWebsiteSession(cookies,agent,user);});
        connect(m_archiveWebsite,&ArchiveWeb::message,this,[this](const QString &message){statusBar()->showMessage(message);});
    }
    if(m_contentStack->currentWidget()!=m_archiveWebsite)m_archiveWebsiteReturn=m_contentStack->currentIndex();
    m_contentStack->setCurrentWidget(m_archiveWebsite);m_archiveWebsite->open(url);
#else
    QDesktopServices::openUrl(url);
#endif
}
