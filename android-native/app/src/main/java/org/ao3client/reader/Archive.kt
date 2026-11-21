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
data class Chapter(val title: String, val text: String)
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
    fun chapters(html: String): List<Chapter> {
        val doc = Jsoup.parse(html)
        val result = doc.select("#chapters .userstuff").filter { el ->
            el.parents().none { it.hasClass("preface") || it.hasClass("afterword") || it.hasClass("notes") || it.hasClass("summary") || it.hasClass("userstuff") }
        }.mapIndexed { i, el ->
            val body = el.clone()
            body.select(".landmark, .hidden").remove()
            body.select("br").forEach { it.after("\n") }
            body.select("p, div, h1, h2, h3, h4, blockquote, li").forEach { it.appendText("\n\n") }
            val title = el.parents().firstOrNull { it.hasClass("chapter") }?.selectFirst("h3.title")?.text()
            Chapter(title?.ifBlank { "Chapter ${i + 1}" } ?: "Chapter ${i + 1}", body.wholeText().trim())
        }.filter { it.text.isNotBlank() }
        if (result.isEmpty()) error("No readable chapters were returned. The work may need sign-in or be unavailable.")
        return result
    }
    fun username(html: String): String? {
        val doc = Jsoup.parse(html)
        if (doc.select("a[href*=\"/users/logout\"], form[action*=\"/users/logout\"]").isEmpty()) return null
        return doc.selectFirst("#greeting a[href^=/users/]")?.attr("href")?.substringAfter("/users/")?.substringBefore('/')?.substringBefore('?')
    }

    fun sessionIdentity(html: String): String? {
        username(html)?.takeIf { it.matches(Regex("[A-Za-z0-9_]+")) }?.let { return it }
        val doc = Jsoup.parse(html)
        if (doc.select("a[href='/users/login'], form[action='/users/login']").isNotEmpty()) return null
        error("Could not verify session. Try again when AO3 is available.")
    }
}

class Archive(private val prefs: SharedPreferences) {
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
    private fun request(path: String, form: Map<String,String>? = null): String {
        val generation = cookieGeneration
        var url = URL(BASE + path)
        var post = form
        repeat(6) {
            if(generation != cookieGeneration) throw java.util.concurrent.CancellationException("Session changed")
            require(url.protocol == "https" && url.host == "archiveofourown.org") { "Unexpected redirect from AO3" }
            val connection = url.openConnection() as HttpURLConnection
            try {
                connection.instanceFollowRedirects = false
                connection.useCaches = false
                connection.setRequestProperty("Cache-Control", "no-cache")
                connection.connectTimeout = 15000; connection.readTimeout = 30000
                connection.setRequestProperty("User-Agent", "AO3Client/3.0 (Unofficial Android Reader)")
                connection.setRequestProperty("Accept", if(path.startsWith("/autocomplete/")) "application/json" else "text/html")
                val cookieHeader = cookies.get(url.toURI(), emptyMap())["Cookie"]
                if (!cookieHeader.isNullOrEmpty()) connection.setRequestProperty("Cookie", cookieHeader.joinToString("; "))
                post?.let { values ->
                    connection.requestMethod = "POST"; connection.doOutput = true
                    connection.setRequestProperty("Content-Type", "application/x-www-form-urlencoded")
                    connection.outputStream.use { out -> out.write(values.entries.joinToString("&") { encode(it.key) + "=" + encode(it.value) }.toByteArray()) }
                }
                val status = connection.responseCode
                synchronized(cookieLock) {
                if(generation != cookieGeneration) throw java.util.concurrent.CancellationException("Session changed")
                cookies.put(url.toURI(), connection.headerFields.filterKeys { it != null })
                val saved = JSONArray()
                cookies.cookieStore.cookies.filterNot { it.hasExpired() }.forEach { cookie ->
                    val attributes = cookie.toString() + "; Domain=${cookie.domain ?: "archiveofourown.org"}; Path=${cookie.path ?: "/"}" + (if (cookie.secure) "; Secure" else "") + (if (cookie.maxAge >= 0) "; Max-Age=${cookie.maxAge}" else "")
                    saved.put(JSONObject().put("value", attributes))
                }
                prefs.edit().putString("cookies", saved.toString()).apply()
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
    fun rememberedUsername(): String? = synchronized(cookieLock) {
        prefs.getString("sessionUsername", null)?.takeIf { cookies.cookieStore.cookies.any { cookie -> !cookie.hasExpired() } }
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
        val token = Jsoup.parse(request("/users/login")).selectFirst("input[name=authenticity_token]")?.attr("value") ?: error("AO3 did not return a sign-in form")
        request("/users/login", mapOf("authenticity_token" to token, "user[login]" to name, "user[password]" to password, "user[remember_me]" to "1", "commit" to "Log in"))
        return session() ?: error("Sign-in failed. Check your username and password.")
    }
    fun logout() = synchronized(cookieLock) { cookieGeneration++; cookies.cookieStore.removeAll(); prefs.edit().remove("cookies").remove("sessionUsername").apply() }
    fun suggest(type: String, term: String): List<String> {
        require(type in listOf("tag", "fandom", "character", "relationship", "freeform"))
        val array = JSONArray(request("/autocomplete/$type?term=${encode(term)}"))
        return (0 until array.length()).map { i -> if (array.get(i) is JSONObject) array.getJSONObject(i).optString("name") else array.getString(i) }.filter { it.isNotBlank() }.distinct().take(6)
    }
}
