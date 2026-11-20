# Release verification

## Automated gates

- Desktop release configuration compiles with one worker; all CTest suites pass.
- Android debug and release JVM tests, lint and APK builds pass. One opt-in live test is skipped unless enabled explicitly.
- Cross-edition backup fixture, round trip, merge protection, malicious IDs, credential exclusion and corrupt-index recovery are tested.
- Desktop reader chapter navigation, translation cancellation/errors, offline use, bookmarks, live appearance layout, book modes and opacity are covered by functional/offscreen tests.
- Android APK signatures, package/version/target, non-debuggable release manifest and checksums are verified. Release key remains outside source control.
- Installed desktop binary runs from outside the source folder with the desktop launcher and all dynamic dependencies present.
- Source excludes generated builds, local properties, keys, credentials and attachments. License, privacy, third-party notices and CI are present.

## Physical device acceptance (requires a connected phone)

No emulator is started automatically because of the earlier host out-of-memory failure. Passing JVM tests and lint does not establish GPU/UI correctness. Until checked, publish as a prerelease.

- Flip both ways in all themes; inspect opacity, fine seams, shading, and cancelled/partial drags.
- Use chapter arrows/menu and page-boundary navigation; verify distinct chapter text.
- Change appearance while reading, including large system fonts and small screens.
- Restore text-only/fullscreen controls; test rotation, Back and screen reader labels.
- Force-stop/restart and kill while backgrounded; verify position recovery.
- Download, enable airplane mode, reopen all chapters and bookmarks.
- Export from development APK, install signed release, import and verify the library.
- Read a very large fic for an extended session; check memory, battery, responsiveness and page layout.

## External service checks

AO3 HTML, login and public translation endpoints may change independently. Fixture tests do not prove availability. Live tests must not post account actions or bypass rate limits. A blocked live request is reported, not converted into an empty result. Public GitHub distribution is separate from store submission; this repository does not claim Play Store approval or exhaustive AO3 native functionality.
