package org.ao3client.reader

import org.jsoup.Jsoup
import org.jsoup.nodes.Element
import java.net.URI
import java.net.URLDecoder

internal class ToolSubmissionUncertain(message:String,cause:Throwable):Exception(message,cause)
internal data class ToolSection(val key:String,val label:String,val association:String,val html:String)
internal data class ToolSubmit(val name:String,val value:String,val label:String)
internal data class ToolUpload(val name:String,val filename:String,val mime:String,val bytes:ByteArray)
internal data class ToolForm(val key:String,val source:String,val action:String,val method:String,val title:String,val fields:List<AccountField>,val fixed:Map<String,List<String>>,val submits:List<ToolSubmit>,val checkpoint:Map<String,List<String>>,val authorityAction:String="",val authorityMethod:String="",val authorityKey:String="",val authorityOverride:String="",val sections:List<ToolSection> = emptyList(),val addedSections:Int=0)
internal data class ToolPage(val source:String,val user:String,val title:String,val text:String,val notices:List<String>,val links:List<Pair<String,String>>,val forms:List<ToolForm>,val csrf:String="",val feed:Boolean=false,val inProgress:Boolean=false,val previewHtml:String="",val previewCss:String="")
internal object NativeTools {
    private val roots=setOf("comments","works","chapters","series","collections","collection_items","collection_participants","challenge_signups","challenge_assignments","challenge_claims","potential_matches","prompts","gifts","related_works","serial_works","orphans","skins","tag_sets","tag_nominations","invitations","user_invite_requests","faq","archive_faqs","admin_posts","tos","tos_faq","content","privacy","support","abuse_reports","external_works","bookmarks")
    private val own=setOf("stats","comments","favorite_tags","works","series","pseuds","collections","collection_items","related_works","gifts","assignments","claims","signups","nominations","tag_sets","creatorships","external_authors","invitations","skins","profile","preferences","change_email","confirm_change_email","changed_email","change_password","changed_password","change_username","changed_username","reconfirm_email")
    fun route(value:String,user:String):String? {
        val uri=runCatching{URI(Archive.BASE).resolve(value)}.getOrNull() ?: return null
        if(uri.scheme!="https" || uri.host!="archiveofourown.org" || uri.port !in listOf(-1,443) || uri.userInfo!=null || value.startsWith('#') || value.length>8192)return null
        val segments=uri.path.trimEnd('/').split('/').drop(1)
        if(segments.isEmpty() || segments.any{it.isEmpty() || it in setOf(".","..") || it.any{c->c=='\\' || c.code<32}} || segments.size>12)return null
        val allowed=if(segments.first()=="users")segments.size>=2 && user.isNotEmpty() && segments[1]==user && (segments.size==2 || segments[2] in own) else segments.first() in roots || segments.first()=="tags" && segments.last() in setOf("feed","feed.atom")
        return if(allowed)uri.rawPath.trimEnd('/')+(uri.rawQuery?.let{"?$it"} ?: "") else null
    }
    private val namespaces=setOf("stat","external_author","skin_property","creation","tag_set_association","preference","signup","request","offer","claim","assignment","participant","work","chapter","series","collection","collection_profile","collection_preference","collection_item","collection_participant","challenge_signup","challenge_assignment","challenge_claim","gift_exchange","prompt_meme","prompt","potential_match","skin","work_skin","site_skin","skin_parent","bookmark","external_work","pseud","invitation","user_invite_request","related_work","serial_work","gift","tag_set","owned_tag_set","tag_set_nomination","nomination","creatorship","feedback","feedback_report","abuse_report","collection_search","work_search","bookmark_search","people_search","tag_search","user")
    private val scalars=setOf("password_check","password","password_confirmation","new_email","email_confirmation","new_username","username","login","commit","utf8","authenticity_token","_method","save_button","preview_button","edit_button","post_button","update_button","cancel_button","bookmarks_action","pseud_id","pseud_ids","owner_pseuds","work_ids","bookmark_ids","collection_ids","ids","id","page","sort_column","sort_direction","query","q","search","show","type","skin_type","add_site_parents","num_requests","num_offers","parent-options-show","chapters-options-show","series-options-show","backdate-options-show","coauthor-options-show","remove_me","orphan","orphan_pseud","send_me_copy","recipient","recipients","email","url","urls","import_url","import_urls","encoding","anonymous","approved","rejected","include_bookmarks","delete_bookmarks","transfer_bookmarks","status","participant_role")
    fun fieldAllowed(name:String):Boolean {
        if(name.length>240 || !name.matches(Regex("[A-Za-z0-9_\\[\\]()-]+")))return false
        val parts=name.split('[',']').filter{it.isNotEmpty()}
        if(parts.any{it in setOf("admin","is_admin","ticket_number","banned","suspended","roles","spam") || it.startsWith("admin_")})return false
        return if(parts.size==1)true else parts.first() in namespaces || (if(parts.first().endsWith("ies"))parts.first().dropLast(3)+"y" else parts.first().removeSuffix("s")) in namespaces || parts.first().endsWith("_search") || parts.first() in scalars
    }
    private fun Element.trusted()=(listOf(this)+parents()).none{it.hasClass("userstuff") || it.id()=="workskin" || it.tagName() in setOf("script","style","template")}
    private fun Element.controlsTrusted()=trusted() && parents().none{it.tagName()=="textarea"}
    private fun resolved(source:String,value:String)=runCatching{URI(Archive.BASE+source).resolve(value.ifEmpty{source}).toString()}.getOrDefault("")
    private val sectionCall=Regex("""^\s*add_section\(\s*this\s*,\s*"([A-Za-z_]+)"\s*,\s*("(?:\\.|[^"\\])*")\s*\)\s*;?\s*(?:return\s+false\s*;?)?\s*$""")
    private fun section(label:String,call:String):ToolSection? {
        if(call.length>512*1024)return null
        val match=sectionCall.matchEntire(call) ?: return null
        val literal=Regex("""\\(['`$])""").replace(match.groupValues[2],"\$1")
        val html=runCatching{org.json.JSONArray("[$literal]").getString(0)}.getOrNull() ?: return null
        if(html.length>262144 || !html.contains("new_"+match.groupValues[1]) || Regex("</?form\\b",RegexOption.IGNORE_CASE).containsMatchIn(html))return null
        val digest=java.security.MessageDigest.getInstance("SHA-256").digest(html.toByteArray()).joinToString(""){"%02x".format(it)}
        return ToolSection(match.groupValues[1]+"|"+digest.take(16),label,match.groupValues[1],html)
    }
    private fun values(form:ToolForm):Map<String,List<String>> = form.fixed.filterKeys{it !in setOf("authenticity_token","utf8") && !it.contains("password",true)}+form.fields.filter{it.type !in setOf("password","file")}.associate{it.name to it.values}+form.fields.associate{"@type:"+it.name to listOf(it.type)}+form.fields.filter{it.choices.isNotEmpty()}.associate{"@choices:"+it.name to it.choices.map{c->c.value}}+mapOf("@sections" to form.sections.map{it.key})
    fun parse(html:String,source:String,user:String):ToolPage {
        require(html.length<=8*1024*1024 && route(source,user)!=null){"Invalid AO3 tools page."}
        if(URI(Archive.BASE+source).path.endsWith("/feed") || URI(Archive.BASE+source).path.endsWith("/feed.atom")) {
            val doc=Jsoup.parse(html,"",org.jsoup.parser.Parser.xmlParser());val feed=doc.selectFirst("feed") ?: error("AO3 did not return a feed.")
            require(feed.attr("xmlns")=="http://www.w3.org/2005/Atom"){"AO3 did not return an Atom feed."}
            val links=feed.select("entry").take(200).mapNotNull{entry->entry.select("link[href]").firstOrNull{it.attr("rel") in setOf("","alternate")}?.let{link->route(link.attr("href"),user)?.takeIf{URI(Archive.BASE+it).path.matches(Regex("/works/[0-9]+"))}?.let{entry.selectFirst("title")?.text().orEmpty() to it}}}
            val text=feed.select("entry").take(200).joinToString("\n\n"){entry->listOf(entry.selectFirst("title")?.text(),entry.selectFirst("updated")?.text(),entry.selectFirst("summary,content")?.let{Jsoup.parse(it.text()).text()}).filterNotNull().joinToString("\n")}.take(1024*1024)
            return ToolPage(source,user,feed.selectFirst("title")?.text().orEmpty(),text,emptyList(),links,emptyList(),feed=true)
        }
        val doc=Jsoup.parse(html);val identity=ArchiveParser.sessionIdentity(html)
        require(user.isEmpty() || identity==user){"AO3 could not verify this account. Check native sign-in."}
        val main=doc.select("#main").firstOrNull{it.trusted()} ?: error("AO3 did not return this page.")
        val title=main.select("h2.heading, h2.title, h2").firstOrNull{it.trusted()}?.text().orEmpty().ifBlank{"AO3"}
        val forms=mutableListOf<ToolForm>()
        val meta=doc.selectFirst("head meta[name=csrf-token]")?.attr("content").orEmpty()
        for((index,form) in main.select("form").filter{it.trusted()}.withIndex()) {
            val action=route(resolved(source,form.attr("action")),user) ?: continue
            val method=form.attr("method").lowercase().ifEmpty{"get"};if(method !in setOf("get","post") || method=="post" && user.isEmpty())continue
            val controls=form.select("input[name], textarea[name], select[name]").filter{it.controlsTrusted() && !it.hasAttr("disabled") && fieldAllowed(it.attr("name"))}
            if(form.select("input[name],textarea[name],select[name]").any{it.controlsTrusted() && !it.hasAttr("disabled") && !fieldAllowed(it.attr("name"))})continue
            val hidden=controls.filter{!it.attr("name").contains("password",true) && !it.attr("name").endsWith("[_destroy]") && (it.tagName()=="input" && it.attr("type").equals("hidden",true) || it.hasAttr("readonly"))}.groupBy({it.attr("name")},{it.`val`()})
            if(method=="post" && (hidden["authenticity_token"]?.size!=1 || hidden["authenticity_token"]!!.single().isBlank()) || (hidden["_method"]?.size ?: 0)>1)continue
            if(hidden["_method"]?.singleOrNull().orEmpty() !in setOf("","put","patch","delete"))continue
            fun label(el:Element)=form.select("label[for]").firstOrNull{it.controlsTrusted() && el.id().isNotEmpty() && it.attr("for")==el.id()}?.text()?.takeIf{it.isNotBlank()} ?: el.attr("aria-label").ifBlank{el.attr("name").split('[',']').filter{it.isNotEmpty() && !it.all(Char::isDigit)}.lastOrNull().orEmpty().replace('_',' ').replaceFirstChar{it.uppercase()}}
            val fields=controls.filter{it.attr("name").contains("password",true) || it.attr("name").endsWith("[_destroy]") || !it.hasAttr("readonly") && (it.tagName()!="input" || it.attr("type").lowercase() !in setOf("hidden","submit","button","reset","image"))}.groupBy{it.attr("name")}.mapNotNull{(name,list)->
                val el=list.first();val type=el.attr("type").lowercase();val nameLabel=label(el)
                when {
                    name.contains("password",true)->AccountField(name,nameLabel,"password",listOf(""))
                    name.endsWith("[_destroy]")->AccountField(name,"Remove item","check",listOf(el.`val`().ifBlank{"0"}),listOf(Choice("Remove item","1")),el.`val`().ifBlank{"0"})
                    el.tagName()=="select"->{val choices=el.select("option").filter{!it.hasAttr("disabled") && it.parents().none{p->p.tagName()=="optgroup" && p.hasAttr("disabled")}}.map{Choice(it.text(),if(it.hasAttr("value"))it.attr("value") else it.text())}.distinctBy{it.value};val selected=el.select("option[selected]").map{if(it.hasAttr("value"))it.attr("value") else it.text()}.filter{v->choices.any{it.value==v}};val multi=el.hasAttr("multiple");AccountField(name,nameLabel,if(multi)"multi" else "choice",if(multi)selected else selected.takeLast(1).ifEmpty{choices.take(1).map{it.value}},choices)}
                    el.tagName()=="textarea"->AccountField(name,nameLabel,"area",listOf(el.`val`()))
                    type in setOf("checkbox","radio")-> {val choices=list.map{Choice(label(it),it.attr("value").ifEmpty{"1"})}.distinctBy{it.value};val off=hidden[name]?.lastOrNull() ?: "0";val check=type=="checkbox" && choices.size==1 && !name.endsWith("[]");val selected=list.filter{it.hasAttr("checked")}.map{it.attr("value").ifEmpty{"1"}};AccountField(name,if(choices.size>1)nameLabel.substringBefore(':') else nameLabel,if(check)"check" else if(type=="radio")"choice" else "multi",if(check && selected.isEmpty())listOf(off) else selected,choices,off)}
                    type=="file"->AccountField(name,nameLabel,"file",emptyList())
                    type=="password"->AccountField(name,nameLabel,"password",listOf(""))
                    type in setOf("","text","url","email","number","date","datetime-local","time","tel","search","datetime","color","range","month","week")->AccountField(name,nameLabel,"text",listOf(el.`val`()))
                    else->null
                }
            }
            if(method=="get" && fields.any{it.type in setOf("password","file")})continue
            val buttons=form.select("input[type=submit],button:not([type]),button[type=submit]").filter{it.controlsTrusted() && !it.hasAttr("disabled") && (it.attr("name").isEmpty() || fieldAllowed(it.attr("name")))}.map{ToolSubmit(it.attr("name"),it.attr("value"),if(it.tagName()=="input")it.attr("value") else it.text())}.distinct().ifEmpty{listOf(ToolSubmit("","",if(method=="get")"Apply" else "Submit"))}
            val fixed=hidden.filterKeys{n->fields.none{it.name==n}}
            val formTitle=form.select("legend").firstOrNull{it.trusted()}?.text().orEmpty().ifBlank{buttons.first().label}
            val sections=form.select("a[onclick]").filter{it.controlsTrusted()}.mapNotNull{section(it.text(),it.attr("onclick"))}.distinctBy{it.key}
            val f=ToolForm(form.id().ifEmpty{"form-$index"}+"|$action|$method",source,action,method,formTitle,fields,fixed,buttons,emptyMap(),sections=sections);forms+=f.copy(checkpoint=values(f))
        }
        if(user.isNotEmpty() && meta.isNotEmpty())for(link in main.select("a[data-method][href]").filter{it.trusted()}) {
            val method=link.attr("data-method").lowercase();if(method !in setOf("post","put","patch","delete"))continue
            val action=route(resolved(source,link.attr("href")),user) ?: continue
            val f=ToolForm("link|$action|$method",source,action,"post",link.text(),emptyList(),mapOf("authenticity_token" to listOf(meta))+if(method=="post")emptyMap() else mapOf("_method" to listOf(method)),listOf(ToolSubmit("","",link.text())),emptyMap());forms+=f.copy(checkpoint=values(f))
        }
        val links=main.select("a[href]").filter{it.trusted() && !it.hasAttr("data-method")}.mapNotNull{link->route(resolved(source,link.attr("href")),user)?.let{link.text().trim() to it}}.filter{it.first.isNotEmpty()}.distinct().take(2000)
        val notices=doc.select(".flash.notice,.flash.error,#main .error,#main .notice:not(.required)").filter{it.trusted()}.map{it.text()}.filter{it.isNotBlank()}.distinct()
        val copy=main.clone();copy.select("form,script,style,template,.actions,.navigation,h2,.landmark").remove()
        val body=copy.wholeText().replace(Regex("\\n[ \\t]*\\n(?:[ \\t]*\\n)+"),"\n\n").trim().take(1024*1024)
        val preview=main.select("#previewpane #workskin").firstOrNull{it.parents().none{p->p.hasClass("userstuff")}}
        val css=if(preview!=null)(doc.select("#work-skin link[rel=stylesheet][href]").mapNotNull{publicImageUrl(it.attr("href"))}.joinToString("\n"){"@import url(\""+it.replace("\"","%22")+"\");"}+"\n"+doc.select("#work-skin style").joinToString("\n"){it.data()}).take(262144) else ""
        return ToolPage(source,user,title,body,notices,links,forms,meta,previewHtml=preview?.let{staticReadingHtml(it.html())}.orEmpty(),previewCss=css)
    }
    fun dirty(page:ToolPage?,draft:Map<String,Map<String,List<String>>>,uploads:Map<String,Map<String,ToolUpload>>):Boolean = page?.inProgress==true || uploads.values.any{it.isNotEmpty()} || page?.forms?.any{form->form.fields.any{(draft[form.key]?.get(it.name) ?: it.values)!=it.values}}==true
    fun memoryCost(page:ToolPage,draft:Map<String,Map<String,List<String>>>,uploads:Map<String,Map<String,ToolUpload>>):Long {
        val strings=page.text.length.toLong()+page.previewHtml.length+page.previewCss.length+page.forms.sumOf{form->form.fields.sumOf{it.values.sumOf{v->v.length.toLong()}}+form.fixed.values.sumOf{it.sumOf{v->v.length.toLong()}}+form.checkpoint.values.sumOf{it.sumOf{v->v.length.toLong()}}+form.sections.sumOf{it.html.length.toLong()}}+draft.values.sumOf{fields->fields.values.sumOf{it.sumOf{v->v.length.toLong()}}}
        return strings*2+uploads.values.sumOf{files->files.values.sumOf{it.bytes.size.toLong()}}
    }
    fun prepare(original:ToolForm,freshPage:ToolPage,draft:Map<String,List<String>>,button:ToolSubmit):List<Pair<String,String>> {
        require(button in original.submits){"This action is no longer available."}
        val action=original.authorityAction.ifEmpty{original.action};val method=original.authorityMethod.ifEmpty{original.method}
        val override=if(original.authorityAction.isEmpty())original.fixed["_method"]?.firstOrNull().orEmpty() else original.authorityOverride
        val key=original.authorityKey.ifEmpty{original.key}
        val fresh=if(original.method=="get")original else freshPage.forms.firstOrNull{it.action==action && it.method==method && (it.key==key || it.fixed["_method"]?.firstOrNull().orEmpty()==override)} ?: error("AO3 no longer offers this action. Reload the page.")
        require(original.method=="get" || values(fresh)==original.checkpoint){"This page changed on AO3. Reload it before submitting."}
        require(draft.values.sumOf{it.sumOf(String::length)}<=4*1024*1024){"This form is too large."}
        val result=original.fixed.toMutableMap();result.remove("authenticity_token");result.remove("utf8")
        for(field in original.fields) {
            val v=draft[field.name] ?: field.values
            require(field.type=="multi" || v.size<=1){"Invalid field value."}
            if(field.type in setOf("choice","multi","check"))require(v.all{value->field.choices.any{it.value==value} || field.type=="check" && value==field.offValue}){"An option is no longer available."}
            if(field.type!="file")result[field.name]=v.ifEmpty{listOf("")}
        }
        require(draft.keys.all{n->original.fields.any{it.name==n}}){"Unknown form field."}
        if(original.method=="post") {
            val token=if(original.action==fresh.action)fresh.fixed["authenticity_token"]?.firstOrNull().orEmpty() else freshPage.csrf
            require(token.isNotEmpty()){"AO3 did not return a fresh token. Reload this page."}
            result["authenticity_token"]=listOf(token)
        }
        if(button.name.isNotEmpty())result[button.name]=listOf(button.value)
        return result.flatMap{(name,values)->values.map{name to it}}
    }
    fun continued(page:ToolPage,original:ToolForm,postedPath:String):ToolPage = if(page.source.substringBefore('?')==postedPath.substringBefore('?'))page.copy(source=original.source,inProgress=page.forms.any{it.method=="post"},forms=page.forms.map{f->if(f.method=="post")f.copy(source=original.source,checkpoint=original.checkpoint,authorityAction=original.authorityAction.ifEmpty{original.action},authorityMethod=original.authorityMethod.ifEmpty{original.method},authorityKey=original.authorityKey.ifEmpty{original.key},authorityOverride=if(original.authorityAction.isEmpty())original.fixed["_method"]?.firstOrNull().orEmpty() else original.authorityOverride) else f}) else page
    fun addSection(form:ToolForm,key:String,user:String):ToolForm {
        val section=form.sections.firstOrNull{it.key==key} ?: error("This section is unavailable.")
        require(form.addedSections<50 && form.fields.size<1000){"This form has too many sections."}
        val html=section.html.replace("new_"+section.association,"native_"+System.currentTimeMillis()+"_"+form.addedSections)
        val hidden="<input name='authenticity_token' type='hidden' value='template'>"+(form.fixed["_method"]?.firstOrNull()?.let{"<input name='_method' type='hidden' value='"+it+"'>"} ?: "")
        val page="<nav id='greeting'><a href='/users/$user'>$user</a><a href='/users/logout'>Out</a></nav><div id='main'><form method='${form.method}' action=\"${org.jsoup.nodes.Entities.escape(form.action)}\">$hidden$html</form></div>"
        val added=parse(page,form.source,user).forms.singleOrNull() ?: error("AO3 returned an unsupported section.")
        require(added.fields.none{field->form.fields.any{it.name==field.name}}){"This section is already present."}
        return form.copy(fields=form.fields+added.fields,fixed=form.fixed+added.fixed.filterKeys{it !in setOf("authenticity_token","_method")},sections=(form.sections+added.sections).distinctBy{it.key},addedSections=form.addedSections+1)
    }
    fun readImage(stream:java.io.InputStream):ByteArray {
        val out=java.io.ByteArrayOutputStream();val buffer=ByteArray(8192)
        while(out.size()<=2*1024*1024){val n=stream.read(buffer,0,minOf(buffer.size,2*1024*1024+1-out.size()));if(n<0)break;if(n>0)out.write(buffer,0,n)}
        return out.toByteArray()
    }
    fun upload(name:String,filename:String,bytes:ByteArray):ToolUpload {
        require(fieldAllowed(name) && bytes.size in 1..2*1024*1024){"Choose an image smaller than 2 MB."}
        val mime=when{bytes.size>=8 && bytes.take(8).toByteArray().contentEquals(byteArrayOf(-119,80,78,71,13,10,26,10))->"image/png";bytes.size>=3 && bytes[0]==(-1).toByte() && bytes[1]==(-40).toByte() && bytes[2]==(-1).toByte()->"image/jpeg";bytes.size>=6 && String(bytes,0,6,Charsets.US_ASCII) in setOf("GIF87a","GIF89a")->"image/gif";else->error("Choose a PNG, JPEG or GIF image.")}
        return ToolUpload(name,filename.substringAfterLast('/').substringAfterLast('\\').replace(Regex("[^A-Za-z0-9_.-]"),"_").take(120).ifEmpty{"image"},mime,bytes)
    }
    fun multipart(fields:List<Pair<String,String>>,uploads:List<ToolUpload>,boundary:String):ByteArray {
        require(boundary.matches(Regex("[A-Za-z0-9-]{8,100}")) && uploads.sumOf{it.bytes.size}<=8*1024*1024)
        val out=java.io.ByteArrayOutputStream()
        fun text(s:String){out.write(s.toByteArray(Charsets.UTF_8))}
        for((name,value) in fields){require(fieldAllowed(name));text("--$boundary\r\nContent-Disposition: form-data; name=\"$name\"\r\n\r\n$value\r\n")}
        for(file in uploads){val safe=upload(file.name,file.filename,file.bytes);text("--$boundary\r\nContent-Disposition: form-data; name=\"${safe.name}\"; filename=\"${safe.filename}\"\r\nContent-Type: ${safe.mime}\r\n\r\n");out.write(safe.bytes);text("\r\n")}
        text("--$boundary--\r\n");return out.toByteArray()
    }
}
