package org.ao3client.reader

import org.json.JSONObject
import org.json.JSONArray
import java.util.Base64

internal const val SKIN_MAX_BYTES=4*1024*1024
data class SkinImage(val data:String,val credit:String="User supplied",val source:String="",val license:String="User supplied") {
    fun json()=JSONObject().put("data",data).put("credit",credit).put("source",source).put("license",license)
    fun bytes():ByteArray=Base64.getDecoder().decode(data.substringAfter(','))
}
data class SkinLayer(val image:String,val target:String="header",val placement:String="banner",val fit:String="cover",val anchor:String="center",val width:Int=256,val height:Int=96,val opacity:Double=1.0,val offsetX:Int=0,val offsetY:Int=0) {
    fun json()=JSONObject().put("image",image).put("target",target).put("placement",placement).put("fit",fit).put("anchor",anchor).put("width",width).put("height",height).put("opacity",opacity).put("offsetX",offsetX).put("offsetY",offsetY)
}
internal fun skinImageSize(b:ByteArray):Pair<Int,Int>? {
    fun byte(i:Int)=b[i].toInt() and 255
    fun int(i:Int)=(byte(i) shl 24) or (byte(i+1) shl 16) or (byte(i+2) shl 8) or byte(i+3)
    if(b.size>=33 && b.take(8).toByteArray().contentEquals(byteArrayOf(-119,80,78,71,13,10,26,10)) && b.copyOfRange(12,16).toString(Charsets.US_ASCII)=="IHDR" && b.copyOfRange(b.size-8,b.size-4).toString(Charsets.US_ASCII)=="IEND")return int(16) to int(20)
    if(b.size<12 || byte(0)!=255 || byte(1)!=216 || byte(b.size-2)!=255 || byte(b.size-1)!=217)return null
    var p=2
    while(p+3<b.size){
        if(byte(p++)!=255)return null
        while(p<b.size && byte(p)==255)p++
        if(p>=b.size)return null
        val marker=byte(p++);if(marker==218 || marker==217)break
        if(marker==1 || marker in 208..215)continue
        if(p+2>b.size)return null
        val length=(byte(p) shl 8) or byte(p+1);if(length<2 || p+length>b.size)return null
        if(marker in 192..195 || marker in 197..199 || marker in 201..203 || marker in 205..207){if(length<8)return null;return ((byte(p+5) shl 8) or byte(p+6)) to ((byte(p+3) shl 8) or byte(p+4))}
        p+=length
    }
    return null
}
internal fun parseSkinImages(o:JSONObject):Pair<Map<String,SkinImage>,List<SkinLayer>> {
    val source=o.opt("images") as? JSONObject ?: error("Images and layers are required in version 4.")
    val rawLayers=o.opt("layers") as? JSONArray ?: error("Images and layers are required in version 4.")
    require(source.length()<=4 && rawLayers.length()<=8){"Use at most four images and eight layers."}
    val images=source.keys().asSequence().associateWith { id ->
        require(Regex("^[a-z][a-z0-9_-]{0,31}$").matches(id)){"Invalid image name."}
        val a=source.opt(id) as? JSONObject ?: error("Images must be objects.")
        require(a.keys().asSequence().all{it in setOf("data","credit","source","license")}){"Unknown image field."}
        fun text(key:String):String {val value=a.opt(key);require(value is String && value.length<=1024){"Image credits must be text up to 1024 characters."};return value}
        val data=a.opt("data") as? String ?: error("Missing image data.")
        require(Regex("^data:image/(png|jpeg);base64,[A-Za-z0-9+/]+={0,2}$").matches(data)){"Use embedded PNG or JPEG images."}
        val asset=SkinImage(data,text("credit"),text("source"),text("license"))
        val bytes=asset.bytes();val size=skinImageSize(bytes)
        require(bytes.isNotEmpty() && bytes.size<=512*1024 && size!=null && size.first in 1..4096 && size.second in 1..4096 && size.first.toLong()*size.second<=2097152){"Each image must be valid, at most 512 KB and two megapixels."}
        require(data.startsWith("data:image/png")==bytes.take(4).toByteArray().contentEquals(byteArrayOf(-119,80,78,71))){"Image type does not match its contents."}
        asset
    }
    var banners=0
    val layers=(0 until rawLayers.length()).map { i ->
        val l=rawLayers.opt(i) as? JSONObject ?: error("Layers must be objects.")
        require(l.keys().asSequence().all{it in setOf("image","target","placement","fit","anchor","width","height","opacity","offsetX","offsetY")}){"Unknown layer field."}
        val image=l.optString("image");val target=l.optString("target");val placement=l.optString("placement");val fit=l.optString("fit");val anchor=l.optString("anchor")
        require(image in images){"A layer refers to a missing image."}
        require(target in listOf("window","header","cards") && placement in listOf("background","overlay","banner") && fit in listOf("contain","cover","tile") && anchor in listOf("top-left","top-right","bottom-left","bottom-right","center")){"Invalid layer target, placement, fit or anchor."}
        if(placement=="banner")require(target=="header" && ++banners<=1 && fit!="tile"){"Use one non-tiled header banner."}
        fun number(key:String):Double {require(l.opt(key) is Number){"Layer sizes must be numbers."};val n=l.getDouble(key);val low=if(key=="opacity")0.0 else if(key.startsWith("offset"))-256.0 else 16.0;val high=if(key=="opacity")1.0 else if(key.startsWith("offset"))256.0 else if(key=="height" && placement=="banner")240.0 else 1024.0;require(n.isFinite() && n in low..high && (key=="opacity" || n%1.0==0.0)){"Layer size, position or opacity is outside its allowed range."};return n}
        SkinLayer(image,target,placement,fit,anchor,number("width").toInt(),number("height").toInt(),number("opacity"),number("offsetX").toInt(),number("offsetY").toInt())
    }
    return images to layers
}
