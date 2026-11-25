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
    SkinOutlinedButton(onClick={vm.showTools("/works/$id")},modifier=Modifier.fillMaxWidth()){Text("Work tools")}
    SkinOutlinedButton(onClick={vm.showTools("/works/$id/collections")},modifier=Modifier.fillMaxWidth()){Text("Collections")}
    Text("AO3 website",style=MaterialTheme.typography.titleMedium)
    SkinOutlinedButton(onClick={vm.openWebsite(Archive.BASE+"/works/$id")},modifier=Modifier.fillMaxWidth()) {Text("Original formatting and work skin")}
    SkinOutlinedButton(onClick={vm.openWebsite(Archive.BASE+"/works/$id?show_comments=true#comments")},modifier=Modifier.fillMaxWidth()) {Text("Comment management")}
}

@Composable internal fun NativeAccountLinks(vm:ClientState) {
    listOf("Import a work" to "/works/new?import=true","Account overview" to "/users/${vm.username}","Statistics" to "/users/${vm.username}/stats","Co-creator requests" to "/users/${vm.username}/creatorships","Create collection" to "/collections/new","Tag sets and nominations" to "/users/${vm.username}/tag_sets","Post a work" to "/works/new","Drafts" to "/users/${vm.username}/works/drafts","Manage works" to "/users/${vm.username}/works/show_multiple","Series" to "/users/${vm.username}/series","Collections" to "/users/${vm.username}/collections","Browse collections and challenges" to "/collections","Gifts" to "/users/${vm.username}/gifts","Related works" to "/users/${vm.username}/related_works","Assignments" to "/users/${vm.username}/assignments","Claims" to "/users/${vm.username}/claims","Challenge sign-ups" to "/users/${vm.username}/signups","Invitations" to "/users/${vm.username}/invitations","Profile pictures and pseuds" to "/users/${vm.username}/pseuds","Site and work skins" to "/users/${vm.username}/skins","Change password" to "/users/${vm.username}/change_password","Change email" to "/users/${vm.username}/change_email","Change username" to "/users/${vm.username}/change_username","Manage AO3 bookmarks" to "/users/${vm.username}/bookmarks").forEach{(label,path)->SkinTextButton(onClick={vm.showTools(path)}){Text(label)}}
    SkinTextButton(onClick={vm.showAccountEditor("profile")}){Text("Edit profile")}
    SkinTextButton(onClick={vm.showAccountEditor("preferences")}){Text("AO3 preferences")}
    listOf("pseuds" to "Pseuds","blocked" to "Blocked users","muted" to "Muted users").forEach{(kind,title)->SkinTextButton(onClick={vm.showArchiveList(kind)}){Text(title)}}
}
