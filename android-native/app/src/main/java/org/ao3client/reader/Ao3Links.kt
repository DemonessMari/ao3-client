package org.ao3client.reader

import androidx.compose.runtime.Composable
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.unit.dp

@Composable fun Ao3AccountLinks(user:String?) {
    val browser=LocalUriHandler.current
    Text("AO3 account (browser)",style=MaterialTheme.typography.titleMedium)
    listOf("AO3 bookmarks" to "bookmarks","Subscriptions" to "subscriptions","Reading history" to "readings","Preferences" to "preferences").forEach { (title,path) ->
        TextButton(onClick={browser.openUri(if(user.isNullOrBlank()) "https://archiveofourown.org/users/login" else "https://archiveofourown.org/users/${Archive.encode(user)}/$path")}) { Text(title) }
    }
}

@Composable fun Ao3WorkLinks(id:String) {
    val browser=LocalUriHandler.current
    Text("AO3 (browser)",style=MaterialTheme.typography.titleMedium)
    Text("Browser sign-in is separate.",style=MaterialTheme.typography.bodySmall)
    OutlinedButton(onClick={browser.openUri("https://archiveofourown.org/works/$id")},modifier=Modifier.fillMaxWidth()) { Text("Open work") }
    OutlinedButton(onClick={browser.openUri("https://archiveofourown.org/works/$id?show_comments=true#comments")},modifier=Modifier.fillMaxWidth()) { Text("Comments") }
}
