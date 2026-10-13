#include "framebuffer.h"

namespace tb {

// ── Fonte 5×7 (colunas; bit 0 = linha de cima) ─────────────────────────────
// Tabela clássica de domínio público para ASCII 32..126, digitada para o TubaBot.
static const uint8_t FONT5X7[95][5] = {
  {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
  {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
  {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x08,0x2A,0x1C,0x2A,0x08},{0x08,0x08,0x3E,0x08,0x08},
  {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
  {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
  {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
  {0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},
  {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x01,0x01},{0x3E,0x41,0x41,0x51,0x32},
  {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
  {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x04,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
  {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x7F,0x20,0x18,0x20,0x7F},
  {0x63,0x14,0x08,0x14,0x63},{0x03,0x04,0x78,0x04,0x03},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
  {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
  {0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},
  {0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x08,0x14,0x54,0x54,0x3C},
  {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},{0x00,0x7F,0x10,0x28,0x44},
  {0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
  {0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
  {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
  {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},
  {0x00,0x00,0x7F,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x08,0x04,0x08,0x10,0x08},
};
static const uint8_t GLYPH_DOTLESS_I[5] = {0x00,0x44,0x7C,0x40,0x00};
static const uint8_t GLYPH_DEGREE[5] = {0x00,0x06,0x09,0x09,0x06};

enum Accent : uint8_t { AC_NONE, AC_ACUTE, AC_GRAVE, AC_CIRC, AC_TILDE, AC_DIAER, AC_CEDIL };

struct GlyphInfo { const uint8_t* cols; uint8_t accent; bool upper; };

static GlyphInfo glyphFor(uint32_t cp) {
  GlyphInfo g{FONT5X7['?' - 32], AC_NONE, false};
  if (cp >= 32 && cp <= 126) { g.cols = FONT5X7[cp - 32]; return g; }
  if (cp == 0xB0) { g.cols = GLYPH_DEGREE; return g; }
  // Latin-1: mapeia letra base + acento
  struct M { uint16_t cp; char base; uint8_t ac; };
  static const M map[] = {
    {0xE0,'a',AC_GRAVE},{0xE1,'a',AC_ACUTE},{0xE2,'a',AC_CIRC},{0xE3,'a',AC_TILDE},{0xE4,'a',AC_DIAER},
    {0xE7,'c',AC_CEDIL},{0xE8,'e',AC_GRAVE},{0xE9,'e',AC_ACUTE},{0xEA,'e',AC_CIRC},{0xEB,'e',AC_DIAER},
    {0xEC,'i',AC_GRAVE},{0xED,'i',AC_ACUTE},{0xEE,'i',AC_CIRC},{0xEF,'i',AC_DIAER},{0xF1,'n',AC_TILDE},
    {0xF2,'o',AC_GRAVE},{0xF3,'o',AC_ACUTE},{0xF4,'o',AC_CIRC},{0xF5,'o',AC_TILDE},{0xF6,'o',AC_DIAER},
    {0xF9,'u',AC_GRAVE},{0xFA,'u',AC_ACUTE},{0xFB,'u',AC_CIRC},{0xFC,'u',AC_DIAER},
    {0xC0,'A',AC_GRAVE},{0xC1,'A',AC_ACUTE},{0xC2,'A',AC_CIRC},{0xC3,'A',AC_TILDE},{0xC4,'A',AC_DIAER},
    {0xC7,'C',AC_CEDIL},{0xC8,'E',AC_GRAVE},{0xC9,'E',AC_ACUTE},{0xCA,'E',AC_CIRC},{0xCB,'E',AC_DIAER},
    {0xCC,'I',AC_GRAVE},{0xCD,'I',AC_ACUTE},{0xCE,'I',AC_CIRC},{0xCF,'I',AC_DIAER},{0xD1,'N',AC_TILDE},
    {0xD2,'O',AC_GRAVE},{0xD3,'O',AC_ACUTE},{0xD4,'O',AC_CIRC},{0xD5,'O',AC_TILDE},{0xD6,'O',AC_DIAER},
    {0xD9,'U',AC_GRAVE},{0xDA,'U',AC_ACUTE},{0xDB,'U',AC_CIRC},{0xDC,'U',AC_DIAER},
  };
  for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++) {
    if (map[i].cp == cp) {
      g.accent = map[i].ac;
      g.upper = map[i].base >= 'A' && map[i].base <= 'Z';
      g.cols = (map[i].base == 'i' && map[i].ac != AC_CEDIL) ? GLYPH_DOTLESS_I : FONT5X7[map[i].base - 32];
      return g;
    }
  }
  return g;
}

static void drawAccent(Framebuffer& fb, int x, int y, int sc, uint8_t ac, bool upper, bool on) {
  // Minúsculas: acento nas linhas 0–1 da célula (livres). Maiúsculas: acima da célula.
  int r0 = upper ? -2 : 0;
  struct P { int8_t c, r; };
  static const P acute[] = {{2,1},{3,0}};
  static const P grave[] = {{1,0},{2,1}};
  static const P circ[]  = {{1,1},{2,0},{3,1}};
  static const P tilde[] = {{0,1},{1,0},{2,1},{3,0}};
  static const P diaer[] = {{1,1},{3,1}};
  const P* pts = nullptr; int n = 0;
  switch (ac) {
    case AC_ACUTE: pts = acute; n = 2; break;
    case AC_GRAVE: pts = grave; n = 2; break;
    case AC_CIRC: pts = circ; n = 3; break;
    case AC_TILDE: pts = tilde; n = 4; break;
    case AC_DIAER: pts = diaer; n = 2; break;
    case AC_CEDIL: fb.fillRect(x + 2 * sc, y + 7 * sc, sc, sc, on); fb.fillRect(x + 1 * sc, y + 8 * sc - (sc > 1 ? 0 : 0), sc, sc, on); return;
    default: return;
  }
  for (int i = 0; i < n; i++) fb.fillRect(x + pts[i].c * sc, y + (r0 + pts[i].r) * sc, sc, sc, on);
}

int Framebuffer::text(int x, int y, const char* s, int scale, bool on) {
  if (!s) return 0;
  int x0 = x;
  const char* p = s; const char* e = s + tbStrlen(s);
  while (p < e) {
    uint32_t cp = utf8Next(p, e);
    if (cp == '\n') { x = x0; y += 9 * scale; continue; }
    GlyphInfo g = glyphFor(cp);
    for (int c = 0; c < 5; c++) {
      uint8_t col = g.cols[c];
      for (int r = 0; r < 7; r++)
        if (col & (1u << r)) {
          if (scale == 1) set(x + c, y + r, on);
          else fillRect(x + c * scale, y + r * scale, scale, scale, on);
        }
    }
    if (g.accent) drawAccent(*this, x, y, scale, g.accent, g.upper, on);
    x += 6 * scale;
  }
  return x - x0;
}

int Framebuffer::textWidth(const char* s, int scale) {
  if (!s) return 0;
  int n = 0, best = 0;
  const char* p = s; const char* e = s + tbStrlen(s);
  while (p < e) {
    uint32_t cp = utf8Next(p, e);
    if (cp == '\n') { best = tbMax(best, n); n = 0; continue; }
    n++;
  }
  best = tbMax(best, n);
  return best ? best * 6 * scale - scale : 0;  // sem espaço após o último caractere
}

int Framebuffer::textCentered(int y, const char* s, int scale, bool on) {
  int w = textWidth(s, scale);
  return text((FB_W - w) / 2, y, s, scale, on);
}

int Framebuffer::fitScale(const char* s, int maxW, int maxScale) {
  for (int sc = maxScale; sc > 1; sc--) if (textWidth(s, sc) <= maxW) return sc;
  return 1;
}

// ── Primitivas ──────────────────────────────────────────────────────────────
void Framebuffer::hline(int x, int y, int w, bool on) {
  if (w < 0) { x += w + 1; w = -w; }
  if (y < 0 || y >= FB_H) return;
  int x1 = tbMin(x + w, FB_W);
  for (int i = tbMax(x, 0); i < x1; i++) set(i, y, on);
}
void Framebuffer::vline(int x, int y, int h, bool on) {
  if (h < 0) { y += h + 1; h = -h; }
  if (x < 0 || x >= FB_W) return;
  int y1 = tbMin(y + h, FB_H);
  for (int j = tbMax(y, 0); j < y1; j++) set(x, j, on);
}
void Framebuffer::line(int x0, int y0, int x1, int y1, bool on) {
  int dx = tbAbs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -tbAbs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    set(x0, y0, on);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}
void Framebuffer::rect(int x, int y, int w, int h, bool on) {
  if (w <= 0 || h <= 0) return;
  hline(x, y, w, on); hline(x, y + h - 1, w, on);
  vline(x, y, h, on); vline(x + w - 1, y, h, on);
}
void Framebuffer::fillRect(int x, int y, int w, int h, bool on) {
  if (w <= 0 || h <= 0) return;
  for (int j = tbMax(y, 0); j < tbMin(y + h, FB_H); j++) hline(x, j, w, on);
}
void Framebuffer::invertRect(int x, int y, int w, int h) {
  for (int j = tbMax(y, 0); j < tbMin(y + h, FB_H); j++)
    for (int i = tbMax(x, 0); i < tbMin(x + w, FB_W); i++) invert(i, j);
}
void Framebuffer::circle(int cx, int cy, int r, bool on) {
  int x = r, y = 0, err = 1 - r;
  while (x >= y) {
    set(cx + x, cy + y, on); set(cx + y, cy + x, on); set(cx - y, cy + x, on); set(cx - x, cy + y, on);
    set(cx - x, cy - y, on); set(cx - y, cy - x, on); set(cx + y, cy - x, on); set(cx + x, cy - y, on);
    y++;
    if (err < 0) err += 2 * y + 1; else { x--; err += 2 * (y - x) + 1; }
  }
}
void Framebuffer::fillCircle(int cx, int cy, int r, bool on) {
  // pixel (dx,dy) dentro se dx²+dy² <= r² + r (critério "meio pixel", simétrico)
  int rr = r * r + r;
  for (int dy = -r; dy <= r; dy++) {
    int w = 0;
    while ((w + 1) * (w + 1) + dy * dy <= rr) w++;
    hline(cx - w, cy + dy, 2 * w + 1, on);
  }
}
void Framebuffer::fillEllipse(int cx, int cy, int rx, int ry, bool on) {
  if (rx <= 0 || ry <= 0) return;
  int64_t a2 = (int64_t)rx * rx, b2 = (int64_t)ry * ry;
  for (int dy = -ry; dy <= ry; dy++) {
    // x² / a² + y² / b² <= 1  ->  x² <= a² (1 - y²/b²)
    int w = 0;
    while ((int64_t)(w + 1) * (w + 1) * b2 + (int64_t)dy * dy * a2 <= a2 * b2 + a2 / 2) w++;
    hline(cx - w, cy + dy, 2 * w + 1, on);
  }
}
void Framebuffer::roundRect(int x, int y, int w, int h, int r, bool on) {
  r = tbMin(r, tbMin(w, h) / 2);
  hline(x + r, y, w - 2 * r, on); hline(x + r, y + h - 1, w - 2 * r, on);
  vline(x, y + r, h - 2 * r, on); vline(x + w - 1, y + r, h - 2 * r, on);
  int f = 1 - r, ddx = 1, ddy = -2 * r, px = 0, py = r;
  while (px < py) {
    if (f >= 0) { py--; ddy += 2; f += ddy; }
    px++; ddx += 2; f += ddx;
    set(x + w - 1 - r + px, y + r - py, on); set(x + w - 1 - r + py, y + r - px, on);
    set(x + r - px, y + r - py, on); set(x + r - py, y + r - px, on);
    set(x + w - 1 - r + px, y + h - 1 - r + py, on); set(x + w - 1 - r + py, y + h - 1 - r + px, on);
    set(x + r - px, y + h - 1 - r + py, on); set(x + r - py, y + h - 1 - r + px, on);
  }
}
void Framebuffer::fillRoundRect(int x, int y, int w, int h, int r, bool on) {
  if (w <= 0 || h <= 0) return;
  r = tbMax(0, tbMin(r, tbMin(w, h) / 2));
  for (int j = 0; j < h; j++) {
    int inset = 0;
    int dy = -1;
    if (j < r) dy = r - j;
    else if (j >= h - r) dy = j - (h - 1 - r);
    if (dy > 0) {
      // largura do arco nessa linha
      int rr = r * r;
      int dx = 0;
      while ((dx + 1) * (dx + 1) + (dy - 0) * (dy - 0) <= rr) dx++;
      inset = r - dx;
    }
    hline(x + inset, y + j, w - 2 * inset, on);
  }
}
void Framebuffer::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, bool on) {
  // ordena por y
  if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y1 > y2) { int t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
  if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y2 == y0) { int a = tbMin(x0, tbMin(x1, x2)), b = tbMax(x0, tbMax(x1, x2)); hline(a, y0, b - a + 1, on); return; }
  for (int y = y0; y <= y2; y++) {
    // aresta longa 0-2
    int xa = x0 + (int)((int64_t)(x2 - x0) * (y - y0) / (y2 - y0));
    int xb;
    if (y < y1) xb = (y1 == y0) ? x1 : x0 + (int)((int64_t)(x1 - x0) * (y - y0) / (y1 - y0));
    else xb = (y2 == y1) ? x1 : x1 + (int)((int64_t)(x2 - x1) * (y - y1) / (y2 - y1));
    if (xa > xb) { int t = xa; xa = xb; xb = t; }
    hline(xa, y, xb - xa + 1, on);
  }
}
void Framebuffer::blitRows(int x, int y, int w, int h, const uint8_t* rows, bool on, bool flipX) {
  int bpr = (w + 7) / 8;
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      if (rows[j * bpr + i / 8] & (0x80 >> (i & 7))) set(x + (flipX ? w - 1 - i : i), y + j, on);
}
void Framebuffer::heart(int cx, int cy, int s, bool on) {
  // Coração verdadeiro: dois lóbulos circulares com entalhe central e ponta inferior.
  // s = raio dos lóbulos. Largura total ≈ 4s+1, altura ≈ 3.4s.
  int r = s;
  int ly = cy - r / 2;
  int off = r - (r + 3) / 4;  // lóbulos levemente sobrepostos -> entalhe visível
  fillCircle(cx - off, ly, r, on);
  fillCircle(cx + off, ly, r, on);
  // corpo: triângulo do "equador" dos lóbulos até a ponta
  int half = off + r;
  int tipY = ly + r + (r * 3) / 2 + 1;
  fillTriangle(cx - half, ly + 1, cx + half, ly + 1, cx, tipY, on);
  // garante o entalhe: limpa o "V" superior entre os lóbulos
  int notch = tbMax(1, r / 3);
  for (int k = 0; k < notch; k++) hline(cx - (notch - k) + 1, ly - r + k, 2 * (notch - k) - 1, !on);
}
void Framebuffer::progressBar(int x, int y, int w, int h, int permille) {
  permille = tbClamp(permille, 0, 1000);
  rect(x, y, w, h);
  int inner = (w - 4) * permille / 1000;
  fillRect(x + 2, y + 2, inner, h - 4);
}

}  // namespace tb
