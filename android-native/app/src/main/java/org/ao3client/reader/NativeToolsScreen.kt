package org.ao3client.reader

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.Dispatchers

@Composable internal fun NativeToolsScreen(vm:ClientState) {
    val page=vm.toolPage
    var expanded by remember(page?.source,page?.forms){mutableStateOf(if(page?.forms?.size==1)page.forms.first().key else "")}
    var previewOpen by remember{mutableStateOf(false)}
    var pageFilter by remember(page?.source){mutableStateOf("")}
    var confirmation by remember{mutableStateOf<Pair<ToolForm,ToolSubmit>?>(null)}
    LazyColumn(Modifier.fillMaxSize(),contentPadding=PaddingValues(16.dp),verticalArrangement=Arrangement.spacedBy(16.dp)) {
        if(page!=null) {
            if(page.source.substringBefore('?').matches(Regex("/works/[0-9]+")))item{SkinButton(onClick={vm.openArchiveEntry(page.source.substringBefore('?'))}){Text("Open in reader")}}
            item{SkinTextButton(onClick={vm.openWebsite(Archive.BASE+page.source)}){Text("Open page on AO3")}}
            if(page.previewHtml.isNotEmpty())item{SkinButton(onClick={previewOpen=true}){Text("View formatted preview")}}
            item{Text(page.title,style=MaterialTheme.typography.headlineSmall)}
            items(page.notices){Text(it)}
            if(vm.toolUncertain)item{Text("Reload this page to check the result before trying again.")}
            items(page.forms,key={it.key}){form->
                Column(verticalArrangement=Arrangement.spacedBy(12.dp)) {
                    if(page.forms.size>1)MenuRow(title=form.title,onClick={expanded=if(expanded==form.key)"" else form.key},trailing={Text(if(expanded==form.key)"−" else "+")})
                    if(expanded==form.key) {
                        for(field in form.fields)key(field.name) {
                            val values=vm.toolDraft[form.key]?.get(field.name) ?: field.values
                            fun update(v:List<String>){vm.editToolField(form.key,field.name,v)}
                            when(field.type) {
                                "area"->if(AuthoringText.field(field.name))AuthoringEditor(form.key,field,values.firstOrNull().orEmpty(),!vm.toolSending,authoringTypeface(vm)){update(listOf(it))} else AccountFieldControl(form.key,field,values,!vm.toolSending,::update)
                                "file"->ToolFileControl(vm,form,field)
                                "password"->SkinOutlinedTextField(values.firstOrNull().orEmpty(),{update(listOf(it))},Modifier.fillMaxWidth(),enabled=!vm.toolSending,label={Text(field.label)},singleLine=true,visualTransformation=PasswordVisualTransformation(),keyboardOptions=KeyboardOptions(keyboardType=KeyboardType.Password))
                                else->{val suggestion=when{field.name.contains("fandom",true)->"fandom";field.name.contains("character",true)->"character";field.name.contains("relationship",true)->"relationship";field.name.contains("freeform",true) || field.name.endsWith("[tag_string]")->"tag";else->null};if(field.type=="text" && suggestion!=null && !vm.toolSending)FilterControl(FilterField(field.name,field.label,suggestion=suggestion),values,::update,vm) else AccountFieldControl(form.key,field,values,!vm.toolSending,::update)}
                            }
                        }
                        for(section in form.sections)SkinOutlinedButton(onClick={vm.addToolSection(form.key,section.key)},enabled=!vm.toolSending && !vm.toolUncertain){Text(section.label)}
                        for(button in form.submits)SkinButton(onClick={if(form.method=="get")vm.submitTool(form,button) else confirmation=form to button},enabled=!vm.toolSending && !vm.toolUncertain){Text(button.label.ifBlank{"Submit"})}
                    }
                }
            }
            if(page.links.isNotEmpty())item{Text("Pages and actions",style=MaterialTheme.typography.titleMedium)}
            if(page.links.size>8)item{SkinOutlinedTextField(pageFilter,{pageFilter=it},Modifier.fillMaxWidth(),label={Text("Find page or action")},singleLine=true)}
            items(page.links.filter{it.first.contains(pageFilter,true)}){(label,path)->SkinTextButton(onClick={vm.showTools(path)},enabled=!vm.toolSending){Text(label)}}
            if(page.text.isNotBlank())items(page.text.chunked(8000)){androidx.compose.foundation.text.selection.SelectionContainer{Text(it)}}
        }
        item{SkinOutlinedButton(onClick={vm.showTools(page?.source ?: vm.toolPath,true)},enabled=!vm.toolSending && !vm.loading){Text("Reload from AO3")}}
    }
    if(previewOpen && page!=null)FormattedReader(vm,{previewOpen=false},Chapter(page.title,"",html=page.previewHtml,workCss=page.previewCss))
    confirmation?.let{(form,button)->AlertDialog(onDismissRequest={confirmation=null},title={Text(button.label.ifBlank{"Submit to AO3?"})},text={Text("Submit these changes to AO3?")},confirmButton={SkinTextButton(onClick={confirmation=null;vm.submitTool(form,button)}){Text("Confirm")}},dismissButton={SkinTextButton(onClick={confirmation=null}){Text("Cancel")}})}
    vm.pendingTools?.let{(path,reload)->AlertDialog(onDismissRequest={vm.pendingTools=null},title={Text("Discard changes?")},text={Text("Discard this unsaved form and open the selected page?")},confirmButton={SkinTextButton(onClick={vm.pendingTools=null;vm.showTools(path,reload,true)}){Text("Discard")}},dismissButton={SkinTextButton(onClick={vm.pendingTools=null}){Text("Cancel")}})}
}

@Composable private fun ToolFileControl(vm:ClientState,form:ToolForm,field:AccountField) {
    val context=LocalContext.current;val scope=rememberCoroutineScope()
    val selected=vm.toolUploads[form.key]?.get(field.name)
    val launcher=rememberLauncherForActivityResult(ActivityResultContracts.GetContent()){uri->if(uri!=null)scope.launch{
        try {
            val upload=withContext(Dispatchers.IO){val bytes=context.contentResolver.openInputStream(uri)?.use{NativeTools.readImage(it)} ?: error("Could not read this image.");NativeTools.upload(field.name,uri.lastPathSegment.orEmpty(),bytes)}
            vm.addToolUpload(form.key,upload)
        }catch(e:Exception){vm.error=e.message ?: "Could not open this image."}
    }}
    Column {Text(field.label);SkinOutlinedButton(onClick={launcher.launch("image/*")},enabled=!vm.toolSending){Text(selected?.filename ?: "Choose image")};if(selected!=null)SkinTextButton(onClick={vm.removeToolUpload(form.key,field.name)},enabled=!vm.toolSending){Text("Remove selected image")}}
}
