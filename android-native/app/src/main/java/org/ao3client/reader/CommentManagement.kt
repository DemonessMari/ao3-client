package org.ao3client.reader
import org.jsoup.Jsoup
import org.jsoup.nodes.Element
import java.net.URI

internal data class CommentControl(val label:String,val path:String,val fields:Map<String,String> = emptyMap())
internal object CommentManagement {
    private fun trusted(node:Element)=node.parents().none{it.hasClass("userstuff") || it.id()=="workskin"} && !node.hasClass("userstuff")
    private val labels=mapOf("freeze" to "Freeze thread","unfreeze" to "Unfreeze thread","hide" to "Hide comment","unhide" to "Show comment","review" to "Approve comment","reject" to "Mark as spam","approve" to "Not spam")
    fun controls(root:Element,id:String):List<CommentControl> {
        val nav=root.select("#navigation_for_comment_$id").firstOrNull{trusted(it)} ?: return emptyList()
        val result=mutableListOf<CommentControl>()
        nav.select("a[href]").filter(::trusted).forEach{link->
            val uri=runCatching{URI(Archive.BASE).resolve(link.attr("href"))}.getOrNull() ?: return@forEach
            val path=ao3Path(link.attr("href")) ?: return@forEach
            val target=uri.rawPath+(uri.rawQuery?.let{"?$it"} ?: "")
            when {
                path=="/comments/$id/edit"->result+=CommentControl("Edit",target)
                path=="/comments/delete_comment" && Regex("(?:^|&)id="+Regex.escape(id)+"(?:&|$)").containsMatchIn(uri.rawQuery.orEmpty())->result+=CommentControl("Delete",target)
                path.startsWith("/comments/$id/") && link.attr("data-method")=="put"->{val action=path.substringAfterLast('/');val label=labels[action] ?: return@forEach
                    val token=root.ownerDocument()?.select("head meta[name=csrf-token]")?.firstOrNull()?.attr("content").orEmpty()
                    if(token.isNotBlank())result+=CommentControl(label,path,mapOf("authenticity_token" to token,"_method" to "put"))
                }
            }
        }
        nav.select("form[method=post]").filter(::trusted).forEach{form->
            val path=ao3Path(form.attr("action")) ?: return@forEach
            val match=Regex("/comments/"+Regex.escape(id)+"/(freeze|unfreeze|hide|unhide|review|reject|approve)").matchEntire(path) ?: return@forEach
            val fields=form.select("input[type=hidden][name]:not([disabled])").filter{trusted(it) && it.attr("name") in setOf("authenticity_token","_method")}.associate{it.attr("name") to it.attr("value")}
            if(fields["_method"]=="put" && !fields["authenticity_token"].isNullOrBlank())result+=CommentControl(labels.getValue(match.groupValues[1]),path,fields)
        }
        return result.distinctBy{it.label}
    }
    fun deleteForm(html:String,id:String):CommentControl {
        val form=Jsoup.parse(html).select("form[method=post]").firstOrNull{trusted(it) && ao3Path(it.attr("action"))=="/comments/$id"} ?: error("AO3 did not return the delete confirmation form.")
        val fields=form.select("input[type=hidden][name]:not([disabled])").filter{trusted(it) && it.attr("name") in setOf("authenticity_token","_method")}.associate{it.attr("name") to it.attr("value")}
        require(fields["_method"]=="delete" && !fields["authenticity_token"].isNullOrBlank())
        return CommentControl("Delete","/comments/$id",fields)
    }
}
