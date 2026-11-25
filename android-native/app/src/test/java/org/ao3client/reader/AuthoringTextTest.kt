package org.ao3client.reader

import android.text.SpannableStringBuilder
import org.junit.Test
import org.junit.Assert.*
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[34],manifest=Config.NONE)
class AuthoringTextTest {
    @Test fun paragraphsLinksAndInlineFormattingExportAsAo3Html() {
        val html="<h2>Title</h2>\n<p>A <strong>bold &amp; safe</strong> word<br>next <a href='https://example.org/?a=1&amp;b=2'>link</a></p><ol><li>First</li><li>Second</li></ol>"
        assertTrue(AuthoringText.supported(html));val text=AuthoringText.parse(html);val out=AuthoringText.html(text)
        assertTrue(out,out.contains("<h2>Title</h2>"));assertTrue(out,out.contains("<strong>bold &amp; safe</strong>"));assertTrue(out,out.contains("<br>next"));assertTrue(out,out.contains("href=\"https://example.org/?a=1&amp;b=2\""));assertTrue(out,out.contains("<ol><li>First</li><li>Second</li></ol>"));assertFalse(out.contains("style="));assertEquals(out,AuthoringText.html(AuthoringText.parse(out)))
    }
    @Test fun partialFormattingRemovalPreservesTheRestAndUndoRetainsSpans() {
        val text=AuthoringText.parse("<p><strong>abcdef</strong></p>");val history=AuthorHistory(text)
        AuthoringText.toggle(text,2,4,"Bold");history.record(text)
        assertEquals("<p><strong>ab</strong>cd<strong>ef</strong></p>",AuthoringText.html(text))
        assertEquals("<p><strong>abcdef</strong></p>",AuthoringText.html(history.undo()!!));assertEquals(AuthoringText.html(text),AuthoringText.html(history.redo()!!))
        val adjacent=AuthoringText.parse("<p><strong>ab</strong><strong>cd</strong></p>");AuthoringText.toggle(adjacent,0,4,"Bold");assertEquals("<p>abcd</p>",AuthoringText.html(adjacent))
    }
    @Test fun paragraphToolsAndBlankLinesSurviveSerialization() {
        val text=SpannableStringBuilder("First\nSecond\n\nLast");AuthoringText.paragraph(text,0,12,"Numbered")
        val html=AuthoringText.html(text);assertTrue(html,html.startsWith("<ol><li>First</li><li>Second</li></ol>"));assertTrue(html,html.contains("<p><br></p>"))
        AuthoringText.paragraph(text,0,0,"Heading");assertTrue(AuthoringText.html(text).startsWith("<h2>First</h2>"))
    }
    @Test fun linkChangesPreserveUnselectedText(){val text=AuthoringText.parse("<p><a href='https://example.org'>abcdef</a></p>");AuthoringText.link(text,2,4,"https://other.example");val out=AuthoringText.html(text);assertTrue(out,out.contains("href=\"https://example.org\">ab</a>"));assertTrue(out,out.contains("href=\"https://other.example\">cd</a>"));assertTrue(out,out.contains("href=\"https://example.org\">ef</a>"))}
    @Test fun complexHtmlStaysInSourceModeRatherThanBeingFlattened() {
        listOf("<table><tr><td>Keep</td></tr></table>","<p style='color:red'>Keep</p>","<img src='https://example.org/a.png'>","<ul><li>Outer<ul><li>Inner</li></ul></li></ul>").forEach{assertFalse(it,AuthoringText.supported(it))}
        assertTrue(AuthoringText.field("work[chapter_attributes][content]"));assertFalse(AuthoringText.field("skin[css]"))
    }
}
