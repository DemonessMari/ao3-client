# Verification

2026-11-23: desktop and Android 3.2.0-rc.14 (Android code 29).

- Desktop: Arch Linux, Qt 6.12.0, glibc 2.44. All five CTest suites pass: content, functional, launcher, WebEngine rendering and installer. The native skin preview was rendered and inspected. Builds use one compilation job.
- Android: 112 tests pass with live networking enabled. Two searches returned distinct sets of 20 works, and a work opened with readable chapter content. The standard offline suite passes 111 tests and skips the live test. Lint reports zero errors and 33 warnings, primarily style and dependency suggestions.
- Release APK and AAB signatures verify. The APK uses the previous release signer, has 16 KB alignment, and contains the canonical privacy and website-session assets. Imported test fonts are excluded.
- A read-only check confirmed the saved desktop native session, a live work bookmark form and native bookmark, subscription and inbox lists. No account writes were performed. The check used a temporary cookie copy and recorded no private list contents.
- Controlled tests cover ordered series lists, owned account forms, fresh tokens, inbox record IDs, account changes, comment edit drafts, permission controls, uncertain writes without retries and rejection of controls embedded in user content. Volume keys can be reversed or returned to system volume independently.
- Native skins accept versions 1 and 2, reject unknown fields and unsafe ranges, enforce color contrast, and preserve imported fonts. Desktop checks verify that applying a scale twice does not compound it and resetting restores button styles and font sizes. Menus retain their platform shape.
- Desktop packaging and installation checks verify the installed binary against the build, resolve libraries and the launcher, and start from outside the source directory with isolated app data. The install lives under `~/Applications/ao3-client`; updating it preserves reading data.

Still pending: physical-phone rendering and installation, accessibility, large fonts, rotation, Back, file pickers, page turns, restart/offline recovery and extended memory/battery checks; production account writes and role-specific workflows. No phone was connected during this pass.

The earlier phone-wide 404 report has not been reproduced or established as resolved. Current desktop authenticated requests and Android-backend public searches/reading succeed; that does not establish the cause or status on the phone.

Full native website parity remains incomplete. Creator/account-management tools, bulk list management, external/series bookmark editing and faithful HTML/media/work skins use the embedded AO3 website. Offline copies preserve text, notes and basic inline formatting. See [feature coverage](FEATURES.md). Keep releases marked prerelease until device acceptance passes.
