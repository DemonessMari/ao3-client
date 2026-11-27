package org.ao3client.reader

import androidx.compose.animation.core.animate
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.Alignment
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.*
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.semantics.*
import androidx.compose.ui.text.*
import androidx.compose.ui.unit.*
import kotlinx.coroutines.launch
import kotlin.math.*

// Cylindrical sheet: the flat front stays still, only the section past the
// moving bend wraps around the cylinder and exposes the paper's reverse side.
internal data class CurlPoint(val x: Float, val lift: Float, val normal: Float)
internal fun curlPoint(x: Float, width: Float, progress: Float): CurlPoint {
    val p=progress.coerceIn(0f,1f)
    val radius=max(.001f,width*.12f*sin(PI.toFloat()*p))
    val crease=width*(1-p)-PI.toFloat()*radius/2
    val u=x-crease
    return when {
        u<=0 -> CurlPoint(x,0f,1f)
        u<PI.toFloat()*radius -> { val angle=u/radius; CurlPoint(crease+radius*sin(angle),radius*(1-cos(angle)),cos(angle)) }
        else -> CurlPoint(crease-(u-PI.toFloat()*radius),2*radius,-1f)
    }
}

internal fun curlStripStops(start:Float,end:Float,radius:Float):List<Float> =
    ((0..64).map { start+(end-start)*it/64 } +
        listOf(start+PI.toFloat()*radius/2,start+PI.toFloat()*radius))
        .filter { it in start..end }.distinct().sorted()

// Convex clipping keeps the occlusion silhouette independent of path winding
// and avoids cracks from repeated boolean unions of nearly coincident strips.
internal fun clipPaperPolygon(points:List<Offset>,width:Float,height:Float):List<Offset> {
    var polygon=points
    for(side in 0..3) {
        if(polygon.isEmpty()) break
        val output=mutableListOf<Offset>()
        val axis=side/2; val bound=if(side%2==0) 0f else if(axis==0) width else height
        fun coordinate(p:Offset)=if(axis==0) p.x else p.y
        fun inside(p:Offset)=if(side%2==0) coordinate(p)>=bound else coordinate(p)<=bound
        var previous=polygon.last()
        for(current in polygon) {
            if(inside(previous)!=inside(current)) {
                val t=(bound-coordinate(previous))/(coordinate(current)-coordinate(previous))
                output+=previous+(current-previous)*t
            }
            if(inside(current)) output+=current
            previous=current
        }
        polygon=output
    }
    return polygon
}

private fun DrawScope.paperTurn(old: TextLayoutResult, under: TextLayoutResult, progress: Float, paper: Color, ink: Color, pad: Offset) {
    drawRect(paper); drawText(under,color=ink,topLeft=pad)
    if(progress>=.9999f) return
    if(progress<=.0001f) { drawRect(paper); drawText(old,color=ink,topLeft=pad); return }
    val w=size.width; val h=size.height
    val angle=.30f*sin(PI.toFloat()*progress); val ca=cos(angle); val sa=sin(angle)
    val span=w*ca+h*sa; val tall=h*ca+w*sa
    val minX=(w-span)/2; val minY=(h-tall)/2
    val radius=max(.001f,span*.12f*sin(PI.toFloat()*progress))
    val crease=minX+span*(1-progress)-PI.toFloat()*radius/2
    fun flat(x:Float,y:Float)=Offset(w/2+(x-w/2)*ca-(y-h/2)*sa,h/2+(x-w/2)*sa+(y-h/2)*ca)
    fun projected(x:Float,y:Float):Offset {
        val curled=curlPoint(x-minX,span,progress)
        val position=flat(curled.x+minX,y); val scale=1+curled.lift/(span*8)
        return Offset(w/2+(position.x-w/2)*scale,h/2+(position.y-h/2)*scale)
    }
    fun quad(x0:Float,x1:Float,project:Boolean):List<Offset> = listOf(x0 to minY,x1 to minY,x1 to minY+tall,x0 to minY+tall).map { (x,y)->if(project) projected(x,y) else flat(x,y) }
    fun path(points:List<Offset>)=android.graphics.Path().apply { moveTo(points[0].x,points[0].y); points.drop(1).forEach { lineTo(it.x,it.y) }; close() }
    val canvas=drawContext.canvas.nativeCanvas
    val tangent=crease+PI.toFloat()*radius/2
    val shadowStart=projected(tangent,h/2)
    val shadowVector=Offset(ca,sa)* (span*.035f)
    val shadowEnd=shadowStart+shadowVector
    val shadow=android.graphics.Paint().apply { shader=android.graphics.LinearGradient(shadowStart.x,shadowStart.y,shadowEnd.x,shadowEnd.y,intArrayOf(0x20000000,0),null,android.graphics.Shader.TileMode.CLAMP) }
    val shadowTop=projected(tangent,minY); val shadowBottom=projected(tangent,minY+tall)
    canvas.drawPath(path(listOf(shadowTop,shadowTop+shadowVector,shadowBottom+shadowVector,shadowBottom)),shadow)
    data class Strip(val matrix:android.graphics.Matrix,val clip:android.graphics.Path,val normal:Float)
    val strips=mutableListOf<Strip>()
    val paperMask=android.graphics.Path().apply { fillType=android.graphics.Path.FillType.WINDING }
    val reverseMask=android.graphics.Path().apply { fillType=android.graphics.Path.FillType.WINDING }
    fun addStrip(x0:Float,x1:Float,normal:Float) {
        val source=quad(x0,x1,false); val target=quad(x0,x1,true)
        val src=source.flatMap { listOf(it.x,it.y) }.toFloatArray(); val dst=target.flatMap { listOf(it.x,it.y) }.toFloatArray()
        val matrix=android.graphics.Matrix(); if(!matrix.setPolyToPoly(src,0,dst,0,4)) return
        val clipped=clipPaperPolygon(source,w,h); if(clipped.size<3) return
        val mapped=clipped.flatMap { listOf(it.x,it.y) }.toFloatArray(); matrix.mapPoints(mapped)
        var visible=mapped.toList().chunked(2).map { Offset(it[0],it[1]) }
        val area=visible.indices.sumOf { i -> val a=visible[i]; val b=visible[(i+1)%visible.size]; (a.x*b.y-b.x*a.y).toDouble() }
        if(area<0) visible=visible.reversed()
        val projectedPath=path(visible)
        paperMask.addPath(projectedPath)
        if(normal<=0) reverseMask.addPath(projectedPath)
        strips+=Strip(matrix,path(clipped),normal)
    }
    if(crease>minX) addStrip(minX,crease,1f)
    // Never let a projected quad straddle the front/reverse tangent: its
    // projection changes direction there and can otherwise form a torn sliver.
    val stops=curlStripStops(crease,minX+span,radius)
    for(i in 0 until stops.lastIndex) {
        val x0=stops[i]; val x1=stops[i+1]
        addStrip(x0,x1,(curlPoint(x0-minX,span,progress).normal+curlPoint(x1-minX,span,progress).normal)/2)
    }
    // Seal the complete sheet once before drawing its individually clipped strips.
    // Antialiased strip boundaries must never expose the text underneath.
    val opaquePaper=paper.copy(alpha=1f)
    val seal=android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG).apply {
        color=lerp(opaquePaper,ink.copy(alpha=1f),.04f).toArgb(); alpha=255
        style=android.graphics.Paint.Style.FILL_AND_STROKE
        strokeWidth=2f
    }
    canvas.drawPath(paperMask,seal)
    for(strip in strips) {
        if(strip.normal<=0) continue
        canvas.save(); canvas.concat(strip.matrix); canvas.clipPath(strip.clip)
        drawRect(opaquePaper); drawText(old,color=ink,topLeft=pad)
        if(strip.normal<1) drawRect(Color.Black.copy(alpha=(1-strip.normal)*.24f))
        canvas.restore()
    }
    // Draw the reverse ON TOP of every front-text strip, in device coordinates.
    // An underlay cannot erase front glyphs leaking between hardware clip edges.
    // A single opaque, dithered gradient also removes per-strip shading seams.
    val base=lerp(opaquePaper,ink.copy(alpha=1f),.04f).toArgb()
    val bend=lerp(opaquePaper,ink.copy(alpha=1f),.14f).toArgb()
    val reverseStart=flat(crease,h/2); val reverseEnd=flat(crease+radius,h/2)
    val reversePaint=android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG).apply {
        alpha=255; isDither=true
        style=android.graphics.Paint.Style.FILL_AND_STROKE; strokeWidth=2f
        shader=android.graphics.LinearGradient(reverseStart.x,reverseStart.y,reverseEnd.x,reverseEnd.y,
            intArrayOf(base,bend),null,android.graphics.Shader.TileMode.CLAMP)
    }
    canvas.drawPath(reverseMask,reversePaint)


}

@Composable internal fun CurlBookPages(vm: ClientState, pages: List<BookPage>, initial: Int, style: TextStyle, textWidth: Int, rich: AnnotatedString, footerMeasured:(Int)->Unit={}, changed: (Int)->Unit) {
    var index by remember { mutableIntStateOf(initial) }
    var target by remember { mutableIntStateOf(initial) }
    var phase by remember { mutableFloatStateOf(0f) }
    var busy by remember { mutableStateOf(false) }
    var drag by remember { mutableFloatStateOf(0f) }
    val scope=rememberCoroutineScope()
    val measurer=rememberTextMeasurer(cacheSize=4)
    val density=LocalDensity.current
    val uriHandler=LocalUriHandler.current
    val paper=MaterialTheme.colorScheme.surface.copy(alpha=1f); val ink=MaterialTheme.colorScheme.onSurface
    val old=remember(pages,index,style,textWidth,rich) { measurer.measure(rich.subSequence(pages[index].offset,pages[index].offset+pages[index].text.length),style,constraints=Constraints(minWidth=textWidth,maxWidth=textWidth)) }
    val other=remember(pages,target,style,textWidth,rich) { measurer.measure(rich.subSequence(pages[target].offset,pages[target].offset+pages[target].text.length),style,constraints=Constraints(minWidth=textWidth,maxWidth=textWidth)) }
    val pad=with(density) { Offset(vm.preferences.margin.dp.toPx(),20.dp.toPx()) }
    LaunchedEffect(index) { changed(index) }
    fun settle(commit:Boolean) { busy=true; scope.launch {
        if(vm.pageAnimation && target!=index) animate(phase,if(commit) 1f else 0f,animationSpec=tween(420)) { value,_->phase=value }
        if(commit) index=target
        phase=0f; target=index; busy=false
    } }
    fun turn(delta:Int) { if(busy) return
        if(index+delta !in pages.indices) {
            val chapter=vm.chapter+delta
            if(chapter in vm.chapters.indices) {
                vm.changeChapter(chapter, fromEnd=delta<0)
                if(delta>0) { vm.positionIndex=0; vm.positionOffset=0; vm.bookOffset=0; vm.saveProgress() }
            }
            return
        }
        target=(index+delta).coerceIn(pages.indices); if(target!=index) settle(true) }
    var consumedStep by remember { mutableLongStateOf(vm.readerStep.first) }
    LaunchedEffect(vm.readerStep) {if(consumedStep!=vm.readerStep.first){consumedStep=vm.readerStep.first;turn(vm.readerStep.second)}}
    Column(Modifier.fillMaxSize()) {
        Box(Modifier.weight(1f)) {
            Canvas(Modifier.fillMaxSize().semantics {
                contentDescription=pages[index].text
                if(vm.textOnly) onClick(label="Show reading controls") { vm.textOnly=false; true }
                customActions=listOf(CustomAccessibilityAction("Next page") { turn(1); true },CustomAccessibilityAction("Previous page") { turn(-1); true })
            }.pointerInput(index,vm.textOnly,rich,vm.tapLeft,vm.tapCenter,vm.tapRight,vm.tapEdge) {
                detectTapGestures { point ->
                    if(!busy && old.layoutInput.text.isNotEmpty()) {
                        val position=point-pad; val offset=old.getOffsetForPosition(position)
                        if(offset<old.layoutInput.text.length && old.getBoundingBox(offset).contains(position)) {
                            val link=old.layoutInput.text.getLinkAnnotations(offset,offset+1).firstOrNull()?.item as? LinkAnnotation.Url
                            if(link!=null) { runCatching { uriHandler.openUri(link.url) }.onFailure { vm.error="Could not open link." }; return@detectTapGestures }
                        }
                    }
                    when(val action=tapZoneAction(point.x/size.width,vm.tapLeft,vm.tapCenter,vm.tapRight,vm.tapEdge)){-1,1->turn(action);2->vm.textOnly=!vm.textOnly}
                }
            }.pointerInput(index,vm.pageAnimation) {
                detectHorizontalDragGestures(
                    onDragStart={ if(!busy) { drag=0f; phase=0f; target=index } },
                    onHorizontalDrag={ change, amount -> if(!busy) { change.consume(); drag+=amount; target=(index+if(drag<0) 1 else -1).coerceIn(pages.indices); phase=(abs(drag)/size.width).coerceIn(0f,1f) } },
                    onDragEnd={ if(!busy) { if(target==index && abs(drag)>size.width*.22f) turn(if(drag<0) 1 else -1) else settle(phase>.22f) } },
                    onDragCancel={ if(!busy) settle(false) }
                )
            }) {
                val canvas=drawContext.canvas.nativeCanvas; canvas.save(); canvas.clipRect(0f,0f,size.width,size.height)
                if(target==index || !vm.pageAnimation) { drawRect(paper); drawText(old,color=ink,topLeft=pad) }
                else if(target>index) paperTurn(old,other,phase,paper,ink,pad)
                else paperTurn(other,old,1-phase,paper,ink,pad)
                canvas.restore()
            }

        }
        if(!vm.textOnly)ReaderPageFooter(index+1,pages.size,(index>0 || vm.chapter>0) && !busy,(index<pages.lastIndex || vm.chapter<vm.chapters.lastIndex) && !busy,
            previousLabel=if(index==0 && vm.chapter>0)"Previous chapter" else "Previous page",nextLabel=if(index==pages.lastIndex && vm.chapter<vm.chapters.lastIndex)"Next chapter" else "Next page",
            onPrevious={turn(-1)},onNext={turn(1)},modifier=Modifier.onSizeChanged{footerMeasured(it.height)})
    }
}
