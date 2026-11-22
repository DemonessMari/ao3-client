package org.ao3client.reader

import org.jsoup.Jsoup
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.items
import androidx.compose.ui.unit.dp

data class ArchiveComment(val id:String,val author:String,val authorPath:String,val date:String,val text:String,val depth:Int)
data class CommentPage(val comments:List<ArchiveComment>,val page:Int,val pages:Int)
object CommentParser {
    fun parse(html:String,page:Int=1):CommentPage {
        require(html.length<=8*1024*1024) { "Comments page is too large" }
        val doc=Jsoup.parse(html)
        check(doc.select("#comments_placeholder, #feedback, ol.thread, li[id^=comment_]").isNotEmpty()) { "AO3 did not return a comments page. Sign in if the work is restricted." }
        val comments=doc.select("li.comment[id^=comment_]").filter { it.id().matches(Regex("comment_\\d+")) && it.parents().none { p->p.id()=="workskin" || p.hasClass("userstuff") } }.map { element ->
            val copy=element.clone();copy.select("ol.thread").remove()
            val heading=copy.selectFirst("h4.byline")
            val date=heading?.selectFirst(".posted.datetime")?.text().orEmpty()
            heading?.select(".posted.datetime")?.remove()
            val author=heading?.selectFirst("a[href^=/users/]")
            val body=copy.selectFirst("blockquote.userstuff")
            body?.select("br")?.forEach { it.after("\n") }
            body?.select("p, div, li, blockquote")?.forEach { it.appendText("\n\n") }
            ArchiveComment(element.id().removePrefix("comment_"), author?.text() ?: heading?.text() ?: "Comment",author?.attr("href").orEmpty(),date,body?.wholeText()?.trim() ?: copy.selectFirst("p.message, p")?.text().orEmpty(),(element.parents().count { it.tagName()=="ol" && it.hasClass("thread") }-1).coerceIn(0,8))
        }
        val pages=doc.select(".pagination a[href]").mapNotNull { Regex("[?&]page=(\\d+)").find(it.attr("href"))?.groupValues?.get(1)?.toIntOrNull() }.maxOrNull() ?: page
        return CommentPage(comments,page,maxOf(page,pages))
    }
}

@androidx.compose.runtime.Composable
fun CommentsScreen(vm:ClientState) {
    val page=vm.commentsPage
    androidx.compose.foundation.lazy.LazyColumn(
        modifier=androidx.compose.ui.Modifier.fillMaxSize(),
        contentPadding=androidx.compose.foundation.layout.PaddingValues(16.dp),
        verticalArrangement=androidx.compose.foundation.layout.Arrangement.spacedBy(12.dp)
    ) {
        item { androidx.compose.material3.OutlinedButton(onClick={vm.showComments(vm.commentsWork,page?.page ?: 1)},enabled=!vm.loading) { androidx.compose.material3.Text("Refresh") } }
        item { OutlinedButton(onClick={vm.composeComment()},enabled=!vm.loading && !vm.commentSending && !vm.commentUncertain) { Text("Add comment") } }
        vm.commentForm?.let { form -> item {
            Column(Modifier.fillMaxWidth(),verticalArrangement=Arrangement.spacedBy(8.dp)) {
                Text(if(form.context.isBlank()) "New comment" else "Reply to comment #${form.context}",style=MaterialTheme.typography.titleMedium)
                if(form.notice.isNotBlank()) Text(form.notice)
                if(form.pseuds.size>1) form.pseuds.forEach { (id,label) -> Row { RadioButton(selected=vm.commentPseud==id,onClick={vm.commentPseud=id},enabled=!vm.commentSending);Text(label) } }
                if(form.guest) {
                    OutlinedTextField(value=vm.commentName,onValueChange={vm.commentName=it},label={Text("Name")},enabled=!vm.commentSending,modifier=Modifier.fillMaxWidth())
                    OutlinedTextField(value=vm.commentEmail,onValueChange={vm.commentEmail=it},label={Text("Email (not displayed)")},enabled=!vm.commentSending,modifier=Modifier.fillMaxWidth())
                }
                OutlinedTextField(value=vm.commentDraft,onValueChange={vm.commentDraft=it},label={Text("Comment")},minLines=4,enabled=!vm.commentSending,modifier=Modifier.fillMaxWidth())
                Row(horizontalArrangement=Arrangement.spacedBy(12.dp)) {
                    Button(onClick={vm.sendComment()},enabled=!vm.commentSending && !vm.commentUncertain && vm.commentDraft.isNotBlank() && (!form.guest || (vm.commentName.isNotBlank() && vm.commentEmail.isNotBlank()))) { Text(if(vm.commentSending) "Posting…" else "Post") }
                    TextButton(onClick={vm.closeComment()},enabled=!vm.commentSending) { Text("Close") }
                }
            }
        } }
        if(page!=null) {
            if(page.comments.isEmpty()) item { androidx.compose.material3.Text("No comments.") }
            items(page.comments,key={it.id}) { comment ->
                androidx.compose.foundation.layout.Column(androidx.compose.ui.Modifier.fillMaxWidth().padding(start=(comment.depth.coerceAtMost(4)*8).dp),verticalArrangement=androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)) {
                    androidx.compose.material3.Text(comment.author,style=androidx.compose.material3.MaterialTheme.typography.titleMedium)
                    if(comment.date.isNotBlank()) androidx.compose.material3.Text(comment.date,style=androidx.compose.material3.MaterialTheme.typography.bodySmall)
                    androidx.compose.foundation.text.selection.SelectionContainer { androidx.compose.material3.Text(comment.text) }
                    if(!comment.text.contains("This comment has been deleted")) TextButton(onClick={vm.composeComment(comment.id)},enabled=!vm.loading && !vm.commentSending && !vm.commentUncertain) { Text("Reply") }
                    androidx.compose.material3.HorizontalDivider()
                }
            }
            item { androidx.compose.foundation.layout.Row(androidx.compose.ui.Modifier.fillMaxWidth(),horizontalArrangement=androidx.compose.foundation.layout.Arrangement.SpaceBetween) {
                androidx.compose.material3.TextButton(onClick={vm.showComments(vm.commentsWork,page.page-1)},enabled=page.page>1 && !vm.loading) { androidx.compose.material3.Text("Previous") }
                androidx.compose.material3.Text("${page.page} / ${page.pages}")
                androidx.compose.material3.TextButton(onClick={vm.showComments(vm.commentsWork,page.page+1)},enabled=page.page<page.pages && !vm.loading) { androidx.compose.material3.Text("Next") }
            } }
        }
    }
}

data class CommentForm(val action:String,val hidden:Map<String,String>,val pseuds:List<Pair<String,String>>,val guest:Boolean,val notice:String,val context:String="")
data class CommentSubmission(val notice:String="",val error:String="",val uncertain:Boolean=false)
fun validCommentAction(action:String):Boolean = try {
    val uri=java.net.URI(Archive.BASE).resolve(action)
    uri.scheme=="https" && uri.host=="archiveofourown.org" && (uri.port==-1 || uri.port==443) && uri.userInfo==null && uri.rawQuery==null && uri.rawFragment==null && uri.path.matches(Regex("/(?:works/\\d+/|chapters/\\d+/|comments/\\d+/)?comments"))
} catch(_:Exception) { false }
fun parseCommentForm(html:String):CommentForm {
    val doc=Jsoup.parse(html)
    val form=doc.select("form").firstOrNull { it.parents().none {p->p.id()=="workskin" || p.hasClass("userstuff")} && it.attr("method").equals("post",true) && it.select("textarea[name='comment[comment_content]']").isNotEmpty() }
        ?: error("Commenting is unavailable. Check the work's permissions or sign in.")
    check(validCommentAction(form.attr("action"))) { "AO3 returned an invalid comment form" }
    val hidden=linkedMapOf<String,String>()
    form.select("input[type=hidden][name], input[type=submit][name]").filterNot { it.hasAttr("disabled") }.forEach { hidden[it.attr("name")]=it.attr("value") }
    val pseuds=form.select("select[name='comment[pseud_id]'] option").map { it.attr("value") to it.text() }
    if(pseuds.isNotEmpty()) hidden["comment[pseud_id]"]=form.selectFirst("select[name='comment[pseud_id]'] option[selected]")?.attr("value") ?: pseuds.first().first
    check(!hidden["authenticity_token"].isNullOrBlank() && "_method" !in hidden) { "AO3 did not return a valid new-comment form" }
    return CommentForm(form.attr("action"),hidden,pseuds,form.select("input[name='comment[name]'], input[name='comment[email]']").isNotEmpty(),form.select(".notice").joinToString("\n") { it.text() })
}
fun parseCommentSubmission(html:String):CommentSubmission {
    val doc=Jsoup.parse(html)
    val notice=doc.select(".flash.comment_notice").firstOrNull { node -> !node.hasClass("userstuff") && node.parents().none { it.id()=="workskin" || it.hasClass("userstuff") } }?.text().orEmpty()
    val error=doc.select("#errorExplanation, #error, .errorExplanation, .flash.comment_error").filter { node -> !node.hasClass("userstuff") && node.parents().none { it.id()=="workskin" || it.hasClass("userstuff") } }.joinToString("\n") { it.text() }
    return if(notice.isNotBlank() || error.isNotBlank()) CommentSubmission(notice,error) else CommentSubmission(error="Could not confirm whether the comment was posted. Refresh comments before trying again.",uncertain=true)
}
