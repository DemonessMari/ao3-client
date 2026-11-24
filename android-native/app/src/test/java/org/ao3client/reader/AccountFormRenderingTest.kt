package org.ao3client.reader

import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.ui.test.junit4.createEmptyComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextInput
import android.view.View
import androidx.compose.foundation.layout.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
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
class AccountFormRenderingTest {
    @get:Rule val compose=createEmptyComposeRule()
    @Test fun largeChoiceListsAreSearchableAndKeepTheSelectedServerValue() {
        val source=javaClass.classLoader!!.getResource("account-form-preferences.html")!!.readText()
        val html=source.replace("<input id='preference_time_zone' name='preference[time_zone]' value='Europe/Tallinn'>","<select id='preference_time_zone' name='preference[time_zone]'>"+(0..30).joinToString(""){"<option value='Zone/$it'>Zone $it</option>"}+"</select>")
        val form=AccountForms.parse(html,"preferences","Writer")
        val field=form.fields.single{it.name=="preference[time_zone]"}
        var values by mutableStateOf(field.values)
        val controller=Robolectric.buildActivity(ComponentActivity::class.java)
        compose.runOnUiThread {
            controller.get().setTheme(android.R.style.Theme_Material_Light_NoActionBar)
            val activity=controller.setup().visible().get()
            activity.setContent {MaterialTheme{Column(Modifier.fillMaxSize().padding(16.dp)){AccountFieldControl(form.source,field,values,true){values=it}}}}
        }
        try {
            compose.waitForIdle()
            compose.runOnUiThread{val root=controller.get().window.decorView;root.measure(View.MeasureSpec.makeMeasureSpec(420,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(800,View.MeasureSpec.EXACTLY));root.layout(0,0,420,800)}
            compose.waitForIdle();assertEquals(31,field.choices.size)
            compose.onNodeWithText("Time zone").performClick()
            compose.onNodeWithText("Find option").performTextInput("19")
            compose.onNodeWithText("Zone 19").performClick()
            compose.runOnIdle{assertEquals(listOf("Zone/19"),values)}
        } finally {compose.runOnUiThread{controller.pause().stop().destroy()};compose.waitForIdle()}
    }
}
