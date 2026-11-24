package org.ao3client.reader

import androidx.compose.runtime.Composable
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.ui.Modifier

@Composable fun Ao3AccountLinks(vm:ClientState) {
    Text("AO3 website",style=MaterialTheme.typography.titleMedium)
    Text("Website sign-in is separate.",style=MaterialTheme.typography.bodySmall)
    listOf("Profile pictures" to "pseuds","AO3 bookmarks" to "bookmarks","Subscriptions" to "subscriptions","Reading history" to "readings","Marked for later" to "readings?show=to-read","Inbox" to "inbox","Skins" to "skins","Preferences" to "preferences","Drafts" to "works/drafts").forEach { (title,path) ->
        SkinTextButton(onClick={vm.openWebsite(if(vm.username.isNullOrBlank()) Archive.BASE+"/users/login" else Archive.BASE+"/users/${Archive.encode(vm.username.orEmpty())}/$path")}) { Text(title) }
    }
    SkinTextButton(onClick={vm.openWebsite(Archive.BASE+"/")}) {Text("Browse AO3")}
}

@Composable fun Ao3WorkLinks(vm:ClientState,id:String) {
    Text("AO3 website",style=MaterialTheme.typography.titleMedium)
    SkinOutlinedButton(onClick={vm.openWebsite(Archive.BASE+"/works/$id")},modifier=Modifier.fillMaxWidth()) {Text("Original formatting and work skin")}
    SkinOutlinedButton(onClick={vm.openWebsite(Archive.BASE+"/works/$id?show_comments=true#comments")},modifier=Modifier.fillMaxWidth()) {Text("Comment management")}
}

@Composable internal fun NativeAccountLinks(vm:ClientState) {
    SkinTextButton(onClick={vm.showAccountEditor("profile")}){Text("Edit profile")}
    SkinTextButton(onClick={vm.showAccountEditor("preferences")}){Text("AO3 preferences")}
    listOf("pseuds" to "Pseuds","blocked" to "Blocked users","muted" to "Muted users").forEach{(kind,title)->SkinTextButton(onClick={vm.showArchiveList(kind)}){Text(title)}}
}
