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
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.CancellationException

fun AppSkin.colorScheme():ColorScheme {
    fun c(hex:String)=Color(android.graphics.Color.parseColor(hex))
    val bg=c(background);val base=c(surface);val fg=c(text);val primary=c(accent);val on=c(onAccent)
    val scheme=if(AppSkin.luminance(background)<.18)darkColorScheme() else lightColorScheme()
    return scheme.copy(primary=primary,onPrimary=on,primaryContainer=base,onPrimaryContainer=fg,secondary=primary,onSecondary=on,secondaryContainer=base,onSecondaryContainer=fg,tertiary=primary,onTertiary=on,tertiaryContainer=base,onTertiaryContainer=fg,background=bg,onBackground=fg,surface=base,onSurface=fg,surfaceVariant=bg,onSurfaceVariant=fg,surfaceTint=Color.Transparent,error=fg,onError=base,errorContainer=base,onErrorContainer=fg,inverseSurface=fg,inverseOnSurface=base,inversePrimary=on,outline=fg,outlineVariant=fg,surfaceBright=base,surfaceDim=bg,surfaceContainer=base,surfaceContainerHigh=base,surfaceContainerHighest=base,surfaceContainerLow=bg,surfaceContainerLowest=bg)
}
@Composable fun SkinControls(vm:ClientState) {
    CompositionLocalProvider(LocalAppSkin provides null,LocalSkinRadius provides null){SkinEditor(vm)}
}
@Composable private fun SkinEditor(vm:ClientState) {
    val context=LocalContext.current
    val scope=rememberCoroutineScope()
    val initial=vm.appSkin?:AppSkin.preset(vm.preferences.theme)
    var name by rememberSaveable{mutableStateOf(initial.name)}
    var background by rememberSaveable{mutableStateOf(initial.background)}
    var surface by rememberSaveable{mutableStateOf(initial.surface)}
    var text by rememberSaveable{mutableStateOf(initial.text)}
    var accent by rememberSaveable{mutableStateOf(initial.accent)}
    var onAccent by rememberSaveable{mutableStateOf(initial.onAccent)}
    var uiScale by rememberSaveable{mutableStateOf(initial.uiScale.toFloat())}
    var cornerRadius by rememberSaveable{mutableStateOf(initial.cornerRadius.toFloat())}
    var componentJson by rememberSaveable{mutableStateOf(org.json.JSONObject(initial.components.mapValues{org.json.JSONObject(it.value)}).toString())}
    var componentRole by rememberSaveable{mutableStateOf("buttons")}
    fun fill(s:AppSkin){name=s.name;background=s.background;surface=s.surface;text=s.text;accent=s.accent;onAccent=s.onAccent;uiScale=s.uiScale.toFloat();cornerRadius=s.cornerRadius.toFloat();componentJson=org.json.JSONObject(s.components.mapValues{org.json.JSONObject(it.value)}).toString();vm.draftSkinImages=s.images;vm.draftSkinLayers=s.layers}
    val source=org.json.JSONObject(componentJson)
    val components=source.keys().asSequence().associateWith{role->val r=source.getJSONObject(role);r.keys().asSequence().associateWith{r.get(it)}}
    val candidate=AppSkin(name,background,surface,text,accent,onAccent,uiScale.toDouble(),cornerRadius.toInt(),if(vm.draftSkinImages.isEmpty() && vm.draftSkinLayers.isEmpty())3 else 4,components,vm.draftSkinImages,vm.draftSkinLayers)
    val parsed=runCatching{AppSkin.parse(candidate.copy(version=3,images=emptyMap(),layers=emptyList()).json().toByteArray())}
    val valid=parsed.getOrNull()?.copy(version=candidate.version,images=candidate.images,layers=candidate.layers)
    var lastPreview by remember{mutableStateOf(initial)}
    if(valid!=null)SideEffect{lastPreview=valid}
    val preview=valid ?: lastPreview
    val import=rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()){uri->if(uri!=null)scope.launch{try{
        val skin=withContext(Dispatchers.IO){val bytes=context.contentResolver.openInputStream(uri)?.use{val buffer=ByteArray(SKIN_MAX_BYTES+1);var count=0;while(count<buffer.size){val n=it.read(buffer,count,buffer.size-count);if(n<=0)break;count+=n};buffer.copyOf(count)} ?: error("Cannot read the file.");AppSkin.parse(bytes)};fill(skin)
    }catch(e:CancellationException){throw e}catch(e:Exception){vm.error=e.message}}}
    val export=rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/json")){uri->if(uri!=null)scope.launch{try{
        val pending=vm.pendingSkinExport;require(pending.isNotEmpty()){ "Export expired. Please try again." }
        withContext(Dispatchers.IO){context.contentResolver.openOutputStream(uri)?.use{it.write(pending.toByteArray())} ?: error("Cannot save the file.")};if(vm.pendingSkinExport==pending)vm.pendingSkinExport=""
    }catch(e:CancellationException){throw e}catch(e:Exception){vm.error=e.message}}}
    Column(Modifier.fillMaxSize()) {
    Column(Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(20.dp),verticalArrangement=Arrangement.spacedBy(16.dp)) {
        FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp),verticalArrangement=Arrangement.spacedBy(8.dp)){listOf("light" to "Light","dark" to "Dark","sepia" to "Sepia").forEach{(key,label)->SkinOutlinedButton(onClick={fill(AppSkin.preset(key))}){Text(label)}}}
        Text("neos · ZerafinaCSS · MIT",style=MaterialTheme.typography.bodySmall)
        FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp),verticalArrangement=Arrangement.spacedBy(8.dp)){
            listOf("neos/tumblr-blue" to "Neos Blue","neos/charcoal-rose" to "Neos Rose","illustrated/nebula" to "Neos Nebula").forEach{(file,title)->SkinOutlinedButton(onClick={try{fill(context.assets.open("$file.json").use{AppSkin.parse(it.readBytes())})}catch(e:Exception){vm.error=e.message}}){Text(title)}}
        }
        SkinOutlinedTextField(name,{name=it.take(60)},Modifier.fillMaxWidth(),label={Text("Name")},singleLine=true)
        listOf(Triple("Background",background,{v:String->background=v}),Triple("Surface",surface,{v:String->surface=v}),Triple("Text",text,{v:String->text=v}),Triple("Accent",accent,{v:String->accent=v}),Triple("Text on accent",onAccent,{v:String->onAccent=v})).forEach{(label,value,set)->SkinOutlinedTextField(value,{set(it.take(7))},Modifier.fillMaxWidth(),label={Text(label)},singleLine=true)}
        Text("Interface size: ${(uiScale*100).toInt()}%")
        Slider(value=uiScale,onValueChange={uiScale=it},valueRange=1f..1.5f,steps=9)
        Text("Control corners: ${cornerRadius.toInt()}")
        Slider(value=cornerRadius,onValueChange={cornerRadius=it},valueRange=0f..24f,steps=23)
        Text("Components",style=MaterialTheme.typography.titleMedium)
        FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp),verticalArrangement=Arrangement.spacedBy(8.dp)){listOf("buttons" to "Buttons","inputs" to "Inputs","cards" to "Cards","header" to "Header").forEach{(key,title)->FilterChip(selected=componentRole==key,onClick={componentRole=key},label={Text(title)})}}
        val rule=source.optJSONObject(componentRole) ?: org.json.JSONObject()
        fun change(key:String,value:Any?){val all=org.json.JSONObject(componentJson);val next=all.optJSONObject(componentRole) ?: org.json.JSONObject();if(value==null)next.remove(key) else next.put(key,value);if(next.length()==0)all.remove(componentRole) else all.put(componentRole,next);componentJson=all.toString()}
        listOf("background" to "Background","text" to "Text","border" to "Border").forEach{(key,title)->SkinOutlinedTextField(rule.optString(key),{change(key,it.take(7).takeIf{v->v.isNotEmpty()})},Modifier.fillMaxWidth(),label={Text(title)},placeholder={Text("Inherit")},singleLine=true)}
        listOf("cornerRadius" to "Corners","padding" to "Spacing","textScale" to "Text size","borderWidth" to "Border width").forEach{(key,title)->
            val inherited=!rule.has(key)
            val fallback=when(key){"cornerRadius"->cornerRadius;"textScale"->1f;"padding"->if(componentRole=="cards")16f else if(componentRole=="buttons")8f else 0f;"borderWidth"->if(componentRole=="cards" || rule.has("border"))1f else 0f;else->0f}
            val value=rule.optDouble(key,fallback.toDouble()).toFloat()
            val formatted=if(key=="textScale")"${(value*100).toInt()}%" else "${value.toInt()}"
            Column(Modifier.fillMaxWidth().padding(vertical=4.dp),verticalArrangement=Arrangement.spacedBy(8.dp)) {
                Row(Modifier.fillMaxWidth().heightIn(min=56.dp),verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(16.dp)){
                    Column(Modifier.weight(1f),verticalArrangement=Arrangement.spacedBy(4.dp)){Text(title,style=MaterialTheme.typography.titleSmall);Text(if(inherited)"Theme default · $formatted" else formatted,style=MaterialTheme.typography.bodySmall)}
                    Row(verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(8.dp)){Text("Inherit",style=MaterialTheme.typography.labelMedium);Switch(checked=inherited,onCheckedChange={change(key,if(it)null else if(key=="textScale")fallback.toDouble() else fallback.toInt())})}
                }
                if(!inherited){val range=when(key){"textScale"->.85f..1.5f;"borderWidth"->0f..3f;else->0f..24f};Slider(value=value,onValueChange={change(key,if(key=="textScale")it.toDouble() else it.toInt())},valueRange=range,steps=when(key){"textScale"->12;"borderWidth"->2;else->23})}
                HorizontalDivider()
            }
        }
        SkinImageControls(vm)
        val density=androidx.compose.ui.platform.LocalDensity.current
        CompositionLocalProvider(LocalAppSkin provides preview, LocalSkinRadius provides preview.cornerRadius,androidx.compose.ui.platform.LocalDensity provides androidx.compose.ui.unit.Density(density.density*(preview.uiScale/(vm.appSkin?.uiScale ?: 1.0)).toFloat(),density.fontScale)) {MaterialTheme(colorScheme=preview.colorScheme(),shapes=preview.shapes()) {Surface(color=MaterialTheme.colorScheme.background,modifier=Modifier.fillMaxWidth()){Column(Modifier.padding(16.dp),verticalArrangement=Arrangement.spacedBy(12.dp)){SkinHeader{Text("Archive of Our Own",Modifier.padding(8.dp),style=MaterialTheme.typography.titleLarge)};SkinCard(onClick={}){Text("Chapter title");Text("A preview of your reading text.")};SkinOutlinedTextField("Search preview",{},Modifier.fillMaxWidth(),singleLine=true);SkinButton(onClick={}){Text("Button")}}}}}
        if(valid==null)Text(parsed.exceptionOrNull()?.message.orEmpty(),color=MaterialTheme.colorScheme.error)

        FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp),verticalArrangement=Arrangement.spacedBy(8.dp)){SkinOutlinedButton(onClick={import.launch(arrayOf("application/json","text/plain","application/octet-stream"))}){Text("Import")};SkinOutlinedButton(onClick={if(valid!=null){vm.pendingSkinExport=valid.json();export.launch("ao3-skin.json")}},enabled=valid!=null){Text("Export")};SkinTextButton(onClick={fill(AppSkin.preset(vm.preferences.theme));vm.applySkin(null);vm.notice="App skin reset."}){Text("Reset")}}
    }
    Surface(tonalElevation=2.dp){SkinButton(onClick={if(valid!=null){vm.applySkin(valid);vm.notice="App skin applied."}},enabled=valid!=null,modifier=Modifier.fillMaxWidth().padding(16.dp)){Text("Apply skin")}}
    }
}

internal fun AppSkin.shapes():Shapes {
    val shape=androidx.compose.foundation.shape.RoundedCornerShape(cornerRadius.dp)
    return Shapes(small=shape,medium=shape)
}
