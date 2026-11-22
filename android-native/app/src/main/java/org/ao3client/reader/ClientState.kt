package org.ao3client.reader

import android.app.Application
import android.content.Context
import androidx.compose.runtime.*
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.*
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.io.File

data class Preferences(val language: String = "en", val filterLanguage: Boolean = false, val service: String = "", val apiKey: String = "", val fontSize: Int = 19, val theme: String = "light", val fontFamily: String = "serif", val lineSpacing: Float = 1.6f)

class ClientState @JvmOverloads constructor(app: Application, checkSession: Boolean = true) : AndroidViewModel(app) {
    val storage = app.getSharedPreferences("native-reader", Context.MODE_PRIVATE)
    private val archive = Archive(storage)
    var preferences by mutableStateOf(Preferences(storage.getString("language", "en")!!, storage.getBoolean("filterLanguage", false), storage.getString("service", "")!!, storage.getString("apiKey", "")!!, storage.getInt("fontSize", 19), storage.getString("theme", "light")!!, storage.getString("fontFamily", "serif")!!, storage.getFloat("lineSpacing", 1.6f)))
    var screen by mutableStateOf("home")
    var settingsReturn = "home"
    var query by mutableStateOf("")
    var filters by mutableStateOf<Map<String,List<String>>>(if (preferences.filterLanguage) mapOf("language_id" to listOf(preferences.language)) else emptyMap())
    var filterDraft by mutableStateOf<Map<String,List<String>>>(filters)
    private var appliedFilters = filters
    private var appliedQuery = ""
    private var searchListing by mutableStateOf<Listing?>(null)
    private var accountListing by mutableStateOf<Listing?>(null)
    val listing: Listing? get() = if(ownWorks) accountListing else searchListing
    var profile by mutableStateOf<AuthorProfile?>(null)
    var profileUser by mutableStateOf("")
    var profileTab by mutableStateOf("profile")
    var profileListing by mutableStateOf<Listing?>(null)
    var profileReturn = "details"
    var ownWorks by mutableStateOf(false)
    var selected by mutableStateOf<Fic?>(null)
    val accountPending=mutableStateMapOf<String,Boolean>()
    fun setMarkedForLater(id:String,later:Boolean)=workAccountAction(id) {expected -> archive.setMarkedForLater(id,later,expected)}
    fun setWorkSubscribed(id:String,subscribed:Boolean)=workAccountAction(id) {expected -> archive.setWorkSubscribed(id,subscribed,expected)}
    private fun workAccountAction(id:String,action:(String)->String) {
        val expected=username ?: return
        if(id in accountPending)return
        error=null
        val generation=kudosGeneration
        accountPending[id]=true
        viewModelScope.launch {
            try {
                val message=withContext(Dispatchers.IO) {action(expected)}
                if(generation==kudosGeneration)notice=message
            } catch(e:CancellationException) {throw e}
              catch(e:Exception) {if(generation==kudosGeneration)error=e.message ?: "Account action unavailable"}
            finally {if(generation==kudosGeneration)accountPending.remove(id)}
        }
    }
    val kudosStates=mutableStateMapOf<String,String>()
    private var kudosGeneration=0
    fun giveKudos(id:String) {
        if(kudosStates.containsKey(id))return
        error=null
        val generation=kudosGeneration;val expected=username
        kudosStates[id]="Sending kudos…"
        viewModelScope.launch {
            try {
                val result=withContext(Dispatchers.IO) { archive.giveKudos(id,expected) }
                if(generation!=kudosGeneration)return@launch
                kudosStates[id]=if(result.success) "Kudos given" else if(result.uncertain) "Check kudos on AO3" else "Kudos unavailable"
                notice=result.message
            } catch(e:CancellationException) { if(generation==kudosGeneration)kudosStates[id]="Check kudos on AO3";throw e }
              catch(e:Exception) { if(generation==kudosGeneration) { kudosStates.remove(id);error=e.message ?: "Kudos could not be sent" } }
        }
    }
    var officialDownloads by mutableStateOf<List<OfficialDownload>?>(null)
    fun fetchOfficialDownloads(id:String)=load {
        val formats=withContext(Dispatchers.IO) { archive.downloads(id) }
        if(selected?.id==id) officialDownloads=formats
    }
    fun saveOfficialDownload(id:String,item:OfficialDownload,uri:android.net.Uri)=load {
        officialDownloads=null
        withContext(Dispatchers.IO) {
            val bytes=OfficialDownloads.fetch(item,id)
            ensureActive()
            getApplication<Application>().contentResolver.openOutputStream(uri,"w")?.use { it.write(bytes) } ?: error("Cannot save download")
        }
        notice="Saved ${item.filename}"
    }
    var chapters by mutableStateOf<List<Chapter>>(emptyList())
    var chapter by mutableIntStateOf(0)
    var translation by mutableStateOf<String?>(null)
    var translating by mutableStateOf(false)
    var translationProgress by mutableStateOf("")
    var commentsPage by mutableStateOf<CommentPage?>(null)
    var commentsWork=""
    var commentsReturn="details"
    fun showComments(workId:String,page:Int=1) {
        if(commentSending && workId!=commentsWork)return
        if(screen!="comments") { commentsReturn=screen;commentsPage=null; }
        if(commentsWork!=workId) { commentForm=null;commentDraft="";commentUncertain=false;++commentFormRevision }
        commentsWork=workId; navigate("comments")
        load { commentsPage=withContext(Dispatchers.IO) { archive.comments(workId,page) };commentUncertain=false }
    }
    var commentForm by mutableStateOf<CommentForm?>(null)
    var commentDraft by mutableStateOf("")
    var commentName by mutableStateOf("")
    var commentEmail by mutableStateOf("")
    var commentPseud by mutableStateOf("")
    var commentSending by mutableStateOf(false)
    var commentUncertain by mutableStateOf(false)
    private var commentFormRevision=0
    fun composeComment(replyId:String="") {
        if(commentSending || commentUncertain) return
        val revision=++commentFormRevision;val work=commentsWork
        load {
            val form=withContext(Dispatchers.IO) { archive.commentForm(work,replyId) }
            if(revision==commentFormRevision && work==commentsWork) {
                commentForm=form;commentPseud=form.hidden["comment[pseud_id]"].orEmpty()
            }
        }
    }
    fun closeComment() { if(!commentSending) { ++commentFormRevision;commentForm=null } }
    fun sendComment() {
        val form=commentForm ?: return
        if(commentSending || commentUncertain || commentDraft.isBlank()) return
        val work=commentsWork;val text=commentDraft;val pseud=commentPseud;val name=commentName;val email=commentEmail
        commentSending=true;error=null
        viewModelScope.launch {
            try {
                val result=withContext(Dispatchers.IO) { archive.postComment(form,text,pseud,name,email) }
                if(work==commentsWork) {
                    commentUncertain=result.uncertain
                    if(result.error.isNotBlank()) error=result.error
                    else { notice=result.notice;commentDraft="";commentForm=null;if(screen=="comments")showComments(work,commentsPage?.page ?: 1) }
                }
            } catch(_:Exception) {
                if(work==commentsWork) { commentUncertain=true;error="Could not confirm whether the comment was posted. Refresh comments before trying again." }
            } finally { commentSending=false }
        }
    }
    var username by mutableStateOf<String?>(archive.rememberedUsername())
    var loading by mutableStateOf(false)
    var error by mutableStateOf<String?>(null)
    var notice by mutableStateOf<String?>(null)
    var settingsConfirmation by mutableStateOf(false)
    private var loadJob: Job? = null
    private var translateJob: Job? = null
    private var translationGeneration = 0
    var fullScreen by mutableStateOf(false)
    var textOnly by mutableStateOf(false)
    var bookMode by mutableStateOf(storage.getBoolean("bookMode", false))
    var pageAnimation by mutableStateOf(storage.getBoolean("pageAnimation", true))
    var bookOffset = -1
    fun readingMode(book: Boolean = bookMode, animation: Boolean = pageAnimation) {
        if(book != bookMode) bookOffset = -1
        bookMode=book; pageAnimation=animation
        storage.edit().putBoolean("bookMode",book).putBoolean("pageAnimation",animation).apply()
    }
    var positionIndex = 0
    var positionOffset = 0
    private val library = ReadingLibrary(File(app.filesDir, "reading-library"))
    var libraryEntries by mutableStateOf<List<LibraryEntry>>(emptyList())
    var bookmarks by mutableStateOf<List<ReadingBookmark>>(emptyList())
    var readerReturn = "details"
    var detailsReturn = "search"
    var readerRevision by mutableIntStateOf(0)
    private var requestRevision = 0
    private var sessionJob: Job? = null
    init { if(preferences.service=="https://translate.cutie.dating") { preferences=preferences.copy(service=""); storage.edit().putString("service","").putBoolean("translationConsented",false).apply() }; refreshLibrary(); if(storage.getBoolean("resumeReader",false)) libraryEntries.firstOrNull { it.fic.id==storage.getString("lastFic","") }?.let { openLibrary(it) }; if(checkSession) sessionJob = viewModelScope.launch { try { username = withContext(Dispatchers.IO) { archive.session() } } catch (_: Exception) { /* Browsing remains usable offline. */ } } }
    private fun load(block: suspend () -> Unit) {
        loadJob?.cancel(); val revision = ++requestRevision; error = null; loading = true
        loadJob = viewModelScope.launch {
            try { block() } catch (e: CancellationException) { throw e } catch (e: Exception) { error = e.message ?: "Request failed. Please try again." } finally { if (revision == requestRevision) loading = false }
        }
    }
    fun cancelLoad() { ++requestRevision; loadJob?.cancel(); loading=false }
    fun navigate(destination: String) {
        if (screen == "reader") saveProgress()
        if (destination != "details") officialDownloads=null
        if (destination != "reader") { ++requestRevision; loadJob?.cancel(); loading = false; cancelTranslation(); fullScreen = false; textOnly = false }
        error = null
        if (destination == "filters" && screen != "filters") filterDraft = filters
        if (destination == "library") refreshLibrary()
        if (destination == "settings") settingsReturn = screen
        screen = destination
        storage.edit().putBoolean("resumeReader",destination=="reader").apply()
    }
    fun search(page: Int = 1, fresh: Boolean = true) {
        if (fresh) { appliedFilters = filters; appliedQuery = query.trim(); ownWorks = false; searchListing = null }
        screen = "search"
        val account = ownWorks; val user = username; val querySnapshot = appliedQuery; val filterSnapshot = appliedFilters
        load {
            val result = withContext(Dispatchers.IO) {
                if (account) archive.works(user ?: error("Please sign in first"), page)
                else archive.search(querySnapshot, page, filterSnapshot)
            }
            if(account) accountListing = result else searchListing = result
        }
    }
    fun showProfile(path: String) {
        val name = Regex("/users/([A-Za-z0-9_]+)(?:/|$)").find(path)?.groupValues?.get(1)
        if(name == null) { error = "This work has no public author profile."; return }
        if(screen != "details" || detailsReturn != "profile") profileReturn = screen
        profileUser = name; profile = null; profileListing = null; profileTab = "profile"; screen = "profile"
        load { profile = withContext(Dispatchers.IO) { archive.profile(name) } }
    }
    fun profileWorks(bookmarks: Boolean, page: Int = 1) {
        profileTab = if(bookmarks) "bookmarks" else "works"
        if(page == 1) profileListing = null
        val name = profileUser
        load { profileListing = withContext(Dispatchers.IO) { archive.authorWorks(name, page, bookmarks) } }
    }
    fun myWorks() {
        if (username == null) { navigate("login"); return }
        ownWorks = true; search(1, false)
    }
    fun showDetails(fic: Fic) { officialDownloads=null; detailsReturn = screen; readerReturn = "details"; if(selected?.id != fic.id) chapters = emptyList(); selected = fic; navigate("details") }
    fun read() {
        val fic = selected ?: return
        load {
            val result = withContext(Dispatchers.IO) { val content = library.load(fic.id) ?: archive.read(fic.id).first; library.remember(fic); content }
            chapters = result
            storage.edit().putString("lastFic",fic.id).putBoolean("resumeReader",true).apply()
            refreshLibrary(); refreshBookmarks()
            chapter = storage.getInt("progress.${fic.id}.chapter", 0).coerceIn(chapters.indices)
            restorePosition(); translation = null; screen = "reader"; fullScreen = false
        }
    }

    fun exportLibrary(stream: java.io.OutputStream, includeDownloads:Boolean = true) = load {
        saveProgress()
        withContext(Dispatchers.IO) { stream.bufferedWriter().use { val data=LibraryBackup.snapshot(library,storage,includeDownloads).toString(); require(data.toByteArray(Charsets.UTF_8).size<=LibraryBackup.MAX_BYTES) { "Backup exceeds 100 MB. Export reading data without downloads, and save large fics separately as text." }; it.write(data) } }
        notice="Library backup exported. Keep it private: it contains reading history and downloaded text."
    }
    fun importLibrary(stream: java.io.InputStream) = load {
        withContext(Dispatchers.IO) { stream.use {
            val bytes=readBackupBytes(it); require(bytes.size<=LibraryBackup.MAX_BYTES) { "Backup exceeds 100 MB" }
            LibraryBackup.merge(JSONObject(String(bytes,Charsets.UTF_8)),library,storage)
        } }
        refreshLibrary(); refreshBookmarks(); notice="Library backup imported. Existing downloads and reading positions were kept."
    }
    fun refreshLibrary() { try { libraryEntries = library.entries(); if(java.io.File(getApplication<Application>().filesDir,"reading-library").listFiles().orEmpty().any { it.name.contains(".corrupt-") } && !storage.getBoolean("recoveryNoticeShown",false)) { notice="Some damaged library data was preserved and recovered where possible. Check your bookmarks and keep a backup."; storage.edit().putBoolean("recoveryNoticeShown",true).apply() } } catch(e: Exception) { error = e.message } }
    fun openLibrary(entry: LibraryEntry) { selected = entry.fic; readerReturn = "library"; read() }
    fun saveOffline() {
        val fic = selected ?: return; val content = chapters
        if(content.isEmpty()) return
        load { withContext(Dispatchers.IO) { library.saveOffline(fic, content) }; refreshLibrary() }
    }
    fun favorite(ficId: String, value: Boolean) { try { library.favorite(ficId, value); refreshLibrary() } catch(e: Exception) { error = e.message } }
    fun removeOffline(ficId: String) { try { library.removeOffline(ficId); refreshLibrary() } catch(e: Exception) { error = e.message } }
    fun refreshBookmarks() { try { bookmarks = selected?.let { library.bookmarks(it.id) }.orEmpty() } catch(e: Exception) { error = e.message } }
    fun addBookmark(title: String) {
        val fic = selected ?: return
        try { library.addBookmark(fic.id, title, chapter, positionIndex, positionOffset, if(bookMode) bookOffset else -1, readingFraction(translation ?: chapters[chapter].text,positionIndex,if(bookMode) bookOffset else -1)); refreshBookmarks() } catch(e: Exception) { error = e.message }
    }
    fun removeBookmark(bookmark: ReadingBookmark) { val fic = selected ?: return; try { library.removeBookmark(fic.id, bookmark.id); refreshBookmarks() } catch(e: Exception) { error = e.message } }
    fun jumpBookmark(bookmark: ReadingBookmark) {
        if(bookmark.chapter !in chapters.indices) return
        saveProgress(); cancelTranslation(); translation = null; chapter = bookmark.chapter
        positionIndex = bookmark.index; positionOffset = bookmark.offset; bookOffset = bookmark.bookOffset
        if(bookmark.fraction in 0.0..1.0 && bookmark.index==0 && bookmark.offset==0) {
            val text=chapters[chapter].text
            if(bookOffset<0) bookOffset=(text.length*bookmark.fraction).toInt()
            positionIndex=paragraphStarts(text).indexOfLast { it<=bookOffset }.coerceAtLeast(0)
        }
        readerRevision++
    }
    fun changeChapter(index: Int, fromEnd: Boolean = false) {
        if (index !in chapters.indices) return
        saveProgress(); cancelTranslation(); chapter = index; translation = null
        positionIndex = 0; positionOffset = 0; bookOffset = 0
        if(fromEnd) { bookOffset = (chapters[index].text.length - 1).coerceAtLeast(0); positionIndex = 0; positionOffset = 0 }
        readerRevision++
    }
    fun restorePosition() {
        val key = "progress.${selected?.id}.$chapter"
        bookOffset = storage.getInt("$key.bookOffset", -1)
        positionIndex = storage.getInt("$key.index", 0); positionOffset = storage.getInt("$key.offset", 0)
        if(bookOffset<0 && positionIndex==0 && positionOffset==0 && storage.contains("$key.fraction")) {
            val text=chapters[chapter].text; bookOffset=(storage.getFloat("$key.fraction",0f)*text.length).toInt().coerceIn(0,text.length)
            val starts=Regex("""\n\s*\n""").findAll(text).map { it.range.last+1 }.toList()
            positionIndex=starts.count { it<=bookOffset }; positionOffset=0
        }
    }
    fun saveProgress() {
        if (selected == null || chapters.isEmpty()) return
        val key = "progress.${selected!!.id}.$chapter"
        storage.edit().putString("lastFic",selected!!.id).putBoolean("resumeReader",screen=="reader").putInt("progress.${selected!!.id}.chapter", chapter).putInt("$key.bookOffset", bookOffset).putInt("$key.index", positionIndex).putInt("$key.offset", positionOffset).putFloat("$key.fraction",readingFraction(translation ?: chapters[chapter].text,positionIndex,if(bookMode) bookOffset else -1).toFloat()).apply()
    }
    fun restart() {
        selected?.let { fic -> storage.edit().also { edit -> storage.all.keys.filter { it.startsWith("progress.${fic.id}.") }.forEach { edit.remove(it) } }.apply() }
        cancelTranslation(); chapter = 0; positionIndex = 0; positionOffset = 0; bookOffset = 0; translation = null; readerRevision++
    }
    fun login(name: String, password: String) = load {
        ++kudosGeneration;kudosStates.clear();accountPending.clear()
        sessionJob?.cancel()
        username = withContext(Dispatchers.IO) { archive.login(name.trim(), password) }
        screen = "home"
    }
    fun logout() { ++kudosGeneration;kudosStates.clear();accountPending.clear();sessionJob?.cancel(); loadJob?.cancel(); archive.logout(); username = null; commentsPage=null; commentsWork="";commentForm=null;commentDraft="";commentEmail="";++commentFormRevision; searchListing = null; accountListing = null; selected = null; chapters = emptyList(); ownWorks = false; navigate("home") }
    suspend fun suggestions(type: String, term: String) = withContext(Dispatchers.IO) { archive.suggest(type, term) }
    fun savePreferences(value: Preferences) {
        val code = value.language.trim().lowercase()
        if (!code.matches(Regex("[a-z]{2,3}(?:-[a-z]{2,4})?")) || (value.service.isNotBlank() && !validTranslationService(value.service.trim()))) { error = "Enter a language code and an HTTPS translation URL, or leave it blank. HTTP is allowed for localhost only."; return }
        if(preferences.service!=value.service.trim()) storage.edit().putBoolean("translationConsented",false).apply()
        preferences = value.copy(language = code, service = value.service.trim(), lineSpacing = value.lineSpacing.coerceIn(1.2f, 2.2f))
        storage.edit().putString("language", code).putBoolean("filterLanguage", value.filterLanguage).putString("service", value.service.trim()).putString("apiKey", value.apiKey).putInt("fontSize", value.fontSize).putString("theme", value.theme).putString("fontFamily", value.fontFamily).putFloat("lineSpacing", value.lineSpacing.coerceIn(1.2f, 2.2f)).apply()
        filters = if (value.filterLanguage) filters + ("language_id" to listOf(code)) else filters - "language_id"
        cancelTranslation(); translation = null; error = null; screen = settingsReturn
        settingsConfirmation = true
    }
    fun appearance(theme: String = preferences.theme, family: String = preferences.fontFamily, spacing: Float = preferences.lineSpacing) {
        preferences = preferences.copy(theme = theme, fontFamily = family, lineSpacing = spacing.coerceIn(1.2f, 2.2f))
        storage.edit().putString("theme", preferences.theme).putString("fontFamily", preferences.fontFamily).putFloat("lineSpacing", preferences.lineSpacing).apply()
    }
    fun fontSize(value: Int) {
        preferences = preferences.copy(fontSize = value.coerceIn(14, 32))
        storage.edit().putInt("fontSize", preferences.fontSize).apply()
    }
    fun cancelTranslation() { translationGeneration++; translateJob?.cancel(); translating = false; translationProgress = "" }
    fun original() { cancelTranslation(); translation = null }
    fun translate() {
        if (translating) { cancelTranslation(); return }
        if (chapters.isEmpty()) return
        if(!validTranslationService(preferences.service)) { error="Configure a trusted HTTPS LibreTranslate service in Reading settings first."; return }
        val generation=++translationGeneration
        val text = chapters[chapter].text; val prefs = preferences
        translateJob = viewModelScope.launch {
            translating = true; error = null
            try {
                val pieces = translationChunks(text)
                val result = StringBuilder()
                for ((i, piece) in pieces.withIndex()) {
                    ensureActive(); translationProgress = "Translating ${i + 1} of ${pieces.size}"
                    if(i > 0) result.append("\n")
                    result.append(withContext(Dispatchers.IO) {
                        val base = URL(prefs.service)
                        val endpoint = if (base.path.trim('/').endsWith("translate")) base else URL(prefs.service.trimEnd('/') + "/translate")
                        val connection = endpoint.openConnection() as HttpURLConnection
                        try {
                            connection.instanceFollowRedirects = false; connection.connectTimeout = 15000; connection.readTimeout = 45000
                            connection.requestMethod = "POST"; connection.doOutput = true
                            connection.setRequestProperty("Content-Type", "application/json")
                            val body = JSONObject().put("q", piece).put("source", "auto").put("target", prefs.language).put("format", "text")
                            if (prefs.apiKey.isNotBlank()) body.put("api_key", prefs.apiKey)
                            // This connection has no AO3 cookie manager or headers.
                            connection.outputStream.use { it.write(body.toString().toByteArray()) }
                            val status = connection.responseCode
                            val response = (if (status >= 400) connection.errorStream else connection.inputStream)?.bufferedReader()?.use { it.readText() }.orEmpty()
                            if (status !in 200..299) error("Translation service returned HTTP $status")
                            val json = JSONObject(response)
                            if (json.has("error")) error(json.getString("error"))
                            json.optString("translatedText").ifBlank { error("Translation service returned no text") }
                        } finally { connection.disconnect() }
                    })
                }
                translation = result.toString()
            } catch (e: CancellationException) { throw e } catch (e: Exception) { error = e.message ?: "Translation failed" } finally { if(generation==translationGeneration) { translating = false; translationProgress = "" } }
        }
    }
}
