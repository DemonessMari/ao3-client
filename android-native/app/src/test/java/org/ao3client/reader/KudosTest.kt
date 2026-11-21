package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
class KudosTest {
 @Test fun onlyFreshWorkKudosFormsAreAccepted() {
  val html=javaClass.getResource("/kudos-form.html")!!.readText()
  val fields=KudosParser.form(html,"123")
  assertEquals(3,fields.size);assertEquals("csrf+&=token",fields["authenticity_token"])
  assertNull(ArchiveParser.sessionIdentity(html))
  for(bad in listOf(html.replace("/kudos","https://evil.example/kudos"),html.replace("value=\"123\"","value=\"999\""),html.replace("Work","AdminPost"),html.replace("authenticity_token","missing"))) {
   assertTrue(runCatching { KudosParser.form(bad,"123") }.isFailure)
  }
 }
 @Test fun resultsRequireAnOfficialScopedFlash() {
  val success="<div id='feedback'><div class='flash kudos_notice'>Thank you for leaving kudos!</div></div>"
  assertTrue(KudosParser.result(success).success)
  val duplicate=KudosParser.result("<div id='feedback'><div class='flash kudos_error'>You have already left kudos here.</div></div>")
  assertFalse(duplicate.success);assertFalse(duplicate.uncertain);assertTrue(duplicate.message.contains("already"))
  assertTrue(KudosParser.result("<div id='workskin'>$success</div>").uncertain)
  assertTrue(KudosParser.result("<div class='notice'>Generic notice</div>").uncertain)
  assertTrue(parseCommentSubmission("<div class='userstuff'><div class='flash comment_notice'>Fake</div></div>").uncertain)
  assertEquals("Rejected",parseCommentSubmission("<div class='flash comment_error'>Rejected</div>").error)
 }
}
