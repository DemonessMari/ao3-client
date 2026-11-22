package org.ao3client.reader

import org.json.JSONObject
import kotlin.math.pow

/** Shared with the desktop's ao3-client-skin version 1 format. */
data class AppSkin(val name:String="Light",val background:String="#fff8f5",val surface:String="#ffffff",val text:String="#292723",val accent:String="#920b19",val onAccent:String="#ffffff") {
    fun json():String=JSONObject().put("format","ao3-client-skin").put("version",1).put("name",name).put("background",background).put("surface",surface).put("text",text).put("accent",accent).put("onAccent",onAccent).toString(2)
    companion object {
        fun preset(theme:String)=when(theme){"dark"->AppSkin("Dark","#191b1e","#25272b","#e7e1d8","#ffb3b9","#350009");"sepia"->AppSkin("Sepia","#f1e3c7","#fff2d9","#292723","#843d21","#ffffff");else->AppSkin()}
        fun luminance(hex:String):Double {
            fun channel(i:Int):Double {val v=hex.substring(i,i+2).toInt(16)/255.0;return if(v<=0.04045)v/12.92 else ((v+.055)/1.055).pow(2.4)}
            return .2126*channel(1)+.7152*channel(3)+.0722*channel(5)
        }
        fun contrast(a:String,b:String):Double {val x=luminance(a);val y=luminance(b);return (maxOf(x,y)+.05)/(minOf(x,y)+.05)}
        fun parse(bytes:ByteArray):AppSkin {
            require(bytes.size<=16384){"Skin files must be smaller than 16 KB."}
            val o=try{JSONObject(bytes.toString(Charsets.UTF_8))}catch(e:Exception){error("Choose an AO3 Client skin JSON file (version 1).")}
            require(o.optString("format")=="ao3-client-skin" && o.opt("version") is Number && o.getDouble("version")==1.0){"Choose an AO3 Client skin JSON file (version 1)."}
            val keys=setOf("format","version","name","background","surface","text","accent","onAccent")
            require(o.keys().asSequence().all{it in keys}){"The skin contains unknown fields."}
            val name=(o.opt("name") as? String).orEmpty().trim()
            require(name.isNotEmpty() && name.length<=60 && name.none{it.code<32 || it.code==127}){"Give the skin a name of 1–60 characters."}
            fun color(key:String):String {val value=(o.opt(key) as? String).orEmpty();require(Regex("^#[0-9a-fA-F]{6}$").matches(value)){"Use opaque #RRGGBB colors for $key."};return value.lowercase()}
            val skin=AppSkin(name,color("background"),color("surface"),color("text"),color("accent"),color("onAccent"))
            require(contrast(skin.text,skin.background)>=4.5 && contrast(skin.text,skin.surface)>=4.5 && contrast(skin.onAccent,skin.accent)>=4.5 && contrast(skin.accent,skin.background)>=4.5 && contrast(skin.accent,skin.surface)>=4.5){"Increase contrast: text and accents need 4.5:1 against their backgrounds."}
            return skin
        }
    }
}
