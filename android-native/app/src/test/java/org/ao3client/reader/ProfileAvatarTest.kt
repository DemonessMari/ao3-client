package org.ao3client.reader
import org.junit.Assert.*
import org.junit.Test
import java.net.HttpURLConnection
import java.net.URL
import java.io.ByteArrayInputStream
class ProfileAvatarTest {
    private fun fixture()=javaClass.classLoader!!.getResourceAsStream("profile-avatar.html")!!.bufferedReader().use{it.readText()}
    @Test fun profileHeaderSuppliesTheIconAndBiographyCannotOverrideIt(){
        val p=ArchiveParser.profile(fixture(),"Writer");assertEquals(Archive.BASE+"/rails/active_storage/blobs/redirect/abc/avatar.png",p.avatarUrl);assertEquals("A green & blue icon",p.avatarAlt)
        val html=fixture().replace(Regex("<div class=\"primary header module\">.*?</div>"),"");assertEquals("",ArchiveParser.profile(html,"Writer").avatarUrl)
    }
    @Test fun onlyAo3ImagePathsAndVerifiedStorageRedirectsAreAllowed(){
        assertTrue(avatarUrlAllowed(Archive.BASE+"/system/pseud/icons/123/original.jpg"));assertFalse(avatarUrlAllowed("https://example.com/avatar.png"));assertFalse(avatarUrlAllowed(Archive.BASE+"/works/1"));assertFalse(avatarUrlAllowed("http://archiveofourown.org/system/pseud/icons/1.png"));assertFalse(avatarUrlAllowed("https://archiveofourown.org:444/system/pseud/icons/1.png"));assertFalse(avatarUrlAllowed("https://user@archiveofourown.org/system/pseud/icons/1.png"));assertFalse(avatarUrlAllowed("https://bucket.s3.amazonaws.com/a"));assertTrue(avatarUrlAllowed("https://bucket.s3.amazonaws.com/a",true));assertFalse(avatarUrlAllowed("https://bucket.s3.amazonaws.com.evil/a",true))
    }
    private class Reply(url:URL,val code:Int,val bytes:ByteArray=byteArrayOf(1),val location:String?=null):HttpURLConnection(url){
        var closed=false
        override fun connect(){};override fun usingProxy()=false;override fun disconnect(){closed=true};override fun getResponseCode()=code;override fun getContentLengthLong()=bytes.size.toLong();override fun getInputStream()=ByteArrayInputStream(bytes);override fun getHeaderField(name:String)=if(name=="Location")location else null
    }
    @Test fun redirectsNeverForwardAo3CookiesToStorage(){
        val replies=mutableListOf<Reply>();val data=byteArrayOf(1,2,3)
        assertArrayEquals(data,loadAvatarBytes(Archive.BASE+"/rails/active_storage/blobs/redirect/abc/a.png","session=value","Agent"){url->Reply(url,if(replies.isEmpty())302 else 200,data,if(replies.isEmpty())"https://bucket.s3.amazonaws.com/a.png" else null).also{replies+=it}})
        assertEquals("session=value",replies[0].getRequestProperty("Cookie"));assertNull(replies[1].getRequestProperty("Cookie"));assertTrue(replies.all{it.closed})
    }
    @Test fun unexpectedRedirectsAndOversizedPicturesFailWithoutContinuing(){
        var calls=0
        assertTrue(runCatching{loadAvatarBytes(Archive.BASE+"/system/pseud/icons/a.png",null,"Agent"){url->calls++;Reply(url,302,location="https://example.com/a.png")}}.isFailure);assertEquals(1,calls)
        assertTrue(runCatching{loadAvatarBytes(Archive.BASE+"/system/pseud/icons/a.png",null,"Agent"){url->Reply(url,200,ByteArray(2*1024*1024+1))}}.isFailure)
    }
}
