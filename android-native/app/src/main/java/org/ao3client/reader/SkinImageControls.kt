@file:OptIn(androidx.compose.foundation.layout.ExperimentalLayoutApi::class)
package org.ao3client.reader

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.Alignment
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.util.Base64

internal fun importSkinImage(bytes:ByteArray):SkinImage {
    require(bytes.size<=8*1024*1024){"Choose an image smaller than 8 MB."}
    val bounds=BitmapFactory.Options().apply{inJustDecodeBounds=true};BitmapFactory.decodeByteArray(bytes,0,bytes.size,bounds)
    require(bounds.outMimeType in listOf("image/png","image/jpeg") && bounds.outWidth in 1..16384 && bounds.outHeight in 1..16384 && bounds.outWidth.toLong()*bounds.outHeight<=64000000){"Choose a PNG or JPEG image up to 64 megapixels."}
    val options=BitmapFactory.Options().apply{inSampleSize=1};while(maxOf(bounds.outWidth,bounds.outHeight)/options.inSampleSize>1024)options.inSampleSize*=2
    var bitmap=BitmapFactory.decodeByteArray(bytes,0,bytes.size,options) ?: error("Cannot decode this image.")
    val orientation=runCatching{android.media.ExifInterface(java.io.ByteArrayInputStream(bytes)).getAttributeInt(android.media.ExifInterface.TAG_ORIENTATION,1)}.getOrDefault(1)
    val matrix=android.graphics.Matrix()
    when(orientation){2->matrix.setScale(-1f,1f);3->matrix.setRotate(180f);4->matrix.setScale(1f,-1f);5->{matrix.setRotate(90f);matrix.postScale(-1f,1f)};6->matrix.setRotate(90f);7->{matrix.setRotate(-90f);matrix.postScale(-1f,1f)};8->matrix.setRotate(270f)}
    if(!matrix.isIdentity){val rotated=Bitmap.createBitmap(bitmap,0,0,bitmap.width,bitmap.height,matrix,true);if(rotated!==bitmap)bitmap.recycle();bitmap=rotated}
    val png=bounds.outMimeType=="image/png" && bitmap.hasAlpha()
    try {repeat(5){
        val output=java.io.ByteArrayOutputStream();check(bitmap.compress(if(png)Bitmap.CompressFormat.PNG else Bitmap.CompressFormat.JPEG,85,output))
        if(output.size()<=512*1024)return SkinImage("data:image/${if(png)"png" else "jpeg"};base64,"+Base64.getEncoder().encodeToString(output.toByteArray()))
        val smaller=Bitmap.createScaledBitmap(bitmap,(bitmap.width/2).coerceAtLeast(1),(bitmap.height/2).coerceAtLeast(1),true);if(smaller!==bitmap)bitmap.recycle();bitmap=smaller
    }} finally {bitmap.recycle()}
    error("The image is too large after resizing.")
}
@Composable internal fun SkinImageControls(vm:ClientState) {
    val context=LocalContext.current;val scope=rememberCoroutineScope();var busy by remember{mutableStateOf(false)}
    fun addLayer(id:String){if(vm.draftSkinLayers.size>=8)return;val first=vm.draftSkinLayers.none{it.placement=="banner"};vm.draftSkinLayers=vm.draftSkinLayers+SkinLayer(id,target=if(first)"header" else "cards",placement=if(first)"banner" else "overlay",fit=if(first)"cover" else "contain",anchor=if(first)"center" else "bottom-right",width=128,height=96,opacity=if(first)1.0 else .2)}
    val picker=rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()){uri->if(uri!=null)scope.launch{
        busy=true
        try {val image=withContext(Dispatchers.IO){val bytes=context.contentResolver.openInputStream(uri)?.use{it.readBytesBounded(8*1024*1024)} ?: error("Cannot read the image.");importSkinImage(bytes)};require(vm.draftSkinImages.size<4){"Use at most four images."};val id=(1..5).map{"image-$it"}.first{it !in vm.draftSkinImages};vm.draftSkinImages=vm.draftSkinImages+(id to image);addLayer(id)}catch(e:CancellationException){throw e}catch(e:Exception){vm.error=e.message}finally{busy=false}
    }}
    Text("Images",style=MaterialTheme.typography.titleMedium)
    Text("Images stay in the skin file. Decorations sit beneath text and never receive taps. Text contrast can reduce decoration opacity; banners keep their requested opacity.",style=MaterialTheme.typography.bodySmall)
    SkinOutlinedButton(onClick={picker.launch(arrayOf("image/png","image/jpeg"))},enabled=!busy && vm.draftSkinImages.size<4){Text(if(busy)"Importing…" else "Add image")}
    vm.draftSkinImages.forEach{(id,asset)->OutlinedCard(Modifier.fillMaxWidth()){Column(Modifier.padding(16.dp),verticalArrangement=Arrangement.spacedBy(8.dp)){
        Text(id,style=MaterialTheme.typography.titleSmall)
        SkinOutlinedTextField(asset.credit,{vm.draftSkinImages=vm.draftSkinImages+(id to asset.copy(credit=it.take(1024)))},Modifier.fillMaxWidth(),label={Text("Image credit")})
        Text(asset.license,style=MaterialTheme.typography.bodySmall)
        FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp),verticalArrangement=Arrangement.spacedBy(8.dp)){
            SkinOutlinedButton(onClick={addLayer(id)},enabled=vm.draftSkinLayers.size<8){Text("Add layer")}
            SkinTextButton(onClick={vm.draftSkinLayers=vm.draftSkinLayers.filter{it.image!=id};vm.draftSkinImages=vm.draftSkinImages-id}){Text("Remove image")}
        }
    }}}
    vm.draftSkinLayers.forEachIndexed{index,layer->key(index,layer.image){
        var expanded by rememberSaveable{mutableStateOf(false)}
        fun update(value:SkinLayer){vm.draftSkinLayers=vm.draftSkinLayers.toMutableList().also{it[index]=value}}
        OutlinedCard(Modifier.fillMaxWidth()){Column(Modifier.padding(16.dp),verticalArrangement=Arrangement.spacedBy(12.dp)){
            Row(verticalAlignment=Alignment.CenterVertically){Text("${index+1}. ${layer.target} · ${layer.placement}",Modifier.weight(1f),style=MaterialTheme.typography.titleSmall);SkinTextButton(onClick={expanded=!expanded}){Text(if(expanded)"Collapse" else "Edit")};SkinTextButton(onClick={vm.draftSkinLayers=vm.draftSkinLayers.filterIndexed{i,_->i!=index}}){Text("Remove")}}
            if(expanded){
                Text("Target");FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp),verticalArrangement=Arrangement.spacedBy(8.dp)){listOf("window","header","cards").forEach{v->FilterChip(selected=layer.target==v,onClick={update(layer.copy(target=v,placement=if(v!="header" && layer.placement=="banner")"overlay" else layer.placement))},label={Text(v)})}}
                Text("Placement");FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp),verticalArrangement=Arrangement.spacedBy(8.dp)){listOf("background","overlay","banner").forEach{v->FilterChip(selected=layer.placement==v,enabled=v!="banner" || vm.draftSkinLayers.filterIndexed{i,_->i!=index}.none{it.placement=="banner"},onClick={update(layer.copy(placement=v,target=if(v=="banner")"header" else layer.target,fit=if(v=="banner")"cover" else layer.fit,height=if(v=="banner")layer.height.coerceAtMost(240) else layer.height))},label={Text(v)})}}
                Text("Image fit");FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp),verticalArrangement=Arrangement.spacedBy(8.dp)){listOf("contain","cover","tile").forEach{v->FilterChip(selected=layer.fit==v,enabled=layer.placement!="banner" || v!="tile",onClick={update(layer.copy(fit=v))},label={Text(v)})}}
                if(layer.placement!="banner" && layer.fit!="tile"){Text("Anchor");FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp),verticalArrangement=Arrangement.spacedBy(8.dp)){listOf("top-left","top-right","center","bottom-left","bottom-right").forEach{v->FilterChip(selected=layer.anchor==v,onClick={update(layer.copy(anchor=v))},label={Text(v)})}}}
                if(layer.placement!="banner"){Text("Width · ${layer.width}");Slider(layer.width.toFloat(),{update(layer.copy(width=it.toInt()))},valueRange=16f..1024f,steps=62)}
                Text("Height · ${layer.height}");Slider(layer.height.toFloat(),{update(layer.copy(height=it.toInt()))},valueRange=16f..(if(layer.placement=="banner")240f else 1024f))
                Text("Opacity · ${(layer.opacity*100).toInt()}%");Slider(layer.opacity.toFloat(),{update(layer.copy(opacity=it.toDouble()))},valueRange=0f..1f)
                if(layer.placement!="banner"){Text("Horizontal offset · ${layer.offsetX}");Slider(layer.offsetX.toFloat(),{update(layer.copy(offsetX=it.toInt()))},valueRange=-256f..256f);Text("Vertical offset · ${layer.offsetY}");Slider(layer.offsetY.toFloat(),{update(layer.copy(offsetY=it.toInt()))},valueRange=-256f..256f)}
            }
        }}
    }}
}
private fun java.io.InputStream.readBytesBounded(limit:Int):ByteArray {val out=java.io.ByteArrayOutputStream();val buffer=ByteArray(16384);while(true){val n=read(buffer);if(n<0)break;require(out.size()+n<=limit){"Choose an image smaller than 8 MB."};out.write(buffer,0,n)};return out.toByteArray()}
