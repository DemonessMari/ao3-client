package org.ao3client.reader

import org.jsoup.Jsoup
import java.net.URI

internal object DiscoverySearch {
    private fun choices(vararg pairs:Pair<String,String>)=pairs.map{Choice(it.first,it.second)}
    fun fields(kind:String):List<FilterField> = when(kind) {
        "people" -> listOf(FilterField("query","All fields"),FilterField("name","Name"),FilterField("fandom","Fandom",suggestion="fandom"))
        "tags" -> listOf(
            FilterField("name","Tag name"),FilterField("fandoms","Fandoms",suggestion="fandom"),
            FilterField("type","Tag type",choices=choices("Fandom" to "Fandom","Character" to "Character","Relationship" to "Relationship","Additional tag" to "Freeform")),
            FilterField("wrangling_status","Tag status",choices=choices("Canonical" to "canonical","Noncanonical" to "noncanonical","Synonymous" to "synonymous","Canonical or synonymous" to "canonical_synonymous","Noncanonical and nonsynonymous" to "noncanonical_nonsynonymous","Noncanonical, nonsynonymous and wrangleable" to "noncanonical_nonsynonymous_not_unwrangleable","Unwrangleable" to "unwrangleable")),
            FilterField("sort_column","Sort by",choices=choices("Name" to "name","Date created" to "created_at","Uses" to "uses")),FilterField("sort_direction","Sort direction",choices=choices("Ascending" to "asc","Descending" to "desc")))
        "bookmarks" -> listOf(
            FilterField("bookmarkable_query","Any field on work"),FilterField("other_tag_names","Work tags",suggestion="tag"),
            FilterField("bookmarkable_type","Item type",choices=choices("Work" to "Work","Series" to "Series","External work" to "External Work")),
            FilterField("word_count","Word count"),FilterField("language_id","Work language",choices=Catalogue.languages),FilterField("bookmarkable_date","Date updated"),
            FilterField("bookmark_query","Any field on bookmark"),FilterField("other_bookmark_tag_names","Bookmarker’s tags",suggestion="tag"),FilterField("bookmarker","Bookmarker"),FilterField("bookmark_notes","Notes"),
            FilterField("rec","Recommendations only",choices=choices("Yes" to "1")),FilterField("with_notes","With notes only",choices=choices("Yes" to "1")),FilterField("date","Date bookmarked"),
            FilterField("sort_column","Sort by",choices=choices("Date bookmarked" to "created_at","Date updated" to "bookmarkable_date","Word count" to "word_count")))
        else -> emptyList()
    }
    fun title(kind:String)=when(kind){"people"->"People search";"tags"->"Tag search";else->"Bookmark search"}
    fun path(kind:String,values:Map<String,String>,page:Int):String {
        val spec=fields(kind);require(spec.isNotEmpty() && page in 1..1000000)
        val prefix=when(kind){"people"->"people_search";"tags"->"tag_search";else->"bookmark_search"}
        val args=spec.mapNotNull{f->values[f.key]?.trim()?.takeIf{it.isNotEmpty()}?.let{"$prefix[${f.key}]" to it}}.toMutableList()
        val primary=when(kind){"people"->"query";"tags"->"name";else->"bookmarkable_query"}
        if(args.none{it.first=="$prefix[$primary]"})args+="$prefix[$primary]" to ""
        args+="page" to "$page"
        return "/$kind/search?"+args.joinToString("&"){Archive.encode(it.first)+"="+Archive.encode(it.second)}
    }
    fun parse(html:String,kind:String,page:Int):ArchivePage {
        require(fields(kind).isNotEmpty() && html.length<=8*1024*1024)
        val doc=visibleListingDocument(html)
        val main=doc.select("#main.$kind-search").firstOrNull{it.parents().none{p->p.hasClass("userstuff")}}
            ?: error("AO3 did not return search results. Try again when the site is available.")
        fun org.jsoup.nodes.Element.trusted()=(listOf(this)+parents()).none{it.hasClass("userstuff")}
        val marker=when(kind){"people"->"pseud";"tags"->"tag";else->"bookmark"}
        val root=main.select("ol.$marker.index").firstOrNull{it.trusted()}
        require(root!=null || kind=="bookmarks" && main.text().contains("No results found",true)){"AO3 did not return search results."}
        val entries=root?.children()?.filter{it.tagName()=="li"}?.mapIndexed{index,item->
            val heading=item.select("h4.heading").firstOrNull{it.trusted()}
            val link=(if(kind=="tags")item else heading)?.select("a[href]")?.firstOrNull{a->
                a.trusted() && ao3Path(a.attr("href"))?.matches(if(kind=="tags")Regex("/tags/[^/]+") else Regex("/(?:works/[0-9]+|series/[0-9]+|users/[A-Za-z0-9_]+(?:/pseuds/[^/]+)?)"))==true
            }
            val id=item.id().ifBlank{"entry-$index"}
            val name=if(kind=="tags")link?.text().orEmpty() else heading?.text().orEmpty()
            val tagField=mapOf("Fandom" to "fandom_names","Character" to "character_names","Relationship" to "relationship_names","Freeform" to "freeform_names")[item.text().substringBefore(':').trim()]
            val target=if(kind=="tags" && name.isNotEmpty())if(tagField!=null)"/works/search?"+Archive.encode("work_search[$tagField]")+"="+Archive.encode(name) else link?.let{ao3Path(it.attr("href"))}.orEmpty()
                else link?.let{ao3Path(it.attr("href"))}.orEmpty().ifBlank{if(kind=="bookmarks" && id.matches(Regex("bookmark_[0-9]+")))"/bookmarks/"+id.removePrefix("bookmark_") else ""}
            val body=if(kind=="tags")item.text() else item.select("blockquote.userstuff, dl.stats, h5.heading, h5.byline").filter{el->el.tagName()=="blockquote" || el.trusted()}.joinToString("\n\n"){it.text()}
            ArchiveEntry(id,name.ifBlank{if(kind=="bookmarks")"Unavailable bookmarked item" else "Unavailable entry"},body,target,if(kind=="tags" && item.select(".canonical").isNotEmpty() && link!=null)listOf("Tag feed") else emptyList(),if(kind=="tags" && link!=null)URI(Archive.BASE).resolve(link.attr("href")).rawPath+"/feed.atom" else "")
        }.orEmpty()
        val pages=main.select(".pagination a[href]").filter{it.trusted()}.mapNotNull{a->
            runCatching{val u=URI(Archive.BASE).resolve(a.attr("href"));if(ao3Path(u.toString())!="/$kind/search")null else u.rawQuery?.split('&')?.firstOrNull{it.startsWith("page=")}?.substringAfter('=')?.toIntOrNull()?.takeIf{it in 1..1000000}}.getOrNull()
        }.plus(page).maxOrNull() ?: page
        return ArchivePage(title(kind),entries,page,pages)
    }
}
