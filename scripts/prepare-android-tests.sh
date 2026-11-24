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
verify() {
    [[ -s "$1" && -s "$1.sha512" ]] && printf '%s  %s\n' "$(cat "$1.sha512")" "$1" | sha512sum --check --status
}
for extension in pom jar; do
    name="$artifact-$version.$extension"
    if verify "$destination/$name"; then continue; fi
    verified=false
    for repository in https://repo.maven.apache.org/maven2 https://maven-central.storage-download.googleapis.com/maven2; do
        printf 'Fetching %s from %s\n' "$name" "$repository"
        limit=60
        if [[ "$repository" == *googleapis.com* ]]; then limit=180; fi
        fetched=true
        for suffix in .sha512 ''; do
            if ! curl --fail --silent --show-error --location --connect-timeout 10 --max-time "$limit" \
                --retry 1 --retry-all-errors --retry-delay 3 --retry-max-time 20 \
                "$repository/$relative/$name$suffix" -o "$temporary/$name$suffix"; then fetched=false; break; fi
        done
        if [[ "$fetched" == true ]] && verify "$temporary/$name"; then
            mv "$temporary/$name" "$temporary/$name.sha512" "$destination/"
            verified=true
            break
        fi
        rm -f -- "$temporary/$name" "$temporary/$name.sha512"
    done
    if [[ "$verified" != true ]]; then printf 'Could not fetch a verified Android test runtime.\n' >&2; exit 1; fi
done
