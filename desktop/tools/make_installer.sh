#!/usr/bin/env bash
# Signs the built plugins with Developer ID, packages them as one .pkg, notarizes it with Apple and staples the approval.
# Needs: a finished Release build in desktop/build, both Developer ID certificates in the keychain, and notarization
# credentials stored once with: xcrun notarytool store-credentials "millstone-notary" --apple-id <email> --team-id <team>
# Usage: make_installer.sh [--no-notarize]. The result is desktop/build/installer/Millstone-<version>.pkg
set -euo pipefail

TEAM=6RVUV25A85
APP_ID="Developer ID Application: Dominik Glaser ($TEAM)"
PKG_ID="Developer ID Installer: Dominik Glaser ($TEAM)"
PROFILE=millstone-notary
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
ART=$ROOT/desktop/build/Millstone_artefacts/Release
OUT=$ROOT/desktop/build/installer
NOTARIZE=1
[ "${1:-}" = "--no-notarize" ] && NOTARIZE=0

VERSION=$(sed -n 's/^project(Millstone VERSION \([0-9.]*\).*/\1/p' "$ROOT/desktop/CMakeLists.txt")
BUILT=$(plutil -extract CFBundleShortVersionString raw "$ART/VST3/Millstone.vst3/Contents/Info.plist")
[ "$VERSION" = "$BUILT" ] || { echo "The build is version $BUILT but CMakeLists.txt says $VERSION: rebuild first." >&2; exit 1; }
[ -z "$(git -C "$ROOT" status --porcelain)" ] || echo "WARNING: the working tree has uncommitted changes."

rm -rf "$OUT"
mkdir -p "$OUT/pkgs" "$OUT/scripts" "$OUT/resources" "$OUT/licences"

# The licence the installer shows, and the notices that travel inside each bundle.
sed "s/@VERSION@/$VERSION/" "$ROOT/desktop/installer/notice.txt" > "$OUT/licences/NOTICE.txt"
cp "$ROOT/LICENSES/AGPL-3.0.txt" "$ROOT/LICENSES/GPL-3.0.txt" "$ROOT/THIRD_PARTY_NOTICES.txt" "$OUT/licences/"
cat "$OUT/licences/NOTICE.txt" "$ROOT/LICENSES/AGPL-3.0.txt" > "$OUT/resources/license.txt"

# One staged root per format, each signed where it will be installed from.
stage() {   # name, bundle, install location
    local name=$1 bundle=$2 location=$3 root=$OUT/root_$1
    mkdir -p "$root"
    ditto "$bundle" "$root/$(basename "$bundle")"
    mkdir -p "$root/$(basename "$bundle")/Contents/Resources/Licenses"
    cp "$OUT"/licences/* "$root/$(basename "$bundle")/Contents/Resources/Licenses/"
    codesign --force --timestamp --options runtime --sign "$APP_ID" "$root/$(basename "$bundle")"
    codesign --verify --strict --deep "$root/$(basename "$bundle")"
    # Not relocatable: otherwise the installer "updates" any copy with this bundle ID it finds elsewhere on the disk.
    pkgbuild --analyze --root "$root" "$OUT/$name.plist" >/dev/null
    plutil -replace 0.BundleIsRelocatable -bool NO "$OUT/$name.plist"
    local scripts=()
    [ "$name" = AU ] && scripts=(--scripts "$OUT/scripts")
    pkgbuild --quiet --root "$root" --component-plist "$OUT/$name.plist" --install-location "$location" \
        --identifier "com.glaser.millstone.pkg.$name" --version "$VERSION" ${scripts[@]+"${scripts[@]}"} "$OUT/pkgs/$name.pkg"
}

# macOS only notices a new Audio Unit once its registrar has restarted.
cat > "$OUT/scripts/postinstall" <<'POST'
#!/bin/sh
killall -9 AudioComponentRegistrar 2>/dev/null
exit 0
POST
chmod +x "$OUT/scripts/postinstall"

stage VST3 "$ART/VST3/Millstone.vst3" /Library/Audio/Plug-Ins/VST3
stage AU "$ART/AU/Millstone.component" /Library/Audio/Plug-Ins/Components
stage Standalone "$ART/Standalone/Millstone.app" /Applications

cat > "$OUT/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Millstone $VERSION</title>
    <license file="license.txt" mime-type="text/plain"/>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true" enable_anywhere="false" enable_currentUserHome="false"/>
    <choices-outline>
        <line choice="VST3"/>
        <line choice="AU"/>
        <line choice="Standalone"/>
    </choices-outline>
    <choice id="VST3" title="VST3 plugin" description="For Ableton Live and other VST3 hosts."><pkg-ref id="com.glaser.millstone.pkg.VST3"/></choice>
    <choice id="AU" title="Audio Unit plugin" description="For Logic, GarageBand, Ableton Live and other AU hosts."><pkg-ref id="com.glaser.millstone.pkg.AU"/></choice>
    <choice id="Standalone" title="Standalone app" description="Plays without a host."><pkg-ref id="com.glaser.millstone.pkg.Standalone"/></choice>
    <pkg-ref id="com.glaser.millstone.pkg.VST3" version="$VERSION">VST3.pkg</pkg-ref>
    <pkg-ref id="com.glaser.millstone.pkg.AU" version="$VERSION">AU.pkg</pkg-ref>
    <pkg-ref id="com.glaser.millstone.pkg.Standalone" version="$VERSION">Standalone.pkg</pkg-ref>
</installer-gui-script>
XML

PKG=$OUT/Millstone-$VERSION.pkg
productbuild --quiet --distribution "$OUT/distribution.xml" --package-path "$OUT/pkgs" --resources "$OUT/resources" --sign "$PKG_ID" --timestamp "$PKG"
pkgutil --check-signature "$PKG" | sed -n '1,3p'

if [ "$NOTARIZE" = 1 ]; then
    xcrun notarytool submit "$PKG" --keychain-profile "$PROFILE" --wait | tee "$OUT/notary.txt"
    grep -q "status: Accepted" "$OUT/notary.txt" || { echo "Notarization was not accepted." >&2; exit 1; }
    xcrun stapler staple "$PKG"
    xcrun stapler validate "$PKG"
    spctl --assess --type install --verbose=2 "$PKG"
fi
shasum -a 256 "$PKG"
echo "DONE: $PKG"
