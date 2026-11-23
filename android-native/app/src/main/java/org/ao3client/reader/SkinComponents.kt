package org.ao3client.reader

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.input.TextFieldValue
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp

internal val LocalSkinTypography=staticCompositionLocalOf{Typography()}
internal val LocalAppSkin=staticCompositionLocalOf<AppSkin?>{null}
internal fun Map<String,Any>.number(key:String,fallback:Double)=(get(key) as? Number)?.toDouble() ?: fallback
internal fun Map<String,Any>.color(key:String,fallback:Color)=(get(key) as? String)?.let{Color(android.graphics.Color.parseColor(it))} ?: fallback
@Composable internal fun skinRule(role:String)=LocalAppSkin.current?.components?.get(role).orEmpty()
@Composable internal fun skinShape(role:String):androidx.compose.ui.graphics.Shape {
    val rule=skinRule(role);val radius=(rule["cornerRadius"] as? Number)?.toInt() ?: LocalSkinRadius.current
    return if(radius!=null)RoundedCornerShape(radius.dp) else when(role){"buttons"->ButtonDefaults.shape;"inputs"->OutlinedTextFieldDefaults.shape;"header"->androidx.compose.ui.graphics.RectangleShape;else->CardDefaults.shape}
}
@Composable internal fun skinBorder(role:String):BorderStroke? {val rule=skinRule(role);val width=rule.number("borderWidth",if(role=="cards" || rule.containsKey("border"))1.0 else 0.0);return if(width>0)BorderStroke(width.dp,rule.color("border",MaterialTheme.colorScheme.onSurface)) else null}
@Composable internal fun SkinRole(role:String,content:@Composable ()->Unit){
    val rule=skinRule(role);val scheme=MaterialTheme.colorScheme
    val bg=rule.color("background",if(role=="header")scheme.background else scheme.surface)
    val fg=rule.color("text",scheme.onSurface)
    val factor=rule.number("textScale",1.0).toFloat()
    fun scaled(style:TextStyle)=style.copy(fontSize=style.fontSize*factor,lineHeight=style.lineHeight*factor)
    val t=LocalSkinTypography.current
    val typography=t.copy(displayLarge=scaled(t.displayLarge),displayMedium=scaled(t.displayMedium),displaySmall=scaled(t.displaySmall),headlineLarge=scaled(t.headlineLarge),headlineMedium=scaled(t.headlineMedium),headlineSmall=scaled(t.headlineSmall),titleLarge=scaled(t.titleLarge),titleMedium=scaled(t.titleMedium),titleSmall=scaled(t.titleSmall),bodyLarge=scaled(t.bodyLarge),bodyMedium=scaled(t.bodyMedium),bodySmall=scaled(t.bodySmall),labelLarge=scaled(t.labelLarge),labelMedium=scaled(t.labelMedium),labelSmall=scaled(t.labelSmall))
    MaterialTheme(colorScheme=scheme.copy(surface=bg,onSurface=fg,onSurfaceVariant=fg,background=bg,onBackground=fg,surfaceContainer=bg,primary=fg),typography=typography){ProvideTextStyle(typography.bodyLarge.copy(color=fg),content)}
}
@Composable internal fun SkinHeader(content:@Composable ()->Unit){
    val rule=skinRule("header")
    SkinRole("header"){Surface(shape=skinShape("header"),border=skinBorder("header"),color=MaterialTheme.colorScheme.background){Box(Modifier.fillMaxWidth().padding(rule.number("padding",0.0).dp)){content()}}}
}
@Composable internal fun SkinCard(onClick:()->Unit,modifier:Modifier=Modifier,content:@Composable ColumnScope.()->Unit){
    val rule=skinRule("cards")
    SkinRole("cards"){Card(onClick=onClick,modifier=modifier,shape=skinShape("cards"),border=skinBorder("cards"),colors=CardDefaults.cardColors(containerColor=MaterialTheme.colorScheme.surface,contentColor=MaterialTheme.colorScheme.onSurface)){Column(Modifier.padding(rule.number("padding",0.0).dp),content=content)}}
}
@Composable private fun skinInputColors():TextFieldColors {
    val r=skinRule("inputs");val s=MaterialTheme.colorScheme;val fg=r.color("text",s.onSurface);val bg=r.color("background",s.surface);val border=if(r.containsKey("borderWidth"))Color.Transparent else r.color("border",s.outline)
    return OutlinedTextFieldDefaults.colors(focusedTextColor=fg,unfocusedTextColor=fg,focusedContainerColor=bg,unfocusedContainerColor=bg,focusedBorderColor=border,unfocusedBorderColor=border,focusedLabelColor=fg,unfocusedLabelColor=fg,cursorColor=fg)
}
@Composable private fun skinInputModifier(modifier:Modifier):Modifier {
    val rule=skinRule("inputs");val base=modifier.padding(rule.number("padding",0.0).dp)
    val border=skinBorder("inputs")
    return if(rule.containsKey("borderWidth") && border!=null)base.border(border,skinShape("inputs")) else base
}
@Composable internal fun SkinOutlinedTextField(value:String,onValueChange:(String)->Unit,modifier:Modifier=Modifier,enabled:Boolean=true,label:(@Composable ()->Unit)?=null,placeholder:(@Composable ()->Unit)?=null,supportingText:(@Composable ()->Unit)?=null,singleLine:Boolean=false,minLines:Int=1,visualTransformation:VisualTransformation=VisualTransformation.None,keyboardOptions:KeyboardOptions=KeyboardOptions.Default,keyboardActions:KeyboardActions=KeyboardActions.Default){
    val rule=skinRule("inputs")
    SkinRole("inputs"){OutlinedTextField(value,onValueChange,skinInputModifier(modifier),enabled=enabled,label=label,placeholder=placeholder,supportingText=supportingText,singleLine=singleLine,minLines=minLines,visualTransformation=visualTransformation,keyboardOptions=keyboardOptions,keyboardActions=keyboardActions,shape=skinShape("inputs"),colors=skinInputColors(),textStyle=MaterialTheme.typography.bodyLarge)}
}
@Composable internal fun SkinOutlinedTextField(value:TextFieldValue,onValueChange:(TextFieldValue)->Unit,modifier:Modifier=Modifier,enabled:Boolean=true,label:(@Composable ()->Unit)?=null,placeholder:(@Composable ()->Unit)?=null,singleLine:Boolean=false){
    val rule=skinRule("inputs")
    SkinRole("inputs"){OutlinedTextField(value,onValueChange,skinInputModifier(modifier),enabled=enabled,label=label,placeholder=placeholder,singleLine=singleLine,shape=skinShape("inputs"),colors=skinInputColors(),textStyle=MaterialTheme.typography.bodyLarge)}
}
