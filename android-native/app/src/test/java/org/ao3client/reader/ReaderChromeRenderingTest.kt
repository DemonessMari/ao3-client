package org.ao3client.reader

import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.createEmptyComposeRule
import androidx.compose.ui.unit.dp
import android.view.View
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
class ReaderChromeRenderingTest {
    @get:Rule val compose=createEmptyComposeRule()
    private fun layout(view:View){view.measure(View.MeasureSpec.makeMeasureSpec(420,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(800,View.MeasureSpec.EXACTLY));view.layout(0,0,420,800)}
    @Test fun fullscreenHeaderNeverOverlapsNotesAndExitAndFooterRemainsAccessible(){
        var notes=0;var exit=0;var next=0
        val controller=Robolectric.buildActivity(ComponentActivity::class.java)
        compose.runOnUiThread{controller.get().setTheme(android.R.style.Theme_Material_Light_NoActionBar);controller.setup().visible().get().setContent{
            MaterialTheme(colorScheme=darkColorScheme(surface=Color(0xff263c54),background=Color(0xff192734),onSurface=Color(0xffd9e4f3))){Surface(Modifier.fillMaxSize(),color=MaterialTheme.colorScheme.surface){Column{
                ReaderChapterHeader("Chapter 1 with a long title that must not cover the controls",true,true,{notes++},{exit++})
                Text("正文示例\n\nA quiet page with room for the story.\n\nReading controls share the paper background.",Modifier.weight(1f).padding(24.dp))
                ReaderPageFooter(10,15,false,true,onPrevious={},onNext={next++})
            }}}
        }}
        try{
            compose.waitForIdle();compose.runOnUiThread{layout(controller.get().window.decorView)};compose.waitForIdle()
            val a=compose.onNodeWithContentDescription("Author notes").fetchSemanticsNode().boundsInRoot
            val b=compose.onNodeWithContentDescription("Exit fullscreen").fetchSemanticsNode().boundsInRoot
            assertTrue("Notes and exit overlap",a.right<=b.left);assertTrue(b.right<=420f)
            compose.onNodeWithContentDescription("Author notes").performClick();compose.onNodeWithContentDescription("Exit fullscreen").performClick()
            compose.onNodeWithContentDescription("Previous page").assertIsNotEnabled();compose.onNodeWithContentDescription("Next page").performClick()
            compose.runOnIdle{assertEquals(1,notes);assertEquals(1,exit);assertEquals(1,next)
                if(System.getenv("AO3_CAPTURE_TOOLS")=="1"){val root=controller.get().window.decorView;val image=android.graphics.Bitmap.createBitmap(420,800,android.graphics.Bitmap.Config.ARGB_8888);root.draw(android.graphics.Canvas(image));java.io.File("/tmp/ao3-fullscreen-style.png").outputStream().use{image.compress(android.graphics.Bitmap.CompressFormat.PNG,100,it)};image.recycle()}
            }
        }finally{compose.runOnUiThread{controller.pause().stop().destroy()};compose.waitForIdle()}
    }
    @Test fun guestKudosUsesTheSameMenuRowGeometryAndDisabledBehavior(){
        var count=0;var enabled by mutableStateOf(true)
        val controller=Robolectric.buildActivity(ComponentActivity::class.java)
        compose.runOnUiThread{controller.get().setTheme(android.R.style.Theme_Material_Light_NoActionBar);controller.setup().visible().get().setContent{MaterialTheme{Column(Modifier.fillMaxSize()){
            MenuRow("Save offline",onClick={});KudosControl("Give guest kudos",enabled,true){count++};MenuRow("Series",onClick={})
        }}}}
        try{
            compose.waitForIdle();compose.runOnUiThread{layout(controller.get().window.decorView)};compose.waitForIdle()
            val before=compose.onNodeWithText("Save offline").fetchSemanticsNode().boundsInRoot
            val kudos=compose.onNodeWithText("Give guest kudos").fetchSemanticsNode().boundsInRoot
            val after=compose.onNodeWithText("Series").fetchSemanticsNode().boundsInRoot
            assertEquals(before.width,kudos.width,1f);assertEquals(before.height,kudos.height,1f);assertEquals(after.height,kudos.height,1f)
            compose.onNodeWithText("Give guest kudos").performClick();compose.runOnIdle{assertEquals(1,count);enabled=false};compose.waitForIdle();compose.onNodeWithText("Give guest kudos").assertIsNotEnabled()
        }finally{compose.runOnUiThread{controller.pause().stop().destroy()};compose.waitForIdle()}
    }
}
