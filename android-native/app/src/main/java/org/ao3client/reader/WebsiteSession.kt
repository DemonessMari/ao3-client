package org.ao3client.reader
import java.net.HttpCookie

fun validWebsiteSessionIdentity(user:String,agent:String)=user.matches(Regex("[A-Za-z0-9_]+")) && agent.isNotBlank() && agent.length<=512 && '\r' !in agent && '\n' !in agent
fun websiteSessionCookies(header:String):List<HttpCookie> {
    require(header.length<=65536 && '\r' !in header && '\n' !in header) {"Invalid website cookies"}
    return header.split(';').mapNotNull {entry ->
        val equal=entry.indexOf('=');if(equal<1)return@mapNotNull null
        val name=entry.substring(0,equal).trim();val value=entry.substring(equal+1).trim()
        if(!name.matches(Regex("[A-Za-z0-9_-]+")) || value.isEmpty())null
        else HttpCookie(name,value).apply {domain="archiveofourown.org";path="/";secure=true;isHttpOnly=true;version=0}
    }
}
