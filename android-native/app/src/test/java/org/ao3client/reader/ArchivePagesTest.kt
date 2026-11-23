package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
class ArchivePagesTest {
    private fun fixture(name:String)=javaClass.classLoader!!.getResource("archive-$name.html")!!.readText()
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
