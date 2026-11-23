package org.ao3client.reader

import org.junit.Assert.*
import org.junit.Test

class SeriesNavigationTest {
    private fun fixture()=javaClass.classLoader!!.getResource("work-series.html")!!.readText()
    @Test fun followsOfficialVisibleNeighborsAndKeepsMultipleMembershipsSeparate() {
        val rows=parseWorkSeries(fixture())
        assertEquals(2,rows.size)
        assertEquals("42",rows[0].id);assertEquals("First & Second",rows[0].title)
        assertEquals("Part 2 of First & Second",rows[0].position)
        assertEquals("101",rows[0].previous);assertEquals("303",rows[0].next)
        assertEquals("43",rows[1].id);assertNull(rows[1].previous);assertNull(rows[1].next)
    }
    @Test fun emptySeriesDiffersFromUnavailableWork() {
        assertTrue(parseWorkSeries("<dl class='work meta group'></dl>").isEmpty())
        assertTrue(runCatching{parseWorkSeries("<h1>Sign in</h1>")}.isFailure)
        assertTrue(runCatching{parseWorkSeries("<div class='userstuff'><dl class='work meta'></dl></div>")}.isFailure)
    }
    @Test fun routesRejectForeignOriginsAndNonWorkPaths() {
        assertNull(archiveNumericId("https://user@archiveofourown.org/works/3","works"))
        assertNull(archiveNumericId("http://archiveofourown.org/works/3","works"))
        assertNull(archiveNumericId("/works/3/chapters/4","works"))
        assertNull(archiveNumericId("/series/3","works"))
    }
}
