@file:OptIn(androidx.compose.ui.test.ExperimentalTestApi::class)

package org.ao3client.reader

import android.graphics.Bitmap
import android.graphics.Canvas
import android.view.View
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusManager
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.layout.boundsInWindow
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.text.input.TextFieldValue
import androidx.compose.ui.unit.dp
import org.junit.Assert.*
import org.junit.Test
import org.junit.Rule
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.junit4.createEmptyComposeRule
import androidx.compose.ui.test.hasSetTextAction
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.performTextInput
import androidx.compose.ui.test.performTextInputSelection
import androidx.compose.ui.text.TextRange
import androidx.compose.ui.focus.onFocusChanged
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.Closeable
import java.io.File
import kotlin.math.abs
import kotlin.math.roundToInt

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[34],manifest=Config.NONE,qualifiers="w420dp-h800dp-mdpi")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
class SkinInputRenderingTest {
    @get:Rule val compose=createEmptyComposeRule()
    private data class Case(val width:Int?=1,val selectionValue:Boolean=false,val focused:Boolean=false,val label:Boolean=true,val supporting:Boolean=false,val filled:Boolean=false,val enabled:Boolean=true,val radius:Int=4)
    private class Render(val bitmap:Bitmap,val bounds:Rect,val focused:Boolean) {
        val x=(bounds.left+bounds.width*.75f).roundToInt()
        val top=bounds.top.roundToInt()
        fun rows(color:Int)= (top until bounds.bottom.roundToInt()).filter{close(bitmap.getPixel(x,it),color)}
        fun save(name:String){File(System.getProperty("java.io.tmpdir"),name).outputStream().use{bitmap.compress(Bitmap.CompressFormat.PNG,100,it)}}
    }
    private class Field(private val source:AppSkin,private val compose:ComposeTestRule):Closeable {
        private val controller=Robolectric.buildActivity(ComponentActivity::class.java)
        private val activity:ComponentActivity
        private val state=mutableStateOf(Case())
        val text=mutableStateOf("")
        val editor=mutableStateOf(TextFieldValue(""))
        private val requester=FocusRequester()
        private var focus:FocusManager?=null
        private var bounds=Rect.Zero
        private var focused=false
        init {
            controller.get().setTheme(android.R.style.Theme_Material_Light_NoActionBar)
            activity=compose.runOnUiThread{controller.setup().visible().get()}
            compose.runOnUiThread {activity.setContent {
                val case=state.value
                val inputs=source.components.getValue("inputs").toMutableMap().apply {if(case.width==null)remove("borderWidth") else put("borderWidth",case.width);put("cornerRadius",case.radius)}
                val skin=source.copy(components=source.components+("inputs" to inputs))
                CompositionLocalProvider(LocalAppSkin provides skin) {
                    MaterialTheme(colorScheme=skin.colorScheme()) {
                        focus=LocalFocusManager.current
                        Column(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background).padding(16.dp)) {
                            val modifier=Modifier.fillMaxWidth().focusRequester(requester).onGloballyPositioned{bounds=it.boundsInWindow()}.onFocusChanged{focused=it.isFocused}
                            val label:(@androidx.compose.runtime.Composable ()->Unit)?=if(case.label)({Text("Title")})else null
                            if(case.selectionValue)SkinOutlinedTextField(editor.value,{editor.value=it},modifier,enabled=case.enabled,label=label,singleLine=true)
                            else SkinOutlinedTextField(text.value,{text.value=it},modifier,enabled=case.enabled,label=label,supportingText=if(case.supporting)({Text("Helper text")})else null,singleLine=true)
                        }
                    }
                }
            }}
        }
        fun draw(case:Case):Render {
            compose.runOnUiThread {focus?.clearFocus(force=true);state.value=case;text.value=if(case.filled)"Tag value" else "";editor.value=TextFieldValue(text.value)}
            compose.waitForIdle()
            val root=activity.window.decorView
            compose.runOnUiThread {root.measure(View.MeasureSpec.makeMeasureSpec(420,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(800,View.MeasureSpec.EXACTLY));root.layout(0,0,420,800)}
            compose.waitForIdle()
            if(case.focused && case.enabled)compose.runOnUiThread{requester.requestFocus()}
            compose.mainClock.advanceTimeBy(500);compose.waitForIdle()
            return compose.runOnUiThread {
                assertTrue("Input was not laid out",bounds.width>100)
                assertEquals("Focus did not settle",case.focused && case.enabled,focused)
                val bitmap=Bitmap.createBitmap(root.width,root.height,Bitmap.Config.ARGB_8888);root.draw(Canvas(bitmap));Render(bitmap,bounds,focused)
            }
        }
        override fun close(){compose.runOnUiThread{controller.pause().stop().destroy()};compose.waitForIdle()}
    }
    @Test fun customOutlineAndFillShareTheirTopEdge() {
        val skin=skin("illustrated/nebula.json");val border=color(skin,"border");val fill=color(skin,"background")
        Field(skin,compose).use {field->
            for(width in 1..3)for(selection in listOf(false,true))for(focused in listOf(false,true)) {
                val image=field.draw(Case(width=width,selectionValue=selection,focused=focused,filled=focused,radius=if(width==3)0 else 4))
                image.save("ao3-input-outline.png");if(width==1 && !selection && !focused)image.save("ao3-input-fixed-empty.png")
                val edge=image.rows(border).firstOrNull();assertNotNull("No input outline: width=$width selection=$selection focus=$focused",edge)
                assertTrue("Background stops below outline: width=$width selection=$selection focus=$focused",close(image.bitmap.getPixel(image.x,edge!!+width+2),fill))
            }
        }
    }
    @Test fun supportingTextStaysOutsideTheOutline() {
        for(theme in listOf("illustrated/nebula.json","neos/charcoal-rose.json")) {
            val skin=skin(theme)
            Field(skin,compose).use {field->
                val image=field.draw(Case(focused=true,filled=true,supporting=true))
                val outline=image.rows(color(skin,"border"));assertTrue(outline.isNotEmpty())
                assertTrue("Floating label has no clearance",outline.first()>image.top)
                assertTrue("Supporting text is inside the outline",outline.last()<image.bounds.bottom-8)
                assertTrue(close(image.bitmap.getPixel(image.x,outline.first()+3),color(skin,"background")))
                image.save("ao3-input-supporting-${if(theme.contains("nebula"))"blue" else "rose"}.png")
            }
        }
    }
    @Test fun zeroBorderAndUnlabelledInputsKeepTheirFill() {
        val skin=skin("illustrated/nebula.json")
        Field(skin,compose).use {field->
            for(label in listOf(false,true))for(focused in listOf(false,true)) {
                val image=field.draw(Case(width=0,label=label,focused=focused,filled=focused,radius=0))
                image.save("ao3-input-zero-$label-$focused.png")
                assertTrue("Zero-width border became visible: label=$label focused=$focused",image.rows(color(skin,"border")).isEmpty())
                assertTrue(close(image.bitmap.getPixel(image.x,image.top+(if(label)8 else 0)+3),color(skin,"background")))
                val plain=field.draw(Case(width=null,label=label,focused=focused,filled=focused))
                val outline=plain.rows(color(skin,"border"));assertTrue(outline.isNotEmpty());assertTrue(close(plain.bitmap.getPixel(plain.x,outline.first()+(if(focused)2 else 1)+2),color(skin,"background")))
            }
            field.draw(Case(enabled=false)).save("ao3-input-disabled.png")
        }
    }
    @Test fun bothValueOverloadsKeepEditingAndSelection() {
        val skin=skin("illustrated/nebula.json")
        Field(skin,compose).use {field->
            for(selection in listOf(false,true)) {
                field.draw(Case(selectionValue=selection,focused=true))
                val input=compose.onAllNodes(hasSetTextAction()).onFirst()
                input.performTextInput("ships");input.performTextInputSelection(TextRange(1,4));input.performTextInput("X")
                compose.runOnIdle {assertEquals("sXs",if(selection)field.editor.value.text else field.text.value);if(selection)assertEquals(TextRange(2),field.editor.value.selection)}
            }
        }
    }
    private fun skin(path:String)=AppSkin.parse(javaClass.classLoader!!.getResource(path).readBytes())
    private fun color(skin:AppSkin,key:String)=android.graphics.Color.parseColor(skin.components.getValue("inputs").getValue(key) as String)
    companion object {
        private fun close(a:Int,b:Int)=listOf(16,8,0).all{abs((a shr it and 255)-(b shr it and 255))<35}
    }
}
