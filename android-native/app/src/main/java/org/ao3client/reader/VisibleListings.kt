package org.ao3client.reader

import org.jsoup.Jsoup
import org.jsoup.nodes.Document

internal fun visibleListingDocument(html: String): Document {
    val doc=Jsoup.parse(html)
    val rule=Regex("""\s*((?:\.user-[0-9]+\s*,\s*)*\.user-[0-9]+)\s*\{\s*display\s*:\s*none\s*!important\s*;\s*visibility\s*:\s*hidden\s*!important\s*;?\s*\}\s*""",RegexOption.IGNORE_CASE)
    val muted=doc.head().select("style").mapNotNull { rule.matchEntire(it.data())?.groupValues?.get(1) }
        .flatMap { it.split(',').map { name->name.trim().removePrefix(".") } }.toSet()
    if(muted.isNotEmpty())doc.select("li, div").filter { element->
        element.classNames().any { it in muted } &&
            (listOf(element)+element.parents()).none { it.hasClass("userstuff") || it.id()=="workskin" || it.tagName()=="textarea" }
    }.forEach { it.remove() }
    return doc
}
