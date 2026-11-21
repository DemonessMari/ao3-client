# 3.0.1 release candidate

- Removed decorative wording and repeated instructions from both apps.
- Shortened documentation and notices.
- Preserve sessions on incomplete verification responses; restore the remembered Android account while checking the session.

Android: install the APK over the production-signed 3.0.0 app. Development-signed installs still require backup and migration before switching keys.

Linux: extract the archive and run `./install.sh`. Requires system Qt; see `BUILD-INFO.txt`.

Phone rendering and long-session testing remain pending.
