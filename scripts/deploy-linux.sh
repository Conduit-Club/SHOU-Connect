#!/bin/bash
# Linux deployment script
# Usage: ./scripts/deploy-linux.sh "SHOUConnect" "SHOUConnect" "build" "x86_64" "false"

set -euo pipefail

TARGET_NAME="${1:-SHOUConnect}"
DISPLAY_NAME="${2:-SHOUConnect}"
BUILD_DIR="${3:-build}"
ARCH="${4:-x86_64}"
NIGHTLY="${5:-false}"

# Determine AppImage tool architecture
if [ "$ARCH" = "x86_64" ]; then
    APPIMAGE_ARCH="x86_64"
else
    APPIMAGE_ARCH="aarch64"
fi

# Install dependencies
sudo apt-get install -y libxcb-cursor0 fuse

# Download linuxdeploy tools if not present
if [ ! -f linuxdeploy ]; then
    wget -O linuxdeploy "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$APPIMAGE_ARCH.AppImage"
fi

if [ ! -f linuxdeploy-plugin-qt ]; then
    wget -O linuxdeploy-plugin-qt "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$APPIMAGE_ARCH.AppImage"
fi

chmod a+x linuxdeploy linuxdeploy-plugin-qt

# A fresh AppDir avoids keeping stale dependencies from an earlier build.
if [ -e AppDir ]; then
    echo 'AppDir already exists; use a fresh workspace.' >&2
    exit 1
fi
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Create AppDir structure
mkdir -p AppDir/usr/bin AppDir/usr/lib AppDir/usr/share/applications AppDir/usr/share/icons/hicolor/scalable/apps

# Copy executable
cp "$BUILD_DIR/$TARGET_NAME" AppDir/usr/bin/

# Copy libnss3 libraries (for QtWebEngine in Ubuntu 22.04)
# Ubuntu 24.04 move them to /usr/lib/aarch64-linux-gnu or /usr/lib/x86_64-linux-gnu, so no need to special case them
if [ -d "/usr/lib/$APPIMAGE_ARCH-linux-gnu/nss" ]; then
    cp -R "/usr/lib/$APPIMAGE_ARCH-linux-gnu/nss/" AppDir/usr/lib/
fi

# Download and extract zju-connect
ZJU_ARCH="${ARCH}"
if [ "$ARCH" = "x86_64" ]; then
    ZJU_ARCH="amd64"
fi

core_options=()
if [ "$NIGHTLY" = "true" ]; then core_options+=(--nightly); fi
python3 "$SCRIPT_DIR/package_tools.py" fetch-core AppDir/usr/bin \
    --platform linux --architecture "$ZJU_ARCH" "${core_options[@]}"

# Copy icon
cp resource/icon.png "AppDir/usr/share/icons/hicolor/scalable/apps/$TARGET_NAME.png"

# Create desktop file
cat > "AppDir/usr/share/applications/$TARGET_NAME.desktop" <<EOF
[Desktop Entry]
Name=$DISPLAY_NAME
Exec=$TARGET_NAME
Icon=$TARGET_NAME
Type=Application
Categories=Network;Security;
EOF

# Build AppImage
export EXTRA_PLATFORM_PLUGINS="libqwayland.so"
# Deploy first, prune translations, then seal the AppImage without redeploying Qt.
./linuxdeploy --appdir AppDir --plugin qt
python3 "$SCRIPT_DIR/package_tools.py" prune AppDir --output package-pruning.json
./linuxdeploy --appdir AppDir --output appimage
