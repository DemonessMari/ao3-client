#pragma once

#include <QWidget>
#include <QLabel>
#include <QTextBrowser>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTabWidget>
#include "workmodel.h"

class WorkDetailWidget : public QWidget {
    Q_OBJECT
public:
    explicit WorkDetailWidget(QWidget *parent = nullptr);

    void showWork(const Work &work);
    void showContent(const QString &html);
    void clear();
    void setKudosStatus(const QString &status,bool guest=false) { m_kudosButton->setEnabled(status.isEmpty()); m_kudosButton->setText(status.isEmpty()?(guest?QString("Give guest kudos"):QString("Give kudos")):status); }
    Work currentWork() const { return m_currentWork; }

signals:
    void kudosRequested(const QString &workId);
    void workShown(const QString &workId);
    void commentsRequested(const QString &workId);
    void readRequested(const QString &workId);
    void profileRequested(const QString &authorUrl);
    void downloadRequested(const Work &work);

private:
    QWidget *m_noSelectionWidget;
    QWidget *m_detailWidget;

    QLabel *m_titleLabel;
    QLabel *m_authorLabel;
    QLabel *m_ratingLabel;
    QLabel *m_fandomLabel;
    QLabel *m_tagsLabel;
    QLabel *m_statsLabel;
    QLabel *m_mobileSummary = nullptr;
    QLabel *m_summaryHeading = nullptr;
    QTextBrowser *m_summaryBrowser;
    QTextBrowser *m_contentBrowser;
    QTabWidget *m_tabs;

    QPushButton *m_readButton;
    QPushButton *m_downloadButton;
    QPushButton *m_kudosButton;
    QWidget *m_buttonBar;

    Work m_currentWork;
};
