package org.ao3client.reader

import org.junit.Test
import org.junit.Assert.*
import org.json.JSONObject
import org.json.JSONArray
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.font.FontStyle

class RichTextTest {
    private fun chapter():Chapter {
        val html=javaClass.classLoader!!.getResourceAsStream("work-formatting.html")!!.bufferedReader().use { it.readText() }
        return ArchiveParser.chapters(html).single()
    }
    @Test fun nestedEmphasisAndLinksKeepTheirTextRanges() {
        val chapter=chapter(); val rich=chapter.styledText("778899")
        assertEquals(chapter.text,rich.text); assertFalse(chapter.text.contains("unwanted script text"))
        val index=chapter.text.indexOf("bolditalic")
        assertTrue(rich.spanStyles.any { index in it.start until it.end && it.item.fontWeight==FontWeight.Bold })
        assertTrue(rich.spanStyles.any { index in it.start until it.end && it.item.fontStyle==FontStyle.Italic })
        val urls=rich.getLinkAnnotations(0,rich.length).map { (it.item as androidx.compose.ui.text.LinkAnnotation.Url).url }
        assertEquals(listOf("https://archiveofourown.org/works/123","https://example.org/story","https://archiveofourown.org/works/778899?view_full_work=true#footnote"),urls)
        assertNull(readingLink("javascript:alert(1)")); assertNull(readingLink("file:///etc/passwd")); assertNull(readingLink("https://user:password@example.org/"))
    }
    @Test fun stylingSurvivesSerializationAndRejectsInvalidRanges() {
        val chapter=chapter(); assertEquals(chapter,Chapter.fromJson(chapter.json()))
        val json=chapter.json().put("styles",JSONArray().put(JSONObject().put("start",-1).put("end",4)).put(JSONObject().put("start",0).put("end",chapter.text.length+1)))
        assertTrue(Chapter.fromJson(json).styles.isEmpty())
    }
    @Test fun paginationReceivesExactStyledOffsetsAcrossPageBoundaries() {
        val chapter=chapter(); val rich=chapter.styledText("778899"); val offsets=mutableListOf<Int>()
        val pages=paginateBookRanges(chapter.text) { chunk,offset ->
            offsets+=offset
            assertEquals(chunk,rich.subSequence(offset,offset+chunk.length).text)
            minOf(11,chunk.length)
        }
        assertEquals(chapter.text,pages.joinToString("") { it.text })
        assertEquals(pages.map { it.offset },offsets)
        assertTrue(pages.any { page -> rich.subSequence(page.offset,page.offset+page.text.length).spanStyles.isNotEmpty() })
    }
}
