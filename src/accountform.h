#pragma once
#include "archivepages.h"
#include "archivebookmark.h"
#include <QMetaType>

struct AccountField {
    QString name,label,type;
    QStringList values;
    QVector<QPair<QString,QString>> choices;
    QString offValue="0";
};
struct AccountForm {
    QString kind,id,user,source,path,method,title,notice,error;
    QVector<AccountField> fields;
    QMap<QString,QStringList> fixed;
    bool destructive=false;
};
namespace AccountForms {
using namespace ArchivePages;
inline bool nameValid(const QString &name){return !name.isEmpty() && name.size()<=150 && name!="." && name!=".." && !name.contains('/') && !name.contains(QChar(0));}
inline QString source(const QString &kind,const QString &user,const QString &id={}) {
    if(!QRegularExpression("^[A-Za-z0-9_]+$").match(user).hasMatch())return {};
    const auto own="/users/"+user;
    if(kind=="preferences")return own+"/preferences";
    if(kind=="profile")return own+"/profile/edit";
    if(kind=="pseud")return id.isEmpty()?own+"/pseuds/new":nameValid(id)?own+"/pseuds/"+QString::fromLatin1(QUrl::toPercentEncoding(id))+"/edit":QString();
    if(kind=="deleteBookmark")return QRegularExpression("^[0-9]+$").match(id).hasMatch()?"/bookmarks/"+id+"/confirm_delete":QString();
    if(kind=="bookmark")return QRegularExpression("^[0-9]+$").match(id).hasMatch()?"/bookmarks/"+id+"/edit":QString();
    if(kind=="seriesBookmark")return QRegularExpression("^[0-9]+$").match(id).hasMatch()?"/series/"+id+"/bookmarks/new":QString();
    if(kind=="externalBookmark")return "/external_works/new";
    if(kind=="block" || kind=="mute") {
        if(!QRegularExpression("^[A-Za-z0-9_]+$").match(id).hasMatch())return {};
        const auto base=kind=="block"?"blocked":"muted";return own+"/"+base+"/users/confirm_"+kind+"?"+base+"_id="+id;
    }
    if(kind=="unblock" || kind=="unmute") {
        if(!QRegularExpression("^[0-9]+$").match(id).hasMatch())return {};
        const auto base=kind=="unblock"?"blocked":"muted";return own+"/"+base+"/users/"+id+"/confirm_"+kind;
    }
    return {};
}
inline QStringList allowed(const QString &kind) {
    if(kind=="profile")return {"profile[title]","profile[about_me]"};
    if(kind=="preferences") {
        QStringList out;for(const auto &key:QStringList{"minimize_search_engines","disable_share_links","allow_cocreator","adult","view_full_works","hide_warnings","hide_freeform","disable_work_skins","skin_id","time_zone","preferred_locale","work_title_format","comment_emails_off","comment_inbox_off","comment_copy_to_self_off","kudos_emails_off","guest_replies_off","allow_collection_invitation","allow_gifts","collection_emails_off","collection_inbox_off","recipient_emails_off","history_enabled","first_login","banner_seen"})out<<"preference["+key+"]";return out;
    }
    if(kind=="pseud")return {"pseud[name]","pseud[description]","pseud[is_default]","pseud[delete_icon]","pseud[icon_alt_text]","pseud[icon_comment_text]"};
    if(QStringList{"bookmark","seriesBookmark","externalBookmark"}.contains(kind)) {
        QStringList out={"bookmark[pseud_id]","bookmark[bookmarker_notes]","bookmark[tag_string]","bookmark[collection_names]","bookmark[private]","bookmark[rec]"};
        if(kind=="externalBookmark")for(const auto &key:QStringList{"url","author","title","summary","fandom_string","rating_string","category_strings","category_strings[]","relationship_string","character_string"})out<<"external_work["+QString(key).remove("[]")+"]"+(key.endsWith("[]")?"[]":"");
        return out;
    }
    return {};
}
inline bool actionAllowed(const QUrl &url,const QString &kind,const QString &user,const QString &id) {
    if(path(url.toString()).isEmpty())return false;
    const auto own="/users/"+user;const auto p=url.path();
    if(kind=="block" || kind=="mute") {
        const auto base=kind=="block"?"blocked":"muted";const QUrlQuery q(url);
        return p==own+"/"+base+"/users" && q.queryItems().size()==1 && q.queryItemValue(QString(base)+"_id").compare(id,Qt::CaseInsensitive)==0;
    }
    if(url.hasQuery())return false;
    if(kind=="unblock" || kind=="unmute")return p==own+"/"+(kind=="unblock"?"blocked":"muted")+"/users/"+id;
    if(kind=="preferences")return QRegularExpression("^"+QRegularExpression::escape(own)+"/preferences(?:/[0-9]+)?$").match(p).hasMatch();
    if(kind=="profile")return p==own+"/profile";
    if(kind=="pseud")return p==(id.isEmpty()?own+"/pseuds":own+"/pseuds/"+id);
    if(kind=="bookmark" || kind=="deleteBookmark")return p=="/bookmarks/"+id;
    if(kind=="seriesBookmark")return p=="/series/"+id+"/bookmarks";
    return kind=="externalBookmark" && p=="/bookmarks";
}
inline AccountForm parse(const QString &html,const QString &kind,const QString &user,const QString &id={}) {
    AccountForm out;out.kind=kind;out.user=user;out.id=id;out.source=source(kind,user,id);
    if(out.source.isEmpty() || html.size()>8*1024*1024 || !siteIdentityMatches(html,user)){out.error="AO3 could not verify this account form. Check sign-in and reload.";return out;}
    Document d(html);int main=-1;for(int i=0;i<d.list.size();++i)if(d.list[i].attrs.value("id")=="main" && d.trusted(i)){main=i;break;}
    if(main<0){out.error="AO3 did not return this account form.";return out;}
    out.title=d.textAt(d.first("h2",{"heading"},main));
    out.destructive=QStringList{"block","unblock","mute","unmute","deleteBookmark"}.contains(kind);
    if(out.destructive)out.notice=d.textAt(d.first({}, {"caution","notice"},main));
    else if(kind=="profile")out.notice="Your profile is public.";
    for(int form:d.select("form",{},main)) {
        const QUrl url=QUrl("https://archiveofourown.org").resolved(QUrl(d.list[form].attrs.value("action")));
        if(d.list[form].attrs.value("method").toLower()!="post" || !actionAllowed(url,kind,user,id))continue;
        AccountForm result=out;result.path=url.path();if(url.hasQuery())result.path+="?"+url.query(QUrl::FullyEncoded);
        const auto whitelist=allowed(kind);QMap<QString,QStringList> hidden;QMap<QString,int> controlIndex;
        auto label=[&](int n){const auto key=d.list[n].attrs.value("id");for(int l:d.select("label",{},form))if(!key.isEmpty() && d.list[l].attrs.value("for")==key)return d.textAt(l);const auto title=d.list[n].attrs.value("title");if(!title.isEmpty())return title;const auto keyName=QRegularExpression("\\[([^\\]]+)\\]").match(d.list[n].attrs.value("name")).captured(1);QString readable=keyName=="pseud_id"?"Pseud":keyName;readable.replace('_',' ');if(!readable.isEmpty())readable[0]=readable[0].toUpper();return readable;};
        for(int n:d.select({}, {},form)) {
            const auto &node=d.list[n];const auto name=node.attrs.value("name");if(name.isEmpty() || node.attrs.contains("disabled") || !QStringList{"input","select","textarea"}.contains(node.tag))continue;
            const auto type=node.attrs.value("type").toLower();
            if(node.tag=="input" && type=="hidden") {
                if(name=="authenticity_token" || name=="_method" || whitelist.contains(name))hidden[name].append(node.attrs.value("value"));
                continue;
            }
            if(!whitelist.contains(name) || (node.tag=="input" && !QStringList{"","text","url","email","number","checkbox"}.contains(type)))continue;
            AccountField f;f.name=name;f.label=name=="external_work[category_strings][]"?QString("Categories"):label(n);
            if(node.tag=="select") {
                f.type=node.attrs.contains("multiple")?"multi":"choice";
                for(int option:d.select("option",{},n))if(!d.list[option].attrs.contains("disabled")){const auto value=d.list[option].attrs.value("value");bool exists=false;for(const auto &choice:f.choices)exists |= choice.second==value;if(!exists)f.choices.append({d.textAt(option),value});if(d.list[option].attrs.contains("selected")){if(f.type=="choice")f.values={value};else if(!f.values.contains(value))f.values<<value;}}
                if(f.values.isEmpty() && f.type=="choice" && !f.choices.isEmpty())f.values<<f.choices.first().second;
            } else if(type=="checkbox") {
                f.type=name.endsWith("[]")?"multi":"check";const auto value=node.attrs.value("value","1");f.choices.append({f.label,value});if(node.attrs.contains("checked"))f.values<<value;
                if(controlIndex.contains(name)){auto &existing=result.fields[controlIndex.value(name)];existing.choices+=f.choices;existing.values+=f.values;continue;}
            } else if(node.tag=="textarea") {
                f.type="area";QString text=html.mid(node.begin,node.end-node.begin);QTextDocument decoded;decoded.setHtml("<pre>"+text.replace("<","&lt;").replace(">","&gt;")+"</pre>");f.values<<decoded.toPlainText();
            } else {f.type="text";f.values<<node.attrs.value("value");}
            controlIndex[name]=result.fields.size();result.fields.append(f);
        }
        if(hidden.value("authenticity_token").size()!=1 || hidden.value("_method").size()>1)continue;
        result.method=hidden.value("_method").value(0);const bool creating=(kind=="pseud" && id.isEmpty()) || kind=="seriesBookmark" || kind=="externalBookmark" || kind=="block" || kind=="mute";
        if((creating?!result.method.isEmpty():kind=="unblock" || kind=="unmute" || kind=="deleteBookmark"?result.method!="delete":result.method!="put" && result.method!="patch") || hidden.value("authenticity_token").value(0).isEmpty())continue;
        result.fixed["authenticity_token"]={hidden.value("authenticity_token").first()};if(!result.method.isEmpty())result.fixed["_method"]={result.method};
        for(auto &field:result.fields)if(field.type=="check"){field.offValue=hidden.value(field.name).value(0,"0");if(field.values.isEmpty())field.values={field.offValue};}
        for(const auto &key:whitelist)if(!controlIndex.contains(key) && hidden.contains(key))result.fixed[key]=hidden.value(key);
        if(result.fields.isEmpty() && !out.destructive)continue;
        return result;
    }
    out.error="AO3 did not offer an editable form for this action.";return out;
}
inline QMap<QString,QStringList> submission(const AccountForm &original,const AccountForm &fresh,const QMap<QString,QStringList> &draft,QString &error) {
    QMap<QString,QStringList> out=fresh.fixed;
    if(!fresh.error.isEmpty() || original.user!=fresh.user || original.kind!=fresh.kind || original.path!=fresh.path || original.method!=fresh.method){error="This form changed. Reload it before saving.";return {};}
    QStringList oldNames,newNames;for(const auto &f:original.fields)oldNames<<f.name;for(const auto &f:fresh.fields)newNames<<f.name;oldNames.sort();newNames.sort();
    auto oldFixed=original.fixed,newFixed=fresh.fixed;oldFixed.remove("authenticity_token");newFixed.remove("authenticity_token");
    if(oldNames!=newNames || oldFixed!=newFixed){error="The available fields changed. Reload this form.";return {};}
    qint64 size=0;for(auto i=draft.cbegin();i!=draft.cend();++i)for(const auto &v:i.value())size+=v.size();if(size>1024*1024){error="Account form is too large.";return {};}
    for(const auto &field:fresh.fields)out[field.name]=field.values;
    for(auto it=draft.cbegin();it!=draft.cend();++it) {
        const AccountField *old=nullptr,*current=nullptr;for(const auto &f:original.fields)if(f.name==it.key())old=&f;for(const auto &f:fresh.fields)if(f.name==it.key())current=&f;
        if(!old || !current || old->type!=current->type){error="A field is no longer editable. Reload the form.";return {};}
        if(it.value()==old->values)continue;
        if(current->values!=old->values){error="This field changed on AO3. Reload before saving your edit.";return {};}
        if(current->type=="choice" || current->type=="multi" || current->type=="check")for(const auto &value:it.value()){bool valid=current->type=="check" && value==current->offValue;for(const auto &choice:current->choices)valid |= choice.second==value;if(!valid){error="An option is no longer available. Reload the form.";return {};}}
        if(current->type!="multi" && it.value().size()!=1){error="Invalid field value.";return {};}
        out[it.key()]=it.value();
    }
    return out;
}
}
Q_DECLARE_METATYPE(AccountForm)
