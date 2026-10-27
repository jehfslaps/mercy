#!/bin/bash
# Take interface screenshots with test/shot.cpp (the real MercyView, drawn by DPF/NanoVG).
#   tools/screenshots.sh linux <outdir>   (needs test/libdgl-linux.a from the Linux build, runs under Xvfb)
#   tools/screenshots.sh mac   <outdir>   (needs dpf/build-mac/libdgl-opengl.a from the Mac build)
set -e
PLATFORM=$1
OUT=$(mkdir -p "$2" && cd "$2" && pwd)
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT/test"
FLAGS="-std=gnu++17 -O1 -fno-fast-math -DDGL_OPENGL -DDGL_USE_FILE_BROWSER=1 -DDGL_NAMESPACE=DGL -DHAVE_OPENGL -I../dpf/dgl -I../dpf/distrho -I../src"
# mode:dark  (see shot.cpp: 0 main, 1 advanced, 3 yeezus, 11 throne, 4 preset list, 5 module presets,
#             8 bake panel, 13 save-name box, 15 right-click menu)
MODES="0:1 0:0 1:1 1:0 3:1 11:1 4:1 5:1 8:1 8:0 13:1 15:1"

if [ "$PLATFORM" = "linux" ]; then
  g++ $FLAGS shot.cpp ../src/MercyBake.cpp libdgl-linux.a -o shot -lGL -lX11 -lXext -lXrandr -lXcursor -lpthread -ldl
  RUN="xvfb-run -a -s '-screen 0 2200x1400x24 +extension GLX'"
elif [ "$PLATFORM" = "mac" ]; then
  LIB=../dpf/build-mac/libdgl-opengl.a; [ -f "$LIB" ] || LIB=../dpf/build/libdgl-opengl.a
  clang++ $FLAGS -Wno-deprecated-declarations shot.cpp ../src/MercyBake.cpp "$LIB" -o shot \
    -framework Cocoa -framework OpenGL -framework CoreVideo
  RUN=""
else
  echo "usage: $0 linux|mac outdir"; exit 2
fi

for m in $MODES; do
  mode=${m%%:*}; dk=${m##*:}
  eval $RUN ./shot $mode "$OUT/s_${mode}_${dk}.ppm" 1 $dk || echo "screenshot $m failed"
  # convert to png when Pillow is there (the Mac runner keeps the .ppm; the compare job reads either)
  if [ -f "$OUT/s_${mode}_${dk}.ppm" ] && python3 -c "import PIL" 2>/dev/null; then
    python3 -c "from PIL import Image; Image.open('$OUT/s_${mode}_${dk}.ppm').save('$OUT/s_${mode}_${dk}.png')"
    rm "$OUT/s_${mode}_${dk}.ppm"
  fi
done
ls "$OUT"
