#pragma once
#include "accountform.h"
#include "formattedwork.h"
#include <QUrlQuery>
#include <QXmlStreamReader>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QDateTime>

struct ToolSection {QString key,label,association,html;};
struct ToolSubmit {QString name,value,label;bool operator==(const ToolSubmit &b)const{return name==b.name && value==b.value && label==b.label;}};
struct ToolUpload {QString name,filename,mime;QByteArray bytes;};
struct ToolForm {QString key,source,action,method,title;QVector<AccountField> fields;QMap<QString,QStringList> fixed,checkpoint;QVector<ToolSubmit> submits;QString authorityAction,authorityMethod,authorityKey,authorityOverride;QVector<ToolSection> sections;int addedSections=0;};
struct ToolPage {QString source,user,title,text,error,csrf,previewHtml,previewCss;QStringList notices;QVector<QPair<QString,QString>> links;QVector<ToolForm> forms;bool uncertain=false,feed=false,inProgress=false;};
Q_DECLARE_METATYPE(ToolPage)
#ifndef Q_MOC_RUN
namespace NativeTools {
using namespace ArchivePages;
inline QString route(const QString &value,const QString &user) {
    const QUrl uri=QUrl("https://archiveofourown.org").resolved(QUrl(value));
    if(uri.scheme()!="https" || uri.host()!="archiveofourown.org" || uri.port(443)!=443 || !uri.userInfo().isEmpty() || value.startsWith('#') || value.size()>8192)return {};
    QString p=uri.path();while(p.endsWith('/'))p.chop(1);const auto parts=p.mid(1).split('/');
    if(parts.isEmpty() || parts.size()>12)return {};
    for(const auto &part:parts){if(part.isEmpty() || part=="." || part==".." || part.contains('\\'))return {};for(auto c:part)if(c.unicode()<32)return {};}
    const QStringList roots={"comments","works","chapters","series","collections","collection_items","collection_participants","challenge_signups","challenge_assignments","challenge_claims","potential_matches","prompts","gifts","related_works","serial_works","orphans","skins","tag_sets","tag_nominations","invitations","user_invite_requests","faq","archive_faqs","admin_posts","tos","tos_faq","content","privacy","support","abuse_reports","external_works","bookmarks"},own={"stats","comments","favorite_tags","works","series","pseuds","collections","collection_items","related_works","gifts","assignments","claims","signups","nominations","tag_sets","creatorships","external_authors","invitations","skins","profile","preferences","change_email","confirm_change_email","changed_email","change_password","changed_password","change_username","changed_username","reconfirm_email"};
    const bool valid=parts.first()=="users"?parts.size()>=2 && !user.isEmpty() && parts[1]==user && (parts.size()==2 || own.contains(parts[2])):roots.contains(parts.first()) || (parts.first()=="tags" && (parts.last()=="feed" || parts.last()=="feed.atom"));
    if(!valid)return {};auto out=uri;out.setFragment({});out.setPath(p);return out.path(QUrl::FullyEncoded)+(out.hasQuery()?"?"+out.query(QUrl::FullyEncoded):QString());
}
inline bool fieldAllowed(const QString &name) {
    if(name.size()>240 || !QRegularExpression("^[A-Za-z0-9_\\[\\]()-]+$").match(name).hasMatch())return false;
    const auto parts=name.split(QRegularExpression("[\\[\\]]"),Qt::SkipEmptyParts);if(parts.isEmpty())return false;
    for(const auto &part:parts)if(QStringList{"admin","is_admin","ticket_number","banned","suspended","roles","spam"}.contains(part) || part.startsWith("admin_"))return false;
    const QStringList namespaces={"stat","external_author","skin_property","creation","tag_set_association","preference","signup","request","offer","claim","assignment","participant","work","chapter","series","collection","collection_profile","collection_preference","collection_item","collection_participant","challenge_signup","challenge_assignment","challenge_claim","gift_exchange","prompt_meme","prompt","potential_match","skin","work_skin","site_skin","skin_parent","bookmark","external_work","pseud","invitation","user_invite_request","related_work","serial_work","gift","tag_set","owned_tag_set","tag_set_nomination","nomination","creatorship","feedback","feedback_report","abuse_report","collection_search","work_search","bookmark_search","people_search","tag_search","user"},scalars={"password_check","password","password_confirmation","new_email","email_confirmation","new_username","username","login","commit","utf8","authenticity_token","_method","save_button","preview_button","edit_button","post_button","update_button","cancel_button","bookmarks_action","pseud_id","pseud_ids","owner_pseuds","work_ids","bookmark_ids","collection_ids","ids","id","page","sort_column","sort_direction","query","q","search","show","type","skin_type","add_site_parents","num_requests","num_offers","parent-options-show","chapters-options-show","series-options-show","backdate-options-show","coauthor-options-show","remove_me","orphan","orphan_pseud","send_me_copy","recipient","recipients","email","url","urls","import_url","import_urls","encoding","anonymous","approved","rejected","include_bookmarks","delete_bookmarks","transfer_bookmarks","status","participant_role"};
    auto singular=parts.first();if(singular.endsWith("ies")){singular.chop(3);singular+="y";}else if(singular.endsWith('s'))singular.chop(1);return parts.size()==1 || namespaces.contains(parts.first()) || namespaces.contains(singular) || parts.first().endsWith("_search") || scalars.contains(parts.first());
}
inline ToolSection section(const QString &label,const QString &call) {
    if(call.size()>512*1024)return {};
    const QRegularExpression pattern(R"rx(^\s*add_section\(\s*this\s*,\s*"([A-Za-z_]+)"\s*,\s*("(?:\\.|[^"\\])*")\s*\)\s*;?\s*(?:return\s+false\s*;?)?\s*$)rx");
    const auto match=pattern.match(call);if(!match.hasMatch())return {};
    auto literal=match.captured(2);literal.replace(QRegularExpression(R"rx(\\(['`$]))rx"),"\\1");
    const auto array=QJsonDocument::fromJson(("["+literal+"]").toUtf8()).array();if(array.size()!=1 || !array.first().isString())return {};
    const auto html=array.first().toString();if(html.size()>262144 || !html.contains("new_"+match.captured(1)) || QRegularExpression("</?form\\b",QRegularExpression::CaseInsensitiveOption).match(html).hasMatch())return {};
    return {match.captured(1)+"|"+QString::fromLatin1(QCryptographicHash::hash(html.toUtf8(),QCryptographicHash::Sha256).toHex().left(16)),label,match.captured(1),html};
}
inline QMap<QString,QStringList> values(const ToolForm &form) {
    auto out=form.fixed;QStringList sections;for(const auto &section:form.sections)sections<<section.key;out["@sections"]=sections;out.remove("authenticity_token");out.remove("utf8");for(auto i=out.begin();i!=out.end();)if(i.key().contains("password",Qt::CaseInsensitive))i=out.erase(i);else ++i;for(const auto &f:form.fields){if(f.type!="password" && f.type!="file")out[f.name]=f.values;out["@type:"+f.name]={f.type};if(!f.choices.isEmpty()){QStringList choices;for(const auto &choice:f.choices)choices<<choice.second;out["@choices:"+f.name]=choices;}}return out;
}
inline ToolPage parse(const QString &html,const QString &source,const QString &user) {
    ToolPage out;out.source=source;out.user=user;
    if(html.size()>8*1024*1024 || route(source,user).isEmpty()){out.error="Invalid AO3 tools page.";return out;}
    if(QUrl(source).path().endsWith("/feed") || QUrl(source).path().endsWith("/feed.atom")) {
        QXmlStreamReader xml(html);bool recognized=false,inEntry=false;QString title,date,summary,link;int count=0;
        while(!xml.atEnd()) {
            xml.readNext();
            if(xml.isStartElement()) {
                const auto name=xml.name().toString();
                if(name=="feed"){recognized=xml.namespaceUri()==QStringLiteral("http://www.w3.org/2005/Atom");}
                else if(name=="entry"){inEntry=true;title.clear();date.clear();summary.clear();link.clear();}
                else if(name=="title"){const auto value=xml.readElementText(QXmlStreamReader::IncludeChildElements);if(inEntry)title=value;else out.title=value;}
                else if(inEntry && name=="updated")date=xml.readElementText();
                else if(inEntry && (name=="summary" || name=="content"))summary=CommentHtml::text(xml.readElementText(QXmlStreamReader::IncludeChildElements));
                else if(inEntry && name=="link" && (xml.attributes().value("rel").isEmpty() || xml.attributes().value("rel")==QStringLiteral("alternate")))link=route(xml.attributes().value("href").toString(),user);
            } else if(xml.isEndElement() && xml.name()==QStringLiteral("entry")) {
                inEntry=false;if(++count>200)break;out.text+=(out.text.isEmpty()?QString():QString("\n\n"))+title+"\n"+date+"\n"+summary;
                if(QRegularExpression("^/works/[0-9]+$").match(QUrl(link).path()).hasMatch())out.links<<qMakePair(title,link);
            }
        }
        if(!recognized || xml.hasError()){out.error="AO3 did not return an Atom feed.";return out;}out.text=out.text.left(1024*1024);out.feed=true;return out;
    }
    if(!user.isEmpty() && !siteIdentityMatches(html,user)){out.error="AO3 could not verify this account. Check native sign-in.";return out;}
    Document d(html);int main=-1;for(int i=0;i<d.list.size();++i)if(d.list[i].attrs.value("id")=="main" && d.trusted(i)){main=i;break;}
    if(main<0){out.error="AO3 did not return this page.";return out;}
    out.title=d.textAt(d.first("h2",{"heading"},main));if(out.title.isEmpty())out.title=d.textAt(d.first("h2",{},main));if(out.title.isEmpty())out.title="AO3";
    auto resolved=[&](const QString &value){return route(QUrl("https://archiveofourown.org"+source).resolved(QUrl(value.isEmpty()?source:value)).toString(),user);};
    int index=0;
    for(int form:d.select("form",{},main)) {
        const auto &node=d.list[form];ToolForm f;f.source=source;f.action=resolved(node.attrs.value("action"));f.method=node.attrs.value("method","get").toLower();const int number=index++;
        if(f.action.isEmpty() || (f.method!="get" && f.method!="post") || (f.method=="post" && user.isEmpty()))continue;
        bool invalid=false;QMap<QString,QStringList> hidden;QMap<QString,QVector<int>> grouped;QStringList names;
        auto label=[&](int n){const auto id=d.list[n].attrs.value("id");if(!id.isEmpty())for(int l:d.select("label",{},form))if(d.list[l].attrs.value("for")==id)return d.textAt(l);if(!d.list[n].attrs.value("aria-label").isEmpty())return d.list[n].attrs.value("aria-label");auto parts=d.list[n].attrs.value("name").split(QRegularExpression("[\\[\\]]"),Qt::SkipEmptyParts);QString title;for(const auto &p:parts)if(!QRegularExpression("^[0-9]+$").match(p).hasMatch())title=p;title.replace('_',' ');if(!title.isEmpty())title[0]=title[0].toUpper();return title;};
        for(int n:d.select({}, {},form)) {
            const auto &el=d.list[n];const auto name=el.attrs.value("name"),type=el.attrs.value("type").toLower();
            if(el.attrs.contains("disabled") || !QStringList{"input","textarea","select"}.contains(el.tag) || name.isEmpty())continue;
            if(!fieldAllowed(name)){invalid=true;break;}
            if(!name.contains("password",Qt::CaseInsensitive) && !name.endsWith("[_destroy]") && ((el.tag=="input" && type=="hidden") || el.attrs.contains("readonly"))){hidden[name]<<el.attrs.value("value");continue;}
            if(el.tag=="input" && QStringList{"submit","button","reset","image"}.contains(type))continue;
            if(!grouped.contains(name))names<<name;grouped[name]<<n;
        }
        if(invalid || (f.method=="post" && (hidden.value("authenticity_token").size()!=1 || hidden.value("authenticity_token").first().isEmpty())) || hidden.value("_method").size()>1)continue;
        if(!QStringList{"","put","patch","delete"}.contains(hidden.value("_method").value(0)))continue;
        for(const auto &name:names) {
            const auto group=grouped.value(name);const int n=group.first();const auto &el=d.list[n];const auto type=el.attrs.value("type").toLower();AccountField field;field.name=name;field.label=label(n);
            if(name.contains("password",Qt::CaseInsensitive)){field.type="password";field.values={""};}
            else if(name.endsWith("[_destroy]")){field.type="check";field.label="Remove item";field.offValue=el.attrs.value("value","0");field.values={field.offValue};field.choices<<qMakePair(QString("Remove item"),QString("1"));}
            else if(el.tag=="select") {
                field.type=el.attrs.contains("multiple")?"multi":"choice";
                for(int option:d.select("option",{},n)) {
                    bool disabled=false;for(int p=option;p>=0 && p!=n;p=d.list[p].parent)disabled |= d.list[p].attrs.contains("disabled");if(disabled)continue;
                    const auto value=d.list[option].attrs.contains("value")?d.list[option].attrs.value("value"):d.textAt(option);bool exists=false;for(const auto &choice:field.choices)exists |= choice.second==value;if(!exists)field.choices<<qMakePair(d.textAt(option),value);
                    if(d.list[option].attrs.contains("selected")){if(field.type=="choice")field.values={value};else if(!field.values.contains(value))field.values<<value;}
                }
                if(field.values.isEmpty() && field.type=="choice" && !field.choices.isEmpty())field.values<<field.choices.first().second;
            } else if(el.tag=="textarea") {field.type="area";auto raw=html.mid(el.begin,el.end-el.begin);QTextDocument doc;doc.setHtml("<pre>"+raw.replace("<","&lt;").replace(">","&gt;")+"</pre>");field.values={doc.toPlainText()};}
            else if(type=="checkbox" || type=="radio") {
                field.offValue=hidden.value(name).value(0,"0");for(int g:group){const auto value=d.list[g].attrs.value("value","1");bool exists=false;for(const auto &choice:field.choices)exists |= choice.second==value;if(!exists)field.choices<<qMakePair(label(g),value);if(d.list[g].attrs.contains("checked"))field.values<<value;}
                field.type=type=="radio"?"choice":field.choices.size()==1 && !name.endsWith("[]")?"check":"multi";if(field.type=="check" && field.values.isEmpty())field.values={field.offValue};
            } else if(type=="file")field.type="file";
            else if(type=="password"){field.type="password";field.values={""};}
            else if(QStringList{"","text","url","email","number","date","datetime-local","time","tel","search","datetime","color","range","month","week"}.contains(type)){field.type="text";field.values={el.attrs.value("value")};}
            else {invalid=true;break;}
            f.fields<<field;
        }
        if(invalid)continue;
        if(f.method=="get"){bool sensitive=false;for(const auto &field:f.fields)sensitive |= field.type=="password" || field.type=="file";if(sensitive)continue;}
        for(int n:d.select({}, {},form)) {
            const auto &el=d.list[n];const auto type=el.attrs.value("type").toLower();if(el.attrs.contains("disabled"))continue;
            if(!((el.tag=="input" && type=="submit") || (el.tag=="button" && (type.isEmpty() || type=="submit"))))continue;
            const auto name=el.attrs.value("name");if(!name.isEmpty() && !fieldAllowed(name))continue;
            ToolSubmit submit{name,el.attrs.value("value"),el.tag=="input"?el.attrs.value("value"):d.textAt(n)};if(!f.submits.contains(submit))f.submits<<submit;
        }
        if(f.submits.isEmpty())f.submits<<ToolSubmit{{},{},f.method=="get"?QString("Apply"):QString("Submit")};
        f.fixed=hidden;for(const auto &field:f.fields)f.fixed.remove(field.name);
        f.title=d.textAt(d.first("legend",{},form));if(f.title.isEmpty())f.title=f.submits.first().label;
        f.key=node.attrs.value("id");if(f.key.isEmpty())f.key="form-"+QString::number(number);f.key+="|"+f.action+"|"+f.method;
        for(int link:d.select("a",{},form)){const auto item=section(d.textAt(link),d.list[link].attrs.value("onclick"));if(!item.key.isEmpty()){bool exists=false;for(const auto &s:f.sections)exists |= s.key==item.key;if(!exists)f.sections<<item;}}
        f.checkpoint=values(f);out.forms<<f;
    }
    QString token;for(int n:d.select("meta",{}))if(d.list[n].attrs.value("name")=="csrf-token")for(int p=d.list[n].parent;p>=0;p=d.list[p].parent)if(d.list[p].tag=="head")token=d.list[n].attrs.value("content");
    out.csrf=token;
    for(int n:d.select("a",{},main)) {
        const auto &el=d.list[n];const auto action=resolved(el.attrs.value("href"));if(action.isEmpty())continue;const auto title=d.textAt(n);if(title.isEmpty())continue;
        if(el.attrs.contains("data-method")) {
            const auto method=el.attrs.value("data-method").toLower();if(user.isEmpty() || token.isEmpty() || !QStringList{"post","put","patch","delete"}.contains(method))continue;
            ToolForm f;f.key="link|"+action+"|"+method;f.source=source;f.action=action;f.method="post";f.title=title;f.fixed["authenticity_token"]={token};if(method!="post")f.fixed["_method"]={method};f.submits<<ToolSubmit{{},{},title};f.checkpoint=values(f);out.forms<<f;
        } else {const auto link=qMakePair(title,action);if(!out.links.contains(link) && out.links.size()<2000)out.links<<link;}
    }
    for(int n:d.select({},{}))if((hasClass(d.list[n],"flash") && (hasClass(d.list[n],"notice") || hasClass(d.list[n],"error"))) || (beneath(d.list,n,main) && (hasClass(d.list[n],"error") || (hasClass(d.list[n],"notice") && !hasClass(d.list[n],"required"))))){const auto notice=d.textAt(n);if(!notice.isEmpty() && !out.notices.contains(notice))out.notices<<notice;}
    int preview=-1;for(int n:d.select({}, {},main))if(d.list[n].attrs.value("id")=="previewpane"){preview=n;break;}
    if(preview>=0)for(int n=preview+1;n<d.list.size();++n)if(d.list[n].attrs.value("id")=="workskin" && beneath(d.list,n,preview)){out.previewHtml=staticReadingHtml(html.mid(d.list[n].begin,d.list[n].end-d.list[n].begin));out.previewCss=readingWorkCss(html);break;}
    QString body=html.mid(d.list[main].begin,d.list[main].end-d.list[main].begin);const auto nodes=CommentHtml::nodes(body);QVector<QPair<int,int>> ranges;int covered=-1;
    for(const auto &n:nodes)if(n.begin>covered && (QStringList{"form","script","style","template","h2"}.contains(n.tag) || hasClass(n,"actions") || hasClass(n,"navigation") || hasClass(n,"landmark"))){const int start=body.lastIndexOf('<',n.begin-1),end=body.indexOf('>',n.end);if(start>=0 && end>=0){ranges<<qMakePair(start,end+1-start);covered=end;}}
    for(int i=ranges.size()-1;i>=0;--i)body.remove(ranges[i].first,ranges[i].second);out.text=CommentHtml::text(body).left(1024*1024);return out;
}
inline bool dirty(const ToolPage &page,const QMap<QString,QMap<QString,QStringList>> &draft,const QMap<QString,QMap<QString,ToolUpload>> &uploads) {
    if(page.inProgress)return true;for(const auto &files:uploads)if(!files.isEmpty())return true;
    for(const auto &form:page.forms)for(const auto &field:form.fields)if(draft.value(form.key).value(field.name,field.values)!=field.values)return true;
    return false;
}
inline qint64 memoryCost(const ToolPage &page,const QMap<QString,QMap<QString,QStringList>> &draft,const QMap<QString,QMap<QString,ToolUpload>> &uploads) {
    qint64 size=(page.text.size()+page.previewHtml.size()+page.previewCss.size())*2;auto add=[&](const QMap<QString,QStringList> &values){for(const auto &list:values)for(const auto &s:list)size+=s.size()*2;};
    for(const auto &form:page.forms){add(form.fixed);add(form.checkpoint);for(const auto &field:form.fields)for(const auto &s:field.values)size+=s.size()*2;for(const auto &section:form.sections)size+=section.html.size()*2;}
    for(const auto &form:draft)add(form);for(const auto &form:uploads)for(const auto &file:form)size+=file.bytes.size();return size;
}
inline QMap<QString,QStringList> prepare(const ToolForm &original,const ToolPage &freshPage,const QMap<QString,QStringList> &draft,const ToolSubmit &button,QString &error) {
    if(!original.submits.contains(button)){error="This action is no longer available.";return {};}
    const ToolForm *fresh=original.method=="get"?&original:nullptr;
    const auto action=original.authorityAction.isEmpty()?original.action:original.authorityAction;
    const auto method=original.authorityMethod.isEmpty()?original.method:original.authorityMethod;
    const auto overrideMethod=original.authorityAction.isEmpty()?original.fixed.value("_method").value(0):original.authorityOverride;
    const auto key=original.authorityKey.isEmpty()?original.key:original.authorityKey;
    if(!fresh)for(const auto &f:freshPage.forms)if(f.action==action && f.method==method && (f.key==key || f.fixed.value("_method").value(0)==overrideMethod)){fresh=&f;break;}
    if(!fresh || !freshPage.error.isEmpty()){error=freshPage.error.isEmpty()?"AO3 no longer offers this action. Reload the page.":freshPage.error;return {};}
    if(original.method!="get" && values(*fresh)!=original.checkpoint){error="This page changed on AO3. Reload it before submitting.";return {};}
    qint64 size=0;for(auto i=draft.cbegin();i!=draft.cend();++i)for(const auto &v:i.value())size+=v.size();if(size>4*1024*1024){error="This form is too large.";return {};}
    auto result=original.fixed;result.remove("authenticity_token");result.remove("utf8");QStringList names;
    for(const auto &field:original.fields) {
        names<<field.name;const auto value=draft.value(field.name,field.values);
        if(field.type!="multi" && value.size()>1){error="Invalid field value.";return {};}
        if(QStringList{"choice","multi","check"}.contains(field.type))for(const auto &v:value){bool valid=field.type=="check" && v==field.offValue;for(const auto &choice:field.choices)valid |= choice.second==v;if(!valid){error="An option is no longer available.";return {};}}
        if(field.type!="file")result[field.name]=value.isEmpty()?QStringList{""}:value;
    }
    for(auto i=draft.cbegin();i!=draft.cend();++i)if(!names.contains(i.key())){error="Unknown form field.";return {};}
    if(original.method=="post") {
        const auto token=original.action==fresh->action?fresh->fixed.value("authenticity_token").value(0):freshPage.csrf;
        if(token.isEmpty()){error="AO3 did not return a fresh token. Reload this page.";return {};}
        result["authenticity_token"]={token};
    }
    if(!button.name.isEmpty())result[button.name]={button.value};return result;
}
inline ToolPage continued(ToolPage page,const ToolForm &original,const QString &posted) {
    if(QUrl(page.source).path()==QUrl(posted).path()) {
        page.source=original.source;
        for(const auto &form:page.forms)if(form.method=="post")page.inProgress=true;
        for(auto &form:page.forms)if(form.method=="post") {
            form.source=original.source;form.checkpoint=original.checkpoint;
            form.authorityAction=original.authorityAction.isEmpty()?original.action:original.authorityAction;
            form.authorityMethod=original.authorityMethod.isEmpty()?original.method:original.authorityMethod;
            form.authorityKey=original.authorityKey.isEmpty()?original.key:original.authorityKey;
            form.authorityOverride=original.authorityAction.isEmpty()?original.fixed.value("_method").value(0):original.authorityOverride;
        }
    }
    return page;
}
inline ToolForm addSection(const ToolForm &form,const QString &key,const QString &user,QString &error) {
    ToolSection section;for(const auto &s:form.sections)if(s.key==key)section=s;
    if(section.key.isEmpty() || form.addedSections>=50 || form.fields.size()>=1000){error="This form has too many sections or the section is unavailable.";return form;}
    auto html=section.html;html.replace("new_"+section.association,"native_"+QString::number(QDateTime::currentMSecsSinceEpoch())+"_"+QString::number(form.addedSections));
    QString hidden="<input name='authenticity_token' type='hidden' value='template'>";
    if(form.fixed.contains("_method"))hidden+="<input name='_method' type='hidden' value='"+form.fixed.value("_method").first().toHtmlEscaped()+"'>";
    const auto page=parse("<nav id='greeting'><a href='/users/"+user+"'>"+user+"</a><a href='/users/logout'>Out</a></nav><div id='main'><form method='"+form.method+"' action=\""+form.action.toHtmlEscaped()+"\">"+hidden+html+"</form></div>",form.source,user);
    if(page.forms.size()!=1){error="AO3 returned an unsupported section.";return form;}
    auto result=form;const auto added=page.forms.first();for(const auto &field:added.fields)for(const auto &existing:form.fields)if(field.name==existing.name){error="This section is already present.";return form;}
    result.fields+=added.fields;for(auto i=added.fixed.cbegin();i!=added.fixed.cend();++i)if(i.key()!="authenticity_token" && i.key()!="_method")result.fixed[i.key()]=i.value();
    for(const auto &s:added.sections){bool exists=false;for(const auto &old:result.sections)exists |= old.key==s.key;if(!exists)result.sections<<s;}
    ++result.addedSections;return result;
}
inline ToolUpload upload(const QString &name,const QString &filename,const QByteArray &bytes,QString &error) {
    if(!fieldAllowed(name) || bytes.isEmpty() || bytes.size()>2*1024*1024){error="Choose an image smaller than 2 MB.";return {};}
    QString mime;if(bytes.startsWith(QByteArray::fromHex("89504e470d0a1a0a")))mime="image/png";else if(bytes.startsWith(QByteArray::fromHex("ffd8ff")))mime="image/jpeg";else if(bytes.startsWith("GIF87a") || bytes.startsWith("GIF89a"))mime="image/gif";else {error="Choose a PNG, JPEG or GIF image.";return {};}
    QString file=filename.section('/',-1).section('\\',-1);file.replace(QRegularExpression("[^A-Za-z0-9_.-]"),"_");file=file.left(120);if(file.isEmpty())file="image";return {name,file,mime,bytes};
}
}

#endif
