@file:OptIn(androidx.compose.material3.ExperimentalMaterial3Api::class)
package org.ao3client.reader

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsFocusedAsState
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.selection.LocalTextSelectionColors
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.input.TextFieldValue
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

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
    val rule=skinRule(role);val scheme=if(role=="header")LocalAppSkin.current?.colorScheme() ?: MaterialTheme.colorScheme else MaterialTheme.colorScheme
    val bg=rule.color("background",if(role=="header")scheme.background else scheme.surface)
    val fg=rule.color("text",scheme.onSurface)
    val factor=rule.number("textScale",1.0).toFloat()
    fun scaled(style:TextStyle)=style.copy(fontSize=style.fontSize*factor,lineHeight=style.lineHeight*factor)
    val t=LocalSkinTypography.current
    val typography=t.copy(displayLarge=scaled(t.displayLarge),displayMedium=scaled(t.displayMedium),displaySmall=scaled(t.displaySmall),headlineLarge=scaled(t.headlineLarge),headlineMedium=scaled(t.headlineMedium),headlineSmall=scaled(t.headlineSmall),titleLarge=scaled(t.titleLarge),titleMedium=scaled(t.titleMedium),titleSmall=scaled(t.titleSmall),bodyLarge=scaled(t.bodyLarge),bodyMedium=scaled(t.bodyMedium),bodySmall=scaled(t.bodySmall),labelLarge=scaled(t.labelLarge),labelMedium=scaled(t.labelMedium),labelSmall=scaled(t.labelSmall))
    MaterialTheme(colorScheme=scheme.copy(surface=bg,onSurface=fg,onSurfaceVariant=fg,background=bg,onBackground=fg,surfaceContainer=bg,primary=fg),typography=typography){ProvideTextStyle(typography.bodyLarge.copy(color=fg),content)}
}
@Composable internal fun SkinHeader(content:@Composable ()->Unit){
    val rule=skinRule("header");val banner=LocalAppSkin.current?.layers?.firstOrNull{it.target=="header" && it.placement=="banner"}
    SkinRole("header"){Surface(shape=skinShape("header"),border=skinBorder("header"),color=MaterialTheme.colorScheme.background){Column{
        Box(Modifier.fillMaxWidth()) {SkinImageCanvas("header",Modifier.matchParentSize(),background=MaterialTheme.colorScheme.background,foreground=MaterialTheme.colorScheme.onBackground);Box(Modifier.fillMaxWidth().padding(rule.number("padding",0.0).dp)){content()}}
        if(banner!=null)SkinImageCanvas("header",Modifier.fillMaxWidth().height(banner.height.dp),banner=true)
    }}}
}
@Composable internal fun SkinCard(onClick:()->Unit,modifier:Modifier=Modifier,content:@Composable ColumnScope.()->Unit){
    val rule=skinRule("cards")
    SkinRole("cards"){Card(onClick=onClick,modifier=modifier,shape=skinShape("cards"),border=skinBorder("cards"),colors=CardDefaults.cardColors(containerColor=MaterialTheme.colorScheme.surface,contentColor=MaterialTheme.colorScheme.onSurface)){
        Box{SkinImageCanvas("cards",Modifier.matchParentSize(),background=MaterialTheme.colorScheme.surface,foreground=MaterialTheme.colorScheme.onSurface);Column(Modifier.padding(rule.number("padding",16.0).dp),verticalArrangement=Arrangement.spacedBy(8.dp),content=content)}
    }}
}
@Composable private fun skinInputColors():TextFieldColors {
    val r=skinRule("inputs");val s=MaterialTheme.colorScheme;val fg=r.color("text",s.onSurface);val bg=r.color("background",s.surface);val border=if(r.number("borderWidth",-1.0)==0.0)Color.Transparent else r.color("border",s.outline)
    return OutlinedTextFieldDefaults.colors(focusedTextColor=fg,unfocusedTextColor=fg,focusedContainerColor=bg,unfocusedContainerColor=bg,focusedBorderColor=border,unfocusedBorderColor=border,disabledBorderColor=border.copy(alpha=if(border==Color.Transparent)0f else .38f),focusedLabelColor=fg,unfocusedLabelColor=fg,cursorColor=fg)
}
@Composable private fun skinInputModifier(modifier:Modifier,hasLabel:Boolean):Modifier {
    val outer=modifier.padding(skinRule("inputs").number("padding",0.0).dp)
    val labelSpace=with(LocalDensity.current){8.sp.toDp()}
    return outer.then(if(hasLabel)Modifier.semantics(mergeDescendants=true){}.padding(top=labelSpace) else Modifier).defaultMinSize(OutlinedTextFieldDefaults.MinWidth,OutlinedTextFieldDefaults.MinHeight)
}
@Composable private fun skinInputTextStyle(colors:TextFieldColors,enabled:Boolean,interaction:MutableInteractionSource):TextStyle {
    val focused by interaction.collectIsFocusedAsState()
    return MaterialTheme.typography.bodyLarge.copy(color=if(!enabled)colors.disabledTextColor else if(focused)colors.focusedTextColor else colors.unfocusedTextColor)
}
@Composable private fun SkinInputDecoration(value:String,enabled:Boolean,singleLine:Boolean,visual:VisualTransformation,interaction:MutableInteractionSource,colors:TextFieldColors,label:(@Composable ()->Unit)?,placeholder:(@Composable ()->Unit)?,supportingText:(@Composable ()->Unit)?,inner:@Composable ()->Unit){
    val width=(skinRule("inputs")["borderWidth"] as? Number)?.toFloat()?.dp
    OutlinedTextFieldDefaults.DecorationBox(value=value,innerTextField=inner,enabled=enabled,singleLine=singleLine,visualTransformation=visual,interactionSource=interaction,label=label,placeholder=placeholder,supportingText=supportingText,colors=colors,container={
        OutlinedTextFieldDefaults.Container(enabled=enabled,isError=false,interactionSource=interaction,colors=colors,shape=skinShape("inputs"),focusedBorderThickness=width ?: OutlinedTextFieldDefaults.FocusedBorderThickness,unfocusedBorderThickness=width ?: OutlinedTextFieldDefaults.UnfocusedBorderThickness)
    })
}
@Composable internal fun SkinOutlinedTextField(value:String,onValueChange:(String)->Unit,modifier:Modifier=Modifier,enabled:Boolean=true,label:(@Composable ()->Unit)?=null,placeholder:(@Composable ()->Unit)?=null,supportingText:(@Composable ()->Unit)?=null,singleLine:Boolean=false,minLines:Int=1,visualTransformation:VisualTransformation=VisualTransformation.None,keyboardOptions:KeyboardOptions=KeyboardOptions.Default,keyboardActions:KeyboardActions=KeyboardActions.Default){
    SkinRole("inputs"){
        val interaction=remember{MutableInteractionSource()};val colors=skinInputColors()
        CompositionLocalProvider(LocalTextSelectionColors provides colors.textSelectionColors){
            BasicTextField(value=value,onValueChange=onValueChange,modifier=skinInputModifier(modifier,label!=null),enabled=enabled,singleLine=singleLine,minLines=minLines,visualTransformation=visualTransformation,keyboardOptions=keyboardOptions,keyboardActions=keyboardActions,interactionSource=interaction,textStyle=skinInputTextStyle(colors,enabled,interaction),cursorBrush=SolidColor(colors.cursorColor),decorationBox={inner->SkinInputDecoration(value,enabled,singleLine,visualTransformation,interaction,colors,label,placeholder,supportingText,inner)})
        }
    }
}
@Composable internal fun SkinOutlinedTextField(value:TextFieldValue,onValueChange:(TextFieldValue)->Unit,modifier:Modifier=Modifier,enabled:Boolean=true,label:(@Composable ()->Unit)?=null,placeholder:(@Composable ()->Unit)?=null,singleLine:Boolean=false){
    SkinRole("inputs"){
        val interaction=remember{MutableInteractionSource()};val colors=skinInputColors()
        CompositionLocalProvider(LocalTextSelectionColors provides colors.textSelectionColors){
            BasicTextField(value=value,onValueChange=onValueChange,modifier=skinInputModifier(modifier,label!=null),enabled=enabled,singleLine=singleLine,interactionSource=interaction,textStyle=skinInputTextStyle(colors,enabled,interaction),cursorBrush=SolidColor(colors.cursorColor),decorationBox={inner->SkinInputDecoration(value.text,enabled,singleLine,VisualTransformation.None,interaction,colors,label,placeholder,null,inner)})
        }
    }
}
