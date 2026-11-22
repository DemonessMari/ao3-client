package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
class WebsiteSessionTest {
 private fun page(user:String)="<div id='greeting'><a href='/users/$user'>$user</a><a href='/users/logout'>Log out</a></div>"
 @Test fun incomingCookiesAreScopedAndHeaderValuesValidated() {
  val cookies=websiteSessionCookies("_otwarchive_session=web-token; remember_user_token=remember-token")
  assertEquals(2,cookies.size);assertTrue(cookies.all {it.domain=="archiveofourown.org" && it.path=="/" && it.secure && it.isHttpOnly})
  assertTrue(validWebsiteSessionIdentity("Alice","Browser/1"));assertFalse(validWebsiteSessionIdentity("Alice","Browser\r\nInjected: value"))
  assertTrue(runCatching {websiteSessionCookies("session=value\r\nOther: value")}.isFailure)
  assertNull(ArchiveParser.username("<div id='greeting'><a href='/users/login'>Log in</a></div><div class='userstuff'><a href='/users/logout'>Fake</a></div>"))
 }
 @Test fun failedProbePreservesExistingNativeAccount() {
  val prefs=MemoryPreferences();prefs.edit().putString("sessionUsername","Bob").putString("cookies","[]").putString("networkUserAgent","OldAgent").apply()
  val client=Archive(prefs) {_,_->page("Bob")}
  assertTrue(runCatching {client.useWebsiteSession("_otwarchive_session=web-token","WebAgent","Alice")}.isFailure)
  assertEquals("Bob",prefs.getString("sessionUsername",null));assertEquals("[]",prefs.getString("cookies",null));assertEquals("OldAgent",prefs.getString("networkUserAgent",null))
 }
 @Test fun onlyVerifiedSessionsReplaceNativeCookies() {
  val prefs=MemoryPreferences();val client=Archive(prefs) {cookies,agent->assertEquals("WebAgent",agent);assertEquals("web-token",cookies.cookieStore.cookies.first().value);page("Alice")}
  assertEquals("Alice",client.useWebsiteSession("_otwarchive_session=web-token","WebAgent","Alice"))
  assertEquals("Alice",client.rememberedUsername());assertEquals("WebAgent",prefs.getString("networkUserAgent",null));assertTrue(prefs.getString("cookies","")!!.contains("web-token"))
 }
 @Test fun closingTheClientCancelsProbeWithoutSigningOutTheSavedAccount() {
  val prefs=MemoryPreferences();prefs.edit().putString("sessionUsername","Bob").putString("cookies","[]").apply()
  lateinit var client:Archive
  client=Archive(prefs) {_,_->client.cancelRequests();page("Alice")}
  assertTrue(runCatching {client.useWebsiteSession("_otwarchive_session=web-token","WebAgent","Alice")}.isFailure)
  assertEquals("Bob",prefs.getString("sessionUsername",null));assertEquals("[]",prefs.getString("cookies",null))
 }
 @Test fun logoutDuringProbeCannotRestoreTheOldAccount() {
  val prefs=MemoryPreferences();lateinit var client:Archive
  client=Archive(prefs) {_,_->client.logout();page("Alice")}
  assertTrue(runCatching {client.useWebsiteSession("_otwarchive_session=web-token","WebAgent","Alice")}.isFailure)
  assertNull(client.rememberedUsername());assertNull(prefs.getString("cookies",null));assertNull(prefs.getString("networkUserAgent",null))
 }
}
