package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
class OfficialDownloadsTest {
 @Test fun serverErrorPagesCannotBecomeEbooks() {
  OfficialDownloads.validate("%PDF-1.4 test".toByteArray(),"pdf")
  OfficialDownloads.validate(byteArrayOf(80,75,3,4),"epub")
  OfficialDownloads.validate("<html><div id='chapters'>Story</div></html>".toByteArray(),"html")
  for(format in listOf("epub","pdf","mobi","azw3","html")) {
   assertTrue(runCatching { OfficialDownloads.validate("<html>Maintenance</html>".toByteArray(),format) }.isFailure)
  }
 }
 @Test fun availableFormatsAreValidatedAndDeduplicated() {
  val html=javaClass.getResource("/official-downloads.html")!!.readText()
  val links=OfficialDownloads.parse(html,"123")
  assertEquals(listOf("epub","pdf","html","mobi","azw3"),links.map { it.format })
  assertEquals("A Story.epub",links[0].filename)
  assertTrue(links[0].url.contains("&x=1"))
  assertTrue(OfficialDownloads.parse("<p>Downloads disabled</p>","123").isEmpty())
  assertNull(OfficialDownloads.format("https://archiveofourown.org:444/downloads/123/A.pdf","123"))
 }
}
