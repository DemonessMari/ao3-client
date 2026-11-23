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
    @Test fun nativeLayoutTokensRoundTripAndRejectUnsafeRanges() {
        val skin=AppSkin.parse(fixture()).copy(version=2,uiScale=1.25,cornerRadius=0)
        assertEquals(skin,AppSkin.parse(skin.json().toByteArray()))
        assertEquals(1,AppSkin.parse(fixture()).version)
        for(bad in listOf(skin.copy(uiScale=.5),skin.copy(uiScale=2.0),skin.copy(cornerRadius=-1),skin.copy(cornerRadius=25)))assertTrue(runCatching{AppSkin.parse(bad.json().toByteArray())}.isFailure)
        assertTrue(runCatching{AppSkin.parse(skin.json().replace("\"cornerRadius\": 0","\"cornerRadius\": 1.5").toByteArray())}.isFailure)
    }
    @Test fun independentComponentsRoundTripAndRejectUnsafeRules() {
        val bytes=javaClass.classLoader!!.getResourceAsStream("app-skin-v3.json")!!.use{it.readBytes()}
        val skin=AppSkin.parse(bytes)
        assertEquals(3,skin.version);assertEquals(4,skin.components.size)
        assertEquals(skin,AppSkin.parse(skin.json().toByteArray()))
        assertEquals(0,skin.components.getValue("buttons")["cornerRadius"])
        assertEquals(12,skin.components.getValue("cards")["cornerRadius"])
        fun rejects(role:String,key:String,value:Any){val o=JSONObject(String(bytes));o.getJSONObject("components").getJSONObject(role).put(key,value);assertTrue("$role/$key",runCatching{AppSkin.parse(o.toString().toByteArray())}.isFailure)}
        rejects("buttons","text","#174a8b");rejects("inputs","border","#f5f9ff")
        rejects("cards","padding",25);rejects("cards","padding",1.5);rejects("header","textScale",.5)
        rejects("buttons","borderWidth",4);rejects("inputs","css","display:none")
        val o=JSONObject(String(bytes));o.getJSONObject("components").put("menus",JSONObject())
        assertTrue(runCatching{AppSkin.parse(o.toString().toByteArray())}.isFailure)
    }
}
