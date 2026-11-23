package org.ao3client.reader

import android.content.Intent
import android.webkit.WebView
import android.webkit.WebViewClient
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import androidx.compose.runtime.*
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import androidx.core.net.toUri

@Composable internal fun FormattedReader(vm:ClientState,onClose:()->Unit) {
    val context=LocalContext.current
    val chapter=vm.chapters[vm.chapter]
    val fic=vm.selected ?: return
    var workSkin by remember {mutableStateOf(vm.storage.getBoolean("formattedWorkSkin",true))}
    val colors=MaterialTheme.colorScheme
    fun hex(value:androidx.compose.ui.graphics.Color)="#%06x".format(value.toArgb() and 0xffffff)
    val font by produceState<ByteArray?>(null,vm.preferences.fontFamily,vm.importedFonts) {value=withContext(Dispatchers.IO){vm.importedFonts.firstOrNull{it.id==vm.preferences.fontFamily}?.file?.takeIf{it.length()<=ReaderFontStore.MAX_BYTES}?.readBytes()}}
    val document=remember(chapter,workSkin,vm.preferences,colors.background,colors.onBackground,font) {formattedDocument(chapter,hex(colors.background),hex(colors.onBackground),vm.preferences.fontSize,vm.preferences.lineSpacing.toDouble(),vm.preferences.margin,workSkin,font,vm.preferences.fontFamily)}
    val progressKey="formatted.${fic.id}.${vm.chapter}"
    var restored by remember(vm.chapter){mutableStateOf(false)}
    var loadGeneration by remember(vm.chapter){mutableIntStateOf(0)}
    val view=remember(vm.chapter) {runCatching {object:WebView(context) {
        override fun onScrollChanged(l:Int,t:Int,oldl:Int,oldt:Int) {
            super.onScrollChanged(l,t,oldl,oldt)
            val range=(contentHeight*scale-height).toInt()
            if(range>0 && restored){val fraction=(t.toFloat()/range).coerceIn(0f,1f);vm.storage.edit().putFloat(progressKey,fraction).apply();vm.formattedProgress(fraction.toDouble())}
        }
    }}.getOrNull()}
    DisposableEffect(view) {onDispose {view?.stopLoading();view?.destroy()}}
    Dialog(onDismissRequest=onClose,properties=DialogProperties(usePlatformDefaultWidth=false)) {
        Surface(Modifier.fillMaxSize()) {Column(Modifier.fillMaxSize().statusBarsPadding().navigationBarsPadding()) {
            Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.SpaceBetween) {
                SkinTextButton(onClick=onClose){Text("Back to reader")}
                SkinTextButton(onClick={workSkin=!workSkin;vm.storage.edit().putBoolean("formattedWorkSkin",workSkin).apply()}){Text(if(workSkin)"Work skin on" else "Work skin off")}
            }
            Row(Modifier.fillMaxWidth()) {
                Text(chapter.title,Modifier.weight(1f).padding(horizontal=16.dp,vertical=12.dp),style=MaterialTheme.typography.titleMedium)
                SkinTextButton(onClick={vm.fontSize(vm.preferences.fontSize-1)}){Text("A−")}
                SkinTextButton(onClick={vm.fontSize(vm.preferences.fontSize+1)}){Text("A+")}
            }
            if(view==null)Text("Android System WebView is unavailable.",Modifier.padding(16.dp))
            else key(view) {AndroidView(modifier=Modifier.weight(1f).fillMaxWidth(),factory={
                view.apply {
                    settings.apply {javaScriptEnabled=false;domStorageEnabled=false;allowFileAccess=false;allowContentAccess=false;blockNetworkLoads=true;setSupportZoom(true);builtInZoomControls=true;displayZoomControls=false}
                    webViewClient=object:WebViewClient() {
                        override fun shouldInterceptRequest(v:WebView,r:WebResourceRequest):WebResourceResponse?=if(r.url.scheme in listOf("data","about"))null else WebResourceResponse("text/plain","UTF-8",java.io.ByteArrayInputStream(byteArrayOf()))
                        override fun shouldOverrideUrlLoading(v:WebView,r:WebResourceRequest):Boolean {
                            if(r.url.fragment!=null && r.url.toString().substringBefore('#')==v.url.orEmpty().substringBefore('#'))return false
                            if(r.hasGesture() && r.url.scheme in listOf("https","http"))runCatching {context.startActivity(Intent(Intent.ACTION_VIEW,r.url))}
                            return true
                        }
                        override fun onPageFinished(v:WebView,url:String) {val fraction=vm.storage.getFloat(progressKey,readingFraction(chapter.text,vm.positionIndex,vm.bookOffset).toFloat());val generation=loadGeneration;v.postDelayed({if(generation==loadGeneration){v.scrollTo(0,((v.contentHeight*v.scale-v.height).coerceAtLeast(0f)*fraction).toInt());restored=true}},150)}
                    }
                }
            },update={if(it.tag!=document){restored=false;loadGeneration++;it.tag=document;it.loadDataWithBaseURL("https://archiveofourown.org/works/${fic.id}",document,"text/html","UTF-8",null)}})}
            Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.SpaceBetween) {
                SkinTextButton(onClick={vm.changeChapter(vm.chapter-1)},enabled=vm.chapter>0){Text("Previous chapter")}
                Text("${vm.chapter+1} / ${vm.chapters.size}",Modifier.padding(vertical=12.dp))
                SkinTextButton(onClick={vm.changeChapter(vm.chapter+1)},enabled=vm.chapter<vm.chapters.lastIndex){Text("Next chapter")}
            }
        }}
    }
}
