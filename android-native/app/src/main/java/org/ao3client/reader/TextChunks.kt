package org.ao3client.reader

fun translationChunks(text: String, limit: Int = 1800): List<String> {
    require(limit >= 2)
    val result = mutableListOf<String>()
    var start = 0
    while(start < text.length) {
        var end = (start + limit).coerceAtMost(text.length)
        if(end < text.length) {
            if(text[end - 1].isHighSurrogate() && text[end].isLowSurrogate()) end--
            val space = (end - 1 downTo start + (end - start) / 2).firstOrNull { text[it].isWhitespace() }
            if(space != null) end = space + 1
        }
        result += text.substring(start, end); start = end
    }
    return result
}
