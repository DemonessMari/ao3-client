@file:OptIn(androidx.compose.material3.ExperimentalMaterial3Api::class, androidx.compose.foundation.layout.ExperimentalLayoutApi::class)
package org.ao3client.reader
import androidx.compose.foundation.background

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.foundation.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.lazy.*
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material.icons.automirrored.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.text.TextRange
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.*
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import kotlinx.coroutines.delay
import kotlinx.coroutines.CancellationException

class MainActivity : ComponentActivity() {
    private val model: ClientState by viewModels()
    private fun readerVolumeDirection(keyCode:Int):Int? {
        if(model.screen!="reader" || !model.volumeNavigation || model.readerOverlayOpen)return null
        return when(keyCode){android.view.KeyEvent.KEYCODE_VOLUME_DOWN->model.volumeStep(true);android.view.KeyEvent.KEYCODE_VOLUME_UP->model.volumeStep(false);else->null}
    }
    override fun onKeyDown(keyCode:Int,event:android.view.KeyEvent):Boolean {
        val direction=readerVolumeDirection(keyCode)
        if(direction!=null){if(event.repeatCount==0)model.stepReading(direction);return true}
        return super.onKeyDown(keyCode,event)
    }
    override fun onKeyUp(keyCode:Int,event:android.view.KeyEvent):Boolean {
        if(readerVolumeDirection(keyCode)!=null)return true
        return super.onKeyUp(keyCode,event)
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState); enableEdgeToEdge()
        setContent {
            val colors = when(if(model.preferences.theme=="system") { if(isSystemInDarkTheme()) "dark" else "light" } else model.preferences.theme) {
                "dark" -> darkColorScheme(primary = Color(0xFFFFB3B9), background = Color(0xFF191B1E), surface = Color(0xFF191B1E))
                "sepia" -> lightColorScheme(primary = Color(0xFF843D21), background = Color(0xFFF1E3C7), surface = Color(0xFFF1E3C7))
                else -> lightColorScheme(primary = Color(0xFF920B19), secondary = Color(0xFF76564C), background = Color(0xFFFFF8F5), surface = Color(0xFFFFF8F5))
            }
            val readingFamily=readerFontFamily(model)
            val interfaceFamily=if(useImportedInterfaceFont(model.preferences))readingFamily else FontFamily.Default
            val typography=remember(interfaceFamily) {interfaceTypography(interfaceFamily)}
            val skin=model.appSkin
            val density=androidx.compose.ui.platform.LocalDensity.current
            androidx.compose.runtime.CompositionLocalProvider(LocalSkinTypography provides typography, LocalAppSkin provides skin, LocalSkinRadius provides skin?.takeIf{it.version>=2}?.cornerRadius, androidx.compose.ui.platform.LocalDensity provides androidx.compose.ui.unit.Density(density.density*(skin?.uiScale ?: 1.0).toFloat(),density.fontScale)) {
                MaterialTheme(colorScheme = skin?.colorScheme() ?: colors,typography=typography,shapes=skin?.takeIf{it.version>=2}?.shapes() ?: Shapes()) {
                    ClientApp(model)
                }
            }
        }
    }
    override fun onStop() { model.saveProgress(); super.onStop() }
}

@Composable fun ClientApp(vm: ClientState) {
    if(vm.notice!=null) AlertDialog(onDismissRequest={vm.notice=null},title={Text("AO3 Client")},text={Text(vm.notice.orEmpty())},confirmButton={SkinTextButton(onClick={vm.notice=null}) { Text("OK") }})
    if(vm.settingsConfirmation) AlertDialog(
        onDismissRequest = { vm.settingsConfirmation = false },
        title = { Text("Settings saved") },
        text = { Text("Language and translation settings saved.") },
        confirmButton = { SkinTextButton(onClick = { vm.settingsConfirmation = false }) { Text("OK") } }
    )
    val keyboard = LocalSoftwareKeyboardController.current
    BackHandler(vm.screen !in listOf("home", "reader")) {
        keyboard?.hide()
        vm.navigate(when (vm.screen) { "skins" -> vm.skinReturn; "settings" -> vm.settingsReturn; "archiveBookmark" -> vm.bookmarkReturn; "archiveList" -> vm.archiveReturn; "series" -> vm.seriesReturn; "comments" -> vm.commentsReturn; "filters" -> "search"; "details" -> vm.detailsReturn; "profile" -> vm.profileReturn; else -> "home" })
    }
    if(vm.screen=="website") {key(vm.websiteRevision) {ArchiveWebsiteScreen(vm)};return}
    if (vm.screen == "reader") { key(vm.selected?.id, vm.chapter, vm.readerRevision) { ReaderScreen(vm) }; return }
    Box(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
    SkinImageCanvas("window",Modifier.matchParentSize())
    Scaffold(containerColor=Color.Transparent,
        topBar = { ClientTopBar(vm) },
        bottomBar = { if (vm.screen in listOf("home", "search", "library", "settings")) NavigationBar {
            NavigationBarItem(selected = vm.screen == "home", onClick = { vm.navigate("home") }, icon = { Icon(Icons.Default.Home, null) }, label = { Text("Home") })
            NavigationBarItem(selected = vm.screen == "search" && !vm.ownWorks, onClick = { vm.ownWorks = false; vm.navigate("search") }, icon = { Icon(Icons.Default.Search, null) }, label = { Text("Search") })
            NavigationBarItem(selected = vm.screen == "library", onClick = { vm.navigate("library") }, icon = { Icon(Icons.AutoMirrored.Filled.List, null) }, label = { Text("Library") })
            NavigationBarItem(selected = vm.screen == "settings", onClick = { vm.navigate("settings") }, icon = { Icon(Icons.Default.Settings, null) }, label = { Text("Settings") })
        } }
    ) { padding ->
        Column(Modifier.fillMaxSize().padding(padding).imePadding()) {
            if (vm.loading) { LinearProgressIndicator(Modifier.fillMaxWidth()); SkinTextButton(onClick=vm::cancelLoad) { Text("Cancel request") } }
            vm.error?.let { ErrorCard(it) { vm.error = null } }
            when(vm.screen) {
                "home" -> HomeScreen(vm)
                "search" -> SearchScreen(vm)
                "library" -> LibraryScreen(vm)
                "archiveBookmark" -> ArchiveBookmarkScreen(vm)
                "comments" -> CommentsScreen(vm)
                "archiveList" -> ArchiveListScreen(vm)
                "series" -> SeriesScreen(vm)
                "profile" -> ProfileScreen(vm)
                "filters" -> FilterScreen(vm)
                "details" -> DetailScreen(vm)
                "skins" -> SkinControls(vm)
                "settings" -> SettingsScreen(vm)
                "login" -> LoginScreen(vm)
                "account" -> Column(Modifier.verticalScroll(rememberScrollState()).padding(24.dp), verticalArrangement = Arrangement.spacedBy(20.dp)) { ProfilePicture(vm,vm.username.orEmpty()); Text(vm.username.orEmpty(), style = MaterialTheme.typography.headlineSmall); SkinTextButton(onClick = { vm.showProfile("/users/" + vm.username.orEmpty()) }) { Text("View my profile") }; SkinTextButton(onClick = vm::myWorks) { Text("Browse my works") }; SkinTextButton(onClick={vm.showAccountReadings(true)}){Text("Marked for Later")}; SkinTextButton(onClick={vm.showArchiveList("history")}){Text("AO3 reading history")}; Ao3AccountLinks(vm); SkinOutlinedButton(onClick=vm::verifyNativeSession){Text("Check native sign-in")}; SkinOutlinedButton(onClick = vm::logout) { Text("Sign out") } }
            }
        }
    }
}
}
@Composable private fun ClientTopBar(vm: ClientState) {
    val showBack=vm.screen in listOf("skins","archiveBookmark","comments","filters","details","login","profile","series","archiveList") || vm.screen=="settings" && vm.settingsReturn=="reader"
    val navigation: @Composable () -> Unit = {
        if(showBack)IconButton(onClick={vm.navigate(when(vm.screen) {
            "skins" -> vm.skinReturn; "settings" -> vm.settingsReturn; "archiveBookmark" -> vm.bookmarkReturn
            "archiveList" -> vm.archiveReturn; "series" -> vm.seriesReturn; "comments" -> vm.commentsReturn; "profile" -> vm.profileReturn; "login" -> "home"; "details" -> vm.detailsReturn; else -> "search"
        })}) {Icon(Icons.AutoMirrored.Filled.ArrowBack,"Back")}
    }
    val account: @Composable () -> Unit = {
        if(vm.screen !in listOf("login","settings")) {
            val user=vm.username
            if(user==null)SkinTextButton(onClick={vm.navigate("login")}) {Text("Sign in")}
            else {
                var expanded by remember(user) { mutableStateOf(false) }
                Box {
                    IconButton(onClick={expanded=true},modifier=Modifier.semantics { contentDescription="Account: $user" }) { ProfilePicture(vm,user,size=36.dp) }
                    DropdownMenu(expanded=expanded,onDismissRequest={expanded=false}) {
                        Text(user,Modifier.padding(horizontal=16.dp,vertical=8.dp),style=MaterialTheme.typography.titleMedium)
                        DropdownMenuItem(text={Text("Check native sign-in")},onClick={expanded=false;vm.verifyNativeSession()})
                        DropdownMenuItem(text={Text("Account details")},onClick={expanded=false;vm.navigate("account")})
                        DropdownMenuItem(text={Text("My profile")},onClick={expanded=false;vm.showProfile("/users/$user")})
                        listOf("bookmarks" to "My AO3 bookmarks","subscriptions" to "Subscriptions","inbox" to "Inbox").forEach{(kind,title)->DropdownMenuItem(text={Text(title)},onClick={expanded=false;vm.showArchiveList(kind)})}
                        DropdownMenuItem(text={Text("Marked for Later")},onClick={expanded=false;vm.showAccountReadings(true)})
                        DropdownMenuItem(text={Text("AO3 reading history")},onClick={expanded=false;vm.showArchiveList("history")})
                        DropdownMenuItem(text={Text("My works")},onClick={expanded=false;vm.myWorks()})
                        DropdownMenuItem(text={Text("Sign out")},onClick={expanded=false;vm.logout()})
                    }
                }
            }
        }
    }
    SkinHeader {
    if(vm.screen in listOf("home","search","library","settings","account")) {
        Surface(color=Color.Transparent) {
            Row(Modifier.fillMaxWidth().windowInsetsPadding(WindowInsets.safeDrawing.only(WindowInsetsSides.Top+WindowInsetsSides.Horizontal)).heightIn(min=64.dp).padding(horizontal=12.dp,vertical=8.dp),verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                navigation()
                androidx.compose.foundation.Image(androidx.compose.ui.res.painterResource(R.drawable.icon),contentDescription=null,modifier=Modifier.size(width=44.dp,height=36.dp))
                Text("Archive of Our Own",Modifier.weight(1f),style=MaterialTheme.typography.titleMedium,fontWeight=FontWeight.Bold)
                account()
            }
        }
    } else TopAppBar(title={Text(when(vm.screen) {
        "skins" -> "App skin"; "archiveBookmark" -> "AO3 bookmark"; "comments" -> "Comments"; "filters" -> "Search filters"
        "archiveList" -> vm.archiveList?.title.orEmpty(); "series" -> "Series"; "details" -> "About this fic"; "login" -> "Sign in"; "profile" -> vm.profileUser; else -> "Archive of Our Own"
    },maxLines=1,overflow=TextOverflow.Ellipsis)},navigationIcon=navigation,actions={account()},colors=TopAppBarDefaults.topAppBarColors(containerColor=Color.Transparent,scrolledContainerColor=Color.Transparent))
    }
}

@Composable fun ErrorCard(message: String, dismiss: () -> Unit) {
    Surface(color = MaterialTheme.colorScheme.errorContainer, modifier = Modifier.fillMaxWidth()) {
        Row(Modifier.padding(horizontal = 16.dp, vertical = 8.dp), verticalAlignment = Alignment.CenterVertically) {
            SelectionContainer(Modifier.weight(1f)) { Text(message, color = MaterialTheme.colorScheme.onErrorContainer) }
            IconButton(onClick = dismiss) { Icon(Icons.Default.Close, "Dismiss error") }
        }
    }
}
@Composable fun HomeScreen(vm:ClientState) {
    LazyColumn(Modifier.fillMaxSize(),contentPadding=PaddingValues(20.dp),verticalArrangement=Arrangement.spacedBy(16.dp)) {
        item {Text("Home",style=MaterialTheme.typography.headlineMedium)}
        item {Text("${vm.libraryEntries.size} recent works · ${vm.libraryEntries.count{it.offline}} offline · ${vm.libraryEntries.count{it.favorite}} favorites")}
        item {FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp),verticalArrangement=Arrangement.spacedBy(8.dp)){SkinButton(onClick={vm.navigate("search")}){Text("Search")};SkinOutlinedButton(onClick={vm.refreshLibrary();vm.navigate("library")}){Text("Library")};SkinOutlinedButton(onClick=vm::showSkins){Text("App skin")}}}
        item {Text("Recent reads",style=MaterialTheme.typography.titleLarge)}
        if(vm.libraryEntries.isEmpty())item{Text("Your recent reads will appear here.")}
        items(vm.libraryEntries.take(5),key={it.fic.id}) {entry->SkinCard(onClick={vm.openLibrary(entry)},modifier=Modifier.fillMaxWidth()){Column(verticalArrangement=Arrangement.spacedBy(8.dp)){Text(entry.fic.title,style=MaterialTheme.typography.titleMedium);Text("by ${entry.fic.author}");Text(if(entry.offline)"Available offline" else "Online",style=MaterialTheme.typography.labelMedium)}}}
        item {SkinOutlinedButton(onClick={vm.openWebsite(Archive.BASE+"/")},modifier=Modifier.fillMaxWidth()){Text("Browse AO3 website")}}
    }
}
@Composable fun SearchScreen(vm: ClientState) {
    val keyboard = LocalSoftwareKeyboardController.current
    val listState = rememberLazyListState()
    Column(Modifier.fillMaxSize()) {
        if (!vm.ownWorks) {
            FlowRow(Modifier.padding(horizontal=16.dp),horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                listOf("people" to "People","tags" to "Tags","bookmarks" to "Bookmarks").forEach{(kind,title)->SkinTextButton(onClick={vm.startDiscovery(kind)}){Text(title)}}
            }
            Row(Modifier.padding(horizontal = 16.dp, vertical = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                SkinOutlinedTextField(vm.query, { vm.query = it }, Modifier.weight(1f), placeholder = { Text("Title, author, keywords") }, singleLine = true,
                    keyboardOptions = KeyboardOptions(imeAction = ImeAction.Search), keyboardActions = KeyboardActions(onSearch = { keyboard?.hide(); vm.search() }))
                IconButton(onClick = { keyboard?.hide(); vm.search() }, enabled = !vm.loading) { Icon(Icons.Default.Search, "Search AO3") }
            }
            Row(Modifier.fillMaxWidth().padding(horizontal = 16.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween) {
                Text("Search", modifier = Modifier.weight(1f), style = MaterialTheme.typography.titleMedium)
                FilterChip(selected = vm.filters.isNotEmpty(), onClick = { keyboard?.hide(); vm.navigate("filters") }, label = { Text("Filters${if(vm.filters.isEmpty()) "" else " · ${vm.filters.size}"}") })
            }
        } else Text(vm.accountListTitle, style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(16.dp))
        val result = vm.listing
        if (result == null) Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) { Text(if(vm.ownWorks) if(vm.loading) "Loading account list…" else "Account list unavailable." else "Enter a search.", Modifier.padding(24.dp)) }
        else LazyColumn(state = listState, modifier = Modifier.fillMaxSize(), contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            if (result.total.isNotBlank()) item { Text(result.total, style = MaterialTheme.typography.labelLarge) }
            if (result.works.isEmpty() && result.unavailable.isEmpty()) item { Text(if(vm.ownWorks) "No works in this list." else "No works found.") }
            items(result.works, key = { it.id }) { fic ->
                SkinCard(onClick = { vm.showDetails(fic) }, modifier = Modifier.fillMaxWidth()) {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text(fic.title.ifBlank { "Untitled" }, style = MaterialTheme.typography.titleLarge, color = MaterialTheme.colorScheme.primary)
                        Text("by ${fic.author}", style = MaterialTheme.typography.bodyMedium)
                        if (fic.fandom.isNotBlank()) Text(fic.fandom, style = MaterialTheme.typography.labelLarge, maxLines = 2, overflow = TextOverflow.Ellipsis)
                        if (fic.summary.isNotBlank()) Text(fic.summary, maxLines = 4, overflow = TextOverflow.Ellipsis)
                        if (fic.stats.isNotBlank()) Text(fic.stats, style = MaterialTheme.typography.bodySmall)
                        if(fic.readingInfo.isNotBlank())Text(fic.readingInfo,style=MaterialTheme.typography.bodyMedium)
                    }
                }
            }
            items(result.unavailable) { info -> Text(info,Modifier.padding(vertical=12.dp)) }
            if(result.pages > 1) item {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                    SkinOutlinedButton(onClick = { vm.search(result.page - 1, false) }, enabled = result.page > 1 && !vm.loading) { Text("Previous") }
                    Text("${result.page} / ${result.pages}")
                    SkinOutlinedButton(onClick = { vm.search(result.page + 1, false) }, enabled = result.page < result.pages && !vm.loading) { Text("Next") }
                }
            }
        }
        LaunchedEffect(result?.page, result?.works?.firstOrNull()?.id) { listState.scrollToItem(0) }
    }
}
@Composable fun FilterScreen(vm: ClientState) {
    val draft = vm.filterDraft
    Column(Modifier.fillMaxSize()) {
        LazyColumn(Modifier.weight(1f), contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(20.dp)) {
            items(Catalogue.fields, key = { it.key }) { field -> FilterControl(field, draft[field.key].orEmpty(), { values -> vm.filterDraft = if(values.isEmpty()) vm.filterDraft - field.key else vm.filterDraft + (field.key to values) }, vm) }
        }
        Surface(tonalElevation = 3.dp) {
            Row(Modifier.fillMaxWidth().padding(16.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                SkinOutlinedButton(onClick = { vm.filterDraft = emptyMap() }) { Text("Reset") }
                SkinButton(onClick = { vm.filters = vm.filterDraft; vm.search() }, Modifier.weight(1f)) { Text("Apply and search") }
            }
        }
    }
}
@Composable fun FilterControl(field: FilterField, values: List<String>, update: (List<String>) -> Unit, vm: ClientState) {
    var expanded by rememberSaveable(field.key) { mutableStateOf(false) }
    var focused by remember { mutableStateOf(false) }
    var requestSuggestions by remember { mutableStateOf(false) }
    var suggestions by remember { mutableStateOf<List<String>>(emptyList()) }
    val value = values.firstOrNull().orEmpty()
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        if(field.choices.isNotEmpty()) {
            MenuRow(
                title = field.label,
                supportingText = if(values.isEmpty()) "Any" else field.choices.filter { it.value in values }.joinToString { it.label },
                onClick = { expanded = !expanded },
                trailing = { Text(if(expanded) "−" else "+", style = MaterialTheme.typography.headlineSmall) }
            )
            if(expanded) {
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilterChip(selected = values.isEmpty(), onClick = { update(emptyList()) }, label = { Text("Any") })
                    field.choices.forEach { choice -> FilterChip(selected = choice.value in values, onClick = { update(if(field.multi) if(choice.value in values) values - choice.value else values + choice.value else if(choice.value in values) emptyList() else listOf(choice.value)) }, label = { Text(choice.label) }) }
                }
            }
        } else {
            var input by remember(field.key) { mutableStateOf(TextFieldValue(value)) }
            LaunchedEffect(value) { if(input.text != value) input = TextFieldValue(value, TextRange(value.length)) }
            SkinOutlinedTextField(input, { requestSuggestions = true; input = it; update(if(it.text.isBlank()) emptyList() else listOf(it.text)) }, Modifier.fillMaxWidth().onFocusChanged { focused = it.isFocused }, label = { Text(field.label) }, placeholder = { Text(field.hint) }, singleLine = true)
            val term = activeTagToken(value, input.selection.end).term
            LaunchedEffect(value, input.selection.end, focused, requestSuggestions) {
                suggestions = emptyList()
                if (field.suggestion != null && term.length >= 2 && focused && requestSuggestions) {
                    delay(300)
                    try { suggestions = vm.suggestions(field.suggestion, term) } catch(e: CancellationException) { throw e } catch (_: Exception) { suggestions = emptyList() }
                }
            }
            if(suggestions.isNotEmpty()) Surface(shape = RoundedCornerShape(8.dp), tonalElevation = 3.dp) {
                Column(Modifier.padding(vertical = 4.dp)) { suggestions.forEach { tag -> MenuRow(title = tag, onClick = {
                    val replacement = insertTag(value, input.selection.end, tag)
                    requestSuggestions = false
                    input = TextFieldValue(replacement.text, TextRange(replacement.cursor))
                    suggestions = emptyList(); update(listOf(replacement.text))
                }) } }
            }
        }
    }
}
@Composable fun DetailScreen(vm: ClientState) {
    val fic = vm.selected ?: return
    var downloadUrl by rememberSaveable { mutableStateOf("") }
    var downloadWorkId by rememberSaveable { mutableStateOf("") }
    var downloadFormat by rememberSaveable { mutableStateOf("") }
    val saveDownload=rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/octet-stream")) { uri ->
        if(uri!=null) vm.saveOfficialDownload(downloadWorkId,OfficialDownload(downloadFormat,downloadUrl),uri)
    }
    vm.officialDownloads?.let { formats -> AlertDialog(onDismissRequest={vm.officialDownloads=null},title={Text("Download")},text={
        Column { if(formats.isEmpty()) Text("AO3 has no download formats available for this work.")
            formats.forEach { item -> SkinTextButton(onClick={downloadWorkId=fic.id;downloadUrl=item.url;downloadFormat=item.format;vm.officialDownloads=null;saveDownload.launch(item.filename)}) { Text(item.format.uppercase()) } }
        }
    },confirmButton={SkinTextButton(onClick={vm.officialDownloads=null}) { Text("Close") }}) }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(20.dp), verticalArrangement = Arrangement.spacedBy(16.dp)) {
        Text(fic.title, style = MaterialTheme.typography.headlineMedium)
        Text("by ${fic.author}", style = MaterialTheme.typography.titleMedium)
        if(fic.rating.isNotBlank()) Text("Rating · ${fic.rating}")
        if(fic.warnings.isNotBlank()) Text("Warnings · ${fic.warnings}")
        if(fic.authorPath.isNotBlank()) SkinOutlinedButton(onClick = { vm.showProfile(fic.authorPath) }) { Text("Author profile") }
        SkinButton(onClick = vm::read, enabled = !vm.loading, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) { Text(if(vm.loading) "Loading fic…" else "Read fic") }
        if(fic.fandom.isNotBlank()) Text(fic.fandom, color = MaterialTheme.colorScheme.primary)
        if(fic.stats.isNotBlank()) Text(fic.stats, style = MaterialTheme.typography.bodyMedium)
        if(fic.summary.isNotBlank()) { Text("Summary", style = MaterialTheme.typography.titleMedium); SelectionContainer { Text(fic.summary) } }
        if(fic.tags.isNotBlank()) { Text("Tags", style = MaterialTheme.typography.titleMedium); Text(fic.tags) }
        KudosButton(vm,fic.id)
        WorkAccountButton(vm,fic.id)
        SkinOutlinedButton(onClick={vm.showSeries()},enabled=!vm.loading) { Text("Series") }
        SkinOutlinedButton(onClick={vm.showComments(fic.id)}) { Text("Comments") }
        SkinOutlinedButton(onClick={vm.fetchOfficialDownloads(fic.id)},enabled=!vm.loading) { Text("Download") }
        Ao3WorkLinks(vm,fic.id)

    }
}
@Composable fun LoginScreen(vm: ClientState) {
    var name by rememberSaveable { mutableStateOf("") }; var password by remember { mutableStateOf("") }
    val keyboard = LocalSoftwareKeyboardController.current
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp), verticalArrangement = Arrangement.spacedBy(20.dp)) {
        Text("Sign in to AO3", style = MaterialTheme.typography.headlineMedium)
        SkinOutlinedTextField(name, { name = it }, Modifier.fillMaxWidth(), label = { Text("Username or email") }, singleLine = true, keyboardOptions = KeyboardOptions(imeAction = ImeAction.Next))
        SkinOutlinedTextField(password, { password = it }, Modifier.fillMaxWidth(), label = { Text("Password") }, singleLine = true, visualTransformation = PasswordVisualTransformation(), keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password, imeAction = ImeAction.Done), keyboardActions = KeyboardActions(onDone = { keyboard?.hide(); vm.login(name, password); password = "" }))
        SkinButton(onClick = { keyboard?.hide(); vm.login(name, password); password = "" }, enabled = name.isNotBlank() && password.isNotEmpty() && !vm.loading, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) { Text("Sign in") }
        SkinOutlinedButton(onClick={keyboard?.hide();password="";vm.openWebsite(Archive.BASE+"/users/login")},modifier=Modifier.fillMaxWidth()) {Text("Sign in on AO3 website")}
    }
}
@Composable fun ReadingAppearanceControls(vm: ClientState) {
    val prefs = vm.preferences
    Text("Reading appearance", style = MaterialTheme.typography.titleLarge)
    Text("Changes save automatically.", style = MaterialTheme.typography.bodySmall)
    SkinOutlinedButton(onClick=vm::showSkins){Text("App skin" + (vm.appSkin?.let { " · "+it.name } ?: ""))}
    if(vm.appSkin!=null) Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(12.dp)) {
        Text("Use app skin colors for reading",Modifier.weight(1f))
        Switch(checked=prefs.useAppSkinColors,onCheckedChange={vm.appearance(useAppSkinColors=it)})
    }
    FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        listOf("system" to "System", "light" to "Light", "sepia" to "Sepia", "dark" to "Dark").forEach { (value, title) ->
            FilterChip(selected = (vm.appSkin == null || !prefs.useAppSkinColors) && prefs.theme == value, onClick = { vm.appearance(theme = value,useAppSkinColors=false) }, label = { Text(title) })
        }
    }
    FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        listOf("serif" to "Serif", "sans-serif" to "Sans serif").forEach { (value, title) ->
            FilterChip(selected = prefs.fontFamily == value, onClick = { vm.appearance(family = value) }, label = { Text(title) })
        }
    }
    FontControls(vm)
    Text("Text size · ${prefs.fontSize}")
    Slider(value = prefs.fontSize.toFloat(), onValueChange = { vm.fontSize(it.toInt()) }, valueRange = 14f..32f, steps = 17)
    Text("Line spacing · ${"%.1f".format(prefs.lineSpacing)}")
    Slider(value = prefs.lineSpacing, onValueChange = { vm.appearance(spacing = it) }, valueRange = 1.2f..2.2f, steps = 9)
    Text("Side margins · ${prefs.margin}")
    Slider(value=prefs.margin.toFloat(),onValueChange={vm.appearance(margin=it.toInt())},valueRange=0f..64f,steps=15)
    FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp),verticalArrangement=Arrangement.spacedBy(8.dp)) {
        listOf("left" to "Left","justify" to "Justified","center" to "Center","right" to "Right").forEach { (value,title) ->
            FilterChip(selected=prefs.alignment==value,onClick={vm.appearance(alignment=value)},label={Text(title)})
        }
    }
    val previewColors=readerColors(vm)
    Text("Text preview. Change the margins and alignment to see how your reading text will look.\nSecond paragraph of text.",modifier=Modifier.fillMaxWidth().background(previewColors.background).padding(horizontal=prefs.margin.dp,vertical=12.dp),color=previewColors.onBackground,textAlign=readingAlignment(prefs.alignment),
        fontFamily = readerFontFamily(vm),
        fontSize = prefs.fontSize.sp, lineHeight = (prefs.fontSize * prefs.lineSpacing).sp)
}

@Composable fun SettingsScreen(vm: ClientState) {
    var language by rememberSaveable { mutableStateOf(vm.preferences.language) }
    var filter by rememberSaveable { mutableStateOf(vm.preferences.filterLanguage) }
    var service by rememberSaveable { mutableStateOf(vm.preferences.service) }
    var apiKey by rememberSaveable { mutableStateOf(vm.preferences.apiKey) }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(20.dp), verticalArrangement = Arrangement.spacedBy(18.dp)) {
        ReadingAppearanceControls(vm)
        HorizontalDivider()
        LibraryBackupControls(vm)
        AboutControls()
        HorizontalDivider()
        Text("Language", style = MaterialTheme.typography.titleLarge)
        SkinOutlinedTextField(language, { language = it }, Modifier.fillMaxWidth(), label = { Text("Default language code") }, supportingText = { Text("For example: en, et, es, fr, de, ja") }, singleLine = true)
        Row(Modifier.fillMaxWidth().clickable { filter = !filter }, verticalAlignment = Alignment.CenterVertically) { Checkbox(filter, { filter = it }); Text("Use this language for new searches", Modifier.weight(1f)) }
        HorizontalDivider(); Text("Translation service", style = MaterialTheme.typography.titleLarge)
        SkinOutlinedTextField(service, { service = it }, Modifier.fillMaxWidth(), label = { Text("LibreTranslate URL") }, keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri), singleLine = true)
        SkinOutlinedTextField(apiKey, { apiKey = it }, Modifier.fillMaxWidth(), label = { Text("API key (optional)") }, visualTransformation = PasswordVisualTransformation(), singleLine = true)
        SkinButton(onClick = { vm.savePreferences(vm.preferences.copy(language = language, filterLanguage = filter, service = service, apiKey = apiKey)) }, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) { Text("Save language and translation") }
    }
}

@Composable fun LibraryBackupControls(vm: ClientState) {
    val resolver=LocalContext.current.applicationContext.contentResolver
    var includeDownloads by rememberSaveable { mutableStateOf(true) }
    var confirm by rememberSaveable { mutableStateOf<android.net.Uri?>(null) }
    val export=rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri -> uri?.let { vm.exportLibrary({ resolver.openOutputStream(it) ?: error("Cannot create backup") },includeDownloads) } }
    val restore=rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { confirm=it }
    Text("Library backup",style=MaterialTheme.typography.titleLarge)
    Text("Backups contain reading data, not credentials. Files are not encrypted.",style=MaterialTheme.typography.bodySmall)
    Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) { Checkbox(includeDownloads,{includeDownloads=it}); Text("Include downloaded chapter text",Modifier.weight(1f)) }
    Text("Maximum size: 100 MB.",style=MaterialTheme.typography.bodySmall)
    SkinOutlinedButton(onClick={export.launch("ao3-library-backup.json")},enabled=!vm.loading,modifier=Modifier.fillMaxWidth()) { Text("Export library backup") }
    SkinOutlinedButton(onClick={restore.launch(arrayOf("application/json","text/plain","application/octet-stream"))},enabled=!vm.loading,modifier=Modifier.fillMaxWidth()) { Text("Import library backup") }
    if(confirm!=null) AlertDialog(onDismissRequest={confirm=null},title={Text("Import library backup?")},text={Text("This adds the backup's fics and bookmarks. Existing downloads and progress are kept.")},confirmButton={SkinTextButton(onClick={ val uri=confirm; confirm=null; if(uri!=null)vm.importLibrary { resolver.openInputStream(uri) ?: error("Cannot read backup") } }) { Text("Import") }},dismissButton={SkinTextButton(onClick={confirm=null}) { Text("Cancel") }})
}

@Composable fun LibraryScreen(vm: ClientState) {
    var filter by rememberSaveable { mutableStateOf("recent") }
    var query by rememberSaveable { mutableStateOf("") }
    val entries = vm.libraryEntries.filter { (filter != "offline" || it.offline) && (filter != "favorites" || it.favorite) && (it.fic.title.contains(query, true) || it.fic.author.contains(query, true)) }
    LazyColumn(Modifier.fillMaxSize(), contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        item { SkinOutlinedTextField(query, { query = it }, Modifier.fillMaxWidth(), label = { Text("Search library") }, singleLine = true) }
        item { FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf("recent" to "Recent", "offline" to "Offline", "favorites" to "Favorites").forEach { (value, title) -> FilterChip(selected = filter == value, onClick = { filter = value }, label = { Text(title) }) }
        } }
        if(entries.isEmpty()) item { Text("No works.") }
        items(entries, key = { it.fic.id }) { entry ->
            SkinCard(onClick = { vm.openLibrary(entry) }, modifier = Modifier.fillMaxWidth()) {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(entry.fic.title, style = MaterialTheme.typography.titleLarge)
                    Text("by ${entry.fic.author}")
                    Text(if(entry.offline) "Available offline" else "Needs AO3 to reopen", style = MaterialTheme.typography.labelLarge)
                    Text("Last read · " + java.text.DateFormat.getDateTimeInstance(java.text.DateFormat.MEDIUM, java.text.DateFormat.SHORT).format(java.util.Date(entry.openedAt)), style = MaterialTheme.typography.bodySmall)
                    FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        SkinOutlinedButton(onClick = { vm.favorite(entry.fic.id, !entry.favorite) }) { Text(if(entry.favorite) "Unfavorite" else "Favorite") }
                        if(entry.offline) SkinTextButton(onClick = { vm.removeOffline(entry.fic.id) }) { Text("Remove download") }
                    }
                }
            }
        }
    }
}

@Composable fun ProfileScreen(vm: ClientState) {
    Column(Modifier.fillMaxSize()) {
        FlowRow(Modifier.padding(horizontal = 16.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            FilterChip(selected = vm.profileTab == "profile", onClick = { vm.profileTab = "profile" }, label = { Text("Profile") })
            FilterChip(selected = vm.profileTab == "works", onClick = { vm.profileWorks(false) }, label = { Text("Works") })
            FilterChip(selected = vm.profileTab == "bookmarks", onClick = { vm.profileWorks(true) }, label = { Text("Public fic bookmarks") })
        }
        if(vm.profileTab == "profile") {
            val profile = vm.profile
            LazyColumn(contentPadding = PaddingValues(20.dp), verticalArrangement = Arrangement.spacedBy(16.dp)) {
                item { Row(verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(16.dp)){ProfilePicture(vm,vm.profileUser,profile?.avatarUrl.orEmpty(),profile?.avatarAlt.orEmpty());Text(vm.profileUser,Modifier.weight(1f),style=MaterialTheme.typography.headlineMedium)} }
                item{EntitySubscriptionButton(vm,"User",vm.profileUser)}
                if(profile != null) {
                    if(profile.title.isNotBlank()) item { Text(profile.title, style = MaterialTheme.typography.titleLarge) }
                    if(profile.details.isNotBlank()) item { Text(profile.details) }
                    item { Text("Biography", style = MaterialTheme.typography.titleMedium) }
                    item { SelectionContainer { Text(profile.biography.ifBlank { "No public biography provided." }) } }
                }
            }
        } else {
            val result = vm.profileListing
            LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                if(result?.works?.isEmpty() == true) item { Text("No public fics in this view.") }
                items(result?.works.orEmpty(), key = { it.id }) { fic -> SkinCard(onClick = { vm.showDetails(fic) }, modifier = Modifier.fillMaxWidth()) {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) { Text(fic.title, style = MaterialTheme.typography.titleLarge); Text("by ${fic.author}"); if(fic.summary.isNotBlank()) Text(fic.summary, maxLines = 4, overflow = TextOverflow.Ellipsis); Text(fic.stats, style = MaterialTheme.typography.bodySmall) }
                } }
                if(result != null && result.pages > 1) item { Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    SkinTextButton(onClick = { vm.profileWorks(vm.profileTab == "bookmarks", result.page - 1) }, enabled = result.page > 1 && !vm.loading) { Text("Previous") }
                    Text("${result.page} / ${result.pages}")
                    SkinTextButton(onClick = { vm.profileWorks(vm.profileTab == "bookmarks", result.page + 1) }, enabled = result.page < result.pages && !vm.loading) { Text("Next") }
                } }
            }
        }
    }
}

@Composable internal fun SeriesScreen(vm:ClientState) {
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(20.dp),verticalArrangement=Arrangement.spacedBy(16.dp)) {
        Text("Series",style=MaterialTheme.typography.headlineMedium)
        val entries=vm.workSeries
        if(entries!=null && entries.isEmpty())Text("This work is not part of a series.")
        entries?.forEach {entry->
            Text(entry.position,style=MaterialTheme.typography.titleMedium)
            SkinOutlinedButton(onClick={vm.showArchiveList("series",entry.id)}){Text("View series")}
            SkinButton(onClick={entry.previous?.let(vm::openSeriesWork)},enabled=entry.previous!=null && !vm.loading,modifier=Modifier.fillMaxWidth()){Text("Previous work")}
            SkinButton(onClick={entry.next?.let(vm::openSeriesWork)},enabled=entry.next!=null && !vm.loading,modifier=Modifier.fillMaxWidth()){Text("Next work")}
            HorizontalDivider()
        }
    }
}
