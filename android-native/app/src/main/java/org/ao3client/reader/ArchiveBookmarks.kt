package org.ao3client.reader

import org.jsoup.Jsoup
import org.jsoup.nodes.Element
import java.net.URI
import androidx.compose.runtime.*
import androidx.compose.material3.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.ui.Alignment
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.toggleable
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

data class ArchiveBookmarkForm(val workId:String,val id:String,val path:String,val token:String,val notes:String,val tags:String,val collections:String,val pseud:String,val pseuds:List<Pair<String,String>>,val privateBookmark:Boolean,val recommendation:Boolean)
data class ArchiveFormResult(val message:String,val success:Boolean=false,val uncertain:Boolean=false)
object ArchiveBookmarks {
    private fun story(n:Element)=n.parents().any {it.id()=="workskin" || it.hasClass("userstuff")} || n.hasClass("userstuff")
    private fun path(action:String):String? {
        val u=runCatching {URI(Archive.BASE).resolve(action)}.getOrNull() ?: return null
        return u.path.takeIf {u.scheme=="https" && u.host=="archiveofourown.org" && u.userInfo==null && u.port in listOf(-1,443) && u.rawQuery==null && u.rawFragment==null}
    }
    fun form(html:String,workId:String):ArchiveBookmarkForm {
        val form=Jsoup.parse(html).select("#feedback #bookmark-form form").firstOrNull { !story(it) && it.attr("method").equals("post",true) } ?: error("AO3 bookmarking is unavailable. Check your sign-in and work permissions.")
        val path=path(form.attr("action")) ?: error("Invalid bookmark form")
        val id=Regex("/bookmarks/([0-9]+)").matchEntire(path)?.groupValues?.get(1).orEmpty()
        require(id.isNotEmpty() || path=="/works/$workId/bookmarks") {"Invalid bookmark target"}
        fun input(name:String)=form.getElementsByAttributeValue("name",name).filterNot {it.hasAttr("disabled")}
        fun value(name:String)=input(name).firstOrNull()?.attr("value").orEmpty()
        val method=value("_method")
        require(if(id.isEmpty()) input("_method").isEmpty() else method=="put") {"Invalid bookmark method"}
        val pseudSelect=input("bookmark[pseud_id]").firstOrNull {it.tagName()=="select"}
        val pseuds=pseudSelect?.select("option")?.map {it.attr("value") to it.text()} ?: listOf(value("bookmark[pseud_id]") to value("bookmark[pseud_id]"))
        val pseud=pseudSelect?.selectFirst("option[selected]")?.attr("value") ?: pseuds.firstOrNull()?.first.orEmpty()
        val token=value("authenticity_token")
        require(token.isNotBlank() && pseud.isNotBlank()) {"Invalid bookmark form"}
        fun checked(name:String)=input(name).any {it.attr("type")=="checkbox" && it.hasAttr("checked")}
        return ArchiveBookmarkForm(workId,id,path,token,input("bookmark[bookmarker_notes]").firstOrNull()?.`val`().orEmpty(),value("bookmark[tag_string]"),value("bookmark[collection_names]"),pseud,pseuds,checked("bookmark[private]"),checked("bookmark[rec]"))
    }
    fun fields(fresh:ArchiveBookmarkForm,draft:ArchiveBookmarkForm):Map<String,String> {
        val fields=linkedMapOf("authenticity_token" to fresh.token,"bookmark[pseud_id]" to draft.pseud,"bookmark[bookmarker_notes]" to draft.notes,"bookmark[tag_string]" to draft.tags,"bookmark[collection_names]" to draft.collections,"bookmark[private]" to if(draft.privateBookmark) "1" else "0","bookmark[rec]" to if(draft.recommendation) "1" else "0")
        if(fresh.id.isNotEmpty())fields["_method"]="put"
        return fields
    }
    fun deleteForm(html:String,id:String):Map<String,String> {
        val form=Jsoup.parse(html).select("form.destroy").firstOrNull {!story(it) && it.attr("method").equals("post",true) && path(it.attr("action"))=="/bookmarks/$id"} ?: error("Invalid bookmark delete form")
        val fields=form.select("input[type=hidden][name]:not([disabled])").filter {it.attr("name") in setOf("authenticity_token","_method")}.associate {it.attr("name") to it.attr("value")}
        require(fields["_method"]=="delete" && !fields["authenticity_token"].isNullOrBlank()) {"Invalid bookmark delete form"}
        return fields
    }
    fun record(html:String,workId:String,expectedId:String=""):Boolean=Jsoup.parse(html).select("li.bookmark[id^=bookmark_]").any {item ->
        val id=Regex("bookmark_([0-9]+)").matchEntire(item.id())?.groupValues?.get(1).orEmpty()
        if(id.isEmpty() || (expectedId.isNotEmpty() && expectedId!=id))false
        else {
            val links=item.select("a[href]").filterNot {story(it)}.mapNotNull {path(it.attr("href"))}
            "/works/$workId" in links && "/bookmarks/$id/edit" in links
        }
    }
    fun result(html:String):ArchiveFormResult {
        val doc=Jsoup.parse(html)
        val errors=doc.select("#errorExplanation, .errorExplanation, .flash.error").filterNot {story(it)}.joinToString("\n") {it.text()}
        if(errors.isNotBlank())return ArchiveFormResult(errors)
        val notice=doc.select(".flash.notice").firstOrNull {!story(it)}?.text().orEmpty()
        return if(notice.isNotBlank())ArchiveFormResult(notice,true) else ArchiveFormResult("Could not confirm the change. Check your AO3 bookmarks before trying again.",uncertain=true)
    }
}

@Composable
fun ArchiveBookmarkScreen(vm:ClientState) {
    val form=vm.archiveBookmark ?: return
    var reloading by remember(form.workId) {mutableStateOf(false)}
    var deleting by remember(form.workId,form.id) {mutableStateOf(false)}
    fun update(next:ArchiveBookmarkForm) {vm.archiveBookmark=next}
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),verticalArrangement=Arrangement.spacedBy(12.dp)) {
        Text(if(form.id.isEmpty()) "New AO3 bookmark" else "Edit AO3 bookmark",style=MaterialTheme.typography.titleLarge)
        if(form.pseuds.size>1)form.pseuds.forEach { (id,name)->Row(Modifier.fillMaxWidth().selectable(selected=form.pseud==id,enabled=!vm.bookmarkSending,role=Role.RadioButton,onClick={update(form.copy(pseud=id))}),verticalAlignment=Alignment.CenterVertically) {RadioButton(selected=form.pseud==id,onClick=null,enabled=!vm.bookmarkSending);Text(name)} }
        OutlinedTextField(form.notes,{update(form.copy(notes=it))},label={Text("Notes (HTML allowed)")},minLines=4,modifier=Modifier.fillMaxWidth(),enabled=!vm.bookmarkSending)
        OutlinedTextField(form.tags,{update(form.copy(tags=it))},label={Text("Tags, comma separated")},modifier=Modifier.fillMaxWidth(),enabled=!vm.bookmarkSending)
        OutlinedTextField(form.collections,{update(form.copy(collections=it))},label={Text("Collections, comma separated")},modifier=Modifier.fillMaxWidth(),enabled=!vm.bookmarkSending)
        Row(Modifier.fillMaxWidth().toggleable(value=form.privateBookmark,enabled=!vm.bookmarkSending,role=Role.Checkbox,onValueChange={update(form.copy(privateBookmark=it))}),verticalAlignment=Alignment.CenterVertically) {Checkbox(form.privateBookmark,null,enabled=!vm.bookmarkSending);Text("Private bookmark")}
        Row(Modifier.fillMaxWidth().toggleable(value=form.recommendation,enabled=!vm.bookmarkSending,role=Role.Checkbox,onValueChange={update(form.copy(recommendation=it))}),verticalAlignment=Alignment.CenterVertically) {Checkbox(form.recommendation,null,enabled=!vm.bookmarkSending);Text("Recommendation")}
        SkinButton(onClick={vm.saveArchiveBookmark()},enabled=!vm.bookmarkSending && !vm.bookmarkUncertain) {Text(if(vm.bookmarkSending) "Saving…" else "Save to AO3")}
        if(form.id.isNotBlank())SkinOutlinedButton(onClick={deleting=true},enabled=!vm.bookmarkSending && !vm.bookmarkUncertain) {Text("Delete AO3 bookmark")}
        SkinTextButton(onClick={reloading=true},enabled=!vm.bookmarkSending) {Text("Reload from AO3")}
    }
    if(reloading)AlertDialog(onDismissRequest={reloading=false},title={Text("Reload bookmark?")},text={Text("Replace this draft with the bookmark saved on AO3?")},confirmButton={SkinTextButton(onClick={reloading=false;vm.editArchiveBookmark(form.workId,true)}) {Text("Reload")}},dismissButton={SkinTextButton(onClick={reloading=false}) {Text("Cancel")}})
    if(deleting)AlertDialog(onDismissRequest={deleting=false},title={Text("Delete AO3 bookmark?")},text={Text("This deletes your account bookmark, including its notes and tags.")},confirmButton={SkinTextButton(onClick={deleting=false;vm.saveArchiveBookmark(true)}) {Text("Delete")}},dismissButton={SkinTextButton(onClick={deleting=false}) {Text("Cancel")}})
}
