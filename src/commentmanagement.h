#pragma once
#include "archivepages.h"
struct CommentControl {QString label,path;QMap<QString,QString> fields;};
inline QMap<QString,CommentControl> commentControls(const ArchivePages::Document &d,const QString &id) {
    using namespace CommentHtml;QMap<QString,CommentControl> out;int nav=-1;
    for(int i=0;i<d.list.size();++i)if(d.list[i].attrs.value("id")=="navigation_for_comment_"+id && d.trusted(i)){nav=i;break;}
    if(nav<0)return out;
    const QMap<QString,QString> labels={{"freeze","Freeze thread"},{"unfreeze","Unfreeze thread"},{"hide","Hide comment"},{"unhide","Show comment"},{"review","Approve comment"},{"reject","Mark as spam"},{"approve","Not spam"}};
    QString token;for(int i:d.select("meta",{}))if(d.list[i].attrs.value("name")=="csrf-token"){for(int p=d.list[i].parent;p>=0;p=d.list[p].parent)if(d.list[p].tag=="head")token=d.list[i].attrs.value("content");}
    for(int i:d.select("a",{},nav)) {
        const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(d.list[i].attrs.value("href")));const auto path=ArchivePages::path(d.list[i].attrs.value("href"));QString source=path;if(url.hasQuery())source+="?"+url.query(QUrl::FullyEncoded);
        if(path=="/comments/"+id+"/edit")out["Edit"]={"Edit",source,{}};
        if(path=="/comments/delete_comment" && QUrlQuery(url).queryItemValue("id")==id)out["Delete"]={"Delete",source,{}};
        const auto action=QRegularExpression("^/comments/"+id+"/([a-z]+)$").match(path).captured(1);
        if(labels.contains(action) && d.list[i].attrs.value("data-method")=="put" && !token.isEmpty())out[labels.value(action)]={labels.value(action),path,{{"authenticity_token",token},{"_method","put"}}};
    }
    for(int i:d.select("form",{},nav)) {
        if(d.list[i].attrs.value("method").toLower()!="post")continue;
        const auto path=ArchivePages::path(d.list[i].attrs.value("action"));const auto action=QRegularExpression("^/comments/"+id+"/([a-z]+)$").match(path).captured(1);if(!labels.contains(action))continue;
        QMap<QString,QString> fields;for(int j:d.select("input",{},i))if(!d.list[j].attrs.contains("disabled") && d.list[j].attrs.value("type")=="hidden" && QStringList{"authenticity_token","_method"}.contains(d.list[j].attrs.value("name")))fields[d.list[j].attrs.value("name")]=d.list[j].attrs.value("value");
        if(fields.value("_method")=="put" && !fields.value("authenticity_token").isEmpty())out[labels.value(action)]={labels.value(action),path,fields};
    }
    return out;
}
inline CommentControl commentDeleteControl(const QString &html,const QString &id) {
    ArchivePages::Document d(html);
    for(int i:d.select("form",{}))if(d.list[i].attrs.value("method").toLower()=="post" && ArchivePages::path(d.list[i].attrs.value("action"))=="/comments/"+id){
        QMap<QString,QString> fields;for(int j:d.select("input",{},i))if(!d.list[j].attrs.contains("disabled") && d.list[j].attrs.value("type")=="hidden" && QStringList{"authenticity_token","_method"}.contains(d.list[j].attrs.value("name")))fields[d.list[j].attrs.value("name")]=d.list[j].attrs.value("value");
        if(fields.value("_method")=="delete" && !fields.value("authenticity_token").isEmpty())return {"Delete","/comments/"+id,fields};
    }
    return {};
}
