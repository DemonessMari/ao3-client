package org.ao3client.reader

import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.ui.test.junit4.createEmptyComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import android.widget.EditText
import android.view.View
import android.view.ViewGroup
import android.graphics.Typeface
import androidx.compose.foundation.layout.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.junit.Assert.*
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[34],manifest=Config.NONE,qualifiers="w420dp-h800dp-mdpi")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
class AuthoringRenderingTest {
    @get:Rule val compose=createEmptyComposeRule()
    @Test fun nativeSelectionToolbarUpdatesHtmlAndModeChangesKeepTheDraft() {
        val original="<p>Selected words</p>";var value by mutableStateOf(original)
        val field=AccountField("work[chapter_attributes][content]","Work text","area",listOf(original))
        val controller=Robolectric.buildActivity(ComponentActivity::class.java)
        fun find(v:View):EditText?=if(v is EditText)v else if(v is ViewGroup)(0 until v.childCount).firstNotNullOfOrNull{find(v.getChildAt(it))} else null
        compose.runOnUiThread{controller.get().setTheme(android.R.style.Theme_Material_Light_NoActionBar);controller.setup().visible().get().setContent{MaterialTheme{Column(Modifier.fillMaxSize().padding(16.dp)){AuthoringEditor("test",field,value,true,Typeface.MONOSPACE){value=it}}}}}
        try {
            compose.waitForIdle();compose.runOnUiThread{val root=controller.get().window.decorView;root.measure(View.MeasureSpec.makeMeasureSpec(420,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(800,View.MeasureSpec.EXACTLY));root.layout(0,0,420,800)};compose.waitForIdle();compose.runOnIdle{assertEquals(original,value);val edit=find(controller.get().window.decorView)!!;assertSame(Typeface.MONOSPACE,edit.typeface);edit.requestFocus();edit.setSelection(0,8)}
            compose.onNodeWithText("Bold").performClick();compose.runOnIdle{assertEquals("<p><strong>Selected</strong> words</p>",value);if(System.getenv("AO3_CAPTURE_TOOLS")=="1"){val root=controller.get().window.decorView;val bitmap=android.graphics.Bitmap.createBitmap(420,800,android.graphics.Bitmap.Config.ARGB_8888);root.draw(android.graphics.Canvas(bitmap));java.io.File("/tmp/ao3-rich-android.png").outputStream().use{bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG,100,it)};bitmap.recycle()}}
            compose.onNodeWithText("HTML").performClick();compose.runOnIdle{assertTrue(value.contains("<strong>Selected</strong>"))}
            compose.onNodeWithText("Visual").performClick();compose.runOnIdle{assertTrue(value.contains("<strong>Selected</strong>"))}
        } finally {compose.runOnUiThread{controller.pause().stop().destroy()};compose.waitForIdle()}
    }
    @Test fun visualEditorUsesInputSkinColorsAndScaledTypography(){
        val field=AccountField("chapter[content]","Work text","area",listOf("<p>Preview</p>"))
        val skin=AppSkin(version=3,components=mapOf("inputs" to mapOf("background" to "#18304a","text" to "#ffffff","borderWidth" to 0,"cornerRadius" to 12,"textScale" to 1.25,"padding" to 4)))
        val controller=Robolectric.buildActivity(ComponentActivity::class.java)
        var expectedSize=0f
        fun find(v:View):EditText?=if(v is EditText)v else if(v is ViewGroup)(0 until v.childCount).firstNotNullOfOrNull{find(v.getChildAt(it))} else null
        compose.runOnUiThread{controller.get().setTheme(android.R.style.Theme_Material_Light_NoActionBar);controller.setup().visible().get().setContent{MaterialTheme{CompositionLocalProvider(LocalAppSkin provides skin,LocalSkinTypography provides MaterialTheme.typography,androidx.compose.ui.platform.LocalDensity provides androidx.compose.ui.unit.Density(1.2f,1.1f)){expectedSize=with(androidx.compose.ui.platform.LocalDensity.current){20.sp.toPx()};AuthoringEditor("skin-test",field,field.values.first(),true){}}}}}
        try{compose.waitForIdle();compose.runOnIdle{val edit=find(controller.get().window.decorView)!!;assertEquals(android.graphics.Color.WHITE,edit.currentTextColor);assertEquals(android.graphics.Color.parseColor("#18304a"),(edit.background as android.graphics.drawable.ColorDrawable).color);assertEquals(expectedSize,edit.textSize,.1f)}}finally{compose.runOnUiThread{controller.pause().stop().destroy()};compose.waitForIdle()}
    }

}
