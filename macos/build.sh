#!/bin/bash
# Builds AzerothCore.app and AzerothCore.dmg (plus a .sha256) into ./dist.
# Usage: ./build.sh [version]
set -euo pipefail
cd "$(dirname "$0")"
VERSION="${1:-0.0.0}"

swift build -c release --arch arm64 --arch x86_64
BIN="$(swift build -c release --arch arm64 --arch x86_64 --show-bin-path)/AzerothCore"

rm -rf dist
APP="dist/AzerothCore.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$BIN" "$APP/Contents/MacOS/AzerothCore"

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
  <key>LSMinimumSystemVersion</key><string>13.0</string>
  <key>NSHighResolutionCapable</key><true/>
</dict></plist>
PLIST

# App icon from the shared artwork.
ICONSET="dist/icon.iconset"
mkdir -p "$ICONSET"
for size in 16 32 128 256 512; do
  sips -z $size $size ../windows/Assets/icon.png --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
  sips -z $((size*2)) $((size*2)) ../windows/Assets/icon.png --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/icon.icns"
rm -rf "$ICONSET"

# Ad-hoc signature so the app runs on Apple silicon. Not notarized.
codesign --force --deep --sign - "$APP"

hdiutil create -volname AzerothCore -srcfolder "$APP" -ov -format UDZO dist/AzerothCore.dmg >/dev/null
(cd dist && shasum -a 256 AzerothCore.dmg > AzerothCore.dmg.sha256)
echo "Built dist/AzerothCore.dmg ($VERSION)"
