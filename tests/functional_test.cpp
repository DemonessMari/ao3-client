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
    void commentFormsAndPosting() {
        QFile fixture(QStringLiteral(AO3_FIXTURE_DIR "/comment-form.html"));QVERIFY(fixture.open(QIODevice::ReadOnly));const auto html=fixture.readAll();
        const auto form=parseCommentForm(QString::fromUtf8(html));QVERIFY2(form.error.isEmpty(),qPrintable(form.error));
        QCOMPARE(form.hidden.value("authenticity_token"),QString("csrf+&=token"));QCOMPARE(form.hidden.value("comment[pseud_id]"),QString("12"));
        QCOMPARE(form.pseuds.size(),2);QVERIFY(form.notice.contains("moderated"));QVERIFY(!form.guest);
        QVERIFY(!validCommentAction("https://example.invalid/comments"));QVERIFY(!validCommentAction("//example.invalid/comments"));
        QVERIFY(!parseCommentForm(QString::fromUtf8(html).replace("/works/123/comments","https://example.invalid/comments")).error.isEmpty());
        QVERIFY(parseCommentSubmission("<p id='comment_notice'>Comment awaiting moderation.</p>").notice.contains("moderation"));
        QVERIFY(!parseCommentSubmission("<div id='errorExplanation'>Comment is too long.</div>").uncertain);
        QVERIFY(parseCommentSubmission("<h1>Maintenance</h1>").uncertain);
        MainWindow window;window.show();auto *network=installNetwork(window);QTest::qWait(30);network->page=html;
        window.m_commentsWork="123";window.m_contentStack->setCurrentWidget(window.m_commentsPage);window.composeComment();
        QTRY_VERIFY(!window.m_commentForm.action.isEmpty());QCOMPARE(network->operations.last(),QNetworkAccessManager::GetOperation);
        QCOMPARE(network->requests.last().path(),QString("/works/123/comments/new"));
        window.m_commentDraft->setPlainText("A+B & 世界 %2B=100%\nSecond line");network->page="<p id='comment_notice'>Comment awaiting moderation.</p>";
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
        const QByteArray identity = R"(<div>Hi, <a class='user' href='/users/MixedCase'>MixedCase</a>!</div><a href='/users/logout'>Sign out</a>)";
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
        QCOMPARE(window.m_loginButton->text(), QStringLiteral("MixedCase"));
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
        QTest::qWait(30);
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
        QTest::qWait(30);
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
        window.show(); QTest::qWait(30);
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
        QPalette dark = original;
        dark.setColor(QPalette::Window, QColor(QStringLiteral("#202020")));
        dark.setColor(QPalette::WindowText, Qt::white);
        dark.setColor(QPalette::Text, Qt::white);
        dark.setColor(QPalette::ButtonText, Qt::white);
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
        QVERIFY(reader.palette().color(QPalette::WindowText).lightness() < 100);
        if (qEnvironmentVariableIsSet("AO3_CAPTURE_THEME")) reader.grab().save(QStringLiteral("/tmp/ao3-reader-narrow.png"));
        QTimer::singleShot(0, &reader, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()); QVERIFY(dialog);
            QTest::qWait(30);
            QVERIFY(dialog->width() >= 540);
            QVERIFY(dialog->palette().color(QPalette::Window).lightness() > 200);
            QVERIFY(dialog->palette().color(QPalette::WindowText).lightness() < 100);
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
