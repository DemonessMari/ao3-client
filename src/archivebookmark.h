#pragma once
#include "commentpage.h"
struct ArchiveBookmarkForm {
    QString workId,id,path,token,notes,tags,collections,pseud,error;
    QVector<QPair<QString,QString>> pseuds;
    bool privateBookmark=false,recommendation=false;
};
inline ArchiveBookmarkForm parseArchiveBookmark(const QString &html,const QString &workId) {
    using namespace CommentHtml;ArchiveBookmarkForm out;out.workId=workId;const auto list=nodes(html);
    for(int i=0;i<list.size();++i) {
        const auto &node=list[i];if(node.tag!="form" || node.attrs.value("method").toLower()!="post")continue;
        bool bookmark=false,story=false,feedback=false;
        for(int p=node.parent;p>=0;p=list[p].parent) {if(list[p].attrs.value("id")=="bookmark-form")bookmark=true;if(list[p].attrs.value("id")=="feedback")feedback=true;if(list[p].attrs.value("id")=="workskin" || hasClass(list[p],"userstuff"))story=true;}
        if(!bookmark || !feedback || story)continue;
        const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(node.attrs.value("action")));
        if(url.scheme()!="https" || url.host()!="archiveofourown.org" || !url.userInfo().isEmpty() || url.port(443)!=443 || url.hasQuery() || url.hasFragment())continue;
        const auto edit=QRegularExpression("^/bookmarks/([0-9]+)$").match(url.path());
        if(!edit.hasMatch() && url.path()!="/works/"+workId+"/bookmarks")continue;
        out.path=url.path();out.id=edit.captured(1);QString method;
        for(int j=i+1;j<list.size();++j) {
            const auto &n=list[j];if(!beneath(list,j,i) || n.attrs.contains("disabled"))continue;
            const auto name=n.attrs.value("name"),value=n.attrs.value("value");
            if(n.tag=="input") {
                if(name=="authenticity_token" && n.attrs.value("type")=="hidden")out.token=value;
                if(name=="_method")method=value;
                if(name=="bookmark[tag_string]")out.tags=value;
                if(name=="bookmark[collection_names]")out.collections=value;
                if(name=="bookmark[pseud_id]")out.pseud=value;
                if(n.attrs.value("type")=="checkbox" && n.attrs.contains("checked")) {if(name=="bookmark[private]")out.privateBookmark=true;if(name=="bookmark[rec]")out.recommendation=true;}
            }
            if(n.tag=="textarea" && name=="bookmark[bookmarker_notes]") {QString value=html.mid(n.begin,n.end-n.begin);value.replace("<","&lt;").replace(">","&gt;");QTextDocument doc;doc.setHtml("<pre>"+value+"</pre>");out.notes=doc.toPlainText();}
            if(n.tag=="select" && name=="bookmark[pseud_id]")for(int k=j+1;k<list.size();++k)if(list[k].tag=="option" && beneath(list,k,j)) {
                const auto id=list[k].attrs.value("value");out.pseuds.append({id,text(html.mid(list[k].begin,list[k].end-list[k].begin))});if(list[k].attrs.contains("selected") || out.pseud.isEmpty())out.pseud=id;
            }
        }
        if(out.pseuds.isEmpty() && !out.pseud.isEmpty())out.pseuds.append({out.pseud,out.pseud});
        if(out.token.isEmpty() || out.pseud.isEmpty() || (out.id.isEmpty()? !method.isEmpty():method!="put"))out.error="AO3 returned an invalid bookmark form";
        return out;
    }
    out.error="AO3 bookmarking is unavailable. Check your sign-in and work permissions.";return out;
}
inline QMap<QString,QString> archiveBookmarkFields(const ArchiveBookmarkForm &fresh,const ArchiveBookmarkForm &draft) {
    QMap<QString,QString> fields{{"authenticity_token",fresh.token},{"bookmark[pseud_id]",draft.pseud},{"bookmark[bookmarker_notes]",draft.notes},{"bookmark[tag_string]",draft.tags},{"bookmark[collection_names]",draft.collections},{"bookmark[private]",draft.privateBookmark?"1":"0"},{"bookmark[rec]",draft.recommendation?"1":"0"}};
    if(!fresh.id.isEmpty())fields["_method"]="put";
    return fields;
}
struct ArchiveFormResult {QString message;bool success=false,uncertain=false;};
inline ArchiveFormResult archiveFormResult(const QString &html) {
    using namespace CommentHtml;const auto list=nodes(html);QString notice,error;
    for(int i=0;i<list.size();++i) {
        const auto &n=list[i];bool story=hasClass(n,"userstuff");for(int p=n.parent;p>=0;p=list[p].parent)if(list[p].attrs.value("id")=="workskin" || hasClass(list[p],"userstuff"))story=true;
        if(story)continue;
        if(hasClass(n,"flash") && hasClass(n,"notice"))notice=text(html.mid(n.begin,n.end-n.begin));
        if(n.attrs.value("id")=="errorExplanation" || hasClass(n,"errorExplanation") || (hasClass(n,"flash") && hasClass(n,"error")))error=text(html.mid(n.begin,n.end-n.begin));
    }
    if(!error.isEmpty())return {error,false,false};
    if(!notice.isEmpty())return {notice,true,false};
    return {"Could not confirm the change. Check your AO3 bookmarks before trying again.",false,true};
}
inline QMap<QString,QString> parseBookmarkDelete(const QString &html,const QString &id) {
    using namespace CommentHtml;const auto list=nodes(html);
    for(int i=0;i<list.size();++i) {
        const auto &n=list[i];if(n.tag!="form" || n.attrs.value("method").toLower()!="post" || !hasClass(n,"destroy"))continue;
        bool story=false;for(int p=n.parent;p>=0;p=list[p].parent)if(list[p].attrs.value("id")=="workskin" || hasClass(list[p],"userstuff"))story=true;if(story)continue;
        const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(n.attrs.value("action")));
        if(url.scheme()!="https" || url.host()!="archiveofourown.org" || !url.userInfo().isEmpty() || url.port(443)!=443 || url.path()!="/bookmarks/"+id || url.hasQuery() || url.hasFragment())continue;
        QMap<QString,QString> fields;for(int j=i+1;j<list.size();++j)if(beneath(list,j,i) && list[j].tag=="input" && list[j].attrs.value("type")=="hidden" && !list[j].attrs.contains("disabled")) {const auto key=list[j].attrs.value("name");if(key=="authenticity_token" || key=="_method")fields[key]=list[j].attrs.value("value");}
        if(fields.value("_method")=="delete" && !fields.value("authenticity_token").isEmpty())return fields;
    }
    return {};
}

inline bool archiveBookmarkRecord(const QString &html,const QString &workId,const QString &expectedId={}) {
    using namespace CommentHtml;const auto list=nodes(html);
    for(int i=0;i<list.size();++i) {
        const auto &node=list[i];const auto match=QRegularExpression("^bookmark_([0-9]+)$").match(node.attrs.value("id"));
        if(node.tag!="li" || !hasClass(node,"bookmark") || !match.hasMatch() || (!expectedId.isEmpty() && expectedId!=match.captured(1)))continue;
        bool work=false,owned=false;
        for(int j=i+1;j<list.size();++j)if(list[j].tag=="a" && beneath(list,j,i)) {
            bool story=false;for(int p=list[j].parent;p>=0;p=list[p].parent)if(hasClass(list[p],"userstuff"))story=true;if(story)continue;
            const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(list[j].attrs.value("href")));
            if(url.scheme()!="https" || url.host()!="archiveofourown.org" || !url.userInfo().isEmpty() || url.port(443)!=443)continue;
            if(url.path()=="/works/"+workId)work=true;
            if(url.path()=="/bookmarks/"+match.captured(1)+"/edit")owned=true;
        }
        if(work && owned)return true;
    }
    return false;
}
