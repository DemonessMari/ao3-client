package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
class SubscriptionsTest {
 private fun fixture(name:String)=javaClass.getResource("/$name.html")!!.readText()
 @Test fun subscriptionsAreScopedToWorkAndAccount() {
  val fresh=fixture("subscription-new");val existing=fixture("subscription-existing")
  val create=Subscriptions.form(fresh,"123","Alice")!!;val remove=Subscriptions.form(existing,"123","Alice")!!
  assertFalse(create.subscribed);assertEquals("/users/Alice/subscriptions",create.path);assertEquals(3,create.fields.size);assertEquals("csrf+&=token",create.fields["authenticity_token"])
  assertTrue(remove.subscribed);assertEquals("/users/Alice/subscriptions/456",remove.path);assertEquals("delete",remove.fields["_method"])
  assertNull(Subscriptions.form(fresh,"999","Alice"));assertNull(Subscriptions.form(existing,"123","Bob"))
 }
 @Test fun malformedOrStoryFormsCannotChangeSubscriptions() {
  val fresh=fixture("subscription-new");val existing=fixture("subscription-existing")
  for(bad in listOf(fresh.replace("/users/Alice/subscriptions","https://evil.example/users/Alice/subscriptions"),fresh.replace("value=\"Work\"","value=\"User\""),existing.replace("value=\"delete\"","value=\"patch\""),fresh.replace("authenticity_token","missing"),"<div id='workskin'>$fresh</div>","<div class='userstuff'>$existing</div>",existing.replace("/subscriptions/456","/subscriptions/delete_all")))assertNull(Subscriptions.form(bad,"123","Alice"))
 }
}
