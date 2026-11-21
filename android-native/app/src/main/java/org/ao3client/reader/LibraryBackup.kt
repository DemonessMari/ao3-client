package org.ao3client.reader

import android.content.SharedPreferences
import org.json.JSONArray
import org.json.JSONObject

object LibraryBackup {
    const val MAX_BYTES = 100 * 1024 * 1024
    fun snapshot(library: ReadingLibrary, prefs: SharedPreferences, includeDownloads:Boolean = true): JSONObject {
        val works=JSONArray()
        library.entries().forEach { entry ->
            val id=entry.fic.id; val chapters=library.load(id).orEmpty(); val content=JSONArray(); val positions=JSONArray(); val marks=JSONArray()
            if(includeDownloads) chapters.forEach { content.put(it.json()) }
            fun fraction(chapter:Int,index:Int,bookOffset:Int):Double {
                val text=chapters.getOrNull(chapter)?.text.orEmpty()
                return if(bookOffset>=0 && text.isNotEmpty()) (bookOffset.toDouble()/text.length).coerceIn(0.0,1.0) else if(text.isNotEmpty()) (index.toDouble()/text.split(Regex("""\n\s*\n""")).size.coerceAtLeast(1)).coerceIn(0.0,1.0) else 0.0
            }
            library.bookmarks(id).forEach { mark -> marks.put(JSONObject().put("id",mark.id).put("title",mark.title).put("chapter",mark.chapter).put("index",mark.index).put("offset",mark.offset).put("bookOffset",mark.bookOffset).put("fraction",if(mark.fraction in 0.0..1.0) mark.fraction else fraction(mark.chapter,mark.index,mark.bookOffset))) }
            val savedChapters=prefs.all.keys.mapNotNull { Regex("^progress\\.${Regex.escape(id)}\\.(\\d+)\\.(?:index|bookOffset|fraction)$").find(it)?.groupValues?.get(1)?.toInt() }.distinct()
            savedChapters.forEach { chapter -> val key="progress.$id.$chapter"; val index=prefs.getInt("$key.index",0); val book=prefs.getInt("$key.bookOffset",-1)
                positions.put(JSONObject().put("chapter",chapter).put("index",index).put("offset",prefs.getInt("$key.offset",0)).put("bookOffset",book).put("fraction",if(prefs.contains("$key.fraction")) prefs.getFloat("$key.fraction",0f).toDouble() else fraction(chapter,index,book))) }
            val work=JSONObject().put("id",id).put("title",entry.fic.title).put("author",entry.fic.author).put("authorUrl",entry.fic.authorPath).put("fandom",entry.fic.fandom).put("summary",entry.fic.summary).put("language",entry.fic.language).put("rating",entry.fic.rating).put("warnings",entry.fic.warnings)
            works.put(JSONObject().put("work",work).put("favorite",entry.favorite).put("chapters",content).put("bookmarks",marks).put("progress",JSONObject().put("chapter",prefs.getInt("progress.$id.chapter",0)).put("positions",positions)))
        }
        return JSONObject().put("format","ao3-client-library").put("version",1).put("works",works)
    }
    fun validate(root:JSONObject) {
        require(root.optString("format")=="ao3-client-library" && root.optInt("version")==1) { "Unsupported library backup" }
        val works=root.getJSONArray("works"); require(works.length()<=10000) { "Backup contains too many works" }; val ids=mutableSetOf<String>()
        for(i in 0 until works.length()) {
            val entry=works.getJSONObject(i); val id=entry.getJSONObject("work").getString("id")
            require(id.matches(Regex("\\d+")) && ids.add(id)) { "Invalid or duplicate fic ID" }
            val chapters=entry.getJSONArray("chapters"); for(c in 0 until chapters.length()) require(chapters.getJSONObject(c).getString("text").isNotBlank()) { "Backup contains an empty chapter" }
            val marks=entry.getJSONArray("bookmarks"); for(m in 0 until marks.length()) { val mark=marks.getJSONObject(m); require(mark.getString("title").isNotBlank() && mark.getInt("chapter")>=0 && mark.getDouble("fraction") in 0.0..1.0 && mark.optInt("index",0)>=0 && mark.optInt("offset",0)>=0 && mark.optInt("bookOffset",-1)>=-1) { "Invalid reading bookmark" } }
            val progress=entry.getJSONObject("progress"); require(progress.getInt("chapter")>=0)
            val positions=progress.getJSONArray("positions"); for(p in 0 until positions.length()) { val position=positions.getJSONObject(p); require(position.getInt("chapter")>=0 && position.getDouble("fraction") in 0.0..1.0 && position.optInt("index",0)>=0 && position.optInt("offset",0)>=0 && position.optInt("bookOffset",-1)>=-1) { "Invalid reading position" } }
        }
    }
    fun merge(root:JSONObject,library:ReadingLibrary,prefs:SharedPreferences) {
        validate(root) // Validate everything before writing any library data.
        val works=root.getJSONArray("works"); val edit=prefs.edit()
        for(i in 0 until works.length()) {
            val entry=works.getJSONObject(i); val work=entry.getJSONObject("work"); val id=work.getString("id")
            val fic=Fic(id,work.optString("title"),work.optString("author"),work.optString("fandom"),work.optString("summary"),language=work.optString("language"),authorPath=work.optString("authorUrl"),rating=work.optString("rating"),warnings=work.optString("warnings"))
            library.remember(fic)
            val content=entry.getJSONArray("chapters"); val chapters=(0 until content.length()).map { val chapter=content.getJSONObject(it); Chapter.fromJson(chapter) }
            if(chapters.isNotEmpty() && library.load(id)==null) library.saveOffline(fic,chapters)
            if(entry.optBoolean("favorite")) library.favorite(id,true)
            val marks=entry.getJSONArray("bookmarks"); val existing=library.bookmarks(id)
            for(m in 0 until marks.length()) {
                val mark=marks.getJSONObject(m); val chapter=mark.getInt("chapter"); val fraction=mark.getDouble("fraction")
                val bookOffset=mark.optInt("bookOffset",-1).takeIf { it>=0 } ?: chapters.getOrNull(chapter)?.let { (it.text.length*fraction).toInt() } ?: -1
                if(existing.none { it.title==mark.getString("title") && it.chapter==chapter && it.bookOffset==bookOffset }) library.addBookmark(id,mark.getString("title"),chapter,mark.optInt("index",0),mark.optInt("offset",0),bookOffset,fraction)
            }
            val progress=entry.getJSONObject("progress"); if(!prefs.contains("progress.$id.chapter")) edit.putInt("progress.$id.chapter",progress.getInt("chapter"))
            val positions=progress.getJSONArray("positions")
            for(p in 0 until positions.length()) { val position=positions.getJSONObject(p); val key="progress.$id.${position.getInt("chapter")}"; if(!prefs.contains("$key.index") && !prefs.contains("$key.fraction")) {
                edit.putInt("$key.index",position.optInt("index",0)).putInt("$key.offset",position.optInt("offset",0)).putInt("$key.bookOffset",position.optInt("bookOffset",-1)).putFloat("$key.fraction",position.getDouble("fraction").toFloat())
            } }
        }
        check(edit.commit()) { "Could not save imported reading positions" }
    }
}
