package org.ao3client.reader

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowLeft
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp

@Composable internal fun ReaderChapterHeader(title:String,hasNotes:Boolean,fullscreen:Boolean,onNotes:()->Unit,onExit:()->Unit) {
    Row(Modifier.fillMaxWidth().padding(start=24.dp,end=8.dp),verticalAlignment=Alignment.CenterVertically) {
        Text(title,Modifier.weight(1f).padding(vertical=12.dp),style=MaterialTheme.typography.labelLarge,color=MaterialTheme.colorScheme.onSurfaceVariant,maxLines=1,overflow=TextOverflow.Ellipsis)
        if(hasNotes)IconButton(onClick=onNotes){Icon(Icons.Default.Info,"Author notes")}
        if(fullscreen)IconButton(onClick=onExit){Icon(Icons.Default.Close,"Exit fullscreen")}
    }
}
@Composable internal fun ReaderPageFooter(page:Int,pages:Int,previousEnabled:Boolean,nextEnabled:Boolean,modifier:Modifier=Modifier,previousLabel:String="Previous page",nextLabel:String="Next page",onPrevious:()->Unit,onNext:()->Unit) {
    Row(modifier.fillMaxWidth().padding(horizontal=12.dp),horizontalArrangement=Arrangement.SpaceBetween,verticalAlignment=Alignment.CenterVertically) {
        IconButton(onClick=onPrevious,enabled=previousEnabled){Icon(Icons.AutoMirrored.Filled.KeyboardArrowLeft,previousLabel)}
        Text("$page / $pages",Modifier.semantics{contentDescription="Page $page of $pages"},style=MaterialTheme.typography.labelMedium,color=MaterialTheme.colorScheme.onSurfaceVariant)
        IconButton(onClick=onNext,enabled=nextEnabled){Icon(Icons.AutoMirrored.Filled.KeyboardArrowRight,nextLabel)}
    }
}
