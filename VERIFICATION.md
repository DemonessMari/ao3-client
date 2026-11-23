# Verification

2026-11-23: desktop and Android 3.2.0-rc.12.

- Desktop: Arch Linux, Qt 6.12.0, glibc 2.44. All five CTest suites passed: content, functional, launcher, WebEngine rendering and installer.
- Android branding now uses the full site name and logo on main screens, with wrapping text and safe-area insets. Profile/work titles and navigation are retained. Phone rendering and large-system-font checks remain pending.
- Android release: 96 unit/fixture tests passed; one live test skipped in the standard suite. Lint: zero errors, 32 warnings (including style suggestions).
- The embedded desktop renderer displayed the real AO3 sign-in form. No credentials were entered and no account action was submitted.
- An earlier separate live Android test passed: two searches returned distinct result sets and a work opened with readable chapter text.
- Interface-font checks compare the prior system-font interface with the selected imported font. Desktop checks cover existing navigation/buttons, heading size/weight, work-summary document fonts, opt-out and missing-file fallback; before/after desktop captures were inspected. Android tests cover all 15 typography roles and preference persistence; Mari confirmed imported fonts now apply on the phone in rc7. Other phone appearance/accessibility checks remain pending.
- Font tests cover bounded/validated imports, duplicate detection, private copies, restart recovery, missing-font fallback and removal without changing the chapter. Desktop rendering uses a licensed test font with a distinct family name. Android storage tests use an injected font validator; Mari confirmed imported-font application on the phone; picker cancellation and accessibility still need acceptance. The new desktop font controls were visually inspected.
- New layout tests cover live paragraph reflow, side margins in scrolling and book mode, book-position preservation, system-theme resolution, and Android preference persistence/bounds without leaving the chapter. The full-name desktop header and scrollable reading settings were inspected. Phone rendering of these new controls remains unverified.
- Header layout checks passed at 900, 1100 and 1440 pixels. Profile-picture tests cover header scoping, bounded bitmap decoding, native display, missing-image fallback, redirect restrictions and exclusion of AO3 cookies from storage requests.
- The previous public live profile check returned HTTP 429; live picture retrieval remains unverified. Android picture rendering still needs device acceptance.
- Skin tests cover the shared desktop/Android file format, malformed/low-contrast colors, persistence and reader return navigation. Desktop light/dark Home rendering was inspected; filter tests open the panel explicitly and verify reader restoration. Recent reads reopen independently of Library filters.
- Session tests cover current AO3 greeting markup, logout links with query parameters, identity mismatch, cancellation and preservation of the previous account on a failed website-session import.
- Bookmark and comment fixtures cover form scoping, nested threads, fresh identity/token checks and uncertain outcomes. These do not establish live account acceptance.

- Signed Android APK and AAB builds passed. APK certificate matches 3.1.0; Android rc12 version code 27 and 16 KB alignment verified. Shared identity script and privacy assets match source.
- Desktop 3.2.0-rc.12 installed under `~/Applications/ao3-client`. The existing launcher resolves correctly, required libraries are present, and startup passed from outside the source folder. The executable has no embedded source-folder path. The previous installation was preserved in Downloads.
- Final APK, AAB, desktop archive and checksums are in `~/Downloads`.

Official-source review used `otwcode/otwarchive` revision `452391b648f2fb06b55c36133812449747634504`; production availability may differ.

No live comment or other account write was submitted during testing. Authenticated account operations, phone rendering/accessibility, translation and long-session testing remain pending. See [feature coverage](FEATURES.md) for native gaps and website-only tools. This is a test candidate, not a full-parity release.

The 2026-11-23 public read request for work 65535541 returned HTTP 200 and chapter markup using native-style headers. Search and a browser-style comparison timed out. The user's across-work phone 404 report remains unresolved; this check did not reproduce it or establish a rate limit. Native error messages now distinguish HTTP 404 and 429. “Cancel request” is shown during ordinary loading.

Chapter-filter request tests verify server-side exclusion, query grouping and pagination, preserving AO3’s existing single-chapter inclusion option. Desktop authentication checks cover the account initial and accessible name. Account-avatar popup appearance on the phone remains unverified.

Reader extras: desktop auto-scroll tests verify movement, chapter-end stopping, chapter changes, book-mode exclusion and leaving the reader. Android tests verify bounded speed and persistence of volume navigation/keep-screen-on. Device checks remain pending for scrolling smoothness, volume keys, overlays, backgrounding and wake behavior. Neither edition starts auto-scroll automatically.

Network-path tests exercise the real Android request method with controlled HTTP connections: work IDs, redirects, read/download/comment/bookmark GET routes, selectable 404 messages without query strings or response bodies, and a 429 without automatic retry or identity removal. They do not reproduce or explain the reported phone 404s. Production request headers and authentication are unchanged.

Read-aloud: the installed desktop eSpeak NG generated 77,934 bytes of valid WAV audio without playback. A controlled child-process test verifies pause/resume and stop without playing audio or stopping unrelated processes. Speech input uses stdin and language/rate arguments are bounded. Android chunk tests cover long multilingual text, exact coverage and surrogate boundaries; actual offline voice selection, audio focus, pause/resume and speaker/headphone output remain unverified on the phone. Android requests offline voices and pauses when backgrounded; no background-playback service is provided.

Book-mode tap-zone tests verify reversed left/right navigation, disabled center taps and retaining the desktop F10 escape. Android tests cover zone boundaries and persisted assignments/widths; actual phone gestures remain pending. The native account menu now offers an explicit server sign-in check. A controlled request test verifies a 404 work response can coexist with an authenticated root response. The user's across-work 404 cause remains unresolved.

Android privacy assets are now generated from the canonical shared privacy document during preBuild; artifact verification compares the packaged bytes to source.

Android rc11 fixes signed-out session detection for AO3 login URLs with query parameters and canonical absolute URLs. A public anonymous root request with the native user agent returned HTTP 200 and `/users/login?return_to=...` links, which rc10 did not recognize. A separate default-Python-user-agent request returned HTTP 403; this is not evidence of the phone's failure cause. Identity links inside work content and foreign-origin links are ignored; conflicting login/logout controls cannot confirm sign-out. Unrecognized pages preserve saved native identity and give a specific explanation for recognizable challenge, waiting-room or maintenance pages. Website and native sessions remain separate. The reported phone-wide work 404s remain unresolved.

Series navigation: both editions parse membership labels and previous/next visible-work links from AO3's work metadata. Shared fixtures cover multiple memberships, missing neighbors, story-content spoofing, foreign links, canonical URLs and missing metadata. Desktop functional tests follow a neighbor through the real client request method into native details and ignore a response after leaving the series screen. The desktop series screen was rendered and inspected. Android controlled-network tests verify routes and fresh title/author parsing; a view-model test checks restoration of the original fic, chapter text, reading position and reader return destination. These are controlled tests, not authenticated production or phone-rendering acceptance. Full series lists and series subscriptions remain website workflows. The work 404 report remains unresolved.
