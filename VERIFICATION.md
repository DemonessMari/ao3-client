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

## Unreleased source after rc.14

2026-11-23: six desktop suites pass, including 59 functional checks and a rendered original-formatting view above 2 MB. A narrow-layout regression introduced by the new controls was corrected. A test crash from mismatched compiled window layouts no longer occurs after a consistent rebuild.

Android: 118 unit tests pass, one live test is skipped in the offline run, and the separate live test passes with two distinct 20-work searches and a readable chapter. Lint has zero errors, 35 warnings and two hints. Controlled tests cover formatted-content persistence, backup retention, HTML sanitization, image embedding and single-request bulk inbox changes. No production account writes were performed.

The connected phone was on rc.10 and was updated successfully to the verified signed rc.14 APK, code 29, retaining app data. The new source changes are not installed or published. A temporary, signed instrumentation helper also passed read-only public requests on the physical phone: two distinct searches returned 20 works each, reading returned one chapter, and download-format and comments pages responded. The copied session was signed out, so authenticated actions and visual acceptance remain pending. The helper was removed afterward. Evidence is in the ignored `.codex-attachments/ao3-next-verification` folder.


## 3.2.0-rc.15

2026-11-23: all six desktop suites pass after the component-skin changes. The new editor check verifies five tabs, draft preview updates and Cancel without applying; its rendered layout was inspected. Component tests cover separate palettes, corners and spacing, repeated application without size accumulation, restoration, cross-platform JSON and invalid rules. A stale style pointer exposed by the mobile-layout test was replaced with a tracked pointer; the complete suite now passes with style replacement.

Android release: 120 tests pass, one live test is skipped; lint has zero errors, 37 warnings and two hints. APK and AAB signatures verify, and the APK passes 16 KB alignment checks. Android version code 30 uses the existing release signer. Component rendering on the physical phone still requires visual acceptance; automated tests do not establish that.

Native skin version 3 adds independent button, input, card and header rules without CSS injection. Older versions remain supported. Remaining release checks are authenticated writes, device accessibility/rotation/Back/file-pickers, external media/work-skin assets and long reading sessions. Keep this release marked prerelease.

The desktop installation in `~/Applications/ao3-client` matches the tested binary and passes an isolated startup check from `/tmp`. The connected phone reports rc.15/code 30 after an in-place update. Existing app data was retained. Release downloads live in `~/Downloads`; the desktop archive includes a complete example component skin.


## 3.2.0-rc.18

2026-11-24: all six desktop suites pass, including a regression for nested header/input palettes and rendering of both bundled neos adaptations. Rendered community themes and the editor were inspected; the editor fits all component rows at the tested size and retains scrolling for smaller screens. Native component rules are applied after stylesheet refresh, with parents before children.

Android: 122 tests pass and one live test is skipped; release lint has zero errors, 37 warnings and two hints. Regression tests verify reading changes preserve the complete custom interface skin and persist the independent reading-color choice across restart. APK and AAB signatures verify. Both bundled designs, original author credit, MIT license and adaptation notes are packaged offline.

Skin editor inheritance rows now align text and switches with explicit spacing; wrapping action/chip rows have vertical gaps. The editor uses neutral component geometry outside its draft preview. Fic-card padding is applied once. The neos variants retain supported upstream geometry and surfaces, with documented contrast adjustments and unsupported website features.

Phone visual acceptance and rc.18 installation are pending: no device was connected during this pass. Physical layout, large fonts, rotation and long-session checks remain separate from automated validation.

The desktop installation in `~/Applications/ao3-client` matches the tested rc.18 binary and passes an isolated startup check from `/tmp`; existing application data is retained.


## 3.2.0-rc.19

2026-11-24: native skin version 4 adds embedded PNG/JPEG assets, background and overlay layers, and a reserved header banner in both editions. Assets are bounded by file size, dimensions, count and decoded cache size. Images are painted beneath controls without receiving input or fetching remote resources. Decoration opacity is bounded across stacked layers to preserve text contrast; reading content retains its plain reading surface.

Desktop regression checks cover the bundled Neos Nebula skin, image/credit round trips, rejected remote images and invalid layer fields, native rendering, button interaction, editor preservation, repeat application and banner removal. The rendered editor and example were inspected. All six desktop suites pass with the final stacked-opacity adjustment. The installed desktop binary matches the build and passes isolated startup from /tmp, retaining existing app data.

Android: 125 tests pass and one live-network test is skipped. Release lint reports zero errors and 39 warnings. APK and AAB signatures verify and the APK passes 16 KB alignment checks. The portable example and image licenses are included. Android version code is 34.

The example combines a Bryan Goff photograph under the Unsplash License, an unmodified OpenMoji star by Vanessa Boutzikoudi under CC BY-SA 4.0, and the credited neos component adaptation under MIT. Source and license details are included in the portable skin and package. No upstream Stargazer CSS was copied.

Physical-phone installation and visual acceptance remain pending: no device was connected during this pass. Automated checks do not establish physical layout, rotation, file-picker handling or long-session memory behavior. Remaining website-parity and authenticated-write limits above still apply.


## 3.2.0-rc.20

2026-11-24: all six desktop suites pass. Added checks verify the full title remains on one line, the window reaches 900/1100/1440 logical-pixel widths, search stays centered, and navigation belongs to the header. The header check also passes with QT_FONT_DPI=144. Rendered wide and narrow Neos Nebula layouts were inspected.

A synthetic wide avatar verifies center cropping across the circular account button, corner clipping and click handling. A solid image verifies background-cover rendering reaches all four corners of a larger target. Existing checks cover skin import/export, old versions, banner spacing, repeated application and reset. These checks use local test images, not server profile-picture availability.

Removed redundant image/inheritance explanations and decorative source headings, and shortened bundled skin names. Credits, licenses, controls and validation errors remain. Home no longer has an extra outer frame, flat navigation remains flat under custom skins and uses an underline on hover/focus without changing text contrast, and fill-area image controls no longer offer ignored sizes/offsets.

Physical-phone visual acceptance remains pending. Android also uses background-cover fill and edge-to-edge profile-picture cropping. Full website parity and authenticated-write acceptance remain incomplete.


## 3.2.0-rc.21

2026-11-24: all six desktop suites pass, including minimum window-edge spacing and a regression for account avatars becoming oval under large button padding. Header controls retain their outer insets with zero-padding skins; account avatars keep a square, scaled size independently of ordinary button padding.

A three-minute automated reader run completed 890 navigation operations across scrolling/book modes and six chapters. Peak resident memory was 103,304 KiB; median memory after warm-up grew by 60 KiB. Saved offline text and reading position were restored after recreating the reader. This short automated run does not establish hours-long reading, battery behavior or physical-device acceptance.

Read-only live desktop checks confirmed the saved native session, account lists and a bookmark form. The actual account image was fetched and rendered. A specifically authorized temporary private bookmark was created, verified, deleted and confirmed absent, using production account methods with fresh forms. No public comments or kudos were submitted. Account details, images and cookie copies are excluded from the repository and release artifacts.

Android: 127 tests pass and one live-network test is skipped in the standard release suite; lint reports zero errors and 39 warnings. The separate live test passed two distinct 20-work searches and opened a readable chapter. New tests cover backup providers opening away from the UI thread, stream closure, round-trip data and recovery after provider errors. Import confirmation survives recreation; reader export propagates cancellation without an error message.

Physical-phone visual, rotation, Back, accessibility and extended-session checks remain pending. Full native website parity remains incomplete; website-only tools remain in the embedded site.
