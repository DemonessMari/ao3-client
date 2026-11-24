#pragma once
#include "siteidentity.h"
struct AccountReadingDocument {
    QString listingHtml,error;
    QMap<QString,QString> activity;
    QStringList unavailable;
    int pages=1;
};
inline AccountReadingDocument accountReadingDocument(const QString &source,const QString &user,int page) {
    const QString html=CommentHtml::visibleListingHtml(source);
    using namespace CommentHtml;AccountReadingDocument out;out.pages=page;
    if(html.size()>8*1024*1024){out.error="AO3 reading list is too large.";return out;}
    if(!siteIdentityMatches(html,user)){out.error="AO3 could not verify this account's reading list. Check native sign-in and try again.";return out;}
    const auto list=nodes(html);int main=-1;
    auto story=[&](int i){for(int p=i;p>=0;p=list[p].parent)if(hasClass(list[p],"userstuff"))return true;return false;};
    for(int i=0;i<list.size();++i)if(list[i].attrs.value("id")=="main" && hasClass(list[i],"readings-index") && !story(i)){main=i;break;}
    if(main<0){out.error="AO3 did not return your reading list. If history is disabled, enable it in AO3 preferences.";return out;}
    out.listingHtml="<ol class='work'>";
    for(int i=main+1;i<list.size();++i) {
        const auto &node=list[i];if(!beneath(list,i,main) || story(i))continue;
        if(node.tag=="ol" && hasClass(node,"reading") && hasClass(node,"work"))out.listingHtml+=html.mid(node.begin,node.end-node.begin);
        if(node.tag=="h4" && hasClass(node,"viewed")) {
            for(int p=node.parent;p>=0 && p!=main;p=list[p].parent)if(list[p].tag=="li" && hasClass(list[p],"reading") && hasClass(list[p],"work")) {
                const auto activity=text(html.mid(node.begin,node.end-node.begin));
                const auto id=QRegularExpression("^work_([0-9]+)$").match(list[p].attrs.value("id")).captured(1);
                if(id.isEmpty()){if(!activity.isEmpty())out.unavailable.append(activity);}else out.activity[id]=activity;
                break;
            }
        }
        if(node.tag=="a") {
            bool pagination=false;for(int p=node.parent;p>=0 && p!=main;p=list[p].parent)pagination|=hasClass(list[p],"pagination");
            if(!pagination)continue;
            const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(node.attrs.value("href")));
            if(url.scheme()!="https" || url.host()!="archiveofourown.org" || url.port(443)!=443 || !url.userInfo().isEmpty() || url.path()!="/users/"+user+"/readings")continue;
            const int number=QUrlQuery(url).queryItemValue("page").toInt();if(number>0 && number<=1000000)out.pages=qMax(out.pages,number);
        }
    }
    out.listingHtml+="</ol>";return out;
}
