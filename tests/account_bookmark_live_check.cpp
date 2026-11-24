#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QSettings>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include "ao3client.h"

int main(int argc,char **argv) {
    QApplication app(argc,argv);QStandardPaths::setTestModeEnabled(true);
    if(argc<4 || argc>5 || !QRegularExpression("^[0-9]+$").match(QString::fromLocal8Bit(argv[3])).hasMatch() || (argc==5 && QString::fromLocal8Bit(argv[4])!="--execute-private-bookmark-test")) {
        QTextStream(stderr)<<"Pass cookie file, settings file and work ID. Default is read-only; --execute-private-bookmark-test creates and removes one labelled private test bookmark.\n";return 2;
    }
    const auto agent=QSettings(QString::fromLocal8Bit(argv[2]),QSettings::IniFormat).value("network/userAgent","AO3Client/3.0 (Unofficial Desktop Reader)");
    QTemporaryDir directory;app.setOrganizationName("AO3Test");app.setApplicationName("AccountBookmarkCheck");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());QSettings().setValue("network/userAgent",agent);
    const auto copy=directory.filePath("cookies.dat");if(!QFile::copy(QString::fromLocal8Bit(argv[1]),copy))return 2;QFile::setPermissions(copy,QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    auto *jar=new LoginManager(nullptr,copy);AO3Client client(jar);const auto work=QString::fromLocal8Bit(argv[3]);bool valid=false;
    {
        QEventLoop loop;QTimer timeout;timeout.setSingleShot(true);QObject::connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);QObject::connect(&client,&AO3Client::sessionChecked,&loop,[&](bool confirmed,const UserInfo &){valid=confirmed;loop.quit();});timeout.start(45000);client.checkSession();loop.exec();
    }
    if(!valid){QTextStream(stdout)<<"No confirmed session. No account writes performed.\n";return 77;}
    auto fetch=[&](ArchiveBookmarkForm &form){
        bool received=false;QEventLoop loop;QTimer timeout;timeout.setSingleShot(true);QObject::connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);QObject::connect(&client,&AO3Client::archiveBookmarkFetched,&loop,[&](const ArchiveBookmarkForm &value){form=value;received=true;loop.quit();});timeout.start(45000);client.fetchArchiveBookmark(work);loop.exec();return received && form.error.isEmpty();
    };
    ArchiveBookmarkForm initial;if(!fetch(initial) || !initial.id.isEmpty() || initial.pseuds.isEmpty()) {QTextStream(stdout)<<"A fresh empty bookmark form was not available. No account writes performed.\n";return 77;}
    const QString note="AO3 Client verification — temporary private bookmark";
    if(argc==4){QTextStream(stdout)<<"Ready for an optional private-bookmark test on https://archiveofourown.org/works/"<<work<<". Label: "<<note<<". Existing bookmark absent. No account writes performed.\n";return 0;}
    auto submit=[&](const ArchiveBookmarkForm &form,bool deleting){
        bool received=false;QEventLoop loop;QTimer timeout;timeout.setSingleShot(true);QObject::connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);QObject::connect(&client,&AO3Client::archiveBookmarkSaved,&loop,[&](const QString &id,const ArchiveFormResult &){if(id==work){received=true;loop.quit();}});timeout.start(120000);client.saveArchiveBookmark(form,deleting);loop.exec();return received;
    };
    auto draft=initial;draft.notes=note;draft.tags.clear();draft.collections.clear();draft.privateBookmark=true;draft.recommendation=false;draft.pseud=initial.pseuds.first().first;
    const bool responded=submit(draft,false);ArchiveBookmarkForm created;
    if(!responded || !fetch(created) || created.id.isEmpty() || created.notes!=note || !created.privateBookmark){QTextStream(stdout)<<"Creation was not confirmed. No retry was sent. Check the bookmark on https://archiveofourown.org/works/"<<work<<".\n";return 1;}
    QTextStream(stdout)<<"Temporary private bookmark created and verified.\n";
    const bool removed=submit(created,true);ArchiveBookmarkForm after;
    if(!removed || !fetch(after) || !after.id.isEmpty()){QTextStream(stdout)<<"Cleanup was not confirmed. Check the labelled test bookmark on https://archiveofourown.org/works/"<<work<<". No retry was sent.\n";return 1;}
    QTextStream(stdout)<<"Temporary private bookmark deleted; original unbookmarked state confirmed.\n";return 0;
}
