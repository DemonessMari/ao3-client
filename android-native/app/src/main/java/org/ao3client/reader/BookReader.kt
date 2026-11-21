@file:OptIn(androidx.compose.foundation.ExperimentalFoundationApi::class)
package org.ao3client.reader

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.pager.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.Alignment
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.foundation.background
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.Constraints
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.coroutines.ensureActive
import kotlin.coroutines.coroutineContext

internal data class BookPage(val offset: Int, val text: String)
internal fun paginateBook(text: String, fit: (String) -> Int): List<BookPage> = paginateBookRanges(text) { chunk, _ -> fit(chunk) }
internal fun paginateBookRanges(text: String, fit: (String,Int) -> Int): List<BookPage> {
    val pages = mutableListOf<BookPage>(); var offset = 0
    while(offset < text.length) {
        var limit = minOf(text.length, offset + 8192)
        if(limit < text.length && text[limit-1].isHighSurrogate()) limit--
        val chunk = text.substring(offset, limit)
        var count = fit(chunk,offset).coerceIn(1, chunk.length)
        if(count < chunk.length && chunk[count-1].isHighSurrogate()) count = if(count > 1) count-1 else 2
        pages += BookPage(offset, chunk.substring(0,count)); offset += count
    }
    return pages.ifEmpty { listOf(BookPage(0,"")) }
}

@Composable fun BookReader(vm: ClientState, text: String, paragraphs: List<String>, modifier: Modifier = Modifier, rich: AnnotatedString = AnnotatedString(text)) {
    val chapterIndex=vm.chapter
    val density = LocalDensity.current
    val measurer = rememberTextMeasurer(cacheSize = 0)
    val prefs = vm.preferences
    val style = TextStyle(fontFamily = if(prefs.fontFamily == "sans-serif") FontFamily.SansSerif else FontFamily.Serif,
        fontSize = prefs.fontSize.sp, lineHeight = (prefs.fontSize*prefs.lineSpacing).sp)
    BoxWithConstraints(modifier) {
        val width = with(density) { (maxWidth-48.dp).roundToPx() }.coerceAtLeast(1)
        val height = with(density) { (maxHeight-100.dp).roundToPx() }.coerceAtLeast(1)
        val linePixels = with(density) { (prefs.fontSize*prefs.lineSpacing).sp.toPx() }
        if(width < 60 || height < linePixels) {
            Text("Close the keyboard or enlarge the reading area to show a page.")
            return@BoxWithConstraints
        }
        val pagination = produceState<List<BookPage>?>(null,text,rich,width,height,style,density.fontScale) {
            value=null
            value=withContext(Dispatchers.Default) {
            val jobContext=coroutineContext
            paginateBookRanges(text) { chunk,offset ->
                jobContext.ensureActive()
                val layout = measurer.measure(rich.subSequence(offset,offset+chunk.length), style, constraints = Constraints(maxWidth = width), maxLines = (height/linePixels).toInt()+3)
                val line = (0 until layout.lineCount).lastOrNull { layout.getLineBottom(it) <= height } ?: 0
                layout.getLineEnd(line, visibleEnd = false)
            }
            }
        }
        val pages=pagination.value
        if(pages==null) { Box(Modifier.fillMaxSize(),contentAlignment=Alignment.Center) { CircularProgressIndicator() }; return@BoxWithConstraints }
        val starts = remember(text) { var cursor=0; paragraphs.map { p -> text.indexOf(p,cursor).coerceAtLeast(cursor).also { cursor=it+p.length } } }
        val anchor = if(vm.bookOffset >= 0) vm.bookOffset else starts.getOrElse(vm.positionIndex) { 0 }
        key(text,rich,width,height,style,vm.readerRevision,vm.chapter) {
            CurlBookPages(vm,pages,pages.indexOfLast { it.offset <= anchor }.coerceAtLeast(0),style,width,rich) { page ->
                if(vm.chapter!=chapterIndex) return@CurlBookPages
                vm.bookOffset=pages[page].offset
                vm.positionIndex=starts.indexOfLast { it<=vm.bookOffset }.coerceAtLeast(0)
                vm.positionOffset=0; vm.saveProgress()
            }
        }
    }
}
