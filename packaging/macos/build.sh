#!/bin/bash
# Builds Zeetures for every Mac (Apple Silicon and Intel in one universal
# program, macOS 10.15 and later) and packages it as a disk image. Run from
# the repository on a Mac, either kind, by the nightly workflow:
#
#   packaging/macos/build.sh <output .dmg>
set -euo pipefail

dmg=$1
version=$(sed -n 's/^#define OPENC2E_FORK_VERSION "\(.*\)"/\1/p' src/openc2e/openc2eimgui/C2ToolBar.cpp)

cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENC2E_ASAN=OFF \
	"-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
cmake --build build -j3
rm -rf build/installed
cmake --install build --prefix build/installed
# (without the debug information)
strip build/installed/openc2e
lipo -archs build/installed/openc2e
lipo -archs build/installed/openc2e | grep -q arm64
lipo -archs build/installed/openc2e | grep -q x86_64

packaging/macos/make-dmg.sh build/installed "$version" "$dmg"
