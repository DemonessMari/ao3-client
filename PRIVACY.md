# Privacy

AO3 Client is unofficial and is not affiliated with AO3 or OTW.

Search, suggestions, profiles, works, comments and sign-in contact `https://archiveofourown.org` directly. Passwords are sent over HTTPS and are not saved. Native session cookies are stored locally; native sign-out removes them. The embedded AO3 website and external browsers have separate sign-in sessions. Use the website’s sign-out or Clear website sign-in to clear its cookies. Use website sign-in in reader explicitly copies AO3 cookies into the native client only after a fresh server identity check. This also saves the website user-agent string to keep the imported session consistent. Native sign-out clears the native copy; website sign-out is separate.

Native profile pictures load from AO3 and may follow AO3 image redirects to Amazon S3. AO3 cookies are not sent to S3. Missing or unavailable images use an initial.

There are no ads, analytics, crash-reporting SDKs or developer-operated servers.

Translation sends chapter text, the selected language and any configured API key to your LibreTranslate provider after consent. Changing providers resets consent. AO3 credentials are excluded. Remote providers require HTTPS; localhost permits HTTP. The provider controls its own retention and privacy practices.

Preferences, imported fonts, translation keys, history, progress, favorites, bookmarks and downloads are stored locally without additional app-level encryption. Android cloud/device backups are disabled. Desktop data uses Qt's per-user directories.

Exported backups contain library data and optional downloaded text, but no credentials. They are not encrypted. Imports retain existing downloads and progress. Corrupt files are preserved where possible; recovery may lose recent changes. Offline copies do not refresh automatically.

Removing a download retains history and bookmarks. Clearing Android storage or uninstalling deletes app data. Desktop uninstall retains reading data. Export before clearing data or changing Android signing keys.

The embedded AO3 view runs the official website, including its JavaScript, forms, skins and media. It keeps website cookies, history and cache in app-private storage. Work images or embedded media may contact their hosting services. File uploads use a system picker; only files you choose are provided to the website. It exposes no app bridge or direct access to local files. External links use the browser’s own session and privacy settings.

Report privacy issues through [private security reports](https://github.com/DemonessMari/ao3-client/security/advisories/new). Do not attach credentials, private works or full backups to public issues.

Read aloud sends chapter text to the installed eSpeak NG process on desktop or to an Android system text-to-speech voice advertised as offline. The client does not send this text to an online speech service. Android voice installation is managed by system settings.
