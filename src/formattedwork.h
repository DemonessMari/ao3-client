#pragma once
#include "commentpage.h"
#include "textstyle.h"
#include <QJsonObject>

inline QString readingCssUrls(QString css) {
    const QRegularExpression urls(R"rx(url\(\s*(['"]?)(/[^)'"\s]+)\1\s*\))rx",QRegularExpression::CaseInsensitiveOption);
    auto matches=urls.globalMatch(css);QVector<QPair<QPair<int,int>,QString>> edits;
    while(matches.hasNext()){const auto m=matches.next();const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(m.captured(2)));edits.prepend({{int(m.capturedStart()),int(m.capturedLength())},"url(\""+url.toString(QUrl::FullyEncoded).replace("\"","%22")+"\")"});}
    for(const auto &edit:edits)css.replace(edit.first.first,edit.first.second,edit.second);
    return css;
}

inline QString staticReadingHtml(QString html) {
    if(html.size()>40*1024*1024)return {};
    const QRegularExpression frames("<iframe\\b[^>]*>[\\s\\S]*?</iframe\\s*>",QRegularExpression::CaseInsensitiveOption);
    auto frameMatches=frames.globalMatch(html);QVector<QPair<QPair<int,int>,QString>> frameLinks;
    while(frameMatches.hasNext()){const auto m=frameMatches.next();const auto nodes=CommentHtml::nodes(m.captured());if(nodes.isEmpty())continue;const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(nodes.first().attrs.value("src")));const auto link=url.scheme()=="https" && !url.host().isEmpty() && url.userInfo().isEmpty()?"<p><a href=\""+url.toString().toHtmlEscaped()+"\" data-ao3-embed=\"1\">Open embedded player</a></p>":QString();frameLinks.prepend({{int(m.capturedStart()),int(m.capturedLength())},link});}
    for(const auto &link:frameLinks)html.replace(link.first.first,link.first.second,link.second);
    html.remove(QRegularExpression("<!--.*?-->|<(script|style|iframe|object|embed|form|textarea|button|select)\\b[^>]*>.*?</\\1\\s*>",QRegularExpression::DotMatchesEverythingOption|QRegularExpression::CaseInsensitiveOption));
    const QSet<QString> tags={"p","div","span","br","hr","h1","h2","h3","h4","h5","h6","blockquote","pre","code","kbd","samp","var","b","strong","i","em","u","s","strike","del","ins","sup","sub","small","big","abbr","acronym","cite","q","a","img","ul","ol","li","dl","dt","dd","table","thead","tbody","tfoot","tr","th","td","caption","colgroup","col","details","summary","ruby","rt","rp","audio","video","source","track"};
    const QSet<QString> common={"id","class","style","title","lang","dir"};
    const QMap<QString,QSet<QString>> extra={{"a",{"href","name","data-ao3-embed"}},{"img",{"src","alt","width","height"}},{"td",{"colspan","rowspan"}},{"th",{"colspan","rowspan","scope"}},{"ol",{"start","type"}},{"li",{"value"}},{"col",{"span","width"}},{"details",{"open"}},{"audio",{"src","controls","preload","loop"}},{"video",{"src","controls","preload","loop","poster","width","height"}},{"source",{"src","type","media"}},{"track",{"src","kind","srclang","label","default"}}};
    const QRegularExpression tokens(R"rx(<\s*(/?)\s*([A-Za-z][\w:-]*)((?:[^>"']|"[^"]*"|'[^']*')*)>)rx");
    auto matches=tokens.globalMatch(html);QString out;int cursor=0;
    while(matches.hasNext()) {
        const auto token=matches.next();out+=html.mid(cursor,token.capturedStart()-cursor);cursor=token.capturedEnd();const auto tag=token.captured(2).toLower();if(!tags.contains(tag))continue;
        if(!token.captured(1).isEmpty()){out+="</"+tag+">";continue;}
        out+="<"+tag;const auto parsed=CommentHtml::nodes(token.captured());if(!parsed.isEmpty())for(auto it=parsed[0].attrs.begin();it!=parsed[0].attrs.end();++it) {
            if(!common.contains(it.key()) && !extra.value(tag).contains(it.key()))continue;
            if(it.key()=="href" && !it.value().startsWith('#') && readingLink(it.value()).isEmpty())continue;
            if((it.key()=="src" || it.key()=="poster") && !it.value().startsWith("data:image/",Qt::CaseInsensitive) && !it.value().startsWith("data:audio/",Qt::CaseInsensitive) && !it.value().startsWith("data:video/",Qt::CaseInsensitive) && !it.value().startsWith("https://",Qt::CaseInsensitive) && !it.value().startsWith('/'))continue;
            auto value=it.key()=="href" && !it.value().startsWith('#')?readingLink(it.value()):it.value();
            if((it.key()=="src" || it.key()=="poster") && !value.startsWith("data:",Qt::CaseInsensitive))value=QUrl("https://archiveofourown.org").resolved(QUrl(value)).toString();
            if(it.key()=="preload")value="none";
            if(it.key()=="style")value=readingCssUrls(value);
            out+=" "+it.key()+"=\""+value.toHtmlEscaped()+"\"";
        }
        if(tag=="audio" || tag=="video")out+=" controls preload=\"none\"";
        out+=">";
    }
    return out+html.mid(cursor);
}
inline QSet<QUrl> readingEmbedTargets(const QString &html) {
    QSet<QUrl> out;for(const auto &node:CommentHtml::nodes(staticReadingHtml(html)))if(node.tag=="a" && node.attrs.value("data-ao3-embed")=="1"){const auto url=QUrl(node.attrs.value("href"));if(url.scheme()=="https" && !url.host().isEmpty() && url.userInfo().isEmpty() && url.host()!="archiveofourown.org" && !url.host().endsWith(".archiveofourown.org"))out.insert(url);}return out;
}
inline bool hasReadingMedia(const QString &html) {
    const auto safe=staticReadingHtml(html);
    return safe.contains("Open embedded player</a>") || QRegularExpression("<(?:img|audio|video|source)\\b[^>]*\\bsrc=\"(?:https://|data:(?:image|audio|video)/)",QRegularExpression::CaseInsensitiveOption).match(safe).hasMatch();
}
inline QString readingWorkCss(const QString &html) {
    const auto list=CommentHtml::nodes(html);QString css,imports;
    for(int i=0;i<list.size();++i)if(list[i].tag=="style")for(int p=list[i].parent;p>=0;p=list[p].parent)if(list[p].attrs.value("id")=="work-skin"){css+=html.mid(list[i].begin,list[i].end-list[i].begin)+"\n";break;}
    for(int i=0;i<list.size();++i)if(list[i].tag=="link" && list[i].attrs.value("rel").split(' ').contains("stylesheet"))for(int p=list[i].parent;p>=0;p=list[p].parent)if(list[p].attrs.value("id")=="work-skin"){const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(list[i].attrs.value("href")));if(url.scheme()=="https" && url.userInfo().isEmpty())imports+="@import url(\""+url.toString(QUrl::FullyEncoded).replace("\"","%22")+"\");\n";break;}
    return readingCssUrls((imports+css).left(262144));
}
inline QString formattedDocument(const QString &html,QString css,const QString &background,const QString &text,int size=18,double spacing=1.6,int margin=24,bool workSkin=true,const QByteArray &font={},const QString &family="serif",bool external=false) {
    QString body=staticReadingHtml(html);
    const QRegularExpression images("<img\\b[^>]*>",QRegularExpression::CaseInsensitiveOption);auto imagesFound=images.globalMatch(body);QVector<QPair<int,QString>> replace;
    while(!external && imagesFound.hasNext()){const auto m=imagesFound.next();const auto nodes=CommentHtml::nodes(m.captured());if(nodes.isEmpty() || nodes[0].attrs.value("src").startsWith("data:image/",Qt::CaseInsensitive))continue;replace.prepend({int(m.capturedStart()),"<p>"+nodes[0].attrs.value("alt","Image not saved. Save offline with images to include it.").toHtmlEscaped()+"</p>"});}
    for(const auto &r:replace){const auto m=images.match(body,r.first);body.replace(r.first,m.capturedLength(),r.second);}
    if(!external)body.replace(QRegularExpression("<(audio|video)\\b[^>]*>[\\s\\S]*?</\\1\\s*>",QRegularExpression::CaseInsensitiveOption),"<p>Audio/video requires external content.</p>");
    const auto fontFamily=QStringList{"serif","sans-serif","monospace"}.contains(family)?family:QString("serif");
    const bool validFont=font.size()>=12 && font.size()<=20*1024*1024 && (font.left(4)==QByteArray::fromHex("00010000") || font.left(4)=="OTTO");
    const auto fontCss=validFont?"@font-face{font-family:Ao3Imported;src:url(data:font/ttf;base64,"+QString::fromLatin1(font.toBase64())+")}body{font-family:Ao3Imported,serif}":"body{font-family:"+fontFamily+"}";
    css=readingCssUrls(css);
    css.replace("</","<\\/");
    auto document=QString("<!doctype html><html><head><meta charset=\"utf-8\"><meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; img-src data:; style-src 'unsafe-inline'; font-src data:; media-src data:; connect-src 'none'; frame-src 'none'; object-src 'none'; form-action 'none'; base-uri 'none'\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><style>html{background:%1;color:%2}body{margin:%3px;font:%4px serif;line-height:%5;overflow-wrap:break-word}img{max-width:100%;height:auto}table{max-width:100%;border-collapse:collapse}th,td{padding:.3em}pre{white-space:pre-wrap}a{color:inherit;text-decoration:underline}.landmark,.hidden{display:none}</style><style>%6</style><style>%8</style></head><body><div id=\"workskin\"><div id=\"chapters\"><div class=\"userstuff\">%7</div></div></div></body></html>").arg(background,text).arg(margin).arg(size).arg(spacing).arg(workSkin?css:QString(),body,fontCss);
    if(external){const int start=document.indexOf("default-src 'none'"),end=document.indexOf('"',start);auto policy=document.mid(start,end-start);policy.replace("img-src data:;","img-src data: https:;");policy.replace("style-src 'unsafe-inline';","style-src 'unsafe-inline' https:;");policy.replace("font-src data:;","font-src data: https:;");policy.replace("media-src data:;","media-src data: https:;");document.replace(start,end-start,policy);}
    return document;
}
