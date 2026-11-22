package org.ao3client.reader

import androidx.compose.runtime.Composable
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

internal fun tapZoneAction(fraction:Float,left:Int,center:Int,right:Int,edge:Int):Int {
    val boundary=edge.coerceIn(20,40)/100f
    return (if(fraction<boundary)left else if(fraction>1-boundary)right else center).coerceIn(-1,2)
}
@Composable internal fun TapZoneControls(vm:ClientState) {
    Text("Book-mode tap zones",style=MaterialTheme.typography.titleMedium)
    listOf("Left" to vm.tapLeft,"Center" to vm.tapCenter,"Right" to vm.tapRight).forEach{(region,value)->
        var expanded by remember {mutableStateOf(false)}
        val choices=listOf(-1 to "Previous page",0 to "No action",1 to "Next page",2 to "Show/hide controls")
        Box {OutlinedButton(onClick={expanded=true},modifier=Modifier.fillMaxWidth()){Text("$region · ${choices.first{it.first==value}.second}")}
            DropdownMenu(expanded,{expanded=false}){choices.forEach{(action,title)->DropdownMenuItem(text={Text(title)},onClick={vm.tapZone(region,action);expanded=false})}}}
    }
    Text("Each edge zone · ${vm.tapEdge}%")
    Slider(vm.tapEdge.toFloat(),{vm.tapEdge(it.toInt())},valueRange=20f..40f)
}
