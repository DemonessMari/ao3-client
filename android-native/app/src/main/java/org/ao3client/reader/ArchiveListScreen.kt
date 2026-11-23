package org.ao3client.reader
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@Composable internal fun ArchiveListScreen(vm:ClientState) {
    var confirm by remember{mutableStateOf<Pair<ArchiveEntry,String>?>(null)}
    val page=vm.archiveList
    LazyColumn(Modifier.fillMaxSize(),contentPadding=PaddingValues(16.dp),verticalArrangement=Arrangement.spacedBy(16.dp)) {
        item {SkinOutlinedButton(onClick={vm.showArchiveList(vm.archiveKind,vm.archiveId,page?.page ?: 1)},enabled=!vm.loading){Text("Refresh")}}
        if(page!=null){
            item{Text(page.title,style=MaterialTheme.typography.headlineSmall)}
            if(page.description.isNotBlank())item{SelectionContainer{Text(page.description)}}
            if(vm.archiveKind=="series")item{EntitySubscriptionButton(vm,"Series",vm.archiveId)}
            if(page.entries.isEmpty())item{Text("No entries.")}
            items(page.entries,key={it.id}){entry->
                Column(Modifier.fillMaxWidth(),verticalArrangement=Arrangement.spacedBy(8.dp)) {
                    Text(entry.title,style=MaterialTheme.typography.titleMedium)
                    if(entry.unread)Text("Unread",style=MaterialTheme.typography.labelMedium)
                    if(entry.body.isNotBlank())SelectionContainer{Text(entry.body)}
                    if(entry.target.isNotEmpty())SkinOutlinedButton(onClick={vm.openArchiveEntry(entry.target)},enabled=!vm.loading){Text(if(entry.target.startsWith("/comments/"))"Open thread" else if(entry.target.startsWith("/bookmarks/"))"Open on AO3" else "Open")}
                    entry.actions.forEach{action->SkinTextButton(onClick={if(action in listOf("Unsubscribe","Delete from inbox"))confirm=entry to action else vm.actOnArchiveEntry(entry,action)},enabled=!vm.loading && !vm.archiveUncertain){Text(action)}}
                    HorizontalDivider()
                }
            }
            item {Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.SpaceBetween){SkinTextButton(onClick={vm.showArchiveList(vm.archiveKind,vm.archiveId,page.page-1)},enabled=page.page>1 && !vm.loading){Text("Previous")};Text("${page.page} / ${page.pages}");SkinTextButton(onClick={vm.showArchiveList(vm.archiveKind,vm.archiveId,page.page+1)},enabled=page.page<page.pages && !vm.loading){Text("Next")}}}
        }
    }
    confirm?.let{(entry,action)->AlertDialog(onDismissRequest={confirm=null},title={Text("$action?")},text={Text(entry.title)},confirmButton={SkinTextButton(onClick={confirm=null;vm.actOnArchiveEntry(entry,action)}){Text(action)}},dismissButton={SkinTextButton(onClick={confirm=null}){Text("Cancel")}})}
}

@Composable internal fun EntitySubscriptionButton(vm:ClientState,type:String,target:String) {
    val key="$type:$target";val state=vm.entitySubscriptions[key] ?: return
    SkinOutlinedButton(onClick={vm.setEntitySubscription(type,target,!state)},enabled=vm.entitySubscriptionPending[key]!=true){Text(if(state)"Unsubscribe" else if(type=="User")"Subscribe to author" else "Subscribe to series")}
}
