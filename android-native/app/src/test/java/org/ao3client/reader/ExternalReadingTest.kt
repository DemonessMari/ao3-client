package org.ao3client.reader

import org.junit.Assert.*
import org.junit.Test
import java.net.HttpURLConnection
import java.net.URI
import java.net.InetAddress
import java.io.ByteArrayInputStream

class ExternalReadingTest {
    private class Response(uri:URI,val code:Int,val mime:String,val bytes:ByteArray,val location:String?=null):HttpURLConnection(uri.toURL()) {
        var closed=false
        override fun connect(){}
        override fun disconnect(){closed=true}
        override fun usingProxy()=false
        override fun getResponseCode()=code
        override fun getContentType()=mime
        override fun getContentLengthLong()=bytes.size.toLong()
        override fun getHeaderField(name:String)=when(name){"Location"->location;"Content-Range"->"bytes 0-3/100";"Set-Cookie"->"must-not-forward";else->null}
        override fun getInputStream()=ByteArrayInputStream(bytes)
    }
    @Test fun mediaStreamsRangesWithoutCookiesAndClosesConnections() {
        val calls=mutableListOf<Response>();val client=ExternalReading({uri->Response(uri,206,"audio/mpeg",byteArrayOf(0,1,2,3)).also{calls+=it}},{arrayOf(InetAddress.getByAddress(byteArrayOf(1,1,1,1)))})
        val resource=client.open("https://media.example/audio.mp3","bytes=0-3")
        assertEquals(206,resource.status);assertEquals("",calls.single().getRequestProperty("Cookie"));assertEquals("bytes=0-3",calls.single().getRequestProperty("Range"));assertFalse(resource.headers.containsKey("Set-Cookie"))
        assertEquals(4,resource.stream.readBytes().size);resource.stream.close();assertTrue(calls.single().closed);client.close()
    }
    @Test fun privateRedirectsNeverReachTheNetworkAndReaderCloseCancelsStreams() {
        val calls=mutableListOf<Response>();val client=ExternalReading({uri->Response(uri,302,"text/plain",byteArrayOf(),"https://10.0.0.1/private").also{calls+=it}},{host->arrayOf(InetAddress.getByAddress(if(host=="10.0.0.1")byteArrayOf(10,0,0,1) else byteArrayOf(1,1,1,1)))})
        assertTrue(runCatching{client.open("https://media.example/redirect")}.isFailure);assertEquals(1,calls.size);assertTrue(calls.single().closed)
        val live=mutableListOf<Response>();val streaming=ExternalReading({uri->Response(uri,200,"video/mp4",byteArrayOf(1)).also{live+=it}},{arrayOf(InetAddress.getByAddress(byteArrayOf(1,1,1,1)))})
        streaming.open("https://media.example/video.mp4");streaming.close();assertTrue(live.single().closed);assertTrue(runCatching{streaming.open("https://media.example/video.mp4")}.isFailure)
        listOf("127.0.0.1","10.0.0.1","192.168.0.1","100.64.0.1","::1","fc00::1").forEach{assertFalse(it,ExternalReading.publicAddress(InetAddress.getByName(it)))}
    }
    @Test fun mediaOnlyWorksRemainReadableAndKeepTheirOriginalContent() {
        for(body in listOf("<audio src='https://media.example/a.mp3'></audio>","<iframe src='https://player.example/embed'></iframe>","<img src='https://media.example/a.png'>")) {
            val chapters=ArchiveParser.chapters("<div id='workskin'><div id='chapters'><div class='userstuff'>$body</div></div></div>")
            assertEquals(1,chapters.size);assertTrue(chapters.first().text.isNotBlank());assertTrue(chapters.first().html.isNotBlank())
        }
    }
    @Test fun remoteReadingIsOptInAndCannotEnableScriptsOrAutoplay() {
        val html="<p>img-src data:;</p><audio src='https://media.example/audio.mp3' autoplay></audio><video><source src='https://media.example/video.mp4' type='video/mp4'></video><iframe src='https://player.example/embed'></iframe><script>bad()</script>"
        val chapter=Chapter("Media","Text",html=staticReadingHtml(html),workCss="@import url(https://styles.example/work.css);.a{background:url(/images/a.png)}")
        assertTrue(chapter.html.contains("<audio"));assertFalse(chapter.html.contains("autoplay"));assertFalse(chapter.html.contains("<iframe"));assertTrue(chapter.html.contains("Open embedded player"));assertTrue(java.net.URI("https://player.example/embed") in readingEmbedTargets(chapter.html));assertTrue(readingEmbedTargets("<a data-ao3-embed='1' href='https://archiveofourown.org/users/Writer'>Never a player</a>").isEmpty())
        val offline=formattedDocument(chapter,"#ffffff","#000000",18,1.6,24);assertFalse(offline.contains("<audio"));assertTrue(offline.contains("Audio/video requires external content."))
        val online=formattedDocument(chapter,"#ffffff","#000000",18,1.6,24,external=true);assertTrue(online.contains("media-src data: https:"));assertTrue(online.contains("frame-src 'none'"));assertFalse(online.contains("<script"));assertTrue(online.contains("<audio"));assertTrue(online.contains("<p>img-src data:;</p>"));assertTrue(online.contains("https://archiveofourown.org/images/a.png"))
    }
}
