#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export ANDROID_HOME="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}"
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk}"
: "${AO3_SIGNING_FILE:?Use the signing key for the installed app}"
: "${AO3_SIGNING_PASSWORD:?Set the signing password locally}"
"$project_dir/android-native/gradlew" -p "$project_dir/android-native" -Pao3DeviceTestBuildType=release :app:assembleReleaseAndroidTest --max-workers=1 --no-daemon
output="$project_dir/android-native/app/build/outputs/apk/androidTest/release"
"$ANDROID_HOME/build-tools/36.0.0/apksigner" sign --ks "$AO3_SIGNING_FILE" --ks-key-alias ao3-release --ks-pass env:AO3_SIGNING_PASSWORD --key-pass env:AO3_SIGNING_PASSWORD --out "$output/device-network-signed.apk" "$output/app-release-androidTest.apk"
"$ANDROID_HOME/build-tools/36.0.0/apksigner" verify "$output/device-network-signed.apk"
adb install -r -t "$output/device-network-signed.apk"
trap 'adb uninstall org.ao3client.reader.devicecheck >/dev/null 2>&1 || true' EXIT
log_path="${AO3_DEVICE_TEST_LOG:-$output/device-network-result.log}"
adb shell am instrument -w -r -e ao3_network 1 org.ao3client.reader.devicecheck/org.ao3client.reader.DeviceNetworkInstrumentation | tee "$log_path"
rg -q 'ao3_network_success=true' "$log_path"
