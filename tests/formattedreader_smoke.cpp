#include "formattedreader.h"
#include "formattedwork.h"
#include <QApplication>
#include <QDialog>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QTimer>
#include <QImage>
#include <QTextStream>

int main(int argc,char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);QApplication app(argc,argv);
    Work work;work.id="12";work.title="Formatted reading check";
    WorkChapter chapter;chapter.title="Chapter one";chapter.text="A poem\nOne\nTwo\nCell one Cell two\nFootnote";
    chapter.html=staticReadingHtml("<h2>A poem</h2><p class='poem'>One<br>Two</p><table style='border:2px solid black'><tr><td>Cell one</td><td>Cell two</td></tr></table><p id='note'>Footnote</p><a href='#note'>Jump to note</a><img src='https://example.invalid/unsaved.png' alt='Unsaved illustration'><script>document.body.innerHTML='Unsafe'</script>");
    chapter.html+="<div class=\"hidden\">"+QString(2100000,QChar('x'))+"</div>";
    chapter.workCss="#workskin .poem {background:#ff0000;color:#ffffff;white-space:pre-wrap;padding:20px}";
    bool passed=false;QTimer timeout;timeout.setSingleShot(true);QObject::connect(&timeout,&QTimer::timeout,&app,[]{if(auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget()))dialog->reject();});timeout.start(20000);
    QTimer::singleShot(0,&app,[&]{
        auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!dialog)return;
        auto *view=dialog->findChild<QWebEngineView *>();if(!view){dialog->reject();return;}
        QObject::connect(view,&QWebEngineView::loadFinished,dialog,[&,view,dialog](bool ok){if(!ok){dialog->reject();return;}QTimer::singleShot(700,dialog,[&,view,dialog]{view->page()->toHtml([&,view,dialog](const QString &html){
            const auto image=view->grab().toImage();int red=0;for(int y=0;y<image.height();y+=3)for(int x=0;x<image.width();x+=3){const auto c=image.pixelColor(x,y);if(c.red()>200 && c.green()<80 && c.blue()<80)++red;}
            passed=html.contains("Cell one") && html.contains("Cell two") && html.contains("Unsaved illustration") && !html.contains("<script") && !html.contains("https://example.invalid") && red>50;
            if(argc>1){image.save(QString::fromLocal8Bit(argv[1]));}dialog->accept();
        });});});
    });
    showFormattedReader(nullptr,work,{chapter},0);
    QTextStream(stdout)<<(passed?"Formatted reader renders isolated HTML, work skins and tables\n":"Formatted reader rendering failed\n");return passed?0:1;
}
