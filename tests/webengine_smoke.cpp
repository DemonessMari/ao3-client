#include "archiveweb.h"
#include "websession.h"
#include <memory>
#include <QApplication>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QPixmap>
#include <QVariant>
#include <QImage>
int main(int argc,char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc,argv);QStandardPaths::setTestModeEnabled(true);
    app.setApplicationName("AO3WebEngineTest");app.setOrganizationName("AO3Client");
    const bool live=argc>1 && QString::fromLocal8Bit(argv[1])=="--live";
    const QString screenshot=live?(argc>2?QString::fromLocal8Bit(argv[2]):QString()):(argc>1?QString::fromLocal8Bit(argv[1]):QString());
    QWebEngineProfile profile;QWebEngineView fixture;fixture.setPage(new QWebEnginePage(&profile,&fixture));
    std::unique_ptr<ArchiveWeb> official;if(live)official=std::make_unique<ArchiveWeb>();
    auto *view=live?official->findChild<QWebEngineView *>():&fixture;auto *page=view->page();
    if(live){official->resize(900,1000);official->show();}else{view->resize(600,800);view->show();}
    QEventLoop loop;bool passed=false;QTimer paintTimer;
    QObject::connect(&paintTimer,&QTimer::timeout,&loop,[&]{
        const auto image=view->grab().toImage();int ink=0;
        for(int y=0;y<image.height();y+=4)for(int x=0;x<image.width();x+=4)if(image.pixelColor(x,y).lightness()<200)++ink;
        if(passed && ink>50){if(!screenshot.isEmpty())image.save(screenshot);paintTimer.stop();loop.quit();}
    });
    auto failure=[&] {
        passed=false;
        if(!live){loop.quit();return;}
        page->runJavaScript("JSON.stringify({title:document.title,url:location.href,text:document.body ? document.body.innerText.slice(0,700) : ''})",[&](const QVariant &value){
            QTextStream(stdout)<<value.toString()<<"\n";
            QTimer::singleShot(500,&loop,[&]{if(!screenshot.isEmpty())view->grab().save(screenshot);loop.quit();});
        });
    };
    QObject::connect(view,&QWebEngineView::loadFinished,&loop,[&](bool ok) {
        if(!ok){if(!live)failure();return;}
        const QString condition=live?QString(R"js(location.origin === 'https://archiveofourown.org' && !!document.querySelector('input[name="user[login]"]') && !!document.querySelector('input[type=password]'))js"):
            "("+websiteSessionIdentityScript()+") === 'Alice' && document.querySelector('textarea').value === 'Notes' && document.querySelector('select').value === '9' && getComputedStyle(document.querySelector('blockquote')).fontStyle === 'italic'";
        page->runJavaScript(condition,[&](const QVariant &value){passed=value.toBool();if(passed)paintTimer.start(100);else if(!live)failure();});
    });
    if(live)official->open(QUrl("https://archiveofourown.org/users/login"));else view->setHtml("<!doctype html><html><head><meta charset='utf-8'><style>body{background:#fff;color:#222;font:18px serif;padding:24px}blockquote{font-style:italic}textarea,select{display:block;margin:12px 0;padding:8px;font:inherit}</style></head><body><nav id='greeting'><a href='/users/Alice'>Hi, Alice!</a> <a href='/users/logout?return_to=%2F'>Log out</a></nav><h1>AO3 rendering check</h1><blockquote>Emphasis, tables and official forms.</blockquote><table><tr><td>Chapter</td><td>One</td></tr></table><form><textarea>Notes</textarea><select><option value='9'>Pseud</option></select><button>Save</button></form></body></html>");
    QTimer::singleShot(30000,&loop,[&]{if(live)failure();else loop.quit();});QTimer::singleShot(35000,&loop,&QEventLoop::quit);loop.exec();passed=passed && !paintTimer.isActive();
    QTextStream(stdout)<<(passed?"WebEngine rendering and forms passed\n":"WebEngine rendering failed\n");return passed?0:1;
}
