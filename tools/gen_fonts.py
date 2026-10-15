#!/usr/bin/env python3
"""Gera core/font_data.cpp: fontes antialiased (4 bits de alfa por pixel) a partir
da Nunito (SIL Open Font License 1.1, tools/fonts/OFL.txt).

Uso: python3 tools/gen_fonts.py        (requer Pillow)

Cada fonte guarda só os glifos de que precisa. O resultado é C++ puro, sem
dependências: o mesmo arquivo serve ao firmware, ao simulador (WASM) e aos testes.
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TTF = os.path.join(ROOT, "tools", "fonts", "Nunito.ttf")
OUT = os.path.join(ROOT, "core", "font_data.cpp")

ASCII = "".join(chr(c) for c in range(32, 127))
PTBR = "áàâãéêíóôõúüçÁÀÂÃÉÊÍÓÔÕÚÜÇºª°·•…–—“”‘’×÷±"
FULL = ASCII + PTBR
DIGITS = "0123456789:.,%-+ ºhmsAPM°/"
CLOCK = "0123456789: "

# (nome, peso, tamanho em px, conjunto)
FONTS = [
    ("FONT_S", "Bold", 20, FULL),
    ("FONT_M", "Bold", 26, FULL),
    ("FONT_L", "ExtraBold", 34, FULL),
    ("FONT_XL", "ExtraBold", 52, FULL),
    ("FONT_XXL", "ExtraBold", 92, DIGITS),
    ("FONT_CLOCK", "Black", 150, CLOCK),
]


def build(name, weight, size, chars):
    f = ImageFont.truetype(TTF, size)
    f.set_variation_by_name(weight)
    asc, desc = f.getmetrics()
    cps = sorted(set(ord(c) for c in chars))
    glyphs, bits = [], bytearray()
    for cp in cps:
        ch = chr(cp)
        adv = int(round(f.getlength(ch)))
        x0, y0, x1, y1 = f.getbbox(ch, anchor="ls")
        w, h, ox, oy = 0, 0, 0, 0
        if x1 > x0 and y1 > y0:
            im = Image.new("L", (x1 - x0, y1 - y0), 0)
            ImageDraw.Draw(im).text((-x0, -y0), ch, font=f, fill=255, anchor="ls")
            bb = im.getbbox()
            if bb:
                im = im.crop(bb)
                w, h = im.size
                ox, oy = x0 + bb[0], y0 + bb[1]
        off = len(bits)
        px = list(im.tobytes()) if w and h else []
        nib = [min(15, (p * 15 + 127) // 255) for p in px]
        if len(nib) % 2:
            nib.append(0)
        for i in range(0, len(nib), 2):
            bits.append((nib[i] << 4) | nib[i + 1])
        if w > 255 or h > 255 or not (-128 <= ox < 128) or not (-128 <= oy < 128):
            raise SystemExit(f"glifo grande demais: {name} {ch!r}")
        glyphs.append((cp, off, w, h, ox, oy, adv))
    return asc, desc, glyphs, bits


def main():
    out = []
    out.append("// GERADO por tools/gen_fonts.py — não editar à mão.\n")
    out.append("// Fonte: Nunito (Vernon Adams, Cyreal, Jacques Le Bailly) — SIL Open Font License 1.1.\n")
    out.append('#include "font.h"\n\nnamespace tb {\n')
    total = 0
    for name, weight, size, chars in FONTS:
        asc, desc, glyphs, bits = build(name, weight, size, chars)
        total += len(bits) + len(glyphs) * 12
        low = name.lower()
        out.append(f"\n// {name}: Nunito {weight} {size}px, {len(glyphs)} glifos, {len(bits)} bytes\n")
        out.append(f"static const uint8_t {low}_bits[] = {{\n")
        for i in range(0, len(bits), 24):
            out.append("  " + ",".join(str(b) for b in bits[i:i + 24]) + ",\n")
        out.append("};\n")
        out.append(f"static const Glyph {low}_glyphs[] = {{\n")
        for cp, off, w, h, ox, oy, adv in glyphs:
            out.append(f"  {{{cp},{off},{w},{h},{ox},{oy},{adv}}},\n")
        out.append("};\n")
        out.append(f"const Font {name} = {{{low}_glyphs, {low}_bits, {len(glyphs)}, {asc}, {desc}, {size}}};\n")
    out.append("\n}  // namespace tb\n")
    with open(OUT, "w", encoding="utf-8") as fh:
        fh.write("".join(out))
    print(f"OK: {OUT} (~{total // 1024} KB de dados)")


if __name__ == "__main__":
    sys.exit(main())
