package org.ao3client.reader

import java.net.HttpURLConnection
import java.net.InetAddress
import java.net.URI
import java.io.FilterInputStream
import java.io.InputStream
import java.io.IOException

internal data class ReadingResource(val mime:String,val encoding:String?,val status:Int,val headers:Map<String,String>,val stream:InputStream)
internal class ExternalReading(
    private val connect:(URI)->HttpURLConnection={it.toURL().openConnection() as HttpURLConnection},
    private val resolve:(String)->Array<InetAddress> = {InetAddress.getAllByName(it)}
):java.io.Closeable {
    private val active=mutableSetOf<HttpURLConnection>()
    @Volatile private var closed=false
    fun cancelRequests(){synchronized(active){active.forEach{it.disconnect()};active.clear()}}
    override fun close(){closed=true;cancelRequests()}
    fun open(value:String,range:String?=null):ReadingResource {
        var url=value
        repeat(4) {
            check(!closed){"Reader closed"}
            val uri=URI(publicImageUrl(url) ?: error("Only public HTTPS resources can be loaded."))
            val addresses=resolve(uri.host)
            require(addresses.isNotEmpty() && addresses.all(::publicAddress)){"Local network resources are blocked."}
            val c=connect(uri);synchronized(active){check(!closed && active.size<16){"Too many reading resources"};active+=c}
            var handedOff=false
            try {
                c.instanceFollowRedirects=false;c.useCaches=false;c.connectTimeout=15000;c.readTimeout=30000
                c.setRequestProperty("Cookie","");c.setRequestProperty("Accept-Encoding","identity");c.setRequestProperty("User-Agent","AO3Client (Reading resource)")
                if(range!=null && range.matches(Regex("bytes=(?:[0-9]+-[0-9]*|-[0-9]+)")))c.setRequestProperty("Range",range)
                val status=c.responseCode
                if(status in 300..399){url=uri.resolve(c.getHeaderField("Location") ?: error("Missing reading-resource redirect")).toString()}
                else {
                    require(status in listOf(200,206)){"Reading resource unavailable"}
                    val mime=c.contentType.orEmpty().substringBefore(';').ifBlank{"application/octet-stream"}
                    val limit=when{mime.startsWith("audio/") || mime.startsWith("video/")->Long.MAX_VALUE;mime=="text/css"->1024L*1024;else->8L*1024*1024}
                    require(c.contentLengthLong<=limit){"Reading resource is too large"}
                    val headers=mutableMapOf("Access-Control-Allow-Origin" to "*")
                    for(name in listOf("Content-Length","Content-Range","Accept-Ranges","Cache-Control"))c.getHeaderField(name)?.let{headers[name]=it}
                    val stream=object:FilterInputStream(c.inputStream) {
                        private var count=0L
                        private fun add(n:Int):Int {if(n>0){count+=n;if(count>limit){close();throw IOException("Reading resource is too large")}};return n}
                        override fun read():Int {val result=`in`.read();if(result>=0)add(1);return result}
                        override fun read(b:ByteArray,off:Int,len:Int)=add(`in`.read(b,off,len))
                        override fun close(){try{super.close()}finally{synchronized(active){active.remove(c)};c.disconnect()}}
                    }
                    handedOff=true
                    return ReadingResource(mime,if(mime.startsWith("text/"))"UTF-8" else null,status,headers,stream)
                }
            } finally {if(!handedOff){synchronized(active){active.remove(c)};c.disconnect()}}
        }
        error("Too many reading-resource redirects")
    }
    companion object {
        fun publicAddress(address:InetAddress):Boolean {
            if(address.isAnyLocalAddress || address.isLoopbackAddress || address.isLinkLocalAddress || address.isSiteLocalAddress || address.isMulticastAddress)return false
            val b=address.address
            if(b.size==16 && (b[0].toInt() and 0xfe)==0xfc)return false
            if(b.size==4){val first=b[0].toInt() and 255;val second=b[1].toInt() and 255;if(first==0 || first>=224 || first==100 && second in 64..127)return false}
            return true
        }
    }
}
