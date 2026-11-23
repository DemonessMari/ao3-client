package org.ao3client.reader

import org.jsoup.Jsoup
import java.net.URI

internal fun parseAccountReadings(html:String,user:String,page:Int):Listing {
    require(html.length<=8*1024*1024){"AO3 reading list is too large."}
    require(ArchiveParser.sessionIdentity(html)==user){"AO3 could not verify this account's reading list. Check native sign-in and try again."}
    val doc=Jsoup.parse(html)
    val main=doc.select("#main.readings-index").firstOrNull{it.parents().none{p->p.hasClass("userstuff")}}
        ?: error("AO3 did not return your reading list. If history is disabled, enable it in AO3 preferences.")
    val lists=main.select("ol.reading.work").filter{it.parents().none{p->p.hasClass("userstuff")}}
    val activity=mutableMapOf<String,String>();val unavailable=mutableListOf<String>()
    lists.forEach{list->list.select("li.reading.work").filter{item->item.parents().none{it.hasClass("userstuff") || it.tagName()=="li" && it.hasClass("work")}}.forEach { item->
        val info=item.select("h4.viewed").firstOrNull{it.parents().none{p->p.hasClass("userstuff")}}?.text().orEmpty()
        val id=Regex("work_([0-9]+)").matchEntire(item.id())?.groupValues?.get(1)
        if(id==null){if(info.isNotBlank())unavailable+=info}else activity[id]=info
    }}
    val clean=Jsoup.parse("<ol class='work'>${lists.joinToString(""){it.html()}}</ol>")
    clean.select(".userstuff li.work").remove()
    val listing=ArchiveParser.listing(clean.outerHtml(),page)
    val pages=main.select(".pagination a[href]").filter{it.parents().none{p->p.hasClass("userstuff")}}.mapNotNull{link->runCatching {
        val url=URI(Archive.BASE).resolve(link.attr("href"))
        if(url.scheme!="https" || url.host!="archiveofourown.org" || url.port !in listOf(-1,443) || url.userInfo!=null || url.path!="/users/$user/readings")null
        else Regex("(?:^|&)page=([0-9]+)(?:&|$)").find(url.rawQuery.orEmpty())?.groupValues?.get(1)?.toIntOrNull()?.takeIf{it in 1..1000000}
    }.getOrNull()}.plus(page).maxOrNull() ?: page
    return listing.copy(works=listing.works.map{it.copy(readingInfo=activity[it.id].orEmpty())},pages=pages,unavailable=unavailable)
}
