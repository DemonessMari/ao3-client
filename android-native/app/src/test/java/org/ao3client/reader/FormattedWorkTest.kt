package org.ao3client.reader

import org.junit.Test
import org.junit.Assert.*
import java.io.File

class FormattedWorkTest {
    private val original="""<div id="work-skin"><style>#workskin .poem {white-space:pre-wrap}</style><div id="chapters"><div class="userstuff"><h2>Heading</h2><p class="poem">One<br>Two</p><table><tr><td>Cell</td></tr></table><p id="note">Footnote <a href="#note">jump</a></p><img src="https://example.org/picture.png" alt="Illustration"><script>alert(1)</script><iframe src="https://bad.example"></iframe></div></div></div>"""
    @Test fun formattingAndWorkSkinSurviveOfflineAndBackup() {
        val chapter=ArchiveParser.chapters(original).single()
        assertTrue(chapter.html.contains("<table>"));assertTrue(chapter.html.contains("class=\"poem\""));assertTrue(chapter.workCss.contains("white-space"))
        val directory=java.nio.file.Files.createTempDirectory("formatted-work").toFile()
        try {
            val library=ReadingLibrary(directory);library.saveOffline(Fic("12","Title","Writer"),listOf(chapter))
            assertEquals(chapter,library.load("12")!!.single())
            val backup=LibraryBackup.snapshot(library,MemoryPreferences());LibraryBackup.validate(backup)
            assertEquals(chapter.html,backup.getJSONArray("works").getJSONObject(0).getJSONArray("chapters").getJSONObject(0).getString("html"))
        } finally {directory.deleteRecursively()}
    }
    @Test fun staticContentCannotExecuteOrLoadBackgroundRequests() {
        val chapter=ArchiveParser.chapters(original).single()
        assertFalse(chapter.html.contains("<script"));assertFalse(chapter.html.contains("<iframe"))
        val clean=staticReadingHtml("<p onclick='steal()'>Hello</p><a href='javascript:steal()'>bad</a><img src='file:///secret'><form action='/delete'><input></form>")
        assertFalse(clean.contains("onclick"));assertFalse(clean.contains("javascript:"));assertFalse(clean.contains("file:"));assertFalse(clean.contains("<form"))
        val page=formattedDocument(chapter,"#ffffff","#000000",19,1.6,24)
        assertTrue(page.contains("default-src 'none'"));assertTrue(page.contains("img-src data:"));assertFalse(page.contains("<img src=\"https:"));assertTrue(page.contains("Illustration"))
        assertFalse(formattedDocument(chapter,"#fff","#000",19,1.6,24,false).contains("#workskin .poem"))
    }
    @Test fun formattedFontsUseOnlyLocalDataAndSafeFamilies() {
        val chapter=Chapter("One","Text",html="<p>Text</p>")
        val font=byteArrayOf(0,1,0,0,0,0,0,0,0,0,0,0)
        val page=formattedDocument(chapter,"#fff","#000",19,1.6,24,font=font)
        assertTrue(page.contains("data:font/ttf;base64,"));assertTrue(page.contains("font-family:Ao3Imported"))
        assertFalse(formattedDocument(chapter,"#fff","#000",19,1.6,24,fontFamily="url(https://bad.example)").contains("bad.example"))
    }
    @Test fun repeatedImagesCannotBypassTheTotalHtmlBudget() {
        val chapter=ArchiveParser.chapters(original).single();val png=byteArrayOf(-119,80,78,71,13,10,26,10)
        val saved=cacheChapterImages(listOf(chapter)){png}.single()
        val one=saved.html.toByteArray().size
        val failure=runCatching{cacheChapterImages(listOf(chapter,chapter),one+1){png}}.exceptionOrNull()
        assertTrue(failure?.message.orEmpty().contains("offline limit"));assertTrue(chapter.html.contains("https://example.org/picture.png"))
    }
    @Test fun imagesAreEmbeddedOnceAndFailurePreservesTheSource() {
        val chapter=ArchiveParser.chapters(original).single();var calls=0
        val png=byteArrayOf(-119,80,78,71,13,10,26,10)
        val saved=cacheChapterImages(listOf(chapter,chapter)){calls++;png}
        assertEquals(1,calls);assertTrue(saved.all{it.html.contains("data:image/png;base64,")})
        assertTrue(formattedDocument(saved.first(),"#fff","#000",19,1.6,24).contains("<img"))
        try {cacheChapterImages(listOf(chapter)){error("offline")};fail()}catch(_:IllegalStateException){}
        assertTrue(chapter.html.contains("https://example.org/picture.png"))
        assertNull(publicImageUrl("http://example.org/a"));assertNull(publicImageUrl("https://localhost/a"));assertNull(publicImageUrl("https://user:secret@example.org/a"));assertNull(imageMime("<svg></svg>".toByteArray()))
    }
}
