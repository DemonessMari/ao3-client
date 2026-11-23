package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
class AccountReadingsTest {
    private fun fixture()=javaClass.classLoader!!.getResource("account-readings.html")!!.readText()
    private val header="<nav id='greeting'><a href='/users/Writer'>Writer</a><a href='/users/logout'>Log out</a></nav>"
    @Test fun historyPreservesAvailableAndDeletedEntriesAndScopedPagination() {
        val result=parseAccountReadings(fixture(),"Writer",1)
        assertEquals("123",result.works.single().id);assertEquals("Saved story",result.works.single().title)
        assertTrue(result.works.single().readingInfo.contains("Update available"));assertEquals(2,result.pages)
        assertTrue(result.unavailable.single().contains("Deleted work"))
    }
    @Test fun emptyHistoryIsRecognizedWithoutPretendingPreferencesOrLoginAreEmpty() {
        assertTrue(parseAccountReadings(header+"<div id='main' class='readings-index'></div>","Writer",1).works.isEmpty())
        listOf(header+"<div id='main' class='preferences-edit'></div>","<a href='/users/login?return_to=%2F'>Log in</a>",header+"<div class='userstuff'><div id='main' class='readings-index'></div></div>").forEach {
            assertTrue(runCatching{parseAccountReadings(it,"Writer",1)}.isFailure)
        }
        assertTrue(runCatching{parseAccountReadings(fixture(),"Other",1)}.isFailure)
    }
}
