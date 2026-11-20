package org.ao3client.reader

import org.junit.Test
import org.junit.Rule
import org.junit.rules.TemporaryFolder
import org.junit.Assert.*
import java.io.File

class ReadingLibraryTest {
    @get:Rule val directory = TemporaryFolder()
    private val fic = Fic("123", "Title 世界", "Author", summary = "Summary")
    @Test fun offlineCopySurvivesNewInstanceAndKeepsOriginalChapters() {
        val root = directory.newFolder(); val library = ReadingLibrary(root)
        val chapters = listOf(Chapter("First", "Bonjour 世界 😀"), Chapter("Second", "Original text"))
        library.saveOffline(fic, chapters)
        val reopened = ReadingLibrary(root)
        assertEquals(chapters, reopened.load("123"))
        assertEquals(fic, reopened.entries().single().fic)
        assertTrue(reopened.entries().single().offline)
    }
    @Test fun historyDoesNotDuplicateAndFavoritesAndBookmarksSurviveDownloadRemoval() {
        val library = ReadingLibrary(directory.newFolder())
        library.saveOffline(fic, listOf(Chapter("One", "Body")))
        library.favorite(fic.id, true); library.addBookmark(fic.id, "Return here", 0, 8, 12, 765)
        library.remember(fic.copy(title = "Updated title")); library.removeOffline(fic.id)
        val entry = library.entries().single()
        assertEquals("Updated title", entry.fic.title); assertTrue(entry.favorite); assertFalse(entry.offline)
        assertNull(library.load(fic.id))
        val bookmark = library.bookmarks(fic.id).single()
        assertEquals("Return here", bookmark.title); assertEquals(8, bookmark.index); assertEquals(12, bookmark.offset); assertEquals(765, bookmark.bookOffset)
        library.removeBookmark(fic.id, bookmark.id); assertTrue(library.bookmarks(fic.id).isEmpty())
    }
    @Test(expected = IllegalArgumentException::class) fun cacheIdentifiersCannotEscapeLibrary() { ReadingLibrary(directory.newFolder()).load("../outside") }
    @Test(expected = Exception::class) fun corruptSavedCopyIsNotReturnedAsValidContent() {
        val root = directory.newFolder(); File(root, "123.json").writeText("damaged")
        ReadingLibrary(root).load("123")
    }
}
