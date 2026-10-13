// Framebuffer monocromático 128×64, 1 bit por pixel, no MESMO layout de páginas
// do SSD1306 (8 páginas × 128 colunas; bit 0 = linha de cima de cada página).
// O firmware envia este buffer diretamente ao OLED; o simulador desenha os
// mesmos bits ampliados no canvas.
#pragma once
#include "tb_base.h"

namespace tb {

static const int FB_W = 128;
static const int FB_H = 64;
static const int FB_BYTES = FB_W * FB_H / 8;  // 1024

class Framebuffer {
 public:
  uint8_t buf[FB_BYTES];

  void clear() { tbMemset(buf, 0, sizeof buf); }
  void fill() { tbMemset(buf, 0xFF, sizeof buf); }

  inline void set(int x, int y, bool on = true) {
    if ((unsigned)x >= (unsigned)FB_W || (unsigned)y >= (unsigned)FB_H) return;
    uint8_t& b = buf[(y >> 3) * FB_W + x];
    uint8_t m = (uint8_t)(1u << (y & 7));
    if (on) b |= m; else b &= (uint8_t)~m;
  }
  inline bool get(int x, int y) const {
    if ((unsigned)x >= (unsigned)FB_W || (unsigned)y >= (unsigned)FB_H) return false;
    return (buf[(y >> 3) * FB_W + x] >> (y & 7)) & 1;
  }
  void invert(int x, int y) { set(x, y, !get(x, y)); }

  void hline(int x, int y, int w, bool on = true);
  void vline(int x, int y, int h, bool on = true);
  void line(int x0, int y0, int x1, int y1, bool on = true);
  void rect(int x, int y, int w, int h, bool on = true);
  void fillRect(int x, int y, int w, int h, bool on = true);
  void invertRect(int x, int y, int w, int h);
  void roundRect(int x, int y, int w, int h, int r, bool on = true);
  void fillRoundRect(int x, int y, int w, int h, int r, bool on = true);
  void circle(int cx, int cy, int r, bool on = true);
  void fillCircle(int cx, int cy, int r, bool on = true);
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, bool on = true);
  void fillEllipse(int cx, int cy, int rx, int ry, bool on = true);
  // Bitmap em linhas: cada linha usa (w+7)/8 bytes, MSB = pixel da esquerda.
  void blitRows(int x, int y, int w, int h, const uint8_t* rows, bool on = true, bool flipX = false);
  void heart(int cx, int cy, int size, bool on = true);  // size: meia largura aproximada
  void progressBar(int x, int y, int w, int h, int permille);

  // Texto: fonte 5×7 própria (célula 6×8 por escala), UTF-8 com acentos pt-BR.
  // Retorna a largura desenhada em pixels.
  int text(int x, int y, const char* s, int scale = 1, bool on = true);
  static int textWidth(const char* s, int scale = 1);
  // Centraliza no eixo X.
  int textCentered(int y, const char* s, int scale = 1, bool on = true);
  // Maior escala (1..maxScale) na qual o texto cabe em maxW.
  static int fitScale(const char* s, int maxW, int maxScale);
};

}  // namespace tb
