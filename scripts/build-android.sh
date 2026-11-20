#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export ANDROID_HOME="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}"
if [[ -z "${JAVA_HOME:-}" && -d /usr/lib/jvm/java-17-openjdk ]]; then export JAVA_HOME=/usr/lib/jvm/java-17-openjdk; fi
if [[ -n "${JAVA_HOME:-}" ]]; then export PATH="$JAVA_HOME/bin:$PATH"; fi
mode="${1:-debug}"
version="$(sed -n "s/.*versionName '\([^']*\)'.*/\1/p" "$project_dir/android-native/app/build.gradle")"
case "$mode" in
  debug) tasks=(:app:testDebugUnitTest :app:lintDebug :app:assembleDebug); source_apk="$project_dir/android-native/app/build/outputs/apk/debug/app-debug.apk"; suffix=-debug ;;
  release)
    : "${AO3_SIGNING_FILE:?Set AO3_SIGNING_FILE to your private release keystore}"
    : "${AO3_SIGNING_PASSWORD:?Set AO3_SIGNING_PASSWORD locally; never commit it}"
    tasks=(:app:testReleaseUnitTest :app:lintRelease :app:assembleRelease :app:bundleRelease)
    source_apk="$project_dir/android-native/app/build/outputs/apk/release/app-release.apk"; suffix= ;;
  *) printf 'Usage: %s [debug|release]\n' "$0" >&2; exit 2 ;;
esac
"$project_dir/android-native/gradlew" -p "$project_dir/android-native" "${tasks[@]}" --max-workers=1 --no-daemon
mkdir -p "$project_dir/dist"
apk_path="$project_dir/dist/ao3-client-android-$version$suffix.apk"
cp "$source_apk" "$apk_path"
"$ANDROID_HOME/build-tools/36.0.0/apksigner" verify "$apk_path"
(cd "$project_dir/dist" && sha256sum "$(basename "$apk_path")" > "$(basename "$apk_path").sha256")
if [[ "$mode" == release ]]; then
    cp "$project_dir/android-native/app/build/outputs/bundle/release/app-release.aab" "$project_dir/dist/ao3-client-android-$version.aab"
fi
printf 'APK: %s\n' "$apk_path"
