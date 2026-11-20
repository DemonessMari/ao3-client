package org.ao3client.reader
import java.net.URL
fun validTranslationService(value:String):Boolean = runCatching {
    val url=URL(value); url.host.isNotBlank() && url.userInfo==null && (url.protocol=="https" || (url.protocol=="http" && url.host in listOf("localhost","127.0.0.1","[::1]","::1")))
}.getOrDefault(false)
