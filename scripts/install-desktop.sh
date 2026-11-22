#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
application_dir="${1:-$HOME/Applications/ao3-client}"
build_dir="${AO3_BUILD_DIR:-$project_dir/build}"
cmake -S "$project_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel 1
ctest --test-dir "$build_dir" --output-on-failure
stage="$(mktemp -d)"; trap 'rm -rf -- "$stage"' EXIT
cmake --install "$build_dir" --prefix "$stage/usr"
python3 "$project_dir/scripts/install-package.py" "$stage/usr" "$application_dir"
if command -v update-desktop-database >/dev/null; then update-desktop-database "$HOME/.local/share/applications"; fi
