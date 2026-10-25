"""Stack same-width spritesheets vertically into one sheet.

usage: merge_spritesheets.py OUT.png IN1.png IN2.png ... [--cell WxH]

Prints each input's `first` cell index (row-major) for ASGE's Create Clip dialog.
Without --cell, the cell size is assumed to be a square of height/3 of the first input.
"""
import argparse
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("out")
ap.add_argument("inputs", nargs="+")
ap.add_argument("--cell", help="cell size WxH, e.g. 170x170")
args = ap.parse_args()

imgs = [Image.open(p).convert("RGBA") for p in args.inputs]
width = imgs[0].width
if any(i.width != width for i in imgs):
    raise SystemExit("all inputs must have the same width: " + ", ".join(f"{p}={i.width}" for p, i in zip(args.inputs, imgs)))

cw, ch = map(int, args.cell.split("x")) if args.cell else (imgs[0].height // 3,) * 2
if width % cw or any(i.height % ch for i in imgs):
    raise SystemExit(f"sheet sizes aren't a multiple of the {cw}x{ch} cell")

sheet = Image.new("RGBA", (width, sum(i.height for i in imgs)))
y = 0
cols = width // cw
for path, img in zip(args.inputs, imgs):
    sheet.paste(img, (0, y))
    print(f"{path}: first = {y // ch * cols}, rows = {img.height // ch}")
    y += img.height
sheet.save(args.out)
print(f"wrote {args.out} ({sheet.width}x{sheet.height}, {cols} columns, {y // ch} rows)")
