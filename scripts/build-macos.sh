#!/bin/bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
: "${QT_ROOT:?Set QT_ROOT to the Qt macOS installation directory}"
cmake -S "$root" -B "$root/build/macos" -DCMAKE_PREFIX_PATH="$QT_ROOT" -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build "$root/build/macos" --parallel 3
ctest --test-dir "$root/build/macos" -R '^(macos|annotation)$' --output-on-failure
cmake --install "$root/build/macos" --prefix "$root/dist/macos"
"$QT_ROOT/bin/macdeployqt" "$root/dist/macos/MouseWheel.app" -always-overwrite
mkdir -p "$root/dist/macos/MouseWheel.app/Contents/Resources"
cp -R "$root/licenses" "$root/dist/macos/MouseWheel.app/Contents/Resources/"
cp "$root/LICENSE" "$root/dist/macos/MouseWheel.app/Contents/Resources/"
codesign --force --deep --sign - "$root/dist/macos/MouseWheel.app"
codesign --verify --deep --strict "$root/dist/macos/MouseWheel.app"
ditto -c -k --sequesterRsrc --keepParent "$root/dist/macos/MouseWheel.app" "$root/dist/MouseWheel-macos-universal.zip"
