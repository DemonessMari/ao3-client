package org.ao3client.reader
import org.jsoup.Jsoup
import java.net.URI

data class KudosResult(val message:String,val success:Boolean=false,val uncertain:Boolean=false)
object KudosParser {
    fun form(html:String,id:String):Map<String,String> {
        val form=Jsoup.parse(html).select("#feedback form#new_kudo").firstOrNull { f->f.parents().none { it.id()=="workskin" || it.hasClass("userstuff") } } ?: error("Kudos are unavailable for this work")
        val url=URI(Archive.BASE).resolve(form.attr("action"))
        require(form.attr("method").equals("post",true) && url.scheme=="https" && url.host=="archiveofourown.org" && url.userInfo==null && url.port in listOf(-1,443) && url.path=="/kudos" && url.rawQuery==null && url.rawFragment==null) { "Invalid kudos form" }
        val names=setOf("authenticity_token","kudo[commentable_id]","kudo[commentable_type]")
        val fields=form.select("input[type=hidden][name]:not([disabled])").filter { it.attr("name") in names }.associate { it.attr("name") to it.attr("value") }
        require(fields["kudo[commentable_id]"]==id && fields["kudo[commentable_type]"]=="Work" && !fields["authenticity_token"].isNullOrBlank()) { "Invalid kudos target" }
        return fields
    }
    fun result(html:String):KudosResult {
        val doc=Jsoup.parse(html)
        for(node in doc.select("#feedback .flash.kudos_notice, #feedback .flash.kudos_error")) {
            if(node.hasClass("userstuff") || node.parents().any { it.id()=="workskin" || it.hasClass("userstuff") }) continue
            if(node.text().isNotBlank())return KudosResult(node.text(),node.hasClass("kudos_notice"))
        }
        return KudosResult("Could not confirm kudos. Check this work on AO3.",uncertain=true)
    }
}

@androidx.compose.runtime.Composable
fun KudosButton(vm:ClientState,id:String,menu:Boolean=false) {
 val label=vm.kudosStates[id] ?: if(vm.username==null) "Give guest kudos" else "Give kudos"
 val enabled=!vm.kudosStates.containsKey(id)
 KudosControl(label,enabled,menu){vm.giveKudos(id)}
}

@androidx.compose.runtime.Composable
internal fun KudosControl(label:String,enabled:Boolean,menu:Boolean,onClick:()->Unit) {
 if(menu)MenuRow(label,onClick=onClick,enabled=enabled)
 else SkinOutlinedButton(onClick=onClick,enabled=enabled){androidx.compose.material3.Text(label)}
}
