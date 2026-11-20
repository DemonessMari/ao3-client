#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
: "${QT_ANDROID_DIR:?Set QT_ANDROID_DIR to Qt 6.8+ android_arm64_v8a}"
: "${QT_HOST_PATH:?Set QT_HOST_PATH to the matching desktop Qt installation}"
: "${ANDROID_SDK_ROOT:?Set ANDROID_SDK_ROOT}"
: "${ANDROID_NDK_ROOT:?Set ANDROID_NDK_ROOT}"
: "${AO3_ANDROID_OPENSSL_DIR:?Set AO3_ANDROID_OPENSSL_DIR to the OpenSSL ssl_3/arm64-v8a directory}"
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk}"
export PATH="$JAVA_HOME/bin:$PATH"
export LD_LIBRARY_PATH="$QT_HOST_PATH/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
build_dir="${AO3_ANDROID_BUILD_DIR:-$project_dir/build-android}"
"$QT_ANDROID_DIR/bin/qt-cmake" -S "$project_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DQT_HOST_PATH="$QT_HOST_PATH" -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DAO3_ANDROID_OPENSSL_DIR="$AO3_ANDROID_OPENSSL_DIR"
cmake --build "$build_dir" --target ao3-client --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-1}"
mkdir -p "$project_dir/dist"
# androiddeployqt's default debug package is signed with the local Android
# development key, while the native library above is compiled with optimizations.
"$QT_HOST_PATH/bin/androiddeployqt" \
    --input "$build_dir/android-ao3-client-deployment-settings.json" \
    --output "$build_dir/android-build" \
    --jdk "$JAVA_HOME" --android-platform android-35 --gradle \
    --apk "$project_dir/dist/ao3-client-1.0.1-arm64-v8a.apk"
"$ANDROID_SDK_ROOT/build-tools/35.0.0/apksigner" verify \
    "$project_dir/dist/ao3-client-1.0.1-arm64-v8a.apk"
sha256sum "$project_dir/dist/ao3-client-1.0.1-arm64-v8a.apk" > "$project_dir/dist/ao3-client-1.0.1-arm64-v8a.apk.sha256"
printf 'APK: %s\n' "$project_dir/dist/ao3-client-1.0.1-arm64-v8a.apk"
