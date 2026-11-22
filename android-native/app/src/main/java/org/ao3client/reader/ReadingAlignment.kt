package org.ao3client.reader

import androidx.compose.ui.text.style.TextAlign

internal fun readingAlignment(value: String): TextAlign = when(value) {
    "justify" -> TextAlign.Justify
    "center" -> TextAlign.Center
    "right" -> TextAlign.Right
    else -> TextAlign.Left
}
