package org.ao3client.reader

import org.junit.Assert.*
import org.junit.Test
import java.net.URLDecoder

class DiscoverySearchTest {
    private fun fixture(kind:String)=javaClass.classLoader!!.getResource("discovery-$kind.html")!!.readText()
    @Test fun peopleUseTheAuthorHeadingAndIgnoreBiographyControls() {
        val page=DiscoverySearch.parse(fixture("people"),"people",1)
        assertEquals(1,page.entries.size);assertEquals("Storyteller (Writer)",page.entries.single().title)
        assertEquals("/users/Writer/pseuds/Storyteller",page.entries.single().target)
        assertTrue(page.entries.single().body.contains("Stories & poetry."));assertEquals(3,page.pages)
    }
    @Test fun tagsProduceAnEncodedNativeWorkFilter() {
        val page=DiscoverySearch.parse(fixture("tags"),"tags",1)
        assertEquals("Friends & Family",page.entries.first().title)
        assertEquals("/works/search?work_search[freeform_names]=Friends & Family",URLDecoder.decode(page.entries.first().target,"UTF-8"))
        assertEquals("",page.entries[1].target);assertEquals(2,page.pages)
    }
    @Test fun bookmarksKeepSeparateNotesForTheSameWorkAndPreserveOtherItemTypes() {
        val page=DiscoverySearch.parse(fixture("bookmarks"),"bookmarks",1)
        assertEquals(5,page.entries.size);assertEquals(listOf("bookmark_41","bookmark_42"),page.entries.take(2).map{it.id})
        assertEquals(listOf("/works/123","/works/123","/series/20","/bookmarks/44","/bookmarks/45"),page.entries.map{it.target})
        assertEquals("A Work by Author",page.entries.first().title);assertTrue(page.entries.first().body.contains("First reader’s notes."));assertTrue(page.entries[1].body.contains("Different notes."));assertEquals(4,page.pages)
    }
    @Test fun queryParametersCannotBecomeAnotherSearchOrPrivateScope() {
        val path=DiscoverySearch.path("bookmarks",mapOf("bookmark_query" to "a & b/+","show_private" to "1","bookmarker" to "Writer"),2)
        val args=path.substringAfter('?').split('&').associate{val parts=it.split('=',limit=2);URLDecoder.decode(parts[0],"UTF-8") to URLDecoder.decode(parts[1],"UTF-8")}
        assertEquals("a & b/+",args["bookmark_search[bookmark_query]"]);assertEquals("2",args["page"])
        assertFalse(args.containsKey("bookmark_search[show_private]"));assertTrue(args.containsKey("bookmark_search[bookmarkable_query]"))
        assertTrue(runCatching{DiscoverySearch.path("../users",emptyMap(),1)}.isFailure)
    }
    @Test fun emptyResultsAreDifferentFromAnUnavailableSite() {
        assertTrue(DiscoverySearch.parse("<div id='main' class='bookmarks-search'><h2>Search Results</h2><p>No results found.</p></div>","bookmarks",1).entries.isEmpty())
        assertTrue(DiscoverySearch.parse("<div id='main' class='people-search'><ol class='pseud index'></ol></div>","people",1).entries.isEmpty())
        for(html in listOf("<h1>Service unavailable</h1>","<div id='main' class='tags-search'><p>Tag search</p></div>","<blockquote class='userstuff'>${fixture("people")}</blockquote>"))assertTrue(runCatching{DiscoverySearch.parse(html,"people",1)}.isFailure)
    }
}
