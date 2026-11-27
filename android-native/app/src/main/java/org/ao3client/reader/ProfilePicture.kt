package org.ao3client.reader

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.Alignment
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.platform.LocalDensity
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

internal fun decodeAvatarBitmap(bytes:ByteArray):Bitmap? {
    if(bytes.isEmpty() || bytes.size>2*1024*1024)return null
    val bounds=BitmapFactory.Options().apply{inJustDecodeBounds=true};BitmapFactory.decodeByteArray(bytes,0,bytes.size,bounds)
    if(bounds.outMimeType !in listOf("image/png","image/jpeg","image/gif") || bounds.outWidth<=0 || bounds.outHeight<=0 || bounds.outWidth>4096 || bounds.outHeight>4096 || bounds.outWidth.toLong()*bounds.outHeight>4*1024*1024)return null
    val options=BitmapFactory.Options().apply{inSampleSize=1};while(maxOf(bounds.outWidth,bounds.outHeight)/options.inSampleSize>256)options.inSampleSize*=2
    return BitmapFactory.decodeByteArray(bytes,0,bytes.size,options)
}
@Composable fun ProfilePicture(vm:ClientState,username:String,url:String?=null,alt:String="",size:Dp=80.dp) {
    key(username,url,vm.username){ProfilePictureContent(vm,username,url,alt,size)}
}
@Composable private fun ProfilePictureContent(vm:ClientState,username:String,url:String?,alt:String,size:Dp) {
    val bitmap by produceState<Bitmap?>(null,username,url,vm.username,vm.avatarRevision){value=null;try{value=withContext(Dispatchers.IO){vm.profilePictureBytes(username,url)?.let(::decodeAvatarBitmap)}}catch(e:CancellationException){throw e}catch(_:Exception){value=null}}
    Surface(modifier=Modifier.size(size).semantics(mergeDescendants=true){contentDescription=alt.ifBlank{"Profile picture for $username"}},shape=MaterialTheme.shapes.small,color=MaterialTheme.colorScheme.surfaceVariant){
        Box(Modifier.fillMaxSize(),contentAlignment=Alignment.Center){val image=bitmap;if(image!=null)Image(image.asImageBitmap(),null,Modifier.fillMaxSize(),contentScale=ContentScale.Crop) else {
            val style=if(size<60.dp)MaterialTheme.typography.titleMedium else MaterialTheme.typography.headlineLarge
            val density=LocalDensity.current
            val fontSize=with(density){minOf(style.fontSize.toPx(),(size*.55f).toPx()).toSp()}
            Text(username.take(1).uppercase(),style=style.copy(fontSize=fontSize,lineHeight=TextUnit.Unspecified),maxLines=1,softWrap=false)
        }}
    }
}
