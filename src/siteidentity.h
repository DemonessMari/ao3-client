#pragma once
#include "commentpage.h"
struct SiteIdentity {QString user;bool login=false,logout=false;};
inline SiteIdentity parseSiteIdentity(const QString &html) {
    SiteIdentity out;const auto list=CommentHtml::nodes(html);
    for(int i=0;i<list.size();++i) {
        const auto &n=list[i];bool story=CommentHtml::hasClass(n,"userstuff"),greeting=false;
        for(int p=n.parent;p>=0;p=list[p].parent){if(list[p].attrs.value("id")=="greeting")greeting=true;if(list[p].attrs.value("id")=="workskin" || CommentHtml::hasClass(list[p],"userstuff"))story=true;}
        if(story)continue;
        const auto href=n.attrs.value(n.tag=="form"?"action":"href");if(href.isEmpty())continue;
        const auto url=QUrl("https://archiveofourown.org").resolved(QUrl(href));
        if(url.scheme()!="https" || url.host()!="archiveofourown.org" || url.port(443)!=443 || !url.userInfo().isEmpty())continue;
        if(url.path()=="/users/login")out.login=true;
        if(url.path()=="/users/logout")out.logout=true;
        if(greeting && QRegularExpression("^/users/[A-Za-z0-9_]+$").match(url.path()).hasMatch() && url.path()!="/users/login" && url.path()!="/users/logout")out.user=url.path().mid(7);
    }
    return out;
}
inline bool siteIdentityMatches(const QString &html,const QString &expected) {
    const auto identity=parseSiteIdentity(html);
    return expected.isEmpty()?identity.login && !identity.logout:identity.logout && identity.user==expected;
}
