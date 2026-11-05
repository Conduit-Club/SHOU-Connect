#!/bin/bash
# macOS deployment script
# Usage: ./scripts/deploy-macos.sh "SHOUConnect" "build" "amd64" "false"

set -euo pipefail

TARGET_NAME="${1:-SHOUConnect}"
BUILD_DIR="${2:-build}"
ARCH="${3:-arm64}"
NIGHTLY="${4:-false}"
APP_PATH="$TARGET_NAME.app"

# Never merge a new deployment with files from an older package.
if [ -e "$APP_PATH" ]; then
    echo "Deployment already exists; use a fresh workspace: $APP_PATH" >&2
    exit 1
fi
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Copy app bundle
cp -R "$BUILD_DIR/$TARGET_NAME.app" .

# Download and extract zju-connect
ZJU_ARCH="${ARCH}"
if [ "$ARCH" = "x86_64" ]; then
    ZJU_ARCH="amd64"
fi

core_options=(fetch-core "$APP_PATH/Contents/MacOS" --platform darwin --architecture "$ZJU_ARCH")
if [ "$NIGHTLY" = "true" ]; then core_options+=(--nightly); fi
python3 "$SCRIPT_DIR/package_tools.py" "${core_options[@]}"

# Run macdeployqt
macdeployqt "$APP_PATH"

# Reduce bundle size by stripping unused slices from universal Mach-O binaries.
while IFS= read -r -d '' binary; do
    file_output="$(file "$binary")"
    if [[ "$file_output" != *"Mach-O universal binary"* ]]; then
        continue
    fi

    lipo_info="$(lipo -info "$binary" 2>/dev/null || true)"
    if [[ "$lipo_info" != *"$ARCH"* ]]; then
        echo "Skipping universal binary without $ARCH slice: $binary" >&2
        continue
    fi

    original_mode="$(stat -f%Lp "$binary")"
    thin_output="$(mktemp)"
    lipo "$binary" -thin "$ARCH" -output "$thin_output"
    chmod "$original_mode" "$thin_output"
    mv "$thin_output" "$binary"

done < <(find "$APP_PATH" -type f -print0)

# macdeployqt has a bug that it doesn't copy the Qt translations to the bundle
# so we need to copy them manually
QT_TRANSLATIONS=$(qmake -query QT_INSTALL_TRANSLATIONS)
mkdir -p "$APP_PATH/Contents/translations"
if [ -d "$QT_TRANSLATIONS" ]; then
    for language in zh_CN zh_TW en; do
        translation="$QT_TRANSLATIONS/qtbase_$language.qm"
        if [ -f "$translation" ]; then cp "$translation" "$APP_PATH/Contents/translations/"; fi
    done
fi

# Prune before final signing; macdeployqt may have ad-hoc signed nested code.
python3 "$SCRIPT_DIR/package_tools.py" prune "$APP_PATH" --output package-pruning.json
# Re-sign Mach-O files and nested bundles from the inside out after thinning/pruning.
# This preserves existing entitlements, including the WebEngine helper's.
while IFS= read -r -d '' binary; do
    if file "$binary" | grep -q 'Mach-O'; then
        if codesign -d "$binary" >/dev/null 2>&1; then
            codesign --force --sign - --preserve-metadata=identifier,entitlements,flags "$binary"
        else
            codesign --force --sign - "$binary"
        fi
    fi
done < <(find "$APP_PATH" -type f -print0)
while IFS= read -r -d '' bundle; do
    codesign --force --sign - --preserve-metadata=identifier,entitlements,flags "$bundle"
done < <(find "$APP_PATH" -depth -type d \( -name '*.framework' -o -name '*.app' -o -name '*.bundle' \) -print0)
codesign --verify --deep --strict "$APP_PATH"
