package org.ao3client.reader

import java.net.URI
import java.net.HttpURLConnection
import org.jsoup.Jsoup

data class OfficialDownload(val format:String,val url:String) {
    val filename:String get()=URI(url).path.substringAfterLast('/')
}
object OfficialDownloads {
    const val MAX_BYTES=64*1024*1024
    fun format(url:String,workId:String):String? = runCatching {
        val u=URI(url)
        require(workId.matches(Regex("[0-9]+")) && u.scheme=="https" && u.host=="archiveofourown.org" && u.userInfo==null && u.port in listOf(-1,443))
        val prefix="/downloads/$workId/"
        require(u.path.startsWith(prefix) && !u.path.removePrefix(prefix).contains('/'))
        u.path.substringAfterLast('.').lowercase().takeIf { it in listOf("epub","pdf","html","mobi","azw3") }
    }.getOrNull()
    fun parse(html:String,workId:String):List<OfficialDownload> = Jsoup.parse(html,Archive.BASE).select("a[href]").mapNotNull {
        val url=it.absUrl("href");format(url,workId)?.let { f->OfficialDownload(f,url) }
    }.distinctBy { it.format }
    fun validate(data:ByteArray,format:String) {
        require(data.isNotEmpty() && data.size<=MAX_BYTES) { "Download is empty or exceeds 64 MB" }
        if(format=="html") require(Jsoup.parse(data.toString(Charsets.UTF_8)).selectFirst("div#chapters")!=null) { "AO3 did not return an HTML download" }
        if(format=="pdf") require(data.take(5).toByteArray().toString(Charsets.US_ASCII)=="%PDF-") { "Invalid PDF download" }
        if(format=="epub") require(data.size>=4 && data[0]==80.toByte() && data[1]==75.toByte()) { "Invalid EPUB download" }
        if(format in listOf("mobi","azw3")) require(data.size>=68 && data.copyOfRange(60,68).toString(Charsets.US_ASCII)=="BOOKMOBI") { "Invalid ebook download" }
    }
    fun fetch(item:OfficialDownload,workId:String,cookie:String?=null,userAgent:String?=null):ByteArray {
        require(format(item.url,workId)==item.format) { "Invalid AO3 download link" }
        val connection=URI(item.url).toURL().openConnection() as HttpURLConnection
        try {
            connection.instanceFollowRedirects=false
            if(!cookie.isNullOrBlank())connection.setRequestProperty("Cookie",cookie)
            connection.connectTimeout=15000;connection.readTimeout=60000
            connection.setRequestProperty("User-Agent",userAgent ?: "AO3Client (Unofficial Android Reader)")
            val status=connection.responseCode
            require(status!=429) { "AO3 rate limit reached (HTTP 429). Wait before trying again." }
            require(status!=404) { "AO3 could not find this download (HTTP 404)." }
            require(status==200) { "AO3 did not return the requested file (HTTP $status). Try again later." }
            require(connection.contentLengthLong<=MAX_BYTES) { "Download exceeds 64 MB" }
            require(item.format=="html" || !connection.contentType.orEmpty().contains("text/html")) { "AO3 returned a web page instead of the download" }
            val data=connection.inputStream.use { input ->
                val out=java.io.ByteArrayOutputStream();val buffer=ByteArray(16384)
                while(true) { val count=input.read(buffer);if(count<0)break;require(out.size()+count<=MAX_BYTES) { "Download exceeds 64 MB" };out.write(buffer,0,count) }
                out.toByteArray()
            }
            validate(data,item.format)
            return data
        } finally { connection.disconnect() }
    }
}
