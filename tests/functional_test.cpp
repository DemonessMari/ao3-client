#include "readerspeech.h"
#include "accountreadings.h"
#include "profileavatar.h"
#include "readerfonts.h"
#include <QBuffer>
#include "appskin.h"
#include <QtTest>
#include "bookview.h"
#include <QTextBlock>
#include <QTextDocument>
#include <QFontInfo>
#include <QNetworkReply>
#include <QTimer>
#include <QTemporaryDir>
#include <QFileDialog>
#include <QTextBrowser>
#include <QSignalSpy>
#include <QUrlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QTcpServer>
#include <QTcpSocket>
#include <memory>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QCompleter>
#include <QAbstractItemView>
#include "taglineedit.h"
#include "workcontent.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QCheckBox>
#include <QScopeGuard>
#include <QScrollBar>
#include <QSpinBox>
#include <QInputDialog>
#include "mainwindow.h"
#include "archiveweb.h"
#include "readinglibrary.h"
#include "librarybackup.h"
#include "serviceurl.h"

class FixtureReply : public QNetworkReply {
public:
    FixtureReply(const QNetworkRequest &request, const QByteArray &data, QObject *parent, bool fail = false, int status = 200)
        : QNetworkReply(parent), data(data) {
        setRequest(request); setUrl(request.url());
        if (fail) setError(QNetworkReply::TimeoutError, QStringLiteral("Fixture timeout"));
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, [this] { setFinished(true); emit readyRead(); emit finished(); });
    }
    void abort() override {}
    qint64 bytesAvailable() const override { return data.size() - offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *buffer, qint64 size) override {
        const qint64 count = qMin(size, data.size() - offset);
        if (!count) return -1;
        memcpy(buffer, data.constData() + offset, count); offset += count; return count;
    }
private:
    QByteArray data; qint64 offset = 0;
};

class FixtureNetwork : public QNetworkAccessManager {
public:
    QList<QUrl> requests;
    QList<Operation> operations;
    QList<QByteArray> bodies;
    QByteArray page;
    QList<QByteArray> pages;
    bool fail = false;
    int status = 200;
protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *body) override {
        requests.append(request.url());
        operations.append(operation);
        bodies.append(body ? body->readAll() : QByteArray());
        return new FixtureReply(request, pages.isEmpty() ? page : pages.takeFirst(), this, fail, status);
    }
};

class FunctionalTest : public QObject {
    Q_OBJECT
    FixtureNetwork *installNetwork(MainWindow &window) {
        auto *network = new FixtureNetwork;
        auto *original = window.m_client->m_manager;
        window.m_loginManager->setParent(nullptr);
        original->setCookieJar(new QNetworkCookieJar(original));
        network->setParent(window.m_client);
        network->setCookieJar(window.m_loginManager);
        window.m_client->m_manager = network;
        delete original;
        return network;
    }
private slots:
    void commentManagementUsesFreshPermissionForms() {
        QFile file(QStringLiteral(AO3_FIXTURE_DIR "/comment-managed.html"));QVERIFY(file.open(QIODevice::ReadOnly));const auto html=QString::fromUtf8(file.readAll());
        const auto controls=commentControls(ArchivePages::Document(html),"55");QVERIFY(controls.contains("Edit"));QVERIFY(controls.contains("Delete"));QCOMPARE(controls.value("Freeze thread").fields.value("authenticity_token"),QString("freeze+token"));
        QVERIFY(commentControls(ArchivePages::Document("<div class='userstuff'>"+html+"</div>"),"55").isEmpty());
        QFile edit(QStringLiteral(AO3_FIXTURE_DIR "/comment-edit.html"));QVERIFY(edit.open(QIODevice::ReadOnly));const auto form=parseCommentForm(QString::fromUtf8(edit.readAll()));QVERIFY(form.error.isEmpty());QVERIFY(form.editing);QCOMPARE(form.initialText,QString("First & second\n\n<em>Keep this HTML</em>"));
    }
    void nativeArchiveListsUseOwnedFormsAndSeriesOrder() {
        auto fixture=[](const QString &name){QFile file(QStringLiteral(AO3_FIXTURE_DIR "/archive-")+name+".html");if(!file.open(QIODevice::ReadOnly))return QString();return QString::fromUtf8(file.readAll());};
        const auto series=ArchivePages::parse(fixture("series"),"series",{},"42",1);QVERIFY(series.error.isEmpty());QCOMPARE(series.entries.size(),2);QCOMPARE(series.entries[0].target,QString("/works/20"));QCOMPARE(series.entries[1].target,QString("/works/10"));
        const auto html=fixture("subscriptions");const auto subscriptions=ArchivePages::parse(html,"subscriptions","Writer",{},1);QVERIFY(subscriptions.error.isEmpty());QCOMPARE(subscriptions.entries.size(),3);QCOMPARE(subscriptions.pages,2);
        const auto action=ArchivePages::action(html,"subscriptions","Writer",{},1,"4","Unsubscribe");QVERIFY(action.error.isEmpty());QCOMPARE(action.fields.value("authenticity_token"),QString("fresh+&token"));QCOMPARE(action.path,QString("/users/Writer/subscriptions/4"));
        QVERIFY(!ArchivePages::parse(html,"subscriptions","Other",{},1).error.isEmpty());
        const auto inbox=ArchivePages::parse(fixture("inbox"),"inbox","Writer",{},1);QVERIFY(inbox.error.isEmpty());QCOMPARE(inbox.entries.size(),1);QVERIFY(inbox.entries.first().unread);
        const auto read=ArchivePages::action(fixture("inbox"),"inbox","Writer",{},1,"55","Mark read");QVERIFY(read.error.isEmpty());QCOMPARE(read.fields.value("inbox_comments[]"),QString("77"));
        const auto spoof=QString(fixture("inbox")).replace("<p>A comment &amp; a reply.</p>","<input name='inbox_comments[]' value='999'><input type='hidden' name='authenticity_token' value='fake-token'>");const auto safe=ArchivePages::action(spoof,"inbox","Writer",{},1,"55","Mark read");QVERIFY(safe.error.isEmpty());QCOMPARE(safe.fields.value("inbox_comments[]"),QString("77"));QCOMPARE(safe.fields.value("authenticity_token"),QString("inbox+token"));
        QVERIFY(!ArchivePages::action(QString(fixture("inbox")).replace("name=\"read\"","name=\"read\" disabled"),"inbox","Writer",{},1,"55","Mark read").error.isEmpty());
        MainWindow window;auto *network=installNetwork(window);QTest::qWait(30);window.m_loginManager->setUserInfo({"Writer","https://archiveofourown.org/users/Writer",true});network->page=html.toUtf8();window.showArchiveList("subscriptions");QTRY_COMPARE(window.m_listsData.entries.size(),3);
        const QString empty="<nav id='greeting'><a href='/users/Writer'>Writer</a><a href='/users/logout'>Log out</a></nav><div id='main' class='subscriptions-index'></div>";
        network->pages={html.toUtf8(),empty.toUtf8()};QSignalSpy finished(window.m_client,&AO3Client::archiveActionFinished);window.m_client->performArchiveAction("subscriptions",{},1,"4","Unsubscribe");QTRY_COMPARE(finished.size(),1);QVERIFY(!finished.last()[1].toBool());QVERIFY(network->bodies.last().contains("fresh%2B%26token"));QVERIFY(window.m_listsData.entries.isEmpty());
        window.setLoggedInState(false);QVERIFY(window.m_listsData.entries.isEmpty());QVERIFY(window.m_listsKind.isEmpty());
    }
    void bulkInboxUsesFreshRecordIdsAndChecksEveryResult() {
        QFile file(QStringLiteral(AO3_FIXTURE_DIR "/archive-inbox.html"));QVERIFY(file.open(QIODevice::ReadOnly));QString html=QString::fromUtf8(file.readAll());
        const QRegularExpression itemRx("<li id=\"feedback_comment_55\"[\\s\\S]*?</li>");const auto match=itemRx.match(html);QVERIFY(match.hasMatch());const auto item=match.captured();html.replace(item,item+QString(item).replace("feedback_comment_55","feedback_comment_56").replace("value=\"77\"","value=\"78\""));
        MainWindow window;auto *network=installNetwork(window);QTest::qWait(30);window.m_loginManager->setUserInfo({"Writer","https://archiveofourown.org/users/Writer",true});
        network->pages={html.toUtf8(),QString(html).replace("class=\"unread comment\"","class=\"comment\"").toUtf8()};QSignalSpy finished(window.m_client,&AO3Client::archiveActionFinished);
        window.m_client->performArchiveAction("inbox",{},1,"55","Mark read",{"55","56"});QTRY_COMPARE(finished.size(),1);QVERIFY(!finished.last()[1].toBool());
        const auto posted=network->bodies.last();QVERIFY(posted.contains("inbox_comments%5B%5D=77"));QVERIFY(posted.contains("inbox_comments%5B%5D=78"));QVERIFY(!posted.contains("inbox_comments%5B%5D=55"));
    }
    void workActionsWrapInsideNarrowDetails() {
        WorkDetailWidget detail;Work work;work.id="123";work.title="A story";work.author="Writer";detail.showWork(work);detail.resize(380,850);detail.show();QTest::qWait(30);
        auto *actions=detail.findChild<QWidget *>("detailActions");QVERIFY(actions);QVERIFY(actions->layout()->hasHeightForWidth());
        int top=-1;bool wrapped=false;
        for(auto *button:actions->findChildren<QPushButton *>(QString(),Qt::FindDirectChildrenOnly)) {
            QVERIFY(button->geometry().right()<actions->width());QVERIFY(button->geometry().bottom()<actions->height());
            if(top<0)top=button->y();else if(button->y()>top)wrapped=true;
        }
        QVERIFY(wrapped);
    }
    void accountReadingsVerifyIdentityKeepPaginationAndShowDeletedWorks() {
        QFile file(QStringLiteral(AO3_FIXTURE_DIR "/account-readings.html"));QVERIFY(file.open(QIODevice::ReadOnly));const auto html=QString::fromUtf8(file.readAll());
        const auto doc=accountReadingDocument(html,"Writer",1);QVERIFY(doc.error.isEmpty());QCOMPARE(doc.pages,2);QCOMPARE(doc.unavailable.size(),1);QVERIFY(doc.activity.value("123").contains("Update available"));
        QVERIFY(!accountReadingDocument(html,"Other",1).error.isEmpty());
        const QString header="<nav id='greeting'><a href='/users/Writer'>Writer</a><a href='/users/logout'>Log out</a></nav>";
        QVERIFY(accountReadingDocument(header+"<div id='main' class='readings-index'></div>","Writer",1).error.isEmpty());
        QVERIFY(!accountReadingDocument(header+"<div id='main' class='preferences-edit'></div>","Writer",1).error.isEmpty());
        MainWindow window;auto *network=installNetwork(window);QTest::qWait(30);window.m_loginManager->setUserInfo({"Writer","https://archiveofourown.org/users/Writer",true});network->page=html.toUtf8();
        window.showAccountReadings(true);QTRY_COMPARE(window.m_currentResult.works.size(),1);QCOMPARE(window.m_workList->count(),2);QCOMPARE(window.m_detailWidget->currentWork().id,QString("123"));QVERIFY(window.m_currentResult.works.first().readingInfo.contains("Visited twice"));
        window.m_searchInput->clear();window.m_filterPanel->setFilters({});window.performSearch();QCOMPARE(window.m_readingMode,QString("later"));
        QCOMPARE(QUrlQuery(network->requests.last()).queryItemValue("show"),QString("to-read"));window.goToPage(2);QTRY_COMPARE(window.m_currentResult.currentPage,2);QCOMPARE(QUrlQuery(network->requests.last()).queryItemValue("show"),QString("to-read"));
        window.showAccountReadings(false);QTRY_COMPARE(window.m_readingMode,QString("history"));QTRY_COMPARE(window.m_currentResult.works.size(),1);QVERIFY(!QUrlQuery(network->requests.last()).hasQueryItem("show"));
        if(qEnvironmentVariableIsSet("AO3_CAPTURE_READINGS")){window.resize(1100,850);window.show();QTest::qWait(40);window.grab().save("/tmp/ao3-readings-rc13.png");}
        window.setLoggedInState(false);QVERIFY(window.m_currentResult.works.isEmpty());QCOMPARE(window.m_workList->count(),0);
    }
    void seriesNavigationUsesVisibleNeighborsAndNativeWorkDetails() {
        QFile file(QStringLiteral(AO3_FIXTURE_DIR "/work-series.html"));QVERIFY(file.open(QIODevice::ReadOnly));const auto html=QString::fromUtf8(file.readAll());
        const auto parsed=parseWorkSeries(html);QVERIFY(parsed.error.isEmpty());QCOMPARE(parsed.entries.size(),2);
        const auto first=parsed.entries[0];QCOMPARE(first.id,QString("42"));QCOMPARE(first.title,QString("First & Second"));QCOMPARE(first.previous,QString("101"));QCOMPARE(first.next,QString("303"));
        QVERIFY(parsed.entries[1].previous.isEmpty());QVERIFY(parsed.entries[1].next.isEmpty());
        QVERIFY(parseWorkSeries("<dl class='work meta'></dl>").error.isEmpty());QVERIFY(!parseWorkSeries("<h1>Sign in</h1>").error.isEmpty());
        QVERIFY(!parseWorkSeries("<div class='userstuff'><dl class='work meta'></dl></div>").error.isEmpty());
        QVERIFY(archiveNumericId("https://user@archiveofourown.org/works/3","works").isEmpty());QVERIFY(archiveNumericId("/works/3/chapters/4","works").isEmpty());
        MainWindow window;auto *network=installNetwork(window);QTest::qWait(30);network->page=html.toUtf8();
        window.m_contentStack->setCurrentIndex(2);window.showSeries("202");
        QTRY_COMPARE(window.m_seriesPage->findChildren<QPushButton *>().size(),7);
        QCOMPARE(network->requests.last().path(),QString("/works/202"));
        QPushButton *next=nullptr;for(auto *button:window.m_seriesPage->findChildren<QPushButton *>())if(button->text()=="Next work" && button->isEnabled())next=button;
        if(qEnvironmentVariableIsSet("AO3_CAPTURE_SERIES")){window.resize(1100,850);window.show();QTest::qWait(40);window.grab().save("/tmp/ao3-series-rc12.png");}
        QVERIFY(next);next->click();QTRY_COMPARE(window.m_detailWidget->currentWork().id,QString("303"));QCOMPARE(window.m_contentStack->currentIndex(),2);
        QCOMPARE(network->requests.last().path(),QString("/works/303"));QCOMPARE(window.m_detailWidget->currentWork().title,QString("A work & its sequel"));QCOMPARE(window.m_detailWidget->currentWork().author,QString("Writer"));
        window.showSeries("202");window.navigateBack();QTest::qWait(30);QCOMPARE(window.m_contentStack->currentIndex(),2);QCOMPARE(window.m_detailWidget->currentWork().id,QString("303"));
    }
    void headerIdentityAndSearchCentering() {
        const auto originalPalette=QApplication::palette();auto restore=qScopeGuard([&]{QApplication::setPalette(originalPalette);});applyAppSkin();
        MainWindow window;installNetwork(window);window.show();auto *name=window.findChild<QLabel *>("appName");auto *search=window.findChild<QWidget *>("headerSearch");QVERIFY(name);QVERIFY(search);QCOMPARE(name->text(),QString("Archive of Our Own"));
        for(int width:{1440,1100,900}){window.resize(width,850);QTest::qWait(30);const auto center=search->mapTo(window.m_headerBar,search->rect().center());QVERIFY(qAbs(center.x()-window.m_headerBar->rect().center().x())<=12);QVERIFY(name->isVisible());}
        if(qEnvironmentVariableIsSet("AO3_CAPTURE_HEADER")){window.resize(1440,850);QTest::qWait(40);window.grab().save("/tmp/ao3-header-rc3.png");}
    }
    void profilePicturesAreScopedBoundedAndDisplayed() {
        QFile file(QStringLiteral(AO3_FIXTURE_DIR "/profile-avatar.html"));QVERIFY(file.open(QIODevice::ReadOnly));const auto html=QString::fromUtf8(file.readAll());const auto avatar=profileAvatar(html);QCOMPARE(avatar.url,QString("https://archiveofourown.org/rails/active_storage/blobs/redirect/abc/avatar.png"));QCOMPARE(avatar.alt,QString("A green & blue icon"));
        auto spoof=html;spoof.remove(QRegularExpression("<div class=\"primary header module\">.*?</div>"));QVERIFY(profileAvatar(spoof).url.isEmpty());QVERIFY(!avatarUrlAllowed(QUrl("https://example.com/avatar.png")));QVERIFY(!avatarUrlAllowed(QUrl("https://user@archiveofourown.org/system/pseud/icons/a.png")));QVERIFY(!avatarUrlAllowed(QUrl("https://archiveofourown.org/works/1")));QVERIFY(avatarUrlAllowed(QUrl("https://bucket.s3.amazonaws.com/a"),true));
        QImage source(40,20,QImage::Format_ARGB32);source.fill(Qt::green);QByteArray bytes;QBuffer buffer(&bytes);QVERIFY(buffer.open(QIODevice::WriteOnly));QVERIFY(source.save(&buffer,"PNG"));QVERIFY(!decodeProfileAvatar(bytes).isNull());QVERIFY(decodeProfileAvatar(QByteArray(2*1024*1024+1,'x')).isNull());QVERIFY(decodeProfileAvatar("<html>Unavailable</html>").isNull());
        MainWindow window;auto *network=installNetwork(window);QTest::qWait(30);network->pages={html.toUtf8(),bytes};window.showProfile("/users/Writer");QTRY_VERIFY(!window.m_profileAvatar->pixmap().isNull());QCOMPARE(window.m_profileAvatar->accessibleName(),QString("A green & blue icon"));QVERIFY(window.m_profileText->toPlainText().contains("Biography"));
        QSignalSpy received(window.m_client,&AO3Client::profileAvatarFetched);const auto count=network->requests.size();window.m_client->fetchProfileAvatar("Writer",QUrl("https://example.com/avatar.png"));QCOMPARE(received.size(),1);QCOMPARE(network->requests.size(),count);
        received.clear();network->pages={html.toUtf8(),QByteArray("Not an image")};window.showProfile("/users/Other");QTRY_COMPARE(received.size(),1);QVERIFY(window.m_profileAvatar->pixmap().isNull());QCOMPARE(window.m_profileAvatar->text(),QString("O"));QVERIFY(window.m_profileText->toPlainText().contains("Biography"));
    }
    void appSkinFormatAndContrast() {
        QFile file(QStringLiteral(AO3_FIXTURE_DIR "/app-skin.json"));QVERIFY(file.open(QIODevice::ReadOnly));const auto bytes=file.readAll();AppSkin skin;QString error;QVERIFY2(AppSkin::parse(bytes,skin,error),qPrintable(error));QCOMPARE(skin.name,QString("Blue"));AppSkin copy;QVERIFY(AppSkin::parse(skin.json(),copy,error));QCOMPARE(copy.accent,skin.accent);
        for(int i=0;i<3;++i)QVERIFY2(AppSkin::parse(AppSkin::preset(i).json(),copy,error),qPrintable(error));
        auto object=QJsonDocument::fromJson(bytes).object();for(const auto &pair:QVector<QPair<QString,QJsonValue>>{{"text","#ffffff"},{"onAccent","#174a8b"},{"accent","#eeeeee"},{"surface","#00ffffff"},{"version",2},{"css","body{display:none}"}}){auto invalid=object;invalid[pair.first]=pair.second;QVERIFY(!AppSkin::parse(QJsonDocument(invalid).toJson(),copy,error));}
        QVERIFY(!AppSkin::parse(QByteArray(16385,'x'),copy,error));
    }
    void nativeSkinLayoutTokensAreBoundedAndReversible() {
        AppSkin skin=AppSkin::preset(1),parsed;QString error;skin.version=2;skin.uiScale=1.25;skin.cornerRadius=0;
        QVERIFY(AppSkin::parse(skin.json(),parsed,error));QCOMPARE(parsed.uiScale,1.25);QCOMPARE(parsed.cornerRadius,0);QCOMPARE(parsed.version,2);
        for(double invalid:{.5,2.0}){auto bad=skin;bad.uiScale=invalid;QVERIFY(!AppSkin::parse(bad.json(),parsed,error));}
        for(int invalid:{-1,25}){auto bad=skin;bad.cornerRadius=invalid;QVERIFY(!AppSkin::parse(bad.json(),parsed,error));}
        const auto prior=QSettings().value("appearance/skin");auto restore=qScopeGuard([&]{if(prior.isValid())QSettings().setValue("appearance/skin",prior);else QSettings().remove("appearance/skin");applyAppSkin();});
        QPushButton button("Native button");button.setStyleSheet("padding: 8px;");const auto base=button.font().pointSizeF();
        QSettings().setValue("appearance/skin",skin.json());applyAppSkin();QVERIFY(button.styleSheet().isEmpty());QVERIFY(qAbs(button.font().pointSizeF()-base*1.25)<.1);
        applyAppSkin();QVERIFY(qAbs(button.font().pointSizeF()-base*1.25)<.1);
        QSettings().setValue("appearance/skin",AppSkin::preset(0).json());applyAppSkin();QCOMPARE(button.styleSheet(),QString("padding: 8px;"));QVERIFY(qAbs(button.font().pointSizeF()-base)<.1);
    }
    void appSkinUpdatesExistingChromeAndReader() {
        const auto prior=QSettings().value("appearance/skin"),use=QSettings().value("reader/useAppSkin");const auto palette=QApplication::palette();auto restore=qScopeGuard([&]{if(prior.isValid())QSettings().setValue("appearance/skin",prior);else QSettings().remove("appearance/skin");if(use.isValid())QSettings().setValue("reader/useAppSkin",use);else QSettings().remove("reader/useAppSkin");applyAppSkin();QApplication::setPalette(palette);});
        MainWindow window;installNetwork(window);window.resize(1100,850);window.show();QTest::qWait(30);
        QSettings().setValue("reader/useAppSkin",true);const auto skin=AppSkin::preset(1);QSettings().setValue("appearance/skin",skin.json());applyAppSkin();QCOMPARE(window.palette().color(QPalette::Window),QColor(skin.background));QCOMPARE(window.m_searchInput->palette().color(QPalette::Text),QColor(skin.text));QCOMPARE(window.m_headerBar->palette().color(QPalette::Window),QColor(skin.background));QCOMPARE(window.m_searchButton->palette().color(QPalette::Button),QColor(skin.accent));
        Work work;work.id="865432";work.title="Skin preview";work.author="Reader";window.m_reader->openWork(work,{{"Chapter one","Reading text.\n\nAnother paragraph."}});window.m_reader->applyAppearance();QCOMPARE(window.m_reader->m_browser->palette().color(QPalette::Text),QColor(skin.text));QCOMPARE(window.m_reader->m_browser->palette().color(QPalette::Base),QColor(skin.background));
        if(qEnvironmentVariableIsSet("AO3_CAPTURE_SKINS")){window.showDashboard();QTest::qWait(80);window.grab().save("/tmp/ao3-home-dark.png");QSettings().setValue("appearance/skin",AppSkin::preset(0).json());applyAppSkin();QTest::qWait(80);window.grab().save("/tmp/ao3-home-light.png");auto layout=AppSkin::preset(0);layout.version=2;layout.uiScale=1.25;layout.cornerRadius=4;QSettings().setValue("appearance/skin",layout.json());applyAppSkin();QTest::qWait(80);window.grab().save("/tmp/ao3-home-native-skin.png");}
    }
    void filtersStartClosedAndHomeShowsLibraryActions() {
        MainWindow window;installNetwork(window);window.show();QTest::qWait(30);QVERIFY(window.m_filterDock->isHidden());QVERIFY(!window.m_filterButton->isChecked());QTest::mouseClick(window.m_filterButton,Qt::LeftButton);QVERIFY(!window.m_filterDock->isHidden());QTest::mouseClick(window.m_filterButton,Qt::LeftButton);QVERIFY(window.m_filterDock->isHidden());
        QVERIFY(window.m_dashboardView->findChild<QListWidget *>("homeRecentReads"));QSignalSpy browse(window.m_dashboardView,&DashboardWidget::browseRequested);for(auto *button:window.m_dashboardView->findChildren<QPushButton *>())if(button->text()=="Search")button->click();QCOMPARE(browse.size(),1);
        Work recent;recent.id="879999";recent.title="Recent work";recent.author="Author";QVERIFY(ReadingLibrary().saveOffline(recent,{{"One","A recent saved chapter."}}));window.m_librarySearch->setText("No match");window.m_libraryFilter->setCurrentIndex(2);window.m_dashboardView->refreshLibrary();window.m_dashboardView->workRequested(recent.id);QCOMPARE(window.m_reader->workId(),recent.id);QCOMPARE(window.m_reader->m_browser->toPlainText(),QString("A recent saved chapter."));
    }
    void websiteSessionProbePreservesOrReplacesTheNativeAccount() {
        const auto previousAgent=QSettings().value("network/userAgent");auto restore=qScopeGuard([&]{if(previousAgent.isValid())QSettings().setValue("network/userAgent",previousAgent);else QSettings().remove("network/userAgent");});
        QNetworkCookie cookie("_otwarchive_session","web-token");cookie.setDomain("archiveofourown.org");cookie.setPath("/");cookie.setHttpOnly(true);
        auto foreign=cookie;foreign.setDomain("evil.example");QCOMPARE(websiteSessionCookies({cookie,foreign}).size(),1);QVERIFY(websiteSessionCookies({cookie}).first().isSecure());QVERIFY(!validWebsiteSessionIdentity("Alice","Browser\r\nInjected: value"));
        MainWindow window;auto *network=installNetwork(window);QTest::qWait(50);Q_UNUSED(network);
        auto *probe=new FixtureNetwork;probe->setParent(window.m_client);delete window.m_client->m_sessionManager;window.m_client->m_sessionManager=probe;
        window.m_loginManager->setUserInfo({"Bob","https://archiveofourown.org/users/Bob",true});QNetworkCookie old("old_session","old-token");old.setDomain("archiveofourown.org");old.setPath("/");window.m_loginManager->setCookiesFromUrl({old},QUrl("https://archiveofourown.org/"));const auto beforeCookies=window.m_loginManager->cookies();
        QSignalSpy errors(window.m_client,&AO3Client::errorOccurred);QSignalSpy signedIn(window.m_client,&AO3Client::loginSuccessful);
        probe->page="<div id='greeting'><a href='/users/Bob'>Bob</a><a href='/users/logout'>Log out</a></div>";
        window.m_client->useWebsiteSession({cookie},"Browser/1","Alice");QTRY_COMPARE(errors.count(),1);QCOMPARE(window.m_loginManager->username(),QString("Bob"));QCOMPARE(window.m_loginManager->cookies(),beforeCookies);QCOMPARE(signedIn.count(),0);
        probe->page="<div id='greeting'><a href='/users/Alice'>Alice</a><a href='/users/logout'>Log out</a></div>";
        window.m_client->useWebsiteSession({cookie},"Browser/1","Alice");QTRY_COMPARE(signedIn.count(),1);QCOMPARE(window.m_loginManager->username(),QString("Alice"));bool oldRetained=false;for(const auto &c:window.m_loginManager->cookies())if(c.name()=="old_session")oldRetained=true;QVERIFY(!oldRetained);QCOMPARE(window.m_client->m_userAgent,QString("Browser/1"));
        const int requests=probe->requests.size();window.m_client->useWebsiteSession({foreign},"Browser/1","Alice");QCOMPARE(probe->requests.size(),requests);
    }
    void archiveWebsitePolicyAndNestedComments() {
        for(const auto &url:QStringList{"https://archiveofourown.org/","https://archiveofourown.org:443/users/Alice/preferences","https://archiveofourown.org/works/123?view_adult=true#comments"})QVERIFY(archiveWebUrl(QUrl(url)));
        for(const auto &url:QStringList{"http://archiveofourown.org/","https://archiveofourown.org.evil.example/","https://other.example/","file:///etc/passwd","javascript:alert(1)","https://name:secret@archiveofourown.org/","https://archiveofourown.org:444/"})QVERIFY(!archiveWebUrl(QUrl(url)));
        const QString nested="<ol class='thread'><li id='comment_1' class='comment'><h4 class='byline'><a href='/users/Alice'>Alice</a></h4><blockquote class='userstuff'>Parent</blockquote><ol class='thread'><li id='comment_2' class='comment'><h4 class='byline'><a href='/users/Bob'>Bob</a></h4><blockquote class='userstuff'>Reply</blockquote></li></ol></li></ol>";
        const auto comments=parseCommentPage(nested);QCOMPARE(comments.comments.size(),2);QCOMPARE(comments.comments[0].author,QString("Alice"));QCOMPARE(comments.comments[0].text,QString("Parent"));QCOMPARE(comments.comments[1].author,QString("Bob"));QCOMPARE(comments.comments[1].depth,1);
        QFile file(QStringLiteral(AO3_FIXTURE_DIR "/comment-form.html"));QVERIFY(file.open(QIODevice::ReadOnly));const auto form=QString::fromUtf8(file.readAll());
        QVERIFY(!parseCommentForm("<div class='userstuff'>"+form+"</div>").error.isEmpty());QVERIFY(!validCommentAction("/works/123/comments?unexpected=true"));
    }
    void archiveBookmarkOptionsAndPosting() {
        QFile file(QStringLiteral(AO3_FIXTURE_DIR "/archive-bookmark.html"));QVERIFY(file.open(QIODevice::ReadOnly));const auto html=QString::fromUtf8(file.readAll());
        const auto form=parseArchiveBookmark(html,"123");QVERIFY2(form.error.isEmpty(),qPrintable(form.error));QCOMPARE(form.pseud,QString("10"));QCOMPARE(form.pseuds.size(),2);QCOMPARE(form.notes,QString("<p>First & second</p>\nNext line"));QVERIFY(form.privateBookmark);QVERIFY(!form.recommendation);
        auto draft=form;draft.notes="New notes";draft.privateBookmark=false;draft.recommendation=true;
        const auto fields=archiveBookmarkFields(form,draft);QCOMPARE(fields.value("bookmark[private]"),QString("0"));QCOMPARE(fields.value("bookmark[rec]"),QString("1"));QVERIFY(!fields.contains("unwanted"));
        for(const auto &bad:QStringList{QString(html).replace("/works/123/bookmarks","https://evil.example/works/123/bookmarks"),QString(html).replace("authenticity_token","missing"),"<div id='workskin'>"+html+"</div>",QString(html).replace("/works/123/bookmarks","/works/999/bookmarks")})QVERIFY(!parseArchiveBookmark(bad,"123").error.isEmpty());
        const QString header="<div id='greeting'><a href='/users/Alice'>Alice</a><a href='/users/logout'>Log out</a></div>";
        const QString success=header+"<div class='flash notice'>Bookmark saved.</div><li id='bookmark_456' class='bookmark'><h4><a href='/works/123'>Work</a></h4><a href='/bookmarks/456/edit'>Edit</a></li>";
        QVERIFY(archiveFormResult(success).success);QVERIFY(archiveFormResult("<div id='workskin'>"+success+"</div>").uncertain);
        const auto edited=QString(html).replace("/works/123/bookmarks","/bookmarks/456").replace("<select name=","<input type='hidden' name='_method' value='put'><select name=");
        const auto edit=parseArchiveBookmark(edited,"123");QVERIFY(edit.error.isEmpty());QCOMPARE(edit.id,QString("456"));
        const auto confirm=header+"<form class='simple destroy' method='post' action='/bookmarks/456'><input type='hidden' name='_method' value='delete'><input type='hidden' name='authenticity_token' value='delete-token'></form>";
        QCOMPARE(parseBookmarkDelete(confirm,"456").value("_method"),QString("delete"));QVERIFY(parseBookmarkDelete(confirm,"999").isEmpty());
        MainWindow window;auto *network=installNetwork(window);QTest::qWait(50);window.m_loginManager->setUserInfo({"Alice","https://archiveofourown.org/users/Alice",true});
        QList<ArchiveFormResult> results;connect(window.m_client,&AO3Client::archiveBookmarkSaved,&window,[&](const QString &,const ArchiveFormResult &r){results.append(r);});
        network->pages={html.toUtf8(),success.toUtf8()};const int before=network->requests.size();window.m_client->saveArchiveBookmark(draft);window.m_client->saveArchiveBookmark(draft);
        QTRY_COMPARE(results.size(),1);QVERIFY(results.last().success);QCOMPARE(network->requests.size(),before+2);QCOMPARE(network->requests.last().path(),QString("/works/123/bookmarks"));QVERIFY(network->bodies.last().contains("bookmark%5Bprivate%5D=0"));QVERIFY(network->bodies.last().contains("fresh%26%2Btoken"));
        network->pages={edited.toUtf8(),confirm.toUtf8(),success.toUtf8(),html.toUtf8()};window.m_client->saveArchiveBookmark(edit,true);QTRY_COMPARE(results.size(),2);QVERIFY(results.last().success);QCOMPARE(network->requests[network->requests.size()-2].path(),QString("/bookmarks/456"));QVERIFY(network->bodies[network->bodies.size()-2].contains("_method=delete"));QVERIFY(network->bodies[network->bodies.size()-2].contains("delete-token"));
        network->pages={html.toUtf8(),QByteArray("Unknown")};window.m_client->saveArchiveBookmark(draft);QTRY_COMPARE(results.size(),3);QVERIFY(results.last().uncertain);
        network->page=edited.toUtf8();int count=network->requests.size();window.m_client->saveArchiveBookmark(draft);QTRY_COMPARE(results.size(),4);QCOMPARE(network->requests.size(),count+1);QVERIFY(!results.last().success);
        network->pages={edited.toUtf8(),success.toUtf8()};auto updated=edit;updated.tags="Updated tag";window.m_client->saveArchiveBookmark(updated);QTRY_COMPARE(results.size(),5);QVERIFY(results.last().success);QVERIFY(network->bodies.last().contains("_method=put"));
        network->page=edited.toUtf8();window.editArchiveBookmark("123");QTRY_COMPARE(window.m_archiveBookmark.id,QString("456"));window.m_archiveBookmarkNotes->setPlainText("Unsent draft");window.m_contentStack->setCurrentIndex(0);count=network->requests.size();window.editArchiveBookmark("123");QCOMPARE(window.m_archiveBookmarkNotes->toPlainText(),QString("Unsent draft"));QCOMPARE(network->requests.size(),count);window.onLogout();QVERIFY(window.m_archiveBookmarkNotes->toPlainText().isEmpty());
    }
    void workSubscriptionActions() {
        QFile freshFile(QStringLiteral(AO3_FIXTURE_DIR "/subscription-new.html")),existingFile(QStringLiteral(AO3_FIXTURE_DIR "/subscription-existing.html"));
        QVERIFY(freshFile.open(QIODevice::ReadOnly));QVERIFY(existingFile.open(QIODevice::ReadOnly));
        const auto fresh=QString::fromUtf8(freshFile.readAll()),existing=QString::fromUtf8(existingFile.readAll());
        const auto create=parseWorkSubscription(fresh,"123","Alice"),remove=parseWorkSubscription(existing,"123","Alice");
        QCOMPARE(create.path,QString("/users/Alice/subscriptions"));QVERIFY(!create.subscribed);QCOMPARE(create.fields.size(),3);QCOMPARE(create.fields.value("authenticity_token"),QString("csrf+&=token"));
        QVERIFY(remove.subscribed);QCOMPARE(remove.path,QString("/users/Alice/subscriptions/456"));QCOMPARE(remove.fields.value("_method"),QString("delete"));
        for(const QString &type:QStringList{"User","Series"}){const auto entity=QString(fresh).replace("value=\"Work\"","value=\""+type+"\"").replace("class=\"subscribe\"","class=\"navigation\"");const auto form=parseWorkSubscription(entity,type=="User"?QString():QString("123"),"Alice",type);QVERIFY(!form.path.isEmpty());QCOMPARE(form.fields.value("subscription[subscribable_type]"),type);QVERIFY(parseWorkSubscription("<div class='userstuff'>"+entity+"</div>",{},"Alice",type).path.isEmpty());}
        QVERIFY(parseWorkSubscription(fresh,"999","Alice").path.isEmpty());QVERIFY(parseWorkSubscription(existing,"123","Bob").path.isEmpty());
        for(const auto &bad:QStringList{QString(fresh).replace("/users/Alice/subscriptions","https://evil.example/users/Alice/subscriptions"),QString(fresh).replace("value=\"Work\"","value=\"User\""),QString(existing).replace("value=\"delete\"","value=\"patch\""),QString(fresh).replace("authenticity_token","missing"),"<div id='workskin'>"+fresh+"</div>","<div class='userstuff'>"+existing+"</div>",QString(existing).replace("/subscriptions/456","/subscriptions/delete_all")})QVERIFY(parseWorkSubscription(bad,"123","Alice").path.isEmpty());
        MainWindow window;auto *network=installNetwork(window);QTest::qWait(50);
        window.m_loginManager->setUserInfo({"Alice","https://archiveofourown.org/users/Alice",true});
        QSignalSpy finished(window.m_client,&AO3Client::accountActionFinished);
        network->pages={fresh.toUtf8(),existing.toUtf8()};int before=network->requests.size();
        window.m_client->setWorkSubscribed("123",true);window.m_client->setWorkSubscribed("123",true);window.m_client->setMarkedForLater("123",true);
        QTRY_COMPARE(finished.size(),1);QCOMPARE(finished.last()[1].toString(),QString("Subscribed to this work on AO3"));QCOMPARE(network->requests.size(),before+2);QCOMPARE(network->requests.last().path(),QString("/users/Alice/subscriptions"));
        QVERIFY(!network->bodies.last().contains("unwanted"));QVERIFY(network->bodies.last().contains("subscription%5Bsubscribable_id%5D=123"));QVERIFY(!network->bodies.last().contains("_method"));
        network->page=existing.toUtf8();before=network->requests.size();window.m_client->setWorkSubscribed("123",true);QTRY_COMPARE(finished.size(),2);QCOMPARE(network->requests.size(),before+1);
        network->pages={existing.toUtf8(),fresh.toUtf8()};window.m_client->setWorkSubscribed("123",false);QTRY_COMPARE(finished.size(),3);QCOMPARE(finished.last()[1].toString(),QString("Unsubscribed from this work on AO3"));QCOMPARE(network->requests.last().path(),QString("/users/Alice/subscriptions/456"));QVERIFY(network->bodies.last().contains("_method=delete"));
        network->pages={fresh.toUtf8(),QByteArray("Unknown response")};window.m_client->setWorkSubscribed("123",true);QTRY_COMPARE(finished.size(),4);QVERIFY(finished.last()[1].toString().contains("Could not confirm"));
        network->page=QString(fresh).replace("Alice","Bob").toUtf8();before=network->requests.size();window.m_client->setWorkSubscribed("123",true);QTRY_COMPARE(finished.size(),5);QCOMPARE(network->requests.size(),before+1);
        network->page=fresh.toUtf8();window.m_client->setWorkSubscribed("123",true);window.m_client->logout();QTest::qWait(50);QCOMPARE(finished.size(),5);QVERIFY(window.m_client->m_workAccountPending.isEmpty());
    }
    void readingListActions() {
        QFile fixture(QStringLiteral(AO3_FIXTURE_DIR "/reading-action.html"));QVERIFY(fixture.open(QIODevice::ReadOnly));const auto html=QString::fromUtf8(fixture.readAll());
        const auto form=parseReadingAction(html,"123");QCOMPARE(form.action,QString("mark_for_later"));QCOMPARE(form.fields.size(),2);QCOMPARE(form.fields.value("authenticity_token"),QString("csrf+&=token"));
        QVERIFY(parseReadingAction(html,"456").action.isEmpty());
        for(const auto &bad:QStringList{QString(html).replace("/works/123/mark_for_later","https://evil.example/works/123/mark_for_later"),QString(html).replace("value=\"patch\"","value=\"delete\""),QString(html).replace("authenticity_token","missing"),"<div id='workskin'>"+html+"</div>"})QVERIFY(parseReadingAction(bad,"123").action.isEmpty());
        MainWindow window;auto *network=installNetwork(window);QTest::qWait(50);
        window.m_loginManager->setUserInfo({"Alice","https://archiveofourown.org/users/Alice",true});
        QSignalSpy finished(window.m_client,&AO3Client::accountActionFinished);
        const auto read=QString(html).replace("mark_for_later","mark_as_read");network->pages={html.toUtf8(),read.toUtf8()};int before=network->requests.size();
        window.m_client->setMarkedForLater("123",true);window.m_client->setMarkedForLater("123",true);
        QTRY_COMPARE(finished.size(),1);QCOMPARE(finished.last()[1].toString(),QString("Marked for later on AO3"));QCOMPARE(network->requests.size(),before+2);QCOMPARE(network->requests.last().path(),QString("/works/123/mark_for_later"));QVERIFY(network->bodies.last().contains("_method=patch"));
        network->page=read.toUtf8();before=network->requests.size();window.m_client->setMarkedForLater("123",true);QTRY_COMPARE(finished.size(),2);QCOMPARE(network->requests.size(),before+1);
        network->pages={read.toUtf8(),html.toUtf8()};window.m_client->setMarkedForLater("123",false);QTRY_COMPARE(finished.size(),3);QCOMPARE(finished.last()[1].toString(),QString("Marked as read on AO3"));
        network->pages={html.toUtf8(),QByteArray("Unknown")};window.m_client->setMarkedForLater("123",true);QTRY_COMPARE(finished.size(),4);QVERIFY(finished.last()[1].toString().contains("Could not confirm"));
        network->page=QString(html).replace("Alice","Bob").toUtf8();before=network->requests.size();window.m_client->setMarkedForLater("123",true);QTRY_COMPARE(finished.size(),5);QCOMPARE(network->requests.size(),before+1);
    }
    void kudosFormsResultsAndPosting() {
        QFile fixture(QStringLiteral(AO3_FIXTURE_DIR "/kudos-form.html"));QVERIFY(fixture.open(QIODevice::ReadOnly));const auto html=QString::fromUtf8(fixture.readAll());
        const auto fields=parseKudosForm(html,"123");QCOMPARE(fields.size(),3);QCOMPARE(fields.value("authenticity_token"),QString("csrf+&=token"));
        QVERIFY(parseKudosForm(html,"456").isEmpty());
        QVERIFY(parseKudosForm(QString(html).replace("action=\"/kudos\"","action=\"https://evil.example/kudos\""),"123").isEmpty());
        QVERIFY(siteIdentityMatches(html,{}));QVERIFY(!siteIdentityMatches(html,"Alice"));
        const QString signedIn="<div id='greeting'>Hi, <a href='/users/Alice'>Alice</a><a href='/users/logout'>Log out</a></div>";
        QVERIFY(siteIdentityMatches(signedIn,"Alice"));QVERIFY(!siteIdentityMatches(signedIn,{}));
        QVERIFY(!siteIdentityMatches(QString(signedIn).replace("Alice","Bob")+"<div id='workskin'>Hi, <a href='/users/Alice'>Alice</a></div>","Alice"));
        QVERIFY(siteIdentityMatches("<nav id='greeting'><a href='/users/Alice'>Hi, Alice!</a><a href='/users/logout?return_to=%2F'>Log out</a></nav>","Alice"));
        const QString success="<div id='feedback'><div class='flash kudos_notice'>Thank you for leaving kudos!</div></div>";
        QVERIFY(parseKudosResult(success).success);
        QVERIFY(!parseKudosResult("<div id='feedback'><div class='flash kudos_error'>You have already left kudos here.</div></div>").success);
        QVERIFY(parseKudosResult("<div id='workskin'>"+success+"</div>").uncertain);
        QVERIFY(parseKudosResult("<div class='notice'>Not a kudos confirmation</div>").uncertain);
        QVERIFY(parseCommentSubmission("<div class='userstuff'><div class='flash comment_notice'>Fake confirmation</div></div>").uncertain);
        QVERIFY(parseCommentSubmission("<div class='flash comment_error'>Comment rejected</div>").error.contains("rejected"));
        MainWindow window;auto *network=installNetwork(window);QTest::qWait(50);
        network->pages={html.toUtf8(),success.toUtf8()};const int before=network->requests.size();
        Work work;work.id="123";window.m_detailWidget->showWork(work);
        auto *button=window.m_detailWidget->findChild<QPushButton *>("giveKudos");QVERIFY(button);QVERIFY(button->text().contains("guest"));
        button->click();button->click();QVERIFY(!button->isEnabled());
        QTRY_COMPARE(window.m_client->kudosStatus("123"),QString("Kudos given"));
        QCOMPARE(network->requests.size(),before+2);QCOMPARE(network->requests.last().path(),QString("/kudos"));
        QCOMPARE(network->operations.last(),QNetworkAccessManager::PostOperation);
        QMap<QString,QString> body;for(const auto &pair:network->bodies.last().split('&')) {const int eq=pair.indexOf('=');body[QUrl::fromPercentEncoding(pair.left(eq))]=QUrl::fromPercentEncoding(pair.mid(eq+1));}
        QCOMPARE(body.value("kudo[commentable_id]"),QString("123"));QCOMPARE(body.value("authenticity_token"),QString("csrf+&=token"));
        window.m_client->giveKudos("123");QCOMPARE(network->requests.size(),before+2);
        window.m_reader->openWork(work,{{"One","Story"}});auto *readerKudos=window.m_reader->findChild<QPushButton *>("readerKudos");QVERIFY(readerKudos);QVERIFY(!readerKudos->isEnabled());QCOMPARE(readerKudos->text(),QString("Kudos given"));
        const auto other=QString(html).replace("value=\"123\"","value=\"456\"");network->pages={other.toUtf8(),QByteArray("<h1>Unknown response</h1>")};
        window.m_client->giveKudos("456");QTRY_COMPARE(window.m_client->kudosStatus("456"),QString("Check kudos on AO3"));
        const int after=network->requests.size();window.m_client->giveKudos("456");QCOMPARE(network->requests.size(),after);
        window.m_loginManager->setUserInfo({"Alice", "https://archiveofourown.org/users/Alice",true});network->page=QString(html).replace("value=\"123\"","value=\"789\"").toUtf8();
        window.m_client->giveKudos("789");QTRY_VERIFY(window.m_client->kudosStatus("789").isEmpty());QCOMPARE(network->requests.size(),after+1);
        window.m_client->logout();QVERIFY(window.m_client->kudosStatus("123").isEmpty());
    }
    void officialDownloadsSaveFilesAndPreserveOnFailure() {
        MainWindow window; auto *network=installNetwork(window);
        QTest::qWait(50);
        QTemporaryDir dir; const QString path=dir.filePath("story.pdf");
        QSignalSpy saved(window.m_client,&AO3Client::officialDownloadSaved);
        QSignalSpy errors(window.m_client,&AO3Client::errorOccurred);
        network->page="%PDF-1.4 fixture bytes";
        window.m_client->saveOfficialDownload("123",QUrl("https://archiveofourown.org/downloads/123/A.pdf"),path);
        QTRY_COMPARE(saved.count(),1);
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const auto original=file.readAll();file.close(); QCOMPARE(original,network->page);
        network->page="<html>Service unavailable</html>";
        window.m_client->saveOfficialDownload("123",QUrl("https://archiveofourown.org/downloads/123/A.pdf"),path);
        QTRY_COMPARE(errors.count(),1); QCOMPARE(saved.count(),1);
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),original);file.close();
        network->status=302;
        window.m_client->saveOfficialDownload("123",QUrl("https://archiveofourown.org/downloads/123/A.pdf"),path);
        QTRY_COMPARE(errors.count(),2);
        const int requests=network->requests.size();
        window.m_client->saveOfficialDownload("123",QUrl("https://evil.example/downloads/123/A.pdf"),path);
        QCOMPARE(network->requests.size(),requests); QCOMPARE(errors.count(),3);
    }
    void officialDownloadLinks() {
        QFile fixture(QStringLiteral(AO3_FIXTURE_DIR "/official-downloads.html")); QVERIFY(fixture.open(QIODevice::ReadOnly));
        const auto links=officialDownloads(QString::fromUtf8(fixture.readAll()),"123");
        QCOMPARE(links.size(),5); QCOMPARE(links[0].format,QString("epub"));
        QVERIFY(links[0].url.query().contains("x=1"));
        QCOMPARE(officialDownloads("<p>Downloads disabled</p>","123").size(),0);
        QVERIFY(officialDownloadFormat(QUrl("https://archiveofourown.org:444/downloads/123/A.pdf"),"123").isEmpty());
    }

    void commentFormsAndPosting() {
        QFile fixture(QStringLiteral(AO3_FIXTURE_DIR "/comment-form.html"));QVERIFY(fixture.open(QIODevice::ReadOnly));const auto html=fixture.readAll();
        const auto form=parseCommentForm(QString::fromUtf8(html));QVERIFY2(form.error.isEmpty(),qPrintable(form.error));
        QCOMPARE(form.hidden.value("authenticity_token"),QString("csrf+&=token"));QCOMPARE(form.hidden.value("comment[pseud_id]"),QString("12"));
        QCOMPARE(form.pseuds.size(),2);QVERIFY(form.notice.contains("moderated"));QVERIFY(!form.guest);
        QVERIFY(!validCommentAction("https://example.invalid/comments"));QVERIFY(!validCommentAction("//example.invalid/comments"));
        QVERIFY(!parseCommentForm(QString::fromUtf8(html).replace("/works/123/comments","https://example.invalid/comments")).error.isEmpty());
        QVERIFY(parseCommentSubmission("<div class='flash comment_notice'>Comment awaiting moderation.</div>").notice.contains("moderation"));
        QVERIFY(!parseCommentSubmission("<div id='errorExplanation'>Comment is too long.</div>").uncertain);
        QVERIFY(parseCommentSubmission("<h1>Maintenance</h1>").uncertain);
        MainWindow window;window.show();auto *network=installNetwork(window);QTest::qWait(30);network->page=html;
        window.m_commentsWork="123";window.m_contentStack->setCurrentWidget(window.m_commentsPage);window.composeComment();
        QTRY_VERIFY(!window.m_commentForm.action.isEmpty());QCOMPARE(network->operations.last(),QNetworkAccessManager::GetOperation);
        QCOMPARE(network->requests.last().path(),QString("/works/123/comments/new"));
        window.m_commentDraft->setPlainText("A+B & 世界 %2B=100%\nSecond line");network->page="<div class='flash comment_notice'>Comment awaiting moderation.</div>";
        const int before=network->requests.size();window.m_commentPost->click();window.m_commentPost->click();
        QTRY_VERIFY(!window.m_commentSending);QVERIFY(window.m_commentDraft->toPlainText().isEmpty());
        int posts=0;for(int i=before;i<network->operations.size();++i)if(network->operations[i]==QNetworkAccessManager::PostOperation) {
            ++posts;QMap<QString,QString> body;for(const auto &pair:network->bodies[i].split('&')) {const int eq=pair.indexOf('=');body[QUrl::fromPercentEncoding(pair.left(eq))]=QUrl::fromPercentEncoding(pair.mid(eq+1));}
            QCOMPARE(body.value("comment[comment_content]"),QString("A+B & 世界 %2B=100%\nSecond line"));QCOMPARE(body.value("authenticity_token"),QString("csrf+&=token"));
        }
        QCOMPARE(posts,1);
        network->page=html;window.composeComment();QTRY_VERIFY(window.m_commentEditor->isVisible());window.m_commentDraft->setPlainText("Keep this draft");
        network->fail=true;window.m_commentPost->click();QTRY_VERIFY(!window.m_commentSending);
        QVERIFY(window.m_commentUncertain);QCOMPARE(window.m_commentDraft->toPlainText(),QString("Keep this draft"));QVERIFY(!window.m_commentPost->isEnabled());
        const int failedCount=network->requests.size();window.m_commentPost->click();QCOMPARE(network->requests.size(),failedCount);

    }
    void commentsParsingAndNavigation() {
        QFile fixture(QStringLiteral(AO3_FIXTURE_DIR "/comments-page.html"));QVERIFY(fixture.open(QIODevice::ReadOnly));const auto html=QString::fromUtf8(fixture.readAll());
        const auto parsed=parseCommentPage(html);
        QVERIFY(parsed.error.isEmpty());QCOMPARE(parsed.comments.size(),3);QCOMPARE(parsed.pages,4);
        QCOMPARE(parsed.comments[0].id,QString("101"));QCOMPARE(parsed.comments[0].author,QString("Pen"));
        QVERIFY(parsed.comments[0].text.contains("Hello & 世界."));QVERIFY(parsed.comments[0].text.contains("A quoted line."));
        QVERIFY(!parsed.comments[0].text.contains("private@example.invalid"));QVERIFY(!parsed.comments[0].text.contains("192.0.2.1"));
        QCOMPARE(parsed.comments[1].depth,1);QCOMPARE(parsed.comments[1].author,QString("Guest"));QCOMPARE(parsed.comments[1].text,QString("Reply text."));
        QVERIFY(parsed.comments[2].text.contains("deleted"));
        QVERIFY(!parseCommentPage("<h1>Maintenance</h1>").error.isEmpty());
        QVERIFY(parseCommentPage("<div id='comments_placeholder'></div>").comments.isEmpty());
        MainWindow window;window.show();auto *network=installNetwork(window);QTest::qWait(30);network->page=html.toUtf8();
        window.showComments("123");
        QTRY_VERIFY(window.m_commentsText->toPlainText().contains("Hello & 世界."));
        QCOMPARE(network->requests.last().path(),QString("/works/123/comments"));
        QCOMPARE(window.m_contentStack->currentWidget(),window.m_commentsPage);QVERIFY(window.m_commentsNext->isEnabled());
        QTest::mouseClick(window.m_commentsNext,Qt::LeftButton);
        QTRY_COMPARE(window.m_commentsCurrent,2);QCOMPARE(QUrlQuery(network->requests.last()).queryItemValue("page"),QString("2"));
        QVERIFY(window.navigateBack());QVERIFY(window.m_contentStack->currentWidget()!=window.m_commentsPage);
    }
    void initTestCase() {
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName("AO3ClientTests");
        QCoreApplication::setApplicationName(QStringLiteral("ao3-functional-") + QUuid::createUuid().toString(QUuid::Id128));
    }
    void cleanupTestCase() {
        const QString settingsFile = QSettings().fileName();
        QSettings().clear();
        QFile::remove(settingsFile);
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
    }
    void libraryBackupRoundTripAndValidation() {
        QTemporaryDir source,destination; QFile fixture(QString(AO3_FIXTURE_DIR)+"/library-backup-v1.json"); QVERIFY(fixture.open(QIODevice::ReadOnly));
        const auto data=QJsonDocument::fromJson(fixture.readAll()).object(); QString error;
        QVERIFY2(LibraryBackup::merge(data,&error,ReadingLibrary(source.path())),qPrintable(error));
        const auto snapshot=LibraryBackup::snapshot(ReadingLibrary(source.path()));
        const auto bytes=QJsonDocument(snapshot).toJson(); QVERIFY(!bytes.contains("cookies")); QVERIFY(!bytes.contains("translation/key"));
        QVERIFY2(LibraryBackup::merge(snapshot,&error,ReadingLibrary(destination.path())),qPrintable(error));
        QVERIFY(LibraryBackup::merge(snapshot,&error,ReadingLibrary(destination.path())));
        ReadingLibrary imported(destination.path()); QCOMPARE(imported.entries().size(),1); QCOMPARE(imported.load("987654321").size(),2);
        QVERIFY(imported.entries().first().toObject().value("favorite").toBool()); QCOMPARE(QSettings().value("bookmarks/987654321").toList().size(),1);
        auto invalid=data; auto works=invalid.value("works").toArray(); auto entry=works.first().toObject(); auto work=entry.value("work").toObject(); work["id"]="../escape"; entry["work"]=work; works[0]=entry; invalid["works"]=works;
        QVERIFY(!LibraryBackup::merge(invalid,&error,imported)); QCOMPARE(imported.entries().size(),1);
        QFile broken(destination.path()+"/index.json"); QVERIFY(broken.open(QIODevice::WriteOnly)); broken.write("damaged"); broken.close();
        QVERIFY(!imported.entries().isEmpty()); QVERIFY(imported.entries().first().toObject().value("offline").toBool()); QVERIFY(!imported.entries().isEmpty()); QVERIFY(!QDir(destination.path()).entryList({"index.corrupt-*"}).isEmpty());
        QSettings().remove("bookmarks/987654321"); QSettings().remove("readingProgress/987654321");
    }
    void translationRequiresEncryptedOrLocalService() {
        QVERIFY(validTranslationService(QUrl("https://translator.example"))); QVERIFY(validTranslationService(QUrl("http://127.0.0.1:5000")));
        QVERIFY(!validTranslationService(QUrl("http://translator.example"))); QVERIFY(!validTranslationService(QUrl("https://user:secret@translator.example"))); QVERIFY(!validTranslationService(QUrl("file:///tmp/service")));
    }
    void chapterBoundaryTurnsReachDistinctContent() {
        ReaderWidget reader; reader.resize(900,900); reader.show(); Work work; work.id="1010101";
        reader.openWork(work,{{"One","First chapter"},{"Two","Distinct second chapter"}}); QTRY_VERIFY(!reader.m_restoringPosition);
        reader.setBookMode(true); reader.m_book->animated=false; reader.m_book->turn(1); QTRY_VERIFY(!reader.m_restoringPosition);
        QCOMPARE(reader.m_index,1); QCOMPARE(reader.m_browser->toPlainText(),"Distinct second chapter");
        reader.m_book->turn(-1); QTRY_VERIFY(!reader.m_restoringPosition); QCOMPARE(reader.m_index,0);
        QSettings().remove("readingProgress/1010101"); QSettings().setValue("reader/bookMode",false);
    }
    void compactReaderKeepsTextPrimary() {
        QSettings().setValue("reader/bookMode",false);
        ReaderWidget reader; reader.resize(900,850); reader.show(); Work work; work.id="1111122"; work.title="Reading space";
        reader.openWork(work,{{"One",QString("A readable paragraph. ").repeated(1000)}}); QTRY_VERIFY(!reader.m_restoringPosition);
        QVERIFY(reader.m_desktopOptions->isHidden()); QVERIFY(reader.m_browser->height()>reader.height()*.55);
        QTest::mouseClick(reader.m_desktopOptionsToggle,Qt::LeftButton); QVERIFY(reader.m_desktopOptions->isVisible());
        QVERIFY(reader.closeOptions()); QVERIFY(reader.m_desktopOptions->isHidden());
    }
    void cookiePersistence() {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("cookies.dat"));
        const QUrl origin(QStringLiteral("https://archiveofourown.org/"));
        {
            LoginManager manager(nullptr, path);
            QNetworkCookie cookie("_session_id", "test-session");
            cookie.setPath(QStringLiteral("/"));
            cookie.setSecure(true);
            QVERIFY(manager.setCookiesFromUrl({cookie}, origin));
            QVERIFY(!manager.isLoggedIn());
            manager.checkSession();
            QVERIFY(!manager.isLoggedIn());
        }
        {
            LoginManager restored(nullptr, path);
            QCOMPARE(restored.cookies().size(), 1);
            QVERIFY(!restored.isLoggedIn());
            QVERIFY(!(QFile::permissions(path) & (QFileDevice::ReadGroup | QFileDevice::ReadOther)));
            restored.logout();
        }
        LoginManager signedOut(nullptr, path);
        QVERIFY(signedOut.cookies().isEmpty());
    }
    void authenticationAndUserWorks() {
        MainWindow window;
        auto *network = installNetwork(window);
        QTest::qWait(30);
        const QUrl origin(QStringLiteral("https://archiveofourown.org/"));
        QNetworkCookie anonymous("_session_id", "anonymous");
        anonymous.setPath(QStringLiteral("/"));
        QVERIFY(window.m_loginManager->setCookiesFromUrl({anonymous}, origin));
        const QByteArray loginPage = R"(<input name="authenticity_token" value="csrf-test" />)";
        const QByteArray identity = R"(<nav id='greeting'><a class='user' href='/users/MixedCase'>Hi, MixedCase!</a><a href='/users/logout?return_to=%2F'>Sign out</a></nav>)";
        QSignalSpy failed(window.m_client, &AO3Client::loginFailed);
        QSignalSpy success(window.m_client, &AO3Client::loginSuccessful);
        network->pages = {loginPage, "Invalid password", "<a href='/users/login'>Sign in</a>"};
        window.m_client->login(QStringLiteral("wrong"), QStringLiteral("wrong"));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(success.count(), 0);
        QCOMPARE(network->operations.at(network->operations.size() - 2), QNetworkAccessManager::PostOperation);
        QVERIFY(network->bodies.at(network->bodies.size() - 2).contains("authenticity_token=csrf-test"));
        QVERIFY(!window.m_loginManager->isLoggedIn());
        network->pages = {loginPage, "Redirected", identity};
        window.m_client->login(QStringLiteral("MixedCase"), QStringLiteral("test-only"));
        QTRY_COMPARE(success.count(), 1);
        QVERIFY(window.m_loginManager->isLoggedIn());
        QCOMPARE(window.m_loginManager->username(), QStringLiteral("MixedCase"));
        QCOMPARE(window.m_loginManager->userInfo().dashboardUrl, QStringLiteral("https://archiveofourown.org/users/MixedCase"));
        QCOMPARE(window.m_loginButton->text(), QStringLiteral("M"));
        QCOMPARE(window.m_loginButton->accessibleName(),QStringLiteral("Account: MixedCase"));
        QVERIFY(!window.m_navWorks->isHidden());
        QCOMPARE(window.m_contentStack->currentIndex(), 0);
        // Restart does not trust stored cookies until a new server verification.
        LoginManager restored;
        QVERIFY(!restored.cookies().isEmpty());
        QVERIFY(!restored.isLoggedIn());
        window.m_loginManager->setUserInfo({});
        network->page = identity;
        QSignalSpy checked(window.m_client, &AO3Client::sessionChecked);
        window.m_client->checkSession();
        QTRY_COMPARE(checked.count(), 1);
        QVERIFY(checked.first().first().toBool());
        QCOMPARE(window.m_loginManager->username(), QStringLiteral("MixedCase"));
        // An incomplete response must not be mistaken for an expired session.
        network->page = "<h1>Maintenance</h1>";
        QSignalSpy incomplete(window.m_client, &AO3Client::errorOccurred);
        window.m_client->checkSession();
        QTRY_COMPARE(incomplete.count(), 1);
        QVERIFY(window.m_loginManager->isLoggedIn());
        // Temporary connection failures do not erase a verified session or cookies.
        network->fail = true;
        QSignalSpy error(window.m_client, &AO3Client::errorOccurred);
        window.m_client->checkSession();
        QTRY_COMPARE(error.count(), 1);
        QVERIFY(window.m_loginManager->isLoggedIn());
        QVERIFY(!window.m_loginManager->cookies().isEmpty());
        network->fail = false;
        const QByteArray works = R"(<ol><li id="work_456" class="work blurb"><h4 class="heading"><a href="/works/456">My Work</a></h4></li></ol>)";
        network->page = works + R"(<ol class="pagination"><a href="/users/MixedCase/works?page=2">2</a><em class="current">1</em></ol>)";
        QTest::mouseClick(window.m_navWorks, Qt::LeftButton);
        QTRY_COMPARE(window.m_workList->count(), 1);
        QVERIFY(window.m_showingUserWorks);
        QCOMPARE(window.m_detailWidget->currentWork().id, QStringLiteral("456"));
        QCOMPARE(network->requests.last().path(), QStringLiteral("/users/MixedCase/works"));
        network->page = works + R"(<ol class="pagination"><a href="/users/MixedCase/works?page=1">1</a><em class="current">2</em></ol>)";
        window.goToPage(2);
        QTRY_COMPARE(window.m_workList->count(), 1);
        QCOMPARE(network->requests.last().path(), QStringLiteral("/users/MixedCase/works"));
        QCOMPARE(QUrlQuery(network->requests.last()).queryItemValue(QStringLiteral("page")), QStringLiteral("2"));
        QCOMPARE(window.m_currentResult.totalPages, 2);
        QVERIFY(window.m_prevButton->isEnabled());
        QVERIFY(!window.m_nextButton->isEnabled());
        // Switching to search discards an older pending account response.
        window.showUserWorks();
        network->page = QByteArray(works).replace("456", "789");
        window.m_searchInput->setText(QStringLiteral("new search"));
        window.performSearch();
        QTRY_COMPARE(window.m_detailWidget->currentWork().id, QStringLiteral("789"));
        QVERIFY(!window.m_showingUserWorks);
        // Expired server sessions clear both the header and account results.
        network->page = works;
        window.showUserWorks();
        QTRY_COMPARE(window.m_workList->count(), 1);
        network->page = "<a href='/users/login'>Sign in</a>";
        window.m_client->checkSession();
        QTRY_VERIFY(!window.m_loginManager->isLoggedIn());
        QCOMPARE(window.m_loginButton->text(), QStringLiteral("Log In"));
        QVERIFY(window.m_navWorks->isHidden());
        QCOMPARE(window.m_workList->count(), 0);
        network->page = identity;
        window.m_client->checkSession();
        QTRY_VERIFY(window.m_loginManager->isLoggedIn());
        // A late authenticated response cannot undo sign-out.
        network->page = identity;
        window.m_client->checkSession();
        window.onLogout();
        QTest::qWait(30);
        QVERIFY(!window.m_loginManager->isLoggedIn());
        QCOMPARE(window.m_workList->count(), 0);
        QCOMPARE(window.m_contentStack->currentIndex(), 0);
        LoginManager loggedOut;
        QVERIFY(loggedOut.cookies().isEmpty());
    }
    void excludeSingleChapterUsesServerQuery() {
        MainWindow window;auto *network=installNetwork(window);network->page="<ol class='work index group'></ol>";
        SearchFilters filters;filters["single_chapter"]={"exclude"};window.m_filterPanel->setFilters(filters);
        QCOMPARE(window.m_filterPanel->filters().value("single_chapter"),QStringList{"exclude"});
        QSignalSpy done(window.m_client,&AO3Client::searchCompleted);window.m_client->search("alpha OR beta",2,filters);QTRY_COMPARE(done.size(),1);
        QUrl searchUrl;for(const auto &request:network->requests)if(request.path()=="/works/search")searchUrl=request;QVERIFY(!searchUrl.isEmpty());
        const QUrlQuery query(searchUrl);QCOMPARE(query.queryItemValue("work_search[query]"),QString("(alpha OR beta) AND -expected_number_of_chapters:1"));QVERIFY(!query.hasQueryItem("work_search[single_chapter]"));QCOMPARE(query.queryItemValue("page"),QString("2"));
    }
    void searchUsesRealHttpRoute() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QStringList requestedQueries;
        QStringList requestPaths;
        QList<QUrl> requestedUrls;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket, buffer] {
                buffer->append(socket->readAll());
                if (!buffer->contains("\r\n\r\n")) return;
                const auto parts = buffer->split(' ');
                if (parts.size() < 2) return;
                const QUrl url = QUrl::fromEncoded(parts[1]);
                requestPaths.append(url.path());
                requestedUrls.append(url);
                const QString query = QUrlQuery(url).queryItemValue(QStringLiteral("work_search[query]"));
                requestedQueries.append(query);
                QByteArray response;
                // The index deliberately ignores the query, matching the original bug.
                if (url.path() != QStringLiteral("/works/search")) response = "<ol class='work index'></ol>";
                else if (query == QStringLiteral("first & query"))
                    response = R"(<ol><li id="work_101" class="work"><h4 class="heading"><a href="/works/101">First response</a></h4></li></ol>)";
                else if (query == QStringLiteral("second query"))
                    response = R"(<ol><li id="work_202" class="work"><h4 class="heading"><a href="/works/202">Second response</a></h4></li></ol>)";
                else response = "<ol class='work index'></ol>";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: " + QByteArray::number(response.size()) + "\r\n\r\n" + response);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
        // Only the transport address changes: Qt sends/receives actual HTTP bytes.
        class LocalNetwork : public QNetworkAccessManager {
        public:
            quint16 port = 0;
            QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *body) override {
                QNetworkRequest local(request);
                QUrl url = request.url();
                url.setScheme(QStringLiteral("http")); url.setHost(QStringLiteral("127.0.0.1")); url.setPort(port);
                local.setUrl(url);
                return QNetworkAccessManager::createRequest(op, local, body);
            }
        };
        auto *manager = new LoginManager;
        AO3Client client(manager);
        auto *original = client.m_manager;
        manager->setParent(nullptr);
        original->setCookieJar(new QNetworkCookieJar(original));
        auto *network = new LocalNetwork;
        network->setParent(&client); network->setCookieJar(manager); network->port = server.serverPort();
        client.m_manager = network;
        delete original;
        QSignalSpy responses(&client, &AO3Client::searchCompleted);
        client.search(QStringLiteral("first & query"));
        QTRY_COMPARE(responses.count(), 1);
        const auto first = qvariant_cast<SearchResult>(responses.takeFirst().at(0));
        QVERIFY(first.errorMessage.isEmpty());
        QCOMPARE(first.works.size(), 1);
        QCOMPARE(first.works.first().id, QStringLiteral("101"));
        const SearchFilters httpFilters = {{QStringLiteral("fandom_names"), {QStringLiteral("A & B")}}, {QStringLiteral("archive_warning_ids[]"), {QStringLiteral("17"), QStringLiteral("18")}}};
        client.search(QStringLiteral("second query"), 2, httpFilters);
        QTRY_COMPARE(responses.count(), 1);
        const auto second = qvariant_cast<SearchResult>(responses.takeFirst().at(0));
        QCOMPARE(second.works.size(), 1);
        QCOMPARE(second.works.first().id, QStringLiteral("202"));
        QCOMPARE(second.currentPage, 2);
        const QUrlQuery sent(requestedUrls.last());
        QCOMPARE(sent.queryItemValue(QStringLiteral("work_search[fandom_names]")), QStringLiteral("A & B"));
        QCOMPARE(sent.allQueryItemValues(QStringLiteral("work_search[archive_warning_ids][]")), QStringList({QStringLiteral("17"), QStringLiteral("18")}));
        QCOMPARE(requestPaths, QStringList({QStringLiteral("/works/search"), QStringLiteral("/works/search")}));
        QCOMPARE(requestedQueries, QStringList({QStringLiteral("first & query"), QStringLiteral("second query")}));
    }
    void searchFiltersAndArrows() {
        MainWindow window;
        window.show();
        auto *network = installNetwork(window);
        window.show();QTest::qWait(30);QTest::mouseClick(window.m_filterButton,Qt::LeftButton);QVERIFY(!window.m_filterDock->isHidden());
        QCOMPARE(window.m_prevButton->text(), QStringLiteral("Previous"));
        QCOMPARE(window.m_nextButton->text(), QStringLiteral("Next"));
        QVERIFY(!window.m_prevButton->icon().isNull());
        QVERIFY(!window.m_nextButton->icon().isNull());
        network->page = R"(<select name='work_search[rating_ids]'><option value=''>Any</option><option value='913'>Explicit</option><option value='910'>General Audiences</option></select>
<select name="work_search[language_id]"><option value="en">English</option><option value="et">Eesti</option></select>
<input type='checkbox' name='work_search[archive_warning_ids][]' id='w17' value='917'><label for='w17'>Violence &amp; injury</label>
<input type='checkbox' name='work_search[category_ids][]' id='c23' value='923'><label for='c23'>F/F</label>)";
        QSignalSpy options(window.m_client, &AO3Client::searchOptionsFetched);
        window.m_filterPanel->setLoading();
        window.m_client->fetchSearchOptions();
        QTRY_COMPARE(options.count(), 1);
        QVERIFY(window.m_filterChoicesLoaded);
        if (qEnvironmentVariableIsSet("AO3_CAPTURE_FILTERS")) {
            window.show();
            QTest::qWait(30);
            window.grab().save(qEnvironmentVariable("AO3_CAPTURE_FILTERS"));
        }
        QCOMPARE(network->requests.last().path(), QStringLiteral("/works/search"));
        const SearchFilters filters = {
            {QStringLiteral("fandom_names"), {QStringLiteral("A & B")}},
            {QStringLiteral("excluded_tag_names"), {QStringLiteral("Unwanted")}},
            {QStringLiteral("excluded_tag_ids[]"), {QStringLiteral("910")}},
            {QStringLiteral("rating_ids"), {QStringLiteral("913")}},
            {QStringLiteral("language_id"), {QStringLiteral("et")}},
            {QStringLiteral("complete"), {QStringLiteral("T")}},
            {QStringLiteral("word_count"), {QStringLiteral(">10000")}},
            {QStringLiteral("revised_at"), {QStringLiteral(">2025-01-01")}},
            {QStringLiteral("comments_count"), {QStringLiteral(">10")}},
            {QStringLiteral("bookmarks_count"), {QStringLiteral("50-500")}},
            {QStringLiteral("sort_column"), {QStringLiteral("kudos_count")}},
            {QStringLiteral("sort_direction"), {QStringLiteral("desc")}},
            {QStringLiteral("archive_warning_ids[]"), {QStringLiteral("917")}},
            {QStringLiteral("category_ids[]"), {QStringLiteral("923")}}
        };
        window.m_filterPanel->setFilters(filters);
        QCOMPARE(window.m_filterPanel->filters(), filters);
        if (qEnvironmentVariableIsSet("AO3_CAPTURE_FILTERS")) {
            auto *tabs = window.m_filterPanel->findChild<QTabWidget *>(QStringLiteral("filterTabs"));
            for (int i = 0; i < tabs->count(); ++i) {
                tabs->setCurrentIndex(i);
                QTest::qWait(10);
                window.grab().save(qEnvironmentVariable("AO3_CAPTURE_FILTERS") + QString::number(i) + QStringLiteral(".png"));
            }
            tabs->setCurrentIndex(0);
        }
        // The panel is a child of the main window and cannot float into a popup.
        QVERIFY(!window.m_filterPanel->isWindow());
        QCOMPARE(window.m_filterDock->widget(), window.m_filterPanel);
        QVERIFY(!(window.m_filterDock->features() & QDockWidget::DockWidgetFloatable));
        network->page = R"(<ol class='work index'></ol><ol class='pagination'><a href='/works/search?page=2'>2</a></ol>)";
        auto *apply = window.m_filterPanel->findChild<QPushButton *>(QStringLiteral("applyFilters"));
        QVERIFY(apply);
        QTest::mouseClick(apply, Qt::LeftButton);
        QTRY_COMPARE(window.m_currentResult.totalPages, 2);
        QCOMPARE(window.m_currentFilters, filters);
        auto verify = [&] {
            const QUrlQuery query(network->requests.last());
            for (auto it = filters.cbegin(); it != filters.cend(); ++it) {
                QString key = it.key(); const bool array = key.endsWith(QStringLiteral("[]"));
                if (array) key.chop(2);
                QCOMPARE(query.allQueryItemValues(QStringLiteral("work_search[%1]%2").arg(key, array ? QStringLiteral("[]") : QString())), it.value());
            }
        };
        verify();
        window.goToPage(2);
        QTRY_COMPARE(QUrlQuery(network->requests.last()).queryItemValue(QStringLiteral("page")), QStringLiteral("2"));
        verify();
        // Draft edits do not affect pagination until applied.
        const int requests = network->requests.size();
        window.m_filterPanel->findChild<QLineEdit *>(QStringLiteral("comments_count"))->setText(QStringLiteral(">99"));
        QCOMPARE(network->requests.size(), requests);
        QCOMPARE(window.m_currentFilters, filters);
        window.goToPage(1);
        verify();
        // Header Search also reads the visible filter fields.
        window.m_searchInput->setText(QStringLiteral("updated"));
        QTest::mouseClick(window.m_searchButton, Qt::LeftButton);
        QCOMPARE(QUrlQuery(network->requests.last()).queryItemValue(QStringLiteral("work_search[comments_count]")), QStringLiteral(">99"));
        QTest::mouseClick(window.m_filterPanel->findChild<QPushButton *>(QStringLiteral("resetFilters")), Qt::LeftButton);
        QVERIFY(window.m_currentFilters.isEmpty());
        const QUrlQuery cleared(network->requests.last());
        QVERIFY(!cleared.hasQueryItem(QStringLiteral("work_search[rating_ids]")));
        // A choices failure is visible and does not silently replace current selections.
        network->page = "<p>Unavailable</p>";
        window.m_client->fetchSearchOptions();
        QTRY_COMPARE(options.count(), 2);
        QVERIFY(!window.m_filterChoicesLoaded);
    }
    void tagSuggestions() {
        MainWindow window;
        auto *network = installNetwork(window);
        window.show();
        QTest::qWait(30);QTest::mouseClick(window.m_filterButton,Qt::LeftButton);
        auto *tabs = window.m_filterPanel->findChild<QTabWidget *>(QStringLiteral("filterTabs"));
        tabs->setCurrentIndex(1);
        auto *edit = window.m_filterPanel->findChild<TagLineEdit *>(QStringLiteral("fandom_names"));
        QVERIFY(edit);
        edit->setFocus();
        network->page = R"([{"id":"tag1","name":"Star Wars"},{"id":"tag2","name":"Star Trek"},{"name":"Star Wars"}])";
        QSignalSpy requested(window.m_filterPanel, &SearchFilterPanel::tagSuggestionsRequested);
        QSignalSpy fetched(window.m_client, &AO3Client::tagSuggestionsFetched);
        QSignalSpy searched(window.m_client, &AO3Client::searchCompleted);
        const int before = network->requests.size();
        QTest::keyClicks(edit, "Sta");
        QCOMPARE(network->requests.size(), before);
        QTRY_COMPARE(fetched.count(), 1);
        QCOMPARE(requested.count(), 1);
        QCOMPARE(network->requests.last().path(), QStringLiteral("/autocomplete/fandom"));
        QCOMPARE(QUrlQuery(network->requests.last()).queryItemValue(QStringLiteral("term")), QStringLiteral("Sta"));
        QTRY_VERIFY(edit->completer()->popup()->isVisible());
        QCOMPARE(edit->completer()->completionModel()->rowCount(), 2);
        auto *popup = edit->completer()->popup();
        popup->setCurrentIndex(edit->completer()->completionModel()->index(0, 0));
        QTest::keyClick(popup, Qt::Key_Return);
        QCOMPARE(edit->text(), QStringLiteral("Star Wars"));
        QCOMPARE(searched.count(), 0);
        // Completing a second tag preserves the first, and a later suffix.
        edit->setText(QStringLiteral("Existing, St, Later"));
        edit->setCursorPosition(12);
        edit->setFocus();
        QTest::keyClicks(edit, "a");
        QTRY_COMPARE(fetched.count(), 2);
        QCOMPARE(QUrlQuery(network->requests.last()).queryItemValue(QStringLiteral("term")), QStringLiteral("Sta"));
        const auto secondTag = edit->completer()->completionModel()->index(1, 0);
        QTest::mouseClick(popup->viewport(), Qt::LeftButton, Qt::NoModifier, popup->visualRect(secondTag).center());
        QCOMPARE(edit->text(), QStringLiteral("Existing, Star Trek, Later"));
        // A stale response after editing cannot replace the new dropdown contents.
        const quint64 oldRevision = requested.last().at(3).toULongLong();
        edit->setText(QStringLiteral("Different"));
        window.m_filterPanel->showTagSuggestions(QStringLiteral("fandom_names"), oldRevision, {QStringLiteral("Stale")}, {});
        QVERIFY(!popup->isVisible());
        // Character/relationship/freeform/excluded fields use the matching AO3 services.
        const QMap<QString, QString> types = {{"character_names", "character"}, {"relationship_names", "relationship"}, {"freeform_names", "freeform"}, {"excluded_tag_names", "tag"}};
        for (auto it = types.cbegin(); it != types.cend(); ++it) {
            tabs->setCurrentIndex(it.key() == QStringLiteral("excluded_tag_names") ? 3 : 1);
            auto *field = window.m_filterPanel->findChild<TagLineEdit *>(it.key());
            QVERIFY(field);
            field->setFocus();
            const int count = fetched.count();
            QTest::keyClicks(field, "ab");
            QTRY_COMPARE(fetched.count(), count + 1);
            QCOMPARE(network->requests.last().path(), QStringLiteral("/autocomplete/") + it.value());
            field->completer()->popup()->hide();
        }
        tabs->setCurrentIndex(1);
        edit->setFocus();
        edit->clear();
        const int count = fetched.count();
        QTest::keyClicks(edit, "x");
        QTest::qWait(350);
        QCOMPARE(fetched.count(), count);
        network->page = "<p>Unavailable</p>";
        QTest::keyClicks(edit, "y");
        QTRY_COMPARE(fetched.count(), count + 1);
        QVERIFY(!popup->isVisible());
        QCOMPARE(edit->text(), QStringLiteral("xy"));
        QVERIFY(edit->toolTip().contains(QStringLiteral("manually")));
    }
    void readerNavigationTranslationAndSettings() {
        QSettings().setValue("translation/url","https://translator.example");
        QSettings().setValue("translation/consented",true);
        MainWindow window;
        installNetwork(window);
        window.show(); QTest::qWait(30);QTest::mouseClick(window.m_filterButton,Qt::LeftButton);QVERIFY(!window.m_filterDock->isHidden());
        const QString html = QStringLiteral("<div id='chapters'><div class='chapter'><div class='chapter preface group'><h3 class='title'>Chapter 1: Beginning</h3></div><div class='userstuff module'><p>Bonjour le monde.</p></div></div><div class='chapter'><div class='chapter preface group'><h3 class='title'>Chapter 2: Ending</h3></div><div class='userstuff module'><p>Deuxième chapitre.</p></div></div></div>");
        const auto chapters = extractWorkChapters(html);
        QCOMPARE(chapters.size(), 2);
        QCOMPARE(chapters.first().title, QStringLiteral("Chapter 1: Beginning"));
        Work work; work.id = QStringLiteral("42"); work.title = QStringLiteral("A spacious reader"); work.author = QStringLiteral("Author");
        window.openReader(work, chapters);
        auto *reader = window.m_reader;
        QCOMPARE(window.m_contentStack->currentWidget(), reader);
        QVERIFY(window.m_filterDock->isHidden()); QVERIFY(window.m_headerBar->isHidden());
        QVERIFY(reader->m_bottomNext->isVisible()); QVERIFY(reader->m_desktopOptions->isHidden());
        QVERIFY(!reader->m_previous->isEnabled()); QVERIFY(reader->m_next->isEnabled());
        QVERIFY(reader->m_browser->toPlainText().contains(QStringLiteral("Bonjour")));
        QTest::mouseClick(reader->m_bottomNext, Qt::LeftButton);
        QCOMPARE(reader->m_index, 1);
        QVERIFY(reader->m_previous->isEnabled()); QVERIFY(!reader->m_next->isEnabled());
        QVERIFY(reader->m_browser->toPlainText().contains(QStringLiteral("Deuxième")));
        QTest::mouseClick(reader->m_bottomPrevious, Qt::LeftButton);
        QCOMPARE(reader->m_index, 0);
        window.toggleReaderFullscreen(); QVERIFY(window.isFullScreen());
        QTest::keyClick(&window, Qt::Key_Escape); QTRY_VERIFY(!window.isFullScreen());
        QTest::mouseClick(reader->m_desktopOptionsToggle,Qt::LeftButton);
        QVERIFY(reader->m_translate->isVisible());
        // The source text stays available after translating and after service errors.
        auto *translator = reader->m_translator;
        delete translator->m_manager;
        auto *network = new FixtureNetwork; network->setParent(translator); translator->m_manager = network;
        network->page = R"({"translatedText":"Hello world."})";
        QSignalSpy completed(translator, &TranslationClient::completed);
        QTest::mouseClick(reader->m_translate, Qt::LeftButton);
        QTRY_COMPARE(completed.count(), 1);
        QVERIFY(reader->m_browser->toPlainText().contains(QStringLiteral("Hello world.")));
        const auto sent = QJsonDocument::fromJson(network->bodies.last()).object();
        QCOMPARE(sent.value(QStringLiteral("source")).toString(), QStringLiteral("auto"));
        QCOMPARE(sent.value(QStringLiteral("q")).toString(), QStringLiteral("Bonjour le monde."));
        QCOMPARE(network->requests.last().path(), QStringLiteral("/translate"));
        QTest::mouseClick(reader->m_original, Qt::LeftButton);
        QVERIFY(reader->m_browser->toPlainText().contains(QStringLiteral("Bonjour")));
        // Late translation completion after changing chapters is ignored.
        reader->m_translations.clear(); reader->translateChapter(); reader->showChapter(1);
        QTest::qWait(30);
        QVERIFY(reader->m_browser->toPlainText().contains(QStringLiteral("Deuxième")));
        network->page = R"({"error":"Language unavailable"})";
        reader->translateChapter();
        QTRY_VERIFY(reader->m_status->text().contains(QStringLiteral("Language unavailable")));
        QVERIFY(reader->m_browser->toPlainText().contains(QStringLiteral("Deuxième")));
        // Save the default language using the settings UI itself.
        SearchOptions options;
        options.choices[QStringLiteral("language_id")] = {{QStringLiteral("English"), QStringLiteral("en")}, {QStringLiteral("Français"), QStringLiteral("fr")}};
        window.m_filterPanel->setOptions(options);
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()); QVERIFY(dialog);
            auto *language = dialog->findChild<QComboBox *>(QStringLiteral("defaultReadingLanguage")); QVERIFY(language);
            language->setCurrentIndex(language->findData(QStringLiteral("fr")));
            dialog->findChild<QCheckBox *>(QStringLiteral("filterDefaultLanguage"))->setChecked(true);
            dialog->findChild<QLineEdit *>(QStringLiteral("translationServiceUrl"))->setText(QStringLiteral("https://translator.example"));
            QTest::mouseClick(dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save), Qt::LeftButton);
        });
        reader->showSettings();
        QCOMPARE(QSettings().value(QStringLiteral("reader/language")).toString(), QStringLiteral("fr"));
        QCOMPARE(reader->targetLanguage(), QStringLiteral("fr"));
        QCOMPARE(window.m_filterPanel->filters().value(QStringLiteral("language_id")), QStringList{QStringLiteral("fr")});
        ReaderWidget restored;
        QCOMPARE(restored.targetLanguage(), QStringLiteral("fr"));
        if (qEnvironmentVariableIsSet("AO3_CAPTURE_READER")) window.grab().save(qEnvironmentVariable("AO3_CAPTURE_READER"));
        window.closeReader();
        QCOMPARE(window.m_contentStack->currentIndex(), 2); QVERIFY(!window.m_headerBar->isHidden()); QVERIFY(!window.m_filterDock->isHidden());
        reader->openWork(work, {chapters.first()});
        QVERIFY(!reader->m_previous->isEnabled()); QVERIFY(!reader->m_next->isEnabled());
        QVERIFY(!reader->m_bottomPrevious->isEnabled()); QVERIFY(!reader->m_bottomNext->isEnabled());
        reader->clear();
        QSettings().clear();
    }
    void curlSheetOccludesUnderPage() {
        BookView view; view.resize(460,680); view.show(); QTest::qWait(30);
        const auto capture=[&](const QString &color,int time) {
            QTextDocument source; source.setDefaultFont(QFont("serif",14));
            source.setHtml("<h1>OPAQUE PAPER</h1><p>"+QString("Only this page should be visible on its front. ").repeated(8)+"</p><p style='page-break-before:always;background-color:"+color+"'>"+QString("UNDERNEATH UNDERNEATH UNDERNEATH ").repeated(150)+"</p>");
            view.setContent(&source,0,QColor("#fff7e7"),QColor("#28221b"));
            view.turn(1); view.animation.pause(); view.animation.setCurrentTime(time);
            return view.grab().toImage();
        };
        for(int time: {70,150,240,300}) {
            const auto red=capture("#ff0000",time),blue=capture("#0000ff",time);
            int covered=0,exposedDifferences=0;
            for(int y=3;y<red.height()-3;y+=3) for(int x=3;x<red.width()-3;x+=3) {
                if(view.lastPaperMask.contains(QRectF(x-1,y-1,2,2))) { ++covered; QCOMPARE(red.pixel(x,y),blue.pixel(x,y)); }
                else if(red.pixel(x,y)!=blue.pixel(x,y)) ++exposedDifferences;
            }
            QVERIFY(covered>100); QVERIFY(exposedDifferences>0);
        }
    }
    void paperCurlFrames() {
        BookView view; view.resize(460,680); view.show();
        QTextDocument source; source.setDefaultFont(QFont("serif",14));
        source.setHtml("<h1>THE FIRST PAGE</h1><p>"+QString("The paper bends while the next page stays in place. ").repeated(12)+"</p><h1 style='page-break-before:always'>THE NEXT PAGE</h1><p>"+QString("This is the next sheet underneath the turning page. ").repeated(12)+"</p>");
        view.setContent(&source,0,QColor("#fff7e7"),QColor("#28221b"));
        QVERIFY(view.pages()>1);
        const auto initial=view.grab().toImage();
        view.turn(1); view.animation.pause();
        view.animation.setCurrentTime(160); const auto middle=view.grab().toImage();
        QVERIFY(middle!=initial);
        if(qEnvironmentVariableIsSet("AO3_CAPTURE_CURL")) {
            QDir().mkpath("/tmp/ao3-curl-frames");
            for(int i=0;i<30;++i) { view.animation.setCurrentTime(i*419/29); view.grab().save(QString("/tmp/ao3-curl-frames/frame%1.png").arg(i,3,10,QChar('0'))); }
        }
        view.animation.setCurrentTime(420); const auto final=view.grab().toImage();
        QVERIFY(final!=initial); QVERIFY(final!=middle);
        view.turn(-1); view.animation.pause(); view.animation.setCurrentTime(160);
        QVERIFY(view.grab().toImage()!=final);
        view.animation.setCurrentTime(420); QCOMPARE(view.grab().toImage(),initial);
    }
    void textOnlyAndBookMode() {
        QSettings().setValue("reader/bookMode",false);
        ReaderWidget reader; reader.resize(900,800); reader.show();
        Work work; work.id="54321"; work.title="Book mode regression";
        reader.openWork(work, {{"Chapter one",QString("A paragraph of readable words. ").repeated(2500)}});
        QTest::qWait(40);
        reader.findChild<QCheckBox *>("bookMode")->setChecked(true); QTest::qWait(40);
        QVERIFY(reader.m_book->isVisible()); QVERIFY(reader.m_browser->isHidden());
        QVERIFY(reader.m_book->pages()>2); QCOMPARE(reader.m_book->page(),0);
        reader.m_book->animated=false; reader.m_book->turn(1); QCOMPARE(reader.m_book->page(),1);
        reader.saveProgress(); QVERIFY(QSettings().value("readingProgress/54321/positions/0").toDouble()>0);
        reader.setTextOnly(true); QTest::qWait(30);
        QVERIFY(reader.m_title->isHidden()); QVERIFY(reader.m_chapter->isHidden());
        QVERIFY(reader.m_book->height()>650);
        QVERIFY(reader.closeOptions()); QVERIFY(!reader.m_textOnly); QVERIFY(reader.m_title->isVisible());
        reader.m_book->animated=true; reader.m_book->turn(1); QTest::qWait(450);
        QVERIFY(reader.m_book->page()>0);
        reader.findChild<QCheckBox *>("bookMode")->setChecked(false);
        reader.setTextOnly(true); QVERIFY(reader.m_browser->isVisible());
        QTest::mouseDClick(reader.m_browser->viewport(),Qt::LeftButton); QVERIFY(!reader.m_textOnly);
        QSettings().setValue("reader/bookMode",false);
        if(qEnvironmentVariableIsSet("AO3_CAPTURE_BOOK")) { reader.setBookMode(true); reader.grab().save("/tmp/ao3-book-mode.png"); reader.setBookMode(false); }
    }
    void offlineSpeechCanPauseResumeAndStopItsOwnProcess() {
#ifdef Q_OS_UNIX
        QTemporaryDir temporary;QVERIFY(temporary.isValid());QFile script(temporary.filePath("speech"));QVERIFY(script.open(QIODevice::WriteOnly));script.write("#!/usr/bin/python3\nimport sys,time\nsys.stdin.read()\nwhile True:\n sys.stdout.write('.');sys.stdout.flush();time.sleep(.03)\n");script.close();QVERIFY(script.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner));
        ReaderSpeech speech(nullptr,script.fileName());speech.start("Reader text with shell characters $() and quotes.","en",175);QTRY_VERIFY(speech.process && speech.process->state()==QProcess::Running);auto *process=speech.process;QTRY_VERIFY(process->bytesAvailable()>0);speech.pauseOrResume();QVERIFY(speech.paused());QTest::qWait(70);process->readAllStandardOutput();QTest::qWait(120);QVERIFY(process->readAllStandardOutput().isEmpty());speech.pauseOrResume();QVERIFY(!speech.paused());QTRY_VERIFY(process->bytesAvailable()>0);QPointer<QProcess> stopped=process;speech.stop();QVERIFY(!speech.active());QTRY_VERIFY(!stopped || stopped->state()==QProcess::NotRunning);
        speech.start("Text","bad;language",175);QVERIFY(!speech.active());QVERIFY(!speech.error.isEmpty());
#else
        QSKIP("Desktop speech uses the local Unix eSpeak engine.");
#endif
    }
    void tapZonesCanReverseDisableAndKeepKeyboardEscape() {
        QSettings settings;QMap<QString,QVariant> old;for(const auto &key:QStringList{"tapLeft","tapCenter","tapRight","tapEdge"})old[key]=settings.value("reader/"+key);auto restore=qScopeGuard([&]{for(auto i=old.begin();i!=old.end();++i)if(i.value().isValid())settings.setValue("reader/"+i.key(),i.value());else settings.remove("reader/"+i.key());});
        settings.setValue("reader/tapLeft",1);settings.setValue("reader/tapRight",-1);settings.setValue("reader/tapCenter",0);settings.setValue("reader/tapEdge",35);
        ReaderWidget reader;reader.resize(900,700);reader.show();reader.setBookMode(true);Work work;work.id="774423";reader.openWork(work,{{"One",QString("Long reading text. ").repeated(5000)}});QTest::qWait(70);QVERIFY(reader.m_book->pages()>2);reader.m_book->animated=false;const auto point=QPoint(10,reader.m_book->height()/2);QTest::mouseClick(reader.m_book,Qt::LeftButton,Qt::NoModifier,point);QCOMPARE(reader.m_book->page(),1);QTest::mouseClick(reader.m_book,Qt::LeftButton,Qt::NoModifier,QPoint(reader.m_book->width()/2,reader.m_book->height()/2));QCOMPARE(reader.m_book->page(),1);QTest::mouseClick(reader.m_book,Qt::LeftButton,Qt::NoModifier,QPoint(reader.m_book->width()-10,reader.m_book->height()/2));QCOMPARE(reader.m_book->page(),0);reader.setTextOnly(true);QTest::keyClick(reader.m_book,Qt::Key_F10);QTRY_VERIFY(!reader.m_textOnly);reader.setBookMode(false);
    }
    void autoScrollAdvancesAndStopsAtReadingBoundaries() {
        ReaderWidget reader;reader.resize(900,700);reader.show();Work work;work.id="774422";work.title="Auto-scroll";
        QSettings().remove("readingProgress/774422");reader.openWork(work,{{"One",QString("A paragraph with enough reading text.\n\n").repeated(180)},{"Two","Second chapter."}});QTest::qWait(80);
        auto *bar=reader.m_browser->verticalScrollBar();QVERIFY(bar->maximum()>100);bar->setValue(0);reader.m_autoScrollSpeed->setValue(120);reader.m_autoScroll->setChecked(true);
        QTRY_VERIFY(bar->value()>5);QVERIFY(reader.m_autoScrollTimer.isActive());
        reader.showChapter(1);QVERIFY(!reader.m_autoScroll->isChecked());QVERIFY(!reader.m_autoScrollTimer.isActive());
        reader.showChapter(0);QTest::qWait(50);reader.m_autoScroll->setChecked(true);reader.setBookMode(true);QVERIFY(!reader.m_autoScroll->isChecked());QVERIFY(!reader.m_autoScroll->isEnabled());
        reader.setBookMode(false);QTest::qWait(50);bar->setValue(bar->maximum());reader.m_autoScroll->setChecked(true);QTRY_VERIFY(!reader.m_autoScroll->isChecked());
        bar->setValue(0);reader.m_autoScroll->setChecked(true);reader.hide();QVERIFY(!reader.m_autoScrollTimer.isActive());
    }
    void importedInterfaceFontUpdatesExistingChromeAndWorkPreview() {
        QSettings settings;const auto previous=settings.value("reader/fontFamily"),enabled=settings.value("reader/useImportedFontInInterface");
        QString id;auto restore=qScopeGuard([&]{if(previous.isValid())settings.setValue("reader/fontFamily",previous);else settings.remove("reader/fontFamily");if(enabled.isValid())settings.setValue("reader/useImportedFontInInterface",enabled);else settings.remove("reader/useImportedFontInInterface");applyReaderInterfaceFont();if(!id.isEmpty())removeReaderFont(id);});
        settings.setValue("reader/fontFamily","serif");applyReaderInterfaceFont();const auto systemFamily=QApplication::font().family();
        MainWindow window;installNetwork(window);window.resize(1100,850);window.show();
        Work work;work.id="123";work.title="A preview title";work.author="Writer";work.summary="A summary shown before opening the fic.";window.m_detailWidget->showWork(work);
        auto *name=window.findChild<QLabel *>("appName");QVERIFY(name);const auto size=name->font().pointSizeF();const bool bold=name->font().bold();
        if(qEnvironmentVariableIsSet("AO3_CAPTURE_INTERFACE_FONT"))window.grab().save("/tmp/ao3-interface-font-before.png");
        QFile fixture(QStringLiteral(AO3_FIXTURE_DIR "/reader-test-font.ttf"));QVERIFY(fixture.open(QIODevice::ReadOnly));QString error;id=importReaderFont(fixture.readAll(),error);QVERIFY2(!id.isEmpty(),qPrintable(error));
        settings.setValue("reader/fontFamily",id);settings.setValue("reader/useImportedFontInInterface",true);applyReaderInterfaceFont();QTest::qWait(30);
        QCOMPARE(QFontInfo(window.m_searchButton->font()).family(),QString("Reader Test Font"));QCOMPARE(QFontInfo(name->font()).family(),QString("Reader Test Font"));QCOMPARE(name->font().pointSizeF(),size);QCOMPARE(name->font().bold(),bold);
        bool summary=false;for(auto *browser:window.m_detailWidget->findChildren<QTextBrowser *>())if(browser->toPlainText().contains(work.summary)){summary=true;QCOMPARE(QFontInfo(browser->document()->defaultFont()).family(),QString("Reader Test Font"));}QVERIFY(summary);
        if(qEnvironmentVariableIsSet("AO3_CAPTURE_INTERFACE_FONT"))window.grab().save("/tmp/ao3-interface-font-after.png");
        settings.setValue("reader/useImportedFontInInterface",false);applyReaderInterfaceFont();QTest::qWait(30);QCOMPARE(window.m_searchButton->font().family(),systemFamily);QCOMPARE(readerFont(id).family(),QString("Reader Test Font"));
        settings.setValue("reader/useImportedFontInInterface",true);removeReaderFont(id);applyReaderInterfaceFont();QCOMPARE(QApplication::font().family(),systemFamily);id.clear();
    }
    void importedFontsAreValidatedCopiedAndUsedByTheReader() {
        QSettings settings;const auto previous=settings.value("reader/fontFamily");auto restore=qScopeGuard([&]{if(previous.isValid())settings.setValue("reader/fontFamily",previous);else settings.remove("reader/fontFamily");});
        QFile fixture(QStringLiteral(AO3_FIXTURE_DIR "/reader-test-font.ttf"));QVERIFY(fixture.open(QIODevice::ReadOnly));const auto bytes=fixture.readAll();QString error;
        QVERIFY(importReaderFont("not a font",error).isEmpty());QVERIFY(importReaderFont(QByteArray(20*1024*1024+1,'x'),error).isEmpty());
        const auto id=importReaderFont(bytes,error);QVERIFY2(readerFontId(id),qPrintable(error));QCOMPARE(importReaderFont(bytes,error),id);QCOMPARE(readerFonts().size(),1);
        settings.setValue("reader/fontFamily",id);ReaderWidget reader;reader.resize(900,800);reader.show();Work work;work.title="Custom font";const QString text=QString("A font preview with several words. ").repeated(70);reader.openWork(work,{{"One",text}});QTest::qWait(30);
        QCOMPARE(QFontInfo(reader.m_browser->document()->defaultFont()).family(),QString("Reader Test Font"));QCOMPARE(reader.m_browser->toPlainText(),text);
        const auto path=QDir(readerFontDirectory()).filePath(id.mid(5)+".font");QVERIFY(QFile::exists(path));
        const auto registered=loadedReaderFonts().take(path);QFontDatabase::removeApplicationFont(registered.registration);
        QCOMPARE(readerFonts().size(),1);QCOMPARE(readerFont(id).family(),QString("Reader Test Font"));
        QVERIFY(!removeReaderFont("font:../../outside"));QVERIFY(removeReaderFont(id));QVERIFY(readerFonts().isEmpty());QCOMPARE(readerFont(id).family(),QString("serif"));
        reader.renderText(text);QCOMPARE(reader.m_browser->document()->defaultFont().family(),QString("serif"));
    }
    void readerMarginsAndAlignmentReflowBothModes() {
        QSettings settings;const auto oldMargin=settings.value("reader/margin"),oldAlignment=settings.value("reader/alignment");
        auto restore=qScopeGuard([&]{if(oldMargin.isValid())settings.setValue("reader/margin",oldMargin);else settings.remove("reader/margin");if(oldAlignment.isValid())settings.setValue("reader/alignment",oldAlignment);else settings.remove("reader/alignment");});
        settings.setValue("reader/margin",0);settings.setValue("reader/alignment","left");
        ReaderWidget reader;reader.resize(900,800);reader.show();Work work;work.title="Layout";
        const QString text=QString("A paragraph with enough words to wrap across the reading area. ").repeated(150);
        reader.openWork(work,{{"One",text}});QTest::qWait(40);const auto width=reader.m_browser->viewport()->width();
        settings.setValue("reader/margin",64);settings.setValue("reader/alignment","right");reader.renderText(text);QTest::qWait(40);
        QVERIFY(reader.m_browser->viewport()->width()<width-100);QCOMPARE(reader.m_browser->document()->begin().blockFormat().alignment(),Qt::AlignRight);
        reader.setBookMode(true);QTest::qWait(40);const auto pages=reader.m_book->pages();QVERIFY(pages>1);
        reader.m_book->animated=false;reader.m_book->turn(1);const auto anchor=reader.m_book->anchor();QVERIFY(anchor>0);
        settings.setValue("reader/margin",0);settings.setValue("reader/alignment","justify");reader.renderText(text);QTest::qWait(40);
        QVERIFY(reader.m_book->pages()<=pages);QVERIFY(reader.m_book->anchor()<=anchor);QCOMPARE(reader.m_browser->toPlainText(),text);
        QCOMPARE(reader.m_browser->document()->begin().blockFormat().alignment(),Qt::AlignJustify);reader.setBookMode(false);
    }
    void systemThemeResolutionPreservesCustomSkin() {
        QSettings settings;const auto prior=settings.value("appearance/followSystem"),custom=settings.value("appearance/skin");
        auto restore=qScopeGuard([&]{if(prior.isValid())settings.setValue("appearance/followSystem",prior);else settings.remove("appearance/followSystem");if(custom.isValid())settings.setValue("appearance/skin",custom);else settings.remove("appearance/skin");});
        settings.setValue("appearance/skin",AppSkin::preset(2).json());settings.setValue("appearance/followSystem",true);
        QCOMPARE(resolvedAppSkin(true).background,AppSkin::preset(1).background);QCOMPARE(resolvedAppSkin(false).background,AppSkin::preset(0).background);
        settings.setValue("appearance/followSystem",false);QCOMPARE(resolvedAppSkin(true).background,AppSkin::preset(2).background);
    }
    void appearanceChangesRenderedLayout() {
        QSettings settings;
        settings.setValue("reader/lineSpacing", 120);
        settings.setValue("reader/fontFamily", "serif");
        settings.setValue("reader/fontSize", 15);
        ReaderWidget reader; reader.resize(900, 800); reader.show();
        Work work; work.title = "Appearance regression";
        const QString text = QString("A long paragraph with enough words to wrap over several lines. ").repeated(80) + "\n\nSecond paragraph.";
        reader.openWork(work, {{"Chapter one", text}}); QTest::qWait(30);
        const auto initialHeight = reader.m_browser->document()->size().height();
        const auto serif = QFontInfo(reader.m_browser->document()->defaultFont()).family();
        QTimer::singleShot(0, &reader, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()); QVERIFY(dialog);
            dialog->findChild<QSpinBox *>("readingLineSpacing")->setValue(220);
            dialog->findChild<QComboBox *>("readingFontFamily")->setCurrentIndex(1);
            dialog->findChild<QComboBox *>("readingTheme")->setCurrentIndex(2);
            QCOMPARE(reader.m_browser->document()->begin().blockFormat().lineHeight(),220.0);
            QVERIFY(reader.m_browser->styleSheet().contains("#191b1e"));
            QVERIFY(dialog->findChild<QTextBrowser *>("readingPreview"));
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        });
        reader.showSettings(); QTest::qWait(30);
        QCOMPARE(reader.m_browser->toPlainText(), text);
        QCOMPARE(reader.m_browser->document()->begin().blockFormat().lineHeight(), 220.0);
        QVERIFY(reader.m_browser->document()->size().height() > initialHeight * 1.4);
        QVERIFY(QFontInfo(reader.m_browser->document()->defaultFont()).family() != serif);
        QVERIFY(reader.m_browser->styleSheet().contains("#191b1e"));
        const auto height = reader.m_browser->document()->size().height();
        reader.m_fontSize->setValue(24); QTest::qWait(30);
        QCOMPARE(reader.m_browser->document()->defaultFont().pointSize(), 24);
        QVERIFY(reader.m_browser->document()->size().height() > height * 1.3);
        QCOMPARE(reader.m_browser->toPlainText(), text);
        if(qEnvironmentVariableIsSet("AO3_CAPTURE_APPEARANCE")) reader.grab().save("/tmp/ao3-appearance-fixed.png");
    }
    void readingSettingsDarkThemeAndNarrowLayout() {
        const QPalette original = QApplication::palette();
        const auto restore = qScopeGuard([&] { QApplication::setPalette(original); });
        QPalette dark = AppSkin::preset(1).palette();
        QApplication::setPalette(dark);
        ReaderWidget reader;
        Work work; work.title = QStringLiteral("A long title that should wrap without clipping the reading controls or chapter bar");
        reader.resize(780, 800);
        reader.openWork(work, {{QStringLiteral("A long chapter title that should fit its own row"), QStringLiteral("Readable text.")}});
        reader.m_status->setText(QStringLiteral("A long translation status message must wrap without pushing buttons or the language selector out of view."));
        reader.show(); QTest::qWait(30);
        QVERIFY(reader.rect().contains(reader.m_chapter->geometry()));
        QVERIFY(reader.m_browser->height() > 180);
        QVERIFY(!reader.m_chapter->geometry().intersects(reader.m_next->geometry()));
        QCOMPARE(reader.palette().color(QPalette::WindowText),dark.color(QPalette::WindowText));
        if (qEnvironmentVariableIsSet("AO3_CAPTURE_THEME")) reader.grab().save(QStringLiteral("/tmp/ao3-reader-narrow.png"));
        QTimer::singleShot(0, &reader, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()); QVERIFY(dialog);
            QTest::qWait(30);
            QVERIFY(dialog->width() >= 540);
            QCOMPARE(dialog->palette().color(QPalette::Window),dark.color(QPalette::Window));
            QCOMPARE(dialog->palette().color(QPalette::WindowText),dark.color(QPalette::WindowText));
            auto *url = dialog->findChild<QLineEdit *>(QStringLiteral("translationServiceUrl")); QVERIFY(url);
            QVERIFY(url->width() >= 280);
            if (qEnvironmentVariableIsSet("AO3_CAPTURE_THEME")) dialog->grab().save(QStringLiteral("/tmp/ao3-settings-darktheme.png"));
            dialog->reject();
        });
        reader.showSettings();
    }
    void readingProgressPersists() {
        Work work; work.id = QStringLiteral("98765"); work.title = QStringLiteral("Progress test");
        QString longText;
        for (int i = 0; i < 300; ++i) longText += QStringLiteral("Paragraph %1. Reading progress should survive closing the reader.\n\n").arg(i);
        const QVector<WorkChapter> chapters = {{QStringLiteral("One"), longText}, {QStringLiteral("Two"), longText}};
        {
            ReaderWidget reader; reader.resize(900, 850); reader.show();
            reader.openWork(work, chapters);
            QTRY_VERIFY(!reader.m_restoringPosition);
            auto *scroll = reader.m_browser->verticalScrollBar();
            QVERIFY(scroll->maximum() > 0);
            scroll->setValue(qRound(scroll->maximum() * 0.6));
            QTRY_VERIFY(QSettings().value(QStringLiteral("readingProgress/98765/positions/0")).toDouble() > 0.59);
            reader.showChapter(1);
            QTRY_VERIFY(!reader.m_restoringPosition);
            QCOMPARE(scroll->value(), 0);
            scroll->setValue(qRound(scroll->maximum() * 0.35));
            reader.saveProgress();
        }
        {
            ReaderWidget resumed; resumed.resize(760, 760); resumed.show();
            resumed.openWork(work, chapters);
            QTRY_VERIFY(!resumed.m_restoringPosition);
            QCOMPARE(resumed.m_index, 1);
            auto *scroll = resumed.m_browser->verticalScrollBar();
            QVERIFY(qAbs(double(scroll->value()) / scroll->maximum() - 0.35) < 0.02);
            resumed.showChapter(0);
            QTRY_VERIFY(!resumed.m_restoringPosition);
            QVERIFY(qAbs(double(scroll->value()) / scroll->maximum() - 0.6) < 0.02);
            // Changing text size preserves the relative reading position.
            resumed.m_fontSize->setValue(20);
            QTRY_VERIFY(!resumed.m_restoringPosition);
            QVERIFY(qAbs(double(scroll->value()) / scroll->maximum() - 0.6) < 0.02);
            // The same content under another work ID has independent progress.
            Work other = work; other.id = QStringLiteral("98766");
            resumed.openWork(other, chapters);
            QTRY_VERIFY(!resumed.m_restoringPosition);
            QCOMPARE(resumed.m_index, 0); QCOMPARE(scroll->value(), 0);
            resumed.openWork(work, chapters);
            QTRY_VERIFY(!resumed.m_restoringPosition);
            QVERIFY(scroll->value() > 0);
            QTest::mouseClick(resumed.findChild<QPushButton *>(QStringLiteral("restartReading")), Qt::LeftButton);
            QTRY_VERIFY(!resumed.m_restoringPosition);
            QCOMPARE(resumed.m_index, 0); QCOMPARE(scroll->value(), 0);
            // Saved chapter indices are clamped if chapters have been removed.
            QSettings().setValue(QStringLiteral("readingProgress/98765/chapter"), 99);
            resumed.clear();
        }
        QSettings().setValue(QStringLiteral("readingProgress/98765/chapter"), 99);
        ReaderWidget shortened; shortened.show(); shortened.openWork(work, {chapters.first()});
        QTRY_VERIFY(!shortened.m_restoringPosition);
        QCOMPARE(shortened.m_index, 0);
        shortened.clear();
        QSettings().remove(QStringLiteral("readingProgress/98765"));
        QSettings().remove(QStringLiteral("readingProgress/98766"));
        QSettings().remove(QStringLiteral("reader/fontSize"));
    }
    void translationChunking() {
        TranslationClient translator;
        delete translator.m_manager;
        auto *network = new FixtureNetwork; network->setParent(&translator); translator.m_manager = network;
        network->page = R"({"translatedText":"Translated chunk"})";
        QSignalSpy completed(&translator, &TranslationClient::completed);
        const QString text = QString(4500, QChar('x')) + QStringLiteral(" ☃ end");
        translator.translate(text, QStringLiteral("en"), QUrl(QStringLiteral("https://translator.example")), {});
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(network->requests.size(), 3);
        QString reconstructed;
        for (const auto &body : network->bodies) reconstructed += QJsonDocument::fromJson(body).object().value(QStringLiteral("q")).toString();
        QCOMPARE(reconstructed, text);
        QSignalSpy failed(&translator, &TranslationClient::failed);
        translator.translate(QStringLiteral("Hello"), QStringLiteral("en"), QUrl(QStringLiteral("not-a-url")), {});
        QCOMPARE(failed.count(), 1);
    }
    void filtersWorkWithoutSite() {
        QSettings().remove(QStringLiteral("filters/catalogue"));
        SearchFilterPanel panel;
        auto *rating = panel.findChild<QComboBox *>(QStringLiteral("rating_ids"));
        auto *language = panel.findChild<QComboBox *>(QStringLiteral("language_id"));
        QVERIFY(rating->findData(QStringLiteral("13")) > 0);
        QVERIFY(language->findData(QStringLiteral("et")) > 0);
        QCOMPARE(panel.findChild<QListWidget *>(QStringLiteral("archive_warning_ids[]"))->count(), 6);
        QCOMPARE(panel.findChild<QListWidget *>(QStringLiteral("category_ids[]"))->count(), 6);
        QCOMPARE(panel.findChild<QListWidget *>(QStringLiteral("excluded_rating_ids[]"))->count(), 5);
        const SearchFilters selected = {{QStringLiteral("rating_ids"), {QStringLiteral("13")}}, {QStringLiteral("language_id"), {QStringLiteral("et")}}, {QStringLiteral("archive_warning_ids[]"), {QStringLiteral("18")}}, {QStringLiteral("excluded_tag_ids[]"), {QStringLiteral("17")}}, {QStringLiteral("fandom_names"), {QStringLiteral("My fandom")}}};
        panel.setFilters(selected);
        QCOMPARE(panel.filters(), selected);
        SearchOptions unavailable; unavailable.error = QStringLiteral("HTTP 418");
        panel.setOptions(unavailable);
        QCOMPARE(panel.filters(), selected);
        QVERIFY(rating->findData(QStringLiteral("13")) > 0);
        // Optional imported choices are cached; incomplete updates never wipe defaults.
        SearchOptions extras; extras.choices[QStringLiteral("language_id")] = {{QStringLiteral("Custom language"), QStringLiteral("xyz")}};
        panel.setOptions(extras);
        SearchFilterPanel restarted;
        QVERIFY(restarted.findChild<QComboBox *>(QStringLiteral("rating_ids"))->findData(QStringLiteral("13")) > 0);
        QVERIFY(restarted.findChild<QComboBox *>(QStringLiteral("language_id"))->findData(QStringLiteral("xyz")) > 0);
        restarted.setFilters({{QStringLiteral("language_id"), {QStringLiteral("custom-code")}}});
        QCOMPARE(restarted.filters().value(QStringLiteral("language_id")), QStringList{QStringLiteral("custom-code")});
        MainWindow window; auto *network = installNetwork(window); QTest::qWait(30);
        // Startup must not contact the search form just to populate controls.
        for (const auto &url : network->requests) QVERIFY(url.path() != QStringLiteral("/works/search"));
        network->status = 418; network->page = "<h2>The archive is down for maintenance.</h2>";
        window.m_filterPanel->setFilters(selected);
        window.m_searchInput->setText(QStringLiteral("test")); window.performSearch();
        QTRY_VERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("maintenance")));
        QCOMPARE(window.m_filterPanel->filters(), selected);
    }
    void maintenanceResponse() {
        MainWindow window;
        auto *network = installNetwork(window);
        QTest::qWait(30);
        network->status = 418;
        network->page = "<title>Request temporarily denied.</title><h2>The archive is down for maintenance.</h2>";
        window.m_searchInput->setText(QStringLiteral("test"));
        window.performSearch();
        QTRY_VERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("down for maintenance")));
        QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("418")));
        QCOMPARE(window.m_workList->count(), 0);
        network->page = "<h2>Request temporarily denied.</h2>";
        window.performSearch();
        QTRY_VERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("temporarily refused")));
        QVERIFY(!window.statusBar()->currentMessage().contains(QStringLiteral("down for maintenance")));
    }
    void publicAuthorBrowsing() {
        MainWindow window;
        auto *network = installNetwork(window);
        QTest::qWait(30);
        auto *client = window.findChild<AO3Client *>();
        QVERIFY(client);
        network->page = R"(<div class="user home profile"><h3 class="heading">Writer profile</h3><dl class="meta"><dt>Joined:</dt><dd>2020</dd></dl><div class="bio"><blockquote class="userstuff"><p>Hello world</p></blockquote></div></div>)";
        QSignalSpy profileSpy(client, &AO3Client::authorProfileFetched);
        client->fetchAuthorProfile("Writer");
        QTRY_COMPARE(profileSpy.size(), 1);
        const auto profile = qvariant_cast<AuthorProfile>(profileSpy.takeFirst().first());
        QVERIFY(profile.error.isEmpty());
        QCOMPARE(profile.username, QStringLiteral("Writer"));
        QCOMPARE(profile.biography, QStringLiteral("Hello world"));
        QCOMPARE(network->requests.last().path(), QStringLiteral("/users/Writer/profile"));
        network->page = R"(<ol class="bookmark index group"><li id="bookmark_999" class="bookmark blurb group"><h4 class="heading"><a href="/works/123">A fic</a> by <a rel="author" href="/users/Writer/pseuds/Writer">Writer</a></h4></li></ol><ol class="pagination"><li><a href="?page=4">4</a></li></ol>)";
        QSignalSpy worksSpy(client, &AO3Client::authorWorksFetched);
        client->fetchAuthorWorks("Writer", 2, true);
        QTRY_COMPARE(worksSpy.size(), 1);
        const auto listing = qvariant_cast<SearchResult>(worksSpy.takeFirst().at(2));
        QVERIFY2(listing.errorMessage.isEmpty(), qPrintable(listing.errorMessage));
        QCOMPARE(listing.works.size(), 1);
        QCOMPARE(listing.works.first().id, QStringLiteral("123"));
        QCOMPARE(listing.works.first().authorUrl, QStringLiteral("/users/Writer/pseuds/Writer"));
        QCOMPARE(listing.totalPages, 4);
        QCOMPARE(network->requests.last().path(), QStringLiteral("/users/Writer/bookmarks"));
        QCOMPARE(QUrlQuery(network->requests.last()).queryItemValue("page"), QStringLiteral("2"));
        network->page = "<h2>Maintenance</h2>";
        client->fetchAuthorProfile("Writer");
        QTRY_COMPARE(profileSpy.size(), 1);
        QVERIFY(!qvariant_cast<AuthorProfile>(profileSpy.takeFirst().first()).error.isEmpty());
        if (qEnvironmentVariableIsSet("AO3_PROFILE_HTML")) {
            QFile live(qEnvironmentVariable("AO3_PROFILE_HTML"));
            QVERIFY(live.open(QIODevice::ReadOnly)); network->page = live.readAll();
            client->fetchAuthorProfile("astolat"); QTRY_COMPARE(profileSpy.size(), 1);
            const auto fetched = qvariant_cast<AuthorProfile>(profileSpy.takeFirst().first());
            QVERIFY2(fetched.error.isEmpty(), qPrintable(fetched.error));
            QVERIFY(!fetched.details.isEmpty()); QVERIFY(!fetched.biography.isEmpty());
        }
    }
    void resultParsing() {
        LoginManager manager;
        AO3Client client(&manager);
        // The client owns its cookie jar; detach this stack object before destruction.
        manager.setParent(nullptr);
        const QString html = QStringLiteral(R"(<h2 class='heading'>21 - 40 of 1,234 Works</h2>
<ol class='work index group'>
<li class='blurb work group' id='work_111'><h4 class='heading'><a class='title' href='/works/111'>A &amp; &lt;b&gt;Title&lt;/b&gt;</a> by <a href='/users/One' rel='author'>One &amp; Two</a></h4>
<ul><li class="freeforms"><a class='tag'>Fluff &amp; &#9731;</a></li><li class='relationships'><a>A/B</a></li></ul>
<blockquote class='summary userstuff'><p>First paragraph</p><ol><li>Nested summary item</li></ol><p>Last paragraph</p></blockquote>
<dl class='stats'><dd class='words'>1,234</dd><dd class='comments'><a>2,345</a></dd><dd class='kudos'><a>678</a></dd><dd class='chapters'><a> 2 </a> / 2 </dd></dl></li>
<li id='work_222' class='work blurb'><h4 class='heading'><a href='/works/222'>Second</a></h4><dl class='stats'><dd class='chapters'>1/?</dd></dl></li></ol>
<ol class='pagination actions'><li><a rel='next' href='/works?page=3&amp;query=x'>Next</a></li><li><a href='/works?page=62&amp;query=x'>62</a></li><li><em class='current'>2</em></li></ol>
<ul><li class='freeforms'><a>Footer must not leak</a></li></ul>)");
        const auto result = client.parseSearchResults(html, {}, 2);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(result.works.size(), 2);
        QCOMPARE(result.totalResults, 1234);
        QCOMPARE(result.currentPage, 2);
        QCOMPARE(result.totalPages, 62);
        const auto first = result.works.first();
        QCOMPARE(first.title, QStringLiteral("A & <b>Title</b>"));
        QCOMPARE(first.author, QStringLiteral("One & Two"));
        QCOMPARE(first.wordCount, 1234);
        QCOMPARE(first.commentCount, 2345);
        QCOMPARE(first.kudosCount, 678);
        QVERIFY(first.complete);
        QVERIFY(first.summary.contains(QStringLiteral("Last paragraph")));
        QCOMPARE(first.tags, QStringList{QStringLiteral("Fluff & ☃")});
        QCOMPARE(result.works.last().chaptersTotal, -1);
        QVERIFY(result.works.last().tags.isEmpty());
        QVERIFY(!client.parseSearchResults(QStringLiteral("<p>Please sign in</p>"), {}, 1).errorMessage.isEmpty());
        const auto empty = client.parseSearchResults(QStringLiteral("<ol class='work index group'></ol>"), {}, 1);
        QVERIFY(empty.errorMessage.isEmpty());
        QVERIFY(empty.works.isEmpty());
        // These signatures match AO3's search_results view and search_results_found helper.
        const auto noMatches = client.parseSearchResults(QStringLiteral("<h2 class='heading'>Search Results</h2><p>No results found. You may want to edit your search to make it less specific.</p>"), {}, 1);
        QVERIFY(noMatches.errorMessage.isEmpty());
        QCOMPARE(noMatches.totalResults, 0);
        const auto found = client.parseSearchResults(QStringLiteral("<h2 class='heading'>Search Results</h2><h3 class='heading'>1,234 Found <a>Help</a></h3><ol class='work index'></ol>"), {}, 1);
        QCOMPARE(found.totalResults, 1234);
    }
    void keyboardAndEmptyResults() {
        MainWindow window;
        auto *network = installNetwork(window);
        QTest::qWait(30);
        network->page = R"(<ol class="work index"><li id="work_1" class="work"><h4 class="heading"><a href="/works/1">One</a></h4></li><li id="work_2" class="work"><h4 class="heading"><a href="/works/2">Two</a></h4></li></ol>)";
        window.m_searchInput->setText(QStringLiteral("test"));
        window.performSearch();
        QTRY_COMPARE(window.m_workList->count(), 2);
        QCOMPARE(window.m_detailWidget->currentWork().id, QStringLiteral("1"));
        QTest::keyClick(window.m_workList, Qt::Key_Down);
        QCOMPARE(window.m_detailWidget->currentWork().id, QStringLiteral("2"));
        network->page = "<ol class='work index group'></ol>";
        window.performSearch();
        QTRY_VERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("No works found")));
        QVERIFY(window.m_detailWidget->currentWork().id.isEmpty());
        network->page = "<p>Service unavailable</p>";
        window.performSearch();
        QTRY_VERIFY(window.statusBar()->currentMessage().startsWith(QStringLiteral("Error:")));
        QCOMPARE(window.m_currentResult.totalPages, 1);
        QVERIFY(window.m_paginationWidget->isHidden());
    }
    void readingStylesAndLinksSurviveOfflineAndBookMode() {
        QFile fixture(QStringLiteral(AO3_FIXTURE_DIR "/work-formatting.html")); QVERIFY(fixture.open(QIODevice::ReadOnly));
        const auto chapter=extractWorkChapters(QString::fromUtf8(fixture.readAll())).first();
        QVERIFY(!chapter.text.contains("unwanted script text"));
        Work work; work.id="778899"; work.title="Formatting test";
        ReaderWidget reader; reader.openWork(work,{chapter});
        auto format=[&](const QString &word,QTextDocument *document) {
            const int start=document->toPlainText().indexOf(word); QTextCursor cursor(document);
            cursor.setPosition(start); cursor.setPosition(start+word.size(),QTextCursor::KeepAnchor); return cursor.charFormat();
        };
        QVERIFY(format("bolditalic",reader.m_browser->document()).fontWeight()>=QFont::Bold);
        QVERIFY(format("bolditalic",reader.m_browser->document()).fontItalic());
        QVERIFY(format("underline",reader.m_browser->document()).fontUnderline());
        QVERIFY(format("deleted",reader.m_browser->document()).fontStrikeOut());
        QCOMPARE(format("monospace",reader.m_browser->document()).fontFamilies().toStringList(),QStringList{"monospace"});
        QCOMPARE(format("AO3 link",reader.m_browser->document()).anchorHref(),QString("https://archiveofourown.org/works/123"));
        QCOMPARE(format("footnote",reader.m_browser->document()).anchorHref(),QString("https://archiveofourown.org/works/778899?view_full_work=true#footnote"));
        for(const auto &word:QStringList{"unsafe","local file","credentials"}) { QVERIFY(!format(word,reader.m_browser->document()).isAnchor()); QVERIFY(!format(word,reader.m_browser->document()).fontUnderline()); }
        reader.setBookMode(true); QTRY_VERIFY(reader.m_book->doc);
        QVERIFY(format("bolditalic",reader.m_book->doc).fontItalic());
        QTemporaryDir directory; ReadingLibrary library(directory.path()); QVERIFY(library.saveOffline(work,{chapter}));
        const auto saved=library.load(work.id).first(); QCOMPARE(saved.styles.size(),chapter.styles.size());
        QCOMPARE(saved.styles.first().start,chapter.styles.first().start);
        const auto json=ReadingLibrary::chapterMetadata(chapter); const auto restored=ReadingLibrary::readChapter(json);
        QCOMPARE(restored.styles.size(),chapter.styles.size());
        QVERIFY(readingLink("javascript:alert(1)",work.id).isEmpty());
        QVERIFY(readingLink("file:///tmp/a",work.id).isEmpty());
        reader.setBookMode(false); reader.renderText("Translated text without source positions");
        QVERIFY(reader.m_browser->document()->toPlainText().startsWith("Translated"));
        QVERIFY(!format("Translated",reader.m_browser->document()).fontItalic());
        TextStyleSpan emphasis; emphasis.start=0; emphasis.end=4; emphasis.bold=true; emphasis.italic=true;
        reader.openWork(work,{{"Bold","BOLD",{},{},{emphasis}},{"Plain","Plain chapter"}});
        reader.showChapter(0); reader.showChapter(1);
        QVERIFY(!format("Plain",reader.m_browser->document()).fontItalic());
        QVERIFY(format("Plain",reader.m_browser->document()).fontWeight()<QFont::Bold);
    }
    void chapterNotesStaySeparateAndSurviveOfflineBackup() {
        QFile fixture(QStringLiteral(AO3_FIXTURE_DIR "/work-notes.html")); QVERIFY(fixture.open(QIODevice::ReadOnly));
        const auto chapters = extractWorkChapters(QString::fromUtf8(fixture.readAll()));
        QCOMPARE(chapters.size(),2);
        QVERIFY(chapters[0].notesBefore.contains("Work opening & 世界."));
        QVERIFY(chapters[0].notesBefore.contains("Nested opening paragraph."));
        QVERIFY(chapters[0].notesBefore.contains("Chapter one opening."));
        QVERIFY(!chapters[0].notesBefore.contains("See the end"));
        QVERIFY(!chapters[0].notesBefore.contains("summary"));
        QVERIFY(chapters[0].notesAfter.contains("Chapter one ending."));
        QVERIFY(chapters[0].notesAfter.contains("A nested quotation."));
        QVERIFY(!chapters[0].notesAfter.contains("Work ending."));
        QVERIFY(chapters[1].notesBefore.isEmpty());
        QVERIFY(chapters[1].notesAfter.contains("Chapter two ending."));
        QVERIFY(chapters[1].notesAfter.contains("Work ending."));
        QVERIFY(!chapters[0].text.contains("opening"));
        QVERIFY(chapters[0].text.contains("A story heading"));
        QTemporaryDir source,destination;
        ReadingLibrary library(source.path()); Work work; work.id="991122"; work.title="Notes test";
        QVERIFY(library.saveOffline(work,chapters));
        QCOMPARE(library.load(work.id)[0].notesBefore,chapters[0].notesBefore);
        const auto backup=LibraryBackup::snapshot(library);
        QVERIFY(LibraryBackup::merge(backup,nullptr,ReadingLibrary(destination.path())));
        QCOMPARE(ReadingLibrary(destination.path()).load(work.id)[1].notesAfter,chapters[1].notesAfter);
        const auto legacy=ReadingLibrary::readChapter(QJsonObject{{"title","Old"},{"text","Old offline copy"}});
        QVERIFY(legacy.notesBefore.isEmpty()); QVERIFY(legacy.notesAfter.isEmpty());
        ReaderWidget reader; reader.openWork(work,chapters);
        QVERIFY(!reader.m_notes->isHidden());
        const auto body=reader.m_browser->toPlainText();
        QTimer::singleShot(0,[&reader] {
            auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            const auto close=qScopeGuard([dialog] {dialog->reject();});
            auto *notes=dialog->findChild<QTextBrowser *>("authorNotesText");
            QVERIFY(notes); QVERIFY(notes->toPlainText().contains("Chapter one ending."));
            QCOMPARE(notes->styleSheet(),reader.m_browser->styleSheet());
            QCOMPARE(notes->document()->begin().blockFormat().lineHeight(),reader.m_browser->document()->begin().blockFormat().lineHeight());
        });
        reader.showNotes();
        QCOMPARE(reader.m_browser->toPlainText(),body);
        reader.showChapter(1); QCOMPARE(reader.m_browser->toPlainText(),chapters[1].text);
    }
    void offlineLibraryAndBookmarks() {
        QTemporaryDir directory;
        Work work; work.id = "777123"; work.title = "Offline 世界"; work.author = "Author";
        const QVector<WorkChapter> chapters = {{"One", "Bonjour 世界"}, {"Two", "Second chapter"}};
        ReadingLibrary library(directory.path());
        QVERIFY(library.saveOffline(work, chapters));
        QVERIFY(library.favorite(work.id, true));
        work.title = "Updated"; QVERIFY(library.remember(work));
        ReadingLibrary reopened(directory.path());
        QCOMPARE(reopened.entries().size(), 1);
        QVERIFY(reopened.entries().first().toObject().value("favorite").toBool());
        QCOMPARE(reopened.load(work.id).at(1).text, chapters.at(1).text);
        QVERIFY(reopened.removeOffline(work.id)); QVERIFY(reopened.load(work.id).isEmpty());
        QCOMPARE(reopened.entries().size(), 1);
        QVERIFY(!reopened.saveOffline(Work{}, chapters));
        QVERIFY(reopened.load("../escape").isEmpty());

        MainWindow window; auto *network = installNetwork(window);
        window.show(); QTest::qWait(30);
        QVERIFY(ReadingLibrary().saveOffline(work, chapters));
        window.showLibrary();
        int row = -1;
        for(int i = 0; i < window.m_libraryList->count(); ++i) {
            const auto entry = QJsonObject::fromVariantMap(window.m_libraryList->item(i)->data(Qt::UserRole).toMap());
            if(ReadingLibrary::work(entry).id == work.id) row = i;
        }
        QVERIFY(row >= 0); window.m_libraryList->setCurrentRow(row);
        const int requests = network->requests.size(); window.openLibrarySelection();
        QCOMPARE(window.m_contentStack->currentWidget(), window.m_reader);
        QCOMPARE(network->requests.size(), requests);
        QCOMPARE(window.m_reader->m_chapters.size(), 2);
        window.closeReader(); QCOMPARE(window.m_contentStack->currentWidget(), window.m_libraryPage);
        window.openLibrarySelection();
        QTimer::singleShot(20, &window, [] {
            auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            if(dialog) { dialog->setTextValue("A named place"); dialog->accept(); }
        });
        window.m_reader->addBookmark();
        const auto markers = QSettings().value("bookmarks/" + work.id).toList();
        QCOMPARE(markers.size(), 1); QCOMPARE(markers.first().toMap().value("title").toString(), "A named place");
        window.m_reader->showChapter(1);
        QTimer::singleShot(20, &window, [] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if(!dialog) return;
            auto *list = dialog->findChild<QListWidget *>(); if(list) list->setCurrentRow(0);
            for(auto *button : dialog->findChildren<QPushButton *>()) if(button->text() == "Go to bookmark") button->click();
        });
        window.m_reader->showBookmarks(); QCOMPARE(window.m_reader->m_index, 0);
        QSettings().setValue("reader/theme", 2);
        window.m_reader->applyAppearance();
        QVERIFY(window.m_reader->m_browser->styleSheet().contains("#191b1e"));
        QTest::qWait(20); window.m_reader->grab().save("/tmp/ao3-reader-dark-library.png");
        QSettings().setValue("reader/theme", 0);
    }

    void mobileNavigationAndLayout() {
        const QString previousStyle = QApplication::style()->objectName();
        QApplication::setStyle(QStringLiteral("Fusion"));
        const auto restoreStyle = qScopeGuard([previousStyle] { QApplication::setStyle(previousStyle); });
        qputenv("AO3_MOBILE_UI", "1");
        const auto restore = qScopeGuard([] { qunsetenv("AO3_MOBILE_UI"); });
        MainWindow window;
        auto *network = installNetwork(window);
        network->page = "<html><body>Not signed in</body></html>";
        window.resize(360, 800); window.show();
        QTest::qWait(60);
        QVERIFY(window.width() <= 360);
        QVERIFY(window.m_filterDock->isHidden());
        QVERIFY(window.m_headerBar->height() <= 60);
        QVERIFY(window.m_navBar->geometry().bottom() >= window.height() - 8);
        QTest::mouseClick(window.m_navSearch, Qt::LeftButton);
        QVERIFY(window.m_searchInput->isVisible());
        QTest::mouseClick(window.m_filterButton, Qt::LeftButton);
        QCOMPARE(window.m_contentStack->currentWidget(), window.m_mobileFilters);
        QVERIFY(window.m_filterPanel->isVisible());
        QVERIFY(window.navigateBack());
        QCOMPARE(window.m_contentStack->currentIndex(), 2);
        SearchResult result;
        Work work; work.id = "987654"; work.title = "Phone test"; work.author = "Author";
        result.works.append(work); result.totalResults = 1;
        window.m_contentStack->setCurrentIndex(2);
        window.onSearchCompleted(result);
        QCOMPARE(window.m_contentStack->currentIndex(), 2);
        window.m_workList->setCurrentRow(0);
        QCOMPARE(window.m_contentStack->currentIndex(), 2); // Selection while scrolling must not navigate.
        auto *item = window.m_workList->item(0);
        QTest::mouseClick(window.m_workList->viewport(), Qt::LeftButton, {}, window.m_workList->visualItemRect(item).center());
        QCOMPARE(window.m_contentStack->currentWidget(), window.m_mobileDetails);
        window.grab().save("/tmp/ao3-phone-details.png");
        QVERIFY(window.navigateBack());
        QCOMPARE(window.m_contentStack->currentIndex(), 2);
        window.onWorkSelected(0);
        window.openReader(work, {{"First", "Chapter one"}, {"Second", "Chapter two"}});
        QTest::qWait(30);
        QVERIFY(window.width() <= 360);
        QVERIFY(window.m_reader->m_browser->viewport()->width() >= 270);
        QVERIFY(window.m_reader->m_browser->height() > 500);
        QTest::mouseClick(window.m_reader->m_bottomNext, Qt::LeftButton);
        QCOMPARE(window.m_reader->m_index, 1);
        QVERIFY(window.m_reader->m_browser->toPlainText().contains("Chapter two"));
        window.grab().save("/tmp/ao3-android-reader-preview.png");
        QTest::mouseClick(window.m_reader->m_optionsToggle, Qt::LeftButton);
        QVERIFY(window.m_reader->m_mobileOptions->isVisible());
        QVERIFY(window.navigateBack());
        QCOMPARE(window.m_contentStack->currentWidget(), window.m_reader);
        QVERIFY(window.m_reader->m_mobileOptions->isHidden());
        window.resize(800, 360); QTest::qWait(30);
        QVERIFY(window.height() <= 360);
        QVERIFY(window.m_reader->m_browser->height() >= 100);
        window.grab().save("/tmp/ao3-phone-landscape.png");
        window.resize(360, 800); QTest::qWait(30);
        bool settingsFit = false;
        QTimer::singleShot(40, &window, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            dialog->setWindowState(Qt::WindowNoState); dialog->resize(360, 740);
            settingsFit = dialog->minimumSizeHint().width() <= 360;
            dialog->grab().save("/tmp/ao3-android-settings-preview.png");
            dialog->reject();
        });
        window.m_reader->showSettings();
        QVERIFY(settingsFit);
        QVERIFY(window.navigateBack());
        QCOMPARE(window.m_contentStack->currentWidget(), window.m_mobileDetails);
        QVERIFY(window.navigateBack());
        QCOMPARE(window.m_contentStack->currentIndex(), 2);
        const int requestsBeforeNavigation = network->requests.size();
        window.showDashboard(); QTest::mouseClick(window.m_navSearch, Qt::LeftButton);
        QCOMPARE(window.m_workList->count(), 1);
        QCOMPARE(network->requests.size(), requestsBeforeNavigation);
        window.onWorkSelected(0);
        network->page = "<div id=\"chapters\"><div class=\"userstuff\"><p>Late response</p></div></div>";
        emit window.m_detailWidget->readRequested(work.id);
        QVERIFY(window.navigateBack());
        QTest::qWait(30);
        emit window.m_client->workChaptersFetched(work.id, work.title, {{"Late response", "Should not reopen"}});
        QCOMPARE(window.m_contentStack->currentIndex(), 2);
        window.resize(360, 360); QTest::qWait(30);
        QVERIFY(window.height() <= 360);
        window.resize(360, 800);
        window.grab().save("/tmp/ao3-android-search-preview.png");
    }

    void searchReadDownload() {
        MainWindow window;
        auto *network = installNetwork(window);
        // Allow the queued startup session check to complete before testing search.
        QTest::qWait(30);
        QCOMPARE(window.m_contentStack->currentIndex(), 0);
        QTest::mouseClick(window.m_loginButton, Qt::LeftButton);
        QCOMPARE(window.m_contentStack->currentWidget(), window.m_loginView);
        emit window.m_loginView->cancelled();
        QCOMPARE(window.m_contentStack->currentIndex(), 0);
        const int beforeEmptySearch = network->requests.size();
        window.m_searchInput->setText(QStringLiteral("   "));
        window.performSearch();
        QCOMPARE(network->requests.size(), beforeEmptySearch);
        network->page = R"(<ol class="work index group"><li id="work_123" class="work blurb group">
<h4 class="heading"><a href="/works/123">Test Work</a></h4>
<ul class="tags"><li class='freeforms'><a class="tag">Friendship</a></li></ul>
<blockquote class="userstuff summary">Test summary</blockquote>
<dl class="stats"><dd class="words">1,234</dd><dd class="chapters">2/2</dd></dl></li></ol>)";
        window.m_searchInput->setText(QStringLiteral("test & friendship"));
        QTest::mouseClick(window.m_searchButton, Qt::LeftButton);
        QTRY_COMPARE(window.m_workList->count(), 1);
        QCOMPARE(window.m_detailWidget->currentWork().id, QStringLiteral("123"));
        QCOMPARE(window.m_detailWidget->currentWork().wordCount, 1234);
        QVERIFY(window.m_detailWidget->currentWork().complete);
        QCOMPARE(network->requests.last().host(), QStringLiteral("archiveofourown.org"));
        QCOMPARE(network->requests.last().path(), QStringLiteral("/works/search"));
        QCOMPARE(QUrlQuery(network->requests.last()).queryItemValue(QStringLiteral("work_search[query]")), QStringLiteral("test & friendship"));

        network->page += R"(<ol class="pagination"><li><a href="/works?page=1">1</a></li><li><em class="current">2</em></li><li><a href="/works?page=3">3</a></li></ol>)";
        window.m_currentResult.totalPages = 3;
        window.goToPage(2);
        QTRY_COMPARE(window.m_currentPage, 2);
        QTRY_COMPARE(window.m_workList->count(), 1);
        QVERIFY(window.m_prevButton->isEnabled());
        QVERIFY(window.m_nextButton->isEnabled());
        QCOMPARE(network->requests.last().path(), QStringLiteral("/works/search"));
        QCOMPARE(QUrlQuery(network->requests.last()).queryItemValue(QStringLiteral("work_search[query]")), QStringLiteral("test & friendship"));
        QCOMPARE(QUrlQuery(network->requests.last()).queryItemValue(QStringLiteral("page")), QStringLiteral("2"));
        const int beforeInvalidPage = network->requests.size();
        window.goToPage(0);
        window.goToPage(4);
        QCOMPARE(network->requests.size(), beforeInvalidPage);
        network->page = R"(<h2 class="title heading">Test Work</h2><a rel="author" href="/users/test">Author</a>
<div id="chapters"><div class="userstuff module"><p>First chapter &amp; text</p></div>
<div class="userstuff module"><p>Second chapter</p></div></div>)";
        QSignalSpy read(window.m_client, &AO3Client::workContentFetched);
        QVERIFY(QMetaObject::invokeMethod(window.m_detailWidget, "readRequested", Q_ARG(QString, QStringLiteral("123"))));
        QTRY_COMPARE(read.count(), 1);
        QCOMPARE(QUrlQuery(network->requests.last()).queryItemValue(QStringLiteral("view_full_work")), QStringLiteral("true"));
        auto *tabs = window.m_detailWidget->findChild<QTabWidget *>();
        auto *browser = qobject_cast<QTextBrowser *>(tabs->widget(0));
        QVERIFY(browser->toPlainText().contains(QStringLiteral("First chapter & text")));
        QVERIFY(browser->toPlainText().contains(QStringLiteral("Second chapter")));
        // A response for another work must leave the selected work untouched.
        emit window.m_client->workContentFetched(QStringLiteral("999"), {}, QStringLiteral("Wrong work"));
        QVERIFY(!browser->toPlainText().contains(QStringLiteral("Wrong work")));

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString output = directory.filePath(QStringLiteral("work.txt"));
        bool dialogSeen = false;
        QTimer dialogTimer;
        connect(&dialogTimer, &QTimer::timeout, &window, [&] {
            for (auto *widget : QApplication::topLevelWidgets()) {
                if (auto *dialog = qobject_cast<QFileDialog *>(widget); dialog && dialog->isVisible()) {
                    dialogSeen = true;
                    dialog->setDirectory(directory.path());
                    dialog->selectFile(QStringLiteral("work.txt"));
                    auto *fileName = dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"));
                    if (fileName) fileName->setText(output);
                    QMetaObject::invokeMethod(dialog, "accept");
                }
            }
        });
        dialogTimer.start(10);
        window.m_client->downloadWork(QStringLiteral("123"));
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(output), 3000);
        QVERIFY(dialogSeen);
        QFile file(output); QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray saved = file.readAll();
        QVERIFY(saved.contains("Test Work\nby Author"));
        QVERIFY(saved.contains("Second chapter"));
        QVERIFY(!saved.contains("<div"));
        QCOMPARE(QUrlQuery(network->requests.last()).queryItemValue(QStringLiteral("view_full_work")), QStringLiteral("true"));
        network->page = "<p>Login required</p>";
        window.m_client->downloadWork(QStringLiteral("123"));
        QTRY_VERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("nothing was saved")));
        QCOMPARE(QDir(directory.path()).entryList(QDir::Files).size(), 1);
        // Single-chapter AO3 markup must also reach the actual reader widget.
        network->page = R"(<h2 class="title heading">Single</h2><div id="chapters" role="article"><h3 class="landmark heading" id="work">Work Text:</h3><div class="userstuff"><p>Single chapter body.</p><h3>Story heading</h3></div></div>)";
        const int previousReads = read.count();
        window.m_client->fetchWorkContent(QStringLiteral("123"));
        QTRY_COMPARE(read.count(), previousReads + 1);
        QVERIFY(browser->toPlainText().contains(QStringLiteral("Single chapter body.")));
        QVERIFY(browser->toPlainText().contains(QStringLiteral("Story heading")));
        QVERIFY(!browser->toPlainText().contains(QStringLiteral("Work Text:")));

    }
};
QTEST_MAIN(FunctionalTest)
#include "functional_test.moc"
