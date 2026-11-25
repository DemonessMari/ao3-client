#pragma once
#include "archivepages.h"
#include "filtercatalogue.h"

namespace DiscoverySearch {
struct Field { QString key,label; QVector<QPair<QString,QString>> choices; };
inline QVector<Field> fields(const QString &kind) {
    if(kind=="people")return {{"query","All fields",{}},{"name","Name",{}},{"fandom","Fandom",{}}};
    if(kind=="tags")return {
        {"name","Tag name",{}},{"fandoms","Fandoms",{}},
        {"type","Tag type",{{"Fandom","Fandom"},{"Character","Character"},{"Relationship","Relationship"},{"Additional tag","Freeform"}}},
        {"wrangling_status","Tag status",{{"Canonical","canonical"},{"Noncanonical","noncanonical"},{"Synonymous","synonymous"},{"Canonical or synonymous","canonical_synonymous"},{"Noncanonical and nonsynonymous","noncanonical_nonsynonymous"},{"Noncanonical, nonsynonymous and wrangleable","noncanonical_nonsynonymous_not_unwrangleable"},{"Unwrangleable","unwrangleable"}}},
        {"sort_column","Sort by",{{"Name","name"},{"Date created","created_at"},{"Uses","uses"}}},
        {"sort_direction","Sort direction",{{"Ascending","asc"},{"Descending","desc"}}}};
    if(kind=="bookmarks")return {
        {"bookmarkable_query","Any field on work",{}},{"other_tag_names","Work tags",{}},
        {"bookmarkable_type","Item type",{{"Work","Work"},{"Series","Series"},{"External work","External Work"}}},
        {"word_count","Word count",{}},{"language_id","Work language",standardFilterChoices().choices.value("language_id")},{"bookmarkable_date","Date updated",{}},
        {"bookmark_query","Any field on bookmark",{}},{"other_bookmark_tag_names","Bookmarker’s tags",{}},{"bookmarker","Bookmarker",{}},{"bookmark_notes","Notes",{}},
        {"rec","Recommendations only",{{"Yes","1"}}},{"with_notes","With notes only",{{"Yes","1"}}},{"date","Date bookmarked",{}},
        {"sort_column","Sort by",{{"Date bookmarked","created_at"},{"Date updated","bookmarkable_date"},{"Word count","word_count"}}}};
    return {};
}
inline QString title(const QString &kind){return kind=="people"?"People search":kind=="tags"?"Tag search":"Bookmark search";}
inline QUrl url(const QString &kind,const QMap<QString,QString> &values,int page) {
    const auto spec=fields(kind);if(spec.isEmpty())return {};
    QUrl u("https://archiveofourown.org/"+kind+"/search");QUrlQuery q;
    const auto prefix=kind=="people"?"people_search":kind=="tags"?"tag_search":"bookmark_search";
    for(const auto &f:spec)if(!values.value(f.key).trimmed().isEmpty())q.addQueryItem(QString(prefix)+"["+f.key+"]",values.value(f.key).trimmed());
    const auto primary=kind=="people"?"query":kind=="tags"?"name":"bookmarkable_query";
    if(!q.hasQueryItem(QString(prefix)+"["+primary+"]"))q.addQueryItem(QString(prefix)+"["+primary+"]","");
    q.addQueryItem("page",QString::number(qBound(1,page,1000000)));u.setQuery(q);return u;
}
inline ArchivePage parse(const QString &source,const QString &kind,int page) {
    const QString html=CommentHtml::visibleListingHtml(source);
    using namespace CommentHtml;using namespace ArchivePages;
    ArchivePage out;out.title=title(kind);out.page=out.pages=page;
    if(fields(kind).isEmpty() || html.size()>8*1024*1024){out.error="Invalid AO3 search response.";return out;}
    Document d(html);int main=-1;
    for(int i=0;i<d.list.size();++i)if(d.list[i].attrs.value("id")=="main" && hasClass(d.list[i],kind+"-search") && d.trusted(i)){main=i;break;}
    if(main<0){out.error="AO3 did not return search results. Try again when the site is available.";return out;}
    const auto marker=kind=="people"?"pseud":kind=="tags"?"tag":"bookmark";
    const int root=d.first("ol",{marker,"index"},main);
    if(root<0 && !(kind=="bookmarks" && d.textAt(main).contains("No results found",Qt::CaseInsensitive))){out.error="AO3 did not return search results.";return out;}
    if(root>=0)for(int i:d.select("li",{},root)) {
        if(d.list[i].parent!=root)continue;
        ArchiveEntry e;e.id=d.list[i].attrs.value("id");if(e.id.isEmpty())e.id=QString::number(i);
        const int heading=kind=="tags"?i:d.first("h4",{"heading"},i);
        if(kind=="tags") {
            for(int a:d.select("a",{},i))if(path(d.list[a].attrs.value("href")).startsWith("/tags/")) {
                if(d.first({}, {"canonical"},i)>=0){e.actions={"Tag feed"};const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(d.list[a].attrs.value("href")));e.selection=url.path(QUrl::FullyEncoded)+"/feed.atom";}
                e.title=d.textAt(a);const auto type=d.textAt(i).section(':',0,0).trimmed();
                const QMap<QString,QString> tagFields={{"Fandom","fandom_names"},{"Character","character_names"},{"Relationship","relationship_names"},{"Freeform","freeform_names"}};
                if(tagFields.contains(type)){QUrl target("/works/search");QUrlQuery q;q.addQueryItem("work_search["+tagFields.value(type)+"]",e.title);target.setQuery(q);e.target=target.toString(QUrl::FullyEncoded);}
                else {e.target=path(d.list[a].attrs.value("href"));}
                break;
            }
            e.body=d.textAt(i);
        } else {
            e.title=d.textAt(heading);e.target=d.target(heading);
            QStringList body;
            for(int n:d.select({}, {},i,false))if((d.list[n].tag=="blockquote" && hasClass(d.list[n],"userstuff")) || (d.list[n].tag=="dl" && hasClass(d.list[n],"stats")) || (d.list[n].tag=="h5" && hasClass(d.list[n],"heading")) || (d.list[n].tag=="h5" && hasClass(d.list[n],"byline")))body<<d.textAt(n);
            e.body=body.join("\n\n");
            if(kind=="bookmarks" && e.target.isEmpty()) {const auto id=QRegularExpression("^bookmark_([0-9]+)$").match(e.id).captured(1);if(!id.isEmpty())e.target="/bookmarks/"+id;}
        }
        if(e.title.isEmpty())e.title=kind=="bookmarks"?"Unavailable bookmarked item":"Unavailable entry";
        out.entries.append(e);
    }
    for(int p:d.select({}, {"pagination"},main))for(int a:d.select("a",{},p)) {
        const QUrl link=QUrl("https://archiveofourown.org").resolved(QUrl(d.list[a].attrs.value("href")));
        if(path(link.toString())!="/"+kind+"/search")continue;
        const int n=QUrlQuery(link).queryItemValue("page").toInt();if(n>0 && n<=1000000)out.pages=qMax(out.pages,n);
    }
    return out;
}
}
