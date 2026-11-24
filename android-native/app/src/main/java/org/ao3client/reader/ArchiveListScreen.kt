package org.ao3client.reader
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@Composable internal fun ArchiveListScreen(vm:ClientState) {
    var accountTarget by remember(vm.archiveKind){mutableStateOf("")}
    var clearHistory by remember{mutableStateOf(false)}
    var confirm by remember{mutableStateOf<Pair<ArchiveEntry,String>?>(null)}
    val page=vm.archiveList
    val listState=rememberLazyListState()
    LaunchedEffect(vm.archiveKind,page,vm.discoveryEditing){if(vm.archiveKind.startsWith("search:"))listState.scrollToItem(0)}
    var filter by remember(vm.archiveKind,vm.archiveId){mutableStateOf("")}
    var selected by remember(page){mutableStateOf(setOf<String>())}
    var batchConfirm by remember{mutableStateOf<Pair<Set<String>,String>?>(null)}
    val visible=page?.entries.orEmpty().filter{filter.isBlank() || (it.title+"\n"+it.body).contains(filter,true)}
    LazyColumn(Modifier.fillMaxSize(),state=listState,contentPadding=PaddingValues(16.dp),verticalArrangement=Arrangement.spacedBy(16.dp)) {
        if(vm.archiveKind.startsWith("search:")) {
            val kind=vm.archiveKind.removePrefix("search:")
            item{Text(DiscoverySearch.title(kind),style=MaterialTheme.typography.headlineSmall)}
            item{SkinTextButton(onClick={vm.discoveryEditing=!vm.discoveryEditing}){Text(if(vm.discoveryEditing)"Hide search options" else "Edit search")}}
            if(vm.discoveryEditing) {
                items(DiscoverySearch.fields(kind),key={"search-field-${it.key}"}){field->
                    FilterControl(field,vm.discoveryDraft[kind]?.get(field.key)?.takeIf{it.isNotBlank()}?.let{listOf(it)}.orEmpty(),{values->vm.discoveryDraft=vm.discoveryDraft+(kind to (vm.discoveryDraft[kind].orEmpty()+(field.key to values.firstOrNull().orEmpty())))},vm)
                }
                item{Row(horizontalArrangement=Arrangement.spacedBy(12.dp)){SkinButton(onClick=vm::submitDiscovery,enabled=!vm.loading){Text("Search")};SkinTextButton(onClick={vm.discoveryDraft=vm.discoveryDraft-kind}){Text("Reset options")}}}
            }
        }
        if(page?.page==0)return@LazyColumn
        item {SkinOutlinedButton(onClick={vm.showArchiveList(vm.archiveKind,vm.archiveId,page?.page ?: 1)},enabled=!vm.loading){Text("Refresh")}}
        if(page!=null){
            if(!vm.archiveKind.startsWith("search:"))item{Text(page.title,style=MaterialTheme.typography.headlineSmall)}
            item{SkinOutlinedTextField(filter,{filter=it},label={Text("Filter this page")},singleLine=true,modifier=Modifier.fillMaxWidth())}
            if(vm.archiveKind in listOf("pseuds","bookmarks","series"))item{SkinOutlinedButton(onClick={vm.showAccountEditor(if(vm.archiveKind=="pseuds")"pseud" else if(vm.archiveKind=="bookmarks")"externalBookmark" else "seriesBookmark",if(vm.archiveKind=="series")vm.archiveId else "")}){Text(if(vm.archiveKind=="pseuds")"New pseud" else if(vm.archiveKind=="bookmarks")"New external bookmark" else "New series bookmark")}}
            if(vm.archiveKind in listOf("blocked","muted"))item{Column(verticalArrangement=Arrangement.spacedBy(8.dp)){SkinOutlinedTextField(accountTarget,{accountTarget=it},label={Text("AO3 username")},singleLine=true,modifier=Modifier.fillMaxWidth());SkinOutlinedButton(onClick={vm.showAccountEditor(if(vm.archiveKind=="blocked")"block" else "mute",accountTarget.trim())}){Text(if(vm.archiveKind=="blocked")"Block user" else "Mute user")}}}
            if(vm.archiveKind=="history")item{SkinOutlinedButton(onClick={clearHistory=true},enabled=page.entries.isNotEmpty() && !vm.loading && !vm.archiveUncertain){Text("Clear entire AO3 history")}}
            if(vm.archiveKind=="inbox")item {
                Column {
                    Row {SkinTextButton(onClick={selected=visible.filter{it.selection.isNotEmpty()}.map{it.id}.toSet()}){Text("Select visible")};SkinTextButton(onClick={selected=emptySet()}){Text("Clear selection")}}
                    Text("${selected.size} selected")
                    listOf("Mark read","Mark unread","Delete from inbox").forEach {label->
                        val eligible=page.entries.filter{it.id in selected && label in it.actions}.map{it.id}.toSet()
                        SkinTextButton(onClick={if(label=="Delete from inbox")batchConfirm=eligible to label else vm.actOnInboxSelection(eligible,label)},enabled=eligible.isNotEmpty() && !vm.loading && !vm.archiveUncertain){Text("$label (${eligible.size})")}
                    }
                }
            }
            if(page.description.isNotBlank())item{SelectionContainer{Text(page.description)}}
            if(vm.archiveKind=="series")item{EntitySubscriptionButton(vm,"Series",vm.archiveId)}
            if(page.entries.isEmpty())item{Text("No entries.")}
            if(visible.isEmpty() && page.entries.isNotEmpty())item{Text("No matching entries on this page.")}
            items(visible,key={it.id}){entry->
                Column(Modifier.fillMaxWidth(),verticalArrangement=Arrangement.spacedBy(8.dp)) {
                    if(vm.archiveKind=="inbox" && entry.selection.isNotEmpty())Row {Checkbox(entry.id in selected,{checked->selected=if(checked)selected+entry.id else selected-entry.id});Text("Select comment",Modifier.padding(top=12.dp))}
                    Text(entry.title,style=MaterialTheme.typography.titleMedium)
                    if(entry.unread)Text("Unread",style=MaterialTheme.typography.labelMedium)
                    if(entry.body.isNotBlank() && entry.body!=entry.title)SelectionContainer{Text(entry.body)}
                    if(entry.target.isNotEmpty())SkinOutlinedButton(onClick={vm.openArchiveEntry(entry.target)},enabled=!vm.loading){Text(if(entry.target.startsWith("/comments/"))"Open thread" else if(entry.target.startsWith("/bookmarks/"))"Open on AO3" else "Open")}
                    entry.actions.forEach{action->SkinTextButton(onClick={when(action){"Edit pseud"->vm.showAccountEditor("pseud",entry.id);"Edit bookmark"->vm.showAccountEditor("bookmark",entry.id.removePrefix("bookmark_"));"Unblock","Unmute"->vm.showAccountEditor(action.lowercase(),entry.id);else->if(action in listOf("Unsubscribe","Delete from inbox","Delete from history"))confirm=entry to action else vm.actOnArchiveEntry(entry,action)}},enabled=!vm.loading && !vm.archiveUncertain){Text(action)}}
                    HorizontalDivider()
                }
            }
            item {Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.SpaceBetween){SkinTextButton(onClick={vm.showArchiveList(vm.archiveKind,vm.archiveId,page.page-1)},enabled=page.page>1 && !vm.loading){Text("Previous")};Text("${page.page} / ${page.pages}");SkinTextButton(onClick={vm.showArchiveList(vm.archiveKind,vm.archiveId,page.page+1)},enabled=page.page<page.pages && !vm.loading){Text("Next")}}}
        }
    }
    if(clearHistory)AlertDialog(onDismissRequest={clearHistory=false},title={Text("Clear AO3 history?")},text={Text("Permanently erase your AO3 reading history and Marked for Later list?")},confirmButton={SkinTextButton(onClick={clearHistory=false;vm.actOnArchiveEntry(ArchiveEntry("","",""),"Clear history")}){Text("Clear history")}},dismissButton={SkinTextButton(onClick={clearHistory=false}){Text("Cancel")}})
    batchConfirm?.let{(ids,label)->AlertDialog(onDismissRequest={batchConfirm=null},title={Text("$label?")},text={Text("${ids.size} selected comments will be removed from your inbox. The comments themselves remain on AO3.")},confirmButton={SkinTextButton(onClick={batchConfirm=null;vm.actOnInboxSelection(ids,label)}){Text(label)}},dismissButton={SkinTextButton(onClick={batchConfirm=null}){Text("Cancel")}})}
    confirm?.let{(entry,action)->AlertDialog(onDismissRequest={confirm=null},title={Text("$action?")},text={Text(entry.title+if(action=="Delete from history")"\n\nRemove this entry from your AO3 history and Marked for Later list?" else "")},confirmButton={SkinTextButton(onClick={confirm=null;vm.actOnArchiveEntry(entry,action)}){Text(action)}},dismissButton={SkinTextButton(onClick={confirm=null}){Text("Cancel")}})}
}

@Composable internal fun EntitySubscriptionButton(vm:ClientState,type:String,target:String) {
    val key="$type:$target";val state=vm.entitySubscriptions[key] ?: return
    SkinOutlinedButton(onClick={vm.setEntitySubscription(type,target,!state)},enabled=vm.entitySubscriptionPending[key]!=true){Text(if(state)"Unsubscribe" else if(type=="User")"Subscribe to author" else "Subscribe to series")}
}
