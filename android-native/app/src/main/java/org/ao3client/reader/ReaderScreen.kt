@file:OptIn(androidx.compose.material3.ExperimentalMaterial3Api::class)
package org.ao3client.reader

import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.clickable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.lazy.*
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material.icons.automirrored.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.Alignment
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch
import kotlinx.coroutines.delay
import androidx.compose.foundation.gestures.scrollBy
import androidx.compose.foundation.gestures.animateScrollBy
import androidx.compose.foundation.interaction.collectIsDraggedAsState
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver

@Composable internal fun readerColors(vm:ClientState):ColorScheme {
    if(vm.preferences.useAppSkinColors)vm.appSkin?.let{return it.colorScheme()}
    val theme=if(vm.preferences.theme=="system") {if(androidx.compose.foundation.isSystemInDarkTheme())"dark" else "light"} else vm.preferences.theme
    return AppSkin.preset(theme).colorScheme()
}
@Composable fun ReaderScreen(vm: ClientState) {
    MaterialTheme(colorScheme=readerColors(vm)){ReaderContent(vm)}
}
@Composable private fun ReaderContent(vm: ClientState) {
    val readingFamily=readerFontFamily(vm)
    val fic = vm.selected ?: return
    if(vm.chapters.isEmpty()) return
    val activity = LocalContext.current as MainActivity
    val context = LocalContext.current
    var tools by remember { mutableStateOf(false) }
    val formatted=vm.formattedReading
    var translationConsent by remember { mutableStateOf(false) }
    var appearance by remember { mutableStateOf(false) }
    var speechOptions by remember { mutableStateOf(false) }
    val speech=remember(context) {ReaderSpeech(context)}
    DisposableEffect(speech) {onDispose {speech.close()}}
    DisposableEffect(vm.chapter,vm.translation,speech) {onDispose {speech.stop()}}
    var notes by remember(vm.chapter) { mutableStateOf(false) }
    val currentChapter = vm.chapters[vm.chapter]
    var chaptersMenu by remember { mutableStateOf(false) }
    var bookmarksMenu by remember { mutableStateOf(false) }
    var bookmarkDialog by remember { mutableStateOf(false) }
    var bookmarkName by remember { mutableStateOf("") }
    val text = vm.translation ?: vm.chapters[vm.chapter].text
    val rich = remember(text,currentChapter.styles,vm.translation,fic.id) { if(vm.translation==null) currentChapter.styledText(fic.id) else AnnotatedString(text) }
    val paragraphs = remember(text) { text.split(Regex("""\n\s*\n""")).filter { it.isNotBlank() }.ifEmpty { listOf(text) } }
    val paragraphOffsets=remember(text,paragraphs) { var cursor=0; paragraphs.map { p -> text.indexOf(p,cursor).coerceAtLeast(cursor).also { cursor=it+p.length } } }
    val chapterIndex = vm.chapter
    val scroll = key(vm.chapter, vm.readerRevision, vm.bookMode) { rememberLazyListState(vm.positionIndex.coerceIn(paragraphs.indices), vm.positionOffset.coerceAtLeast(0)) }
    val ioScope=rememberCoroutineScope()
    var autoScroll by remember(vm.chapter,text,vm.bookMode) { mutableStateOf(false) }
    val dragged by scroll.interactionSource.collectIsDraggedAsState()
    LaunchedEffect(dragged) {if(dragged)autoScroll=false}
    val density=LocalDensity.current.density
    val view=LocalView.current
    SideEffect { vm.readerOverlayOpen=formatted || tools || appearance || notes || chaptersMenu || bookmarksMenu || bookmarkDialog || translationConsent || speechOptions }
    DisposableEffect(activity,view,vm.keepScreenOn) {
        val previous=view.keepScreenOn;view.keepScreenOn=vm.keepScreenOn
        val observer=LifecycleEventObserver { _,event -> if(event==Lifecycle.Event.ON_PAUSE){autoScroll=false;if(speech.active && !speech.paused)speech.pauseOrResume();vm.saveProgress()} }
        activity.lifecycle.addObserver(observer)
        onDispose {activity.lifecycle.removeObserver(observer);view.keepScreenOn=previous;vm.readerOverlayOpen=false}
    }
    LaunchedEffect(autoScroll,vm.autoScrollSpeed,scroll) {
        if(!autoScroll)vm.saveProgress()
        while(autoScroll && !vm.bookMode) {
            delay(33)
            if(!scroll.canScrollForward){autoScroll=false;break}
            if(!scroll.isScrollInProgress)scroll.scrollBy(vm.autoScrollSpeed*density*.033f)
        }
    }
    LaunchedEffect(tools,appearance,notes,chaptersMenu,bookmarksMenu,bookmarkDialog,translationConsent) {
        if(tools || appearance || notes || chaptersMenu || bookmarksMenu || bookmarkDialog || translationConsent)autoScroll=false
    }
    var consumedStep by remember { mutableLongStateOf(vm.readerStep.first) }
    LaunchedEffect(vm.readerStep) {
        if(consumedStep!=vm.readerStep.first){consumedStep=vm.readerStep.first;autoScroll=false
            if(!vm.bookMode)scroll.animateScrollBy(vm.readerStep.second*(scroll.layoutInfo.viewportEndOffset-scroll.layoutInfo.viewportStartOffset)*.85f)
        }
    }

    val save = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("text/plain")) { uri ->
        uri?.let { ioScope.launch { try {
            val content = fic.title + "\nby " + fic.author + "\n\n" + vm.chapters.joinToString("\n\n") { it.exportText() }
            kotlinx.coroutines.withContext(kotlinx.coroutines.Dispatchers.IO) { context.contentResolver.openOutputStream(it)?.bufferedWriter()?.use { writer -> writer.write(content) } ?: error("Unable to open file") }
        } catch (e: kotlinx.coroutines.CancellationException) { throw e } catch (e: Exception) { vm.error = e.message } } }
    }
    val hideChrome = vm.fullScreen || vm.textOnly
    DisposableEffect(hideChrome) {
        val controller = WindowCompat.getInsetsController(activity.window, activity.window.decorView)
        controller.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        if (hideChrome) controller.hide(WindowInsetsCompat.Type.systemBars()) else controller.show(WindowInsetsCompat.Type.systemBars())
        onDispose { controller.show(WindowInsetsCompat.Type.systemBars()) }
    }
    LaunchedEffect(scroll, chapterIndex, vm.translation, vm.bookMode) {
        var lastAutoSave=0L
        snapshotFlow { scroll.firstVisibleItemIndex to scroll.firstVisibleItemScrollOffset }.distinctUntilChanged().collect { (index, offset) -> if(vm.chapter == chapterIndex && !vm.bookMode) { vm.bookOffset=-1; vm.positionIndex = index; vm.positionOffset = offset; val now=android.os.SystemClock.uptimeMillis();if(!autoScroll || now-lastAutoSave>=1000){vm.saveProgress();lastAutoSave=now} } }
    }
    BackHandler { when { speechOptions -> speechOptions=false; notes -> notes = false; appearance -> appearance = false; tools -> tools = false; vm.textOnly -> vm.textOnly = false; vm.fullScreen -> vm.fullScreen = false; else -> vm.navigate(vm.readerReturn) } }
    Scaffold(
        topBar = { if(!hideChrome) SkinHeader {TopAppBar(title = { Text(fic.title, maxLines = 1, overflow = TextOverflow.Ellipsis, style = MaterialTheme.typography.titleMedium) }, navigationIcon = { IconButton(onClick = { vm.navigate(vm.readerReturn) }) { Icon(Icons.AutoMirrored.Filled.ArrowBack, "Back to fic") } }, actions = { IconButton(onClick = { tools = true }) { Icon(Icons.Default.MoreVert, "Reading options") } },colors=TopAppBarDefaults.topAppBarColors(containerColor=androidx.compose.ui.graphics.Color.Transparent,scrolledContainerColor=androidx.compose.ui.graphics.Color.Transparent))} },
        bottomBar = { if(!hideChrome) Surface(tonalElevation = 3.dp) {
            Row(Modifier.fillMaxWidth().navigationBarsPadding().padding(horizontal = 12.dp, vertical = 6.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween) {
                IconButton(onClick = { vm.changeChapter(vm.chapter - 1) }, enabled = vm.chapter > 0) { Icon(Icons.AutoMirrored.Filled.KeyboardArrowLeft, "Previous chapter") }
                Box { SkinTextButton(onClick = { chaptersMenu = true }) { Text("Chapter ${vm.chapter + 1} of ${vm.chapters.size}") }
                    DropdownMenu(expanded = chaptersMenu, onDismissRequest = { chaptersMenu = false }, modifier = Modifier.heightIn(max = 320.dp).widthIn(max = 320.dp)) {
                        vm.chapters.forEachIndexed { i, chapter -> DropdownMenuItem(text = { Text("${i + 1}. ${chapter.title}", softWrap = true) }, onClick = { chaptersMenu = false; vm.changeChapter(i) }) }
                    }
                }
                IconButton(onClick = { vm.changeChapter(vm.chapter + 1) }, enabled = vm.chapter < vm.chapters.lastIndex) { Icon(Icons.AutoMirrored.Filled.KeyboardArrowRight, "Next chapter") }
            }
        } }
    ) { padding ->
        Box(Modifier.fillMaxSize().padding(padding)) {
            Column(Modifier.fillMaxSize()) {
                if(vm.loading && !vm.textOnly) { LinearProgressIndicator(Modifier.fillMaxWidth()); SkinTextButton(onClick=vm::cancelLoad) { Text("Cancel request") } }
                if(speech.active && !vm.textOnly) Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.End) {SkinTextButton(onClick={speech.pauseOrResume()}){Text(if(speech.paused)"Resume speech" else "Pause speech")};SkinTextButton(onClick={speech.stop()}){Text("Stop speech")}}
                if(autoScroll && !vm.textOnly) SkinTextButton(onClick={autoScroll=false}) {Text("Pause auto-scroll")}
                if(!vm.textOnly) Row(Modifier.fillMaxWidth().padding(start=24.dp,end=12.dp),verticalAlignment=Alignment.CenterVertically) {
                    Text(currentChapter.title,Modifier.weight(1f).padding(vertical=6.dp),style=MaterialTheme.typography.labelLarge)
                    if(currentChapter.notesBefore.isNotBlank() || currentChapter.notesAfter.isNotBlank()) SkinTextButton(onClick={notes=true}) { Text("Author notes") }
                }
                if(!vm.textOnly) vm.error?.let { ErrorCard(it) { vm.error = null } }
                if(vm.translating && !vm.textOnly) { LinearProgressIndicator(Modifier.fillMaxWidth()); Text(vm.translationProgress, Modifier.padding(horizontal = 20.dp, vertical = 4.dp), style = MaterialTheme.typography.labelMedium) }
                if(vm.translation != null && !vm.textOnly) Text("Machine translation · ${vm.preferences.language}", Modifier.padding(horizontal = 20.dp, vertical = 4.dp), style = MaterialTheme.typography.labelMedium)
                if(vm.bookMode) BookReader(vm,text,paragraphs,Modifier.weight(1f).fillMaxWidth(),rich)
                else SelectionContainer(Modifier.weight(1f)) {
                    LazyColumn(state = scroll, contentPadding = PaddingValues(horizontal = vm.preferences.margin.dp, vertical = 20.dp), verticalArrangement = Arrangement.spacedBy(18.dp), modifier = Modifier.fillMaxSize().widthIn(max = 840.dp)) {
                        itemsIndexed(paragraphs) { i, paragraph -> Text(rich.subSequence(paragraphOffsets[i],paragraphOffsets[i]+paragraph.length), fontFamily = readingFamily, fontSize = vm.preferences.fontSize.sp, lineHeight = (vm.preferences.fontSize * vm.preferences.lineSpacing).sp,modifier=Modifier.fillMaxWidth(),textAlign=readingAlignment(vm.preferences.alignment)) }
                        item {
                            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                                SkinTextButton(onClick = { vm.changeChapter(vm.chapter - 1) }, enabled = vm.chapter > 0) { Text("Previous chapter") }
                                SkinTextButton(onClick = { vm.changeChapter(vm.chapter + 1) }, enabled = vm.chapter < vm.chapters.lastIndex) { Text("Next chapter") }
                            }
                        }
                        item { Spacer(Modifier.height(24.dp)) }
                    }
                }
            }
            if(vm.textOnly && !vm.bookMode) Box(Modifier.align(Alignment.Center).size(80.dp).clickable(onClickLabel="Show reading controls") { vm.textOnly=false })
            if(vm.fullScreen && !vm.textOnly) FilledTonalIconButton(onClick = { vm.fullScreen = false }, modifier = Modifier.align(Alignment.TopEnd).padding(8.dp)) { Icon(Icons.Default.Close, "Exit fullscreen") }
        }
    }
    if(formatted)FormattedReader(vm,vm::closeFormattedReading)
    if(tools) ModalBottomSheet(
        onDismissRequest = { tools = false },
        sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
        shape = RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp)
    ) {
        Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(horizontal = 24.dp).padding(bottom = 24.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("Reading options", style = MaterialTheme.typography.titleLarge)
            MenuRow("Read aloud",onClick={autoScroll=false;tools=false;speechOptions=true})
            TapZoneControls(vm)
            if(!vm.bookMode) {
                MenuRow(if(autoScroll) "Pause auto-scroll" else "Start auto-scroll",onClick={tools=false;autoScroll=!autoScroll})
                Text("Auto-scroll speed · ${vm.autoScrollSpeed}")
                Slider(value=vm.autoScrollSpeed.toFloat(),onValueChange={vm.autoScrollSpeed(it.toInt())},valueRange=10f..120f)
            }
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically){Text("Volume buttons turn pages",Modifier.weight(1f));Switch(vm.volumeNavigation,{vm.volumeNavigation(it)})}
            if(vm.volumeNavigation)VolumeControls(vm)
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically){Text("Keep screen on",Modifier.weight(1f));Switch(vm.keepScreenOn,{vm.keepScreenOn(it)})}

            if(currentChapter.html.isNotBlank())MenuRow("Original formatting",supportingText="Tables, poetry, images and work skin",onClick={tools=false;vm.formattedReading=true})
            MenuRow("Save offline with images",onClick={tools=false;vm.saveOffline(true)})
            MenuRow(if(vm.libraryEntries.any { it.fic.id == fic.id && it.offline }) "Save current copy offline" else "Save offline", onClick = { tools = false; vm.saveOffline() })
            KudosButton(vm,fic.id)
            WorkAccountButton(vm,fic.id)
            MenuRow("Series",onClick={tools=false;vm.showSeries()})
            MenuRow("Comments",onClick={tools=false;vm.showComments(fic.id)})
            if(currentChapter.notesBefore.isNotBlank() || currentChapter.notesAfter.isNotBlank()) MenuRow("Author notes",onClick={tools=false;notes=true})
            MenuRow("Add bookmark", onClick = { tools = false; bookmarkName = vm.chapters[vm.chapter].title; bookmarkDialog = true })
            MenuRow("Bookmarks · ${vm.bookmarks.size}", onClick = { tools = false; bookmarksMenu = true })
            MenuRow("Reading appearance", supportingText = "Theme, font, text size and line spacing · applies immediately", onClick = { tools = false; appearance = true })
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                Text("Text size · ${vm.preferences.fontSize}", Modifier.weight(1f))
                Row { SkinOutlinedButton(onClick = { vm.fontSize(vm.preferences.fontSize - 1) }) { Text("A−") }; Spacer(Modifier.width(12.dp)); SkinOutlinedButton(onClick = { vm.fontSize(vm.preferences.fontSize + 1) }) { Text("A+") } }
            }
            SkinButton(onClick = { if(vm.translating || vm.storage.getBoolean("translationConsented",false)) { vm.translate(); tools = false } else translationConsent=true }, Modifier.fillMaxWidth()) { Text(if(vm.translating) "Cancel translation" else "Translate chapter to ${vm.preferences.language}") }
            if(vm.translation != null) MenuRow("Show original", onClick = { vm.original(); tools = false })
            MenuRow("Text only", supportingText="Hide all controls. Tap the center or press Back to restore them.", onClick={ tools=false; vm.textOnly=true })
            Row(Modifier.fillMaxWidth(), verticalAlignment=Alignment.CenterVertically) { Text("Book mode",Modifier.weight(1f)); Switch(checked=vm.bookMode,onCheckedChange={vm.readingMode(book=it)}) }
            Row(Modifier.fillMaxWidth(), verticalAlignment=Alignment.CenterVertically) { Text("Animate page turns",Modifier.weight(1f)); Switch(checked=vm.pageAnimation,onCheckedChange={vm.readingMode(animation=it)}) }
            MenuRow("Fullscreen reading", onClick = { tools = false; vm.fullScreen = true })
            MenuRow("Reading and translation settings", onClick = { tools = false; vm.navigate("settings") })
            MenuRow("Save full fic as text", onClick = { tools = false; save.launch(fic.title.replace(Regex("[/\\:*?\"<>|]"), "_") + ".txt") })
            HorizontalDivider()
            Ao3WorkLinks(vm,fic.id)
            MenuRow("Start from the beginning", onClick = { tools = false; vm.restart() })
        }
    }
    if(speechOptions) ModalBottomSheet(onDismissRequest={speechOptions=false},sheetState=rememberModalBottomSheetState(skipPartiallyExpanded=true),shape=RoundedCornerShape(topStart=16.dp,topEnd=16.dp)) {
        var language by remember {mutableStateOf(vm.storage.getString("speechLanguage",if(vm.translation!=null)vm.preferences.language else "en").orEmpty())}
        var rate by remember {mutableFloatStateOf(vm.storage.getFloat("speechRate",1f).coerceIn(.5f,2f))}
        Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(24.dp),verticalArrangement=Arrangement.spacedBy(12.dp)) {
            Text("Read aloud",style=MaterialTheme.typography.titleLarge)
            SkinOutlinedTextField(language,{language=it.take(16)},label={Text("Speech language, e.g. en or es")},singleLine=true,modifier=Modifier.fillMaxWidth())
            Text("Speech rate · ${"%.1f".format(rate)}×");Slider(rate,{rate=it},valueRange=.5f..2f)
            speech.error?.let {Text(it,color=MaterialTheme.colorScheme.error)}
            SkinButton(onClick={vm.storage.edit().putString("speechLanguage",language.trim()).putFloat("speechRate",rate).apply();val offset=if(vm.bookMode)vm.bookOffset.coerceAtLeast(0) else paragraphOffsets.getOrElse(scroll.firstVisibleItemIndex){text.length};speech.start(text,offset,language.trim(),rate)},modifier=Modifier.fillMaxWidth()){Text("Read from here")}
            Row {SkinTextButton(onClick={speech.pauseOrResume()},enabled=speech.active){Text(if(speech.paused)"Resume" else "Pause")};SkinTextButton(onClick={speech.stop()}){Text("Stop")}}
            SkinTextButton(onClick={speechOptions=false}){Text("Done")}
        }
    }
    if(notes) ModalBottomSheet(onDismissRequest={notes=false},sheetState=rememberModalBottomSheetState(skipPartiallyExpanded=true),shape=RoundedCornerShape(topStart=16.dp,topEnd=16.dp)) {
        Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(24.dp),verticalArrangement=Arrangement.spacedBy(16.dp)) {
            Text("Author notes",style=MaterialTheme.typography.titleLarge)
            SelectionContainer {
                Column(verticalArrangement=Arrangement.spacedBy(12.dp)) {
                    if(currentChapter.notesBefore.isNotBlank()) { Text("Before chapter",style=MaterialTheme.typography.titleMedium); Text(currentChapter.notesBefore,fontFamily=readingFamily,fontSize=vm.preferences.fontSize.sp,lineHeight=(vm.preferences.fontSize*vm.preferences.lineSpacing).sp) }
                    if(currentChapter.notesAfter.isNotBlank()) { Text("After chapter",style=MaterialTheme.typography.titleMedium); Text(currentChapter.notesAfter,fontFamily=readingFamily,fontSize=vm.preferences.fontSize.sp,lineHeight=(vm.preferences.fontSize*vm.preferences.lineSpacing).sp) }
                }
            }
            SkinButton(onClick={notes=false},modifier=Modifier.fillMaxWidth()) { Text("Done") }
        }
    }
    if(translationConsent) AlertDialog(onDismissRequest={translationConsent=false},title={Text("Send chapter for translation?")},text={Text("Chapter text will be sent to your translation service.")},confirmButton={SkinTextButton(onClick={vm.storage.edit().putBoolean("translationConsented",true).apply(); translationConsent=false; tools=false; vm.translate()}) { Text("Translate") }},dismissButton={SkinTextButton(onClick={translationConsent=false}) { Text("Cancel") }})
    if(appearance) ModalBottomSheet(
        onDismissRequest = { appearance = false },
        sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
        shape = RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp)
    ) {
        Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(24.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            ReadingAppearanceControls(vm)
            SkinButton(onClick = { appearance = false }, modifier = Modifier.fillMaxWidth()) { Text("Done") }
        }
    }
    if(bookmarkDialog) AlertDialog(onDismissRequest = { bookmarkDialog = false }, title = { Text("Add bookmark") }, text = {
        SkinOutlinedTextField(bookmarkName, { bookmarkName = it }, label = { Text("Bookmark name") }, singleLine = true)
    }, confirmButton = { SkinTextButton(onClick = { vm.addBookmark(bookmarkName); bookmarkDialog = false }, enabled = bookmarkName.isNotBlank()) { Text("Save bookmark") } }, dismissButton = { SkinTextButton(onClick = { bookmarkDialog = false }) { Text("Cancel") } })
    if(bookmarksMenu) ModalBottomSheet(onDismissRequest = { bookmarksMenu = false }, sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true), shape = RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp)) {
        LazyColumn(contentPadding = PaddingValues(16.dp)) {
            item { Text("Bookmarks", style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(16.dp)) }
            if(vm.bookmarks.isEmpty()) item { Text("No bookmarks.", Modifier.padding(16.dp)) }
            items(vm.bookmarks, key = { it.id }) { bookmark ->
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Box(Modifier.weight(1f)) { MenuRow(bookmark.title, supportingText = "Chapter ${bookmark.chapter + 1}", onClick = { vm.jumpBookmark(bookmark); bookmarksMenu = false }) }
                    SkinTextButton(onClick = { vm.removeBookmark(bookmark) }) { Text("Delete") }
                }
            }
        }
    }

}
