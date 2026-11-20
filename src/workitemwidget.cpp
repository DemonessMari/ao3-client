#include "workitemwidget.h"
#include "mobileui.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QLocale>

WorkItemWidget::WorkItemWidget(const Work &work, QWidget *parent)
    : QWidget(parent)
    , m_work(work)
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(2);
    layout->setContentsMargins(mobileUi() ? 16 : 10, mobileUi() ? 14 : 8, mobileUi() ? 16 : 10, mobileUi() ? 14 : 8);
    if (mobileUi()) layout->setSpacing(6);

    auto *titleLabel = new QLabel(work.title.isEmpty() ? QStringLiteral("(untitled)") : work.title, this);
    QFont tf = titleLabel->font();
    tf.setPointSize(11);
    tf.setBold(true);
    titleLabel->setFont(tf);
    titleLabel->setStyleSheet(QStringLiteral("color: #2a5f8f;"));
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    auto *byline = new QLabel(QStringLiteral("by %1").arg(work.author.isEmpty() ? QStringLiteral("Anonymous") : work.author), this);
    byline->setWordWrap(true);
    byline->setStyleSheet(QStringLiteral("color: #666; font-size: 10pt;"));
    layout->addWidget(byline);

    QStringList tagParts;
    if (!work.rating.isEmpty()) tagParts << work.rating;
    if (!work.warnings.isEmpty()) tagParts << work.warnings;
    if (!work.category.isEmpty()) tagParts << work.category;
    if (!work.fandom.isEmpty()) tagParts << work.fandom;

    if (!tagParts.isEmpty()) {
        auto *tagsLabel = new QLabel(tagParts.join(QStringLiteral(" | ")), this);
        tagsLabel->setStyleSheet(QStringLiteral("color: #900; font-size: 10pt;"));
        tagsLabel->setWordWrap(true);
        layout->addWidget(tagsLabel);
    }

    if (!work.summary.isEmpty()) {
        QString summary = work.summary;
        if (summary.length() > 300)
            summary = summary.left(300) + QStringLiteral("...");
        auto *summaryLabel = new QLabel(summary, this);
        summaryLabel->setStyleSheet(QStringLiteral("color: #555; font-size: 10pt;"));
        summaryLabel->setWordWrap(true);
        layout->addWidget(summaryLabel);
    }

    QStringList statsParts;
    statsParts << QStringLiteral("Words: %1").arg(QLocale().toString(work.wordCount));
    statsParts << QStringLiteral("Chapters: %1/%2")
                    .arg(work.chaptersWritten)
                    .arg(work.chaptersTotal == -1 ? QStringLiteral("?") : QString::number(work.chaptersTotal));
    if (work.complete)
        statsParts << QStringLiteral("Complete");
    if (work.kudosCount > 0)
        statsParts << QStringLiteral("Kudos: %1").arg(QLocale().toString(work.kudosCount));
    if (work.hitCount > 0)
        statsParts << QStringLiteral("Hits: %1").arg(QLocale().toString(work.hitCount));
    if (!work.language.isEmpty())
        statsParts << work.language;

    auto *statsLabel = new QLabel(statsParts.join(QStringLiteral(" | ")), this);
    statsLabel->setWordWrap(true);
    statsLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 10pt;"));
    layout->addWidget(statsLabel);

    for (auto *label : findChildren<QLabel *>()) {
        label->setTextFormat(Qt::PlainText);
        label->setStyleSheet(label->styleSheet() + QStringLiteral("background: transparent; border: none;"));
    }

    setStyleSheet(QStringLiteral(
        "WorkItemWidget { background: white; border-bottom: 1px solid #ddd; }"
        "WorkItemWidget:hover { background: #f0f5fa; }"
    ));
}
