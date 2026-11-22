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
    @Test fun readerExtrasPersistAndBoundSpeed() {
        val app=TestApp(directory.newFolder(),MemoryPreferences());val vm=state(app)
        vm.autoScrollSpeed(500);assertEquals(120,vm.autoScrollSpeed);vm.autoScrollSpeed(-1);assertEquals(10,vm.autoScrollSpeed)
        vm.volumeNavigation(true);vm.keepScreenOn(true);vm.tapZone("Left",1);vm.tapZone("Center",0);vm.tapZone("Right",-1);vm.tapEdge(500)
        val restored=state(app);assertEquals(10,restored.autoScrollSpeed);assertTrue(restored.volumeNavigation);assertTrue(restored.keepScreenOn);assertEquals(1,restored.tapLeft);assertEquals(0,restored.tapCenter);assertEquals(-1,restored.tapRight);assertEquals(40,restored.tapEdge)
        val previous=vm.readerStep.first;vm.stepReading(1);assertEquals(previous+1,vm.readerStep.first);assertEquals(1,vm.readerStep.second)
    }
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
    @Test fun readerLayoutPersistsAndClampsWithoutLeavingTheChapter() {
        val app=TestApp(directory.newFolder(),MemoryPreferences());val vm=state(app);vm.screen="reader";vm.chapter=2;vm.bookOffset=123
        vm.applySkin(AppSkin.preset("sepia"));vm.appearance(margin=32);assertEquals("Sepia",vm.appSkin?.name)
        vm.appearance(theme="system",margin=52,alignment="justify");assertNull(vm.appSkin)
        assertEquals("reader",vm.screen);assertEquals(2,vm.chapter);assertEquals(123,vm.bookOffset)
        val restored=state(app);assertEquals("system",restored.preferences.theme);assertEquals(52,restored.preferences.margin);assertEquals("justify",restored.preferences.alignment)
        vm.appearance(margin=999,alignment="invalid");assertEquals(64,vm.preferences.margin);assertEquals("left",vm.preferences.alignment)
        vm.appearance(margin=-1,alignment="right");assertEquals(0,vm.preferences.margin);assertEquals("right",state(app).preferences.alignment)
    }
    @Test fun missingImportedFontFallsBackAndRemovingOneKeepsTheChapter() {
        val app=TestApp(directory.newFolder(),MemoryPreferences());val missing="font:"+"a".repeat(64)
        app.prefs.edit().putString("fontFamily",missing).apply();val vm=state(app);assertEquals("serif",vm.preferences.fontFamily)
        val bytes=javaClass.getResourceAsStream("/reader-test-font.ttf")!!.readBytes();val store=ReaderFontStore(File(app.directory,"fonts")) {true};val font=store.import(bytes.inputStream(),"Test font")
        val restored=state(app);assertEquals(1,restored.importedFonts.size);restored.screen="reader";restored.chapter=2;restored.bookOffset=123;restored.appearance(family=font.id)
        assertEquals(font.id,state(app).preferences.fontFamily);restored.removeFont(font.id)
        assertEquals("serif",restored.preferences.fontFamily);assertEquals("reader",restored.screen);assertEquals(2,restored.chapter);assertEquals(123,restored.bookOffset)
    }
    @Test fun interfaceFontOptOutPersistsWithoutChangingReadingFontOrChapter() {
        val app=TestApp(directory.newFolder(),MemoryPreferences());val vm=state(app);vm.screen="reader";vm.chapter=2;vm.bookOffset=123;vm.appearance(family="sans-serif")
        assertTrue(vm.preferences.useImportedFontInInterface);vm.interfaceFont(false)
        assertFalse(state(app).preferences.useImportedFontInInterface);assertEquals("sans-serif",vm.preferences.fontFamily);assertEquals(2,vm.chapter);assertEquals(123,vm.bookOffset);assertEquals("reader",vm.screen)
        vm.interfaceFont(true);assertTrue(state(app).preferences.useImportedFontInInterface)
    }
    @Test fun skinPersistsAndTheEditorReturnsToTheReader() {
        val app=TestApp(directory.newFolder(),MemoryPreferences());val vm=state(app)
        assertEquals("home",vm.screen);vm.applySkin(AppSkin.preset("dark"));assertEquals("Dark",state(app).appSkin?.name)
        vm.screen="reader";vm.showSkins();assertEquals("skins",vm.screen);assertEquals("reader",vm.skinReturn)
        assertTrue(runCatching{vm.applySkin(AppSkin(text="#ffffff"))}.isFailure);assertEquals("Dark",vm.appSkin?.name)
        vm.screen="reader";vm.navigate("settings");assertEquals("reader",vm.settingsReturn);vm.showSkins();vm.navigate(vm.skinReturn);assertEquals("settings",vm.screen);assertEquals("reader",vm.settingsReturn)
        vm.applySkin(null);assertNull(state(app).appSkin)
    }
    @Test fun noPublicTranslationProviderIsSilentlySelected() {
        val app=TestApp(directory.newFolder(),MemoryPreferences()); val vm=state(app)
        assertEquals("",vm.preferences.service); vm.selected=Fic("123","Title","Writer"); vm.chapters=listOf(Chapter("One","Original")); vm.translate()
        assertFalse(vm.translating); assertNull(vm.translation); assertTrue(vm.error!!.contains("Configure"))
    }
}
