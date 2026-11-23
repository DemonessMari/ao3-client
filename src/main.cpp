#include <QApplication>
#include "appskin.h"
#include <QIcon>
#include "mainwindow.h"
#include "mobileui.h"
#include "readabletheme.h"
#include <QSettings>
#include <QCommandLineParser>
#include <QTimer>
#include <QStyleHints>

int main(int argc, char *argv[])
{
#ifdef Q_OS_ANDROID
    qputenv("ANDROID_OPENSSL_SUFFIX", "_3");
#endif
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("AO3 Client"));
    app.setApplicationVersion(QStringLiteral("3.2.0-rc.18"));
    QCommandLineParser parser; parser.setApplicationDescription("Unofficial AO3 reader"); parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"smoke-test","Open the main window briefly and exit (installation check)."}); parser.process(app);
    app.setOrganizationName(QStringLiteral("AO3Client"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/ao3.png")));

    app.setStyleSheet(QStringLiteral(
        "QMainWindow { background: palette(window); }"
        "QToolTip { background: #333; color: white; border: none; padding: 4px; }"
    ));

    if (mobileUi()) {
        app.setStyle(QStringLiteral("Fusion"));
        app.setStyleSheet(app.styleSheet() + QStringLiteral(
            "QPushButton, QLineEdit, QComboBox, QSpinBox { min-height: 36px; }"
            "QTabBar::tab { min-height: 36px; }"));
    }
    if(QSettings().value("translation/url").toString()=="https://translate.cutie.dating") { QSettings().remove("translation/url"); QSettings().remove("translation/consented"); }
    applyAppSkin();
    QObject::connect(QGuiApplication::styleHints(),&QStyleHints::colorSchemeChanged,&app,[]{if(QSettings().value("appearance/followSystem",false).toBool())applyAppSkin();});
    MainWindow window;
    if (mobileUi()) {
        applyReadableTheme(&window);
        QObject::connect(&app, &QGuiApplication::applicationStateChanged, &window, [&window](Qt::ApplicationState state) {
            if (state != Qt::ApplicationActive) {
                for (auto *reader : window.findChildren<ReaderWidget *>()) reader->saveProgress();
                QSettings().sync();
            }
        });
    }
    window.show();
    if(parser.isSet("smoke-test")) QTimer::singleShot(700,&app,&QCoreApplication::quit);

    return app.exec();
}
