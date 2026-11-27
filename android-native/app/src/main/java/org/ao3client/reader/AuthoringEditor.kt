package org.ao3client.reader

import android.widget.EditText
import android.graphics.Typeface
import android.text.*
import android.text.style.URLSpan
import androidx.compose.runtime.*
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalDensity
import android.util.TypedValue
import androidx.compose.foundation.border
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.unit.dp

@Composable internal fun AuthoringEditor(source:String,field:AccountField,value:String,enabled:Boolean,typeface:Typeface=Typeface.DEFAULT,update:(String)->Unit) {
    var visual by remember(source,field.name){mutableStateOf(AuthoringText.supported(value))}
    var warning by remember{mutableStateOf(false)}
    var linkDialog by remember{mutableStateOf(false)}
    var invalidLink by remember{mutableStateOf(false)}
    var link by remember{mutableStateOf("")}
    var selection by remember{mutableStateOf(0 to 0)}
    var editor by remember(source,field.name){mutableStateOf<EditText?>(null)}
    var synchronizing by remember{mutableStateOf(false)}
    var history by remember(source,field.name){mutableStateOf(AuthorHistory(AuthoringText.parse(value)))}
    var last by remember(source,field.name){mutableStateOf(value)}
    val onChange by rememberUpdatedState(update)
    fun changed(){editor?.text?.let{history.record(it);last=AuthoringText.html(it);onChange(last)}}
    Column(verticalArrangement=Arrangement.spacedBy(8.dp)) {
        Text(field.label,style=MaterialTheme.typography.titleSmall)
        Row {
            FilterChip(selected=visual,onClick={if(AuthoringText.supported(value))visual=true else warning=true},enabled=enabled,label={Text("Visual")})
            Spacer(Modifier.width(8.dp));FilterChip(selected=!visual,onClick={visual=false},enabled=enabled,label={Text("HTML")})
        }
        if(visual) {
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState())) {
                listOf("Bold","Italic","Underline","Strike","Heading","Bullets","Numbered","Paragraph").forEach{kind->SkinTextButton(enabled=enabled,onClick={editor?.let{e->if(kind in listOf("Heading","Bullets","Numbered","Paragraph"))AuthoringText.paragraph(e.text,e.selectionStart,e.selectionEnd,kind) else AuthoringText.toggle(e.text,e.selectionStart,e.selectionEnd,kind);changed();e.invalidate()}}){Text(kind)}}
                listOf("Undo","Redo").forEach{action->SkinTextButton(enabled=enabled,onClick={val text=if(action=="Undo")history.undo() else history.redo();if(text!=null)editor?.let{e->synchronizing=true;e.setText(text);e.setSelection(e.length());synchronizing=false;last=AuthoringText.html(e.text);onChange(last)}}){Text(action)}}
                SkinTextButton(enabled=enabled,onClick={editor?.let{e->if(e.selectionStart>=0 && e.selectionEnd>e.selectionStart){selection=e.selectionStart to e.selectionEnd;link=e.text.getSpans(e.selectionStart,e.selectionEnd,URLSpan::class.java).firstOrNull()?.url.orEmpty();invalidLink=false;linkDialog=true}}}){Text("Link")}
            }
            SkinRole("inputs") {
            val rule=skinRule("inputs");val density=LocalDensity.current
            val foreground=MaterialTheme.colorScheme.onSurface.copy(alpha=if(enabled)1f else .38f).toArgb();val background=MaterialTheme.colorScheme.surface.toArgb()
            val font=with(density){MaterialTheme.typography.bodyLarge.fontSize.toPx()};val inset=with(density){16.dp.roundToPx()}
            val shape=skinShape("inputs");val width=rule.number("borderWidth",1.0).dp;val border=if(width.value==0f)androidx.compose.ui.graphics.Color.Transparent else rule.color("border",MaterialTheme.colorScheme.outline)
            AndroidView(modifier=Modifier.fillMaxWidth().padding(rule.number("padding",0.0).dp).heightIn(min=180.dp,max=360.dp).clip(shape).border(width,border,shape),factory={context->object:EditText(context){override fun onTextContextMenuItem(id:Int)=super.onTextContextMenuItem(if(id==android.R.id.paste)android.R.id.pasteAsPlainText else id)}.apply {
                minLines=6;maxLines=12;gravity=android.view.Gravity.TOP;isSingleLine=false;setHorizontallyScrolling(false);setPadding(inset,inset,inset,inset);contentDescription="Visual editor: ${field.label}"
                inputType=android.text.InputType.TYPE_CLASS_TEXT or android.text.InputType.TYPE_TEXT_FLAG_MULTI_LINE or android.text.InputType.TYPE_TEXT_FLAG_CAP_SENTENCES
                setText(AuthoringText.parse(value));last=value;history=AuthorHistory(text);editor=this
                addTextChangedListener(object:TextWatcher{override fun beforeTextChanged(s:CharSequence?,start:Int,count:Int,after:Int){};override fun onTextChanged(s:CharSequence?,start:Int,before:Int,count:Int){};override fun afterTextChanged(s:Editable?){if(s!=null && !synchronizing){history.record(s);last=AuthoringText.html(s);onChange(last)}}})
            }},update={e->e.isEnabled=enabled;e.setTextColor(foreground);e.highlightColor=androidx.core.graphics.ColorUtils.setAlphaComponent(foreground,64);if(android.os.Build.VERSION.SDK_INT>=29){e.textCursorDrawable?.mutate()?.setTint(foreground);e.textSelectHandleLeft?.mutate()?.setTint(foreground);e.textSelectHandleRight?.mutate()?.setTint(foreground);e.textSelectHandle?.mutate()?.setTint(foreground)};e.setBackgroundColor(background);e.setTextSize(TypedValue.COMPLEX_UNIT_PX,font);e.setPadding(inset,inset,inset,inset);e.typeface=typeface;if(value!=last){synchronizing=true;e.setText(AuthoringText.parse(value));last=value;history=AuthorHistory(e.text);synchronizing=false}})
            }
        } else SkinOutlinedTextField(value,{last=it;update(it)},Modifier.fillMaxWidth().heightIn(max=360.dp),enabled=enabled,label={Text("HTML source")},minLines=6)
    }
    if(warning)AlertDialog(onDismissRequest={warning=false},title={Text("HTML formatting")},text={Text("This content uses formatting the visual editor cannot preserve. Keep editing it as HTML.")},confirmButton={SkinTextButton(onClick={warning=false}){Text("OK")}})
    if(linkDialog)AlertDialog(onDismissRequest={linkDialog=false},title={Text("Link")},text={SkinOutlinedTextField(link,{link=it;invalidLink=false},label={Text("HTTPS URL")},supportingText={if(invalidLink)Text("Enter a complete HTTPS link.")},singleLine=true)},confirmButton={SkinTextButton(onClick={val safe=link.isBlank() || runCatching{val u=java.net.URI(link);u.scheme=="https" && !u.host.isNullOrBlank() && u.userInfo==null}.getOrDefault(false);if(safe){editor?.let{e->AuthoringText.link(e.text,selection.first,selection.second,link);changed()};linkDialog=false}else invalidLink=true}){Text("Save")}},dismissButton={SkinTextButton(onClick={linkDialog=false}){Text("Cancel")}})
}
