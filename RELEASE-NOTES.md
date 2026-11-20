AO3 Client 3.0.0 release candidate brings the Linux desktop and native Android readers into feature parity for reading, search, profiles, local libraries and backups.

- Compact desktop reader with expandable tools; native phone layout remains separate from Qt.
- Live appearance controls in the reader, full-window/text-only modes, paged reading, cross-chapter page navigation and revised opaque curl shading.
- Offline originals, favorites, named bookmarks, saved progress and JSON library backup import/export across editions. Compact backups can omit downloaded text; backup limit is 100 MB.
- Android restart recovery and cancellable requests; stricter HTTPS translation settings with an explicit disclosure and no preselected public provider.
- Public author profiles and listings in the client; clearly labelled browser links for comments, kudos, account bookmarks, subscriptions, series, collections, history and preferences. These account actions are not native screens.
- Production-signed Android APK and AAB, Android 9+ support, target API 36, checksums, privacy/licensing documents and automated CI.

**Android installation:** the production signing key differs from older development builds. Existing testers must install the locally supplied 3.0.0 debug migration build over the old debug app, export the library, then uninstall the development build, install the signed release and import the backup. Do not uninstall before exporting. New users can install the release APK directly. The debug migration APK is not a production release asset.

**Linux installation:** extract the archive and run `./install.sh`. It installs under `~/.local` by default, preserving reading data and resolving the launcher to the installed binary. No source-folder path is required. Archives use system Qt; consult their BUILD-INFO.txt for runtime requirements. Source builds are supported on other distributions; Windows/macOS installers are not included.

**Verification limits:** this is a prerelease, not a claim of exhaustive daily-use/device certification. Automated desktop and Android checks are documented in VERIFICATION.md. A separate live AO3 JVM check timed out while waiting for HTTP headers, although a subsequent root-page request returned HTTP 200. Current online availability therefore remains unverified. No physical Android device was connected for the latest GPU rendering, accessibility or long-session acceptance checks.

MIT-licensed client source. Fanworks remain their creators' property. Unofficial; not affiliated with AO3 or OTW. See PRIVACY.md and THIRD-PARTY-NOTICES.md.
