package org.ao3client.reader

import android.graphics.Typeface
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.security.MessageDigest

internal data class ReaderFont(val id: String, val name: String, val file: File)
internal class ReaderFontStore(private val directory: File, private val validate: (File) -> Boolean = { runCatching { Typeface.createFromFile(it) != null }.getOrDefault(false) }) {
    companion object {
        const val MAX_BYTES = 20 * 1024 * 1024
        fun validId(id: String) = Regex("font:[0-9a-f]{64}").matches(id)
    }
    private fun prefix(input: InputStream,limit: Int): ByteArray {
        val bytes=ByteArray(limit);var count=0
        while(count<limit){val n=input.read(bytes,count,limit-count);if(n<0)break;require(n>0) { "Could not read the font." };count+=n};return bytes.copyOf(count)
    }
    private fun fontFile(id: String): File {
        require(validId(id)) { "Unknown font." }
        return File(directory,id.removePrefix("font:")+".font")
    }
    fun list(): List<ReaderFont> = directory.listFiles().orEmpty().filter { it.isFile && it.extension=="font" && validId("font:"+it.nameWithoutExtension) && it.length() in 12..MAX_BYTES.toLong() }.map {
        val name=runCatching { File(directory,it.nameWithoutExtension+".name").inputStream().use { stream -> String(prefix(stream,320),Charsets.UTF_8) } }.getOrDefault("Imported font")
        ReaderFont("font:"+it.nameWithoutExtension,cleanName(name),it)
    }.sortedBy { it.name.lowercase() }.take(20)
    private fun cleanName(name: String) = name.substringAfterLast('/').substringAfterLast('\\').replace(Regex("[\\p{Cntrl}]"),"").trim().take(80).ifBlank { "Imported font" }
    fun import(input: InputStream, name: String): ReaderFont {
        check(directory.isDirectory || directory.mkdirs()) { "Could not create font storage." }
        val temporary=File.createTempFile("import-",".font",directory)
        try {
            val digest=MessageDigest.getInstance("SHA-256");var size=0
            FileOutputStream(temporary).use { output ->
                val buffer=ByteArray(8192)
                while(true) { val count=input.read(buffer);if(count<0)break;require(count>0) { "Could not read the font." };size+=count;require(size<=MAX_BYTES) { "Choose a font smaller than 20 MB." };output.write(buffer,0,count);digest.update(buffer,0,count) }
                output.fd.sync()
            }
            val signature=temporary.inputStream().use { prefix(it,4) }
            require(size>=12 && (signature.contentEquals(byteArrayOf(0,1,0,0)) || signature.contentEquals("OTTO".toByteArray(Charsets.US_ASCII)))) { "Choose a TrueType (.ttf) or OpenType (.otf) font." }
            val id="font:"+digest.digest().joinToString("") { "%02x".format(it) }
            val fonts=list();fonts.firstOrNull { it.id==id }?.let { return it }
            require(fonts.size<20) { "Remove an imported font before adding another (20 maximum)." }
            require(validate(temporary)) { "This font could not be opened." }
            val destination=fontFile(id);check(temporary.renameTo(destination)) { "Could not save the font." }
            val label=cleanName(name);val metadata=File.createTempFile("name-",".tmp",directory)
            try {metadata.writeText(label);check(metadata.renameTo(File(directory,id.removePrefix("font:")+".name"))) { "Could not save the font name." }} catch(e:Exception) {destination.delete();throw e} finally {metadata.delete()}
            return ReaderFont(id,label,destination)
        } finally {temporary.delete()}
    }
    fun remove(id: String): Boolean {
        if(!validId(id))return false
        if(!fontFile(id).delete())return false
        File(directory,id.removePrefix("font:")+".name").delete();return true
    }
}
