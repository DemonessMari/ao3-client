# Third-party notices

The client source is licensed under the MIT License (see LICENSE). Fanworks remain the property of their respective creators; the client license does not grant rights to downloaded works. AO3 and OTW names belong to their respective owners. No affiliation or endorsement is implied.

## Desktop

Qt 6 Core, Gui, Widgets and Network are dynamically linked system libraries. Qt is available under LGPL v3 / GPL / commercial terms as applicable to the installed distribution. This project does not statically link Qt or bundle it in the Linux archive. Users can replace their system Qt libraries. Source and licensing: https://www.qt.io/licensing/open-source-lgpl-obligations and https://code.qt.io/cgit/qt/qtbase.git/ . A copy of LGPL v3 is included in `licenses/Qt-LGPL-3.0.txt`.

## Native Android

Kotlin, AndroidX and Jetpack Compose: Apache License 2.0. Sources: https://github.com/JetBrains/kotlin and https://android.googlesource.com/platform/frameworks/support/ .

jsoup HTML parser: MIT License, copyright Jonathan Hedley and contributors. Source: https://github.com/jhy/jsoup . The pinned version is listed in `android-native/app/build.gradle`.

License texts and Android notices are included in `android-native/app/src/main/assets/licenses/` and in the APK. The native Android release does not bundle Qt or OpenSSL. The old Qt Android prototype is retained as reference source only and is not the released APK.
