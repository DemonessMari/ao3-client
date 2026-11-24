#!/usr/bin/env bash
set -euo pipefail
# API 34 runtime used by the Robolectric rendering tests.
artifact=android-all-instrumented
version=14-robolectric-10818077-i7
relative="org/robolectric/$artifact/$version"
destination="$HOME/.m2/repository/$relative"
mkdir -p "$destination"
temporary="$(mktemp -d)"
trap 'rm -rf -- "$temporary"' EXIT
for extension in pom jar; do
    name="$artifact-$version.$extension"
    for suffix in .sha512 ''; do
        if [[ ! -s "$destination/$name$suffix" ]]; then
            curl --fail --silent --show-error --location --retry 4 --retry-all-errors --retry-delay 5 \
                "https://repo.maven.apache.org/maven2/$relative/$name$suffix" -o "$temporary/$name$suffix"
            mv "$temporary/$name$suffix" "$destination/$name$suffix"
        fi
    done
    printf '%s  %s\n' "$(cat "$destination/$name.sha512")" "$destination/$name" | sha512sum --check --status
done
