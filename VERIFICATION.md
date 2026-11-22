# Verification

2026-11-22, version 3.2.0-rc.3.

- Desktop: Arch Linux, Qt 6.12.0, glibc 2.44. All five CTest suites passed: content, functional, launcher, WebEngine rendering and installer.
- Android release: 71 unit/fixture tests passed; one live test skipped in the standard suite. Lint: zero errors, 25 warnings (including style suggestions).
- The embedded desktop renderer displayed the real AO3 sign-in form. No credentials were entered and no account action was submitted.
- The separate live Android test passed: two searches returned distinct result sets and a work opened with readable chapter text.
- Header layout checks passed at 900, 1100 and 1440 pixels. Profile-picture tests cover header scoping, bounded bitmap decoding, native display, missing-image fallback, redirect restrictions and exclusion of AO3 cookies from storage requests.
- The public live profile check returned HTTP 429; live picture retrieval remains unverified. Android picture rendering still needs device acceptance.
- Skin tests cover the shared desktop/Android file format, malformed/low-contrast colors, persistence and reader return navigation. Desktop light/dark Home rendering was inspected; filter tests open the panel explicitly and verify reader restoration. Recent reads reopen independently of Library filters.
- Session tests cover current AO3 greeting markup, logout links with query parameters, identity mismatch, cancellation and preservation of the previous account on a failed website-session import.
- Bookmark and comment fixtures cover form scoping, nested threads, fresh identity/token checks and uncertain outcomes. These do not establish live account acceptance.

- Signed Android APK and AAB builds passed. APK certificate matches 3.1.0; version code 18 and 16 KB alignment verified. Shared identity script and privacy assets match source.
- Desktop 3.2.0-rc.3 installed under `~/Applications/ao3-client`. The existing launcher resolves correctly, required libraries are present, and startup passed from outside the source folder. The executable has no embedded source-folder path. The previous installation was preserved in Downloads.
- Final APK, AAB, desktop archive and checksums are in `~/Downloads`.

Official-source review used `otwcode/otwarchive` revision `452391b648f2fb06b55c36133812449747634504`; production availability may differ.

No live comment or other account write was submitted during testing. Authenticated account operations, phone rendering/accessibility, translation and long-session testing remain pending. See [feature coverage](FEATURES.md) for native gaps and website-only tools. This is a test candidate, not a full-parity release.
