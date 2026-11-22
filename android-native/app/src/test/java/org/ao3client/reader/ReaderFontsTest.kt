package org.ao3client.reader

import java.io.ByteArrayInputStream
import java.io.File
import java.io.InputStream
import org.junit.Test
import org.junit.Assert.*
import org.junit.Rule
import org.junit.rules.TemporaryFolder

class ReaderFontsTest {
    @get:Rule val directory=TemporaryFolder()
    private fun bytes()=javaClass.getResourceAsStream("/reader-test-font.ttf")!!.use { it.readBytes() }
    @Test fun importsAreCopiedDeduplicatedAndRecoverAfterRestart() {
        val root=directory.newFolder();var validations=0
        val store=ReaderFontStore(root) {validations++;true};val font=store.import(ByteArrayInputStream(bytes()),"Reader Test.ttf")
        assertTrue(ReaderFontStore.validId(font.id));assertArrayEquals(bytes(),font.file.readBytes());assertEquals(1,validations)
        assertEquals(font.id,store.import(ByteArrayInputStream(bytes()),"Duplicate.ttf").id);assertEquals(1,validations)
        val restored=ReaderFontStore(root) {true}.list();assertEquals(listOf(font),restored)
        assertFalse(store.remove("font:../../outside"));assertTrue(store.remove(font.id));assertTrue(store.list().isEmpty());assertTrue(root.listFiles().orEmpty().isEmpty())
    }
    @Test fun invalidAndOversizeFilesDoNotReplaceAnExistingFont() {
        val root=directory.newFolder();val store=ReaderFontStore(root) {true};val font=store.import(ByteArrayInputStream(bytes()),"Reader.ttf")
        assertTrue(runCatching {store.import(ByteArrayInputStream("invalid".toByteArray()),"Wrong.ttf")}.isFailure)
        val rejected=ReaderFontStore(root) {false};val changed=bytes().also {it[it.lastIndex]=(it.last()+1).toByte()}
        assertTrue(runCatching {rejected.import(ByteArrayInputStream(changed),"Broken.ttf")}.isFailure)
        val huge=object:InputStream(){var remaining=ReaderFontStore.MAX_BYTES+1;override fun read():Int=if(remaining-->0)0 else -1;override fun read(b:ByteArray,off:Int,len:Int):Int {if(remaining<=0)return -1;val n=minOf(remaining,len);b.fill(0,off,off+n);remaining-=n;return n}}
        assertTrue(runCatching {store.import(huge,"Huge.ttf")}.isFailure)
        assertEquals(listOf(font),store.list());assertEquals(2,root.listFiles()!!.size)
    }
    @Test fun namesCannotCreatePathsAndDamagedMetadataFallsBack() {
        val root=directory.newFolder();val store=ReaderFontStore(root) {true};val font=store.import(ByteArrayInputStream(bytes()),"../../custom\u0000.ttf")
        assertEquals("custom.ttf",font.name);assertTrue(root.listFiles()!!.all {it.parentFile==root})
        File(root,font.file.nameWithoutExtension+".name").delete();assertEquals("Imported font",store.list().single().name)
        font.file.delete();assertTrue(store.list().isEmpty())
    }
}
