package org.ao3client.reader

import org.jsoup.Jsoup
import java.net.URI

data class SubscriptionForm(val path:String,val fields:Map<String,String>,val subscribed:Boolean)
object Subscriptions {
    fun form(html:String,id:String,user:String):SubscriptionForm? {
        if(!user.matches(Regex("[A-Za-z0-9_]+")))return null
        for(form in Jsoup.parse(html).select(".subscribe form")) {
            if(form.parents().any {it.id()=="workskin" || it.hasClass("userstuff")})continue
            val url=runCatching {URI(Archive.BASE).resolve(form.attr("action"))}.getOrNull() ?: continue
            if(!form.attr("method").equals("post",true) || url.scheme!="https" || url.host!="archiveofourown.org" || url.userInfo!=null || url.port !in listOf(-1,443) || url.rawQuery!=null || url.rawFragment!=null)continue
            val base="/users/$user/subscriptions"
            val subscribed=url.path.matches(Regex(Regex.escape(base)+"/[0-9]+"))
            if(url.path!=base && !subscribed)continue
            val fields=form.select("input[type=hidden][name]:not([disabled])").filter {it.attr("name") in setOf("authenticity_token","_method","subscription[subscribable_id]","subscription[subscribable_type]")}.associate {it.attr("name") to it.attr("value")}
            if(fields["subscription[subscribable_id]"]!=id || fields["subscription[subscribable_type]"]!="Work" || fields["authenticity_token"].isNullOrBlank())continue
            if(if(subscribed) fields["_method"]!="delete" else fields.containsKey("_method"))continue
            return SubscriptionForm(url.path,fields,subscribed)
        }
        return null
    }
}
