#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${AO3_BUILD_DIR:-$project_dir/build}"
version="$(sed -n 's/.*setApplicationVersion(QStringLiteral("\([^"]*\)")).*/\1/p' "$project_dir/src/main.cpp")"
stage="$(mktemp -d)"; trap 'rm -rf -- "$stage"' EXIT
cmake --install "$build_dir" --prefix "$stage/ao3-client-$version/usr"
cp "$project_dir/SKINS.md" "$project_dir/LICENSE" "$project_dir/README.md" "$project_dir/PRIVACY.md" "$project_dir/THIRD-PARTY-NOTICES.md" "$project_dir/FEATURES.md" "$project_dir/RELEASE-CHECKLIST.md" "$project_dir/VERIFICATION.md" "$project_dir/SECURITY.md" "$stage/ao3-client-$version/"
cp -R "$project_dir/skins" "$stage/ao3-client-$version/skins"
cp "$project_dir/tests/fixtures/app-skin-v3.json" "$stage/ao3-client-$version/example-skin.json"
cp "$project_dir/scripts/install-package.py" "$stage/ao3-client-$version/install-package.py"
cp "$project_dir/scripts/write-launcher.py" "$stage/ao3-client-$version/write-launcher.py"
cat > "$stage/ao3-client-$version/install.sh" <<'INSTALL'
#!/usr/bin/env sh
set -eu
archive_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
application_dir="${1:-$HOME/Applications/ao3-client}"
python3 "$archive_dir/install-package.py" "$archive_dir/usr" "$application_dir"
if command -v update-desktop-database >/dev/null 2>&1; then update-desktop-database "$HOME/.local/share/applications"; fi
INSTALL
chmod +x "$stage/ao3-client-$version/install.sh"
{
  printf 'AO3 Client %s\n' "$version"
  printf 'Architecture: %s\n' "$(uname -m)"
  printf 'System Qt: %s\n' "$(pkg-config --modversion Qt6Core)"
  getconf GNU_LIBC_VERSION
  printf 'System libraries are dynamically linked, not bundled.\n'
} > "$stage/ao3-client-$version/BUILD-INFO.txt"
mkdir -p "$project_dir/dist"
tar -C "$stage" -czf "$project_dir/dist/ao3-client-desktop-$version-linux-$(uname -m).tar.gz" "ao3-client-$version"
printf 'Linux archive created. Requires matching system Qt 6 libraries; see README.\n'
