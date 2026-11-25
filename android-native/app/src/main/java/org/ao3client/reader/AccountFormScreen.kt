@file:OptIn(androidx.compose.foundation.layout.ExperimentalLayoutApi::class)
package org.ao3client.reader

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.unit.dp

@Composable internal fun AccountFieldControl(source:String,field:AccountField,values:List<String>,enabled:Boolean,update:(List<String>)->Unit) {
    val keyboard=LocalSoftwareKeyboardController.current
    when(field.type) {
        "text","area"->SkinOutlinedTextField(values.firstOrNull().orEmpty(),{update(listOf(it))},Modifier.fillMaxWidth().heightIn(max=if(field.type=="area")360.dp else androidx.compose.ui.unit.Dp.Unspecified),enabled=enabled,label={Text(field.label)},singleLine=field.type=="text",minLines=if(field.type=="area")4 else 1)
        "check"->{val on=field.choices.firstOrNull()?.value.orEmpty();Row{Checkbox(on in values,{update(listOf(if(it)on else field.offValue))},enabled=enabled);Text(field.label,Modifier.weight(1f).padding(top=12.dp))}}
        else->{
            var expanded by remember(source,field.name){mutableStateOf(false)}
            Column(verticalArrangement=Arrangement.spacedBy(8.dp)) {
                MenuRow(title=field.label,supportingText=field.choices.filter{it.value in values}.joinToString{it.label},onClick={if(enabled)expanded=!expanded},trailing={Text(if(expanded)"−" else "+")})
                if(expanded) {
                    fun choose(option:Choice){update(if(field.type=="multi")if(option.value in values)values-option.value else values+option.value else listOf(option.value));if(field.type!="multi"){expanded=false;keyboard?.hide()}}
                    if(field.choices.size>12) {
                        var search by remember(source,field.name){mutableStateOf("")}
                        SkinOutlinedTextField(search,{search=it},Modifier.fillMaxWidth(),enabled=enabled,label={Text("Find option")},singleLine=true)
                        val options=field.choices.filter{it.label.contains(search,true)}
                        LazyColumn(Modifier.fillMaxWidth().heightIn(max=280.dp)){items(options,key={it.value}){option->MenuRow(title=option.label,onClick={if(enabled)choose(option)},trailing={if(option.value in values)Text("✓")})};if(options.isEmpty())item{Text("No matching options.",Modifier.padding(12.dp))}}
                    } else FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp),verticalArrangement=Arrangement.spacedBy(8.dp)) {field.choices.forEach{option->FilterChip(selected=option.value in values,onClick={choose(option)},enabled=enabled,label={Text(option.label)})}}
                }
            }
        }
    }
}

@Composable internal fun AccountFormScreen(vm:ClientState) {
    val form=vm.accountForm
    var reload by remember{mutableStateOf(false)}
    LazyColumn(Modifier.fillMaxSize(),contentPadding=PaddingValues(16.dp),verticalArrangement=Arrangement.spacedBy(16.dp)) {
        if(form!=null) {
            item{Text(form.title,style=MaterialTheme.typography.headlineSmall)}
            if(form.notice.isNotBlank())item{Text(form.notice)}
            if(vm.accountUncertain)item{Text("Reload this form before trying again.")}
            items(form.fields,key={it.name}){field->
                val values=vm.accountDraft[field.name] ?: field.values
                val suggestion=mapOf("bookmark[tag_string]" to "tag","external_work[fandom_string]" to "fandom","external_work[relationship_string]" to "relationship","external_work[character_string]" to "character")[field.name]
                if(field.type=="text" && suggestion!=null && !vm.accountSending)FilterControl(FilterField(field.name,field.label,suggestion=suggestion),values,{vm.accountDraft=vm.accountDraft+(field.name to it)},vm)
                else AccountFieldControl(form.source,field,values,!vm.accountSending){vm.accountDraft=vm.accountDraft+(field.name to it)}
            }
            item{SkinButton(onClick=vm::saveAccountEditor,enabled=!vm.accountSending && !vm.accountUncertain){Text(if(vm.accountSending)"Saving…" else if(form.destructive)"Confirm" else "Save to AO3")}}
            if(form.kind=="bookmark")item{SkinOutlinedButton(onClick={vm.showAccountEditor("deleteBookmark",form.id)},enabled=!vm.accountSending){Text("Delete AO3 bookmark")}}
            if(form.kind=="pseud")item{SkinTextButton(onClick={vm.openWebsite(Archive.BASE+form.source)}){Text("Profile picture on AO3")}}
        }
        item{SkinTextButton(onClick={reload=true},enabled=!vm.accountSending && !vm.loading){Text("Reload from AO3")}}
    }
    if(reload)AlertDialog(onDismissRequest={reload=false},title={Text("Reload form?")},text={Text("Replace this draft with the information saved on AO3?")},confirmButton={SkinTextButton(onClick={reload=false;vm.showAccountEditor(vm.accountFormKind,vm.accountFormId,true,true)}){Text("Reload")}},dismissButton={SkinTextButton(onClick={reload=false}){Text("Cancel")}})
}
