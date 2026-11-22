#pragma once
#include "commentpage.h"
#include <QImage>
#include <QImageReader>
#include <QBuffer>

inline bool avatarUrlAllowed(const QUrl &url,bool redirect=false) {
    if(!url.isValid() || url.scheme()!="https" || !url.userInfo().isEmpty() || url.port(443)!=443 || url.hasFragment() || url.toString().size()>4096)return false;
    if(redirect && url.host().endsWith(".amazonaws.com"))return true;
    if(url.host()!="archiveofourown.org")return false;
    const auto path=url.path();return path.startsWith("/system/pseud/icons/") || path.startsWith("/rails/active_storage/") || path.startsWith("/images/skins/iconsets/");
}
struct ProfileAvatar {QString url,alt;};
inline ProfileAvatar profileAvatar(const QString &html) {
    using namespace CommentHtml;const auto list=nodes(html);
    for(int i=0;i<list.size();++i)if(list[i].tag=="img") {
        bool header=false,profile=false,untrusted=false;
        for(int p=list[i].parent;p>=0;p=list[p].parent){header|=hasClass(list[p],"primary") && hasClass(list[p],"header");profile|=hasClass(list[p],"user") && hasClass(list[p],"profile");untrusted|=hasClass(list[p],"userstuff") || list[p].attrs.value("id")=="workskin";}
        if(!header || !profile || untrusted)continue;
        const auto url=QUrl("https://archiveofourown.org/").resolved(QUrl(list[i].attrs.value("src")));
        if(avatarUrlAllowed(url))return {url.toString(),list[i].attrs.value("alt")};
    }
    return {};
}
inline QImage decodeProfileAvatar(const QByteArray &bytes) {
    if(bytes.isEmpty() || bytes.size()>2*1024*1024)return {};
    QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);reader.setDecideFormatFromContent(true);
    const auto format=reader.format().toLower();if(format!="png" && format!="jpeg" && format!="gif")return {};
    const auto size=reader.size();if(!size.isValid() || size.width()>4096 || size.height()>4096 || qint64(size.width())*size.height()>4*1024*1024)return {};
    reader.setScaledSize(size.scaled(256,256,Qt::KeepAspectRatio));return reader.read();
}
