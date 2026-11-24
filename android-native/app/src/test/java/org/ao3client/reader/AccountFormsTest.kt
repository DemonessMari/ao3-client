package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test

class AccountFormsTest {
    @Test fun mutedListingsUseOnlyTrustedAo3Rules() {
        val html=javaClass.classLoader!!.getResource("muted-listings.html")!!.readText()
        val doc=visibleListingDocument(html)
        assertNull(doc.getElementById("work_42"));assertNotNull(doc.getElementById("work_7"))
        assertTrue(doc.text().contains("Explicitly opened story"))
        assertEquals(listOf("7"),ArchiveParser.listing(html,1).works.map{it.id})
        assertEquals(listOf("7"),CommentParser.parse(html).comments.map{it.id})
        assertNotNull(visibleListingDocument(html.replace("display: none","color: red; display: none")).getElementById("work_42"))
        assertNotNull(visibleListingDocument(html.substring(html.indexOf("<body>"))).getElementById("work_42"))
        assertEquals("/users/Writer/blocked/users/9/confirm_unblock",AccountForms.source("unblock","Writer","9"))
        assertEquals("/users/Writer/muted/users/9/confirm_unmute",AccountForms.source("unmute","Writer","9"))
    }
    private fun fixture(name:String)=javaClass.classLoader!!.getResource("account-form-$name.html")!!.readText()
    @Test fun profileEditsPreserveHtmlAndExcludeStaffAndForgedFields() {
        val form=AccountForms.parse(fixture("profile"),"profile","Writer")
        assertEquals(listOf("profile[title]","profile[about_me]"),form.fields.map{it.name})
        assertEquals("Original title",form.fields[0].values.single());assertEquals("<p>Biography & notes</p>\nSecond line",form.fields[1].values.single())
        assertEquals("patch",form.method);assertFalse(form.fixed.containsKey("user[admin]"))
        val raw=fixture("profile").replace("Second line","Second line<input type='hidden' name='authenticity_token' value='forged'>")
        assertEquals(listOf("account+token"),AccountForms.parse(raw,"profile","Writer").fixed["authenticity_token"])
        assertTrue(runCatching{AccountForms.parse(fixture("profile").replace("<input type='hidden' name='authenticity_token'","<input type='hidden' name='authenticity_token' value='duplicate'><input type='hidden' name='authenticity_token'"),"profile","Writer")}.isFailure)
        assertTrue(runCatching{AccountForms.parse(fixture("profile"),"profile","Other")}.isFailure)
        assertTrue(runCatching{AccountForms.parse(fixture("profile").replace("action='/users/Writer/profile'","action='https://evil.example/users/Writer/profile'"),"profile","Writer")}.isFailure)
    }
    @Test fun freshTokensAndUntouchedConcurrentFieldsArePreserved() {
        val original=AccountForms.parse(fixture("profile"),"profile","Writer")
        val fresh=AccountForms.parse(fixture("profile").replace("account+token","fresh+token").replace("Second line","Updated elsewhere"),"profile","Writer")
        val values=AccountForms.submission(original,fresh,original.fields.associate{it.name to it.values}+("profile[title]" to listOf("New title")))
        assertEquals(listOf("fresh+token"),values["authenticity_token"]);assertEquals(listOf("New title"),values["profile[title]"]);assertTrue(values["profile[about_me]"]!!.single().contains("Updated elsewhere"))
        val conflict=AccountForms.parse(fixture("profile").replace("Original title","Other title"),"profile","Writer")
        assertTrue(runCatching{AccountForms.submission(original,conflict,mapOf("profile[title]" to listOf("New title")))}.isFailure)
        assertTrue(runCatching{AccountForms.submission(original,fresh,mapOf("profile[ticket_number]" to listOf("3")))}.isFailure)
    }
    @Test fun preferencesAndPseudsRetainServerOptionsAndDisabledFields() {
        val form=AccountForms.parse(fixture("preferences"),"preferences","Writer")
        assertEquals(listOf("1"),form.fields.single{it.name=="preference[adult]"}.values);assertEquals(listOf("0"),form.fields.single{it.name=="preference[history_enabled]"}.values)
        assertFalse(form.fields.any{it.name=="preference[allow_gifts]"})
        assertTrue(runCatching{AccountForms.submission(form,form,mapOf("preference[skin_id]" to listOf("999")))}.isFailure)
        val pseud=AccountForms.parse(fixture("pseud"),"pseud","Writer","Pen Name")
        assertTrue(pseud.path.endsWith("Pen%20Name"));assertEquals("Pen Name",pseud.fields.single{it.name=="pseud[name]"}.values.single())
        assertEquals("",AccountForms.parse(fixture("pseud-new"),"pseud","Writer").method)
    }
    @Test fun everyBookmarkTypeAndRepeatedCategoriesUseTheCorrectTarget() {
        val edit=AccountForms.parse(fixture("bookmark"),"bookmark","Writer","91")
        assertEquals("/bookmarks/91",edit.path);assertEquals(listOf("33"),edit.fixed["bookmark[pseud_id]"])
        assertEquals("/series/42/bookmarks",AccountForms.parse(fixture("series"),"seriesBookmark","Writer","42").path)
        val external=AccountForms.parse(fixture("external"),"externalBookmark","Writer")
        assertEquals("/bookmarks",external.path);assertEquals(listOf("F/F"),external.fields.single{it.type=="multi"}.values)
        val values=AccountForms.submission(external,external,mapOf("external_work[category_strings][]" to listOf("F/F","M/M")))
        assertEquals(listOf("F/F","M/M"),values["external_work[category_strings][]"])
        val changedPseud=edit.copy(fixed=edit.fixed+("bookmark[pseud_id]" to listOf("99")))
        assertTrue(runCatching{AccountForms.submission(edit,changedPseud,emptyMap())}.isFailure)
        assertEquals("delete",AccountForms.parse(fixture("deleteBookmark"),"deleteBookmark","Writer","91").method)
    }
    @Test fun userControlsVerifyTheTargetAndDeleteMethod() {
        for(kind in listOf("block","mute","unblock","unmute")) {
            val id=if(kind.startsWith("un"))"9" else "Target"
            val form=AccountForms.parse(fixture(kind),kind,"Writer",id)
            assertTrue(form.destructive);assertTrue(form.fields.isEmpty());assertEquals(setOf("authenticity_token")+if(kind.startsWith("un"))setOf("_method") else emptySet(),form.fixed.keys)
            assertTrue(runCatching{AccountForms.parse(fixture(kind),kind,"Other",id)}.isFailure)
        }
        assertTrue(runCatching{AccountForms.parse(fixture("block"),"block","Writer","SomeoneElse")}.isFailure)
        assertTrue(runCatching{AccountForms.parse("<blockquote class='userstuff'>${fixture("block")}</blockquote>","block","Writer","Target")}.isFailure)
    }
    @Test fun directoriesAndOwnedListsIgnoreControlsInUserContent() {
        val media=parseArchivePage(javaClass.classLoader!!.getResource("archive-media.html")!!.readText(),"media","","",1)
        assertEquals(1,media.entries.size);assertEquals("/media/Anime Manga/fandoms",media.entries.single().target)
        val fandoms=parseArchivePage(javaClass.classLoader!!.getResource("archive-fandoms.html")!!.readText(),"fandoms","","Anime Manga",1)
        assertTrue(java.net.URLDecoder.decode(fandoms.entries.single().target,"UTF-8").endsWith("work_search[fandom_names]=A & B"))
        for(kind in listOf("pseuds","blocked","muted")) {
            val html=javaClass.classLoader!!.getResource("archive-$kind.html")!!.readText();val page=parseArchivePage(html,kind,"Writer","",1)
            assertEquals(1,page.entries.size);assertEquals(if(kind=="pseuds")"Pen Name" else "9",page.entries.single().id)
            assertTrue(page.entries.single().actions.isNotEmpty());assertTrue(runCatching{parseArchivePage(html,kind,"Other","",1)}.isFailure)
        }
    }
}
