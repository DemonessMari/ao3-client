package org.ao3client.reader

import org.jsoup.Jsoup
import org.jsoup.safety.Safelist
import java.net.URI

private val readingTags=arrayOf("p","div","span","br","hr","h1","h2","h3","h4","h5","h6","blockquote","pre","code","kbd","samp","var","b","strong","i","em","u","s","strike","del","ins","sup","sub","small","big","abbr","acronym","cite","q","a","img","ul","ol","li","dl","dt","dd","table","thead","tbody","tfoot","tr","th","td","caption","colgroup","col","details","summary","ruby","rt","rp")
internal fun staticReadingHtml(html:String):String {
    if(html.isEmpty())return ""
    require(html.length<=40*1024*1024){"Formatted chapter exceeds 40 MB."}
    val rules=Safelist().addTags(*readingTags).addAttributes(":all","id","class","style","title","lang","dir").addAttributes("a","href","name").addAttributes("img","src","alt","width","height").addAttributes("td","colspan","rowspan").addAttributes("th","colspan","rowspan","scope").addAttributes("ol","start","type").addAttributes("li","value").addAttributes("col","span","width").addAttributes("details","open").preserveRelativeLinks(true)
    val safe=Jsoup.clean(html,rules)
    val doc=Jsoup.parseBodyFragment(safe)
    doc.outputSettings().prettyPrint(false)
    doc.select("a[href]").forEach { if(!it.attr("href").startsWith("#")){val link=readingLink(it.attr("href"));if(link==null)it.removeAttr("href") else it.attr("href",link)} }
    doc.select("img").forEach { image -> val src=image.attr("src");if(!src.startsWith("data:image/",true) && !src.startsWith("https://",true) && !src.startsWith("//") && !src.startsWith("/"))image.removeAttr("src") }
    return doc.body().html()
}
internal const val READING_CSP="default-src 'none'; img-src data:; style-src 'unsafe-inline'; font-src data:; media-src data:; connect-src 'none'; frame-src 'none'; object-src 'none'; form-action 'none'; base-uri 'none'"
internal fun formattedDocument(chapter:Chapter,background:String,text:String,size:Int,spacing:Double,margin:Int,workSkin:Boolean=true,font:ByteArray?=null,fontFamily:String="serif"):String {
    val body=Jsoup.parseBodyFragment(staticReadingHtml(chapter.html));body.outputSettings().prettyPrint(false)
    body.select("img").forEach { if(!it.attr("src").startsWith("data:image/",true)){it.replaceWith(org.jsoup.nodes.Element("p").text(it.attr("alt").ifBlank{"Image not saved. Save offline with images to include it."}))} }
    val family=fontFamily.takeIf{it in listOf("serif","sans-serif","monospace")} ?: "serif"
    val validFont=font?.takeIf{it.size in 12..ReaderFontStore.MAX_BYTES && (it.take(4).toByteArray().contentEquals(byteArrayOf(0,1,0,0)) || it.copyOfRange(0,4).toString(Charsets.US_ASCII)=="OTTO")}
    val fontCss=if(validFont!=null)"@font-face{font-family:Ao3Imported;src:url(data:font/ttf;base64,"+java.util.Base64.getEncoder().encodeToString(validFont)+")}body{font-family:Ao3Imported,serif}" else "body{font-family:$family}"
    val css=if(workSkin)chapter.workCss.replace("</", "<\\/") else ""
    return """<!doctype html><html><head><meta charset="utf-8"><meta http-equiv="Content-Security-Policy" content="$READING_CSP"><meta name="viewport" content="width=device-width, initial-scale=1"><style>html{background:$background;color:$text}body{margin:${margin}px;font: ${size}px serif;line-height:$spacing;overflow-wrap:break-word}img{max-width:100%;height:auto}table{max-width:100%;border-collapse:collapse}th,td{padding:.3em}pre{white-space:pre-wrap}a{color:inherit;text-decoration:underline}.landmark,.hidden{display:none}</style><style>$css</style><style>$fontCss</style></head><body><div id="workskin"><div id="chapters"><div class="userstuff">${body.body().html()}</div></div></div></body></html>"""
}
internal fun publicImageUrl(value:String):String?=runCatching {
    val u=URI(Archive.BASE).resolve(value)
    require(u.scheme=="https" && !u.host.isNullOrBlank() && u.rawUserInfo==null && u.port in listOf(-1,443))
    val host=u.host.lowercase().removeSuffix(".");require(host!="localhost" && !host.endsWith(".localhost") && !host.endsWith(".local"))
    u.toASCIIString()
}.getOrNull()
internal fun cacheChapterImages(chapters:List<Chapter>,maxHtmlBytes:Int=48*1024*1024,fetch:(String)->ByteArray):List<Chapter> {
    require(maxHtmlBytes in 1..48*1024*1024)
    val cache=mutableMapOf<String,String>();var total=0;var htmlTotal=0L
    return chapters.map { chapter ->
        val doc=Jsoup.parseBodyFragment(staticReadingHtml(chapter.html));doc.outputSettings().prettyPrint(false)
        doc.select("img[src]").forEach { image ->
            val source=image.attr("src")
            if(!source.startsWith("data:image/",true))publicImageUrl(source)?.let { url ->
                val data=cache.getOrPut(url) {
                    require(cache.size<100){"A work can save up to 100 images."}
                    val bytes=fetch(url);require(bytes.size<=4*1024*1024 && total+bytes.size<=24*1024*1024){"Images exceed the 24 MB offline limit."}
                    val mime=imageMime(bytes) ?: error("Unsupported image format. Use PNG, JPEG, GIF or WebP.")
                    total+=bytes.size;"data:$mime;base64,"+java.util.Base64.getEncoder().encodeToString(bytes)
                }
                image.attr("src",data)
            }
        }
        val html=doc.body().html();htmlTotal+=html.toByteArray(Charsets.UTF_8).size
        require(htmlTotal<=maxHtmlBytes){"Formatted content exceeds the 48 MB offline limit. Your previous copy was kept."}
        chapter.copy(html=html)
    }
}
internal fun imageMime(b:ByteArray):String?=when {
    b.size>=8 && b.take(8).toByteArray().contentEquals(byteArrayOf(-119,80,78,71,13,10,26,10))->"image/png"
    b.size>=3 && b[0]==(-1).toByte() && b[1]==(-40).toByte() && b[2]==(-1).toByte()->"image/jpeg"
    b.size>=6 && b.copyOfRange(0,6).toString(Charsets.US_ASCII) in listOf("GIF87a","GIF89a")->"image/gif"
    b.size>=12 && b.copyOfRange(0,4).toString(Charsets.US_ASCII)=="RIFF" && b.copyOfRange(8,12).toString(Charsets.US_ASCII)=="WEBP"->"image/webp"
    else->null
}

internal fun fetchReadingImage(value:String):ByteArray {
    var url=value
    repeat(4) {
        require(publicImageUrl(url)!=null){"Only public HTTPS images can be saved."}
        val uri=URI(url)
        val addresses=java.net.InetAddress.getAllByName(uri.host)
        require(addresses.isNotEmpty() && addresses.none { it.isAnyLocalAddress || it.isLoopbackAddress || it.isLinkLocalAddress || it.isSiteLocalAddress || it.isMulticastAddress || (it.address.size==16 && (it.address[0].toInt() and 0xfe)==0xfc) }){"Local network images cannot be downloaded."}
        val connection=uri.toURL().openConnection() as java.net.HttpURLConnection
        try {
            connection.instanceFollowRedirects=false;connection.connectTimeout=15000;connection.readTimeout=20000
            connection.setRequestProperty("User-Agent","AO3Client (Offline image download)")
            if(connection.responseCode in 300..399)url=uri.resolve(connection.getHeaderField("Location") ?: error("Missing image redirect")).toString()
            else {
                require(connection.responseCode==200){"Image download failed (HTTP ${connection.responseCode}). Your previous offline copy was kept."}
                require(connection.contentLengthLong<=4*1024*1024){"Image exceeds 4 MB."}
                return connection.inputStream.use {input ->val out=java.io.ByteArrayOutputStream();val buffer=ByteArray(16384);while(true){val count=input.read(buffer);if(count<0)break;require(out.size()+count<=4*1024*1024){"Image exceeds 4 MB."};out.write(buffer,0,count)};out.toByteArray()}
            }
        } finally {connection.disconnect()}
    }
    error("Too many image redirects.")
}
