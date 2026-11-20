#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
prefix="${1:-$HOME/.local}"
build_dir="${AO3_BUILD_DIR:-$project_dir/build}"
cmake -S "$project_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel 1
ctest --test-dir "$build_dir" --output-on-failure
cmake --install "$build_dir" --prefix "$prefix"
python3 "$project_dir/scripts/write-launcher.py" "$prefix"
if command -v update-desktop-database >/dev/null; then update-desktop-database "$prefix/share/applications"; fi
if command -v gtk-update-icon-cache >/dev/null; then gtk-update-icon-cache --ignore-theme-index "$prefix/share/icons/hicolor" >/dev/null 2>&1 || true; fi
printf 'Installed to %s/bin/ao3-client. The launcher resolves the installed executable independently of PATH.\n' "$prefix"
