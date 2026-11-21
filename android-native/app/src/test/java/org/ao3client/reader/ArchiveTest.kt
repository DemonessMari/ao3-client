package org.ao3client.reader

import org.junit.Assert.*
import org.junit.Test
import java.net.URLDecoder

class ArchiveTest {
    @Test fun realSearchRouteAndArrayFilters() {
        val path = Archive.searchPath("alpha & beta", 3, mapOf("archive_warning_ids[]" to listOf("16", "18"), "rating_ids" to listOf("13"), "excluded_rating_ids[]" to listOf("12"), "excluded_category_ids[]" to listOf("23")))
        assertTrue(path.startsWith("/works/search?"))
        val decoded = URLDecoder.decode(path, "UTF-8")
        assertTrue(decoded.contains("work_search[query]=alpha & beta"))
        assertTrue(decoded.contains("work_search[archive_warning_ids][]=16"))
        assertTrue(decoded.contains("work_search[archive_warning_ids][]=18"))
        assertTrue(decoded.contains("work_search[rating_ids]=13"))
        assertTrue(decoded.contains("page=3"))
        assertTrue(decoded.contains("work_search[excluded_tag_ids][]=12"))
        assertTrue(decoded.contains("work_search[excluded_tag_ids][]=23"))
        assertFalse(decoded.contains("work_search[excluded_rating_ids]"))
    }
    @Test fun listingWithNestedSummaryAndPagination() {
        val html = """<h3 class="heading">1,234 Works Found</h3><ol class="work index group"><li class="work blurb group" id="work_123"><h4 class="heading"><a href="/works/123">Title &amp; Story</a><a rel="author">Writer</a></h4><ul class="fandoms"><li><a class="tag">Fandom</a></li></ul><blockquote class="summary"><p>First.</p><ul><li>Nested summary</li></ul></blockquote><dl class="stats"><dt>Words:</dt><dd class="words">1,234</dd></dl></li></ol><ol class="pagination"><li><a href="/works/search?page=8">8</a></li></ol>"""
        val result = ArchiveParser.listing(html, 1)
        assertEquals(1, result.works.size); assertEquals("123", result.works[0].id)
        assertEquals("Title & Story", result.works[0].title); assertEquals("Writer", result.works[0].author)
        assertTrue(result.works[0].summary.contains("Nested summary")); assertEquals(8, result.pages)
    }
    @Test fun singleAndMultipleChaptersExcludeNotes() {
        val single = """<div id="chapters"><div class="preface"><div class="userstuff">Do not include summary</div></div><h3 class="landmark">Work Text:</h3><div class="userstuff"><p>Hello &amp; 世界</p><h3>Story heading</h3><p>Second paragraph.</p></div><div class="afterword"><div class="userstuff">Do not include notes</div></div></div>"""
        val result = ArchiveParser.chapters(single)
        assertEquals(1, result.size); assertTrue(result[0].text.contains("Hello & 世界"))
        assertTrue(result[0].text.contains("Story heading")); assertTrue(result[0].text.contains("\n"))
        assertFalse(result[0].text.contains("Do not include")); assertFalse(result[0].text.contains("Work Text:"))
        val multi = """<div id="chapters"><div class="chapter"><h3 class="title">First</h3><div class="userstuff module"><p>One</p></div></div><div class="chapter"><h3 class="title">Second</h3><div class="userstuff module"><p>Two</p></div></div></div>"""
        assertEquals(listOf("First", "Second"), ArchiveParser.chapters(multi).map { it.title })
    }
    @Test fun fullWorkKeepsFiveDistinctChaptersAndTitles() {
        val html = "<div id='chapters'>" + (1..5).joinToString("") { i ->
            """<div id="chapter-$i" class="chapter"><div class="chapter preface group"><h3 class="title">Chapter $i: Title $i</h3><div class="summary module"><blockquote class="userstuff">Summary $i</blockquote></div></div><div class="userstuff module" role="article"><h3 class="landmark">Chapter Text</h3><p>Unique story $i.</p></div><div class="end notes module"><blockquote class="userstuff">Notes $i</blockquote></div></div>"""
        } + "</div>"
        val chapters = ArchiveParser.chapters(html)
        assertEquals(5, chapters.size)
        chapters.forEachIndexed { i, chapter ->
            assertEquals("Chapter ${i+1}: Title ${i+1}", chapter.title)
            assertEquals("Unique story ${i+1}.", chapter.text.trim())
        }
    }
    @Test(expected = IllegalStateException::class) fun maintenanceIsNotEmptySuccess() { ArchiveParser.listing("<h2>The archive is down for maintenance.</h2>", 1) }
    @Test fun emptySearchIsValid() { assertTrue(ArchiveParser.listing("<h2>Search Results</h2><p>No results found.</p>", 1).works.isEmpty()) }
    @Test fun sessionNeedsConfirmedAccount() {
        assertNull(ArchiveParser.username("<a href='/users/Somebody'>Somebody</a>"))
        assertEquals("Somebody", ArchiveParser.username("<div id='greeting'><a href='/users/Somebody'>Hi!</a><a href='/users/logout'>Log out</a></div>"))
    }
    @Test fun rememberedAccountSurvivesRestartAndClearsOnLogout() {
        val prefs = MemoryPreferences()
        prefs.edit().putString("sessionUsername", "Somebody").putString("cookies", """[{"value":"_session_id=test; Path=/; Secure; Domain=archiveofourown.org"}]""").apply()
        val archive = Archive(prefs)
        assertEquals("Somebody", archive.rememberedUsername())
        assertEquals("Somebody", Archive(prefs).rememberedUsername())
        archive.logout()
        assertNull(Archive(prefs).rememberedUsername())
        assertFalse(prefs.contains("sessionUsername"))
    }
    @Test fun accountWithoutCookiesIsNotRestored() {
        val prefs = MemoryPreferences()
        prefs.edit().putString("sessionUsername", "Somebody").apply()
        assertNull(Archive(prefs).rememberedUsername())
    }
    @Test fun logoutFormConfirmsSession() {
        assertEquals("Somebody", ArchiveParser.sessionIdentity("<div id='greeting'><a href='/users/Somebody'>Account</a><form action='/users/logout'></form></div>"))
        assertNull(ArchiveParser.sessionIdentity("<a href='/users/login'>Sign in</a>"))
    }
    @Test(expected = IllegalStateException::class) fun maintenanceDoesNotConfirmLogout() {
        ArchiveParser.sessionIdentity("<h1>Maintenance</h1>")
    }
    @Test fun tagSuggestionsReplaceOnlyTheTokenAtTheCaret() {
        val result = insertTag("First, sh, Last", 9, "Sherlock Holmes")
        assertEquals("First, Sherlock Holmes, Last", result.text)
        assertEquals(result.text.indexOf(", Last"), result.cursor)
        assertEquals("First", activeTagToken("First, Last", 2).term)
        assertEquals("", activeTagToken("", 0).term)
    }
    @Test fun translationDoesNotSplitUnicodeOrLoseSource() {
        val original = "a".repeat(1799) + "😀" + " test ".repeat(600)
        val chunks = translationChunks(original)
        assertEquals(original, chunks.joinToString(""))
        assertTrue(chunks.all { it.length <= 1800 && !it.last().isHighSurrogate() && !it.first().isLowSurrogate() })
    }
    @Test fun filterCatalogueIsAvailableWithoutNetwork() {
        assertEquals(5, Catalogue.ratings.size); assertEquals(6, Catalogue.warnings.size)
        assertTrue(Catalogue.fields.any { it.key == "excluded_tag_names" })
        assertTrue(Catalogue.fields.any { it.key == "bookmarks_count" })
    }
    @Test fun publicProfilesAndBookmarkWorkIds() {
        val profile = ArchiveParser.profile("""<div class="user home profile"><h3 class="heading">Writer's profile</h3><dl class="meta"><dt>Joined:</dt><dd>2020</dd></dl><div class="bio module"><blockquote class="userstuff"><p>Hello 世界</p></blockquote></div></div>""", "Writer")
        assertEquals("Writer", profile.username)
        assertTrue(profile.details.contains("2020"))
        assertEquals("Hello 世界", profile.biography)
        val listing = ArchiveParser.listing("""<ol class="bookmark index group"><li id="bookmark_999" class="bookmark blurb group"><h4 class="heading"><a href="/works/123">A fic</a> by <a rel="author" href="/users/Writer/pseuds/Writer">Writer</a></h4></li></ol>""", 1)
        assertEquals("123", listing.works.single().id)
        assertEquals("/users/Writer/pseuds/Writer", listing.works.single().authorPath)
        assertTrue(ArchiveParser.listing("<ol class='bookmark index group'></ol>", 1).works.isEmpty())
    }
    @Test(expected = IllegalStateException::class) fun unavailableProfileIsNotEmptySuccess() {
        ArchiveParser.profile("<h2>Maintenance</h2>", "Writer")
    }
}
