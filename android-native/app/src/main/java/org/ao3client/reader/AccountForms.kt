package org.ao3client.reader

import org.jsoup.Jsoup
import org.jsoup.nodes.Element
import java.net.URI
import java.net.URLDecoder

internal data class AccountField(val name:String,val label:String,val type:String,val values:List<String>,val choices:List<Choice> = emptyList(),val offValue:String="0")
internal data class AccountForm(val kind:String,val id:String,val user:String,val source:String,val path:String,val method:String,val title:String,val notice:String,val fields:List<AccountField>,val fixed:Map<String,List<String>>,val destructive:Boolean=false)
internal object AccountForms {
    private fun segment(s:String)=Archive.encode(s).replace("+","%20")
    fun source(kind:String,user:String,id:String=""):String {
        require(user.matches(Regex("[A-Za-z0-9_]+")))
        val own="/users/$user"
        return when(kind) {
            "preferences"->"$own/preferences";"profile"->"$own/profile/edit"
            "pseud"->{require(id.isEmpty() || id.length<=150 && id !in listOf(".","..") && '/' !in id && '\u0000' !in id);if(id.isEmpty())"$own/pseuds/new" else "$own/pseuds/${segment(id)}/edit"}
            "bookmark","deleteBookmark"->{require(id.matches(Regex("[0-9]+")));"/bookmarks/$id/"+if(kind=="bookmark")"edit" else "confirm_delete"}
            "seriesBookmark"->{require(id.matches(Regex("[0-9]+")));"/series/$id/bookmarks/new"}
            "externalBookmark"->"/external_works/new"
            "block","mute"->{require(id.matches(Regex("[A-Za-z0-9_]+")));val base=if(kind=="block")"blocked" else "muted";"$own/$base/users/confirm_$kind?${base}_id=${Archive.encode(id)}"}
            "unblock","unmute"->{require(id.matches(Regex("[0-9]+")));"$own/${if(kind=="unblock")"blocked" else "muted"}/users/$id/confirm_$kind"}
            else->error("Unknown account form.")
        }
    }
    fun allowed(kind:String):Set<String> = when(kind) {
        "profile"->setOf("profile[title]","profile[about_me]")
        "preferences"->setOf("minimize_search_engines","disable_share_links","allow_cocreator","adult","view_full_works","hide_warnings","hide_freeform","disable_work_skins","skin_id","time_zone","preferred_locale","work_title_format","comment_emails_off","comment_inbox_off","comment_copy_to_self_off","kudos_emails_off","guest_replies_off","allow_collection_invitation","allow_gifts","collection_emails_off","collection_inbox_off","recipient_emails_off","history_enabled","first_login","banner_seen").map{"preference[$it]"}.toSet()
        "pseud"->setOf("name","description","is_default","delete_icon","icon_alt_text","icon_comment_text").map{"pseud[$it]"}.toSet()
        "bookmark","seriesBookmark","externalBookmark"->setOf("pseud_id","bookmarker_notes","tag_string","collection_names","private","rec").map{"bookmark[$it]"}.toSet()+if(kind=="externalBookmark")setOf("url","author","title","summary","fandom_string","rating_string","category_strings","relationship_string","character_string").map{"external_work[$it]"}+setOf("external_work[category_strings][]") else emptySet()
        else->emptySet()
    }
    private fun Element.trusted()=(listOf(this)+parents()).none{it.hasClass("userstuff") || it.id()=="workskin"}
    private fun query(u:URI)=u.rawQuery.orEmpty().split('&').filter{it.isNotEmpty()}.map{val x=it.split('=',limit=2);URLDecoder.decode(x[0],"UTF-8") to URLDecoder.decode(x.getOrElse(1){""},"UTF-8")}
    private fun action(value:String,kind:String,user:String,id:String):String? {
        val u=runCatching{URI(Archive.BASE).resolve(value)}.getOrNull() ?: return null
        if(ao3Path(u.toString())==null)return null
        val own="/users/$user";val p=u.path
        if(kind in setOf("block","mute")) {
            val base=if(kind=="block")"blocked" else "muted";val q=query(u)
            return if(p=="$own/$base/users" && q.size==1 && q.single().first=="${base}_id" && q.single().second.equals(id,true))u.rawPath+"?"+u.rawQuery else null
        }
        if(u.rawQuery!=null)return null
        val valid=when(kind) {
            "unblock","unmute"->p=="$own/${if(kind=="unblock")"blocked" else "muted"}/users/$id"
            "preferences"->p.matches(Regex(Regex.escape(own)+"/preferences(?:/[0-9]+)?"))
            "profile"->p=="$own/profile"
            "pseud"->p==if(id.isEmpty())"$own/pseuds" else "$own/pseuds/$id"
            "bookmark","deleteBookmark"->p=="/bookmarks/$id"
            "seriesBookmark"->p=="/series/$id/bookmarks"
            "externalBookmark"->p=="/bookmarks"
            else->false
        }
        return u.rawPath.takeIf{valid}
    }
    fun parse(html:String,kind:String,user:String,id:String=""):AccountForm {
        val source=source(kind,user,id)
        require(html.length<=8*1024*1024 && ArchiveParser.sessionIdentity(html)==user){"AO3 could not verify this account form. Check sign-in and reload."}
        val main=Jsoup.parse(html).select("#main").firstOrNull{it.trusted()} ?: error("AO3 did not return this account form.")
        val destructive=kind in setOf("block","unblock","mute","unmute","deleteBookmark")
        for(form in main.select("form").filter{it.trusted() && it.attr("method").equals("post",true)}) {
            val path=action(form.attr("action"),kind,user,id) ?: continue
            val whitelist=allowed(kind)
            val hidden=form.select("input[type=hidden][name]:not([disabled])").filter{it.trusted() && it.attr("name") in whitelist+setOf("authenticity_token","_method")}.groupBy({it.attr("name")},{it.attr("value")})
            if(hidden["authenticity_token"]?.size!=1 || (hidden["_method"]?.size ?: 0)>1)continue
            val method=hidden["_method"]?.firstOrNull().orEmpty()
            val creating=kind in setOf("seriesBookmark","externalBookmark","block","mute") || kind=="pseud" && id.isEmpty()
            if((if(creating)method.isNotEmpty() else if(kind in setOf("unblock","unmute","deleteBookmark"))method!="delete" else method !in setOf("put","patch")) || hidden["authenticity_token"]?.firstOrNull().isNullOrBlank())continue
            val fields=mutableListOf<AccountField>()
            val controls=form.select("input[name]:not([type=hidden]):not([disabled]), textarea[name]:not([disabled]), select[name]:not([disabled])").filter{it.trusted() && it.attr("name") in whitelist}.groupBy{it.attr("name")}
            fun label(el:Element):String = form.select("label[for]").firstOrNull{it.trusted() && it.attr("for")==el.id()}?.text()?.takeIf{it.isNotBlank()}
                ?: el.attr("title").takeIf{it.isNotBlank()} ?: el.attr("name").substringAfter('[').substringBefore(']').replace('_',' ').replaceFirstChar{it.uppercase()}
            for((name,elements) in controls) {
                val el=elements.first();val type=el.attr("type").lowercase();val title=if(name=="external_work[category_strings][]")"Categories" else label(el)
                val field=when {
                    el.tagName()=="select"->{val choices=el.select("option:not([disabled])").map{Choice(it.text(),it.attr("value"))}.distinctBy{it.value};val values=el.select("option[selected]:not([disabled])").map{it.attr("value")}.distinct().let{if(el.hasAttr("multiple"))it else it.takeLast(1)}.ifEmpty{if(!el.hasAttr("multiple"))choices.take(1).map{it.value} else emptyList()};AccountField(name,title,if(el.hasAttr("multiple"))"multi" else "choice",values,choices)}
                    el.tagName()=="textarea"->AccountField(name,title,"area",listOf(el.`val`()))
                    type=="checkbox"->{val choices=elements.map{Choice(label(it),it.attr("value").ifEmpty{"1"})};val values=elements.filter{it.hasAttr("checked")}.map{it.attr("value").ifEmpty{"1"}};val off=hidden[name]?.firstOrNull() ?: "0";AccountField(name,title,if(name.endsWith("[]"))"multi" else "check",if(values.isEmpty() && !name.endsWith("[]"))listOf(off) else values,choices,off)}
                    type in setOf("","text","url","email","number")->AccountField(name,title,"text",listOf(el.`val`()))
                    else->null
                }
                if(field!=null)fields+=field
            }
            if(fields.isEmpty() && !destructive)continue
            val fixed=hidden.filterKeys{it in setOf("authenticity_token","_method") || it !in controls}.toMutableMap()
            return AccountForm(kind,id,user,source,path,method,main.select("h2.heading").firstOrNull{it.trusted()}?.text().orEmpty(),if(destructive)main.select(".caution.notice").firstOrNull{it.trusted()}?.text().orEmpty() else if(kind=="profile")"Your profile is public." else "",fields,fixed,destructive)
        }
        error("AO3 did not offer an editable form for this action.")
    }
    fun submission(original:AccountForm,fresh:AccountForm,draft:Map<String,List<String>>):Map<String,List<String>> {
        require(original.user==fresh.user && original.kind==fresh.kind && original.path==fresh.path && original.method==fresh.method){"This form changed. Reload it before saving."}
        require(original.fields.map{it.name}.toSet()==fresh.fields.map{it.name}.toSet() && original.fixed.filterKeys{it!="authenticity_token"}==fresh.fixed.filterKeys{it!="authenticity_token"}){"The available fields changed. Reload this form."}
        val values=(fresh.fixed+fresh.fields.associate{it.name to it.values}).toMutableMap()
        require(draft.values.sumOf{it.sumOf{v->v.length}}<=1024*1024){"Account form is too large."}
        for((name,value) in draft) {
            val old=original.fields.firstOrNull{it.name==name} ?: error("A field is no longer editable. Reload the form.")
            val current=fresh.fields.firstOrNull{it.name==name} ?: error("A field is no longer editable. Reload the form.")
            require(old.type==current.type){"A field is no longer editable. Reload the form."}
            if(value==old.values)continue
            require(current.values==old.values){"This field changed on AO3. Reload before saving your edit."}
            if(current.type in setOf("choice","multi","check"))require(value.all{v->current.choices.any{it.value==v} || current.type=="check" && v==current.offValue}){"An option is no longer available. Reload the form."}
            require(current.type=="multi" || value.size==1){"Invalid field value."}
            values[name]=value
        }
        return values
    }
}
