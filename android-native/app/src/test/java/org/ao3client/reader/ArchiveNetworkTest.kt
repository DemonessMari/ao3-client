package org.ao3client.reader

import java.net.HttpURLConnection
import java.net.URL
import java.io.ByteArrayInputStream
import org.junit.Assert.*
import org.junit.Test

class ArchiveNetworkTest {
    private class Reply(url:URL,val code:Int,val html:String,val location:String?=null):HttpURLConnection(url) {
        val written=java.io.ByteArrayOutputStream()
        override fun getOutputStream()=written
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
    @Test fun commentModerationRefetchesPermissionAndNeverRetriesAnUncertainWrite() {
        val managed=javaClass.classLoader!!.getResource("comment-managed.html")!!.readText()
        val calls=mutableListOf<Reply>()
        val response="<nav id='greeting'><a href='/users/Writer'>Writer</a><a href='/users/logout'>Log out</a></nav><div class='flash comment_notice'>Thread frozen.</div>"
        val client=Archive(MemoryPreferences(),connectionFactory={url->Reply(url,200,if(calls.isEmpty())managed else response).also{calls+=it}})
        val result=client.manageComment("55","Freeze thread","Writer")
        assertFalse(result.uncertain);assertEquals("Thread frozen.",result.notice)
        assertEquals(listOf("GET","POST"),calls.map{it.requestMethod})
        assertEquals("/comments/55/freeze",calls.last().url.path)
        assertTrue(calls.last().written.toString("UTF-8").contains("freeze%2Btoken"))
        val unknownCalls=mutableListOf<Reply>()
        val unknown=Archive(MemoryPreferences(),connectionFactory={url->Reply(url,if(unknownCalls.isEmpty())200 else 503,managed).also{unknownCalls+=it}})
        assertTrue(unknown.manageComment("55","Freeze thread","Writer").uncertain)
        assertEquals(2,unknownCalls.size)
        val deniedCalls=mutableListOf<Reply>()
        val denied=Archive(MemoryPreferences(),connectionFactory={url->Reply(url,200,managed.replace("Writer","Other")).also{deniedCalls+=it}})
        assertTrue(runCatching{denied.manageComment("55","Freeze thread","Writer")}.isFailure)
        assertEquals(1,deniedCalls.size)
    }
    @Test fun inboxReadUsesInboxRecordIdRatherThanCommentId() {
        val html=javaClass.classLoader!!.getResource("archive-inbox.html")!!.readText()
        val calls=mutableListOf<Reply>()
        val client=Archive(MemoryPreferences(),connectionFactory={url->Reply(url,200,if(calls.isEmpty())html else html.replace("class=\"unread comment\"","class=\"read comment\"")).also{calls+=it}})
        assertFalse(client.archivePageAction("inbox","Writer","",1,"55","Mark read").entries.single().unread)
        val body=calls.last().written.toString("UTF-8")
        assertTrue(body.contains("inbox_comments%5B%5D=77"));assertFalse(body.contains("inbox_comments%5B%5D=55"))
        assertEquals(listOf("GET","POST"),calls.map{it.requestMethod})
    }
    @Test fun unsubscribeRefetchesTheTokenAndVerifiesTheResultWithoutRetry() {
        val html=javaClass.classLoader!!.getResource("archive-subscriptions.html")!!.readText()
        val empty="<nav id='greeting'><a href='/users/Writer'>Writer</a><a href='/users/logout'>Log out</a></nav><div id='main' class='subscriptions-index'><h2 class='heading'>Subscriptions</h2></div>"
        val calls=mutableListOf<Reply>()
        val client=Archive(MemoryPreferences(),connectionFactory={url->Reply(url,200,if(calls.isEmpty())html else empty).also{calls+=it}})
        assertTrue(client.archivePageAction("subscriptions","Writer","",1,"4","Unsubscribe").entries.isEmpty())
        assertEquals(listOf("GET","POST"),calls.map{it.requestMethod})
        assertTrue(calls.last().written.toString("UTF-8").contains("fresh%2B%26token"))
        assertEquals("https://archiveofourown.org/users/Writer/subscriptions?page=1",calls.last().getRequestProperty("Referer"))
    }
    @Test fun batchInboxUsesRepeatedServerRecordIdsAndPostsOnlyOnce() {
        val fixture=javaClass.classLoader!!.getResource("archive-inbox.html")!!.readText()
        val item=Regex("""<li id="feedback_comment_55"[\s\S]*?</li>""").find(fixture)!!.value
        val html=fixture.replace(item,item+item.replace("feedback_comment_55","feedback_comment_56").replace("value=\"77\"","value=\"78\""))
        val result=html.replace("class=\"unread comment\"","class=\"comment\"")
        val calls=mutableListOf<Reply>();val client=Archive(MemoryPreferences(),connectionFactory={url->Reply(url,200,if(calls.isEmpty())html else result).also{calls+=it}})
        assertEquals(2,client.archiveInboxBatch("Writer",1,setOf("55","56"),"Mark read").entries.size)
        assertEquals(listOf("GET","POST"),calls.map{it.requestMethod})
        val fields=calls.last().written.toString("UTF-8")
        assertTrue(fields.contains("inbox_comments%5B%5D=77"));assertTrue(fields.contains("inbox_comments%5B%5D=78"));assertFalse(fields.contains("inbox_comments%5B%5D=55"))
    }
    @Test fun readingListsUseOnlyAuthenticatedGetRoutesAndKeepLaterWhenPaging() {
        val html=javaClass.classLoader!!.getResource("account-readings.html")!!.readText();val calls=mutableListOf<Reply>()
        val client=Archive(MemoryPreferences(),connectionFactory={url->Reply(url,200,html).also{calls+=it}})
        assertEquals("123",client.accountReadings("Writer",1,true).works.single().id)
        client.accountReadings("Writer",2,true);client.accountReadings("Writer",1,false)
        assertEquals(listOf("/users/Writer/readings?page=1&show=to-read","/users/Writer/readings?page=2&show=to-read","/users/Writer/readings?page=1"),calls.map{it.url.file})
        assertTrue(calls.all{it.requestMethod=="GET" && it.closed})
        assertTrue(runCatching{client.accountReadings("Other",1,false)}.isFailure)
    }
    @Test fun seriesNeighborsOpenTheNativeWorkRouteWithFreshMetadata() {
        val html=javaClass.classLoader!!.getResource("work-series.html")!!.readText()
        val calls=mutableListOf<Reply>()
        val client=Archive(MemoryPreferences(),connectionFactory={url->Reply(url,200,html).also{calls+=it}})
        val next=client.series("202").first().next!!
        val work=client.workDetails(next)
        assertEquals("303",work.id);assertEquals("A work & its sequel",work.title);assertEquals("Writer",work.author)
        assertEquals(listOf("/works/202","/works/303"),calls.map{it.url.path})
        assertTrue(calls.all{it.requestMethod=="GET" && it.closed})
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
