package org.ao3client.reader

import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ButtonDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

internal val LocalSkinRadius=staticCompositionLocalOf<Int?>{null}

@Composable private fun SkinButtonContent(content:@Composable RowScope.()->Unit){
    val factor=skinRule("buttons").number("textScale",1.0).toFloat()
    val style=LocalSkinTypography.current.labelLarge
    androidx.compose.material3.ProvideTextStyle(style.copy(fontSize=style.fontSize*factor,lineHeight=style.lineHeight*factor)){androidx.compose.foundation.layout.Row(content=content)}
}
@Composable internal fun SkinButton(onClick:()->Unit,modifier:Modifier=Modifier,enabled:Boolean=true,content:@Composable RowScope.()->Unit) {
    val rule=skinRule("buttons");val scheme=LocalAppSkin.current?.colorScheme() ?: androidx.compose.material3.MaterialTheme.colorScheme
    androidx.compose.material3.Button(onClick,modifier,enabled,shape=skinShape("buttons"),border=skinBorder("buttons"),colors=ButtonDefaults.buttonColors(containerColor=rule.color("background",scheme.primary),contentColor=rule.color("text",scheme.onPrimary)),contentPadding=rule["padding"]?.let{androidx.compose.foundation.layout.PaddingValues((it as Number).toInt().dp)} ?: ButtonDefaults.ContentPadding){SkinButtonContent(content)}
}
@Composable internal fun SkinOutlinedButton(onClick:()->Unit,modifier:Modifier=Modifier,enabled:Boolean=true,content:@Composable RowScope.()->Unit) {
    val rule=skinRule("buttons");val scheme=LocalAppSkin.current?.colorScheme() ?: androidx.compose.material3.MaterialTheme.colorScheme
    androidx.compose.material3.OutlinedButton(onClick,modifier,enabled,shape=skinShape("buttons"),border=if(rule.containsKey("borderWidth") || rule.containsKey("border"))skinBorder("buttons") else ButtonDefaults.outlinedButtonBorder(enabled),colors=ButtonDefaults.outlinedButtonColors(containerColor=rule.color("background",if(rule.isEmpty())androidx.compose.ui.graphics.Color.Transparent else scheme.primary),contentColor=rule.color("text",if(rule.isEmpty())scheme.primary else scheme.onPrimary)),contentPadding=rule["padding"]?.let{androidx.compose.foundation.layout.PaddingValues((it as Number).toInt().dp)} ?: ButtonDefaults.ContentPadding){SkinButtonContent(content)}
}
@Composable internal fun SkinTextButton(onClick:()->Unit,modifier:Modifier=Modifier,enabled:Boolean=true,content:@Composable RowScope.()->Unit) {
    val rule=skinRule("buttons");val scheme=LocalAppSkin.current?.colorScheme() ?: androidx.compose.material3.MaterialTheme.colorScheme
    androidx.compose.material3.TextButton(onClick,modifier,enabled,shape=skinShape("buttons"),border=skinBorder("buttons"),colors=ButtonDefaults.textButtonColors(containerColor=rule.color("background",if(rule.isEmpty())androidx.compose.ui.graphics.Color.Transparent else scheme.primary),contentColor=rule.color("text",if(rule.isEmpty())scheme.primary else scheme.onPrimary)),contentPadding=rule["padding"]?.let{androidx.compose.foundation.layout.PaddingValues((it as Number).toInt().dp)} ?: ButtonDefaults.TextButtonContentPadding){SkinButtonContent(content)}
}
