package org.ao3client.reader

import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.LinkAnnotation
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextLinkStyles
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextDecoration
import org.json.JSONObject
import org.jsoup.nodes.Element
import org.jsoup.nodes.Node
import org.jsoup.nodes.TextNode
import org.jsoup.select.NodeTraversor
import org.jsoup.select.NodeVisitor
import java.net.URI

data class TextStyleSpan(val start:Int,val end:Int,val bold:Boolean=false,val italic:Boolean=false,val underline:Boolean=false,val strike:Boolean=false,val code:Boolean=false,val url:String="") {
    fun json()=JSONObject().put("start",start).put("end",end).put("bold",bold).put("italic",italic).put("underline",underline).put("strike",strike).put("code",code).put("url",url)
    companion object {
        fun fromJson(value:JSONObject)=TextStyleSpan(value.optInt("start",-1),value.optInt("end",-1),value.optBoolean("bold"),value.optBoolean("italic"),value.optBoolean("underline"),value.optBoolean("strike"),value.optBoolean("code"),value.optString("url"))
    }
}
internal fun readingLink(target:String,workId:String=""):String? = runCatching {
    if(target.isBlank()) return null
    val url=URI("https://archiveofourown.org/"+if(workId.isEmpty()) "" else "works/$workId?view_full_work=true").resolve(target.trim())
    url.takeIf { it.scheme?.lowercase() in listOf("https","http") && !it.host.isNullOrEmpty() && it.rawUserInfo==null }?.toASCIIString()
}.getOrNull()

internal data class ParsedReadingText(val text:String,val styles:List<TextStyleSpan>)
internal fun readingText(element:Element):ParsedReadingText {
    val body=element.clone()
    body.select(".landmark, .hidden, script, style").remove()
    body.select("br").forEach { it.after("\n") }
    body.select("p, div, h1, h2, h3, h4, blockquote, li").forEach { it.appendText("\n\n") }
    val text=StringBuilder(); val starts=mutableMapOf<Element,Int>(); val styles=mutableListOf<TextStyleSpan>()
    NodeTraversor.traverse(object:NodeVisitor {
        override fun head(node:Node,depth:Int) {
            if(node is TextNode) text.append(node.wholeText)
            if(node is Element) { starts[node]=text.length; if(node.normalName()=="br") text.append('\n') }
        }
        override fun tail(node:Node,depth:Int) {
            if(node !is Element) return
            val tag=node.normalName(); val css=node.attr("style").lowercase()
            val bold=tag in listOf("b","strong","h1","h2","h3","h4","h5","h6") || Regex("font-weight\\s*:\\s*(bold|[7-9]00)").containsMatchIn(css)
            val italic=tag in listOf("i","em","cite","var") || Regex("font-style\\s*:\\s*italic").containsMatchIn(css)
            val underline=tag=="u" || Regex("text-decoration[^:]*:[^;]*underline").containsMatchIn(css)
            val strike=tag in listOf("s","strike","del") || Regex("text-decoration[^:]*:[^;]*line-through").containsMatchIn(css)
            val code=tag in listOf("code","pre","kbd","samp","tt")
            val url=node.attr("href").takeIf { tag=="a" && readingLink(it)!=null }.orEmpty()
            if(bold || italic || underline || strike || code || url.isNotEmpty()) styles+=TextStyleSpan(starts.getValue(node),text.length,bold,italic,underline,strike,code,url)
        }
    },body)
    val original=text.toString(); val trimmed=original.trim(); val left=original.indexOf(trimmed).coerceAtLeast(0)
    val clipped=styles.map { it.copy(start=(it.start-left).coerceAtLeast(0),end=(it.end-left).coerceAtMost(trimmed.length)) }.filter { it.end>it.start }
    return ParsedReadingText(trimmed,clipped)
}
internal fun Chapter.styledText(workId:String):AnnotatedString = buildAnnotatedString {
    append(text)
    for(span in styles) {
        if(span.start<0 || span.end<=span.start || span.end>text.length) continue
        val url=readingLink(span.url,workId)
        val decorations=buildList { if(span.underline || url!=null) add(TextDecoration.Underline); if(span.strike) add(TextDecoration.LineThrough) }
        addStyle(SpanStyle(fontWeight=if(span.bold) FontWeight.Bold else null,fontStyle=if(span.italic) FontStyle.Italic else null,fontFamily=if(span.code) FontFamily.Monospace else null,textDecoration=decorations.takeIf { it.isNotEmpty() }?.let { TextDecoration.combine(it) }),span.start,span.end)
        if(url!=null) addLink(LinkAnnotation.Url(url,TextLinkStyles(style=SpanStyle(textDecoration=TextDecoration.Underline))),span.start,span.end)
    }
}
