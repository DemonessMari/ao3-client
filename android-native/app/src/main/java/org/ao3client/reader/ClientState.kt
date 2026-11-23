package org.ao3client.reader

import android.app.Application
import android.content.Context
import android.content.ContentResolver
import android.net.Uri
import android.provider.OpenableColumns
import androidx.compose.runtime.*
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.*
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.io.File

internal fun validAlignment(value: String) = value.takeIf { it in listOf("left","justify","center","right") } ?: "left"

data class Preferences(val language: String = "en", val filterLanguage: Boolean = false, val service: String = "", val apiKey: String = "", val fontSize: Int = 19, val theme: String = "light", val fontFamily: String = "serif", val lineSpacing: Float = 1.6f, val margin: Int = 24, val alignment: String = "left", val useImportedFontInInterface: Boolean = true)

class ClientState @JvmOverloads constructor(app: Application, checkSession: Boolean = true, archiveFactory:(android.content.SharedPreferences)->Archive={Archive(it)}) : AndroidViewModel(app) {
    val storage = app.getSharedPreferences("native-reader", Context.MODE_PRIVATE)
    private val archive = archiveFactory(storage)
    var autoScrollSpeed by mutableIntStateOf(storage.getInt("autoScrollSpeed",30).coerceIn(10,120));private set
    var volumeNavigation by mutableStateOf(storage.getBoolean("volumeNavigation",false));private set
    var volumeUp by mutableIntStateOf(storage.getInt("volumeUp",-1).coerceIn(-1,1));private set
    var volumeDown by mutableIntStateOf(storage.getInt("volumeDown",1).coerceIn(-1,1));private set
    fun volumeAction(down:Boolean,value:Int){val bounded=value.coerceIn(-1,1);if(down)volumeDown=bounded else volumeUp=bounded;storage.edit().putInt(if(down)"volumeDown" else "volumeUp",bounded).apply()}
    internal fun volumeStep(down:Boolean):Int?=(if(down)volumeDown else volumeUp).takeIf{volumeNavigation && it!=0}
    var formattedReading by mutableStateOf(false)
    var keepScreenOn by mutableStateOf(storage.getBoolean("keepScreenOn",false));private set
    var tapLeft by mutableIntStateOf(storage.getInt("tapLeft",-1).coerceIn(-1,2));private set
    var tapCenter by mutableIntStateOf(storage.getInt("tapCenter",2).coerceIn(-1,2));private set
    var tapRight by mutableIntStateOf(storage.getInt("tapRight",1).coerceIn(-1,2));private set
    var tapEdge by mutableIntStateOf(storage.getInt("tapEdge",30).coerceIn(20,40));private set
    fun tapZone(region:String,value:Int) {val action=value.coerceIn(-1,2);when(region){"Left"->tapLeft=action;"Center"->tapCenter=action;"Right"->tapRight=action;else->return};storage.edit().putInt("tap$region",action).apply()}
    fun tapEdge(value:Int) {tapEdge=value.coerceIn(20,40);storage.edit().putInt("tapEdge",tapEdge).apply()}
    fun verifyNativeSession()=load {
        val previous=username;val confirmed=withContext(Dispatchers.IO){archive.session()}
        if(confirmed==null){logout();notice="AO3 reports that the native session is signed out. Sign in again."}
        else if(previous!=null && confirmed!=previous){logout();notice="The native session belongs to a different account. Sign in again."}
        else {username=confirmed;notice="AO3 verified the native sign-in as $confirmed. A work can still be unavailable or restricted."}
    }
    internal var readerOverlayOpen=false
    internal var readerStep by mutableStateOf(0L to 0);private set
    fun autoScrollSpeed(value:Int) {autoScrollSpeed=value.coerceIn(10,120);storage.edit().putInt("autoScrollSpeed",autoScrollSpeed).apply()}
    fun volumeNavigation(value:Boolean) {volumeNavigation=value;storage.edit().putBoolean("volumeNavigation",value).apply()}
    fun keepScreenOn(value:Boolean) {keepScreenOn=value;storage.edit().putBoolean("keepScreenOn",value).apply()}
    internal fun stepReading(direction:Int) {readerStep=(readerStep.first+1) to direction.coerceIn(-1,1)}

    var preferences by mutableStateOf(Preferences(storage.getString("language", "en")!!, storage.getBoolean("filterLanguage", false), storage.getString("service", "")!!, storage.getString("apiKey", "")!!, storage.getInt("fontSize", 19), storage.getString("theme", "light")!!, storage.getString("fontFamily", "serif")!!, storage.getFloat("lineSpacing", 1.6f),storage.getInt("margin",24).coerceIn(0,64),validAlignment(storage.getString("alignment","left")!!),storage.getBoolean("useImportedFontInInterface",true)))
    private val fontStore=ReaderFontStore(File(app.filesDir,"fonts"))
    internal var importedFonts by mutableStateOf(fontStore.list())
        private set
    var fontImporting by mutableStateOf(false)
        private set
    var fontError by mutableStateOf<String?>(null)
        private set
    fun importFont(resolver: ContentResolver,uri: Uri) {
        if(fontImporting)return
        fontImporting=true; fontError=null
        viewModelScope.launch {
            try {
                val font=withContext(Dispatchers.IO) {
                    val name=resolver.query(uri,arrayOf(OpenableColumns.DISPLAY_NAME),null,null,null)?.use { cursor -> if(cursor.moveToFirst())cursor.getString(0) else null } ?: "Imported font"
                    resolver.openInputStream(uri)?.use { fontStore.import(it,name) } ?: error("Cannot read the font.")
                }
                importedFonts=fontStore.list();appearance(family=font.id);notice="Font imported."
            } catch(e: CancellationException) {throw e}
              catch(e: Exception) {fontError=e.message ?: "Could not import the font."}
            finally {fontImporting=false}
        }
    }
    fun removeFont(id: String) {
        fontError=null
        if(!fontStore.remove(id)){fontError="Could not remove the font.";return}
        importedFonts=fontStore.list();if(preferences.fontFamily==id)appearance(family="serif")
    }
    var appSkin by mutableStateOf(runCatching { AppSkin.parse(storage.getString("appSkin", "")!!.toByteArray()) }.getOrNull())
        private set
    var skinReturn="home"
    fun showSkins(){skinReturn=screen;navigate("skins")}
    fun applySkin(skin:AppSkin?){val valid=skin?.let{AppSkin.parse(it.json().toByteArray())};appSkin=valid;storage.edit().apply{if(valid==null)remove("appSkin") else putString("appSkin",valid.json())}.apply()}
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
    var accountList by mutableStateOf("works");private set
    val accountListTitle get()=when(accountList){"history"->"AO3 reading history";"later"->"Marked for Later";else->"My works"}
    var ownWorks by mutableStateOf(false)
    var selected by mutableStateOf<Fic?>(null)
    var websiteUrl=Archive.BASE+"/"
    var websiteReturn="home"
    var websiteRevision by mutableIntStateOf(0)
    fun openWebsite(url:String) {
        if(!archiveWebsiteUrl(url)){error="This address is outside AO3.";return}
        if(screen!="website")websiteReturn=screen
        websiteUrl=url;++websiteRevision;navigate("website")
    }
    var websiteImporting by mutableStateOf(false)
    fun useWebsiteSession(header:String,agent:String,expected:String) {
        if(websiteImporting)return
        val generation=kudosGeneration;websiteImporting=true;error=null;sessionJob?.cancel()
        viewModelScope.launch {
            try {
                val user=withContext(Dispatchers.IO) {archive.useWebsiteSession(header,agent,expected)}
                if(generation!=kudosGeneration)return@launch
                ++kudosGeneration;kudosStates.clear();accountPending.clear()
                archiveBookmark=null;bookmarkSending=false;bookmarkUncertain=false;++bookmarkRevision
                commentsPage=null;commentsWork="";commentForm=null;commentDraft="";commentEmail="";++commentFormRevision
                searchListing=null;accountListing=null;archiveList=null;entitySubscriptions.clear();profile=null;profileListing=null;saveProgress();selected=null;chapters=emptyList();ownWorks=false
                username=user;screen="account";notice="Website sign-in verified for the native reader."
            } catch(e:CancellationException) {throw e}
              catch(e:Exception) {if(generation==kudosGeneration)error=e.message ?: "Website sign-in could not be verified"}
            finally {websiteImporting=false}
        }
    }
    var archiveBookmark by mutableStateOf<ArchiveBookmarkForm?>(null)
    var bookmarkReturn="details"
    var bookmarkSending by mutableStateOf(false)
    var bookmarkUncertain by mutableStateOf(false)
    private var bookmarkRevision=0
    fun editArchiveBookmark(id:String,refresh:Boolean=false) {
        val expected=username ?: return
        if(bookmarkSending)return
        val revision=++bookmarkRevision
        if(screen!="archiveBookmark")bookmarkReturn=screen
        navigate("archiveBookmark")
        if(!refresh && archiveBookmark?.workId==id)return
        archiveBookmark=null;bookmarkUncertain=false
        load {
            val form=withContext(Dispatchers.IO) {archive.archiveBookmark(id,expected)}
            if(revision==bookmarkRevision)archiveBookmark=form
        }
    }
    fun saveArchiveBookmark(deleting:Boolean=false) {
        val draft=archiveBookmark ?: return;val expected=username ?: return
        if(bookmarkSending || bookmarkUncertain || draft.workId in accountPending)return
        val generation=kudosGeneration;val revision=bookmarkRevision
        bookmarkSending=true;accountPending[draft.workId]=true;error=null
        viewModelScope.launch {
            try {
                val result=withContext(Dispatchers.IO) {archive.saveArchiveBookmark(draft,deleting,expected)}
                if(generation!=kudosGeneration || revision!=bookmarkRevision)return@launch
                bookmarkUncertain=result.uncertain
                if(result.success) {archiveBookmark=null;if(screen=="archiveBookmark")navigate(bookmarkReturn);notice=result.message}
                else error=result.message
            } catch(e:CancellationException) {throw e}
              catch(e:Exception) {if(generation==kudosGeneration && revision==bookmarkRevision)error=e.message ?: "Bookmark could not be saved"}
            finally {if(generation==kudosGeneration) {bookmarkSending=false;accountPending.remove(draft.workId)}}
        }
    }
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
    fun saveOfficialDownload(id:String,item:OfficialDownload,uri:android.net.Uri,websiteCookie:String?=null,websiteAgent:String?=null)=load {
        officialDownloads=null
        withContext(Dispatchers.IO) {
            val session=if(websiteAgent!=null)websiteCookie to websiteAgent else archive.downloadSession(item,id)
            val bytes=OfficialDownloads.fetch(item,id,session.first,session.second)
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
    fun manageComment(id:String,action:String) {
        if(commentSending || commentUncertain)return
        val user=username ?: return
        if(action=="Edit"){
            val revision=++commentFormRevision
            load {val form=withContext(Dispatchers.IO){archive.editComment(id,user)};if(revision==commentFormRevision && username==user){commentForm=form;commentDraft=form.initialText;commentPseud=form.hidden["comment[pseud_id]"].orEmpty()}}
        }else {
            commentSending=true;error=null;val context=commentsWork
            viewModelScope.launch {try{val result=withContext(Dispatchers.IO){archive.manageComment(id,action,user)};if(username==user && commentsWork==context && screen=="comments"){commentUncertain=result.uncertain;if(result.error.isNotBlank())error=result.error else {notice=result.notice;if(action=="Delete" && commentsWork=="comment:$id"){commentForm=null;commentDraft="";val back=commentsReturn;navigate(back);if(back=="archiveList")showArchiveList(archiveKind,archiveId,archiveList?.page ?: 1)}else showComments(commentsWork,commentsPage?.page ?: 1)}}}catch(e:Exception){if(e is CancellationException)throw e;error=e.message}finally{commentSending=false}}
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
    init { if(ReaderFontStore.validId(preferences.fontFamily) && importedFonts.none { it.id==preferences.fontFamily })appearance(family="serif"); if(preferences.service=="https://translate.cutie.dating") { preferences=preferences.copy(service=""); storage.edit().putString("service","").putBoolean("translationConsented",false).apply() }; refreshLibrary(); if(storage.getBoolean("resumeReader",false)) libraryEntries.firstOrNull { it.fic.id==storage.getString("lastFic","") }?.let { openLibrary(it) }; if(checkSession) sessionJob = viewModelScope.launch { try { val confirmed=withContext(Dispatchers.IO) { archive.session() };if(username!=confirmed){accountListing=null;archiveList=null;entitySubscriptions.clear();ownWorks=false};username=confirmed } catch (_: Exception) { /* Browsing remains usable offline. */ } } }
    private fun load(block: suspend () -> Unit) {
        loadJob?.cancel(); val revision = ++requestRevision; error = null; loading = true
        loadJob = viewModelScope.launch {
            try { block() } catch (e: CancellationException) { throw e } catch (e: Exception) { error = e.message ?: "Request failed. Please try again." } finally { if (revision == requestRevision) loading = false }
        }
    }
    fun cancelLoad() { ++requestRevision; loadJob?.cancel(); loading=false }
    fun navigate(destination: String) {
        if (screen == "reader") saveProgress()
        if(screen=="archiveList" && destination==archiveReturn) {
            if(archiveTrail.isNotEmpty()){val previous=archiveTrail.removeAt(archiveTrail.lastIndex);showArchiveList(previous.first,previous.second,previous.third);return}
            archiveOrigin?.let{origin->selected=origin.fic;chapters=origin.chapters;chapter=origin.chapter;translation=origin.translation;detailsReturn=origin.detailsReturn;readerReturn=origin.readerReturn;if(chapters.isNotEmpty())restorePosition();readerRevision++};archiveOrigin=null
        }
        if(screen=="series" && destination==seriesReturn) {
            seriesOrigin?.let { origin ->
                selected=origin.fic;chapters=origin.chapters;chapter=origin.chapter;translation=origin.translation
                detailsReturn=origin.detailsReturn;readerReturn=origin.readerReturn
                if(chapters.isNotEmpty())restorePosition()
                readerRevision++
            }
            seriesOrigin=null
        }
        if(destination in listOf("home","search","library","login"))seriesOrigin=null
        if(destination=="home" || destination=="login"){archiveList=null;archiveKind="";archiveTrail.clear();archiveOrigin=null;archiveUncertain=false}
        if (destination != "details") officialDownloads=null
        if (destination != "reader") { formattedReading=false; ++requestRevision; loadJob?.cancel(); loading = false; cancelTranslation(); fullScreen = false; textOnly = false }
        error = null
        if (destination == "filters" && screen != "filters") filterDraft = filters
        if (destination == "library" || destination == "home") refreshLibrary()
        if (destination == "settings" && screen != "skins") settingsReturn = screen
        screen = destination
        storage.edit().putBoolean("resumeReader",destination=="reader").apply()
    }
    fun search(page: Int = 1, fresh: Boolean = true) {
        if (fresh) { appliedFilters = filters; appliedQuery = query.trim(); ownWorks = false; searchListing = null }
        screen = "search"
        val account = ownWorks; val mode=accountList; val user = username; val querySnapshot = appliedQuery; val filterSnapshot = appliedFilters
        load {
            val result = withContext(Dispatchers.IO) {
                if (account) {val name=user ?: error("Please sign in first");if(mode=="works")archive.works(name,page) else archive.accountReadings(name,page,mode=="later")}
                else archive.search(querySnapshot, page, filterSnapshot)
            }
            if(account) {if(username==user && ownWorks && accountList==mode)accountListing = result} else searchListing = result
        }
    }
    internal fun profilePictureBytes(user:String,url:String?):ByteArray? {val target=url?:archive.profile(user).avatarUrl;return if(target.isEmpty())null else archive.avatar(target)}
    fun showProfile(path: String) {
        val name = Regex("/users/([A-Za-z0-9_]+)(?:/|$)").find(path)?.groupValues?.get(1)
        if(name == null) { error = "This work has no public author profile."; return }
        if(screen != "details" || detailsReturn != "profile") profileReturn = screen
        profileUser = name; profile = null; profileListing = null; profileTab = "profile"; screen = "profile"
        subscriptionState("User",name)
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
        accountList="works";accountListing=null;ownWorks = true; search(1, false)
    }
    fun showAccountReadings(later:Boolean) {
        if(username==null){navigate("login");return}
        if(screen=="reader")saveProgress()
        cancelTranslation();accountList=if(later)"later" else "history";accountListing=null;ownWorks=true;search(1,false)
    }
    internal val entitySubscriptions=mutableStateMapOf<String,Boolean>()
    internal val entitySubscriptionPending=mutableStateMapOf<String,Boolean>()
    private val subscriptionRevisions=mutableMapOf<String,Long>()
    private fun subscriptionState(type:String,target:String) {
        val user=username ?: return;val key="$type:$target";if(entitySubscriptionPending[key]==true)return
        val revision=(subscriptionRevisions[key] ?: 0)+1;subscriptionRevisions[key]=revision
        viewModelScope.launch {try{val state=withContext(Dispatchers.IO){archive.entitySubscription(type,target,user)};if(user==username && subscriptionRevisions[key]==revision && state!=null)entitySubscriptions[key]=state}catch(e:Exception){if(e is CancellationException)throw e}}
    }
    fun setEntitySubscription(type:String,target:String,subscribed:Boolean) {
        val user=username ?: return;val key="$type:$target";if(entitySubscriptionPending[key]==true)return
        subscriptionRevisions[key]=(subscriptionRevisions[key] ?: 0)+1
        entitySubscriptionPending[key]=true
        viewModelScope.launch {try{val state=withContext(Dispatchers.IO){archive.entitySubscription(type,target,user,subscribed)};if(user==username){if(state!=null){entitySubscriptions[key]=state;notice="Subscription confirmed by AO3."}else error="AO3 does not offer a subscription here."}}catch(e:Exception){if(e is CancellationException)throw e;if(user==username){error=e.message;entitySubscriptions.remove(key)}}finally{entitySubscriptionPending.remove(key)}}
    }
    private var archiveOrigin:SeriesOrigin?=null
    private val archiveTrail=mutableListOf<Triple<String,String,Int>>()
    internal var archiveList by mutableStateOf<ArchivePage?>(null);private set
    internal var archiveKind="";private set
    internal var archiveId="";private set
    var archiveReturn="home";private set
    var archiveUncertain by mutableStateOf(false);private set
    fun showArchiveList(kind:String,id:String="",page:Int=1) {
        if(kind!="series" && username==null){navigate("login");return}
        if(screen!="archiveList"){archiveReturn=screen;archiveList=null;archiveTrail.clear();archiveOrigin=selected?.let{SeriesOrigin(it,chapters,chapter,translation,if(detailsReturn=="archiveList") "home" else detailsReturn,readerReturn)}}
        if(screen=="reader")saveProgress()
        archiveKind=kind;archiveId=id;archiveUncertain=false;navigate("archiveList")
        val user=username.orEmpty()
        load {val result=withContext(Dispatchers.IO){archive.archivePage(kind,user,id,page)};if(kind=="series" || username==user)archiveList=result}
        if(kind=="series")subscriptionState("Series",id)
    }
    internal fun actOnInboxSelection(ids:Set<String>,label:String) {
        if(loading || archiveUncertain || archiveKind!="inbox")return
        val user=username ?: return;val page=archiveList?.page ?: return
        load {try {val result=withContext(Dispatchers.IO){archive.archiveInboxBatch(user,page,ids,label)};if(username==user){archiveList=result;notice="Changes confirmed by AO3."}}catch(e:Exception){if(e is CancellationException)throw e;archiveUncertain=true;throw e}}
    }
    internal fun actOnArchiveEntry(entry:ArchiveEntry,label:String) {
        if(loading || archiveUncertain)return
        val user=username ?: return;val kind=archiveKind;val id=archiveId;val page=archiveList?.page ?: return
        load {
            try {val result=withContext(Dispatchers.IO){archive.archivePageAction(kind,user,id,page,entry.id,label)};if(username==user){archiveList=result;notice="Change confirmed by AO3."}}
            catch(e:Exception){if(e is CancellationException)throw e;archiveUncertain=true;throw e}
        }
    }
    fun openArchiveEntry(target:String) {
        when {
            target.matches(Regex("/works/[0-9]+"))->openListedWork(target.substringAfterLast('/'))
            target.matches(Regex("/series/[0-9]+"))->{archiveList?.let{archiveTrail+=Triple(archiveKind,archiveId,it.page)};showArchiveList("series",target.substringAfterLast('/'))}
            target.startsWith("/users/")->showProfile(target)
            target.matches(Regex("/comments/[0-9]+"))->showComments("comment:"+target.substringAfterLast('/'))
            target.isNotEmpty()->openWebsite(Archive.BASE+target)
        }
    }
    private fun openListedWork(id:String)=load {
        val fic=withContext(Dispatchers.IO){archive.workDetails(id)}
        officialDownloads=null;selected=fic;chapters=emptyList();translation=null;detailsReturn="archiveList";readerReturn="details";screen="details"
    }
    private data class SeriesOrigin(val fic:Fic,val chapters:List<Chapter>,val chapter:Int,val translation:String?,val detailsReturn:String,val readerReturn:String)
    private var seriesOrigin:SeriesOrigin?=null
    internal var workSeries by mutableStateOf<List<SeriesNavigation>?>(null);private set
    var seriesReturn="details";private set
    fun showSeries() {
        val fic=selected ?: return
        if(screen=="reader")saveProgress()
        seriesOrigin=SeriesOrigin(fic,chapters,chapter,translation,if(detailsReturn=="series") "home" else detailsReturn,readerReturn)
        seriesReturn=screen;workSeries=null;navigate("series")
        load {workSeries=withContext(Dispatchers.IO){archive.series(fic.id)}}
    }
    fun openSeriesWork(id:String) = load {
        val fic=withContext(Dispatchers.IO){archive.workDetails(id)}
        officialDownloads=null;translation=null;selected=fic;chapters=emptyList();detailsReturn="series";readerReturn="details";screen="details"
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
            restorePosition(); translation = null; formattedReading=false; screen = "reader"; fullScreen = false
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
    fun saveOffline(images:Boolean=false) {
        val fic = selected ?: return; val content = chapters
        if(content.isEmpty()) return
        load { val saved=withContext(Dispatchers.IO) { val context=currentCoroutineContext();val saved=if(images)cacheChapterImages(content){url->context.ensureActive();fetchReadingImage(url)} else content;context.ensureActive();library.saveOffline(fic, saved);saved }; if(selected?.id==fic.id)chapters=saved; refreshLibrary();notice="Saved offline" }
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
    fun closeFormattedReading() {formattedReading=false;restorePosition();readerRevision++}
    internal fun formattedProgress(fraction:Double) {
        if(!formattedReading || selected==null || chapter !in chapters.indices || !fraction.isFinite())return
        val text=chapters[chapter].text;bookOffset=(text.length*fraction.coerceIn(0.0,1.0)).toInt()
        positionIndex=paragraphStarts(text).indexOfLast{it<=bookOffset}.coerceAtLeast(0);positionOffset=0;saveProgress()
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
        ++kudosGeneration;kudosStates.clear();accountPending.clear();archiveBookmark=null;bookmarkSending=false;bookmarkUncertain=false;++bookmarkRevision
        sessionJob?.cancel()
        val confirmed=withContext(Dispatchers.IO) { archive.login(name.trim(), password) };accountListing=null;archiveList=null;entitySubscriptions.clear();ownWorks=false;username=confirmed
        screen = "home"
    }
    fun logout() { entitySubscriptions.clear();entitySubscriptionPending.clear();++kudosGeneration;kudosStates.clear();accountPending.clear();archiveBookmark=null;bookmarkSending=false;bookmarkUncertain=false;++bookmarkRevision;sessionJob?.cancel(); loadJob?.cancel(); archive.logout(); username = null; commentsPage=null; commentsWork="";commentForm=null;commentDraft="";commentEmail="";++commentFormRevision; searchListing = null; accountListing = null; archiveList=null;selected = null; chapters = emptyList(); ownWorks = false; navigate("home") }
    suspend fun suggestions(type: String, term: String) = withContext(Dispatchers.IO) { archive.suggest(type, term) }
    fun savePreferences(value: Preferences) {
        val code = value.language.trim().lowercase()
        if (!code.matches(Regex("[a-z]{2,3}(?:-[a-z]{2,4})?")) || (value.service.isNotBlank() && !validTranslationService(value.service.trim()))) { error = "Enter a language code and an HTTPS translation URL, or leave it blank. HTTP is allowed for localhost only."; return }
        if(preferences.service!=value.service.trim()) storage.edit().putBoolean("translationConsented",false).apply()
        preferences = value.copy(language = code, service = value.service.trim(), lineSpacing = value.lineSpacing.coerceIn(1.2f, 2.2f),margin=value.margin.coerceIn(0,64),alignment=validAlignment(value.alignment))
        storage.edit().putString("language", code).putBoolean("filterLanguage", value.filterLanguage).putString("service", value.service.trim()).putString("apiKey", value.apiKey).putInt("fontSize", value.fontSize).putString("theme", value.theme).putString("fontFamily", value.fontFamily).putFloat("lineSpacing", value.lineSpacing.coerceIn(1.2f, 2.2f)).putInt("margin",preferences.margin).putString("alignment",preferences.alignment).putBoolean("useImportedFontInInterface",preferences.useImportedFontInInterface).apply()
        filters = if (value.filterLanguage) filters + ("language_id" to listOf(code)) else filters - "language_id"
        cancelTranslation(); translation = null; error = null; screen = settingsReturn
        settingsConfirmation = true
    }
    fun appearance(theme: String = preferences.theme, family: String = preferences.fontFamily, spacing: Float = preferences.lineSpacing, margin: Int = preferences.margin, alignment: String = preferences.alignment) {
        if(theme!=preferences.theme) applySkin(null)
        preferences = preferences.copy(theme = theme, fontFamily = family, lineSpacing = spacing.coerceIn(1.2f, 2.2f),margin=margin.coerceIn(0,64),alignment=validAlignment(alignment))
        storage.edit().putString("theme", preferences.theme).putString("fontFamily", preferences.fontFamily).putFloat("lineSpacing", preferences.lineSpacing).putInt("margin",preferences.margin).putString("alignment",preferences.alignment).apply()
    }
    fun interfaceFont(enabled: Boolean) {
        preferences=preferences.copy(useImportedFontInInterface=enabled)
        storage.edit().putBoolean("useImportedFontInInterface",enabled).apply()
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
    override fun onCleared() {archive.cancelRequests();super.onCleared()}

}
