package org.ao3client.reader
internal fun paragraphStarts(text:String):List<Int> {
    var cursor=0
    return text.split(Regex("""\n\s*\n""")).filter { it.isNotBlank() }.map { paragraph -> text.indexOf(paragraph,cursor).coerceAtLeast(cursor).also { cursor=it+paragraph.length } }.ifEmpty { listOf(0) }
}
internal fun readingFraction(text:String,index:Int,bookOffset:Int):Double {
    if(text.isEmpty()) return 0.0
    val offset=if(bookOffset>=0) bookOffset else paragraphStarts(text).getOrElse(index) { text.length }
    return (offset.toDouble()/text.length).coerceIn(0.0,1.0)
}
