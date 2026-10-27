#!/usr/bin/env python3
"""Compare two folders of interface screenshots (e.g. Linux/Windows reference vs Mac).

usage: compare_screens.py <refdir> <testdir> <outdir>

Both are scaled to 960x600 (a Retina Mac renders at 2x). For each screenshot it reports how many pixels
differ noticeably and writes a side-by-side + difference image to <outdir>. Exits 1 if a screenshot is
missing or differs a lot (layout / colour / font problems); tiny edge anti-aliasing differences between
graphics drivers are expected and allowed.
"""
import os
import sys
from PIL import Image, ImageChops

ref, test, out = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(out, exist_ok=True)
names = sorted(n for n in os.listdir(ref) if n.endswith('.png'))
failed = 0
lines = ['| screenshot | pixels that differ noticeably | result |', '|---|---|---|']
for n in names:
    p = os.path.join(test, n)
    if not os.path.exists(p):
        p = os.path.join(test, n[:-4] + '.ppm')
    if not os.path.exists(p):
        lines.append(f'| {n} | missing | FAIL |')
        failed += 1
        continue
    a = Image.open(os.path.join(ref, n)).convert('RGB').resize((960, 600), Image.LANCZOS)
    b = Image.open(p).convert('RGB').resize((960, 600), Image.LANCZOS)
    diff = ImageChops.difference(a, b).convert('L')
    hist = diff.histogram()
    big = sum(hist[48:]) / (960 * 600) * 100   # differs by more than ~19%
    ok = big < 1.0
    failed += 0 if ok else 1
    lines.append(f'| {n} | {big:.3f}% | {"ok" if ok else "FAIL"} |')
    sheet = Image.new('RGB', (960 * 3, 600))
    sheet.paste(a, (0, 0))
    sheet.paste(b, (960, 0))
    sheet.paste(diff.point(lambda v: min(255, v * 4)).convert('RGB'), (1920, 0))
    sheet.save(os.path.join(out, 'compare_' + n))
report = '\n'.join(lines)
print(report)
with open(os.path.join(out, 'report.md'), 'w') as f:
    f.write('### Interface: reference vs Mac (left: reference, middle: Mac, right: difference x4)\n\n' + report + '\n')
sys.exit(1 if failed else 0)
