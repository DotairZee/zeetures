#!/bin/bash
# Zeetures for Linux as one AppImage file (owner, 2026-11-20: players start
# it with a double-click, no terminal). Run by the nightly workflow:
#
#   packaging/linux/make-appimage.sh <cmake install folder> <x86_64|i686> <output .AppImage>
#
# appimagetool and the AppImage runtime are downloaded from the AppImage
# project's releases unless APPIMAGETOOL (and for i686, RUNTIME_FILE) name
# copies already here. APPIMAGETOOL=none with KEEP_APPDIR only makes the
# AppDir. The runtime is the static one, so players need no
# libfuse2.
set -euo pipefail

installed=$(cd "$1" && pwd)
arch=$2
out=$3
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
id=io.github.dotairzee.zeetures

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
appdir="$work/Zeetures.AppDir"
mkdir -p "$appdir/usr/bin" "$appdir/usr/share/applications" "$appdir/usr/share/icons/hicolor/256x256/apps" \
	"$appdir/usr/share/doc/zeetures"

# The program; SDL_GetBasePath() is its own folder, so the art goes beside it
cp "$installed/openc2e" "$appdir/usr/bin/openc2e"
cp -R "$installed/zeetures-art" "$appdir/usr/bin/zeetures-art"
for f in COPYING AUTHORS README.md; do
	cp "$installed/$f" "$appdir/usr/share/doc/zeetures/$f"
done

cat > "$appdir/AppRun" <<'EOF'
#!/bin/sh
here="$(dirname "$(readlink -f "$0")")"
exec "$here/usr/bin/openc2e" "$@"
EOF
chmod +x "$appdir/AppRun"

cp "$here/$id.desktop" "$appdir/$id.desktop"
cp "$here/$id.desktop" "$appdir/usr/share/applications/$id.desktop"
cp "$repo/packaging/icons/zeetures.png" "$appdir/$id.png"
cp "$repo/packaging/icons/zeetures.png" "$appdir/usr/share/icons/hicolor/256x256/apps/$id.png"
ln -s "$id.png" "$appdir/.DirIcon"

# (KEEP_APPDIR=<folder>: also keep the AppDir there, to try it unpacked)
if [ -n "${KEEP_APPDIR:-}" ]; then
	rm -rf "$KEEP_APPDIR"
	cp -a "$appdir" "$KEEP_APPDIR"
	if [ "${APPIMAGETOOL:-}" = none ]; then
		exit 0
	fi
fi

tool=${APPIMAGETOOL:-}
if [ -z "$tool" ]; then
	tool="$work/appimagetool"
	curl -fsSL -o "$tool" https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
	chmod +x "$tool"
fi
runtime_args=()
if [ "$arch" != "x86_64" ]; then
	runtime=${RUNTIME_FILE:-}
	if [ -z "$runtime" ]; then
		runtime="$work/runtime-$arch"
		curl -fsSL -o "$runtime" "https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-$arch"
	fi
	runtime_args=(--runtime-file "$runtime")
fi

rm -f "$out"
ARCH=$arch APPIMAGE_EXTRACT_AND_RUN=1 "$tool" --no-appstream "${runtime_args[@]}" "$appdir" "$out"
echo "Made $out"
