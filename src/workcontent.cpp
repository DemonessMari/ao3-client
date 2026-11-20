#include "workcontent.h"
#include <QRegularExpression>
#include <QTextDocument>
#include <QStringList>
#include <QVector>

namespace {
QString attribute(const QString &html, const QString &name)
{
    const QRegularExpression rx(QStringLiteral(R"regex(\b%1\s*=\s*(["'])(.*?)\1)regex").arg(name),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    return rx.match(html).captured(2);
}
QStringList classes(const QString &html)
{
    return attribute(html, QStringLiteral("class")).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}
}

QVector<WorkChapter> extractWorkChapters(const QString &html)
{
    const QRegularExpression divs(QStringLiteral("<!--.*?-->|<(/?)div\\b([^>]*)>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    QVector<WorkChapter> chapters;
    int previousBodyEnd = 0;
    QVector<bool> excluded;
    int chaptersDepth = -1;
    int bodyDepth = -1;
    int bodyStart = -1;
    auto tags = divs.globalMatch(html);
    while (tags.hasNext()) {
        const auto tag = tags.next();
        if (tag.captured().startsWith(QStringLiteral("<!--"))) continue;
        if (tag.captured(1).isEmpty()) {
            const auto names = classes(tag.captured(2));
            const bool blocked = (!excluded.isEmpty() && excluded.last()) ||
                names.contains(QStringLiteral("summary")) || names.contains(QStringLiteral("notes")) ||
                names.contains(QStringLiteral("preface")) || names.contains(QStringLiteral("afterword"));
            excluded.append(blocked);
            const int depth = excluded.size();
            if (attribute(tag.captured(2), QStringLiteral("id")) == QStringLiteral("chapters")) chaptersDepth = depth;
            // Single-chapter works use plain userstuff inside #chapters. Chaptered
            // works use userstuff module. Never include summaries or notes.
            if (bodyStart < 0 && !blocked && names.contains(QStringLiteral("userstuff")) &&
                (chaptersDepth >= 0 || names.contains(QStringLiteral("module")))) {
                bodyStart = tag.capturedEnd();
                bodyDepth = depth;
            }
        } else {
            const int depth = excluded.size();
            if (bodyStart >= 0 && depth == bodyDepth) {
                QString body = html.mid(bodyStart, tag.capturedStart() - bodyStart);
                const QRegularExpression headings(QStringLiteral("<h3\\b([^>]*)>(.*?)</h3>"),
                    QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
                // Remove only AO3's accessibility label, preserving story headings.
                auto matches = headings.globalMatch(body);
                QVector<QPair<int, int>> removals;
                while (matches.hasNext()) {
                    const auto heading = matches.next();
                    if (classes(heading.captured(1)).contains(QStringLiteral("landmark")) ||
                        attribute(heading.captured(1), QStringLiteral("id")) == QStringLiteral("work"))
                        removals.append({heading.capturedStart(), heading.capturedLength()});
                }
                for (auto it = removals.crbegin(); it != removals.crend(); ++it) body.remove(it->first, it->second);
                QTextDocument document;
                document.setHtml(body);
                const QString text = document.toPlainText().trimmed();
                if (!text.isEmpty()) {
                    QString title;
                    const QString prelude = html.mid(previousBodyEnd, bodyStart - previousBodyEnd);
                    const QRegularExpression titles(QStringLiteral("<h3\\b([^>]*)>(.*?)</h3>"),
                        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
                    auto titleMatches = titles.globalMatch(prelude);
                    while (titleMatches.hasNext()) {
                        const auto heading = titleMatches.next();
                        if (classes(heading.captured(1)).contains(QStringLiteral("title"))) {
                            QTextDocument label; label.setHtml(heading.captured(2));
                            title = label.toPlainText().trimmed();
                        }
                    }
                    if (title.isEmpty()) title = QStringLiteral("Chapter %1").arg(chapters.size() + 1);
                    chapters.append({title, text});
                }
                previousBodyEnd = tag.capturedEnd();
                bodyStart = -1;
            }
            if (depth == chaptersDepth) chaptersDepth = -1;
            if (!excluded.isEmpty()) excluded.removeLast();
        }
    }
    return chapters;
}

QString extractWorkText(const QString &html)
{
    QStringList text;
    for (const auto &chapter : extractWorkChapters(html)) text.append(chapter.text);
    return text.join(QStringLiteral("\n\n"));
}
