package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
import androidx.compose.ui.geometry.Offset
class BookPaginationTest {
    @Test fun pagesPreserveAllTextAndUnicode() {
        val text = "A long paragraph 😀 with words\n\n第二段\n".repeat(500)
        val pages = paginateBook(text) { minOf(173,it.length) }
        assertEquals(text,pages.joinToString("") { it.text })
        assertTrue(pages.size > 2)
        var offset=0
        pages.forEach { assertEquals(offset,it.offset); assertFalse(it.text.last().isHighSurrogate()); assertFalse(it.text.first().isLowSurrogate()); offset+=it.text.length }
    }
    @Test fun narrowPagesStillMakeProgress() {
        val text="😀x😀"; val pages=paginateBook(text) { 0 }
        assertEquals(text,pages.joinToString("") { it.text })
        assertEquals(3,pages.size)
        assertEquals("",paginateBook("") { 0 }.single().text)
    }
    @Test fun curlKeepsUnturnedPaperFixedAndExposesAReverse() {
        val w=1000f
        assertEquals(100f,curlPoint(100f,w,0f).x,.01f)
        val flat=curlPoint(50f,w,.25f)
        assertEquals(50f,flat.x,.01f); assertEquals(0f,flat.lift,.01f)
        val folded=curlPoint(w,w,.5f)
        assertTrue(folded.normal<0f); assertTrue(folded.lift>0f)
        assertEquals(0f,folded.x,.1f)
        assertTrue(curlPoint(w,w,.95f).x<0f)
        for(step in 0..100) for(column in 0..64) {
            val point=curlPoint(w*column/64,w,step/100f)
            assertTrue(point.x.isFinite() && point.lift.isFinite())
        }
    }
    @Test fun opacityMaskClipsBothSurfaceOrientationsToPaperBounds() {
        val corners=listOf(Offset(-10f,-10f),Offset(20f,-10f),Offset(20f,20f),Offset(-10f,20f))
        for(points in listOf(corners,corners.reversed())) {
            val clipped=clipPaperPolygon(points,10f,10f)
            assertEquals(4,clipped.size)
            assertTrue(clipped.all { it.x in 0f..10f && it.y in 0f..10f })
            val area=clipped.indices.sumOf { i -> val a=clipped[i];val b=clipped[(i+1)%clipped.size];(a.x*b.y-b.x*a.y).toDouble() }
            assertEquals(100.0,kotlin.math.abs(area)/2,.001)
        }
        assertTrue(clipPaperPolygon(listOf(Offset(-20f,0f),Offset(-10f,0f),Offset(-10f,5f)),10f,10f).isEmpty())
    }
    @Test fun meshHasAnExactBoundaryAtTheFrontReverseTangent() {
        val start=100f;val end=1000f;val radius=73f
        val tangent=start+kotlin.math.PI.toFloat()*radius/2
        val stops=curlStripStops(start,end,radius)
        assertEquals(start,stops.first(),0f);assertEquals(end,stops.last(),0f)
        assertTrue(stops.contains(tangent))
        assertTrue(stops.zipWithNext().all { (a,b)->a<b && !(a<tangent && b>tangent) })
    }
}
