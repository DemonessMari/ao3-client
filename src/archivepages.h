#pragma once
#include "siteidentity.h"
struct ArchiveEntry {QString id,title,body,target;QStringList actions;QString selection;bool unread=false;};
struct ArchivePage {QString title,error;QVector<ArchiveEntry> entries;int page=1,pages=1;bool canSubscribe=false,subscribed=false;QString description;};
struct PageAction {QString path,error;QMap<QString,QString> fields;};
namespace ArchivePages {
using namespace CommentHtml;
inline QString path(const QString &value){const auto u=QUrl("https://archiveofourown.org").resolved(QUrl(value));return u.scheme()=="https" && u.host()=="archiveofourown.org" && u.port(443)==443 && u.userInfo().isEmpty() && !u.hasFragment()?u.path():QString();}
inline QString route(const QString &kind,const QString &user,const QString &id={}) {
    if(kind=="series")return QRegularExpression("^[0-9]+$").match(id).hasMatch()?"/series/"+id:QString();
    if(kind=="history" && QRegularExpression("^[A-Za-z0-9_]+$").match(user).hasMatch())return "/users/"+user+"/readings";
    return QStringList{"bookmarks","subscriptions","inbox"}.contains(kind) && QRegularExpression("^[A-Za-z0-9_]+$").match(user).hasMatch()?"/users/"+user+"/"+kind:QString();
}
struct Document {
    QString html;QVector<Node> list;
    explicit Document(const QString &h):html(h),list(nodes(h)){}
    bool trusted(int i) const {for(int p=i;p>=0;p=list[p].parent)if(hasClass(list[p],"userstuff"))return false;return true;}
    bool inside(int i,int root) const{return root<0 || beneath(list,i,root);}
    QVector<int> select(const QString &tag,const QStringList &classes,int root=-1,bool trust=true) const {
        QVector<int> out;for(int i=0;i<list.size();++i){if((!tag.isEmpty() && list[i].tag!=tag) || !inside(i,root) || (trust && !trusted(i)))continue;bool matches=true;for(const auto &c:classes)matches &= hasClass(list[i],c);if(matches)out.append(i);}return out;
    }
    int first(const QString &tag,const QStringList &classes,int root=-1,bool trust=true) const{return select(tag,classes,root,trust).value(0,-1);}
    QString textAt(int i) const{return i<0?QString():text(html.mid(list[i].begin,list[i].end-list[i].begin));}
    QString target(int root) const {if(root<0)return {};for(int i:select("a",{},root)){const auto p=path(list[i].attrs.value("href"));if(QRegularExpression("^/(?:works/[0-9]+(?:/chapters/[0-9]+)?|series/[0-9]+|users/[A-Za-z0-9_]+(?:/pseuds/[^/]+)?)$").match(p).hasMatch())return p;}return {};}
};
inline ArchivePage parse(const QString &html,const QString &kind,const QString &user,const QString &id,int page) {
    ArchivePage out;out.page=out.pages=page;
    if(html.size()>8*1024*1024){out.error="AO3 page is too large.";return out;}
    if(kind!="series" && !siteIdentityMatches(html,user)){out.error="AO3 could not verify this account. Check native sign-in.";return out;}
    const QMap<QString,QString> markers={{"series","series-show"},{"bookmarks","bookmarks-index"},{"subscriptions","subscriptions-index"},{"inbox","inbox-show"},{"history","readings-index"}};
    if(!markers.contains(kind)){out.error="Unknown AO3 list.";return out;}
    Document d(html);int main=-1;for(int i=0;i<d.list.size();++i)if(d.list[i].attrs.value("id")=="main" && hasClass(d.list[i],markers.value(kind)) && d.trusted(i)){main=i;break;}
    if(main<0){out.error="AO3 did not return this list. Open it on AO3 to check availability.";return out;}
    out.title=d.textAt(d.first("h2",{"heading"},main));
    if(kind=="history" && !d.select({}, {"error"}).isEmpty()){out.error="AO3 reported an error while updating reading history.";return out;}
    if(kind=="history") {
        const int root=d.first("ol",{"reading","work"},main);
        if(root>=0)for(int i:d.select("li",{"reading","work"},root)) {
            if(d.list[i].parent!=root)continue;
            ArchiveEntry e;const int heading=d.first("h4",{"heading"},i);e.title=d.textAt(heading);if(e.title.isEmpty())e.title="Unavailable work";e.target=d.target(heading);
            e.body=d.textAt(d.first("h4",{"viewed"},i));
            for(int form:d.select("form",{},i)) {
                const auto key=QRegularExpression("^/users/"+QRegularExpression::escape(user)+"/readings/([0-9]+)$").match(path(d.list[form].attrs.value("action"))).captured(1);
                if(key.isEmpty() || d.list[form].attrs.value("method").toLower()!="post")continue;
                QMap<QString,QString> fields;for(int n:d.select("input",{},form))if(!d.list[n].attrs.contains("disabled") && d.list[n].attrs.value("type")=="hidden")fields[d.list[n].attrs.value("name")]=d.list[n].attrs.value("value");
                if(fields.value("reading")==key && fields.value("_method")=="delete" && !fields.value("authenticity_token").isEmpty()){e.id=key;e.actions={"Delete from history"};break;}
            }
            if(e.id.isEmpty())e.id="unavailable-"+QString::number(i);
            out.entries.append(e);
        }
    } else if(kind=="series" || kind=="bookmarks") {
        const int root=d.first(kind=="series"?"ul":"ol",kind=="series"?QStringList{"series","work"}:QStringList{"bookmark"},main);
        if(root>=0)for(int i:d.select("li",{kind=="series"?"work":"bookmark"},root)) {
            if(d.list[i].parent!=root)continue;
            ArchiveEntry e;e.id=d.list[i].attrs.value("id");if(e.id.isEmpty())e.id="entry-"+QString::number(i);const int heading=d.first("h4",{"heading"},i);e.title=d.textAt(heading);if(e.title.isEmpty())e.title="Unavailable work";e.target=d.target(heading);if(e.target.isEmpty() && kind=="bookmarks"){const auto key=QRegularExpression("^bookmark_([0-9]+)$").match(e.id).captured(1);if(!key.isEmpty())e.target="/bookmarks/"+key;}
            QStringList body;for(int j:d.select({}, {},i,false))if(d.list[j].tag=="blockquote" || (d.list[j].tag=="dl" && hasClass(d.list[j],"stats")))body<<d.textAt(j);e.body=body.join("\n\n");out.entries.append(e);
        }
    } else if(kind=="subscriptions") {
        const int root=d.first("dl",{"subscription"},main);
        if(root>=0)for(int i:d.select("dt",{},root)) {
            if(d.list[i].parent!=root)continue;
            ArchiveEntry e;e.title=d.textAt(i);e.target=d.target(i);int dd=-1;for(int j=i+1;j<d.list.size();++j)if(d.list[j].parent==root){if(d.list[j].tag=="dd")dd=j;break;}
            const int form=dd>=0?d.first("form",{},dd):-1;
            if(form>=0){e.id=QRegularExpression("^/users/"+QRegularExpression::escape(user)+"/subscriptions/([0-9]+)$").match(path(d.list[form].attrs.value("action"))).captured(1);if(!e.id.isEmpty())e.actions<<"Unsubscribe";}
            if(e.id.isEmpty())e.id="missing-"+QString::number(i);
            out.entries.append(e);
        }
    } else {
        for(int i:d.select("li",{},main)) {
            const auto key=QRegularExpression("^feedback_comment_([0-9]+)$").match(d.list[i].attrs.value("id")).captured(1);if(key.isEmpty())continue;
            ArchiveEntry e;e.id=key;e.target="/comments/"+key;e.title=d.textAt(d.first("h4",{"byline"},i));if(e.title.isEmpty())e.title="Unavailable comment";e.body=d.textAt(d.first("blockquote",{"userstuff"},i,false));if(e.body.isEmpty())e.body=d.textAt(d.first("p",{"message"},i));e.unread=hasClass(d.list[i],"unread");
            for(int j:d.select("input",{},i))if(d.list[j].attrs.value("name")=="inbox_comments[]")e.selection=d.list[j].attrs.value("value");
            if(QRegularExpression("^[0-9]+$").match(e.selection).hasMatch())e.actions={e.unread?QString("Mark read"):QString("Mark unread"),"Delete from inbox"};
            out.entries.append(e);
        }
    }
    if(kind=="series")out.description=d.textAt(d.first("dl",{"series","meta"},main));
    const auto expected=route(kind,user,id);
    for(int i:d.select({}, {"pagination"},main))for(int j:d.select({}, {},i)) {
        const int number=d.textAt(j).toInt();if(number>0 && (hasClass(d.list[j],"current") || d.list[j].attrs.value("aria-current")=="page"))out.page=number;
        if(d.list[j].tag!="a" || path(d.list[j].attrs.value("href"))!=expected)continue;
        const auto u=QUrl("https://archiveofourown.org").resolved(QUrl(d.list[j].attrs.value("href")));const int n=QUrlQuery(u).queryItemValue("page").toInt();if(n>0 && n<=1000000)out.pages=qMax(out.pages,n);
    }
    out.pages=qMax(out.page,out.pages);return out;
}
inline PageAction action(const QString &html,const QString &kind,const QString &user,const QString &id,int page,const QString &entryId,const QString &label) {
    if(html.size()>8*1024*1024)return {{},"AO3 page is too large.",{}};
    if(kind=="history" && label=="Clear history") {
        if(!siteIdentityMatches(html,user))return {{},"AO3 could not verify this account.",{}};
        Document d(html);for(int form:d.select("form",{})) {
            const auto expected="/users/"+user+"/readings/clear";
            if(d.list[form].attrs.value("method").toLower()!="post" || path(d.list[form].attrs.value("action"))!=expected)continue;
            QString token;bool method=false;for(int n:d.select("input",{},form))if(!d.list[n].attrs.contains("disabled") && d.list[n].attrs.value("type")=="hidden"){if(d.list[n].attrs.value("name")=="authenticity_token")token=d.list[n].attrs.value("value");if(d.list[n].attrs.value("name")=="_method")method=true;}
            if(!token.isEmpty() && !method)return {expected,{},{{"authenticity_token",token}}};
        }
        return {{},"AO3 did not return a history-clearing form.",{}};
    }
    const auto parsed=parse(html,kind,user,id,page);if(!parsed.error.isEmpty())return {{},parsed.error,{}};
    ArchiveEntry entry;bool found=false;for(const auto &e:parsed.entries)if(e.id==entryId && e.actions.contains(label)){entry=e;found=true;break;}
    if(!found)return {{},"This entry changed. Refresh the list.",{}};
    Document d(html);const QString expected="/users/"+user+(kind=="subscriptions"?"/subscriptions/"+entryId:kind=="history"?"/readings/"+entryId:"/inbox");
    for(int i:d.select("form",{})) {
        const auto &form=d.list[i];if(form.attrs.value("method").toLower()!="post" || path(form.attrs.value("action"))!=expected || (kind=="inbox" && form.attrs.value("id")!="inbox-form"))continue;
        PageAction out;const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(form.attrs.value("action")));out.path=url.path();if(url.hasQuery())out.path+="?"+url.query(QUrl::FullyEncoded);
        for(int j:d.select("input",{},i))if(!d.list[j].attrs.contains("disabled") && d.list[j].attrs.value("type")=="hidden" && (QStringList{"authenticity_token","_method"}.contains(d.list[j].attrs.value("name")) || (kind=="history" && d.list[j].attrs.value("name")=="reading")))out.fields[d.list[j].attrs.value("name")]=d.list[j].attrs.value("value");
        if(out.fields.value("authenticity_token").isEmpty() || out.fields.value("_method")!=(kind=="inbox"?"put":"delete"))continue;
        if(kind=="history" && out.fields.value("reading")!=entryId)continue;
        if(kind=="inbox") {
            const QString key=label=="Mark read"?"read":label=="Mark unread"?"unread":"delete";bool button=false;
            for(int j:d.select({}, {},i))if((d.list[j].tag=="input" || d.list[j].tag=="button") && !d.list[j].attrs.contains("disabled") && d.list[j].attrs.value("name")==key){out.fields[key]=d.list[j].attrs.value("value");button=true;}
            if(!button)continue;
            out.fields["inbox_comments[]"]=entry.selection;
        }
        return out;
    }
    return {{},"AO3 did not return an available action form.",{}};
}
}
