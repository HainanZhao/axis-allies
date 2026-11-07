#!/usr/bin/env zsh
set -e

APP_NAME="Axis & Allies 2004.app"
CONTENTS_DIR="$APP_NAME/Contents"
MACOS_DIR="$CONTENTS_DIR/MacOS"
RESOURCES_DIR="$CONTENTS_DIR/Resources"

mkdir -p "$MACOS_DIR" "$RESOURCES_DIR"
cp bin/axis_allies_2004 "$MACOS_DIR/axis_allies_2004"
chmod +x "$MACOS_DIR/axis_allies_2004"
cp macos/AppIcon.icns "$RESOURCES_DIR/AppIcon.icns"
rm -rf "$RESOURCES_DIR/assets"
cp -R assets "$RESOURCES_DIR/assets"

cat > "$CONTENTS_DIR/Info.plist" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>axis_allies_2004</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>CFBundleIdentifier</key>
    <string>com.classic2004.axisallies</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>Axis &amp; Allies 2004</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>2004.1.0</string>
    <key>CFBundleVersion</key>
    <string>2004.1.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <false/>
</dict>
</plist>
EOF

echo "Built native macOS Application Bundle: $APP_NAME (Locked to discrete AMD Radeon Pro 5300M GPU & Retina HiDPI)"
