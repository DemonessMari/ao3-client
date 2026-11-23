package org.ao3client.reader

import org.jsoup.Jsoup
import java.net.URI
import androidx.compose.runtime.*
import androidx.compose.material3.*

data class ReadingAction(val action:String,val fields:Map<String,String>)
object ReadingActions {
    fun form(html:String,id:String):ReadingAction? {
        for(form in Jsoup.parse(html).select(".mark form")) {
            if(form.parents().any { it.id()=="workskin" || it.hasClass("userstuff") })continue
            val url=runCatching { URI(Archive.BASE).resolve(form.attr("action")) }.getOrNull() ?: continue
            if(!form.attr("method").equals("post",true) || url.scheme!="https" || url.host!="archiveofourown.org" || url.userInfo!=null || url.port !in listOf(-1,443) || url.rawQuery!=null || url.rawFragment!=null)continue
            val action=listOf("mark_for_later","mark_as_read").firstOrNull { url.path=="/works/$id/$it" } ?: continue
            val fields=form.select("input[type=hidden][name]:not([disabled])").filter { it.attr("name") in setOf("_method","authenticity_token") }.associate { it.attr("name") to it.attr("value") }
            if(fields["_method"]=="patch" && !fields["authenticity_token"].isNullOrBlank())return ReadingAction(action,fields)
        }
        return null
    }
}

@Composable
fun WorkAccountButton(vm:ClientState,id:String) {
    var choosing by remember(id) { mutableStateOf(false) }
    if(vm.username!=null)SkinOutlinedButton(onClick={choosing=true},enabled=id !in vm.accountPending) { Text("AO3 account") }
    if(choosing)AlertDialog(onDismissRequest={choosing=false},title={Text("AO3 account")},text={
        androidx.compose.foundation.layout.Column {
            SkinTextButton(onClick={choosing=false;vm.editArchiveBookmark(id)}) { Text("AO3 bookmark") }
            SkinTextButton(onClick={choosing=false;vm.setMarkedForLater(id,true)}) { Text("Mark for later") }
            SkinTextButton(onClick={choosing=false;vm.setMarkedForLater(id,false)}) { Text("Mark as read") }
            SkinTextButton(onClick={choosing=false;vm.setWorkSubscribed(id,true)}) { Text("Subscribe to work") }
            SkinTextButton(onClick={choosing=false;vm.setWorkSubscribed(id,false)}) { Text("Unsubscribe from work") }
        }
    },confirmButton={SkinTextButton(onClick={choosing=false}) { Text("Cancel") }})
}
