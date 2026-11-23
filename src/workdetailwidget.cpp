#include "workdetailwidget.h"
#include "flowlayout.h"
#include "mobileui.h"
#include <QScrollArea>
#include <QFrame>
#include <QHBoxLayout>
#include <QFont>
#include <QLocale>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDesktopServices>

WorkDetailWidget::WorkDetailWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_noSelectionWidget = new QWidget(this);
    auto *noSelLayout = new QVBoxLayout(m_noSelectionWidget);
    auto *noSelLabel = new QLabel(QStringLiteral("Select a work to view details"), m_noSelectionWidget);
    noSelLabel->setAlignment(Qt::AlignCenter);
    QFont nf = noSelLabel->font();
    nf.setPointSize(14);
    noSelLabel->setFont(nf);
    noSelLabel->setStyleSheet(QStringLiteral("color: palette(text);"));
    noSelLayout->addWidget(noSelLabel);
    mainLayout->addWidget(m_noSelectionWidget);

    // Detail widget
    m_detailWidget = new QWidget(this);m_detailWidget->setProperty("skinRole","cards");
    auto *layout = new QVBoxLayout(m_detailWidget);
    layout->setSpacing(6);
    layout->setContentsMargins(16, 16, 16, 16);

    m_titleLabel = new QLabel(m_detailWidget);
    QFont tf = m_titleLabel->font();
    tf.setPointSize(16);
    tf.setBold(true);
    m_titleLabel->setFont(tf);
    m_titleLabel->setTextFormat(Qt::PlainText);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setStyleSheet(QStringLiteral("color: palette(highlight);"));
    layout->addWidget(m_titleLabel);

    m_authorLabel = new QLabel(m_detailWidget);
    QFont af = m_authorLabel->font();
    af.setPointSize(11);
    m_authorLabel->setTextFormat(Qt::PlainText);
    m_authorLabel->setFont(af);
    m_authorLabel->setStyleSheet(QStringLiteral("color: palette(text);"));
    layout->addWidget(m_authorLabel);

    auto *sep1 = new QFrame(m_detailWidget);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(sep1);

    m_fandomLabel = new QLabel(m_detailWidget);
    m_fandomLabel->setTextFormat(Qt::PlainText);
    m_fandomLabel->setWordWrap(true);
    m_fandomLabel->setStyleSheet(QStringLiteral("color: palette(highlight); font-size: 9pt;"));
    layout->addWidget(m_fandomLabel);

    m_ratingLabel = new QLabel(m_detailWidget);
    m_ratingLabel->setWordWrap(true);
    m_ratingLabel->setStyleSheet(QStringLiteral("color: palette(text); font-size: 9pt;"));
    layout->addWidget(m_ratingLabel);

    m_tagsLabel = new QLabel(m_detailWidget);
    m_tagsLabel->setWordWrap(true);
    m_tagsLabel->setStyleSheet(QStringLiteral("color: palette(text); font-size: 9pt;"));
    layout->addWidget(m_tagsLabel);

    m_statsLabel = new QLabel(m_detailWidget);
    m_statsLabel->setWordWrap(true);
    m_statsLabel->setStyleSheet(QStringLiteral("color: palette(text); font-size: 9pt;"));
    layout->addWidget(m_statsLabel);

    auto *sep2 = new QFrame(m_detailWidget);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(sep2);

    auto *summaryTitle = new QLabel(QStringLiteral("<b>Summary</b>"), m_detailWidget);
    summaryTitle->setStyleSheet(QStringLiteral("font-size: 10pt;"));
    layout->addWidget(summaryTitle);
    m_summaryHeading = summaryTitle;

    m_summaryBrowser = new QTextBrowser(m_detailWidget);
    m_summaryBrowser->setOpenExternalLinks(true);
    m_summaryBrowser->setMaximumHeight(150);
    m_summaryBrowser->setStyleSheet(QStringLiteral("background: transparent; border: none; font-size: 9pt; color: palette(text);"));
    layout->addWidget(m_summaryBrowser);

    auto *sep3 = new QFrame(m_detailWidget);
    sep3->setFrameShape(QFrame::HLine);
    sep3->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(sep3);

    // Buttons
    m_buttonBar = new QWidget(m_detailWidget);
    m_buttonBar->setObjectName("detailActions");
    auto *btnLayout = new FlowLayout(m_buttonBar);
    btnLayout->setContentsMargins(0, 4, 0, 4);
    btnLayout->setSpacing(8);

    m_readButton = new QPushButton(QStringLiteral("Open reader"), m_buttonBar);
    m_readButton->setMinimumHeight(32);
    m_readButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: palette(highlight); color: palette(highlighted-text); border: none; border-radius: 4px; padding: 4px 20px; font-weight: bold; }"
        "QPushButton:hover { background: palette(highlight); }"
    ));
    btnLayout->addWidget(m_readButton);

    m_downloadButton = new QPushButton(QStringLiteral("Download"), m_buttonBar);
    m_downloadButton->setMinimumHeight(32);
    m_downloadButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: palette(button); color: palette(text); border: 1px solid palette(mid); border-radius: 4px; padding: 4px 20px; }"
        "QPushButton:hover { background: palette(button); }"
    ));
    btnLayout->addWidget(m_downloadButton);
    auto *readingAction=new QPushButton("AO3 account",m_buttonBar);btnLayout->addWidget(readingAction);
    connect(readingAction,&QPushButton::clicked,this,[this] {if(!m_currentWork.id.isEmpty())emit readingActionRequested(m_currentWork.id);});
    m_kudosButton=new QPushButton("Give kudos",m_buttonBar);m_kudosButton->setObjectName("giveKudos");btnLayout->addWidget(m_kudosButton);
    connect(m_kudosButton,&QPushButton::clicked,this,[this] { if(!m_currentWork.id.isEmpty())emit kudosRequested(m_currentWork.id); });
    auto *profile = new QPushButton("Author profile", m_buttonBar); profile->setObjectName("authorProfileButton"); profile->setVisible(!mobileUi()); btnLayout->addWidget(profile);
    connect(profile, &QPushButton::clicked, this, [this] { if(!m_currentWork.authorUrl.isEmpty()) emit profileRequested(m_currentWork.authorUrl); });
    auto *series=new QPushButton("Series",m_buttonBar);series->setObjectName("workSeries");layout->addWidget(series,0,Qt::AlignLeft);
    connect(series,&QPushButton::clicked,this,[this]{if(!m_currentWork.id.isEmpty())emit seriesRequested(m_currentWork.id);});
    auto *comments = new QPushButton("Comments",m_buttonBar); comments->setObjectName("workComments"); btnLayout->addWidget(comments);
    connect(comments,&QPushButton::clicked,this,[this] {if(!m_currentWork.id.isEmpty())emit commentsRequested(m_currentWork.id);});
    auto *online=new QPushButton("AO3 website",m_buttonBar); btnLayout->addWidget(online); online->setToolTip("Official AO3 forms, series, collections and skins. Website sign-in is separate.");
    connect(online,&QPushButton::clicked,this,[this] { if(!m_currentWork.id.isEmpty()) emit archiveRequested(QUrl("https://archiveofourown.org/works/"+m_currentWork.id)); });
    layout->addWidget(m_buttonBar);

    // Content tabs
    m_tabs = new QTabWidget(m_detailWidget);
    m_tabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border: 1px solid palette(mid); border-top: none; background: palette(base); }"
        "QTabBar::tab { padding: 6px 16px; border: 1px solid palette(mid); border-bottom: none; background: palette(button); }"
        "QTabBar::tab:selected { background: palette(base); font-weight: bold; }"
    ));

    m_contentBrowser = new QTextBrowser(m_tabs);
    m_contentBrowser->setOpenExternalLinks(true);
    m_contentBrowser->setStyleSheet(QStringLiteral("font-size: 11pt; color: palette(text); padding: 12px; line-height: 1.6;"));
    m_tabs->addTab(m_contentBrowser, QStringLiteral("Work Content"));
    m_tabs->hide();

    layout->addWidget(m_tabs, 1);
    layout->addStretch();

    m_detailWidget->hide();
    mainLayout->addWidget(m_detailWidget, 1);
    if (mobileUi()) {
        layout->setAlignment(Qt::AlignTop);
        layout->removeWidget(m_buttonBar); layout->insertWidget(2, m_buttonBar);
        m_readButton->setText(QStringLiteral("Read fic"));
        m_readButton->setMinimumHeight(48); m_downloadButton->setMinimumHeight(48);
        m_authorLabel->setWordWrap(true);
        for (auto *label : {m_titleLabel, m_authorLabel, m_tagsLabel, m_fandomLabel, m_statsLabel, m_ratingLabel}) {
            label->setMinimumWidth(0); label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        }
        m_mobileSummary = new QLabel(m_detailWidget); m_mobileSummary->setWordWrap(true);
        m_mobileSummary->setTextFormat(Qt::PlainText); m_mobileSummary->setMinimumWidth(0);
        m_mobileSummary->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        layout->insertWidget(layout->indexOf(m_summaryBrowser), m_mobileSummary);
        m_summaryBrowser->hide();
    }

    connect(m_readButton, &QPushButton::clicked, this, [this]() {
        if (!m_currentWork.id.isEmpty())
            emit readRequested(m_currentWork.id);
    });

    connect(m_downloadButton, &QPushButton::clicked, this, [this]() {
        if (!m_currentWork.id.isEmpty())
            emit downloadRequested(m_currentWork);
    });
}

void WorkDetailWidget::showWork(const Work &work)
{
    m_currentWork = work;
    emit workShown(work.id);
    if(auto *button = findChild<QPushButton *>("authorProfileButton")) button->setEnabled(!work.authorUrl.isEmpty());
    m_noSelectionWidget->hide();
    m_detailWidget->show();
    m_tabs->hide();

    m_titleLabel->setText(work.title.isEmpty() ? QStringLiteral("(untitled)") : work.title);
    m_authorLabel->setText(QStringLiteral("by %1").arg(work.author.isEmpty() ? QStringLiteral("Anonymous") : work.author));

    QStringList fandomParts;
    if (!work.fandom.isEmpty()) fandomParts << work.fandom;
    m_fandomLabel->setText(fandomParts.isEmpty() ? QString() : fandomParts.join(QStringLiteral(", ")));
    m_fandomLabel->setVisible(!fandomParts.isEmpty());

    QStringList ratingParts;
    if (!work.rating.isEmpty()) ratingParts << QStringLiteral("<b>Rating:</b> %1").arg(work.rating.toHtmlEscaped());
    if (!work.warnings.isEmpty()) ratingParts << QStringLiteral("<b>Warnings:</b> %1").arg(work.warnings.toHtmlEscaped());
    if (!work.category.isEmpty()) ratingParts << QStringLiteral("<b>Category:</b> %1").arg(work.category.toHtmlEscaped());
    m_ratingLabel->setText(ratingParts.join(QStringLiteral(" | ")));
    m_ratingLabel->setVisible(!ratingParts.isEmpty());

    QStringList tagParts;
    if (!work.relationships.isEmpty())
        tagParts << QStringLiteral("<b>Relationships:</b> %1").arg(work.relationships.join(QStringLiteral(", ")).toHtmlEscaped());
    if (!work.characters.isEmpty())
        tagParts << QStringLiteral("<b>Characters:</b> %1").arg(work.characters.join(QStringLiteral(", ")).toHtmlEscaped());
    if (!work.tags.isEmpty())
        tagParts << QStringLiteral("<b>Tags:</b> %1").arg(work.tags.join(QStringLiteral(", ")).toHtmlEscaped());
    m_tagsLabel->setText(tagParts.join(QStringLiteral("<br>")));
    m_tagsLabel->setVisible(!tagParts.isEmpty());

    QStringList stats;
    stats << QStringLiteral("<b>Words:</b> %1").arg(QLocale().toString(work.wordCount));
    stats << QStringLiteral("<b>Chapters:</b> %1/%2")
                .arg(work.chaptersWritten)
                .arg(work.chaptersTotal == -1 ? QStringLiteral("?") : QString::number(work.chaptersTotal));
    if (work.complete)
        stats << QStringLiteral("<span style='color:green;'><b>Complete</b></span>");
    if (work.commentCount > 0)
        stats << QStringLiteral("<b>Comments:</b> %1").arg(QLocale().toString(work.commentCount));
    if (work.kudosCount > 0)
        stats << QStringLiteral("<b>Kudos:</b> %1").arg(QLocale().toString(work.kudosCount));
    if (work.hitCount > 0)
        stats << QStringLiteral("<b>Hits:</b> %1").arg(QLocale().toString(work.hitCount));
    if (!work.language.isEmpty())
        stats << QStringLiteral("<b>Language:</b> %1").arg(work.language.toHtmlEscaped());
    m_statsLabel->setText(stats.join(QStringLiteral(" &middot; ")));

    m_summaryHeading->setVisible(!work.summary.isEmpty());
    if (m_mobileSummary) {
        m_mobileSummary->setText(work.summary); m_mobileSummary->setVisible(!work.summary.isEmpty());
    }
    m_summaryBrowser->setVisible(!mobileUi() && !work.summary.isEmpty());
    m_summaryBrowser->setHtml(QStringLiteral("<p style='margin:0;'>%1</p>").arg(work.summary.toHtmlEscaped()));
    m_contentBrowser->clear();
}

void WorkDetailWidget::showContent(const QString &plainText)
{
    m_contentBrowser->setPlainText(plainText);
    // Reading uses the dedicated full-width reader, not this metadata pane.
}

void WorkDetailWidget::clear()
{
    m_currentWork = Work();
    m_noSelectionWidget->show();
    m_detailWidget->hide();
    m_tabs->hide();
    m_contentBrowser->clear();
}
