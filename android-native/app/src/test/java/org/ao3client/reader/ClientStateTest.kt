@file:OptIn(kotlinx.coroutines.ExperimentalCoroutinesApi::class)
package org.ao3client.reader
import android.app.Application
import android.content.SharedPreferences
import java.io.File
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.*
import kotlinx.coroutines.test.*
import org.junit.*
import org.junit.Assert.*
import org.junit.rules.TemporaryFolder

class ClientStateTest {
    @get:Rule val directory=TemporaryFolder()
    private val models=mutableListOf<ClientState>()
    private class TestApp(val directory:File,val prefs:SharedPreferences):Application() {
        override fun getSharedPreferences(name:String,mode:Int)=prefs
        override fun getFilesDir()=directory
    }
    @Before fun setup() { Dispatchers.setMain(UnconfinedTestDispatcher()) }
    @After fun cleanup() { models.forEach { it.viewModelScope.cancel() }; Dispatchers.resetMain() }
    private fun state(app:TestApp)=ClientState(app,checkSession=false).also { models+=it }
    private fun awaitLoaded(vm:ClientState)=runBlocking { withTimeout(5000) { while(vm.loading) delay(10) } }
    @Test fun androidFactoryCanFindTheApplicationConstructor() {
        assertNotNull(ClientState::class.java.getConstructor(Application::class.java))
    }
    @Test fun importedBookmarkCanJumpInScrollingModeWithoutAnOfflineCopy() {
        val vm=state(TestApp(directory.newFolder(),MemoryPreferences())); vm.selected=Fic("123","Title","Writer"); vm.chapters=listOf(Chapter("One","First paragraph.\n\nSecond paragraph.\n\nThird paragraph.")); vm.screen="reader"
        vm.jumpBookmark(ReadingBookmark("id","Imported",0,0,0,-1,.8))
        assertTrue(vm.positionIndex>0); assertTrue(vm.bookOffset>0)
    }
    @Test fun chapterControlsReplaceTextAndResetThePagePosition() {
        val vm=state(TestApp(directory.newFolder(),MemoryPreferences()))
        vm.selected=Fic("123","Title","Writer"); vm.chapters=listOf(Chapter("One","First chapter"),Chapter("Two","Second distinct chapter")); vm.screen="reader"
        vm.bookOffset=9; vm.positionIndex=7; val revision=vm.readerRevision
        vm.changeChapter(1)
        assertEquals("Second distinct chapter",vm.chapters[vm.chapter].text); assertEquals(0,vm.bookOffset); assertEquals(0,vm.positionIndex); assertTrue(vm.readerRevision>revision)
        vm.changeChapter(0,fromEnd=true); assertEquals(vm.chapters[0].text.lastIndex,vm.bookOffset)
        vm.changeChapter(-1); assertEquals(0,vm.chapter)
    }
    @Test fun offlineReaderAndExactPositionRecoverInANewViewModel() {
        val app=TestApp(directory.newFolder(),MemoryPreferences()); val fic=Fic("123","Title","Writer")
        ReadingLibrary(File(app.directory,"reading-library")).saveOffline(fic,listOf(Chapter("One","First"),Chapter("Two","Longer second chapter")))
        val first=state(app); first.openLibrary(first.libraryEntries.single()); awaitLoaded(first); assertNull(first.error)
        first.changeChapter(1); first.bookOffset=8; first.positionIndex=2; first.positionOffset=4; first.saveProgress(); first.viewModelScope.cancel()
        val resumed=state(app); awaitLoaded(resumed)
        assertNull(resumed.error); assertEquals("reader",resumed.screen); assertEquals(1,resumed.chapter); assertEquals(8,resumed.bookOffset); assertEquals(2,resumed.positionIndex); assertEquals(4,resumed.positionOffset)
    }
    @Test fun appearanceChangesSaveWithoutLeavingTheReader() {
        val app=TestApp(directory.newFolder(),MemoryPreferences()); val vm=state(app); vm.screen="reader"
        vm.appearance(theme="dark",family="sans-serif",spacing=1.9f); vm.fontSize(27)
        assertEquals("reader",vm.screen); assertEquals("dark",vm.preferences.theme); assertEquals(27,vm.preferences.fontSize)
        val restored=state(app); assertEquals("dark",restored.preferences.theme); assertEquals("sans-serif",restored.preferences.fontFamily); assertEquals(1.9f,restored.preferences.lineSpacing,.001f); assertEquals(27,restored.preferences.fontSize)
    }
    @Test fun noPublicTranslationProviderIsSilentlySelected() {
        val app=TestApp(directory.newFolder(),MemoryPreferences()); val vm=state(app)
        assertEquals("",vm.preferences.service); vm.selected=Fic("123","Title","Writer"); vm.chapters=listOf(Chapter("One","Original")); vm.translate()
        assertFalse(vm.translating); assertNull(vm.translation); assertTrue(vm.error!!.contains("Configure"))
    }
}
