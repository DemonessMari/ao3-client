package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
class CommentsTest {
    @Test fun formsAndSubmissionResults() {
        val html=javaClass.classLoader!!.getResource("comment-form.html")!!.readText()
        val form=parseCommentForm(html)
        assertEquals("csrf+&=token",form.hidden["authenticity_token"]);assertEquals("12",form.hidden["comment[pseud_id]"])
        assertEquals(2,form.pseuds.size);assertTrue(form.notice.contains("moderated"));assertFalse(form.guest)
        assertFalse(validCommentAction("https://example.invalid/comments"));assertFalse(validCommentAction("//example.invalid/comments"))
        assertTrue(parseCommentSubmission("<div class='flash comment_notice'>Comment awaiting moderation.</div>").notice.contains("moderation"))
        assertFalse(parseCommentSubmission("<div id='errorExplanation'>Comment is too long.</div>").uncertain)
        assertTrue(parseCommentSubmission("<h1>Maintenance</h1>").uncertain)
    }
    @Test(expected=IllegalStateException::class) fun externalFormRejected() {
        parseCommentForm(javaClass.classLoader!!.getResource("comment-form.html")!!.readText().replace("/works/123/comments","https://example.invalid/comments"))
    }

    @Test fun threadsPaginationAndPrivacy() {
        val html=javaClass.classLoader!!.getResource("comments-page.html")!!.readText()
        val page=CommentParser.parse(html)
        assertEquals(3,page.comments.size);assertEquals(4,page.pages)
        val first=page.comments[0];assertEquals("101",first.id);assertEquals("Pen",first.author)
        assertTrue(first.text.contains("Hello & 世界."));assertTrue(first.text.contains("A quoted line."))
        assertFalse(first.text.contains("private@example.invalid"));assertFalse(first.text.contains("192.0.2.1"))
        assertEquals(1,page.comments[1].depth);assertEquals("Guest",page.comments[1].author)
        assertEquals("Reply text.",page.comments[1].text)
        assertTrue(page.comments[2].text.contains("deleted"))
    }
    @Test(expected=IllegalStateException::class) fun maintenanceIsNotNoComments() {CommentParser.parse("<h1>Maintenance</h1>")}
    @Test fun emptyCommentsPageIsValid() {assertTrue(CommentParser.parse("<div id='comments_placeholder'></div>").comments.isEmpty())}
}
