#include "workcontent.h"
#include <QGuiApplication>
#include <QDebug>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QString html = QStringLiteral(
        "<div class='summary'><blockquote>Exclude summary</blockquote></div>"
        "<div id='chapters'><div class='chapter'>"
        "<div role='article' class='userstuff module'><h3 class='landmark heading' id='work'>Chapter Text</h3>"
        "<p>First &amp; &#9731;</p><div><p>Nested text</p></div><p>Last paragraph</p></div>"
        "<div class='notes'><p>Exclude notes</p></div></div>"
        "<div class=\"module userstuff\"><p>Second&nbsp;chapter</p></div></div>"
        "<div>Exclude footer</div>");
    const QString text = extractWorkText(html);
    if (!text.contains(QStringLiteral("First & ☃")) ||
        !text.contains(QStringLiteral("Nested text")) ||
        !text.contains(QStringLiteral("Last paragraph")) ||
        !text.contains(QStringLiteral("Second chapter")) ||
        text.contains(QStringLiteral("Exclude")) || text.contains(QStringLiteral("Chapter Text")) ||
        !extractWorkText(QStringLiteral("<p>Login required</p>")).isEmpty() ||
        !extractWorkText(QStringLiteral("<div class='userstuff module'><p>Truncated")).isEmpty()) {
        qCritical() << "Unexpected extraction:" << text;
        return 1;
    }
    // AO3 works/show: a single chapter uses plain userstuff, not module.
    const QString single = QStringLiteral(
        "<div id='workskin'><div class='preface'><div class='summary'><blockquote class='userstuff'>Exclude summary</blockquote></div></div>"
        "<div id='chapters' role='article'><h3 class='landmark heading' id='work'>Work Text:</h3>"
        "<!-- <div>comment must not alter nesting</div> -->"
        "<div class='userstuff'><p>Single chapter &amp; text.</p><div><h3>A story heading</h3><p>Nested paragraph.</p></div><p>The end.</p></div>"
        "</div><div class='afterword preface group'><div class='userstuff module'>Exclude endnotes</div></div></div>");
    const QString singleText = extractWorkText(single);
    if (!singleText.contains(QStringLiteral("Single chapter & text.")) ||
        !singleText.contains(QStringLiteral("A story heading")) ||
        !singleText.contains(QStringLiteral("Nested paragraph.")) ||
        !singleText.contains(QStringLiteral("The end.")) ||
        singleText.contains(QStringLiteral("Exclude")) || singleText.contains(QStringLiteral("Work Text:"))) {
        qCritical() << "Unexpected single-chapter extraction:" << singleText;
        return 1;
    }
    return 0;
}
