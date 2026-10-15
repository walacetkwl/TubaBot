// Fontes antialiased (Nunito, 4 bits de alfa). Dados em font_data.cpp,
// gerados por tools/gen_fonts.py.
#pragma once
#include "tb_base.h"

namespace tb {

struct Glyph {
  uint32_t cp;      // codepoint Unicode
  uint32_t off;     // deslocamento em bytes nos dados
  uint8_t w, h;     // tamanho do bitmap
  int8_t ox, oy;    // posição do bitmap em relação à origem (esquerda, linha de base)
  uint8_t adv;      // avanço horizontal
};

struct Font {
  const Glyph* glyphs;
  const uint8_t* bits;
  uint16_t count;
  uint8_t ascent, descent;
  uint8_t size;
  const Glyph* find(uint32_t cp) const;
};

extern const Font FONT_S, FONT_M, FONT_L, FONT_XL, FONT_XXL, FONT_CLOCK;

}  // namespace tb
