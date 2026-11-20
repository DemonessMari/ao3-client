# Verification — 3.0.0 release candidate

Verified locally on 2026-11-21. Desktop: Arch Linux x86_64, system Qt 6.12.0, glibc 2.44. Android: JDK 17, Gradle 8.11.1, AGP 8.10.1, compile/target API 36.

## Results

- Desktop Release build: passed. CTest: **3/3 suites passed** (`workcontent`, `functional`, `desktop-launcher`). The functional executable reported 27 passed checks including fixture setup/cleanup.
- Desktop coverage includes real request routes/fixtures, authentication/session persistence, filtering, suggestions, chapter changes, translation cancellation/errors, offline reading, bookmarks, progress, live appearance, text-only/book modes, chapter-boundary turns, pixel opacity checks, backup round trips and corruption recovery.
- The compact reader has an explicit check that the default text area occupies more than 55% of an 850-pixel-high reader. Secondary controls remain in the inline expandable tools panel. Rendered dark-theme and curl frames were inspected locally.
- The desktop launcher was tested through GIO with user-bin absent from PATH, including install paths containing spaces, quotes, dollar signs and percent signs. It resolves the installed binary, not the source folder.
- The installed binary reports 3.0.0 and passed its startup smoke check from outside the project. The Linux archive was separately unpacked, installed into an independent prefix containing spaces, validated and started with a minimal PATH. Dynamic Qt dependencies resolve to system libraries.
- Android release: **33 JVM tests passed; one additional opt-in live test skipped**. These cover archive parsing, pagination, unicode, translation chunking, offline filters, library/bookmarks, cross-edition backups, merge protection, credential exclusion, damaged-index recovery, chapter replacement, in-reader appearance persistence, ViewModel constructor compatibility and offline restart-state recovery.
- Android release lint: **zero errors/fatal issues, 22 warnings** (dependency/tool update notices and KTX style suggestions). These warnings are not suppressed.
- Production APK and AAB built successfully. APK signature and AAB JAR signature verified. The Android signing certificate is self-signed, as expected for directly distributed APKs; private signing material is outside this repository.
- APK package `org.ao3client.reader`, version 3.0.0/code 13, minimum API 28, target API 36. Release is not debuggable. Cloud/device backup exclusions and restricted cleartext network configuration are packaged.
- APK zip alignment passed with 16 KB page alignment. The bundled arm64 graphics-path library has 0x4000 ELF LOAD alignment.
- Source publication excludes generated builds, local SDK properties, signing keys, credentials, attachments and private development logs. Client license, third-party license texts, privacy policy, release notes and CI are included.

## External checks and limits

A separately enabled live AO3 JVM test failed with `SocketTimeoutException` while awaiting HTTP response headers. A subsequent root-page request with curl returned HTTP 200. This does not establish current search/login/translation availability and is not described as a successful end-to-end online check. Default fixture tests skip live network access; they do not fabricate successful searches in the application.

No physical Android device was connected. The latest phone GPU rendering, accessibility, screen-size behavior, actual OS process death, production-signature migration and extended memory/battery acceptance remain unverified on-device. JVM state tests and desktop offscreen rendering do not replace those checks. No emulator was launched because of the earlier host memory failure.

Backups have a 100 MB limit; use compact reading-data backups without downloaded text for larger libraries. Existing downloads/progress take precedence during import. Cross-edition scroll positions are approximate. Browser-only AO3 account actions are not native client features.

This is published as a **prerelease**, not an assertion that every daily-use scenario or device is certified. See RELEASE-CHECKLIST.md for the device acceptance checklist.
