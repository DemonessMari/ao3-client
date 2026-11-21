# Verification

2026-11-21, version 3.0.0.

- Desktop: Arch Linux, Qt 6.12.0, glibc 2.44. All 3 CTest suites passed.
- Android: JDK 17, Gradle 8.11.1, AGP 8.10.1, API 36. 33 JVM tests passed; live test skipped. Lint: 0 errors, 22 warnings.
- Release APK/AAB signatures, package metadata and 16 KB alignment checked.
- Desktop installation and launcher tested outside the source folder, including paths with spaces and special characters.
- Coverage includes parsing, filters, navigation, appearance, offline data, backups, progress, cancellation and corruption recovery. Desktop curl frames were inspected offscreen.

The separate live AO3 test timed out. Search, sign-in and translation need live verification. No physical Android device was connected for rendering, accessibility, process-death, migration or long-session checks.

See [RELEASE-CHECKLIST.md](RELEASE-CHECKLIST.md).
