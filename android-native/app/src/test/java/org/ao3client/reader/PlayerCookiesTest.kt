package org.ao3client.reader

import android.webkit.CookieManager
import org.junit.Test
import org.junit.Assert.*
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[34],manifest=Config.NONE)
class PlayerCookiesTest {
    @Test fun closingThePlayerRestoresCookiePolicyWithoutDeletingWebsiteSignIn() {
        val manager=CookieManager.getInstance();manager.setAcceptCookie(true);manager.setCookie("https://archiveofourown.org","session=kept; Path=/; Secure")
        val original=manager.getCookie("https://archiveofourown.org");assertTrue(original.orEmpty().contains("session=kept"))
        val session=PlayerCookies(manager);assertFalse(manager.acceptCookie());session.close();assertTrue(manager.acceptCookie());assertEquals(original,manager.getCookie("https://archiveofourown.org"));session.close();assertTrue(manager.acceptCookie())
        manager.setAcceptCookie(false);PlayerCookies(manager).close();assertFalse(manager.acceptCookie());manager.setAcceptCookie(true)
    }
}
