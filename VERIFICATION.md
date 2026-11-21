# Verification

2026-11-21, version 3.1.0.

- Desktop: Arch Linux, Qt 6.12.0, glibc 2.44. All three CTest suites passed.
- Android release: 47 fixture/unit tests passed; one live test skipped in the standard suite. Lint: zero errors, 23 warnings.
- The separate live test passed: two searches returned distinct result sets and a work opened with readable chapter text.
- APK/AAB signatures verified; the APK matches the 3.0.1 signing certificate. Version code 15; 16 KB alignment passed.
- Desktop 3.1.0 installed under `~/Applications/ao3-client`. Startup and the existing launcher passed checks outside the source folder.
- Notes, emphasis, links, book mode and offline/backup preservation tested. Desktop notes inspected in light and dark themes.

No live comment was posted during testing. Phone rendering, real sign-in/comment submission, translation and long-session testing remain pending. Account actions, images and work skins are incomplete.
