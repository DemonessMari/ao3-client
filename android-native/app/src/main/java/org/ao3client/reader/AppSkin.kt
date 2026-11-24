package org.ao3client.reader

import org.json.JSONObject
import kotlin.math.pow

/** Shared with the desktop's ao3-client-skin version 1/2/3/4 format. */
data class AppSkin(val name:String="Light",val background:String="#fff8f5",val surface:String="#ffffff",val text:String="#292723",val accent:String="#920b19",val onAccent:String="#ffffff",val uiScale:Double=1.0,val cornerRadius:Int=8,val version:Int=1,val components:Map<String,Map<String,Any>> = emptyMap(),val images:Map<String,SkinImage> = emptyMap(),val layers:List<SkinLayer> = emptyList()) {
    fun json():String=JSONObject().put("format","ao3-client-skin").put("version",version).put("name",name).put("background",background).put("surface",surface).put("text",text).put("accent",accent).put("onAccent",onAccent).also{if(version>=2)it.put("uiScale",uiScale).put("cornerRadius",cornerRadius);if(version>=3)it.put("components",JSONObject(components.mapValues{JSONObject(it.value)}));if(version>=4)it.put("images",JSONObject(images.mapValues{it.value.json()})).put("layers",org.json.JSONArray().also{a->layers.forEach{layer->a.put(layer.json())}})}.toString(2)
    companion object {
        fun preset(theme:String)=when(theme){"dark"->AppSkin("Dark","#191b1e","#25272b","#e7e1d8","#ffb3b9","#350009");"sepia"->AppSkin("Sepia","#f1e3c7","#fff2d9","#292723","#843d21","#ffffff");else->AppSkin()}
        fun luminance(hex:String):Double {
            fun channel(i:Int):Double {val v=hex.substring(i,i+2).toInt(16)/255.0;return if(v<=0.04045)v/12.92 else ((v+.055)/1.055).pow(2.4)}
            return .2126*channel(1)+.7152*channel(3)+.0722*channel(5)
        }
        fun contrast(a:String,b:String):Double {val x=luminance(a);val y=luminance(b);return (maxOf(x,y)+.05)/(minOf(x,y)+.05)}
        fun parse(bytes:ByteArray):AppSkin {
            require(bytes.size<=SKIN_MAX_BYTES){"Skin files must be at most 4 MB."}
            val o=try{JSONObject(bytes.toString(Charsets.UTF_8))}catch(e:Exception){error("Choose an AO3 Client skin JSON file (version 1, 2, 3 or 4).")}
            require(o.optString("format")=="ao3-client-skin" && o.opt("version") is Number && o.getDouble("version") in listOf(1.0,2.0,3.0,4.0)){"Choose an AO3 Client skin JSON file (version 1, 2, 3 or 4)."}
            val keys=((setOf("format","version","name","background","surface","text","accent","onAccent") + if(o.getInt("version")>=2)setOf("uiScale","cornerRadius") else emptySet()) + if(o.getInt("version")>=3)setOf("components") else emptySet()) + if(o.getInt("version")==4)setOf("images","layers") else emptySet()
            require(o.getInt("version")==4 || bytes.size<=16384){"Legacy skin files must be at most 16 KB."}
            require(o.keys().asSequence().all{it in keys}){"The skin contains unknown fields."}
            val name=(o.opt("name") as? String).orEmpty().trim()
            require(name.isNotEmpty() && name.length<=60 && name.none{it.code<32 || it.code==127}){"Give the skin a name of 1–60 characters."}
            fun color(key:String):String {val value=(o.opt(key) as? String).orEmpty();require(Regex("^#[0-9a-fA-F]{6}$").matches(value)){"Use opaque #RRGGBB colors for $key."};return value.lowercase()}
            val version=o.getInt("version")
            val scale=if(version>=2){require(o.opt("uiScale") is Number);o.getDouble("uiScale")}else 1.0
            val radius=if(version>=2){require(o.opt("cornerRadius") is Number);o.getDouble("cornerRadius")}else 8.0
            require(scale.isFinite() && scale in 1.0..1.5){"Interface scale must be between 1 and 1.5."}
            require(radius.isFinite() && radius in 0.0..24.0 && radius%1.0==0.0){"Corner radius must be a whole number from 0 to 24."}
            val skin=AppSkin(name,color("background"),color("surface"),color("text"),color("accent"),color("onAccent"),scale,radius.toInt(),version)
            require(contrast(skin.text,skin.background)>=4.5 && contrast(skin.text,skin.surface)>=4.5 && contrast(skin.onAccent,skin.accent)>=4.5 && contrast(skin.accent,skin.background)>=4.5 && contrast(skin.accent,skin.surface)>=4.5){"Increase contrast: text and accents need 4.5:1 against their backgrounds."}
            val components=linkedMapOf<String,Map<String,Any>>()
            if(version>=3){
                val source=o.opt("components") as? JSONObject ?: error("Components must be an object.")
                source.keys().forEach { role ->
                    require(role in listOf("buttons","inputs","cards","header")){"Unknown component: $role"}
                    val rule=source.opt(role) as? JSONObject ?: error("Components must contain objects.")
                    val values=linkedMapOf<String,Any>()
                    rule.keys().forEach { key ->
                        val value=rule.get(key)
                        when(key){
                            "background","text","border"->{require(value is String && Regex("^#[0-9a-fA-F]{6}$").matches(value)){"Use opaque #RRGGBB component colors."};values[key]=(value as String).lowercase()}
                            "cornerRadius","padding","textScale","borderWidth"->{require(value is Number){"Component sizes must be numbers."};val n=(value as Number).toDouble();val low=if(key=="textScale").85 else 0.0;val high=when(key){"textScale"->1.5;"borderWidth"->3.0;else->24.0};require(n.isFinite() && n in low..high && (key=="textScale" || n%1.0==0.0)){"Component size is outside its allowed range."};values[key]=if(key=="textScale")n else n.toInt()}
                            else->error("Unknown component field: $key")
                        }
                    }
                    val bg=values["background"] as? String ?: when(role){"buttons"->skin.accent;"header"->skin.background;else->skin.surface}
                    val fg=values["text"] as? String ?: if(role=="buttons")skin.onAccent else skin.text
                    require(contrast(bg,fg)>=4.5){"Component text needs 4.5:1 contrast against its background."}
                    (values["border"] as? String)?.let{require(contrast(it,bg)>=3){"Component borders need 3:1 contrast against their background."}}
                    components[role]=values
                }
            }
            val (images,layers)=if(version==4)parseSkinImages(o) else emptyMap<String,SkinImage>() to emptyList<SkinLayer>()
            return skin.copy(components=components,images=images,layers=layers)
        }
    }
}
