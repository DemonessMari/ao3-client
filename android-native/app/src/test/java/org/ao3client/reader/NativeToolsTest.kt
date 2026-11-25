package org.ao3client.reader

import org.junit.Assert.*
import org.junit.Test

class NativeToolsTest {
    private fun fixture(name:String)=javaClass.classLoader!!.getResource("native-tools-$name.html")!!.readText()
    @Test fun workPostingUsesTheChosenButtonAndNeverParsesStoryForms() {
        val html=fixture("work");val page=NativeTools.parse(html,"/works/new","Writer");assertEquals(1,page.forms.size)
        val form=page.forms.single();assertEquals(3,form.submits.size);assertEquals(5,form.fields.size)
        assertTrue(form.fields[1].values.single().contains("<form action='/works/666'"));assertEquals(listOf("Drafts"),page.links.map{it.first})
        val fresh=NativeTools.parse(html.replace("work+token","new+token"),"/works/new","Writer")
        val fields=NativeTools.prepare(form,fresh,mapOf("work[title]" to listOf("New title"),"work[archive_warning_ids][]" to listOf("14","17")),form.submits.first())
        assertTrue("save_button" to "Save As Draft" in fields);assertFalse(fields.any{it.first=="post_button"});assertTrue("authenticity_token" to "new+token" in fields)
        assertEquals(listOf("14","17"),fields.filter{it.first=="work[archive_warning_ids][]"}.map{it.second})
        assertTrue(runCatching{NativeTools.prepare(form,NativeTools.parse(html.replace("Original","Changed elsewhere"),"/works/new","Writer"),emptyMap(),form.submits.first())}.isFailure)
        assertTrue(runCatching{NativeTools.prepare(form,fresh,mapOf("work[rating_string]" to listOf("bad")),form.submits.last())}.isFailure)
    }
    @Test fun previewKeepsUnsavedHiddenTextAndRechecksTheEditableWork() {
        val html=fixture("work").replace("action=\"/works\"","action=\"/works/42\"").replace("<fieldset>","<input name='_method' type='hidden' value='patch'><fieldset>")
        val editor=NativeTools.parse(html,"/works/42/edit","Writer").forms.single()
        val preview=NativeTools.continued(NativeTools.parse(fixture("preview"),"/works/42","Writer"),editor,"/works/42")
        assertEquals(1,preview.forms.size);assertTrue(preview.text.contains("Preview text"));assertTrue(preview.previewHtml.contains("Preview text"));assertFalse(preview.previewHtml.contains("<form"));assertEquals("/works/42/edit",preview.forms.single().source)
        val fields=NativeTools.prepare(preview.forms.single(),NativeTools.parse(html.replace("work+token","new+token"),"/works/42/edit","Writer"),emptyMap(),preview.forms.single().submits.last())
        assertTrue("work[title]" to "New title" in fields);assertTrue("work[chapter_attributes][content]" to "<p>Changed story</p>" in fields);assertTrue("authenticity_token" to "new+token" in fields)
    }
    @Test fun bulkEditorTransitionsRecheckSelectionPageAndUseGlobalCsrf() {
        val html=fixture("bulk");val original=NativeTools.parse(html,"/users/Writer/works/show_multiple","Writer").forms.single()
        val next=html.replace("/works/edit_multiple","/works/update_multiple").replace("name=\"work_ids[]\" type=\"checkbox\"","name=\"work_ids[]\" type=\"hidden\"").replace("value=\"Edit\"","value=\"Update\"")
        val page=NativeTools.continued(NativeTools.parse(next,"/users/Writer/works/edit_multiple","Writer"),original,original.action)
        val form=page.forms.single();assertEquals("/users/Writer/works/show_multiple",form.source)
        val fields=NativeTools.prepare(form,NativeTools.parse(html.replace("global+token","fresh+global"),original.source,"Writer"),emptyMap(),form.submits.single())
        assertTrue("authenticity_token" to "fresh+global" in fields);assertEquals(listOf("42","43"),fields.filter{it.first=="work_ids[]"}.map{it.second})
        assertTrue(runCatching{NativeTools.prepare(form,NativeTools.parse(html.replace("value=\"43\"","value=\"99\""),original.source,"Writer"),emptyMap(),form.submits.single())}.isFailure)
    }
    @Test fun passwordFieldsAreBlankAndExcludedFromCheckpoints() {
        val page=NativeTools.parse(fixture("account"),"/users/Writer/change_password","Writer")
        assertEquals(3,page.forms.size);val form=page.forms.first();assertEquals("password",form.fields.first().type);assertEquals(listOf(""),form.fields.first().values);assertFalse(form.checkpoint.containsKey("password_check"))
        assertEquals("delete",page.forms.last().fixed["_method"]?.single());assertTrue(page.forms.last().action.endsWith("/Other"));assertFalse(page.forms.any{it.action=="/users/Writer"})
    }
    @Test fun challengePromptCountsAndNestedFieldsNeedNoScript() {
        val page=NativeTools.parse(fixture("challenge"),"/collections/Event/signups/new","Writer")
        assertEquals(2,page.forms.size);assertEquals("get",page.forms.first().method);assertTrue(page.forms.last().fields[1].name.contains("[requests_attributes][0]"));assertEquals("choice",page.forms.last().fields.last().type)
        assertEquals(1,NativeTools.parse(fixture("challenge"),"/collections/Event/signups/new","").forms.size)
        assertTrue(NativeTools.fieldAllowed("work[chapter_attributes][published_at(1i)]"))
    }
    @Test fun routesAndPermissionsRejectForeignOrOtherAccountPages() {
        listOf("https://archiveofourown.org@evil.example/works/new","http://archiveofourown.org/works/new","/users/Other/change_password","/admin/users","/works/../admin/users").forEach{assertNull(it,NativeTools.route(it,"Writer"))}
        assertTrue(runCatching{NativeTools.parse(fixture("account"),"/users/Writer/change_password","Other")}.isFailure)
        assertFalse(NativeTools.fieldAllowed("user[admin]"));assertFalse(NativeTools.fieldAllowed("pseud[ticket_number]"))
        val html=fixture("work").replace("action=\"/works\"","action=\"https://evil.example/works\"");assertTrue(NativeTools.parse(html,"/works/new","Writer").forms.isEmpty())
    }
    @Test fun uploadsAreBoundedAndMultipartKeepsRepeatedFieldsAndBinaryData() {
        val bytes=byteArrayOf(-119,80,78,71,13,10,26,10,0,10,-1)
        val file=NativeTools.upload("pseud[icon]","bad\r\nname.png",bytes);assertFalse(file.filename.contains('\r'));assertEquals("image/png",file.mime)
        assertTrue(runCatching{NativeTools.upload("pseud[icon]","file.exe","MZbad".toByteArray())}.isFailure)
        val large=ByteArray(2*1024*1024+50);assertEquals(2*1024*1024+1,NativeTools.readImage(large.inputStream()).size)
        val body=NativeTools.multipart(listOf("work[archive_warning_ids][]" to "14","work[archive_warning_ids][]" to "17"),listOf(file),"AO3-boundary-test")
        val text=body.toString(Charsets.ISO_8859_1);assertEquals(2,Regex(Regex.escape("name=\"work[archive_warning_ids][]\"")).findAll(text).count());assertTrue(text.contains("Content-Type: image/png"));assertTrue(text.endsWith("--AO3-boundary-test--\r\n"))
    }
    @Test fun atomFeedsUseNativeWorkLinksAndNeverExecuteFeedMarkup() {
        val xml=javaClass.classLoader!!.getResource("native-tools-feed.atom")!!.readText()
        val page=NativeTools.parse(xml,"/tags/Friends/feed.atom","Writer");assertTrue(page.feed);assertEquals("Tag feed",page.title);assertEquals(listOf("Work one" to "/works/42"),page.links);assertTrue(page.text.contains("Summary & notes"));assertTrue(page.forms.isEmpty())
        assertTrue(runCatching{NativeTools.parse("<feed xmlns='https://evil.example'><title>Bad</title></feed>","/tags/Friends/feed.atom","")}.isFailure)
    }
    @Test fun nestedSectionsAreDecodedAsDataAndCanBeRemovedWithoutScripts() {
        val html=fixture("sections");val original=NativeTools.parse(html,"/skins/7/edit","Writer").forms.single()
        assertEquals(1,original.sections.size);val added=NativeTools.addSection(original,original.sections.single().key,"Writer");assertEquals(3,added.fields.size);assertEquals(1,added.addedSections);assertEquals(original.checkpoint,added.checkpoint)
        val child=added.fields.single{it.name.endsWith("[parent_skin_title]")};val remove=added.fields.single{it.name.endsWith("[_destroy]")};assertEquals("check",remove.type)
        val fields=NativeTools.prepare(added,NativeTools.parse(html.replace("skin+token","fresh+token"),"/skins/7/edit","Writer"),mapOf(child.name to listOf("Parent"),remove.name to listOf("1")),added.submits.single())
        assertTrue(child.name to "Parent" in fields);assertTrue(remove.name to "1" in fields);assertTrue("authenticity_token" to "fresh+token" in fields)
        val bad=html.replace("; return false;","; execute_unknown(); return false;");assertTrue(NativeTools.parse(bad,"/skins/7/edit","Writer").forms.single().sections.isEmpty())
    }
    @Test fun hiddenPasswordsNeverBecomeStoredFixedValuesOrGetParameters() {
        val html=fixture("account").replace("type=\"password\" name=\"password_check\"","type=\"hidden\" name=\"password_check\"")
        val form=NativeTools.parse(html,"/users/Writer/change_password","Writer").forms.first();assertFalse(form.fixed.containsKey("password_check"));assertFalse(form.checkpoint.containsKey("password_check"));assertEquals(listOf(""),form.fields.first().values)
        val get=html.replace("action=\"/users/Writer/changed_password\" method=\"post\"","action=\"/users/Writer/changed_password\" method=\"get\"");assertTrue(NativeTools.parse(get,"/users/Writer/change_password","Writer").forms.none{it.action.endsWith("changed_password")})
    }
    @Test fun blankOptionsAndReadOnlyValuesArePreserved() {
        val html=fixture("work").replace("<option value=\"General Audiences\" selected>","<option value='' selected>Choose</option><option value=\"General Audiences\">").replace("name=\"work[title]\"","readonly name=\"work[title]\"")
        val form=NativeTools.parse(html,"/works/new","Writer").forms.single();assertEquals(listOf(""),form.fields.single{it.name=="work[rating_string]"}.values);assertEquals(listOf("Original"),form.fixed["work[title]"]);assertTrue(form.fields.none{it.name=="work[title]"})
    }
}
