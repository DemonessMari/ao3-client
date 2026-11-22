package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
import org.json.JSONObject
class AppSkinTest {
    private fun fixture()=javaClass.classLoader!!.getResourceAsStream("app-skin.json")!!.use{it.readBytes()}
    @Test fun sharedFormatRoundTripsAndPresetsHaveReadableContrast(){
        val skin=AppSkin.parse(fixture());assertEquals("Blue",skin.name);assertEquals(skin,AppSkin.parse(skin.json().toByteArray()))
        listOf("light","dark","sepia").forEach{val preset=AppSkin.preset(it);assertEquals(preset,AppSkin.parse(preset.json().toByteArray()))}
    }
    @Test fun rejectsUnreadableTransparentCssAndUnknownFormats(){
        fun rejects(key:String,value:Any){val o=JSONObject(String(fixture()));o.put(key,value);assertTrue("$key=$value",runCatching{AppSkin.parse(o.toString().toByteArray())}.isFailure)}
        rejects("text","#ffffff");rejects("onAccent","#174a8b");rejects("accent","#eeeeee");rejects("background","#00ffffff");rejects("surface","url(https://example.com)");rejects("version",2);rejects("css","body {display:none}");rejects("name","\nBad\nName")
        assertTrue(runCatching{AppSkin.parse(ByteArray(16385))}.isFailure)
    }
}
