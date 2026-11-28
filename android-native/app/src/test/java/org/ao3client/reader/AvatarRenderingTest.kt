@file:OptIn(androidx.compose.ui.test.ExperimentalTestApi::class)
package org.ao3client.reader

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color as AndroidColor
import android.view.View
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CutCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.test.junit4.createEmptyComposeRule
import org.junit.*
import org.junit.Assert.*
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[34],manifest=Config.NONE,qualifiers="w160dp-h160dp-mdpi")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
class AvatarRenderingTest {
    @get:Rule val compose=createEmptyComposeRule()
    @Test fun rectangularImageIsCenterCroppedToCircleRegardlessOfSkinCorners() {
        val image=Bitmap.createBitmap(160,80,Bitmap.Config.ARGB_8888)
        image.eraseColor(AndroidColor.GREEN)
        val paint=android.graphics.Paint();val canvas=Canvas(image)
        paint.color=AndroidColor.RED;canvas.drawRect(0f,0f,40f,80f,paint)
        paint.color=AndroidColor.BLUE;canvas.drawRect(120f,0f,160f,80f,paint)
        val controller=Robolectric.buildActivity(ComponentActivity::class.java)
        controller.get().setTheme(android.R.style.Theme_Material_Light_NoActionBar)
        controller.get().window.addFlags(android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN)
        try {
            compose.runOnUiThread{controller.setup().visible().get().setContent {
                MaterialTheme(shapes=Shapes(small=CutCornerShape(12.dp))) {
                    Box(Modifier.fillMaxSize().background(Color.Magenta).padding(40.dp)){AvatarImage(image,"Writer","Profile picture",80.dp)}
                }
            }}
            compose.waitForIdle()
            compose.runOnUiThread {
                val root=controller.get().window.decorView
                root.measure(View.MeasureSpec.makeMeasureSpec(160,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(160,View.MeasureSpec.EXACTLY));root.layout(0,0,160,160)
            }
            compose.mainClock.advanceTimeByFrame();compose.waitForIdle()
            compose.runOnUiThread {
                val output=Bitmap.createBitmap(160,160,Bitmap.Config.ARGB_8888);controller.get().window.decorView.draw(Canvas(output))
                System.getenv("AO3_AVATAR_RENDER")?.let{File(it).outputStream().use{s->output.compress(Bitmap.CompressFormat.PNG,100,s)}}
                assertEquals(AndroidColor.MAGENTA,output.getPixel(48,48))
                assertEquals(AndroidColor.GREEN,output.getPixel(80,80))
                assertEquals(AndroidColor.GREEN,output.getPixel(44,80))
                assertEquals(AndroidColor.GREEN,output.getPixel(115,80))
                output.recycle()
            }
        } finally {compose.runOnUiThread{controller.pause().stop().destroy()};image.recycle()}
    }
}
