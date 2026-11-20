#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${AO3_BUILD_DIR:-$project_dir/build}"
version="$(sed -n 's/project(ao3-client VERSION \([^ ]*\).*/\1/p' "$project_dir/CMakeLists.txt")"
stage="$(mktemp -d)"; trap 'rm -rf -- "$stage"' EXIT
cmake --install "$build_dir" --prefix "$stage/ao3-client-$version/usr"
cp "$project_dir/LICENSE" "$project_dir/README.md" "$project_dir/PRIVACY.md" "$project_dir/THIRD-PARTY-NOTICES.md" "$stage/ao3-client-$version/"
cp "$project_dir/scripts/write-launcher.py" "$stage/ao3-client-$version/write-launcher.py"
cat > "$stage/ao3-client-$version/install.sh" <<'INSTALL'
#!/usr/bin/env sh
set -eu
archive_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
prefix="${1:-$HOME/.local}"
mkdir -p "$prefix/bin" "$prefix/share"
install -m 755 "$archive_dir/usr/bin/ao3-client" "$prefix/bin/.ao3-client-new-$$"
mv -f "$prefix/bin/.ao3-client-new-$$" "$prefix/bin/ao3-client"
cp -R "$archive_dir/usr/share/." "$prefix/share/"
python3 "$archive_dir/write-launcher.py" "$prefix"
if command -v update-desktop-database >/dev/null 2>&1; then update-desktop-database "$prefix/share/applications"; fi
printf 'Installed AO3 Client in %s/bin; the launcher resolves it directly.\n' "$prefix"
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
