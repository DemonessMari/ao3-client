#include "workcontent.h"
#include <QRegularExpression>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>
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

void attachNotes(const QString &html, QVector<WorkChapter> &chapters, const QVector<QPair<int,int>> &bodies)
{
    struct Node { QString tag; QStringList names; int parent, start, end = -1; };
    QVector<Node> nodes;
    QVector<int> stack;
    const QRegularExpression elements(QStringLiteral("<!--.*?-->|<(/?)(div|blockquote)\\b([^>]*)>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    auto matches = elements.globalMatch(html);
    while (matches.hasNext()) {
        const auto tag = matches.next();
        if (tag.captured().startsWith(QStringLiteral("<!--"))) continue;
        if (tag.captured(1).isEmpty()) {
            nodes.append({tag.captured(2).toLower(), classes(tag.captured(3)),
                stack.isEmpty() ? -1 : stack.last(), int(tag.capturedEnd())});
            stack.append(nodes.size() - 1);
        } else if (!stack.isEmpty() && nodes[stack.last()].tag == tag.captured(2).toLower()) {
            nodes[stack.takeLast()].end = tag.capturedStart();
        }
    }
    for (int i = 0; i < nodes.size(); ++i) {
        const auto &node = nodes[i];
        if (node.end < 0 || !node.names.contains("userstuff")) continue;
        int notes = -1, chapter = -1;
        bool nested = false;
        for (int parent = node.parent; parent >= 0; parent = nodes[parent].parent) {
            const auto &ancestor = nodes[parent];
            if (ancestor.names.contains("userstuff")) nested = true;
            if (notes < 0 && ancestor.names.contains("notes")) notes = parent;
            if (chapter < 0 && ancestor.names.contains("chapter") && !ancestor.names.contains("preface")) chapter = parent;
        }
        if (nested || notes < 0) continue;
        int target = -1;
        for (int c = 0; c < bodies.size(); ++c) {
            if (node.start >= bodies[c].first && node.start < bodies[c].second) { target = -2; break; }
            if (chapter >= 0 && bodies[c].first >= nodes[chapter].start && bodies[c].second <= nodes[chapter].end) target = c;
        }
        if (target == -2) continue;
        if (target < 0) {
            if (chapter >= 0) continue;
            target = node.start < bodies.first().first ? 0 : chapters.size() - 1;
        }
        QTextDocument document;
        document.setHtml(html.mid(node.start, node.end - node.start));
        const auto text = document.toPlainText().trimmed();
        if (text.isEmpty()) continue;
        QString &destination = node.start < bodies[target].first ? chapters[target].notesBefore : chapters[target].notesAfter;
        if (!destination.isEmpty()) destination += QStringLiteral("\n\n");
        destination += text;
    }
}
}

QVector<WorkChapter> extractWorkChapters(const QString &html)
{
    const QRegularExpression divs(QStringLiteral("<!--.*?-->|<(/?)div\\b([^>]*)>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    QVector<WorkChapter> chapters;
    QVector<QPair<int,int>> bodies;
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
                    WorkChapter chapter{title,text};
                    const int trim = document.toPlainText().indexOf(text);
                    for (auto block = document.begin(); block.isValid(); block = block.next()) {
                        for (auto it = block.begin(); !it.atEnd(); ++it) {
                            const auto fragment = it.fragment(); if (!fragment.isValid()) continue;
                            const auto format = fragment.charFormat();
                            TextStyleSpan span;
                            span.start = qMax(0,fragment.position()-trim); span.end = qMin(int(text.size()),fragment.position()+fragment.length()-trim);
                            span.bold = format.fontWeight() >= QFont::Bold; span.italic = format.fontItalic();
                            span.underline = format.fontUnderline(); span.strike = format.fontStrikeOut();
                            span.code = format.fontFixedPitch() || format.fontFamilies().toStringList().contains("monospace");
                            if (!readingLink(format.anchorHref()).isEmpty()) span.url = format.anchorHref();
                            if (span.end>span.start && (span.bold || span.italic || span.underline || span.strike || span.code || !span.url.isEmpty())) chapter.styles.append(span);
                        }
                    }
                    chapters.append(chapter);
                    bodies.append({bodyStart, int(tag.capturedStart())});
                }
                previousBodyEnd = tag.capturedEnd();
                bodyStart = -1;
            }
            if (depth == chaptersDepth) chaptersDepth = -1;
            if (!excluded.isEmpty()) excluded.removeLast();
        }
    }
    if (!chapters.isEmpty()) attachNotes(html, chapters, bodies);
    return chapters;
}

QString extractWorkText(const QString &html)
{
    QStringList text;
    for (const auto &chapter : extractWorkChapters(html)) text.append(chapter.text);
    return text.join(QStringLiteral("\n\n"));
}
