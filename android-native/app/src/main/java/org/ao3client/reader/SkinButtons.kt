package org.ao3client.reader

import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ButtonDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

internal val LocalSkinRadius=staticCompositionLocalOf<Int?>{null}

@Composable internal fun SkinButton(onClick:()->Unit,modifier:Modifier=Modifier,enabled:Boolean=true,content:@Composable RowScope.()->Unit) {
    androidx.compose.material3.Button(onClick,modifier,enabled,shape=LocalSkinRadius.current?.let{RoundedCornerShape(it.dp)} ?: ButtonDefaults.shape,content=content)
}
@Composable internal fun SkinOutlinedButton(onClick:()->Unit,modifier:Modifier=Modifier,enabled:Boolean=true,content:@Composable RowScope.()->Unit) {
    androidx.compose.material3.OutlinedButton(onClick,modifier,enabled,shape=LocalSkinRadius.current?.let{RoundedCornerShape(it.dp)} ?: ButtonDefaults.outlinedShape,content=content)
}
@Composable internal fun SkinTextButton(onClick:()->Unit,modifier:Modifier=Modifier,enabled:Boolean=true,content:@Composable RowScope.()->Unit) {
    androidx.compose.material3.TextButton(onClick,modifier,enabled,shape=LocalSkinRadius.current?.let{RoundedCornerShape(it.dp)} ?: ButtonDefaults.textShape,content=content)
}
