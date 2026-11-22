package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
class ReadingActionsTest {
 @Test fun acceptsOnlyAccountNavigationFormsForThisWork() {
  val html=javaClass.getResource("/reading-action.html")!!.readText()
  val form=ReadingActions.form(html,"123")!!
  assertEquals("mark_for_later",form.action);assertEquals(2,form.fields.size);assertEquals("csrf+&=token",form.fields["authenticity_token"])
  assertEquals("mark_as_read",ReadingActions.form(html.replace("mark_for_later","mark_as_read"),"123")!!.action)
  assertNull(ReadingActions.form(html,"456"))
  for(bad in listOf(html.replace("/works/123/mark_for_later","https://evil.example/works/123/mark_for_later"),html.replace("value=\"patch\"","value=\"delete\""),html.replace("authenticity_token","missing"),"<div id='workskin'>$html</div>","<div class='userstuff'>$html</div>"))assertNull(ReadingActions.form(bad,"123"))
 }
}
