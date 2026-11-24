package org.ao3client.reader

import android.app.Activity
import android.app.Instrumentation
import android.content.Context
import android.os.Bundle

/** Checks the installed backend without opening an activity; a bookmark test requires an explicit work argument. */
class DeviceNetworkInstrumentation:Instrumentation() {
    private var bookmarkWork:String?=null
    override fun onCreate(arguments:Bundle?) {super.onCreate(arguments);bookmarkWork=arguments?.getString("ao3_private_bookmark_work");if(arguments?.getString("ao3_network")!="1"){finish(Activity.RESULT_CANCELED,Bundle().apply{putString("error","Explicit ao3_network=1 is required.")});return};start()}
    override fun onStart() {
        super.onStart()
        val result=Bundle();var step="setup"
        fun report(name:String,value:String){result.putString(name,value);sendStatus(0,Bundle().apply{putString("step",name);putString("result",value)})}
        val copied=DeviceMemoryPreferences()
        try {
            val saved=targetContext.getSharedPreferences("native-reader",Context.MODE_PRIVATE)
            val edit=copied.edit()
            for(key in listOf("cookies","networkUserAgent","sessionUsername"))saved.getString(key,null)?.let{edit.putString(key,it)}
            edit.apply()
            val archive=Archive(copied)
            step="session"
            val identity=archive.session()
            report("session",if(identity==null)"signed-out" else "verified")
            step="search-one";val first=archive.search("sherlock",1,emptyMap());check(first.works.isNotEmpty()){"Empty first search"};report(step,first.works.size.toString())
            step="search-two";val second=archive.search("harry potter",1,emptyMap());check(second.works.isNotEmpty() && first.works.map{it.id}!=second.works.map{it.id}){"Search results were not distinct"};report(step,second.works.size.toString())
            val id=first.works.first().id
            step="read";val chapters=archive.read(id).first;check(chapters.isNotEmpty() && chapters.all{it.text.isNotBlank()}){"No readable chapter"};report(step,chapters.size.toString())
            step="download-formats";val formats=archive.downloads(id);check(formats.isNotEmpty()){"No download formats"};report(step,formats.size.toString())
            step="comments";val comments=archive.comments(id,1);report(step,comments.comments.size.toString())
            bookmarkWork?.let { work ->
                require(work.matches(Regex("[0-9]+"))){"Invalid private-bookmark test target"}
                step="private-bookmark"
                if(identity==null)report(step,"skipped: signed out")
                else {
                    val initial=archive.archiveBookmark(work,identity)
                    if(initial.id.isNotEmpty())report(step,"skipped: existing bookmark kept")
                    else {
                        val label="AO3 Client verification - temporary private bookmark"
                        val draft=initial.copy(notes=label,tags="",collections="",pseud=initial.pseuds.first().first,privateBookmark=true,recommendation=false)
                        archive.saveArchiveBookmark(draft,false,identity)
                        val created=archive.archiveBookmark(work,identity)
                        check(created.id.isNotEmpty() && created.notes==label){"Test bookmark creation not confirmed; check work $work before retrying"}
                        val privateConfirmed=created.privateBookmark
                        archive.saveArchiveBookmark(created,true,identity)
                        check(archive.archiveBookmark(work,identity).id.isEmpty()){"Temporary bookmark cleanup not confirmed for work $work"}
                        check(privateConfirmed){"Bookmark privacy was not confirmed; test entry was removed"}
                        result.putBoolean("private_bookmark_verified",true);report(step,"created privately, verified, deleted; original state restored")
                    }
                }
            }
            step="offline-backup"
            val temporary=java.nio.file.Files.createTempDirectory(targetContext.cacheDir.toPath(),"ao3-library-check-").toFile()
            try {
                val source=ReadingLibrary(java.io.File(temporary,"source"));val prefs=DeviceMemoryPreferences();val fic=Fic("987654321","Device storage check","Test")
                val content=listOf(Chapter("One","Offline reading text 世界.\n\n".repeat(20000)),Chapter("Two","Second chapter.\n\n".repeat(10000)))
                source.saveOffline(fic,content);source.favorite(fic.id,true);source.addBookmark(fic.id,"Test place",1,3,20,100)
                prefs.edit().putInt("progress.${fic.id}.chapter",1).putInt("progress.${fic.id}.1.index",3).putInt("progress.${fic.id}.1.offset",20).apply()
                val file=java.io.File(temporary,"backup.json");file.writeText(LibraryBackup.snapshot(source,prefs,true).toString(),Charsets.UTF_8)
                val restoredPrefs=DeviceMemoryPreferences();val target=java.io.File(temporary,"restored");LibraryBackup.merge(org.json.JSONObject(file.readText(Charsets.UTF_8)),ReadingLibrary(target),restoredPrefs)
                val reopened=ReadingLibrary(target)
                check(reopened.load(fic.id)==content){"Offline chapters changed"};check(reopened.entries().single().favorite){"Favorite was lost"};check(reopened.bookmarks(fic.id).single().title=="Test place"){"Bookmark was lost"};check(restoredPrefs.getInt("progress.${fic.id}.chapter",-1)==1){"Chapter position was lost"};check(restoredPrefs.getInt("progress.${fic.id}.1.index",-1)==3){"Reading position was lost"}
                report(step,"offline text, favorite, bookmark and reading position restored")
            } finally {check(temporary.deleteRecursively()){"Could not remove temporary storage-check files"}}
            result.putBoolean("ao3_network_success",true)
            finish(Activity.RESULT_OK,result)
        } catch(e:Throwable) {
            result.putBoolean("ao3_network_success",false);result.putString("failed_step",step)
            val account=copied.getString("sessionUsername",null)
            var message=e.message.orEmpty().take(400)
            if(!account.isNullOrBlank())message=message.replace(account,"[account]")
            result.putString("error",message.ifBlank{e.javaClass.simpleName})
            finish(Activity.RESULT_CANCELED,result)
        }
    }
}
