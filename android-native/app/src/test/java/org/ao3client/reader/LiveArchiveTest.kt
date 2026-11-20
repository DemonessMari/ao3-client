package org.ao3client.reader
import org.junit.Test
import org.junit.Assert.*
import org.junit.Assume.assumeTrue

class LiveArchiveTest {
    @Test fun distinctLiveSearchesAndReadableWork() {
        assumeTrue("Set AO3_LIVE_TEST=1 for live AO3 requests", System.getenv("AO3_LIVE_TEST") == "1")
        val client = Archive(MemoryPreferences())
        val first = client.search("sherlock", 1, emptyMap())
        val second = client.search("harry potter", 1, emptyMap())
        assertTrue(first.works.isNotEmpty()); assertTrue(second.works.isNotEmpty())
        assertNotEquals(first.works.map { it.id }, second.works.map { it.id })
        val chapters = client.read(first.works.first().id).first
        assertTrue(chapters.isNotEmpty()); assertTrue(chapters.all { it.text.isNotBlank() })
        println("Live native networking: ${first.works.size} and ${second.works.size} distinct-query results; ${chapters.size} readable chapter(s).")
    }
}
