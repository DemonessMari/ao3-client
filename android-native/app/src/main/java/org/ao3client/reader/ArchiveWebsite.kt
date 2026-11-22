package org.ao3client.reader

import androidx.core.net.toUri
import android.annotation.SuppressLint
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.webkit.*
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.Saver
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.ArrowForward
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.unit.dp
import java.net.URI

fun archiveWebsiteUrl(value:String):Boolean=runCatching {
    val u=URI(value)
    u.scheme=="https" && u.host=="archiveofourown.org" && u.userInfo==null && u.port in listOf(-1,443)
}.getOrDefault(false)
private class WebsiteState(val saved:Bundle?=null) {var view:WebView?=null}

@SuppressLint("SetJavaScriptEnabled")
@Composable
fun ArchiveWebsiteScreen(vm:ClientState) {
    val context=LocalContext.current
    val identityScript=remember(context){context.assets.open("session-identity.js").bufferedReader().use {it.readText()}}
    val state=rememberSaveable(vm.websiteRevision,saver=Saver<WebsiteState,Bundle>(save={value->Bundle().also {value.view?.saveState(it)}},restore={WebsiteState(it)})) {WebsiteState()}
    var problem by remember {mutableStateOf<String?>(null)}
    var rendererGone by remember {mutableStateOf(false)}
    var busy by remember {mutableStateOf(false)}
    var canBack by remember {mutableStateOf(false)}
    var canForward by remember {mutableStateOf(false)}
    var address by remember {mutableStateOf(vm.websiteUrl)}
    var menu by remember {mutableStateOf(false)}
    var clear by remember {mutableStateOf(false)}
    var upload by remember {mutableStateOf<ValueCallback<Array<Uri>>?>(null)}
    var download by rememberSaveable {mutableStateOf<String?>(null)}
    var downloadAgent by rememberSaveable {mutableStateOf<String?>(null)}
    val filePicker=rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) {result->upload?.onReceiveValue(WebChromeClient.FileChooserParams.parseResult(result.resultCode,result.data));upload=null}
    val downloadPicker=rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/octet-stream")) {uri->
        val link=download;val agent=downloadAgent;download=null;downloadAgent=null
        if(uri!=null && link!=null) {
            val id=runCatching {Regex("^/downloads/([0-9]+)/").find(URI(link).path)?.groupValues?.get(1)}.getOrNull()
            val format=id?.let {OfficialDownloads.format(link,it)}
            if(id!=null && format!=null)vm.saveOfficialDownload(id,OfficialDownload(format,link),uri,CookieManager.getInstance().getCookie(link),agent ?: "AO3Client WebView")
        }
    }
    fun outside(url:String) {
        val scheme=runCatching {URI(url).scheme}.getOrNull()
        if(scheme in listOf("https","http"))runCatching {context.startActivity(Intent(Intent.ACTION_VIEW,url.toUri()))}.onFailure {problem="No application can open this link."}
        else problem="This address cannot be opened."
    }
    val view=remember(vm.websiteRevision) {runCatching {WebView(context)}.getOrNull()}
    BackHandler {if(view?.canGoBack()==true)view.goBack() else vm.navigate(vm.websiteReturn)}
    DisposableEffect(view) {onDispose {upload?.onReceiveValue(null);upload=null;if(!rendererGone){view?.stopLoading();view?.destroy()};state.view=null}}
    Column(Modifier.fillMaxSize().statusBarsPadding().navigationBarsPadding().imePadding()) {
        Row(Modifier.fillMaxWidth().padding(horizontal=8.dp),horizontalArrangement=Arrangement.spacedBy(4.dp)) {
            IconButton(onClick={vm.navigate(vm.websiteReturn)}) {Icon(Icons.Default.Close,"Close AO3")}
            IconButton(onClick={view?.goBack()},enabled=canBack) {Icon(Icons.AutoMirrored.Filled.ArrowBack,"Previous web page")}
            IconButton(onClick={view?.goForward()},enabled=canForward) {Icon(Icons.AutoMirrored.Filled.ArrowForward,"Next web page")}
            IconButton(onClick={problem=null;if(rendererGone)vm.openWebsite(address) else view?.reload()}) {Icon(Icons.Default.Refresh,"Reload")}
            Box {
                IconButton(onClick={menu=true}) {Icon(Icons.Default.MoreVert,"More options")}
                DropdownMenu(expanded=menu,onDismissRequest={menu=false}) {
                    DropdownMenuItem(text={Text("AO3 home")},onClick={menu=false;view?.loadUrl(Archive.BASE+"/")})
                    DropdownMenuItem(text={Text("Use website sign-in in reader")},enabled=!vm.websiteImporting,onClick={
                        menu=false
                        if(view!=null && archiveWebsiteUrl(view.url.orEmpty()))view.evaluateJavascript(identityScript) {result->
                            if(state.view!==view || vm.screen!="website")return@evaluateJavascript
                            val user=runCatching {org.json.JSONTokener(result).nextValue() as? String}.getOrNull().orEmpty()
                            if(user.isBlank())problem="Sign in to AO3 in this view first."
                            else vm.useWebsiteSession(CookieManager.getInstance().getCookie(Archive.BASE+"/").orEmpty(),view.settings.userAgentString,user)
                        }
                    })
                    DropdownMenuItem(text={Text("Clear website sign-in")},onClick={menu=false;clear=true})
                    DropdownMenuItem(text={Text("Open in browser")},onClick={menu=false;outside(address)})
                }
            }
        }
        Text("AO3 website · separate sign-in",style=MaterialTheme.typography.labelSmall,modifier=Modifier.padding(horizontal=16.dp))
        if(busy || vm.loading || vm.websiteImporting)LinearProgressIndicator(Modifier.fillMaxWidth())
        problem?.let {ErrorCard(it){problem=null}}
        vm.error?.let {ErrorCard(it){vm.error=null}}
        if(view==null)Text("Android System WebView is unavailable. Update it and reopen this screen.",modifier=Modifier.padding(16.dp))
        else if(rendererGone)Text("The website renderer stopped. Use Reload to reopen it.",modifier=Modifier.padding(16.dp))
        else AndroidView(factory={
            state.view=view
            view.settings.apply {
                javaScriptEnabled=true;domStorageEnabled=true;allowFileAccess=false;allowContentAccess=false;mixedContentMode=WebSettings.MIXED_CONTENT_NEVER_ALLOW;setSupportMultipleWindows(false);javaScriptCanOpenWindowsAutomatically=false;safeBrowsingEnabled=true
            }
            CookieManager.getInstance().setAcceptCookie(true);CookieManager.getInstance().setAcceptThirdPartyCookies(view,false)
            view.webViewClient=object:WebViewClient() {
                override fun shouldOverrideUrlLoading(v:WebView,request:WebResourceRequest):Boolean {
                    val url=request.url.toString()
                    if(!request.isForMainFrame)return request.url.scheme !in listOf("https","about")
                    if(archiveWebsiteUrl(url))return false
                    if(request.hasGesture())outside(url) else problem="Navigation outside AO3 was blocked."
                    return true
                }
                override fun onRenderProcessGone(v:WebView,detail:RenderProcessGoneDetail):Boolean {
                    if(state.view===v){rendererGone=true;busy=false;state.view=null;(v.parent as? android.view.ViewGroup)?.removeView(v);v.destroy()}
                    return true
                }
                override fun onPageStarted(v:WebView,url:String?,icon:android.graphics.Bitmap?) {if(state.view===v){busy=true;problem=null}}
                override fun onPageFinished(v:WebView,url:String?) {if(state.view!==v)return;busy=false;canBack=v.canGoBack();canForward=v.canGoForward();if(url!=null && archiveWebsiteUrl(url)){address=url;vm.websiteUrl=url};CookieManager.getInstance().flush()}
                override fun onReceivedError(v:WebView,request:WebResourceRequest,error:WebResourceError) {if(state.view===v && request.isForMainFrame){busy=false;problem="AO3 could not load. Check your connection or reload."}}
                override fun onReceivedHttpError(v:WebView,request:WebResourceRequest,response:WebResourceResponse) {if(state.view===v && request.isForMainFrame)problem=when(response.statusCode) {403->"AO3 may require human verification. Complete any check on this page.";429->"AO3 rate limit reached. Try again later.";else->"AO3 returned HTTP ${response.statusCode}."}}
            }
            view.webChromeClient=object:WebChromeClient() {
                override fun onShowFileChooser(v:WebView,callback:ValueCallback<Array<Uri>>,params:FileChooserParams):Boolean {
                    if(!archiveWebsiteUrl(v.url.orEmpty()))return false
                    upload?.onReceiveValue(null);upload=callback
                    return try {filePicker.launch(params.createIntent());true} catch(_:Exception){callback.onReceiveValue(null);upload=null;problem="No file picker is available.";true}
                }
                override fun onPermissionRequest(request:PermissionRequest) {request.deny()}
                override fun onGeolocationPermissionsShowPrompt(origin:String,callback:GeolocationPermissions.Callback) {callback.invoke(origin,false,false)}
            }
            view.setDownloadListener {url,_,_,_,_->
                val id=runCatching {Regex("^/downloads/([0-9]+)/").find(URI(url).path)?.groupValues?.get(1)}.getOrNull()
                val format=id?.let {OfficialDownloads.format(url,it)}
                if(format==null)problem="This download format is not supported here. Use Open in browser."
                else {download=url;downloadAgent=view.settings.userAgentString;downloadPicker.launch(URI(url).path.substringAfterLast('/'))}
            }
            if(state.saved==null || view.restoreState(state.saved)==null)view.loadUrl(vm.websiteUrl)
            view
        },modifier=Modifier.fillMaxWidth().weight(1f))
    }
    if(clear)AlertDialog(onDismissRequest={clear=false},title={Text("Clear website sign-in?")},text={Text("Clear the embedded AO3 website’s cookies and cache? Native sign-in is separate.")},confirmButton={TextButton(onClick={
        clear=false;view?.stopLoading();view?.clearCache(true);view?.clearHistory();view?.webStorageClear();CookieManager.getInstance().removeAllCookies {CookieManager.getInstance().flush();if(state.view===view)view?.loadUrl(Archive.BASE+"/users/login")}
    }) {Text("Clear")}},dismissButton={TextButton(onClick={clear=false}) {Text("Cancel")}})
}
private fun WebView.webStorageClear() {WebStorage.getInstance().deleteAllData()}
