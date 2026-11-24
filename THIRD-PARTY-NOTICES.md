# Third-party notices

The client source is licensed under the MIT License (see LICENSE). Fanworks remain the property of their respective creators; the client license does not grant rights to downloaded works. AO3 and OTW names belong to their respective owners. No affiliation or endorsement is implied.

## Desktop

Qt 6 Core, Gui, Widgets, Network and WebEngineWidgets are dynamically linked system libraries. Qt is available under LGPL v3 / GPL / commercial terms as applicable to the installed distribution. This project does not statically link Qt or bundle it in the Linux archive. Users can replace their system Qt libraries. Source and licensing: https://www.qt.io/licensing/open-source-lgpl-obligations and https://code.qt.io/cgit/qt/qtbase.git/ . Qt WebEngine uses system-provided Chromium components and their third-party licenses; see the installed Qt WebEngine package and https://code.qt.io/cgit/qt/qtwebengine.git/ . A copy of LGPL v3 is included in `licenses/Qt-LGPL-3.0.txt`.

## Native Android

Kotlin, AndroidX and Jetpack Compose: Apache License 2.0. Sources: https://github.com/JetBrains/kotlin and https://android.googlesource.com/platform/frameworks/support/ .

jsoup HTML parser: MIT License, copyright Jonathan Hedley and contributors. Source: https://github.com/jhy/jsoup . The pinned version is listed in `android-native/app/build.gradle`.

License texts and Android notices are included in `android-native/app/src/main/assets/licenses/` and in the APK. The native Android release does not bundle Qt or OpenSSL. The old Qt Android prototype is retained as reference source only and is not the released APK.


## neos native skin adaptations

Original neos design by ZerafinaCSS: https://github.com/ZerafinaCSS/neos. The Tumblr Blue and Charcoal Rose JSON adaptations retain the original design values where the native format supports them. See `skins/neos/README.md` for mapping and differences.

MIT License

Copyright (c) 2025 Zerafina (https://github.com/ZerafinaCSS)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


## Neos Nebula artwork

The native component design derives from ZerafinaCSS neos (MIT), credited above. The image variant adds:

- Orion Nebula photograph by Bryan Goff, https://unsplash.com/photos/f7YQo-eYHdM, under the Unsplash License: https://unsplash.com/license. Header crop distributed by intothisshadow's Stargazer skin; its CSS is not included.
- OpenMoji star (2B50) by Vanessa Boutzikoudi, 2018, https://github.com/hfg-gmuend/openmoji, CC BY-SA 4.0: https://creativecommons.org/licenses/by-sa/4.0/. Original image bytes are unchanged.

Image credits, source URLs and licenses are embedded in the portable skin and retained on export. See the license files under `skins/illustrated`. Asset licenses are separate from the application's MIT license.
