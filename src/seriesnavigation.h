#pragma once
#include "commentpage.h"
struct SeriesNavigation { QString id,title,position,previous,next; };
struct WorkSeries { QVector<SeriesNavigation> entries; QString error; };
inline QString archiveNumericId(const QString &href,const QString &type) {
    const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(href));
    if(url.scheme()!="https" || url.host()!="archiveofourown.org" || url.port(443)!=443 || !url.userInfo().isEmpty())return {};
    return QRegularExpression("^/"+type+"/([0-9]+)$").match(url.path()).captured(1);
}
inline WorkSeries parseWorkSeries(const QString &html) {
    using namespace CommentHtml;WorkSeries out;
    if(html.size()>8*1024*1024){out.error="Work metadata is too large.";return out;}
    const auto list=nodes(html);int metadata=-1;
    auto story=[&](int i){for(int p=i;p>=0;p=list[p].parent)if(hasClass(list[p],"userstuff"))return true;return false;};
    for(int i=0;i<list.size();++i)if(list[i].tag=="dl" && hasClass(list[i],"work") && hasClass(list[i],"meta") && !story(i)){metadata=i;break;}
    if(metadata<0){out.error="AO3 did not return work metadata. Check whether this work loads on the website.";return out;}
    for(int i=metadata+1;i<list.size();++i) {
        if(list[i].tag!="span" || !hasClass(list[i],"series") || !beneath(list,i,metadata) || story(i))continue;
        SeriesNavigation entry;int position=-1;
        for(int j=i+1;j<list.size() && list[j].begin<list[i].end;++j) {
            if(!beneath(list,j,i) || story(j))continue;
            if(hasClass(list[j],"position")){position=j;entry.position=text(html.mid(list[j].begin,list[j].end-list[j].begin));}
            if(list[j].tag!="a")continue;
            const auto href=list[j].attrs.value("href");
            if(position>=0 && beneath(list,j,position)) {const auto id=archiveNumericId(href,"series");if(!id.isEmpty()){entry.id=id;entry.title=text(html.mid(list[j].begin,list[j].end-list[j].begin));}}
            if(hasClass(list[j],"previous"))entry.previous=archiveNumericId(href,"works");
            if(hasClass(list[j],"next"))entry.next=archiveNumericId(href,"works");
        }
        if(!entry.id.isEmpty() && !entry.title.isEmpty())out.entries.append(entry);
    }
    return out;
}
