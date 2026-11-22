@file:OptIn(androidx.compose.foundation.layout.ExperimentalLayoutApi::class)
package org.ao3client.reader

import android.graphics.Typeface
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp

@Composable internal fun readerFontFamily(vm: ClientState): FontFamily {
    val selected=vm.preferences.fontFamily
    return remember(selected,vm.importedFonts) {
        if(selected=="sans-serif") FontFamily.SansSerif
        else vm.importedFonts.firstOrNull { it.id==selected }?.let { font -> runCatching { FontFamily(Typeface.createFromFile(font.file)) }.getOrNull() } ?: FontFamily.Serif
    }
}
@Composable internal fun FontControls(vm: ClientState) {
    val context=LocalContext.current
    val picker=rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> if(uri!=null)vm.importFont(context.contentResolver,uri) }
    var expanded by remember { mutableStateOf(false) }
    if(vm.importedFonts.isNotEmpty())Box {
        OutlinedButton(onClick={expanded=true},modifier=Modifier.fillMaxWidth()) { Text(vm.importedFonts.firstOrNull { it.id==vm.preferences.fontFamily }?.name ?: "Imported fonts",maxLines=2,overflow=TextOverflow.Ellipsis) }
        DropdownMenu(expanded=expanded,onDismissRequest={expanded=false}) {vm.importedFonts.forEach { font -> DropdownMenuItem(text={Text(font.name,maxLines=2,overflow=TextOverflow.Ellipsis)},onClick={expanded=false;vm.appearance(family=font.id)}) }}
    }
    if(ReaderFontStore.validId(vm.preferences.fontFamily))Row(Modifier.fillMaxWidth(),verticalAlignment=androidx.compose.ui.Alignment.CenterVertically) {
        Checkbox(checked=vm.preferences.useImportedFontInInterface,onCheckedChange=vm::interfaceFont)
        Text("Use imported font in interface",Modifier.weight(1f))
    }
    vm.fontError?.let { Text(it,color=MaterialTheme.colorScheme.error) }
    FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp)) {
        OutlinedButton(onClick={picker.launch(arrayOf("*/*"))},enabled=!vm.fontImporting) {Text(if(vm.fontImporting)"Importing…" else "Import font")}
        if(ReaderFontStore.validId(vm.preferences.fontFamily))TextButton(onClick={vm.removeFont(vm.preferences.fontFamily)},enabled=!vm.fontImporting) {Text("Remove font")}
    }
}
