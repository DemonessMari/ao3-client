@file:OptIn(androidx.compose.ui.test.ExperimentalTestApi::class)
package org.ao3client.reader

import android.app.Application
import android.view.View
import android.graphics.Bitmap
import android.graphics.Canvas
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.createEmptyComposeRule
import androidx.compose.ui.semantics.*
import androidx.compose.ui.text.TextLayoutResult
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.graphics.isSpecified
import kotlinx.coroutines.cancel
import org.junit.*
import org.junit.Assert.*
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.net.HttpURLConnection

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[34],manifest=Config.NONE)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
class PhoneLayoutMatrixTest {
    @get:Rule val compose=createEmptyComposeRule()
    private val title="A long sample title for testing narrow screens and large text"
    private val story=("The train stopped beside a quiet station. A traveller opened a book and waited.\n\n正文示例：这一段用于检查字体、换行和阅读页面的间距。\n\n").repeat(12)
    private fun resource(name:String)=javaClass.classLoader!!.getResource(name)!!.readText()
    private fun matrix(name:String,width:Int,height:Int,font:Float=1f,skin:Boolean=false,theme:String="dark"){
        RuntimeEnvironment.setFontScale(font)
        val app=RuntimeEnvironment.getApplication()
        app.getSharedPreferences("native-reader",0).edit().clear().commit()
        val fic=Fic("424242",title,"Sample Writer","Sample Fandom", "A sample summary long enough to wrap across several lines on a compact phone. This is local layout-test data.","Friendship · Adventure · A deliberately longer tag for wrapping","Words: 12,345 · Chapters: 3/3 · Kudos: 42","en","/users/Writer","General Audiences","No Archive Warnings Apply")
        val listing="<ol class='work index group'>"+(1..3).joinToString(""){i->"<li class='work blurb' id='work_${424240+i}'><h4 class='heading'><a href='/works/${424240+i}'>$title $i</a><a rel='author' href='/users/Writer'>Sample Writer</a></h4><blockquote class='summary'>${fic.summary}</blockquote><dl class='stats'><dd>12,345 words</dd></dl></li>"}+"</ol>"
        val profile="<div class='user profile'><h3 class='heading'>A sample profile</h3><dl class='meta'><dt>Joined</dt><dd>A while ago</dd></dl><div class='bio'><blockquote class='userstuff'>A biography with enough text to check wrapping at larger font sizes.</blockquote></div></div>"
        val vm=ClientState(app,false,{prefs->Archive(prefs,connectionFactory={url->
            val body=when{url.path=="/works/new"->resource("native-tools-work.html").replace("&lt;p&gt;Story &amp; notes&lt;/p&gt;", "&lt;p&gt;Sample writing&lt;/p&gt;").replace("\n&lt;form action='/works/666' method='post'&gt;Not a form&lt;/form&gt;", "");url.path=="/works/search"->listing;else->profile}
            object:HttpURLConnection(url){override fun connect(){};override fun disconnect(){};override fun usingProxy()=false;override fun getResponseCode()=200;override fun getInputStream():java.io.InputStream{check(requestMethod=="GET"){"Layout tests must not submit forms"};return body.byteInputStream()};override fun getHeaderFields():Map<String,List<String>> = emptyMap()}
        })})
        vm.libraryEntries=(1..3).map{LibraryEntry(fic.copy(id="${424240+it}",title="$title $it"),1700000000000L,true,it==1)}
        vm.selected=fic;vm.chapters=listOf(Chapter("Chapter 1 with a long title",story,"Sample notes before the chapter."),Chapter("Chapter 2",story));vm.preferences=vm.preferences.copy(theme=theme)
        if(skin)vm.applySkin(AppSkin.parse(resource("illustrated/nebula.json").toByteArray()))
        val controller=Robolectric.buildActivity(MainActivity::class.java)
        val reports=JSONArray();val problems=mutableListOf<String>()
        val output=System.getenv("AO3_LAYOUT_EVIDENCE")?.let{File(it,name).apply{mkdirs()}}
        fun arrange(){val root=controller.get().window.decorView;root.measure(View.MeasureSpec.makeMeasureSpec(width,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(height,View.MeasureSpec.EXACTLY));root.layout(0,0,width,height)}
        fun capture(screen:String,minViewport:Boolean=true){
            compose.waitForIdle();compose.runOnUiThread{org.robolectric.shadows.ShadowLooper.idleMainLooper();controller.get().window.decorView.forceLayout();arrange()};compose.mainClock.advanceTimeByFrame();compose.waitForIdle()
            val nodes=compose.onAllNodes(SemanticsMatcher("all"){true},useUnmergedTree=true).fetchSemanticsNodes()
            val scrollHeight=nodes.filter{it.config.contains(SemanticsProperties.VerticalScrollAxisRange)}.maxOfOrNull{it.boundsInRoot.height} ?: 0f
            val overflowing=mutableListOf<String>()
            for(node in nodes){if(node.boundsInRoot.width<=0 || node.boundsInRoot.height<=0)continue
                val results=mutableListOf<TextLayoutResult>();node.config.getOrNull(SemanticsActions.GetTextLayoutResult)?.action?.invoke(results)
                for(result in results){
                    if(result.didOverflowHeight && result.lineCount>0 && !result.isLineEllipsized(result.lineCount-1))overflowing+=result.layoutInput.text.text.take(70)
                    if(result.layoutInput.text.text in setOf("Home","Recent reads","Summary","Biography","Reading appearance") && result.layoutInput.style.color.isSpecified){
                        val bg=android.graphics.Color.parseColor(vm.appSkin?.background ?: AppSkin.preset(vm.preferences.theme).background)
                        if(androidx.core.graphics.ColorUtils.calculateContrast(result.layoutInput.style.color.toArgb(),bg)<4.5)problems+="$screen: low-contrast text ${result.layoutInput.text.text}"
                    }
                }
            }
            if(minViewport && scrollHeight<minOf(height*.4f,160f))problems+="$screen: scroll viewport only ${scrollHeight.toInt()}px"
            if(overflowing.isNotEmpty())problems+="$screen: vertically clipped text: ${overflowing.joinToString()}"
            reports.put(JSONObject().put("screen",screen).put("scrollHeight",scrollHeight.toDouble()).put("overflow",JSONArray(overflowing)).put("readingOptionsCount",nodes.count{it.config.getOrNull(SemanticsProperties.ContentDescription)?.contains("Reading options")==true}).put("fullscreen",vm.fullScreen).put("textOnly",vm.textOnly))
            if(output!=null)compose.runOnUiThread{val image=Bitmap.createBitmap(width,height,Bitmap.Config.ARGB_8888);val canvas=Canvas(image);val windows=android.view.inspector.WindowInspector.getGlobalWindowViews().filter{it.visibility==View.VISIBLE && it.isAttachedToWindow && it.width>0};val base=controller.get().window.decorView;val roots=if(screen.startsWith("reader-menu") && windows.isNotEmpty())listOf(base,windows.last()).distinct() else listOf(base);for(root in roots){val xy=IntArray(2);root.getLocationOnScreen(xy);canvas.save();canvas.translate(xy[0].toFloat(),xy[1].toFloat());root.draw(canvas);canvas.restore()};File(output,"$screen.png").outputStream().use{image.compress(Bitmap.CompressFormat.PNG,100,it)};image.recycle()}
        }
        fun screen(value:String){compose.runOnUiThread{vm.fullScreen=false;vm.textOnly=false;vm.screen=value};compose.waitForIdle()}
        try{
            compose.runOnUiThread{controller.create();controller.get().viewModelStore.put("androidx.lifecycle.ViewModelProvider.DefaultKey:"+ClientState::class.java.canonicalName,vm);controller.start().resume().visible()}
            compose.waitForIdle();compose.runOnUiThread{assertSame(vm,ViewModelProvider(controller.get())[ClientState::class.java])}
            screen("home");capture("home")
            screen("search");compose.runOnUiThread{vm.query="Sample";vm.search()};compose.waitUntil(10000){org.robolectric.shadows.ShadowLooper.idleMainLooper();!vm.loading};capture("search");compose.onAllNodes(hasScrollToIndexAction()).onFirst().performScrollToNode(hasText("$title 1"));capture("search-results")
            screen("filters");capture("filters");compose.onNodeWithText("Apply and search").assertIsDisplayed()
            screen("details");capture("details");compose.onNodeWithText("Read fic").assertIsDisplayed()
            screen("library");capture("library")
            screen("login");capture("login")
            screen("settings");capture("settings");compose.onNodeWithText("Save language and translation").performScrollTo();capture("settings-bottom")
            compose.runOnUiThread{vm.showSkins()};compose.waitForIdle();capture("skins")
            compose.runOnUiThread{vm.username="Writer"};screen("account");capture("account");compose.onNodeWithText("Sign out").performScrollTo();capture("account-bottom")
            compose.runOnUiThread{vm.profileUser="Writer";vm.profile=AuthorProfile("Writer","Sample profile", "Joined a while ago", "A longer biography for testing text wrapping and scrolling. ".repeat(8));vm.profileTab="profile";vm.entitySubscriptions["User:Writer"]=false};screen("profile");capture("profile")
            compose.runOnUiThread{vm.showTools("/works/new")};compose.waitUntil(10000){org.robolectric.shadows.ShadowLooper.idleMainLooper();!vm.loading && vm.toolPage!=null};capture("editor")
            compose.runOnUiThread{vm.username=null;vm.bookMode=false};screen("reader");capture("reader-scroll")
            compose.onNodeWithContentDescription("Reading options").performClick();compose.waitForIdle();capture("reader-menu",false);compose.onNodeWithText("Give guest kudos").performScrollTo();capture("reader-menu-kudos",false)
            compose.runOnUiThread{controller.get().onBackPressedDispatcher.onBackPressed()};compose.waitForIdle()
            compose.runOnUiThread{vm.readingMode(book=true);vm.bookOffset=0};compose.waitForIdle()
            compose.onNodeWithContentDescription("Reading options").performClick();compose.onNodeWithText("Fullscreen reading").performScrollTo().performClick();compose.waitForIdle()
            compose.waitUntil(10000){org.robolectric.shadows.ShadowLooper.idleMainLooper();compose.onAllNodes(hasContentDescription("Next page")).fetchSemanticsNodes().isNotEmpty()};compose.waitUntil(10000){org.robolectric.shadows.ShadowLooper.idleMainLooper();compose.onAllNodes(hasContentDescription("Reading options")).fetchSemanticsNodes().isEmpty()};capture("reader-fullscreen",false)
            val notes=compose.onNodeWithContentDescription("Author notes").fetchSemanticsNode().boundsInRoot;val close=compose.onNodeWithContentDescription("Exit fullscreen").fetchSemanticsNode().boundsInRoot
            if(notes.right>close.left)problems+="reader-fullscreen: notes/exit overlap"
            compose.runOnUiThread{vm.textOnly=true};compose.waitUntil(10000){org.robolectric.shadows.ShadowLooper.idleMainLooper();compose.onAllNodes(hasContentDescription("The train stopped",substring=true)).fetchSemanticsNodes().isNotEmpty()};capture("reader-text-only",false)
            compose.onNodeWithContentDescription("Exit fullscreen").assertDoesNotExist()
        }finally{
            output?.let{File(it,"report.json").writeText(JSONObject().put("width",width).put("height",height).put("fontScale",font.toDouble()).put("skin",skin).put("theme",theme).put("screens",reports).put("problems",JSONArray(problems)).toString(2))}
            compose.runOnUiThread{controller.pause().stop().destroy();vm.viewModelScope.cancel()};compose.waitForIdle()
        }
        assertTrue("$name:\n"+problems.joinToString("\n"),problems.isEmpty())
    }
    @Test @Config(qualifiers="w320dp-h640dp-mdpi") fun compactPhone()=matrix("compact",320,640)
    @Test @Config(qualifiers="w390dp-h844dp-mdpi") fun standardPhone()=matrix("standard",390,844,theme="light")
    @Test @Config(qualifiers="w780dp-h360dp-land-mdpi") fun landscapePhone()=matrix("landscape",780,360)
    @Test @Config(qualifiers="w360dp-h740dp-mdpi") fun largeTextPhone()=matrix("large-text",360,740,1.8f)
    @Test @Config(qualifiers="w360dp-h780dp-mdpi") fun skinnedPhone()=matrix("nebula",360,780,1.15f,true)
    @Test @Config(qualifiers="w780dp-h360dp-land-mdpi") fun skinnedLandscapePhone()=matrix("nebula-landscape",780,360,1.3f,true)
}
