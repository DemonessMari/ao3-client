package org.ao3client.reader

import java.net.HttpURLConnection
import java.net.URL
import java.io.ByteArrayInputStream
import org.junit.Assert.*
import org.junit.Test

class ArchiveNetworkTest {
    private class Reply(url:URL,val code:Int,val html:String,val location:String?=null):HttpURLConnection(url) {
        var closed=false
        override fun connect() {}
        override fun disconnect() {closed=true}
        override fun usingProxy()=false
        override fun getResponseCode()=code
        override fun getInputStream()=ByteArrayInputStream(html.toByteArray())
        override fun getErrorStream()=ByteArrayInputStream(html.toByteArray())
        override fun getHeaderFields():Map<String,List<String>> = emptyMap()
        override fun getHeaderField(name:String)=if(name=="Location")location else null
    }
    @Test fun readingUsesTheListingWorkIdAndFollowsChapterRedirects() {
        val calls=mutableListOf<Reply>()
        val client=Archive(MemoryPreferences(),connectionFactory={url->Reply(url,if(calls.isEmpty())302 else 200,"<div id='chapters'><div class='userstuff'><p>Actual chapter content.</p></div></div>","/works/123/chapters/456").also {calls+=it}})
        assertEquals("Actual chapter content.",client.read("123").first.single().text)
        assertEquals("/works/123?view_adult=true&view_full_work=true",calls[0].url.file)
        assertEquals("/works/123/chapters/456",calls[1].url.path)
        assertTrue(calls.all {it.closed})
    }
    @Test fun unavailableWorkActionsExposeStatusAndRouteWithoutServerBodyOrCredentials() {
        val calls=mutableListOf<Reply>()
        val client=Archive(MemoryPreferences(),connectionFactory={url->Reply(url,404,"secret server response").also {calls+=it}})
        val actions=listOf<()->Unit>({client.read("123")},{client.downloads("123")},{client.comments("123",1)},{client.archiveBookmark("123","Writer")})
        actions.forEach {action->val failure=runCatching(action).exceptionOrNull();assertNotNull(failure);val message=failure!!.message.orEmpty();assertTrue(message.contains("HTTP 404"));assertTrue(message.contains("/works/123"));assertFalse(message.contains("secret"));assertFalse(message.contains("view_adult"))}
        assertEquals(4,calls.size);assertTrue(calls.all {it.closed})
    }
    @Test fun aWork404CanBeDistinguishedFromAnExpiredNativeSession() {
        val prefs=MemoryPreferences();prefs.edit().putString("sessionUsername","Writer").putString("cookies","""[{"value":"_session_id=test; Path=/; Secure; Domain=archiveofourown.org"}]""").apply()
        val calls=mutableListOf<Reply>();val client=Archive(prefs,connectionFactory={url->Reply(url,if(url.path=="/")200 else 404,"<div id='greeting'><a href='/users/Writer'>Hi</a><a href='/users/logout'>Log out</a></div>").also{calls+=it}})
        assertTrue(runCatching{client.read("123")}.isFailure);assertEquals("Writer",client.session());assertEquals(listOf("/works/123","/"),calls.map{it.url.path});assertTrue(calls.all{it.getRequestProperty("Cookie").orEmpty().contains("_session_id=test")})
    }
    @Test fun ambiguousSessionResponseKeepsStoredIdentityAndCookies() {
        val prefs=MemoryPreferences()
        prefs.edit().putString("sessionUsername","Writer").putString("cookies","""[{"value":"_session_id=test; Path=/; Secure; Domain=archiveofourown.org"}]""").apply()
        var calls=0
        val client=Archive(prefs,connectionFactory={url->calls++;Reply(url,200,"<title>Just a moment...</title><form id='challenge-form'></form>")})
        val failure=runCatching {client.session()}.exceptionOrNull()
        assertTrue(failure?.message.orEmpty().contains("browser verification"))
        assertEquals(1,calls)
        assertEquals("Writer",client.rememberedUsername())
        assertEquals("Writer",Archive(prefs).rememberedUsername())
        assertTrue(prefs.getString("cookies",null).orEmpty().contains("_session_id=test"))
    }
    @Test fun rateLimitStopsWithoutRetryingOrClearingRememberedIdentity() {
        val prefs=MemoryPreferences();prefs.edit().putString("sessionUsername","Writer").putString("cookies","""[{"value":"_session_id=test; Path=/; Secure; Domain=archiveofourown.org"}]""").apply();var calls=0
        val client=Archive(prefs,connectionFactory={url->calls++;Reply(url,429,"ignored")})
        assertEquals("Writer",client.rememberedUsername())
        val failure=runCatching {client.read("123")}.exceptionOrNull()!!
        assertTrue(failure.message.orEmpty().contains("HTTP 429"));assertEquals(1,calls);assertEquals("Writer",client.rememberedUsername());assertEquals("Writer",prefs.getString("sessionUsername",null))
    }
}
