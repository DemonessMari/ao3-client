package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
class ReaderExtrasTest {
    @Test fun tapZonesHavePredictableBoundariesAndCanBeReversedOrDisabled() {
        assertEquals(-1,tapZoneAction(.1f,-1,2,1,30));assertEquals(2,tapZoneAction(.5f,-1,2,1,30));assertEquals(1,tapZoneAction(.9f,-1,2,1,30))
        assertEquals(1,tapZoneAction(.1f,1,0,-1,40));assertEquals(0,tapZoneAction(.5f,1,0,-1,40));assertEquals(-1,tapZoneAction(.9f,1,0,-1,40))
    }
    @Test fun speechChunksCoverLongMultilingualTextWithoutSplittingSurrogates() {
        val text=("A complete sentence. 中文。 Emoji 😀 and more words!\n").repeat(200);var cursor=0;val chunks=mutableListOf<String>()
        while(cursor<text.length){val end=speechChunkEnd(text,cursor);assertTrue(end>cursor);assertTrue(end-cursor<=350);if(end<text.length)assertFalse(text[end-1].isHighSurrogate());chunks+=text.substring(cursor,end);cursor=end}
        assertEquals(text,chunks.joinToString(""));assertTrue(chunks.size>1)
    }
}
