package org.ao3client.reader

import org.jsoup.Jsoup
import org.jsoup.nodes.Element
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder
import java.net.CookieManager
import java.net.CookiePolicy
import java.net.URI
import android.content.SharedPreferences
import org.json.JSONArray
import org.json.JSONObject

data class Fic(val id: String, val title: String, val author: String, val fandom: String = "", val summary: String = "", val tags: String = "", val stats: String = "", val language: String = "", val authorPath: String = "", val rating: String = "", val warnings: String = "")
data class AuthorProfile(val username: String, val title: String, val details: String, val biography: String)
data class Chapter(val title: String, val text: String, val notesBefore: String = "", val notesAfter: String = "", internal val styles: List<TextStyleSpan> = emptyList()) {
    fun exportText(): String = listOfNotNull(title,notesBefore.takeIf { it.isNotBlank() }?.let { "Author notes\n\n$it" },text,notesAfter.takeIf { it.isNotBlank() }?.let { "End notes\n\n$it" }).joinToString("\n\n")
    fun json(): JSONObject = JSONObject().put("title",title).put("text",text).put("notesBefore",notesBefore).put("notesAfter",notesAfter).put("styles",JSONArray().also { array -> styles.forEach { array.put(it.json()) } })
    companion object {
        fun fromJson(value: JSONObject): Chapter {
            val text=value.getString("text"); val styles=value.optJSONArray("styles") ?: JSONArray()
            return Chapter(value.optString("title"),text,value.optString("notesBefore"),value.optString("notesAfter"),(0 until styles.length()).mapNotNull { i ->
                styles.optJSONObject(i)?.let(TextStyleSpan::fromJson)?.takeIf { it.start>=0 && it.end>it.start && it.end<=text.length }
            })
        }
    }
}
data class Listing(val works: List<Fic>, val page: Int, val pages: Int, val total: String)
data class Choice(val label: String, val value: String)
data class FilterField(val key: String, val label: String, val hint: String = "", val suggestion: String? = null, val choices: List<Choice> = emptyList(), val multi: Boolean = false)

object Catalogue {
    private fun choices(vararg pairs: Pair<String,String>) = pairs.map { Choice(it.first, it.second) }
    val ratings = choices("Not rated" to "9", "General audiences" to "10", "Teen and up" to "11", "Mature" to "12", "Explicit" to "13")
    val warnings = choices("Choose not to warn" to "14", "No warnings apply" to "16", "Graphic violence" to "17", "Major character death" to "18", "Rape / non-con" to "19", "Underage" to "20")
    val categories = choices("F/F" to "116", "F/M" to "22", "Gen" to "21", "M/M" to "23", "Multi" to "2246", "Other" to "24")
    val languages = choices("English" to "en", "Eesti" to "et", "Español" to "es", "Français" to "fr", "Deutsch" to "de", "Português" to "pt", "Italiano" to "it", "Русский" to "ru", "Українська" to "uk", "Polski" to "pl", "日本語" to "ja", "中文" to "zh", "한국어" to "ko", "Suomi" to "fi", "Svenska" to "sv", "Nederlands" to "nl", "Türkçe" to "tr", "العربية" to "ar")
    val fields = listOf(
        FilterField("title", "Title"), FilterField("creators", "Author"),
        FilterField("fandom_names", "Fandoms", "Separate names with commas", "fandom"),
        FilterField("character_names", "Characters", "Separate names with commas", "character"),
        FilterField("relationship_names", "Relationships", "Separate names with commas", "relationship"),
        FilterField("freeform_names", "Additional tags", "Separate tags with commas", "freeform"),
        FilterField("rating_ids", "Rating", choices = ratings),
        FilterField("archive_warning_ids[]", "Include warnings", choices = warnings, multi = true),
        FilterField("category_ids[]", "Categories", choices = categories, multi = true),
        FilterField("language_id", "Language", "Any AO3 language code, e.g. en or et"),
        FilterField("complete", "Completion", choices = choices("Complete" to "T", "In progress" to "F")),
        FilterField("crossover", "Crossovers", choices = choices("Include only" to "T", "Exclude" to "F")),
        FilterField("single_chapter", "Single chapter", choices = choices("Only single-chapter works" to "1")),
        FilterField("word_count", "Word count", "e.g. 1000-50000 or >10000"),
        FilterField("kudos_count", "Kudos", "e.g. >100"), FilterField("hits", "Hits", "e.g. >1000"),
        FilterField("comments_count", "Comments", "e.g. >10"), FilterField("bookmarks_count", "Bookmarks", "e.g. >50"),
        FilterField("revised_at", "Date updated", "e.g. >2025-01-01 or 1 week ago"),
        FilterField("sort_column", "Sort by", choices = choices("Best match" to "_score", "Date updated" to "revised_at", "Date posted" to "created_at", "Title" to "title_to_sort_on", "Author" to "authors_to_sort_on", "Words" to "word_count", "Kudos" to "kudos_count", "Hits" to "hits", "Comments" to "comments_count", "Bookmarks" to "bookmarks_count")),
        FilterField("sort_direction", "Sort direction", choices = choices("Descending" to "desc", "Ascending" to "asc")),
        FilterField("excluded_tag_names", "Exclude tags", "Separate tags with commas", "tag"),
        FilterField("excluded_rating_ids[]", "Exclude ratings", choices = ratings, multi = true),
        FilterField("excluded_warning_ids[]", "Exclude warnings", choices = warnings, multi = true),
        FilterField("excluded_category_ids[]", "Exclude categories", choices = categories, multi = true)
    )
}

object ArchiveParser {
    fun listing(html: String, page: Int): Listing {
        val doc = Jsoup.parse(html)
        val works = doc.select("li.work[id^=work_], li.bookmark[id^=bookmark_]").mapNotNull { item ->
            val link = item.selectFirst("h4.heading a[href^=/works/]") ?: return@mapNotNull null
            val workId = Regex("^/works/(\\d+)$").find(link.attr("href"))?.groupValues?.get(1) ?: return@mapNotNull null
            Fic(workId, link?.text().orEmpty(), item.select("a[rel=author]").text().ifBlank { "Anonymous" }, item.select(".fandoms a.tag").text(), item.selectFirst("blockquote.summary")?.text().orEmpty(), item.select("ul.tags a.tag").joinToString(" · ") { it.text() }, item.selectFirst("dl.stats")?.text().orEmpty(), item.select("dd.language").text(), item.selectFirst("a[rel=author]")?.attr("href").orEmpty(), item.selectFirst(".required-tags .rating")?.attr("title").orEmpty(), item.selectFirst(".required-tags .warnings")?.attr("title").orEmpty())
        }.distinctBy { it.id }
        val pages = doc.select(".pagination a").mapNotNull { Regex("(?:[?&])page=(\\d+)").find(it.attr("href"))?.groupValues?.get(1)?.toIntOrNull() }.plus(page).maxOrNull() ?: 1
        if (works.isEmpty() && doc.select("ol.work, ol.bookmark").isEmpty() && !doc.text().contains("No results found", true) && !doc.text().contains("0 Works", true)) error("AO3 did not return a work listing. Try again later.")
        return Listing(works, page, pages, doc.select("h3.heading").firstOrNull { it.text().contains("Found", true) || it.text().contains("Works", true) }?.text().orEmpty())
    }
    fun profile(html: String, username: String): AuthorProfile {
        val root = Jsoup.parse(html).selectFirst("div.user.profile") ?: error("AO3 did not return this user's public profile.")
        return AuthorProfile(username, root.selectFirst("h3.heading")?.text().orEmpty(), root.selectFirst("dl.meta")?.text().orEmpty(), root.selectFirst(".bio blockquote.userstuff")?.text().orEmpty())
    }
    private fun readableText(element: Element): String = readingText(element).text
    fun chapters(html: String): List<Chapter> {
        val doc = Jsoup.parse(html)
        val bodies = doc.select("#chapters .userstuff").filter { el ->
            el.parents().none { it.hasClass("preface") || it.hasClass("afterword") || it.hasClass("notes") || it.hasClass("summary") || it.hasClass("userstuff") }
        }.filter { readableText(it).isNotBlank() }
        if (bodies.isEmpty()) error("No readable chapters were returned. The work may need sign-in or be unavailable.")
        fun chapterRoot(el: Element) = el.parents().firstOrNull { it.hasClass("chapter") && !it.hasClass("preface") }
        val order = doc.getAllElements().withIndex().associate { it.value to it.index }
        val before = Array(bodies.size) { mutableListOf<String>() }
        val after = Array(bodies.size) { mutableListOf<String>() }
        doc.select(".notes .userstuff").forEach { note ->
            if (note.parents().any { it.hasClass("userstuff") }) return@forEach
            val root = chapterRoot(note)
            val target = if (root != null) bodies.indexOfFirst { chapterRoot(it) === root }
                else if (order.getValue(note) < order.getValue(bodies.first())) 0 else bodies.lastIndex
            if (target < 0) return@forEach
            val content = readableText(note)
            if (content.isNotBlank()) {
                if (order.getValue(note) < order.getValue(bodies[target])) before[target].add(content) else after[target].add(content)
            }
        }
        return bodies.mapIndexed { i, el ->
            val title = chapterRoot(el)?.selectFirst("h3.title")?.text()
            val content=readingText(el)
            Chapter(title?.ifBlank { "Chapter ${i + 1}" } ?: "Chapter ${i + 1}", content.text, before[i].joinToString("\n\n"), after[i].joinToString("\n\n"),content.styles)
        }
    }
    fun username(html: String): String? {
        val doc=Jsoup.parse(html)
        fun trusted(node:Element)=node.parents().none {it.id()=="workskin" || it.hasClass("userstuff")}
        fun path(value:String):String?=runCatching {val u=URI(Archive.BASE).resolve(value);u.path.takeIf {u.scheme=="https" && u.host=="archiveofourown.org" && u.port in listOf(-1,443) && u.userInfo==null}}.getOrNull()
        val logout=doc.select("a[href], form[action]").any {trusted(it) && path(it.attr(if(it.tagName()=="form") "action" else "href"))=="/users/logout"}
        if(!logout)return null
        return doc.select("#greeting a[href]").filter {trusted(it)}.mapNotNull {path(it.attr("href"))?.let {p->Regex("/users/([A-Za-z0-9_]+)").matchEntire(p)?.groupValues?.get(1)}}.firstOrNull {it !in listOf("login","logout")}
    }

    fun sessionIdentity(html: String): String? {
        username(html)?.takeIf { it.matches(Regex("[A-Za-z0-9_]+")) }?.let { return it }
        val doc = Jsoup.parse(html)
        if (doc.select("a[href='/users/login'], form[action='/users/login']").isNotEmpty()) return null
        error("Could not verify session. Try again when AO3 is available.")
    }
}

class Archive(private val prefs: SharedPreferences,private val websiteProbe:((CookieManager,String)->String)?=null) {
    private val cookieLock = Any()
    @Volatile private var cookieGeneration = 0
    private val cookies = CookieManager(null, CookiePolicy.ACCEPT_ORIGINAL_SERVER)
    init {
        try {
            val saved = JSONArray(prefs.getString("cookies", "[]"))
            for (i in 0 until saved.length()) {
                val entry = saved.getJSONObject(i)
                cookies.put(URI("https://archiveofourown.org/"), mapOf("Set-Cookie" to listOf(entry.getString("value"))))
            }
        } catch (_: Exception) { prefs.edit().remove("cookies").apply() }
    }
    companion object {
        const val BASE = "https://archiveofourown.org"
        fun encode(value: String): String = URLEncoder.encode(value, "UTF-8")
        fun searchPath(query: String, page: Int, filters: Map<String,List<String>>): String {
            val args = mutableListOf("work_search[query]" to query, "page" to "$page")
            val excluded = mutableSetOf<String>()
            filters.forEach { (key, values) ->
                val target = if(key in listOf("excluded_rating_ids[]", "excluded_warning_ids[]", "excluded_category_ids[]")) "excluded_tag_ids[]" else key
                values.filter { it.isNotBlank() }.forEach {
                    if(target != "excluded_tag_ids[]" || excluded.add(it)) args += "work_search[${target.removeSuffix("[]")}]${if (target.endsWith("[]")) "[]" else ""}" to it
                }
            }
            return "/works/search?" + args.joinToString("&") { encode(it.first) + "=" + encode(it.second) }
        }
    }
    private fun request(path: String, form: Map<String,String>? = null, expectedGeneration:Int? = null, referer:String? = null, cookieJar:CookieManager=cookies, userAgent:String?=null, persist:Boolean=true): String {
        val generation = expectedGeneration ?: cookieGeneration
        var url = URL(BASE + path)
        var post = form
        repeat(6) {
            if(generation != cookieGeneration) throw java.util.concurrent.CancellationException("Session changed")
            require(url.protocol == "https" && url.host == "archiveofourown.org" && url.port in listOf(-1,443) && url.userInfo==null) { "Unexpected redirect from AO3" }
            val connection = url.openConnection() as HttpURLConnection
            try {
                connection.instanceFollowRedirects = false
                connection.useCaches = false
                connection.setRequestProperty("Cache-Control", "no-cache")
                connection.connectTimeout = 15000; connection.readTimeout = 30000
                connection.setRequestProperty("User-Agent", userAgent ?: prefs.getString("networkUserAgent",null) ?: "AO3Client/3.0 (Unofficial Android Reader)")
                connection.setRequestProperty("Accept", if(path.startsWith("/autocomplete/")) "application/json" else "text/html")
                val cookieHeader = synchronized(cookieLock) {
                    if(generation!=cookieGeneration) throw java.util.concurrent.CancellationException("Session changed")
                    cookieJar.get(url.toURI(), emptyMap())["Cookie"]
                }
                if (!cookieHeader.isNullOrEmpty()) connection.setRequestProperty("Cookie", cookieHeader.joinToString("; "))
                if(referer!=null)connection.setRequestProperty("Referer",BASE+referer)
                post?.let { values ->
                    connection.requestMethod = "POST"; connection.doOutput = true
                    connection.setRequestProperty("Content-Type", "application/x-www-form-urlencoded")
                    connection.outputStream.use { out -> out.write(values.entries.joinToString("&") { encode(it.key) + "=" + encode(it.value) }.toByteArray()) }
                }
                val status = connection.responseCode
                synchronized(cookieLock) {
                if(generation != cookieGeneration) throw java.util.concurrent.CancellationException("Session changed")
                cookieJar.put(url.toURI(), connection.headerFields.filterKeys { it != null })
                if(persist)persistCookies()

                }
                if (status in 300..399) {
                    url = URL(url, connection.getHeaderField("Location") ?: error("Missing redirect location"))
                    if (status != 307 && status != 308) post = null
                } else {
                    val body = (if (status >= 400) connection.errorStream else connection.inputStream)?.bufferedReader()?.use { it.readText() }.orEmpty()
                    if (status >= 400) {
                        if (body.contains("archive is down for maintenance", true)) error("AO3 is down for maintenance (HTTP $status). Try again later.")
                        error("AO3 returned HTTP $status. Please try again later.")
                    }
                    return body
                }
            } finally { connection.disconnect() }
        }
        error("AO3 redirected too many times")
    }
    fun search(query: String, page: Int, filters: Map<String,List<String>>) = ArchiveParser.listing(request(searchPath(query, page, filters)), page)
    fun profile(user: String): AuthorProfile { require(user.matches(Regex("[A-Za-z0-9_]+"))); return ArchiveParser.profile(request("/users/${encode(user)}/profile"), user) }
    fun authorWorks(user: String, page: Int, bookmarks: Boolean): Listing { require(user.matches(Regex("[A-Za-z0-9_]+"))); return ArchiveParser.listing(request("/users/${encode(user)}/${if(bookmarks) "bookmarks" else "works"}?page=$page"), page) }
    fun works(user: String, page: Int) = ArchiveParser.listing(request("/users/${encode(user)}/works?page=$page"), page)
    fun read(id: String): Pair<List<Chapter>,String> {
        require(id.matches(Regex("\\d+")))
        val html = request("/works/$id?view_adult=true&view_full_work=true")
        return ArchiveParser.chapters(html) to html
    }
    fun downloadSession(item:OfficialDownload,id:String):Pair<String?,String> = synchronized(cookieLock) {
        require(OfficialDownloads.format(item.url,id)==item.format) {"Invalid AO3 download link"}
        cookies.get(URI(item.url),emptyMap())["Cookie"]?.joinToString("; ") to (prefs.getString("networkUserAgent",null) ?: "AO3Client/3.0 (Unofficial Android Reader)")
    }
    fun downloads(id:String):List<OfficialDownload> {
        require(id.matches(Regex("[0-9]+")))
        return OfficialDownloads.parse(request("/works/$id?view_adult=true"),id)
    }
    fun archiveBookmark(id:String,expected:String):ArchiveBookmarkForm {
        require(id.matches(Regex("[0-9]+")))
        val html=request("/works/$id?view_adult=true")
        require(ArchiveParser.sessionIdentity(html)==expected) {"Your AO3 session changed. Sign in again."}
        return ArchiveBookmarks.form(html,id)
    }
    fun saveArchiveBookmark(draft:ArchiveBookmarkForm,deleting:Boolean,expected:String):ArchiveFormResult {
        val generation=cookieGeneration
        val fresh=archiveBookmark(draft.workId,expected)
        require(fresh.id==draft.id) {"Your bookmark changed. Reload it before saving."}
        require(fresh.pseuds.any {it.first==draft.pseud}) {"This pseud is unavailable. Reload the bookmark."}
        val fields=if(deleting) {
            require(fresh.id.isNotBlank()) {"No AO3 bookmark to delete"}
            val html=request("/bookmarks/${fresh.id}/confirm_delete",expectedGeneration=generation)
            require(ArchiveParser.sessionIdentity(html)==expected) {"Your AO3 session changed. Sign in again."}
            ArchiveBookmarks.deleteForm(html,fresh.id)
        } else ArchiveBookmarks.fields(fresh,draft)
        if(generation!=cookieGeneration)throw java.util.concurrent.CancellationException("Session changed")
        return try {
            val html=request(if(deleting) "/bookmarks/${fresh.id}" else fresh.path,fields,generation,"/works/${fresh.workId}?view_adult=true")
            val unknown=ArchiveFormResult("Could not confirm the change. Check your AO3 bookmarks before trying again.",uncertain=true)
            val result=if(ArchiveParser.sessionIdentity(html)==expected)ArchiveBookmarks.result(html) else unknown
            if(!result.success)result
            else if(deleting) {if(archiveBookmark(draft.workId,expected).id.isEmpty())result else unknown}
            else if(ArchiveBookmarks.record(html,draft.workId,draft.id))result else unknown
        } catch(e:java.util.concurrent.CancellationException) {throw e}
          catch(_:Exception) {ArchiveFormResult("Could not confirm the change. Check your AO3 bookmarks before trying again.",uncertain=true)}
    }
    fun setWorkSubscribed(id:String,subscribed:Boolean,expected:String):String {
        require(id.matches(Regex("[0-9]+")))
        val generation=cookieGeneration
        val path="/works/$id?view_adult=true"
        val html=request(path)
        require(ArchiveParser.sessionIdentity(html)==expected) {"Your AO3 session changed. Sign in again."}
        val form=Subscriptions.form(html,id,expected) ?: error("Subscription unavailable. Check your AO3 sign-in and access to this work.")
        val success=if(subscribed) "Subscribed to this work on AO3" else "Unsubscribed from this work on AO3"
        if(generation!=cookieGeneration)throw java.util.concurrent.CancellationException("Session changed")
        if(form.subscribed==subscribed)return success
        return try {
            val result=request(form.path,form.fields,generation,path)
            val updated=Subscriptions.form(result,id,expected)
            if(ArchiveParser.sessionIdentity(result)==expected && updated?.subscribed==subscribed)success
            else "Could not confirm the change. Check your subscriptions on AO3."
        } catch(e:java.util.concurrent.CancellationException) {throw e}
          catch(_:Exception) {"Could not confirm the change. Check your subscriptions on AO3."}
    }
    fun setMarkedForLater(id:String,later:Boolean,expected:String):String {
        require(id.matches(Regex("[0-9]+")))
        val generation=cookieGeneration
        val path="/works/$id?view_adult=true"
        val html=request(path)
        require(ArchiveParser.sessionIdentity(html)==expected) { "Your AO3 session changed. Sign in again." }
        val form=ReadingActions.form(html,id) ?: error("Reading history is unavailable. Check your AO3 history preferences.")
        val action=if(later) "mark_for_later" else "mark_as_read"
        val success=if(later) "Marked for later on AO3" else "Marked as read on AO3"
        if(generation!=cookieGeneration)throw java.util.concurrent.CancellationException("Session changed")
        if(form.action!=action)return success
        return try {
            val result=request("/works/$id/$action",form.fields,generation,path)
            val updated=ReadingActions.form(result,id)
            if(ArchiveParser.sessionIdentity(result)==expected && updated!=null && updated.action!=action)success
            else "Could not confirm the change. Check your Marked for Later list on AO3."
        } catch(e:java.util.concurrent.CancellationException) {throw e}
          catch(_:Exception) {"Could not confirm the change. Check your Marked for Later list on AO3."}
    }
    fun giveKudos(id:String,expected:String?):KudosResult {
        require(id.matches(Regex("[0-9]+")))
        val generation=cookieGeneration
        val html=request("/works/$id?view_adult=true")
        require(ArchiveParser.sessionIdentity(html)==expected) { "Your AO3 session changed. Sign in again before giving kudos." }
        val fields=KudosParser.form(html,id)
        if(generation!=cookieGeneration) throw java.util.concurrent.CancellationException("Session changed")
        return try { KudosParser.result(request("/kudos",fields,generation)) }
            catch(e:java.util.concurrent.CancellationException) { throw e }
            catch(_:Exception) { KudosResult("Could not confirm kudos. Check this work on AO3.",uncertain=true) }
    }
    fun rememberedUsername(): String? = synchronized(cookieLock) {
        prefs.getString("sessionUsername", null)?.takeIf { cookies.cookieStore.cookies.any { cookie -> !cookie.hasExpired() } }
    }
    fun commentForm(workId:String,replyId:String=""):CommentForm {
        require(workId.matches(Regex("\\d+")) && (replyId.isEmpty() || replyId.matches(Regex("\\d+"))))
        val path=if(replyId.isEmpty()) "/works/$workId/comments/new" else "/comments/$replyId/comments/new"
        return parseCommentForm(request(path)).copy(context=replyId)
    }
    fun postComment(form:CommentForm,text:String,pseud:String="",guestName:String="",guestEmail:String=""):CommentSubmission {
        require(validCommentAction(form.action) && text.isNotBlank() && !form.hidden["authenticity_token"].isNullOrBlank())
        val fields=form.hidden.toMutableMap();fields["comment[comment_content]"]=text
        if(pseud.isNotBlank()) { require(form.pseuds.any { it.first==pseud });fields["comment[pseud_id]"]=pseud }
        if(form.guest) { require(guestName.isNotBlank() && guestEmail.isNotBlank());fields["comment[name]"]=guestName.trim();fields["comment[email]"]=guestEmail.trim() }
        val uri=URI(BASE).resolve(form.action);val path=uri.rawPath+(uri.rawQuery?.let { "?$it" } ?: "")
        return parseCommentSubmission(request(path,fields))
    }
    fun comments(workId:String,page:Int=1):CommentPage {
        require(workId.matches(Regex("\\d+")) && page>0)
        return CommentParser.parse(request("/works/$workId/comments?page=$page"),page)
    }
    private fun persistCookies(user:String?=null,agent:String?=null) {
        val saved=JSONArray()
        cookies.cookieStore.cookies.filterNot {it.hasExpired()}.forEach {cookie ->
            val attributes=cookie.toString()+"; Domain=${cookie.domain ?: "archiveofourown.org"}; Path=${cookie.path ?: "/"}"+(if(cookie.secure) "; Secure" else "")+(if(cookie.isHttpOnly) "; HttpOnly" else "")+(if(cookie.maxAge>=0) "; Max-Age=${cookie.maxAge}" else "")
            saved.put(JSONObject().put("value",attributes))
        }
        val editor=prefs.edit().putString("cookies",saved.toString())
        if(user!=null)editor.putString("sessionUsername",user)
        if(agent!=null)editor.putString("networkUserAgent",agent)
        editor.apply()
    }
    fun useWebsiteSession(header:String,agent:String,expected:String):String {
        require(validWebsiteSessionIdentity(expected,agent)) {"Invalid website session"}
        val generation=cookieGeneration
        val staging=CookieManager(null,CookiePolicy.ACCEPT_ORIGINAL_SERVER)
        val incoming=websiteSessionCookies(header);require(incoming.isNotEmpty()) {"Sign in on the AO3 website first"}
        incoming.forEach {staging.cookieStore.add(URI(BASE),it)}
        val html=websiteProbe?.invoke(staging,agent) ?: request("/",expectedGeneration=generation,cookieJar=staging,userAgent=agent,persist=false)
        require(ArchiveParser.sessionIdentity(html)==expected) {"AO3 could not verify this website session for the native reader. Your existing reader sign-in was kept."}
        synchronized(cookieLock) {
            if(generation!=cookieGeneration)throw java.util.concurrent.CancellationException("Session changed")
            cookieGeneration++;cookies.cookieStore.removeAll()
            staging.cookieStore.cookies.filter { !it.hasExpired() && it.domain.orEmpty().removePrefix(".").equals("archiveofourown.org",true) }.forEach {cookies.cookieStore.add(URI(BASE),it)}
            persistCookies(expected,agent)
        }
        return expected
    }
    fun session(): String? {
        val generation = cookieGeneration
        val user = ArchiveParser.sessionIdentity(request("/"))
        synchronized(cookieLock) {
            if (generation != cookieGeneration) throw java.util.concurrent.CancellationException("Session changed")
            if (user == null) prefs.edit().remove("sessionUsername").apply()
            else prefs.edit().putString("sessionUsername", user).apply()
        }
        return user
    }
    fun login(name: String, password: String): String {
        synchronized(cookieLock) { cookieGeneration++ }
        val token = Jsoup.parse(request("/users/login")).selectFirst("input[name=authenticity_token]")?.attr("value") ?: error("AO3 did not return a sign-in form")
        request("/users/login", mapOf("authenticity_token" to token, "user[login]" to name, "user[password]" to password, "user[remember_me]" to "1", "commit" to "Log in"))
        return session() ?: error("Sign-in failed. Check your username and password.")
    }
    fun cancelRequests() = synchronized(cookieLock) {cookieGeneration++}
    fun logout() = synchronized(cookieLock) { cookieGeneration++; cookies.cookieStore.removeAll(); prefs.edit().remove("cookies").remove("sessionUsername").apply() }
    fun suggest(type: String, term: String): List<String> {
        require(type in listOf("tag", "fandom", "character", "relationship", "freeform"))
        val array = JSONArray(request("/autocomplete/$type?term=${encode(term)}"))
        return (0 until array.length()).map { i -> if (array.get(i) is JSONObject) array.getJSONObject(i).optString("name") else array.getString(i) }.filter { it.isNotBlank() }.distinct().take(6)
    }
}
