#pragma once
#include "commentpage.h"
struct SubscriptionForm { QString path; QMap<QString,QString> fields; bool subscribed=false; };
inline SubscriptionForm parseWorkSubscription(const QString &html,const QString &id,const QString &user) {
    using namespace CommentHtml;
    if(!QRegularExpression("^[A-Za-z0-9_]+$").match(user).hasMatch())return {};
    const auto list=nodes(html);
    for(int i=0;i<list.size();++i) {
        const auto &n=list[i];if(n.tag!="form" || n.attrs.value("method").toLower()!="post")continue;
        bool subscribe=false,story=false;
        for(int p=n.parent;p>=0;p=list[p].parent) {if(hasClass(list[p],"subscribe"))subscribe=true;if(list[p].attrs.value("id")=="workskin" || hasClass(list[p],"userstuff"))story=true;}
        if(!subscribe || story)continue;
        const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(n.attrs.value("action")));
        if(url.scheme()!="https" || url.host()!="archiveofourown.org" || !url.userInfo().isEmpty() || (url.port(-1)!=-1 && url.port()!=443) || url.hasQuery() || url.hasFragment())continue;
        const QString base="/users/"+user+"/subscriptions";
        const bool subscribed=QRegularExpression("^"+QRegularExpression::escape(base)+"/[0-9]+$").match(url.path()).hasMatch();
        if(url.path()!=base && !subscribed)continue;
        QMap<QString,QString> fields;
        for(int j=i+1;j<list.size();++j)if(list[j].tag=="input" && beneath(list,j,i) && list[j].attrs.value("type").toLower()=="hidden" && !list[j].attrs.contains("disabled")) {
            const auto name=list[j].attrs.value("name");if(QStringList{"authenticity_token","_method","subscription[subscribable_id]","subscription[subscribable_type]"}.contains(name))fields[name]=list[j].attrs.value("value");
        }
        if(fields.value("subscription[subscribable_id]")!=id || fields.value("subscription[subscribable_type]")!="Work" || fields.value("authenticity_token").isEmpty())continue;
        if(subscribed?fields.value("_method")!="delete":fields.contains("_method"))continue;
        return {url.path(),fields,subscribed};
    }
    return {};
}
