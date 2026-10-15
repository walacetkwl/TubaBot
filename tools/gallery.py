#!/usr/bin/env python3
"""Monta docs/telas.png a partir dos .ppm gerados por tools/screens.cpp
(máscara redonda como no AMOLED + legenda). Requer Pillow."""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

src, out = sys.argv[1], sys.argv[2]
files = sorted(f for f in os.listdir(src) if f.endswith(".ppm"))
T, COLS, PAD, CAP = 300, 5, 18, 34
rows = (len(files) + COLS - 1) // COLS
W, H = COLS * (T + PAD) + PAD, rows * (T + PAD + CAP) + PAD
sheet = Image.new("RGB", (W, H), (22, 26, 34))
mask = Image.new("L", (T * 4, T * 4), 0)
ImageDraw.Draw(mask).ellipse((0, 0, T * 4 - 1, T * 4 - 1), fill=255)
mask = mask.resize((T, T), Image.LANCZOS)
try:
    font = ImageFont.truetype(os.path.join(os.path.dirname(__file__), "fonts", "Nunito.ttf"), 20)
    font.set_variation_by_name("Bold")
except Exception:
    font = ImageFont.load_default()
d = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    im = Image.open(os.path.join(src, f)).convert("RGB").resize((T, T), Image.LANCZOS)
    x = PAD + (i % COLS) * (T + PAD)
    y = PAD + (i // COLS) * (T + PAD + CAP)
    sheet.paste(im, (x, y), mask)
    name = f.split("_", 1)[1].rsplit(".", 1)[0].replace("_", " ")
    tw = d.textlength(name, font=font)
    d.text((x + (T - tw) / 2, y + T + 6), name, fill=(200, 208, 222), font=font)
sheet.save(out, optimize=True)
print(f"OK: {out} ({len(files)} telas)")
