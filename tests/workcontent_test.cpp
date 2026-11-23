#include "workcontent.h"
#include "readinglibrary.h"
#include <QTemporaryDir>
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
    const auto formatted=extractWorkChapters("<div id='work-skin'><style>#workskin .poem {white-space:pre-wrap}</style><div id='chapters'><div class='userstuff'><p class='poem'>One<br>Two</p><table><tr><td>Cell</td></tr></table><a href='#note'>Note</a><img src='https://example.org/a.png' alt='Illustration'><script>bad()</script></div></div></div>");
    if(formatted.size()!=1 || !formatted[0].html.contains("<table>") || !formatted[0].workCss.contains("white-space") || formatted[0].html.contains("<script")){qCritical()<<"Formatted extraction failed";return 1;}
    const auto page=formattedDocument(formatted[0].html,formatted[0].workCss,"#ffffff","#000000");
    if(!page.contains("default-src 'none'") || page.contains("<img src=\"https:") || !page.contains("Illustration")){qCritical()<<"Formatted isolation failed";return 1;}
    const auto hostile=staticReadingHtml("<p onclick='bad()'>Safe</p><a href='javascript:bad()'>Bad</a><img src='file:///secret'><form><input></form>");
    if(hostile.contains("onclick") || hostile.contains("javascript:") || hostile.contains("file:") || hostile.contains("<form")){qCritical()<<"Static sanitization failed";return 1;}
    const auto literal=formattedDocument("<p>Literal %1 and %8</p>",{},"#ffffff","#000000");
    if(!literal.contains("Literal %1 and %8")){qCritical()<<"Percent tokens changed story text";return 1;}
    const auto fontPage=formattedDocument("<p>Text</p>",{},"#ffffff","#000000",18,1.6,24,true,QByteArray::fromHex("000100000000000000000000"));
    if(!fontPage.contains("data:font/ttf;base64,")){qCritical()<<"Imported font missing from formatted document";return 1;}
    QTemporaryDir directory;ReadingLibrary library(directory.path());Work work;work.id="12";work.title="Formatted";
    if(!library.saveOffline(work,formatted) || library.load("12").first().html!=formatted.first().html || library.load("12").first().workCss!=formatted.first().workCss){qCritical()<<"Formatted persistence failed";return 1;}
    return 0;
}
