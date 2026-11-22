package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
class ArchiveBookmarksTest {
 private fun fixture()=javaClass.getResource("/archive-bookmark.html")!!.readText()
 @Test fun preservesNotesAndAllBookmarkOptions() {
  val form=ArchiveBookmarks.form(fixture(),"123")
  assertEquals("",form.id);assertEquals("10",form.pseud);assertEquals(2,form.pseuds.size)
  assertEquals("<p>First & second</p>\nNext line",form.notes);assertEquals("Favorite, To read",form.tags);assertEquals("ReadingList",form.collections)
  assertTrue(form.privateBookmark);assertFalse(form.recommendation)
  val changed=form.copy(notes="New notes",privateBookmark=false,recommendation=true)
  val fields=ArchiveBookmarks.fields(form,changed)
  assertEquals("fresh&+token",fields["authenticity_token"]);assertEquals("New notes",fields["bookmark[bookmarker_notes]"])
  assertEquals("0",fields["bookmark[private]"]);assertEquals("1",fields["bookmark[rec]"]);assertFalse(fields.containsKey("unwanted"));assertFalse(fields.containsKey("_method"))
  val edited=ArchiveBookmarks.form(fixture().replace("/works/123/bookmarks","/bookmarks/456").replace("<select name=","<input type='hidden' name='_method' value='put'><select name="),"123")
  assertEquals("456",edited.id);assertEquals("put",ArchiveBookmarks.fields(edited,edited)["_method"])
 }
 @Test fun savedRecordMustMatchTheWorkAndOwnedBookmark() {
  val record="<li class='bookmark' id='bookmark_456'><h4><a href='/works/123'>Work</a></h4><a href='/bookmarks/456/edit'>Edit</a></li>"
  assertTrue(ArchiveBookmarks.record(record,"123"));assertTrue(ArchiveBookmarks.record(record,"123","456"))
  assertFalse(ArchiveBookmarks.record(record,"999"));assertFalse(ArchiveBookmarks.record(record,"123","789"))
  assertFalse(ArchiveBookmarks.record("<div class='userstuff'>$record</div>","123"))
  assertFalse(ArchiveBookmarks.record(record.replace("/bookmarks/456/edit","https://evil.example/bookmarks/456/edit"),"123"))
 }
 @Test fun rejectsUnsafeFormsAndUnconfirmedResults() {
  val html=fixture()
  for(bad in listOf(html.replace("/works/123/bookmarks","https://evil.example/works/123/bookmarks"),html.replace("authenticity_token","missing"),"<div id='workskin'>$html</div>",html.replace("/works/123/bookmarks","/works/999/bookmarks")))assertTrue(runCatching {ArchiveBookmarks.form(bad,"123")}.isFailure)
  assertTrue(ArchiveBookmarks.result("<div class='flash notice'>Bookmark created</div>").success)
  assertFalse(ArchiveBookmarks.result("<div class='errorExplanation'>Invalid tags</div>").success)
  assertTrue(ArchiveBookmarks.result("<div id='workskin'><div class='flash notice'>Fake</div></div>").uncertain)
  val delete="<form class='simple destroy' method='post' action='/bookmarks/456'><input type='hidden' name='_method' value='delete'><input type='hidden' name='authenticity_token' value='fresh'></form>"
  assertEquals("delete",ArchiveBookmarks.deleteForm(delete,"456")["_method"])
  assertTrue(runCatching {ArchiveBookmarks.deleteForm(delete,"999")}.isFailure)
 }
}
