#pragma once
#include "commentpage.h"
struct KudosResult { QString message; bool success=false; bool uncertain=false; };
inline QMap<QString,QString> parseKudosForm(const QString &html,const QString &id) {
    using namespace CommentHtml;
    const auto list=nodes(html);
    for(int i=0;i<list.size();++i) {
        const auto &n=list[i]; if(n.tag!="form" || n.attrs.value("id")!="new_kudo" || n.attrs.value("method").toLower()!="post") continue;
        bool story=false,feedback=false;
        for(int p=n.parent;p>=0;p=list[p].parent) {if(list[p].attrs.value("id")=="feedback")feedback=true;if(list[p].attrs.value("id")=="workskin" || hasClass(list[p],"userstuff"))story=true;}
        if(story || !feedback)continue;
        const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(n.attrs.value("action")));
        if(url.scheme()!="https" || url.host()!="archiveofourown.org" || !url.userInfo().isEmpty() || (url.port(-1)!=-1 && url.port()!=443) || url.path()!="/kudos" || url.hasQuery() || url.hasFragment()) return {};
        QMap<QString,QString> fields;
        for(int j=i+1;j<list.size();++j) if(list[j].tag=="input" && beneath(list,j,i) && list[j].attrs.value("type").toLower()=="hidden" && !list[j].attrs.contains("disabled")) {
            const auto name=list[j].attrs.value("name");
            if(QStringList{"authenticity_token","kudo[commentable_id]","kudo[commentable_type]"}.contains(name)) fields[name]=list[j].attrs.value("value");
        }
        if(fields.value("kudo[commentable_id]")==id && fields.value("kudo[commentable_type]")=="Work" && !fields.value("authenticity_token").isEmpty()) return fields;
    }
    return {};
}
inline bool kudosIdentityMatches(const QString &html,const QString &expected) {
    const auto list=CommentHtml::nodes(html);bool login=false,logout=false;QString user;
    for(int i=0;i<list.size();++i) {
        const auto &n=list[i];bool story=false,greeting=false;
        for(int p=n.parent;p>=0;p=list[p].parent) {if(list[p].attrs.value("id")=="greeting")greeting=true;if(list[p].attrs.value("id")=="workskin" || CommentHtml::hasClass(list[p],"userstuff"))story=true;}
        if(story)continue;
        const auto href=n.attrs.value(n.tag=="form"?"action":"href");if(href.isEmpty())continue;
        const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(href));
        if(url.scheme()!="https" || url.host()!="archiveofourown.org" || !url.userInfo().isEmpty())continue;
        if(url.path()=="/users/login")login=true;
        if(url.path()=="/users/logout")logout=true;
        if(greeting && QRegularExpression("^/users/[A-Za-z0-9_]+$").match(url.path()).hasMatch() && url.path()!="/users/login" && url.path()!="/users/logout")user=url.path().mid(7);
    }
    return expected.isEmpty()?login && !logout:logout && user==expected;
}
inline KudosResult parseKudosResult(const QString &html) {
    const auto list=CommentHtml::nodes(html);
    for(int i=0;i<list.size();++i) {
        const auto &n=list[i];bool feedback=false,story=CommentHtml::hasClass(n,"userstuff");
        for(int p=n.parent;p>=0;p=list[p].parent) { if(list[p].attrs.value("id")=="feedback")feedback=true;if(list[p].attrs.value("id")=="workskin" || CommentHtml::hasClass(list[p],"userstuff"))story=true; }
        if(!feedback || story)continue;
        const bool notice=CommentHtml::hasClass(n,"flash") && CommentHtml::hasClass(n,"kudos_notice");
        const bool error=CommentHtml::hasClass(n,"flash") && CommentHtml::hasClass(n,"kudos_error");
        if(notice || error) {
            const auto message=CommentHtml::text(html.mid(n.begin,n.end-n.begin));
            if(!message.isEmpty()) return {message,notice,false};
        }
    }
    return {"Could not confirm kudos. Check this work on AO3.",false,true};
}
