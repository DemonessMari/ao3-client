package org.ao3client.reader

data class TagToken(val start: Int, val end: Int, val term: String)
data class TagEdit(val text: String, val cursor: Int)
fun activeTagToken(text: String, cursor: Int): TagToken {
    val position = cursor.coerceIn(0, text.length)
    val start = if(position == 0) 0 else text.lastIndexOf(',', position - 1) + 1
    val end = text.indexOf(',', position).let { if(it < 0) text.length else it }
    return TagToken(start, end, text.substring(start, end).trim())
}
fun insertTag(text: String, cursor: Int, tag: String): TagEdit {
    val token = activeTagToken(text, cursor)
    val prefix = text.substring(0, token.start) + if(token.start > 0) " " else ""
    return TagEdit(prefix + tag + text.substring(token.end), prefix.length + tag.length)
}
