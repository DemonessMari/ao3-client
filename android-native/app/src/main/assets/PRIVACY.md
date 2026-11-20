# Privacy

AO3 Client is an unofficial, independent reader. It is not affiliated with Archive of Our Own or the Organization for Transformative Works.

## Network requests

Searches, tag suggestions, public profiles, works and sign-in contact `https://archiveofourown.org` directly. Search terms, requested tags and fic IDs are sent as needed. Passwords are sent to AO3 over HTTPS for sign-in and are not saved. AO3 cookies remain locally stored to keep the session. Sign out removes this client's session cookies; it does not sign out your browser.

The app has no advertising, analytics, crash-reporting SDK, or developer-operated server. It does not upload reading history or downloads. Your operating system, AO3, browser and configured translation provider have their own policies.

## Translation

Translation is optional and disabled until you configure a LibreTranslate URL. Pressing Translate sends the current chapter text and selected language to that provider, along with an API key if you supplied one. You must accept the disclosure before first use of a remote service. Changing providers resets this choice. AO3 cookies are not sent to the translation provider. HTTPS is required for remote providers; HTTP is permitted only for a service on localhost. The app does not verify a provider's privacy or retention practices.

Translation settings and optional API keys are stored locally. They are not protected by an additional app-level encryption layer. Use device encryption and protect your user account. Original chapters remain available if translation fails.

## Local data and backups

Both editions store preferences, recent fics, progress, favorites, local bookmarks and explicitly downloaded original text on your device. Android uses app-private storage and disables Android cloud backup. Desktop uses Qt's per-user settings and data directories. The source folder is not used for your reading library.

Exported library JSON contains reading history, fic metadata, favorites, original downloaded text, reading bookmarks and positions. It does not contain passwords, cookies or translation API keys. Backups are not encrypted: keep them private. Imports merge; existing downloads and positions take precedence. Cross-edition positions are approximate because fonts, pagination and scroll units differ. Exact Android offsets are retained when available.

Corrupt library indexes are preserved with a `.corrupt-…` suffix and recovered from the previous index or saved-copy metadata. Corrupt bookmark files are preserved and replaced with an empty bookmark list. This recovery can lose changes made after the last good index; backups remain recommended. Offline copies are snapshots and are not automatically refreshed.

Removing a download deletes that original text copy while retaining history, favorites and bookmarks. Uninstalling Android or clearing app storage deletes app-private data. On desktop, removal of the executable does not delete your reading library. Export a backup before clearing data or switching Android signing keys.

## Browser links

Features labelled “on AO3” or “browser” open the original AO3 site in your browser. The browser has a separate sign-in session. The client does not automatically post comments, kudos, account bookmarks, subscriptions or other account changes.

## Contact

Report privacy problems through [GitHub Security Advisories](https://github.com/DemonessMari/ao3-client/security/advisories/new). Do not attach session cookies, API keys, passwords, private works or full library backups to public issues.
