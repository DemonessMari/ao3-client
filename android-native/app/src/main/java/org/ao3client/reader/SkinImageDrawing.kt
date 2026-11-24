package org.ao3client.reader

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Paint
import android.graphics.RectF
import android.util.LruCache
import androidx.compose.foundation.Canvas
import androidx.compose.runtime.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.clipRect
import androidx.compose.ui.unit.dp
import kotlin.math.*

private val skinBitmapCache=object:LruCache<String,Bitmap>(16*1024*1024){override fun sizeOf(key:String,value:Bitmap)=value.byteCount}
internal fun decodedSkinBitmap(asset:SkinImage):Bitmap? {
    skinBitmapCache.get(asset.data)?.let{return it}
    val bytes=runCatching{asset.bytes()}.getOrNull() ?: return null
    val bounds=BitmapFactory.Options().apply{inJustDecodeBounds=true};BitmapFactory.decodeByteArray(bytes,0,bytes.size,bounds)
    if(bounds.outWidth !in 1..4096 || bounds.outHeight !in 1..4096 || bounds.outWidth.toLong()*bounds.outHeight>2097152)return null
    val options=BitmapFactory.Options().apply{inSampleSize=1};while(max(bounds.outWidth,bounds.outHeight)/options.inSampleSize>1024)options.inSampleSize*=2
    return BitmapFactory.decodeByteArray(bytes,0,bytes.size,options)?.also{skinBitmapCache.put(asset.data,it)}
}
internal fun safeSkinOpacity(background:String,text:String,requested:Double):Double {
    val rgb=listOf(1,3,5).map{background.substring(it,it+2).toInt(16)}
    fun safe(alpha:Double)=listOf(0,255).all{edge->val color="#"+rgb.joinToString(""){"%02x".format((it*(1-alpha)+edge*alpha).roundToInt().coerceIn(0,255))};AppSkin.contrast(color,text)>=4.5}
    val ceiling=floor(requested*255)/255
    if(safe(ceiling))return ceiling
    var low=0.0;var high=ceiling;repeat(18){val mid=(low+high)/2;if(safe(mid))low=mid else high=mid};return floor(low*255)/255
}
@Composable internal fun SkinImageCanvas(target:String,modifier:Modifier=Modifier,banner:Boolean=false,background:Color=androidx.compose.material3.MaterialTheme.colorScheme.background,foreground:Color=androidx.compose.material3.MaterialTheme.colorScheme.onBackground) {
    val skin=LocalAppSkin.current ?: return
    val layers=skin.layers.filter{it.target==target && (it.placement=="banner")==banner}.sortedBy{if(it.placement=="overlay")1 else 0}
    if(layers.isEmpty())return
    val images by produceState<Map<String,Bitmap?>>(emptyMap(),skin.images){value=withContext(Dispatchers.IO){skin.images.mapValues{decodedSkinBitmap(it.value)}}}
    fun hex(color:Color)="#%02x%02x%02x".format((color.red*255).roundToInt(),(color.green*255).roundToInt(),(color.blue*255).roundToInt())
    val bg=hex(background);val fg=hex(foreground)
    val alphas=remember(layers,bg,fg,skin.accent){
        val budget=min(safeSkinOpacity(bg,fg,1.0),if(target=="window")safeSkinOpacity(bg,skin.accent,1.0) else 1.0)
        val limit=floor(budget/layers.size*255)/255
        layers.map{if(banner)it.opacity else min(it.opacity,limit)}
    }
    Canvas(modifier){clipRect {layers.forEachIndexed{index,layer->images[layer.image]?.let{bitmap->drawSkinLayer(bitmap,layer,alphas[index],banner)}}}}
}
private fun DrawScope.drawSkinLayer(bitmap:Bitmap,layer:SkinLayer,opacity:Double,banner:Boolean) {
    if(size.width<=0 || size.height<=0)return
    val paint=Paint(Paint.ANTI_ALIAS_FLAG or Paint.FILTER_BITMAP_FLAG).apply{alpha=(opacity*255).roundToInt().coerceIn(0,255)}
    var w=layer.width.dp.toPx();var h=layer.height.dp.toPx();val dx=layer.offsetX.dp.toPx();val dy=layer.offsetY.dp.toPx()
    fun paintAt(rect:RectF){var destination=rect;var source=RectF(0f,0f,bitmap.width.toFloat(),bitmap.height.toFloat())
        if(layer.fit=="cover"){val factor=max(rect.width()/bitmap.width,rect.height()/bitmap.height);source=RectF((bitmap.width-rect.width()/factor)/2,(bitmap.height-rect.height()/factor)/2,(bitmap.width+rect.width()/factor)/2,(bitmap.height+rect.height()/factor)/2)}
        else if(layer.fit=="contain"){val factor=min(rect.width()/bitmap.width,rect.height()/bitmap.height);val ww=bitmap.width*factor;val hh=bitmap.height*factor;destination=RectF(rect.centerX()-ww/2,rect.centerY()-hh/2,rect.centerX()+ww/2,rect.centerY()+hh/2)}
        val left=floor(source.left).toInt().coerceIn(0,bitmap.width-1);val top=floor(source.top).toInt().coerceIn(0,bitmap.height-1)
        val right=ceil(source.right).toInt().coerceIn(left+1,bitmap.width);val bottom=ceil(source.bottom).toInt().coerceIn(top+1,bitmap.height)
        drawContext.canvas.nativeCanvas.drawBitmap(bitmap,android.graphics.Rect(left,top,right,bottom),destination,paint)
    }
    if(banner){paintAt(RectF(0f,0f,size.width,size.height));return}
    if(layer.fit=="tile"){
        val count=ceil(size.width/w+2)*ceil(size.height/h+2);if(count>1024){val factor=sqrt(count/1024);w*=factor;h*=factor}
        var y=dy%h-h;while(y<size.height){var x=dx%w-w;while(x<size.width){drawContext.canvas.nativeCanvas.drawBitmap(bitmap,null,RectF(x,y,x+w,y+h),paint);x+=w};y+=h};return
    }
    var x=if(layer.anchor.contains("right"))size.width-w else if(layer.anchor=="center")(size.width-w)/2 else 0f
    var y=if(layer.anchor.contains("bottom"))size.height-h else if(layer.anchor=="center")(size.height-h)/2 else 0f
    x+=dx;y+=dy;paintAt(RectF(x,y,x+w,y+h))
}
