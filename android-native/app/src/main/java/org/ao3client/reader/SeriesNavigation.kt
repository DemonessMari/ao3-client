package org.ao3client.reader

import org.jsoup.Jsoup
import java.net.URI

internal data class SeriesNavigation(val id:String,val title:String,val position:String,val previous:String?,val next:String?)
internal fun archiveNumericId(href:String,type:String):String? = runCatching {
    val url=URI(Archive.BASE).resolve(href)
    if(url.scheme!="https" || url.host!="archiveofourown.org" || url.port !in listOf(-1,443) || url.userInfo!=null)null
    else Regex("/$type/([0-9]+)").matchEntire(url.path)?.groupValues?.get(1)
}.getOrNull()
internal fun parseWorkSeries(html:String):List<SeriesNavigation> {
    require(html.length<=8*1024*1024){"Work metadata is too large."}
    val doc=Jsoup.parse(html)
    val meta=doc.select("dl.work.meta").firstOrNull {node->node.parents().none{it.hasClass("userstuff")}}
        ?: error("AO3 did not return work metadata. Check whether this work loads on the website.")
    return meta.select("span.series").filter{node->node.parents().none{it.hasClass("userstuff")}}.mapNotNull { row ->
        val position=row.selectFirst(".position") ?: return@mapNotNull null
        val link=position.select("a[href]").firstOrNull {archiveNumericId(it.attr("href"),"series")!=null} ?: return@mapNotNull null
        SeriesNavigation(archiveNumericId(link.attr("href"),"series")!!,link.text(),position.text(),
            row.select("a.previous[href]").firstOrNull{link->(listOf(link)+link.parents()).none{it.hasClass("userstuff")}}?.let{archiveNumericId(it.attr("href"),"works")},
            row.select("a.next[href]").firstOrNull{link->(listOf(link)+link.parents()).none{it.hasClass("userstuff")}}?.let{archiveNumericId(it.attr("href"),"works")})
    }
}
