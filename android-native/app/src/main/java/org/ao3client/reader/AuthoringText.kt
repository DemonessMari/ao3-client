package org.ao3client.reader

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Typeface
import android.text.*
import android.text.style.*
import org.jsoup.Jsoup
import org.jsoup.nodes.Node
import org.jsoup.nodes.TextNode
import org.jsoup.nodes.Element

internal class AuthorHeading(val level:Int):RelativeSizeSpan(when(level){1->2f;2->1.5f;3->1.17f;4->1f;5->.83f;else->.67f})
internal class AuthorList(val ordered:Boolean):LeadingMarginSpan {
    override fun getLeadingMargin(first:Boolean)=(24*android.content.res.Resources.getSystem().displayMetrics.density).toInt()
    override fun drawLeadingMargin(c:Canvas,p:Paint,x:Int,dir:Int,top:Int,baseline:Int,bottom:Int,text:CharSequence,start:Int,end:Int,first:Boolean,layout:Layout){if(first){
        var number=1
        if(ordered && text is Spanned){val plain=text.toString();var position=plain.lastIndexOf('\n',(start-1).coerceAtLeast(-1));while(position>=0){val previous=plain.lastIndexOf('\n',position-1)+1;if(text.getSpans(previous,position,AuthorList::class.java).none{it.ordered && text.getSpanStart(it)<position && text.getSpanEnd(it)>previous})break;number++;position=previous-1}}
        val marker=if(ordered)"$number." else "•";c.drawText(marker,x+dir*4*android.content.res.Resources.getSystem().displayMetrics.density,baseline.toFloat(),p)
    }}
}
internal object AuthoringText {
    fun field(name:String)=listOf("[content]","[summary]","[notes]","[endnotes]").any{name.endsWith(it)}
    fun supported(html:String):Boolean {
        val tags=setOf("p","br","div","span","b","strong","i","em","u","s","strike","del","h1","h2","h3","h4","h5","h6","ul","ol","li","a")
        return Jsoup.parseBodyFragment(html).body().getAllElements().drop(1).all{e->e.tagName() in tags && e.attributes().all{e.tagName()=="a" && it.key=="href"} && !(e.tagName() in setOf("ul","ol") && e.parents().any{it.tagName() in setOf("ul","ol")})}
    }
    fun parse(html:String):SpannableStringBuilder {
        val out=SpannableStringBuilder()
        fun walk(n:Node) {
            if(n is TextNode){if(n.isBlank && n.parentNode()?.nodeName() in setOf("body","ul","ol"))return;out.append(n.wholeText.replace(Regex("[\\s]+")," "));return}
            if(n !is Element)return
            val tag=n.tagName();if(tag=="br"){out.append('\u2028');return}
            val block=tag in setOf("p","div","li","h1","h2","h3","h4","h5","h6")
            if(block && out.isNotEmpty() && out.last()!='\n')out.append('\n')
            val start=out.length;n.childNodes().forEach(::walk);val end=out.length
            val span:Any?=when(tag){"b","strong"->StyleSpan(Typeface.BOLD);"i","em"->StyleSpan(Typeface.ITALIC);"u"->UnderlineSpan();"s","strike","del"->StrikethroughSpan();"a"->n.attr("href").takeIf{it.isNotEmpty()}?.let{URLSpan(it)};"h1","h2","h3","h4","h5","h6"->AuthorHeading(tag.drop(1).toInt());else->null}
            if(span!=null && end>start)out.setSpan(span,start,end,Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
            if(block){if(out.isEmpty() || out.last()!='\n')out.append('\n');if(tag=="li"){val parent=n.parent();out.setSpan(AuthorList(parent?.tagName()=="ol"),start,out.length,Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)}}
        }
        Jsoup.parseBodyFragment(html).body().childNodes().forEach(::walk)
        if(out.endsWith("\n"))out.delete(out.length-1,out.length)
        return out
    }
    fun html(text:Spanned):String {
        val out=StringBuilder();var position=0;var list:String?=null
        val lines=text.toString().split('\n')
        for(line in lines) {
            val start=position;val end=start+line.length
            fun <T> spans(type:Class<T>)=text.getSpans(start,end,type).filter{if(start==end)text.getSpanStart(it)==start else text.getSpanStart(it)<end && text.getSpanEnd(it)>start}
            val item=spans(AuthorList::class.java).firstOrNull();val next=if(item==null)null else if(item.ordered)"ol" else "ul"
            if(list!=next){if(list!=null)out.append("</$list>");if(next!=null)out.append("<$next>");list=next}
            val heading=spans(AuthorHeading::class.java).firstOrNull()?.level;val tag=if(item!=null)"li" else if(heading!=null)"h$heading" else "p";out.append("<$tag>")
            var i=start
            while(i<end){val until=text.nextSpanTransition(i,end,Any::class.java).coerceAtLeast(i+1);var value=TextUtils.htmlEncode(text.subSequence(i,until).toString()).replace("\u2028","<br>")
                fun <T> active(type:Class<T>)=text.getSpans(i,until,type).filter{text.getSpanStart(it)<until && text.getSpanEnd(it)>i}
                val styles=active(StyleSpan::class.java).fold(0){a,s->a or s.style}
                if(active(StrikethroughSpan::class.java).isNotEmpty())value="<s>$value</s>";if(active(UnderlineSpan::class.java).isNotEmpty())value="<u>$value</u>";if(styles and Typeface.ITALIC!=0)value="<em>$value</em>";if(styles and Typeface.BOLD!=0)value="<strong>$value</strong>"
                active(URLSpan::class.java).firstOrNull()?.let{value="<a href=\"${TextUtils.htmlEncode(it.url)}\">$value</a>"};out.append(value);i=until
            }
            if(line.isEmpty())out.append("<br>");out.append("</$tag>");position=end+1
        }
        if(list!=null)out.append("</$list>");return out.toString()
    }
    fun toggle(text:Spannable,start:Int,end:Int,kind:String) {
        if(start<0 || end<=start || end>text.length)return
        fun matches(s:Any)=when(kind){"Bold"->s is StyleSpan && s.style==Typeface.BOLD;"Italic"->s is StyleSpan && s.style==Typeface.ITALIC;"Underline"->s is UnderlineSpan;else->s is StrikethroughSpan}
        val old=text.getSpans(start,end,Any::class.java).filter(::matches)
        var covered=start
        for(span in old.sortedBy{text.getSpanStart(it)}){if(text.getSpanStart(span)>covered)break;covered=maxOf(covered,text.getSpanEnd(span))}
        val remove=covered>=end
        for(s in old){
            val a=text.getSpanStart(s);val b=text.getSpanEnd(s);text.removeSpan(s)
            fun copySpan():Any=when(s){is StyleSpan->StyleSpan(s.style);is UnderlineSpan->UnderlineSpan();else->StrikethroughSpan()}
            if(a<start)text.setSpan(copySpan(),a,start,Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
            if(b>end)text.setSpan(copySpan(),end,b,Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        }
        if(!remove){val span:Any=when(kind){"Bold"->StyleSpan(Typeface.BOLD);"Italic"->StyleSpan(Typeface.ITALIC);"Underline"->UnderlineSpan();else->StrikethroughSpan()};text.setSpan(span,start,end,Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)}
    }
    fun link(text:Spannable,start:Int,end:Int,url:String) {
        if(start<0 || end<=start || end>text.length)return
        for(old in text.getSpans(start,end,URLSpan::class.java)){val a=text.getSpanStart(old);val b=text.getSpanEnd(old);text.removeSpan(old);if(a<start)text.setSpan(URLSpan(old.url),a,start,Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);if(b>end)text.setSpan(URLSpan(old.url),end,b,Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)}
        if(url.isNotBlank())text.setSpan(URLSpan(url),start,end,Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
    }
    fun paragraph(text:Spannable,start:Int,end:Int,kind:String) {
        if(start<0 || end<0)return
        val a=text.toString().lastIndexOf('\n',(start-1).coerceAtLeast(-1))+1
        val b=text.toString().indexOf('\n',(if(end>start)end-1 else end).coerceAtMost(text.length)).let{if(it<0)text.length else it}
        var line=a
        while(line<=b){val until=text.toString().indexOf('\n',line).let{if(it<0 || it>b)b else it};val old=text.getSpans(line,until,Any::class.java).filter{it is AuthorHeading || it is AuthorList};old.forEach{text.removeSpan(it)}
            if(kind=="Heading")text.setSpan(AuthorHeading(2),line,until,if(line==until)Spanned.SPAN_INCLUSIVE_INCLUSIVE else Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
            if(kind=="Bullets" || kind=="Numbered")text.setSpan(AuthorList(kind=="Numbered"),line,until,if(line==until)Spanned.SPAN_INCLUSIVE_INCLUSIVE else Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
            if(until>=b)break;line=until+1
        }
    }
}

internal class AuthorHistory(initial:Spanned) {
    private val past=java.util.ArrayDeque<SpannableString>()
    private val future=java.util.ArrayDeque<SpannableString>()
    private var current=SpannableString(initial)
    fun record(text:Spanned){past.addLast(current);current=SpannableString(text);future.clear();while(past.size>40 || past.sumOf{it.length.toLong()}+current.length>2_000_000){if(past.isEmpty())break;past.removeFirst()}}
    fun undo():SpannableString? {if(past.isEmpty())return null;future.addLast(current);current=past.removeLast();return SpannableString(current)}
    fun redo():SpannableString? {if(future.isEmpty())return null;past.addLast(current);current=future.removeLast();return SpannableString(current)}
}
