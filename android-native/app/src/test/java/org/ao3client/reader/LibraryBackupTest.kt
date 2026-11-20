package org.ao3client.reader
import org.junit.Test
import org.junit.Rule
import org.junit.Assert.*
import org.junit.rules.TemporaryFolder
import org.json.JSONObject
import java.io.File

class LibraryBackupTest {
    @get:Rule val directory=TemporaryFolder()
    private fun fixture()=JSONObject(javaClass.classLoader!!.getResourceAsStream("library-backup-v1.json")!!.bufferedReader().use { it.readText() })
    @Test fun desktopFixtureImportsAndRepeatedMergesKeepOneBookmark() {
        val library=ReadingLibrary(directory.newFolder()); val prefs=MemoryPreferences()
        prefs.edit().putString("cookies","PRIVATE_COOKIE").putString("apiKey","PRIVATE_KEY").apply()
        LibraryBackup.merge(fixture(),library,prefs); LibraryBackup.merge(fixture(),library,prefs)
        assertEquals(1,library.entries().size); assertTrue(library.entries().single().favorite)
        assertEquals(2,library.load("987654321")!!.size); assertEquals(1,library.bookmarks("987654321").size)
        assertEquals(1,prefs.getInt("progress.987654321.chapter",-1))
        assertEquals(.25f,prefs.getFloat("progress.987654321.1.fraction",0f),.001f)
        val snapshot=LibraryBackup.snapshot(library,prefs).toString()
        assertFalse(snapshot.contains("PRIVATE_COOKIE")); assertFalse(snapshot.contains("PRIVATE_KEY")); assertFalse(snapshot.contains("cookies")); assertFalse(snapshot.contains("apiKey"))
        val restored=ReadingLibrary(directory.newFolder()); LibraryBackup.merge(JSONObject(snapshot),restored,MemoryPreferences())
        assertEquals(library.load("987654321"),restored.load("987654321"))
    }
    @Test fun compactBackupKeepsMetadataBookmarksAndProgressWithoutDownloads() {
        val library=ReadingLibrary(directory.newFolder()); val prefs=MemoryPreferences(); LibraryBackup.merge(fixture(),library,prefs)
        val compact=LibraryBackup.snapshot(library,prefs,includeDownloads=false); assertEquals(0,compact.getJSONArray("works").getJSONObject(0).getJSONArray("chapters").length())
        val restored=ReadingLibrary(directory.newFolder()); LibraryBackup.merge(compact,restored,MemoryPreferences())
        assertNull(restored.load("987654321")); assertTrue(restored.entries().single().favorite); assertEquals(1,restored.bookmarks("987654321").size)
    }
    @Test fun badBackupDoesNotModifyExistingLibrary() {
        val library=ReadingLibrary(directory.newFolder()); library.remember(Fic("123","Keep","Writer"))
        val data=fixture(); data.getJSONArray("works").getJSONObject(0).getJSONObject("work").put("id","../escape")
        assertThrows(IllegalArgumentException::class.java) { LibraryBackup.merge(data,library,MemoryPreferences()) }
        assertEquals("123",library.entries().single().fic.id)
    }
    @Test fun importPreservesExistingProgressAndDownloads() {
        val library=ReadingLibrary(directory.newFolder()); val prefs=MemoryPreferences()
        library.saveOffline(Fic("987654321","Keep","Writer"),listOf(Chapter("Local","Keep local copy")))
        prefs.edit().putInt("progress.987654321.chapter",0).putInt("progress.987654321.0.index",9).apply()
        LibraryBackup.merge(fixture(),library,prefs)
        assertEquals("Keep local copy",library.load("987654321")!!.single().text)
        assertEquals(9,prefs.getInt("progress.987654321.0.index",0)); assertEquals(0,prefs.getInt("progress.987654321.chapter",-1))
    }
    @Test fun damagedIndexIsPreservedAndRecoveredAcrossRepeatedReads() {
        val root=directory.newFolder(); val library=ReadingLibrary(root)
        library.saveOffline(Fic("123","Keep","Writer"),listOf(Chapter("One","Original")))
        File(root,"index.json").writeText("broken")
        assertEquals("123",library.entries().single().fic.id); assertEquals("123",library.entries().single().fic.id)
        assertTrue(root.listFiles()!!.any { it.name.startsWith("index.json.corrupt-") })
    }
    @Test fun translatorRequiresHttpsExceptLocalhost() {
        assertTrue(validTranslationService("https://translator.example")); assertTrue(validTranslationService("http://localhost:5000"))
        assertFalse(validTranslationService("http://translator.example")); assertFalse(validTranslationService("https://user:secret@translator.example")); assertFalse(validTranslationService("file:///tmp/service"))
    }
}
