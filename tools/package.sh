#!/bin/bash
# Build the release zip.
#   tools/package.sh windows <bindir> <version>   -> Mercy_v<version>_Windows.zip  (VST3)
#   tools/package.sh mac     <bindir> <version>   -> Mercy_v<version>_Mac.zip      (VST3 + AU, Apple Silicon)
set -e
PLATFORM=$1; BIN=$(cd "$2" && pwd); VER=$3
ROOT=$(cd "$(dirname "$0")/.." && pwd)
STAGE=$(mktemp -d)
M="$STAGE/Mercy"
mkdir -p "$M/VST3" "$M/licenses" "$M/source"
cp -R "$BIN/Mercy.vst3" "$M/VST3/"
if [ "$PLATFORM" = "teaser" ]; then
  # the "000" teaser build: just the CLAP, its readme and the licenses
  rm -rf "$M"; M="$STAGE/000"; mkdir -p "$M/CLAP" "$M/licenses"
  cp "$BIN/000.clap" "$M/CLAP/"
  cp "$ROOT/package/TEASER_README.txt" "$M/README.txt"
  cp "$ROOT"/package/licenses/* "$M/licenses/"
  cp "$ROOT/dpf/LICENSE" "$M/licenses/DPF-ISC.txt"
  NAME="000_Windows_CLAP.zip"
  mkdir -p "$ROOT/out"; rm -f "$ROOT/out/$NAME"
  (cd "$STAGE" && zip -q -r -y "$ROOT/out/$NAME" 000)
  rm -rf "$STAGE"; echo "$ROOT/out/$NAME"; exit 0
fi
if [ "$PLATFORM" = "mac" ]; then
  mkdir -p "$M/AU"
  cp -R "$BIN/Mercy.component" "$M/AU/"
  NAME="Mercy_v${VER}_Mac.zip"
else
  NAME="Mercy_v${VER}_Windows.zip"
fi
cp "$ROOT/README.txt" "$M/"
cp "$ROOT"/package/licenses/* "$M/licenses/"
cp "$ROOT/dpf/LICENSE" "$M/licenses/DPF-ISC.txt"
cp "$ROOT"/src/*.cpp "$ROOT"/src/*.hpp "$ROOT"/src/*.h "$ROOT"/src/*.mm "$ROOT/src/Makefile" "$ROOT/BUILD.txt" "$M/source/"
rm -f "$M/source/font_bold.h" "$M/source/font_regular.h"
mkdir -p "$ROOT/out"
rm -f "$ROOT/out/$NAME"
(cd "$STAGE" && zip -q -r -y "$ROOT/out/$NAME" Mercy)
rm -rf "$STAGE"
echo "$ROOT/out/$NAME"
