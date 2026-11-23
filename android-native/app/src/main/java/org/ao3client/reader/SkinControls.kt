@file:OptIn(androidx.compose.foundation.layout.ExperimentalLayoutApi::class)
package org.ao3client.reader

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp

fun AppSkin.colorScheme():ColorScheme {
    fun c(hex:String)=Color(android.graphics.Color.parseColor(hex))
    val bg=c(background);val base=c(surface);val fg=c(text);val primary=c(accent);val on=c(onAccent)
    val scheme=if(AppSkin.luminance(background)<.18)darkColorScheme() else lightColorScheme()
    return scheme.copy(primary=primary,onPrimary=on,primaryContainer=base,onPrimaryContainer=fg,secondary=primary,onSecondary=on,secondaryContainer=base,onSecondaryContainer=fg,tertiary=primary,onTertiary=on,tertiaryContainer=base,onTertiaryContainer=fg,background=bg,onBackground=fg,surface=base,onSurface=fg,surfaceVariant=bg,onSurfaceVariant=fg,surfaceTint=Color.Transparent,error=fg,onError=base,errorContainer=base,onErrorContainer=fg,inverseSurface=fg,inverseOnSurface=base,inversePrimary=on,outline=fg,outlineVariant=fg,surfaceBright=base,surfaceDim=bg,surfaceContainer=base,surfaceContainerHigh=base,surfaceContainerHighest=base,surfaceContainerLow=bg,surfaceContainerLowest=bg)
}
@Composable fun SkinControls(vm:ClientState) {
    val context=LocalContext.current
    val initial=vm.appSkin?:AppSkin.preset(vm.preferences.theme)
    var draft by rememberSaveable {mutableStateOf(initial.json())}
    // Keep invalid field edits visible while validating the complete draft.
    var name by rememberSaveable{mutableStateOf(initial.name)}
    var background by rememberSaveable{mutableStateOf(initial.background)}
    var surface by rememberSaveable{mutableStateOf(initial.surface)}
    var text by rememberSaveable{mutableStateOf(initial.text)}
    var accent by rememberSaveable{mutableStateOf(initial.accent)}
    var onAccent by rememberSaveable{mutableStateOf(initial.onAccent)}
    var uiScale by rememberSaveable{mutableStateOf(initial.uiScale.toFloat())}
    var cornerRadius by rememberSaveable{mutableStateOf(initial.cornerRadius.toFloat())}
    fun fill(s:AppSkin){name=s.name;background=s.background;surface=s.surface;text=s.text;accent=s.accent;onAccent=s.onAccent;uiScale=s.uiScale.toFloat();cornerRadius=s.cornerRadius.toFloat()}
    val candidate=AppSkin(name,background,surface,text,accent,onAccent,uiScale.toDouble(),cornerRadius.toInt(),2)
    val parsed=runCatching{AppSkin.parse(candidate.json().toByteArray())}
    val valid=parsed.getOrNull()
    var previewJson by rememberSaveable{mutableStateOf(initial.json())}
    if(valid!=null)SideEffect{previewJson=valid.json()}
    val preview=valid?:runCatching{AppSkin.parse(previewJson.toByteArray())}.getOrDefault(initial)
    val import=rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()){uri->if(uri!=null)try{val bytes=context.contentResolver.openInputStream(uri)?.use{val buffer=ByteArray(16385);var count=0;while(count<buffer.size){val n=it.read(buffer,count,buffer.size-count);if(n<0)break;if(n==0)break;count+=n};buffer.copyOf(count)}?:error("Cannot read the file.");fill(AppSkin.parse(bytes))}catch(e:Exception){vm.error=e.message}}
    val export=rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/json")){uri->if(uri!=null)try{context.contentResolver.openOutputStream(uri)?.use{it.write(draft.toByteArray())}?:error("Cannot save the file.")}catch(e:Exception){vm.error=e.message}}
    Column(Modifier.fillMaxSize()) {
    Column(Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(20.dp),verticalArrangement=Arrangement.spacedBy(16.dp)) {
        FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp)){listOf("light" to "Light","dark" to "Dark","sepia" to "Sepia").forEach{(key,label)->SkinOutlinedButton(onClick={fill(AppSkin.preset(key))}){Text(label)}}}
        OutlinedTextField(name,{name=it.take(60)},Modifier.fillMaxWidth(),label={Text("Name")},singleLine=true)
        listOf(Triple("Background",background,{v:String->background=v}),Triple("Surface",surface,{v:String->surface=v}),Triple("Text",text,{v:String->text=v}),Triple("Accent",accent,{v:String->accent=v}),Triple("Text on accent",onAccent,{v:String->onAccent=v})).forEach{(label,value,set)->OutlinedTextField(value,{set(it.take(7))},Modifier.fillMaxWidth(),label={Text(label)},singleLine=true)}
        Text("Interface size: ${(uiScale*100).toInt()}%")
        Slider(value=uiScale,onValueChange={uiScale=it},valueRange=1f..1.5f,steps=9)
        Text("Control corners: ${cornerRadius.toInt()}")
        Slider(value=cornerRadius,onValueChange={cornerRadius=it},valueRange=0f..24f,steps=23)
        val density=androidx.compose.ui.platform.LocalDensity.current
        CompositionLocalProvider(LocalSkinRadius provides preview.cornerRadius,androidx.compose.ui.platform.LocalDensity provides androidx.compose.ui.unit.Density(density.density*(preview.uiScale/(vm.appSkin?.uiScale ?: 1.0)).toFloat(),density.fontScale)) {MaterialTheme(colorScheme=preview.colorScheme(),shapes=preview.shapes()) {Surface(color=MaterialTheme.colorScheme.background,modifier=Modifier.fillMaxWidth()){Column(Modifier.padding(16.dp),verticalArrangement=Arrangement.spacedBy(12.dp)){Text("Chapter title",style=MaterialTheme.typography.titleLarge);Surface(color=MaterialTheme.colorScheme.surface){Text("A preview of your reading text.",Modifier.padding(12.dp))};SkinButton(onClick={}){Text("Button")}}}}}
        if(valid==null)Text(parsed.exceptionOrNull()?.message.orEmpty(),color=MaterialTheme.colorScheme.error)

        FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp)){SkinOutlinedButton(onClick={import.launch(arrayOf("application/json","text/plain","application/octet-stream"))}){Text("Import")};SkinOutlinedButton(onClick={if(valid!=null){draft=valid.json();export.launch("ao3-skin.json")}},enabled=valid!=null){Text("Export")};SkinTextButton(onClick={fill(AppSkin.preset(vm.preferences.theme));vm.applySkin(null);vm.notice="App skin reset."}){Text("Reset")}}
    }
    Surface(tonalElevation=2.dp){SkinButton(onClick={if(valid!=null){vm.applySkin(valid);vm.notice="App skin applied."}},enabled=valid!=null,modifier=Modifier.fillMaxWidth().padding(16.dp)){Text("Apply skin")}}
    }
}

internal fun AppSkin.shapes():Shapes {
    val shape=androidx.compose.foundation.shape.RoundedCornerShape(cornerRadius.dp)
    return Shapes(small=shape,medium=shape)
}
