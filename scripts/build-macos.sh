#!/bin/bash
# Builds a universal AzerothCore.app (SwiftUI over the Rust core) and AzerothCore.dmg,
# plus a .sha256, into ./dist.
# Usage: scripts/build-macos.sh [version]   (run from anywhere; needs Rust, Xcode and rustup)
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${1:-0.0.0}"
ROOT="$PWD"
MAC="$ROOT/macos"

# Use the newest Xcode installed unless the caller already chose one.
if [ -z "${DEVELOPER_DIR:-}" ]; then
  newest=$(ls -d /Applications/Xcode*.app 2>/dev/null | sort -V | tail -1 || true)
  [ -n "$newest" ] && export DEVELOPER_DIR="$newest/Contents/Developer"
fi

export AC_VERSION="$VERSION"
export MACOSX_DEPLOYMENT_TARGET=14.0
rustup target add aarch64-apple-darwin x86_64-apple-darwin
for t in aarch64-apple-darwin x86_64-apple-darwin; do
  cargo build -p azerothcore-core --release --target "$t"
done

# One universal static library for the Swift package to link.
rm -rf "$MAC/build" "$MAC/Sources/AzerothCore/Generated"
mkdir -p "$MAC/build/lib" "$MAC/Sources/AzerothCore/Generated"
lipo -create -output "$MAC/build/lib/liblauncher_core.a" \
  target/aarch64-apple-darwin/release/liblauncher_core.a \
  target/x86_64-apple-darwin/release/liblauncher_core.a

# Swift bindings. The generator reads the API from the library, so the host build is enough.
cargo run -q -p azerothcore-core --features cli --bin uniffi-bindgen -- generate \
  --library target/aarch64-apple-darwin/release/liblauncher_core.dylib \
  --language swift --out-dir "$MAC/build/gen"
cp "$MAC/build/gen/launcher_core.swift" "$MAC/Sources/AzerothCore/Generated/"
cp "$MAC/build/gen/launcher_coreFFI.h" "$MAC/Sources/LauncherCoreFFI/include/"

(cd "$MAC" && swift build -c release --arch arm64 --arch x86_64)

rm -rf dist
APP="dist/AzerothCore.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$MAC/.build/apple/Products/Release/AzerothCore" "$APP/Contents/MacOS/AzerothCore"
cp "$MAC"/Resources/hero.png "$MAC"/Resources/logo.png "$APP/Contents/Resources/"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleName</key><string>AzerothCore</string>
  <key>CFBundleDisplayName</key><string>AzerothCore</string>
  <key>CFBundleIdentifier</key><string>com.nickk02.azerothcore</string>
  <key>CFBundleExecutable</key><string>AzerothCore</string>
  <key>CFBundleIconFile</key><string>icon</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>${VERSION}</string>
  <key>CFBundleVersion</key><string>${VERSION}</string>
  <key>LSMinimumSystemVersion</key><string>14.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
</dict></plist>
PLIST

ICONSET="dist/icon.iconset"
mkdir -p "$ICONSET"
for size in 16 32 128 256 512; do
  sips -z $size $size "$MAC/Resources/icon.png" --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
  sips -z $((size*2)) $((size*2)) "$MAC/Resources/icon.png" --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/icon.icns"
rm -rf "$ICONSET"

# Ad-hoc signature so the app runs on Apple silicon. Not notarized.
codesign --force --deep --sign - "$APP"

hdiutil create -volname AzerothCore -srcfolder "$APP" -ov -format UDZO dist/AzerothCore.dmg >/dev/null
(cd dist && shasum -a 256 AzerothCore.dmg > AzerothCore.dmg.sha256)
echo "Built dist/AzerothCore.dmg ($VERSION)"
