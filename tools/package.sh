#!/bin/bash
# Build the release zip.
#   tools/package.sh windows <bindir> <version>   -> Mercy_v<version>_Windows.zip  (Install MERCY.exe; VST3 + CLAP)
#   tools/package.sh mac     <bindir> <version>   -> Mercy_v<version>_Mac.zip      (Install MERCY.pkg; AU + VST3 + CLAP, Apple Silicon)
# Both zips also carry the manual, the readme, the licenses and the plain plugin files ("manual install").
#   tools/package.sh teaser  <bindir> <version>   -> 000_Windows.zip              (VST3 + CLAP)
set -e
PLATFORM=$1; BIN=$(cd "$2" && pwd); VER=$3
ROOT=$(cd "$(dirname "$0")/.." && pwd)
STAGE=$(mktemp -d)
M="$STAGE/Mercy"
mkdir -p "$M/VST3" "$M/CLAP" "$M/licenses" "$M/source"
cp -R "$BIN/Mercy.vst3" "$M/VST3/"
[ "$PLATFORM" = "teaser" ] || cp -R "$BIN/Mercy.clap" "$M/CLAP/"   # a folder (bundle) on Mac
if [ "$PLATFORM" = "teaser" ]; then
  # the "000" teaser build: just the CLAP, its readme and the licenses
  rm -rf "$M"; M="$STAGE/000"; mkdir -p "$M/VST3" "$M/CLAP" "$M/licenses"
  cp -R "$BIN/000.vst3" "$M/VST3/"
  cp -R "$BIN/000.clap" "$M/CLAP/"
  cp "$ROOT/package/TEASER_README.txt" "$M/README.txt"
  cp "$ROOT"/package/licenses/* "$M/licenses/"
  cp "$ROOT/dpf/LICENSE" "$M/licenses/DPF-ISC.txt"
  NAME="000_Windows.zip"
  mkdir -p "$ROOT/out"; rm -f "$ROOT/out/$NAME"
  (cd "$STAGE" && zip -q -r -y "$ROOT/out/$NAME" 000)
  rm -rf "$STAGE"; echo "$ROOT/out/$NAME"; exit 0
fi
# ---- MERCY: the installer up front; the plain plugin files in "manual install" for people who copy by hand
mkdir -p "$M/manual install"
mv "$M/VST3" "$M/CLAP" "$M/manual install/"
cp "$ROOT/README.txt" "$M/"
cp "$ROOT/package/MERCY_User_Manual.pdf" "$M/"
cp "$ROOT"/package/licenses/* "$M/licenses/"
cp "$ROOT/dpf/LICENSE" "$M/licenses/DPF-ISC.txt"
cp "$ROOT"/src/*.cpp "$ROOT"/src/*.hpp "$ROOT"/src/*.h "$ROOT"/src/*.mm "$ROOT/src/Makefile" "$ROOT/BUILD.txt" "$M/source/"
rm -f "$M/source/font_bold.h" "$M/source/font_regular.h"
I="$ROOT/installer"
if [ "$PLATFORM" = "mac" ]; then
  mkdir -p "$M/manual install/AU"
  cp -R "$BIN/Mercy.component" "$M/manual install/AU/"
  NAME="Mercy_v${VER}_Mac.zip"
  # Install MERCY.pkg: one component package per format (choices), installed for all users in /Library/Audio/Plug-Ins
  P=$(mktemp -d)
  for f in au:Components:"$BIN/Mercy.component" vst3:VST3:"$BIN/Mercy.vst3" clap:CLAP:"$BIN/Mercy.clap"; do
    id=${f%%:*}; rest=${f#*:}; dir=${rest%%:*}; src=${rest#*:}
    mkdir -p "$P/root-$id"; cp -R "$src" "$P/root-$id/"
    pkgbuild --analyze --root "$P/root-$id" "$P/$id.plist" >/dev/null
    plutil -replace 0.BundleIsRelocatable -bool NO "$P/$id.plist"   # always install where we say, never "move" an old copy
    pkgbuild --quiet --root "$P/root-$id" --component-plist "$P/$id.plist" --scripts "$I/mac/scripts-$id" \
      --identifier "com.slapworx.mercy.$id" --version "$VER" --install-location "/Library/Audio/Plug-Ins/$dir" "$P/mercy_$id.pkg"
  done
  mkdir -p "$P/res"
  for r in welcome conclusion; do sed "s/@VERSION@/$VER/g" "$I/mac/resources/$r.html" > "$P/res/$r.html"; done
  sed "s/@VERSION@/$VER/g" "$I/mac/distribution.xml" > "$P/distribution.xml"
  SIGN=(); [ -n "$MAC_INSTALLER_IDENTITY" ] && SIGN=(--sign "$MAC_INSTALLER_IDENTITY")   # "Developer ID Installer: ..." once Apple is set up
  productbuild --quiet --distribution "$P/distribution.xml" --resources "$P/res" --package-path "$P" "${SIGN[@]}" "$M/Install MERCY.pkg"
  rm -rf "$P"
  # notarize: Apple checks the installer (and every plugin inside it); then the tickets are stapled on, so
  # Macs open it without asking, even offline. The first notarization on a new account can take a while:
  # if Apple hasn't answered within 45 minutes the zip still goes out signed, and Macs check online instead.
  mkdir -p "$ROOT/out"
  if [ -n "$MAC_INSTALLER_IDENTITY" ] && [ -n "$APPLE_ID" ] && [ -n "$APPLE_APP_PASSWORD" ] && [ -n "$APPLE_TEAM_ID" ]; then
    if xcrun notarytool submit "$M/Install MERCY.pkg" --apple-id "$APPLE_ID" --password "$APPLE_APP_PASSWORD" \
         --team-id "$APPLE_TEAM_ID" --wait --timeout 45m | tee "$ROOT/out/notarize.log" && grep -q "status: Accepted" "$ROOT/out/notarize.log"; then
      xcrun stapler staple "$M/Install MERCY.pkg"
      for b in AU/Mercy.component VST3/Mercy.vst3 CLAP/Mercy.clap; do xcrun stapler staple "$M/manual install/$b" || true; done
      echo "notarized and stapled"
    else
      echo "notarization not finished (see out/notarize.log); shipping signed, not stapled"
    fi
  fi
else
  NAME="Mercy_v${VER}_Windows.zip"
  cp "$ROOT/package/Windows warning - how to install.png" "$M/"   # the SmartScreen "Run anyway" picture
  # Install MERCY.exe (NSIS): VST3 + CLAP into Common Files, an uninstaller, the manual in the Start menu
  P=$(mktemp -d)
  cp -R "$BIN/Mercy.vst3" "$BIN/Mercy.clap" "$P/"
  cp "$M/README.txt" "$M/MERCY_User_Manual.pdf" "$P/"; cp -R "$M/licenses" "$P/"
  NUM=$(echo "$VER" | grep -oE '^[0-9]+(\.[0-9]+)*'); while [ "$(echo "$NUM" | tr -cd . | wc -c)" -lt 3 ]; do NUM="$NUM.0"; done
  (cd "$I" && makensis -V2 -DVERSION="$VER" -DNUMVER="$NUM" -DSTAGE="$P" -DOUTFILE="$M/Install MERCY.exe" mercy.nsi)
  rm -rf "$P"
fi
mkdir -p "$ROOT/out"
rm -f "$ROOT/out/$NAME"
(cd "$STAGE" && zip -q -r -y "$ROOT/out/$NAME" Mercy)
rm -rf "$STAGE"
echo "$ROOT/out/$NAME"
