package org.ao3client.reader

import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.nio.file.AtomicMoveNotSupportedException
import java.util.UUID

data class LibraryEntry(val fic: Fic, val openedAt: Long, val offline: Boolean, val favorite: Boolean)
data class ReadingBookmark(val id: String, val title: String, val chapter: Int, val index: Int, val offset: Int, val bookOffset: Int = -1, val fraction: Double = -1.0)

class ReadingLibrary(private val root: File) {
    private fun id(value: String): String { require(value.matches(Regex("\\d+"))) { "Invalid fic ID" }; return value }
    private fun ficJson(fic: Fic) = JSONObject().put("id", fic.id).put("title", fic.title).put("author", fic.author).put("fandom", fic.fandom).put("summary", fic.summary).put("tags", fic.tags).put("stats", fic.stats).put("language", fic.language).put("authorPath", fic.authorPath).put("rating",fic.rating).put("warnings",fic.warnings)
    private fun fic(json: JSONObject) = Fic(json.getString("id"), json.optString("title"), json.optString("author"), json.optString("fandom"), json.optString("summary"), json.optString("tags"), json.optString("stats"), json.optString("language"), json.optString("authorPath"),json.optString("rating"),json.optString("warnings"))
    private fun array(name: String): JSONArray {
        val file=File(root,name); if(!file.exists()) return JSONArray()
        return try {
            val result=JSONArray(file.readText())
            if(name=="index.json") for(i in 0 until result.length()) id(result.getJSONObject(i).getJSONObject("fic").getString("id"))
            result
        } catch(e:Exception) {
            check(file.renameTo(File(root,"$name.corrupt-${System.currentTimeMillis()}"))) { "Library data is damaged and could not be preserved" }
            if(name!="index.json") return JSONArray()
            val previous=File(root,"index.previous.json")
            runCatching { if(previous.exists()) { val recovered=JSONArray(previous.readText()); for(i in 0 until recovered.length()) id(recovered.getJSONObject(i).getJSONObject("fic").getString("id")); write("index.json",recovered.toString()); return recovered } }
            val recovered=JSONArray()
            root.listFiles().orEmpty().filter { it.name.matches(Regex("\\d+\\.json")) }.forEach { saved -> runCatching { val fic=JSONObject(saved.readText()).getJSONObject("fic"); id(fic.getString("id")); recovered.put(JSONObject().put("fic",fic).put("favorite",false)) } }
            write("index.json",recovered.toString()); recovered
        }
    }
    private fun write(name: String, data: String) {
        check(root.isDirectory || root.mkdirs()) { "Could not create reading library" }
        val destination = File(root, name); val temp = File(root, ".$name.tmp")
        if(name=="index.json" && destination.exists()) runCatching { val previous=destination.readText(); JSONArray(previous); write("index.previous.json",previous) }
        try {
            FileOutputStream(temp).use { stream -> stream.write(data.toByteArray(Charsets.UTF_8)); stream.fd.sync() }
            try { Files.move(temp.toPath(), destination.toPath(), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING) }
            catch (_: AtomicMoveNotSupportedException) { Files.move(temp.toPath(), destination.toPath(), StandardCopyOption.REPLACE_EXISTING) }
        } finally { temp.delete() }
    }
    @Synchronized fun entries(): List<LibraryEntry> {
        val data = array("index.json")
        return (0 until data.length()).map { i -> val entry = data.getJSONObject(i); val book = fic(entry.getJSONObject("fic")); LibraryEntry(book, entry.optLong("openedAt"), File(root, "${id(book.id)}.json").exists(), entry.optBoolean("favorite")) }.sortedByDescending { it.openedAt }
    }
    private fun writeEntries(entries: List<LibraryEntry>) {
        val array = JSONArray(); entries.forEach { array.put(JSONObject().put("fic", ficJson(it.fic)).put("openedAt", it.openedAt).put("favorite", it.favorite)) }; write("index.json", array.toString())
    }
    @Synchronized fun remember(fic: Fic) {
        id(fic.id); val previous = entries(); val favorite = previous.find { it.fic.id == fic.id }?.favorite ?: false
        writeEntries(listOf(LibraryEntry(fic, System.currentTimeMillis(), File(root, "${fic.id}.json").exists(), favorite)) + previous.filterNot { it.fic.id == fic.id })
    }
    @Synchronized fun saveOffline(fic: Fic, chapters: List<Chapter>) {
        id(fic.id); require(chapters.isNotEmpty())
        val content = JSONArray(); chapters.forEach { content.put(JSONObject().put("title", it.title).put("text", it.text)) }
        write("${fic.id}.json", JSONObject().put("fic", ficJson(fic)).put("chapters", content).toString()); remember(fic)
    }
    @Synchronized fun load(ficId: String): List<Chapter>? {
        val file = File(root, "${id(ficId)}.json"); if(!file.exists()) return null
        val data = try { JSONObject(file.readText()).getJSONArray("chapters") } catch(e:Exception) { error("The saved fic is damaged. Remove its offline copy from Library and download again. Your bookmarks are kept.") }
        val result = (0 until data.length()).map { val chapter = data.getJSONObject(it); Chapter(chapter.getString("title"), chapter.getString("text")) }
        check(result.isNotEmpty() && result.all { it.text.isNotBlank() }) { "The saved fic is damaged. Remove its offline copy and try downloading again." }
        return result
    }
    @Synchronized fun removeOffline(ficId: String) { val file = File(root, "${id(ficId)}.json"); check(!file.exists() || file.delete()) { "Could not remove saved fic" } }
    @Synchronized fun favorite(ficId: String, favorite: Boolean) { id(ficId); writeEntries(entries().map { if(it.fic.id == ficId) it.copy(favorite = favorite) else it }) }
    @Synchronized fun bookmarks(ficId: String): List<ReadingBookmark> {
        val data = array("${id(ficId)}-bookmarks.json")
        return (0 until data.length()).map { val item = data.getJSONObject(it); ReadingBookmark(item.getString("id"), item.getString("title"), item.getInt("chapter"), item.getInt("index"), item.getInt("offset"), item.optInt("bookOffset", -1),item.optDouble("fraction",-1.0)) }
    }
    private fun writeBookmarks(ficId: String, values: List<ReadingBookmark>) {
        val data = JSONArray(); values.forEach { data.put(JSONObject().put("id", it.id).put("title", it.title).put("chapter", it.chapter).put("index", it.index).put("offset", it.offset).put("bookOffset", it.bookOffset).put("fraction",it.fraction)) }; write("${id(ficId)}-bookmarks.json", data.toString())
    }
    @Synchronized fun addBookmark(ficId: String, title: String, chapter: Int, index: Int, offset: Int, bookOffset: Int = -1, fraction: Double = -1.0) {
        require(title.isNotBlank() && chapter >= 0 && index >= 0 && offset >= 0)
        writeBookmarks(ficId, bookmarks(ficId) + ReadingBookmark(UUID.randomUUID().toString(), title.trim(), chapter, index, offset, bookOffset,fraction))
    }
    @Synchronized fun removeBookmark(ficId: String, bookmarkId: String) { writeBookmarks(ficId, bookmarks(ficId).filterNot { it.id == bookmarkId }) }
}
