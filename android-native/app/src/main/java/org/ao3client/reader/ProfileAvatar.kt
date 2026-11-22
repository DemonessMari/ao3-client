package org.ao3client.reader

import java.net.URI
import java.net.URL
import java.net.HttpURLConnection
import java.io.ByteArrayOutputStream
import org.jsoup.nodes.Element

internal fun avatarUrlAllowed(value:String,redirect:Boolean=false):Boolean=runCatching {
    val url=URI(value);val host=url.host.orEmpty().lowercase()
    url.scheme=="https" && url.rawUserInfo==null && url.port in listOf(-1,443) && url.rawFragment==null && value.length<=4096 &&
        (redirect && host.endsWith(".amazonaws.com") || host=="archiveofourown.org" && listOf("/system/pseud/icons/","/rails/active_storage/","/images/skins/iconsets/").any{url.path.orEmpty().startsWith(it)})
}.getOrDefault(false)
internal fun profileAvatar(root:Element):Pair<String,String> {
    root.select(".primary.header img").forEach{image->
        if(image.parents().any{it.hasClass("userstuff") || it.id()=="workskin"})return@forEach
        val url=runCatching{URI(Archive.BASE+"/").resolve(image.attr("src")).toString()}.getOrDefault("")
        if(avatarUrlAllowed(url))return url to image.attr("alt")
    }
    return "" to ""
}
internal fun loadAvatarBytes(initial:String,cookie:String?,agent:String,open:(URL)->HttpURLConnection={it.openConnection() as HttpURLConnection}):ByteArray {
    require(avatarUrlAllowed(initial));var url=URL(initial)
    repeat(4){hop->
        require(avatarUrlAllowed(url.toString(),hop>0))
        val connection=open(url)
        try {
            connection.instanceFollowRedirects=false;connection.connectTimeout=10000;connection.readTimeout=15000
            connection.setRequestProperty("User-Agent",agent);connection.setRequestProperty("Accept","image/png,image/jpeg,image/gif")
            if(url.host=="archiveofourown.org" && !cookie.isNullOrEmpty())connection.setRequestProperty("Cookie",cookie)
            val status=connection.responseCode
            if(status in 300..399){require(hop<3);url=URL(url,connection.getHeaderField("Location")?:error("Missing image redirect"));return@repeat}
            require(status==200 && connection.contentLengthLong<=2*1024*1024){"Profile image unavailable"}
            return connection.inputStream.use{input->val out=ByteArrayOutputStream();val buffer=ByteArray(8192);while(true){val n=input.read(buffer);if(n<0)break;require(out.size()+n<=2*1024*1024){"Profile image is too large"};out.write(buffer,0,n)};out.toByteArray()}
        }finally{connection.disconnect()}
    }
    error("Too many image redirects")
}
