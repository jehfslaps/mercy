#!/bin/bash
# usage: shots.sh outdir "mode:dark ..." [scale]
cd /home/claude/mercy/test
OUT=$1; shift; MODES=$1; SC=${2:-1}
mkdir -p $OUT
Xvfb :99 -ac -screen 0 2200x1400x24 +extension GLX -nolisten tcp >/dev/null 2>&1 &
XP=$!; sleep 1
for m in $MODES; do mode=${m%%:*}; dk=${m##*:}
  DISPLAY=:99 timeout 60 ./shot $mode $OUT/s_${mode}_${dk}.ppm $SC $dk 2>/dev/null
  python3 -c "from PIL import Image; Image.open('$OUT/s_${mode}_${dk}.ppm').save('$OUT/s_${mode}_${dk}.png')" && rm $OUT/s_${mode}_${dk}.ppm
done
kill $XP
