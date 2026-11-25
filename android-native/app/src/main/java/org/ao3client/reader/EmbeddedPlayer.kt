package org.ao3client.reader

import android.content.Intent
import android.webkit.*
import androidx.compose.runtime.*
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.compose.ui.unit.dp
import androidx.core.net.toUri
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.Dispatchers
import java.net.URI
import java.net.InetAddress

internal class PlayerCookies(private val manager:CookieManager=CookieManager.getInstance()):java.io.Closeable {
    private val previous=manager.acceptCookie()
    private var closed=false
    init{manager.setAcceptCookie(false)}
    override fun close(){if(!closed){closed=true;manager.setAcceptCookie(previous)}}
}

@Composable internal fun EmbeddedPlayer(url:String,onClose:()->Unit) {
    val context=LocalContext.current;val scope=rememberCoroutineScope()
    var loading by remember{mutableStateOf(false)}
    var loaded by remember{mutableStateOf(false)}
    var error by remember{mutableStateOf<String?>(null)}
    var cookieSession by remember{mutableStateOf<PlayerCookies?>(null)}
    val manager=remember{CookieManager.getInstance()}
    val origins=remember{java.util.Collections.synchronizedSet(mutableSetOf<String>())}
    val view=remember{runCatching{WebView(context)}.getOrNull()}
    DisposableEffect(view) {onDispose{view?.stopLoading();view?.destroy();cookieSession?.close();synchronized(origins){origins.forEach{WebStorage.getInstance().deleteOrigin(it)}}}}
    Dialog(onDismissRequest=onClose,properties=DialogProperties(usePlatformDefaultWidth=false)) {
        Surface(Modifier.fillMaxSize()) {Column(Modifier.fillMaxSize().statusBarsPadding().navigationBarsPadding()) {
            Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.SpaceBetween){SkinTextButton(onClick=onClose){Text("Close player")};SkinTextButton(onClick={runCatching{context.startActivity(Intent(Intent.ACTION_VIEW,url.toUri()))}}){Text("Open in browser")}}
            Text(runCatching{URI(url).host}.getOrDefault("Embedded player"),Modifier.padding(16.dp),style=MaterialTheme.typography.titleMedium)
            if(error!=null)Text(error.orEmpty(),Modifier.padding(16.dp))
            if(!loaded) {
                Text("Load this player without your AO3 sign-in?",Modifier.padding(16.dp))
                SkinButton(onClick={if(!loading)scope.launch{
                    loading=true
                    try {
                        withContext(Dispatchers.IO){val uri=URI(publicImageUrl(url) ?: error("Only public HTTPS players can be loaded."));require(uri.host!="archiveofourown.org" && !uri.host.endsWith(".archiveofourown.org"));require(InetAddress.getAllByName(uri.host).let{it.isNotEmpty() && it.all{address->ExternalReading.publicAddress(address)}}){"Local network players are blocked."}}
                        cookieSession=PlayerCookies(manager);loaded=true
                    }catch(e:Exception){error=e.message ?: "Could not load this player."}finally{loading=false}
                }},enabled=!loading && view!=null,modifier=Modifier.padding(16.dp)){Text(if(loading)"Loading…" else "Load player")}
            } else if(view!=null)AndroidView(modifier=Modifier.weight(1f).fillMaxWidth(),factory={
                view.apply {
                    settings.apply{javaScriptEnabled=true;domStorageEnabled=false;databaseEnabled=false;allowFileAccess=false;allowContentAccess=false;javaScriptCanOpenWindowsAutomatically=false;setSupportMultipleWindows(false);mixedContentMode=WebSettings.MIXED_CONTENT_NEVER_ALLOW;mediaPlaybackRequiresUserGesture=true;setGeolocationEnabled(false)}
                    manager.setAcceptThirdPartyCookies(this,false)
                    webChromeClient=object:WebChromeClient(){override fun onJsAlert(v:WebView,u:String,m:String,result:JsResult):Boolean{result.cancel();return true};override fun onJsConfirm(v:WebView,u:String,m:String,result:JsResult):Boolean{result.cancel();return true};override fun onPermissionRequest(request:PermissionRequest){request.deny()};override fun onGeolocationPermissionsShowPrompt(origin:String,callback:GeolocationPermissions.Callback){callback.invoke(origin,false,false)}}
                    webViewClient=object:WebViewClient() {
                        override fun shouldInterceptRequest(v:WebView,r:WebResourceRequest):WebResourceResponse? {
                            if(r.url.scheme in setOf("data","blob","about"))return null
                            val safe=runCatching{val uri=URI(publicImageUrl(r.url.toString()) ?: error("Blocked"));require(uri.host!="archiveofourown.org" && !uri.host.endsWith(".archiveofourown.org"));require(InetAddress.getAllByName(uri.host).let{it.isNotEmpty() && it.all{address->ExternalReading.publicAddress(address)}});origins+="https://"+uri.host;true}.getOrDefault(false)
                            return if(safe)null else WebResourceResponse("text/plain","UTF-8",java.io.ByteArrayInputStream(byteArrayOf()))
                        }
                        override fun shouldOverrideUrlLoading(v:WebView,r:WebResourceRequest):Boolean {
                            val target=publicImageUrl(r.url.toString()) ?: return true
                            if(r.isForMainFrame && r.url.host!=url.toUri().host){if(r.hasGesture())runCatching{context.startActivity(Intent(Intent.ACTION_VIEW,target.toUri()))};return true}
                            return false
                        }
                    }
                    loadUrl(url,mapOf("Referer" to "https://archiveofourown.org/"))
                }
            })
        }}
    }
}
