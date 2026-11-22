package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
class ArchiveWebsiteTest {
 @Test fun onlyOfficialHttpsAddressesStayInsideTheApp() {
  for(url in listOf("https://archiveofourown.org/","https://archiveofourown.org:443/users/Alice/preferences","https://archiveofourown.org/works/123?view_adult=true#comments"))assertTrue(archiveWebsiteUrl(url))
  for(url in listOf("http://archiveofourown.org/","https://archiveofourown.org.evil.example/","https://other.example/","file:///etc/passwd","javascript:alert(1)","https://name:secret@archiveofourown.org/","https://archiveofourown.org:444/","not a url"))assertFalse(archiveWebsiteUrl(url))
 }
}
