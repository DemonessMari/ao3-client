# AO3 Client

An unofficial desktop and Android reader for Archive of Our Own. Independent of AO3 and the Organization for Transformative Works. **3.0.0 is a release candidate**: automated tests pass before publication, but this is not a claim of exhaustive phone/device testing.

The desktop edition uses Qt 6. Android uses native Kotlin, Jetpack Compose and Material 3; no Qt or WebView is embedded in the Android release.

## Features in both editions

| Feature | Desktop | Android |
| --- | --- | --- |
| Real AO3 search, pagination and bundled offline filters | Yes | Yes |
| Online tag suggestions while typing | Yes | Yes |
| Sign-in, own works, public profiles and author/bookmark listings | Yes | Yes |
| Full-window reading, chapter selection and previous/next chapters | Yes | Yes |
| Light, Sepia, Dark; font, size and line spacing with live preview | Yes | Yes |
| Fullscreen, text-only and paged book modes | Yes | Yes |
| Optional opaque paper-curl animation | Yes | Yes |
| Page navigation across chapter boundaries | Yes | Yes |
| Recent, Offline, Favorites; library search and local bookmarks | Yes | Yes |
| Saved progress, original-text export and optional translation | Yes | Yes |
| Cross-edition library backup export/import | Yes | Yes |
| Complete AO3 site access through clearly labelled browser links | Yes | Yes |

Comments, kudos, account bookmark creation/editing, subscriptions, series, collections, history and account preferences use the original AO3 site in your browser. These are **not native client screens**. Your browser manages its own sign-in. Public bookmark listings in the client show AO3 works only, not external or series bookmarks. Local favorites and reading bookmarks do not modify your AO3 account.

## Reading

Open a fic from search or Library. The reader fills the content area. Chapter selection is separate from page selection. Page buttons and page-edge taps/clicks move between pages; reaching a chapter boundary continues into the next/previous chapter. Android also supports horizontal swipes; desktop supports the wheel, Left/Right, Page Up/Page Down and Space. Disable animation for immediate turns.

Android: **Reading options → Reading appearance** edits theme, font, size and spacing without leaving the fic. Desktop: **Reading settings** provides the same live controls with a preview. Secondary reader controls are in an expandable **Reading tools** panel so the text keeps most of the window. Appearance changes are saved immediately; the separate Save action applies language/translation settings. Font sizes use native platform units, so equal numbers need not look identical.

**Text only** hides chrome. Restore it by tapping/clicking the center in book mode, Android Back, desktop F10/Escape, or double-clicking scrolling desktop text. Desktop F11 toggles window fullscreen. Android Back exits text-only/fullscreen before leaving the reader.

Progress is saved per work. Reopening restores it; Android also restores an interrupted reader after process restart when metadata is available. Explicit Android chapter selection starts the selected chapter at its beginning. Desktop retains its existing per-chapter saved positions; boundary page turns explicitly start at the beginning/end of the adjacent chapter. Positions are approximate after cross-platform backup import. Named bookmarks provide an explicit return point.

Save offline keeps all original chapters, separate from reading history. Offline copies are snapshots. Remove the download, reopen online and save again to refresh it. Translation errors leave the original readable. Nothing pretends to be a successful search when AO3 is unavailable.

## Backups and migration

Android: **Settings → Library backup**. Desktop: **Library → Export/Import library backup**. Backups contain fics, downloads, favorites, bookmarks and reading positions, excluding passwords, cookies and translation keys. Imports merge without replacing existing downloads or reading progress. Backups contain reading history and text: keep them private. Export/import is limited to 100 MB; omit downloaded text for a compact reading-data backup when the library is large, and export original fics separately as text.

The public Android release uses a private production signing key. It cannot update older development-signed APKs directly. First install the locally supplied **3.0.0 debug migration build** over your old development build, export your library, uninstall the development build, install the release APK, and import the backup. Do not uninstall before exporting. The app ID remains `org.ao3client.reader`. Old Qt-prototype data is not migrated automatically.

## Translation

Configure a trusted LibreTranslate URL and optional API key in Reading settings; there is no preselected third-party provider. The default target language is English, optionally used for searches. Remote services require HTTPS; HTTP is allowed only on localhost. Before first use, the client asks permission to send chapter text to the configured service. Availability, supported languages, privacy and quotas depend on that service. No translator is bundled. See [PRIVACY.md](PRIVACY.md).

## Install desktop on Linux

Requires a C++17 compiler, CMake and Qt 6 Core/Gui/Widgets/Network; tests use Qt Test. On Debian/Ubuntu, install `build-essential cmake qt6-base-dev python3`.

```sh
./scripts/install-desktop.sh
```

This builds, tests and installs into `~/.local/bin`, `~/.local/share/applications` and the standard icon directory. The installer resolves the launcher to the installed executable, so it also works when the desktop session PATH omits `~/.local/bin`. The repository template is generic and no launcher refers to the source folder. Existing settings and reading data are retained. You can move/delete the build folder after installation. Pass a custom prefix as the first argument if needed.

Extract the Linux release archive and run its `install.sh` (requires Python 3). It installs the same `usr/bin` and `usr/share` layout under your chosen prefix. It dynamically links system Qt rather than bundling it. The locally built archive requires compatible system Qt and glibc; see release notes for the build environment. Building from source is recommended on other distributions. Windows and macOS installers are not provided.

To uninstall the per-user executable/launcher/icon, remove `~/.local/bin/ao3-client`, `~/.local/share/applications/ao3-client.desktop` and `~/.local/share/icons/hicolor/256x256/apps/ao3-client.png`. Reading data is retained.

## Build and verify

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 1
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen build/ao3-client --smoke-test
./scripts/package-desktop.sh
```

Android requires JDK 17, Android SDK platform 36 and build tools 36.0.0. Gradle and dependencies are pinned; the wrapper distribution checksum is verified. The app supports Android 9 (API 28)+ and targets Android 16 (API 36).

```sh
export ANDROID_HOME="$HOME/Android/Sdk"
./scripts/build-android.sh debug
```

This runs JVM tests and lint, builds an explicitly labelled development APK, verifies its signature and writes a checksum. One worker and in-process Kotlin compilation limit memory use. No emulator is launched automatically. For a production APK/AAB, provide a private keystore with alias `ao3-release`:

```sh
export AO3_SIGNING_FILE=/private/path/release.keystore
# Set AO3_SIGNING_PASSWORD locally, outside shell history and source control.
./scripts/build-android.sh release
```

Keep an encrypted backup of your release keystore and password. Losing the signing key prevents normal updates for direct APK distribution. Keys/passwords are never stored in this repository or uploaded as release assets. CI runs desktop and Android checks, producing only a development APK unless signing is explicitly configured elsewhere.

Opt-in live AO3 check (two searches and one public fic, no account writes):

```sh
AO3_LIVE_TEST=1 ./scripts/build-android.sh debug
```

See [RELEASE-CHECKLIST.md](RELEASE-CHECKLIST.md), [VERIFICATION.md](VERIFICATION.md), [PRIVACY.md](PRIVACY.md) and [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md). Report bugs with version, device/OS, steps and expected/actual behavior. Do not upload private works or full backups.
