#!/bin/bash
# Zeetures for macOS (owner, 2026-11-20: players install and start it
# without Terminal): Zeetures.app, in a disk image beside a shortcut to
# Applications. Run on a Mac by the nightly workflow:
#
#   packaging/macos/make-dmg.sh <cmake install folder> <version> <output .dmg>
#
# The app is signed ad hoc only (no Apple Developer ID), so Gatekeeper asks
# the player to allow it once (README: Download and play).
set -euo pipefail

installed=$1
version=$2
dmg=$3
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
stage="$work/Zeetures"
app="$stage/Zeetures.app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"

# The program; SDL_GetBasePath() is Contents/Resources inside an app, so
# the art goes there
cp "$installed/openc2e" "$app/Contents/MacOS/Zeetures"
cp -R "$installed/zeetures-art" "$app/Contents/Resources/zeetures-art"
cp "$repo/packaging/icons/zeetures.icns" "$app/Contents/Resources/Zeetures.icns"
for f in COPYING AUTHORS README.md; do
	cp "$installed/$f" "$app/Contents/Resources/$f"
done

sed -e "s/@VERSION@/$version/g" "$here/Info.plist.in" > "$app/Contents/Info.plist"
printf 'APPL????' > "$app/Contents/PkgInfo"

# (an ad hoc signature: Apple Silicon runs nothing unsigned)
codesign --force --deep --sign - "$app"
codesign --verify --verbose "$app"

# The disk image: the app, Applications, and the notes
ln -s /Applications "$stage/Applications"
cp "$installed/COPYING" "$stage/License (LGPL 2.1).txt"
cp "$here/Read me.txt" "$stage/Read me.txt"
rm -f "$dmg"
hdiutil create -volname "Zeetures" -srcfolder "$stage" -fs HFS+ -format UDZO -ov "$dmg"
echo "Made $dmg"
