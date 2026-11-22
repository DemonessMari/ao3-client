#pragma once
#include <QObject>
#include <QProcess>
#include <QStandardPaths>
#include <QRegularExpression>
#include <functional>
#ifdef Q_OS_UNIX
#include <signal.h>
#endif

class ReaderSpeech : public QObject {
public:
    explicit ReaderSpeech(QObject *parent=nullptr,QString executable={}):QObject(parent),engineExecutable(executable){}
    ~ReaderSpeech() override {stop();}
    std::function<void()> changed;
    QString error;
    bool active() const {return process!=nullptr;}
    bool paused() const {return suspended;}
    void start(const QString &text,const QString &language,int rate) {
        stop();error.clear();
        const auto executable=engineExecutable.isEmpty()?QStandardPaths::findExecutable("espeak-ng"):engineExecutable;
        if(executable.isEmpty()){error="Install eSpeak NG to read aloud on desktop.";notify();return;}
        if(!QRegularExpression("^[a-z]{2,3}(-[A-Za-z0-9]{2,8})?$").match(language).hasMatch()){error="Choose a speech language code, such as en.";notify();return;}
        if(text.trimmed().isEmpty())return;
        auto *child=new QProcess(this);process=child;
        connect(child,&QProcess::finished,child,&QObject::deleteLater);
        connect(child,&QProcess::finished,this,[this,child](int code,QProcess::ExitStatus status){if(process!=child)return;process=nullptr;suspended=false;if(code!=0 || status!=QProcess::NormalExit)error="The offline speech engine could not play this text. Check the voice and audio output.";notify();});
        connect(child,&QProcess::errorOccurred,this,[this,child](QProcess::ProcessError kind){if(process!=child)return;if(kind==QProcess::FailedToStart){process=nullptr;child->deleteLater();error="Could not start the offline speech engine.";notify();}});
        connect(child,&QProcess::started,this,[child,text]{child->write(text.toUtf8());child->closeWriteChannel();});
        child->start(executable,{"-v",language,"-s",QString::number(qBound(80,rate,350)),"--stdin"});notify();
    }
    void pauseOrResume() {
#ifdef Q_OS_UNIX
        if(!process || process->state()!=QProcess::Running)return;
        if(::kill(pid_t(process->processId()),suspended?SIGCONT:SIGSTOP)==0){suspended=!suspended;notify();}
#endif
    }
    void stop() {auto *previous=process;process=nullptr;suspended=false;if(previous){if(previous->state()!=QProcess::NotRunning)previous->kill();else previous->deleteLater();}notify();}
private:
    friend class FunctionalTest;
    QString engineExecutable;
    QProcess *process=nullptr;
    bool suspended=false;
    void notify(){if(changed)changed();}
};
