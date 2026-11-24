# AO3 Client

Unofficial AO3 reader for Linux (Qt 6) and Android (Kotlin/Compose). Not affiliated with AO3 or OTW. MIT licensed.

Both editions support work, people, tag and bookmark searches with filters, tag suggestions, sign-in, profiles, offline reading, favorites, bookmarks, progress, appearance settings, fullscreen, paged reading, translation and library backups.

Both editions have native kudos, AO3 work bookmarks, Mark for Later/Read, work/author/series subscriptions, account lists, confirmed history deletion/clearing and ordered series lists. Comments support reading, posting, replies, editing, deletion and the moderation controls offered by AO3. Work bookmarks support notes, tags, collections, pseuds, privacy and recommendations.

The embedded AO3 website provides creator/account-management tools, bulk list management, external/series bookmark editing and original work formatting. Its sign-in is separate. For a verification challenge, complete website sign-in yourself, then choose **Use website sign-in in reader**. A fresh server check verifies the account before replacing native sign-in. See [feature coverage](FEATURES.md).

3.2.0-rc.23 is a prerelease. Phone rendering, authenticated writes and long-session testing remain pending. See [verification](VERIFICATION.md).

Native skins support component styling and embedded image layers without CSS. Import an image in the skin editor or try the bundled **Neos Nebula** preset. Image credits and licenses travel with exported skins. See [skin format](SKINS.md).

## Install

Linux: extract the release archive and run `./install.sh`. Requires Python 3 and compatible system Qt 6.8 or newer, Qt WebEngine and glibc; see the archive's `BUILD-INFO.txt`. Installs under `~/Applications/ao3-client`, with the launcher and command symlink under `~/.local`.

Or build and install from source (C++17, CMake, Qt 6.8+ Core/Gui/Widgets/Network/WebEngineWidgets/Test and Python 3):

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

App colors, interface scale and control corners can be edited, previewed and shared between editions using [app skins](SKINS.md). Home shows recent reads and library counts; search filters open using **Filters**.

Native profile views display AO3 profile pictures when available, with an initial as fallback. **Profile pictures** in the account website menu opens AO3’s pseud editor for changing them.
