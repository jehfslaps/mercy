#!/bin/bash
# Joins the arm64 and x86_64 Mac builds into universal ones (Apple Silicon + Intel):
#   tools/mac_universal.sh bin/mac-arm64 bin/mac-x86_64 bin/mac
set -euo pipefail
A="$1"; X="$2"; U="$3"
rm -rf "$U"; mkdir -p "$U"; cp -R "$A"/. "$U"/
n=0
while IFS= read -r f; do
  rel="${f#$A/}"
  if file "$f" | grep -q "Mach-O"; then
    [ -f "$X/$rel" ] || { echo "missing Intel half: $rel"; exit 1; }
    lipo -create "$f" "$X/$rel" -output "$U/$rel"
    archs=$(lipo -archs "$U/$rel")
    case "$archs" in *arm64*x86_64*|*x86_64*arm64*) ;; *) echo "$rel is only '$archs'"; exit 1;; esac
    echo "universal: $rel ($archs)"; n=$((n+1))
  fi
done < <(find "$A" -type f)
[ "$n" -ge 4 ] || { echo "only $n binaries joined"; exit 1; }
