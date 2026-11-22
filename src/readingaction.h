#pragma once
#include "kudos.h"
struct ReadingAction { QString action; QMap<QString,QString> fields; };
inline ReadingAction parseReadingAction(const QString &html,const QString &id) {
    using namespace CommentHtml;
    const auto list=nodes(html);
    for(int i=0;i<list.size();++i) {
        const auto &n=list[i];if(n.tag!="form" || n.attrs.value("method").toLower()!="post")continue;
        bool mark=false,story=false;
        for(int p=n.parent;p>=0;p=list[p].parent) { if(hasClass(list[p],"mark"))mark=true;if(list[p].attrs.value("id")=="workskin" || hasClass(list[p],"userstuff"))story=true; }
        if(!mark || story)continue;
        const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(n.attrs.value("action")));
        if(url.scheme()!="https" || url.host()!="archiveofourown.org" || !url.userInfo().isEmpty() || (url.port(-1)!=-1 && url.port()!=443) || url.hasQuery() || url.hasFragment())continue;
        QString action;for(const auto &a:QStringList{"mark_for_later","mark_as_read"})if(url.path()=="/works/"+id+"/"+a)action=a;
        if(action.isEmpty())continue;
        QMap<QString,QString> fields;
        for(int j=i+1;j<list.size();++j)if(list[j].tag=="input" && beneath(list,j,i) && list[j].attrs.value("type").toLower()=="hidden" && !list[j].attrs.contains("disabled")) {
            const auto name=list[j].attrs.value("name");if(name=="_method" || name=="authenticity_token")fields[name]=list[j].attrs.value("value");
        }
        if(fields.value("_method")=="patch" && !fields.value("authenticity_token").isEmpty())return {action,fields};
    }
    return {};
}
