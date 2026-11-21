#pragma once
#include <QString>
#include <QVector>
#include <QMap>
#include <QRegularExpression>
#include <QTextDocument>
#include <QUrlQuery>
#include <QUrl>

struct ArchiveComment { QString id, author, authorPath, date, text; int depth = 0; };
struct CommentPage { QVector<ArchiveComment> comments; int page = 1, pages = 1; QString notice, error; };
namespace CommentHtml {
struct Node { QString tag; QMap<QString,QString> attrs; int parent=-1, begin=0, end=0; };
inline QString text(const QString &html) { QTextDocument doc; doc.setHtml(html); return doc.toPlainText().trimmed(); }
inline QVector<Node> nodes(const QString &html) {
    QVector<Node> result; QVector<int> stack;
    const QRegularExpression tags(R"rx(<!--[\s\S]*?-->|<\s*(/?)\s*([A-Za-z][\w:-]*)((?:[^>"']|"[^"]*"|'[^']*')*)>)rx");
    const QRegularExpression attrs(R"rx(([\w:-]+)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s"'=<>`]+)))?)rx");
    const QStringList voids={"input","br","img","hr","meta","link","area","base","embed","source","wbr"};
    auto matches=tags.globalMatch(html);
    while(matches.hasNext()) {
        const auto m=matches.next(); if(m.captured().startsWith("<!--")) continue;
        const QString tag=m.captured(2).toLower(); const bool close=!m.captured(1).isEmpty();
        if(!stack.isEmpty() && (result[stack.last()].tag=="script" || result[stack.last()].tag=="style") && (!close || tag!=result[stack.last()].tag)) continue;
        if(close) {
            int pos=stack.size()-1; while(pos>=0 && result[stack[pos]].tag!=tag) --pos;
            if(pos>=0) { while(stack.size()>pos) result[stack.takeLast()].end=m.capturedStart(); }
        } else {
            Node node; node.tag=tag; node.parent=stack.isEmpty()?-1:stack.last(); node.begin=m.capturedEnd(); node.end=html.size();
            auto am=attrs.globalMatch(m.captured(3)); while(am.hasNext()) {const auto a=am.next(); node.attrs[a.captured(1).toLower()]=text(a.captured(2).isNull()?(a.captured(3).isNull()?a.captured(4):a.captured(3)):a.captured(2));}
            result.append(node);
            if(!voids.contains(tag) && !m.captured(3).trimmed().endsWith('/')) stack.append(result.size()-1);
        }
    }
    return result;
}
inline bool hasClass(const Node &node,const QString &name) { return node.attrs.value("class").split(QRegularExpression("\\s+"),Qt::SkipEmptyParts).contains(name); }
inline bool beneath(const QVector<Node> &nodes,int child,int parent) { for(int p=nodes[child].parent;p>=0;p=nodes[p].parent) if(p==parent)return true; return false; }
}
inline CommentPage parseCommentPage(const QString &html,int page=1) {
    using namespace CommentHtml; CommentPage result; result.page=page; result.pages=page;
    if(html.size()>8*1024*1024) {result.error="Comments page is too large";return result;}
    const auto list=nodes(html); bool recognized=false; const QRegularExpression idRx("^comment_(\\d+)$");
    for(int i=0;i<list.size();++i) {
        const auto &node=list[i];
        if(node.attrs.value("id")=="comments_placeholder" || node.attrs.value("id")=="feedback" || hasClass(node,"thread")) recognized=true;
        if(hasClass(node,"pagination")) for(int j=i+1;j<list.size();++j) if(list[j].tag=="a" && beneath(list,j,i)) result.pages=qMax(result.pages,QUrlQuery(QUrl(list[j].attrs.value("href"))).queryItemValue("page").toInt());
        const auto id=idRx.match(node.attrs.value("id")); if(node.tag!="li" || !id.hasMatch() || !hasClass(node,"comment"))continue;
        recognized=true; ArchiveComment comment; comment.id=id.captured(1);
        for(int p=node.parent;p>=0;p=list[p].parent) if(list[p].tag=="ol" && hasClass(list[p],"thread")) ++comment.depth;
        comment.depth=qBound(0,comment.depth-1,8);
        int byline=-1;
        for(int j=i+1;j<list.size() && list[j].begin<node.end;++j) {
            const auto &part=list[j]; if(!beneath(list,j,i))continue;
            const QString body=html.mid(part.begin,part.end-part.begin);
            if(part.tag=="h4" && hasClass(part,"byline")) {byline=j;comment.author=text(body);}
            if(hasClass(part,"posted") && hasClass(part,"datetime")) comment.date=text(body);
            if(part.tag=="blockquote" && hasClass(part,"userstuff") && comment.text.isEmpty())comment.text=text(body);
            if(part.tag=="a" && byline>=0 && beneath(list,j,byline) && comment.authorPath.isEmpty() && part.attrs.value("href").startsWith("/users/")) {comment.authorPath=part.attrs.value("href");comment.author=text(body);}
            if(part.tag=="p" && comment.text.isEmpty() && (hasClass(part,"message") || comment.author.isEmpty())) comment.text=text(body);
        }
        if(!comment.date.isEmpty() && comment.author.endsWith(comment.date)) comment.author=comment.author.left(comment.author.size()-comment.date.size()).trimmed();
        if(comment.author.isEmpty())comment.author="Comment";
        result.comments.append(comment);
    }
    if(!recognized)result.error="AO3 did not return a comments page. Sign in if the work is restricted.";
    return result;
}

struct CommentForm {
    QString action, source, context, notice, error;
    QMap<QString,QString> hidden;
    QVector<QPair<QString,QString>> pseuds;
    bool guest=false;
};
struct CommentSubmission { QString notice,error; bool uncertain=false; };
inline bool validCommentAction(const QString &action) {
    const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(action));
    return url.scheme()=="https" && url.host()=="archiveofourown.org" && url.port(443)==443 && url.userInfo().isEmpty() &&
        QRegularExpression("^/(?:works/\\d+/|chapters/\\d+/|comments/\\d+/)?comments$").match(url.path()).hasMatch();
}
inline CommentForm parseCommentForm(const QString &html) {
    using namespace CommentHtml;CommentForm result;const auto list=nodes(html);
    for(int i=0;i<list.size();++i) {
        if(list[i].tag!="form" || list[i].attrs.value("method").toLower()!="post")continue;
        bool content=false;for(int j=i+1;j<list.size();++j) if(beneath(list,j,i) && list[j].tag=="textarea" && list[j].attrs.value("name")=="comment[comment_content]")content=true;
        if(!content)continue;
        result.action=list[i].attrs.value("action");if(!validCommentAction(result.action)) {result.error="AO3 returned an invalid comment form";return result;}
        for(int j=i+1;j<list.size();++j) {
            const auto &part=list[j];if(!beneath(list,j,i) || part.attrs.contains("disabled"))continue;
            const auto name=part.attrs.value("name");const auto value=part.attrs.value("value");
            if(part.tag=="input" && part.attrs.value("type").toLower()=="hidden")result.hidden[name]=value;
            if(part.tag=="input" && part.attrs.value("type").toLower()=="submit" && !name.isEmpty())result.hidden[name]=value;
            if(name=="comment[name]" || name=="comment[email]")result.guest=true;
            if(part.tag=="select" && name=="comment[pseud_id]") for(int k=j+1;k<list.size();++k) if(list[k].tag=="option" && beneath(list,k,j)) {
                const auto id=list[k].attrs.value("value");result.pseuds.append({id,text(html.mid(list[k].begin,list[k].end-list[k].begin))});
                if(list[k].attrs.contains("selected") || !result.hidden.contains(name))result.hidden[name]=id;
            }
            if(hasClass(part,"notice"))result.notice+=text(html.mid(part.begin,part.end-part.begin))+"\n";
        }
        if(result.hidden.value("authenticity_token").isEmpty() || result.hidden.contains("_method"))result.error="AO3 did not return a valid new-comment form";
        return result;
    }
    result.error="Commenting is unavailable. Check the work's permissions or sign in.";return result;
}
inline CommentSubmission parseCommentSubmission(const QString &html) {
    using namespace CommentHtml;CommentSubmission result;const auto list=nodes(html);
    for(int i=0;i<list.size();++i) {
        const auto &node=list[i];bool story=hasClass(node,"userstuff");
        for(int p=node.parent;p>=0;p=list[p].parent) if(list[p].attrs.value("id")=="workskin" || hasClass(list[p],"userstuff"))story=true;
        if(story)continue;
        if(hasClass(node,"flash") && hasClass(node,"comment_notice"))result.notice=text(html.mid(node.begin,node.end-node.begin));
        if(node.attrs.value("id")=="errorExplanation" || node.attrs.value("id")=="error" || hasClass(node,"errorExplanation") || (hasClass(node,"flash") && hasClass(node,"comment_error")))result.error=text(html.mid(node.begin,node.end-node.begin));
    }
    if(result.notice.isEmpty() && result.error.isEmpty()) {result.uncertain=true;result.error="Could not confirm whether the comment was posted. Refresh comments before trying again.";}
    return result;
}
