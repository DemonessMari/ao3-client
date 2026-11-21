package org.ao3client.reader
import androidx.compose.runtime.*
import androidx.compose.foundation.*
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
@Composable fun AboutControls() {
    val context=LocalContext.current; var file by remember { mutableStateOf<String?>(null) }; var text by remember { mutableStateOf("") }
    Text("AO3 Client 3.1.0 · unofficial",style=MaterialTheme.typography.bodySmall)
    TextButton(onClick={file="PRIVACY.md"}) { Text("Privacy") }
    TextButton(onClick={file="THIRD-PARTY-NOTICES.txt"}) { Text("Third-party notices and licenses") }
    LaunchedEffect(file) { file?.let { name -> text=withContext(Dispatchers.IO) {
        context.assets.open(name).bufferedReader().use { it.readText() }+if(name=="THIRD-PARTY-NOTICES.txt") listOf("licenses/Apache-2.0.txt","licenses/Jsoup-MIT.txt","LICENSE.txt").joinToString("\n\n") { context.assets.open(it).bufferedReader().use { reader -> reader.readText() } } else ""
    } } }
    if(file!=null) AlertDialog(onDismissRequest={file=null},title={Text(if(file=="PRIVACY.md") "Privacy" else "Licenses")},text={Text(text,modifier=Modifier.heightIn(max=420.dp).verticalScroll(rememberScrollState()))},confirmButton={TextButton(onClick={file=null}) { Text("Close") }})
}
