package org.ao3client.reader

import org.jsoup.Jsoup
import org.jsoup.nodes.Element
import java.net.URI

internal data class ArchiveEntry(val id:String,val title:String,val body:String,val target:String="",val actions:List<String> = emptyList(),val selection:String="",val unread:Boolean=false)
internal data class ArchivePage(val title:String,val entries:List<ArchiveEntry>,val page:Int,val pages:Int,val description:String="")
internal data class PageAction(val path:String,val fields:Map<String,String>)
internal fun ao3Path(value:String):String?=runCatching {val u=URI(Archive.BASE).resolve(value);u.path.takeIf{u.scheme=="https" && u.host=="archiveofourown.org" && u.port in listOf(-1,443) && u.userInfo==null && u.fragment==null}}.getOrNull()
private fun Element.trusted()=(listOf(this)+parents()).none{it.hasClass("userstuff")}
private fun entryTarget(root:Element):String=root.select("a[href]").firstOrNull {it.trusted() && ao3Path(it.attr("href"))?.matches(Regex("/(?:works/[0-9]+(?:/chapters/[0-9]+)?|series/[0-9]+|users/[A-Za-z0-9_]+(?:/pseuds/[^/]+)?)"))==true}?.let{ao3Path(it.attr("href"))}.orEmpty()
internal fun archivePageRoute(kind:String,user:String,id:String=""):String {
    require(user.isEmpty() || user.matches(Regex("[A-Za-z0-9_]+")))
    return when(kind){"series"->{require(id.matches(Regex("[0-9]+")));"/series/$id"};"bookmarks","subscriptions","inbox"->{require(user.isNotBlank());"/users/$user/$kind"};else->error("Unknown AO3 list.")}
}
internal fun parseArchivePage(html:String,kind:String,user:String,id:String,page:Int):ArchivePage {
    require(html.length<=8*1024*1024){"AO3 page is too large."}
    if(kind!="series")require(ArchiveParser.sessionIdentity(html)==user){"AO3 could not verify this account. Check native sign-in."}
    val doc=Jsoup.parse(html)
    val marker=when(kind){"series"->"series-show";"bookmarks"->"bookmarks-index";"subscriptions"->"subscriptions-index";"inbox"->"inbox-show";else->error("Unknown AO3 list.")}
    val main=doc.select("#main.$marker").firstOrNull{it.trusted()} ?: error("AO3 did not return this list. Open it on AO3 to check availability.")
    val title=main.selectFirst("h2.heading")?.text().orEmpty().ifBlank{kind.replaceFirstChar{it.uppercase()}}
    val entries=when(kind){
        "series","bookmarks"->main.select(if(kind=="series")"ul.series.work > li.work" else "ol.bookmark > li.bookmark").filter{it.trusted()}.mapIndexed{index,item->
            val heading=item.selectFirst("h4.heading")
            val clean=item.clone();clean.select("form, ul.actions").remove()
            ArchiveEntry(item.id().ifBlank{"entry-$index"},heading?.text().orEmpty().ifBlank{"Unavailable work"},clean.select("blockquote, .user.tags, dl.stats, .user .meta").joinToString("\n\n"){it.text()},heading?.let(::entryTarget).orEmpty().ifBlank{if(kind=="bookmarks")Regex("bookmark_([0-9]+)").matchEntire(item.id())?.groupValues?.get(1)?.let{"/bookmarks/$it"}.orEmpty() else ""})
        }
        "subscriptions"->main.select("dl.subscription > dt").filter{it.trusted()}.mapIndexed{index,item->
            val form=item.nextElementSibling()?.selectFirst("form")
            val subscription=form?.let{ao3Path(it.attr("action"))}?.let{Regex("/users/"+Regex.escape(user)+"/subscriptions/([0-9]+)").matchEntire(it)?.groupValues?.get(1)}
            ArchiveEntry(subscription ?: "missing-$index",item.text(),"",entryTarget(item),if(subscription!=null)listOf("Unsubscribe") else emptyList())
        }
        else->main.select("#inbox-form li[id^=feedback_comment_]").filter{it.trusted() && it.id().matches(Regex("feedback_comment_[0-9]+"))}.map{item->
            val key=item.select("input[name='inbox_comments[]']").firstOrNull{it.trusted()}?.attr("value").orEmpty()
            val unread=item.hasClass("unread")
            ArchiveEntry(item.id().removePrefix("feedback_comment_"),item.selectFirst("h4.byline")?.text().orEmpty().ifBlank{"Unavailable comment"},item.selectFirst("blockquote.userstuff, p.message")?.text().orEmpty(),"/comments/"+item.id().removePrefix("feedback_comment_"),if(key.matches(Regex("[0-9]+")))listOf(if(unread)"Mark read" else "Mark unread","Delete from inbox") else emptyList(),key,unread)
        }
    }
    val route=archivePageRoute(kind,user,id)
    val current=main.select(".pagination .current, .pagination [aria-current=page]").firstOrNull{it.trusted()}?.text()?.toIntOrNull()?.takeIf{it in 1..1000000} ?: page
    val pages=main.select(".pagination a[href]").filter{it.trusted() && ao3Path(it.attr("href"))==route}.mapNotNull{Regex("[?&]page=([0-9]+)").find(it.attr("href"))?.groupValues?.get(1)?.toIntOrNull()?.takeIf{it in 1..1000000}}.plus(current).maxOrNull() ?: current
    return ArchivePage(title,entries,current,pages,if(kind=="series")main.select("dl.series.meta").joinToString("\n\n"){it.wholeText().trim()} else "")
}
internal fun pageAction(html:String,kind:String,user:String,id:String,page:Int,entryId:String,action:String):PageAction {
    val parsed=parseArchivePage(html,kind,user,id,page)
    val entry=parsed.entries.singleOrNull{it.id==entryId} ?: error("This entry changed. Refresh the list.")
    require(action in entry.actions){"This action is unavailable."}
    val main=Jsoup.parse(html).selectFirst("#main")!!
    if(kind=="subscriptions") {
        val path="/users/$user/subscriptions/$entryId"
        val form=main.select("form").firstOrNull{it.trusted() && ao3Path(it.attr("action"))==path && it.attr("method").equals("post",true)} ?: error("Subscription form unavailable.")
        val fields=form.select("input[type=hidden][name]:not([disabled])").filter{it.trusted() && it.attr("name") in setOf("authenticity_token","_method")}.associate{it.attr("name") to it.attr("value")}
        require(fields["_method"]=="delete" && !fields["authenticity_token"].isNullOrBlank())
        return PageAction(path,fields)
    }
    require(kind=="inbox")
    val form=main.selectFirst("form#inbox-form") ?: error("Inbox form unavailable.")
    val path=ao3Path(form.attr("action"));require(path=="/users/$user/inbox" && form.attr("method").equals("post",true))
    val fields=form.select("input[type=hidden][name]:not([disabled])").filter{it.trusted() && it.attr("name") in setOf("authenticity_token","_method")}.associate{it.attr("name") to it.attr("value")}.toMutableMap()
    require(fields["_method"]=="put" && !fields["authenticity_token"].isNullOrBlank())
    val key=when(action){"Mark read"->"read";"Mark unread"->"unread";else->"delete"}
    val button=form.select("input[type=submit][name]:not([disabled]), button[name]:not([disabled])").firstOrNull{it.trusted() && it.attr("name")==key} ?: error("Inbox action unavailable.")
    fields[key]=button.attr("value");fields["inbox_comments[]"]=entry.selection
    val uri=URI(Archive.BASE).resolve(form.attr("action"))
    return PageAction(uri.rawPath+(uri.rawQuery?.let{"?$it"} ?: ""),fields)
}
