#!/usr/bin/env python3
"""Monta docs/telas.png a partir dos .ppm gerados por tools/screens.cpp
(tela 320×240 em paisagem, cantos arredondados + legenda). Requer Pillow."""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

src, out = sys.argv[1], sys.argv[2]
files = sorted(f for f in os.listdir(src) if f.endswith(".ppm"))
TW, TH, COLS, PAD, CAP = 320, 240, 5, 16, 30
rows = (len(files) + COLS - 1) // COLS
W, H = COLS * (TW + PAD) + PAD, rows * (TH + PAD + CAP) + PAD
sheet = Image.new("RGB", (W, H), (22, 26, 34))
mask = Image.new("L", (TW * 4, TH * 4), 0)
ImageDraw.Draw(mask).rounded_rectangle((0, 0, TW * 4 - 1, TH * 4 - 1), radius=40, fill=255)
mask = mask.resize((TW, TH), Image.LANCZOS)
try:
    font = ImageFont.truetype(os.path.join(os.path.dirname(__file__), "fonts", "Nunito.ttf"), 18)
    font.set_variation_by_name("Bold")
except Exception:
    font = ImageFont.load_default()
d = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    im = Image.open(os.path.join(src, f)).convert("RGB")
    x = PAD + (i % COLS) * (TW + PAD)
    y = PAD + (i // COLS) * (TH + PAD + CAP)
    d.rounded_rectangle((x - 3, y - 3, x + TW + 2, y + TH + 2), radius=13, fill=(58, 62, 70))
    sheet.paste(im, (x, y), mask)
    name = f.split("_", 1)[1].rsplit(".", 1)[0].replace("_", " ")
    tw = d.textlength(name, font=font)
    d.text((x + (TW - tw) / 2, y + TH + 5), name, fill=(200, 208, 222), font=font)
sheet.save(out, optimize=True)
print(f"OK: {out} ({len(files)} telas)")
