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
    @Test fun nativeToolsKeepWritingAcrossHelpAndClearPasswordsAndPrivateState() {
        val calls=java.util.Collections.synchronizedList(mutableListOf<String>())
        val vm=ClientState(TestApp(directory.newFolder(),MemoryPreferences()),false,{prefs->Archive(prefs,connectionFactory={url->
            calls+=url.file
            val kind=when(url.path){"/works/new"->"work";"/skins/7/edit"->"sections";"/users/Writer/change_password"->"account";else->null}
            val html=if(kind!=null)javaClass.classLoader!!.getResource("native-tools-$kind.html")!!.readText() else "<nav id='greeting'><a href='/users/Writer'>Writer</a><a href='/users/logout'>Out</a></nav><div id='main'><h2 class='heading'>Content policy</h2><p>Policy text</p></div>"
            object:java.net.HttpURLConnection(url){override fun connect(){};override fun disconnect(){};override fun usingProxy()=false;override fun getResponseCode()=200;override fun getInputStream()=java.io.ByteArrayInputStream(html.toByteArray());override fun getHeaderFields():Map<String,List<String>> = emptyMap()}
        })}).also{models+=it}
        vm.username="Writer";vm.navigate("account");vm.showTools("/works/new");awaitLoaded(vm);val key=vm.toolPage!!.forms.single().key
        vm.editToolField(key,"work[title]",listOf("Unsaved title"));vm.showTools("/content");awaitLoaded(vm);assertNull(vm.pendingTools);vm.toolsBack();assertEquals(listOf("Unsaved title"),vm.toolDraft[key]?.get("work[title]"));assertEquals(2,calls.size)
        vm.toolsBack();assertEquals("@back",vm.pendingTools?.first);vm.pendingTools=null;vm.showTools("@back",discard=true);assertEquals("account",vm.screen)
        vm.showTools("/users/Writer/change_password");awaitLoaded(vm);val secret=vm.toolPage!!.forms.first().key;vm.editToolField(secret,"password",listOf("test-only-secret"));vm.showTools("/content");awaitLoaded(vm);vm.toolsBack();assertFalse(vm.toolDraft.values.any{fields->fields.values.flatten().contains("test-only-secret")})
        vm.showTools("/skins/7/edit");awaitLoaded(vm);val form=vm.toolPage!!.forms.single();vm.addToolSection(form.key,form.sections.single().key);assertEquals(3,vm.toolPage!!.forms.single().fields.size);assertTrue(vm.toolPage!!.inProgress)
        vm.logout();assertNull(vm.toolPage);assertTrue(vm.toolDraft.isEmpty());assertTrue(vm.toolUploads.isEmpty())
    }
    @Test fun accountEditorsPreserveDraftsAndClearPrivateDataOnLogout() {
        val calls=java.util.Collections.synchronizedList(mutableListOf<String>())
        val app=TestApp(directory.newFolder(),MemoryPreferences())
        val vm=ClientState(app,false,{prefs->Archive(prefs,connectionFactory={url->
            calls+=url.file
            val kind=if(url.path.endsWith("preferences"))"preferences" else "profile"
            val html=javaClass.classLoader!!.getResource("account-form-$kind.html")!!.readText()
            object:java.net.HttpURLConnection(url){override fun connect(){};override fun disconnect(){};override fun usingProxy()=false;override fun getResponseCode()=200;override fun getInputStream()=java.io.ByteArrayInputStream(html.toByteArray());override fun getHeaderFields():Map<String,List<String>> = emptyMap()}
        })}).also{models+=it}
        vm.username="Writer";vm.showAccountEditor("profile");awaitLoaded(vm);assertNotNull(vm.accountForm)
        vm.accountDraft=vm.accountDraft+("profile[title]" to listOf("Unsaved title"));vm.navigate("home");vm.showAccountEditor("profile");assertEquals(1,calls.size);assertEquals(listOf("Unsaved title"),vm.accountDraft["profile[title]"])
        vm.showAccountEditor("preferences");assertNotNull(vm.pendingAccountForm);assertEquals(1,calls.size)
        vm.pendingAccountForm=null;vm.showAccountEditor("preferences",discard=true);awaitLoaded(vm);assertEquals("preferences",vm.accountForm!!.kind)
        vm.logout();assertNull(vm.accountForm);assertTrue(vm.accountDraft.isEmpty());assertFalse(vm.accountSending)
    }
    @Test fun publicDiscoveryKeepsAppliedOptionsWhenPagingAndDraftsWhenSwitching() {
        val calls=java.util.Collections.synchronizedList(mutableListOf<String>())
        val app=TestApp(directory.newFolder(),MemoryPreferences())
        val vm=ClientState(app,false,{prefs->Archive(prefs,connectionFactory={url->
            calls+=url.file
            val html=javaClass.classLoader!!.getResource("discovery-${url.path.split('/')[1]}.html")!!.readText()
            object:java.net.HttpURLConnection(url){
                override fun connect(){};override fun disconnect(){};override fun usingProxy()=false
                override fun getResponseCode()=200
                override fun getInputStream()=java.io.ByteArrayInputStream(html.toByteArray())
                override fun getHeaderFields():Map<String,List<String>> = emptyMap()
            }
        })}).also{models+=it}
        vm.navigate("search");vm.startDiscovery("people");assertEquals("archiveList",vm.screen);assertEquals(0,vm.archiveList!!.page);assertTrue(calls.isEmpty())
        vm.discoveryDraft=mapOf("people" to mapOf("name" to "Writer"));vm.submitDiscovery();awaitLoaded(vm)
        assertNull(vm.error);assertEquals(1,vm.archiveList!!.entries.size)
        vm.discoveryDraft=mapOf("people" to mapOf("name" to "Unsubmitted"));vm.showArchiveList("search:people",page=2);awaitLoaded(vm)
        val request=java.net.URLDecoder.decode(calls.last(),"UTF-8");assertTrue(request.contains("people_search[name]=Writer"));assertTrue(request.contains("page=2"));assertFalse(request.contains("Unsubmitted"))
        vm.navigate(vm.archiveReturn);assertEquals("search",vm.screen);vm.startDiscovery("tags");assertEquals("Unsubmitted",vm.discoveryDraft["people"]!!["name"]);assertEquals(0,vm.archiveList!!.page)
    }
    @Test fun backupProvidersOpenOffTheUiThreadAndCloseAfterRoundTrip() {
        val root=directory.newFolder();val prefs=MemoryPreferences();val fic=Fic("654321","Backup round trip","Writer");val chapters=listOf(Chapter("One","Saved offline text 世界"))
        ReadingLibrary(File(root,"reading-library")).saveOffline(fic,chapters)
        val vm=state(TestApp(root,prefs));val caller=Thread.currentThread();val outputClosed=java.util.concurrent.atomic.AtomicBoolean(false)
        val output=object:java.io.ByteArrayOutputStream(){override fun close(){outputClosed.set(true);super.close()}}
        vm.exportLibrary({assertNotSame(caller,Thread.currentThread());output},true);awaitLoaded(vm);assertNull(vm.error);assertTrue(outputClosed.get())
        val target=directory.newFolder();val restored=state(TestApp(target,MemoryPreferences()));val inputClosed=java.util.concurrent.atomic.AtomicBoolean(false)
        val input=object:java.io.ByteArrayInputStream(output.toByteArray()){override fun close(){inputClosed.set(true);super.close()}}
        restored.importLibrary{assertNotSame(caller,Thread.currentThread());input};awaitLoaded(restored);assertNull(restored.error);assertTrue(inputClosed.get());assertEquals(chapters,ReadingLibrary(File(target,"reading-library")).load(fic.id))
    }
    @Test fun backupProviderFailureDoesNotLeaveTheInterfaceBusy() {
        val vm=state(TestApp(directory.newFolder(),MemoryPreferences()))
        vm.exportLibrary({throw java.io.IOException("Provider unavailable")});awaitLoaded(vm);assertEquals("Provider unavailable",vm.error);assertFalse(vm.loading)
        vm.importLibrary{throw java.io.IOException("Backup unavailable")};awaitLoaded(vm);assertEquals("Backup unavailable",vm.error);assertFalse(vm.loading)
    }
    @Test fun accountListsPageInTheirOwnModeAndClearOnLogout() {
        val app=TestApp(directory.newFolder(),MemoryPreferences());val html=javaClass.classLoader!!.getResource("account-readings.html")!!.readText()
        val calls=java.util.Collections.synchronizedList(mutableListOf<String>())
        val vm=ClientState(app,false,{prefs->Archive(prefs,connectionFactory={url->calls+=url.file;object:java.net.HttpURLConnection(url){
            override fun connect(){};override fun disconnect(){};override fun usingProxy()=false
            override fun getResponseCode()=200
            override fun getInputStream()=java.io.ByteArrayInputStream(html.toByteArray())
            override fun getHeaderFields():Map<String,List<String>> = emptyMap()
        }})}).also{models+=it}
        vm.username="Writer";vm.showAccountReadings(true);awaitLoaded(vm)
        assertEquals("later",vm.accountList);assertEquals("123",vm.listing!!.works.single().id)
        vm.search(2,false);awaitLoaded(vm);assertTrue(calls.last().contains("page=2&show=to-read"))
        vm.showAccountReadings(false);awaitLoaded(vm);assertEquals("history",vm.accountList);assertFalse(calls.last().contains("show="))
        vm.logout();assertNull(vm.listing);assertFalse(vm.ownWorks)
    }
    @Test fun seriesNavigationReturnsToTheOriginalFicAndReadingPosition() {
        val app=TestApp(directory.newFolder(),MemoryPreferences())
        val html=javaClass.classLoader!!.getResource("work-series.html")!!.readText()
        val vm=ClientState(app,false,{prefs->Archive(prefs,connectionFactory={url->object:java.net.HttpURLConnection(url){
            override fun connect(){};override fun disconnect(){};override fun usingProxy()=false
            override fun getResponseCode()=200
            override fun getInputStream()=java.io.ByteArrayInputStream(html.toByteArray())
            override fun getHeaderFields():Map<String,List<String>> = emptyMap()
        }})}).also{models+=it}
        vm.selected=Fic("202","Original","Writer");vm.chapters=listOf(Chapter("One","Original chapter content."));vm.chapter=0;vm.screen="reader";vm.readerReturn="library";vm.bookOffset=12
        vm.showSeries();awaitLoaded(vm);assertEquals("series",vm.screen);assertEquals(2,vm.workSeries!!.size)
        vm.openSeriesWork("303");awaitLoaded(vm);assertEquals("details",vm.screen);assertEquals("303",vm.selected!!.id);assertTrue(vm.chapters.isEmpty())
        vm.navigate("series");vm.navigate(vm.seriesReturn)
        assertEquals("reader",vm.screen);assertEquals("202",vm.selected!!.id);assertEquals("Original chapter content.",vm.chapters.single().text);assertEquals(12,vm.bookOffset);assertEquals("library",vm.readerReturn)
    }
    @Test fun volumeKeysCanBeReversedOrReturnToSystemVolume() {
        val app=TestApp(directory.newFolder(),MemoryPreferences());val vm=state(app)
        vm.volumeNavigation(true);vm.volumeAction(false,1);vm.volumeAction(true,0)
        assertEquals(1,vm.volumeStep(false));assertNull(vm.volumeStep(true))
        val restored=state(app);assertEquals(1,restored.volumeStep(false));assertNull(restored.volumeStep(true))
        restored.volumeNavigation(false);assertNull(restored.volumeStep(false))
    }
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
    @Test fun formattedModeSurvivesChapterChangesAndKeepsCanonicalProgress() {
        val vm=state(TestApp(directory.newFolder(),MemoryPreferences()));vm.selected=Fic("123","Title","Writer");vm.chapters=listOf(Chapter("One","First paragraph.\n\nSecond paragraph.\n\nThird paragraph."),Chapter("Two","Different chapter"));vm.screen="reader";vm.formattedReading=true
        vm.formattedProgress(.8);assertTrue(vm.positionIndex>0);assertTrue(vm.bookOffset>0)
        val position=vm.bookOffset;vm.closeFormattedReading();assertFalse(vm.formattedReading);assertEquals(position,vm.bookOffset)
        vm.formattedReading=true;vm.changeChapter(1);assertTrue(vm.formattedReading);assertEquals("Different chapter",vm.chapters[vm.chapter].text)
        vm.navigate("details");assertFalse(vm.formattedReading)
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
        vm.appearance(theme="system",margin=52,alignment="justify");assertEquals("Sepia",vm.appSkin?.name);assertFalse(vm.preferences.useAppSkinColors)
        assertEquals("reader",vm.screen);assertEquals(2,vm.chapter);assertEquals(123,vm.bookOffset)
        val restored=state(app);assertEquals("system",restored.preferences.theme);assertEquals(52,restored.preferences.margin);assertEquals("justify",restored.preferences.alignment);assertEquals("Sepia",restored.appSkin?.name);assertFalse(restored.preferences.useAppSkinColors)
        vm.appearance(margin=999,alignment="invalid");assertEquals(64,vm.preferences.margin);assertEquals("left",vm.preferences.alignment)
        vm.appearance(margin=-1,alignment="right");assertEquals(0,vm.preferences.margin);assertEquals("right",state(app).preferences.alignment)
    }
    @Test fun imageDraftsAndExportsKeepCreditsWithoutReplacingTheAppliedSkin() {
        val app=TestApp(directory.newFolder(),MemoryPreferences());val vm=state(app)
        val skin=AppSkin.parse(javaClass.classLoader!!.getResourceAsStream("illustrated/nebula.json")!!.use{it.readBytes()})
        vm.applySkin(skin);vm.showSkins();assertEquals(skin.images,vm.draftSkinImages);assertEquals(skin.layers,vm.draftSkinLayers)
        vm.draftSkinLayers=emptyList();assertEquals(3,vm.appSkin!!.layers.size)
        vm.pendingSkinExport=skin.json();assertEquals(skin,AppSkin.parse(vm.pendingSkinExport.toByteArray()))
        vm.appearance(theme="light",useAppSkinColors=false);assertEquals(skin,state(app).appSkin)
    }
    @Test fun readerAppearanceNeverRemovesTheCustomInterfaceSkin() {
        val app=TestApp(directory.newFolder(),MemoryPreferences());val vm=state(app)
        val skin=AppSkin.preset("dark").copy(version=3,cornerRadius=16,components=mapOf("cards" to mapOf("padding" to 12)))
        vm.applySkin(skin);vm.screen="reader";vm.chapter=2;vm.bookOffset=123
        vm.appearance(theme="light",useAppSkinColors=false);vm.appearance(family="sans-serif",spacing=1.8f,margin=40,alignment="justify");vm.fontSize(24)
        assertEquals(skin,vm.appSkin);assertFalse(vm.preferences.useAppSkinColors);assertEquals("reader",vm.screen);assertEquals(2,vm.chapter);assertEquals(123,vm.bookOffset)
        val restored=state(app);assertEquals(skin,restored.appSkin);assertFalse(restored.preferences.useAppSkinColors)
        vm.appearance(useAppSkinColors=true);assertTrue(state(app).preferences.useAppSkinColors);assertEquals(skin,vm.appSkin)
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
