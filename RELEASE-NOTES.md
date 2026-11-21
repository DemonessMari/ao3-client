# 3.0.0 release candidate

- Linux and Android: search, profiles, offline library, bookmarks and backup import/export.
- In-reader appearance controls, text-only mode, paged reading and chapter navigation.
- Updated page-curl rendering and Android progress recovery.
- Optional LibreTranslate support; chapter text is sent only with consent.

Account actions such as comments, kudos and subscriptions open in a browser.

Linux: extract the archive and run `./install.sh`. System Qt is required; see `BUILD-INFO.txt`.

Android: install the APK. Older development builds use a different signing key. Install the local debug migration build and export your library before uninstalling, then install the release and import.

Automated checks passed. Live AO3 verification timed out; phone rendering and long-session tests remain pending. See [VERIFICATION.md](VERIFICATION.md).
