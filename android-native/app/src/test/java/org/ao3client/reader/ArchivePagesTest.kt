package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
class ArchivePagesTest {
    private fun fixture(name:String)=javaClass.classLoader!!.getResource("archive-$name.html")!!.readText()
    @Test fun historyActionsUseReadingRecordIdsAndFreshOwnedForms() {
        val html=fixture("history");val page=parseArchivePage(html,"history","Writer","",1)
        assertEquals(listOf("81","82"),page.entries.map{it.id});assertEquals("/works/123",page.entries.first().target);assertEquals("",page.entries[1].target)
        assertEquals(listOf("Delete from history"),page.entries[1].actions)
        val action=pageAction(html,"history","Writer","",1,"81","Delete from history")
        assertEquals("/users/Writer/readings/81",java.net.URI(action.path).path);assertEquals("81",action.fields["reading"]);assertEquals("history+token",action.fields["authenticity_token"])
        assertTrue(runCatching{pageAction(html,"history","Other","",1,"81","Delete from history")}.isFailure)
        assertTrue(runCatching{pageAction(html,"history","Writer","",1,"999","Delete from history")}.isFailure)
        assertTrue(runCatching{pageAction(html.replace("name=\"reading\" value=\"81\"","name=\"reading\" value=\"999\""),"history","Writer","",1,"81","Delete from history")}.isFailure)
        val clear=javaClass.classLoader!!.getResource("history-clear.html")!!.readText()
        assertEquals(mapOf("authenticity_token" to "clear+token"),pageAction(clear,"history","Writer","",1,"","Clear history").fields)
        assertTrue(runCatching{pageAction(clear.replace("action=\"/users/Writer","action=\"https://evil.example/users/Writer"),"history","Writer","",1,"","Clear history")}.isFailure)
        assertTrue(runCatching{pageAction("<blockquote class='userstuff'>$clear</blockquote>","history","Writer","",1,"","Clear history")}.isFailure)
    }
    @Test fun orderedSeriesDoesNotRequireASession() {
        val page=parseArchivePage(fixture("series"),"series","","42",1)
        assertEquals(listOf("/works/20","/works/10"),page.entries.map{it.target})
    }
    @Test fun subscriptionsKeepTypesAndRequireFreshOwnedForms() {
        val html=fixture("subscriptions");val page=parseArchivePage(html,"subscriptions","Writer","",1)
        assertEquals(listOf("/works/123","/series/42","/users/Author"),page.entries.map{it.target});assertEquals(2,page.pages)
        val action=pageAction(html,"subscriptions","Writer","",1,"4","Unsubscribe")
        assertEquals("fresh+&token",action.fields["authenticity_token"]);assertEquals("delete",action.fields["_method"])
        assertTrue(runCatching{parseArchivePage(html,"subscriptions","Other","",1)}.isFailure)
        assertTrue(runCatching{pageAction(html.replace("/users/Writer/subscriptions/4","https://evil.example/users/Writer/subscriptions/4"),"subscriptions","Writer","",1,"4","Unsubscribe")}.isFailure)
    }
    @Test fun inboxUsesInboxRecordIdRatherThanCommentId() {
        val html=fixture("inbox");val page=parseArchivePage(html,"inbox","Writer","",1)
        assertEquals("55",page.entries.single().id);assertTrue(page.entries.single().unread)
        val action=pageAction(html,"inbox","Writer","",1,"55","Mark read")
        assertEquals("77",action.fields["inbox_comments[]"]);assertEquals("put",action.fields["_method"])
        assertEquals("/users/Writer/inbox?page=1",action.path)
    }
    @Test fun inboxCommentMarkupCannotSelectAnotherInboxRecordOrToken() {
        val html=fixture("inbox").replace("<p>A comment &amp; a reply.</p>","<p>A comment.</p><input name='inbox_comments[]' value='999'><input type='hidden' name='authenticity_token' value='fake-token'>")
        val action=pageAction(html,"inbox","Writer","",1,"55","Mark read")
        assertEquals("77",action.fields["inbox_comments[]"])
        assertEquals("inbox+token",action.fields["authenticity_token"])
    }
}
