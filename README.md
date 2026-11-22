# AO3 Client

Unofficial AO3 reader for Linux (Qt 6) and Android (Kotlin/Compose). Not affiliated with AO3 or OTW. MIT licensed.

Both editions support search and filters, tag suggestions, sign-in, profiles, offline reading, favorites, bookmarks, progress, appearance settings, fullscreen, paged reading, translation and library backups.

Comments can be read, posted and replied to inside both editions. The current source also adds kudos, official-format downloads, AO3 work bookmarks, Mark for Later/Mark as Read, work subscriptions, and an embedded AO3 view; these additions are not in the 3.1.0 release yet. Native work bookmarks support notes, tags, collections, pseuds, privacy, recommendations and deletion. Public native bookmark lists show AO3 works only.

The AO3 website view provides official account, inbox, comment-management, series, collections, skin and creator tools, plus original work formatting. Website sign-in is separate; use AO3’s Remember me setting for persistence. It requires a connection. If AO3 presents human verification, use Sign in on AO3 website, complete it yourself, then choose More → Use website sign-in in reader. A server check verifies the handoff before replacing the native account. See [feature coverage](FEATURES.md).

3.1.0 is a prerelease. Phone rendering and long-session testing remain pending. See [verification](VERIFICATION.md).

## Install

Linux: extract the release archive and run `./install.sh`. Requires Python 3 and compatible system Qt, Qt WebEngine and glibc; see the archive's `BUILD-INFO.txt`. Installs under `~/Applications/ao3-client`, with the launcher and command symlink under `~/.local`.

Or build and install from source (C++17, CMake, Qt 6 Core/Gui/Widgets/Network/WebEngineWidgets/Test and Python 3):

```sh
./scripts/install-desktop.sh
```

Android: install the release APK. Requires Android 9 or newer.

**Upgrading an old development APK:** install the local 3.0.0 debug migration build, export your library, then uninstall the development app, install the release APK and import. Export before uninstalling: the signing keys differ.

## Reading and backups

Appearance changes save automatically. Language and translation settings use Save. Text-only controls return with a center tap in book mode, Android Back, or desktop F10/Escape. Desktop F11 toggles fullscreen; double-click restores controls in scrolling mode.

Export/import is under Library on desktop and Settings on Android. Backups merge; existing downloads and progress are kept. Limit: 100 MB. Omit downloaded text for smaller backups. Backups are unencrypted and exclude credentials. Cross-edition positions are approximate.

Translation requires a LibreTranslate URL, optional API key, and consent to send chapter text. Remote services require HTTPS; localhost may use HTTP. Offline copies are snapshots; remove and download again to refresh.

## Build

Desktop:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 1
ctest --test-dir build --output-on-failure
./scripts/package-desktop.sh
```

Android requires JDK 17, SDK platform 36 and build tools 36.0.0:

```sh
export ANDROID_HOME="$HOME/Android/Sdk"
./scripts/build-android.sh debug
```

For release APK/AAB builds, set `AO3_SIGNING_FILE` and `AO3_SIGNING_PASSWORD` privately; the keystore alias is `ao3-release`. Run `./scripts/build-android.sh release`. Keep the signing key backed up and outside source control.

[Privacy](PRIVACY.md) · [Security](SECURITY.md) · [Licenses](THIRD-PARTY-NOTICES.md) · [Release checklist](RELEASE-CHECKLIST.md)

App colors can be edited, previewed and shared between editions using [app skins](SKINS.md). Home shows recent reads and library counts; search filters open using **Filters**.
