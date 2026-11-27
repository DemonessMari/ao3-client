package org.ao3client.reader

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.material3.ListItem
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp

@Composable fun MenuRow(
    title: String,
    onClick: () -> Unit,
    supportingText: String? = null,
    enabled: Boolean = true,
    trailing: @Composable (() -> Unit)? = null
) {
    ListItem(
        headlineContent = { Text(title) },
        supportingContent = supportingText?.let { text -> { Text(text) } },
        trailingContent = trailing,
        colors = ListItemDefaults.colors(containerColor = Color.Transparent,headlineColor=MaterialTheme.colorScheme.onSurface.copy(alpha=if(enabled)1f else .38f),supportingColor=MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha=if(enabled)1f else .38f)),
        modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp)
            .clickable(enabled=enabled,role = Role.Button, onClick = onClick)
    )
}
