// Motor gráfico do TubaBot: tela 2,8" usada em PAISAGEM — interface lógica 320×240
// (o painel físico 240×320 da ES3C28P é girado 90° pelo firmware). RGB565, antialiasing.
//
// - Todas as formas são desenhadas por campo de distância (SDF): bordas suaves
//   em qualquer tamanho, sem bitmaps.
// - O canvas registra a área desenhada a cada quadro; o firmware só envia ao
//   display a união (quadro anterior ∪ atual), o que mantém o SPI folgado.
#pragma once
#include "tb_base.h"
#include "font.h"
#include "fmath.h"

namespace tb {

static const int SCR_W = 320;
static const int SCR_H = 240;
static const int SCR_CX = 160;
static const int SCR_CY = 120;

// Cor 0xRRGGBB
typedef uint32_t Color;
inline uint16_t to565(Color c) { return (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F)); }
inline Color from565(uint16_t v) {
  uint32_t r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
  return ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
}
Color mixColor(Color a, Color b, int t256);   // t256: 0 = a, 256 = b
Color scaleColor(Color c, int t256);         // escurece (t256 < 256)

struct IRect {
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // x1/y1 exclusivos
  IRect() {}
  IRect(int a, int b, int c, int d) : x0(a), y0(b), x1(c), y1(d) {}
  bool empty() const { return x1 <= x0 || y1 <= y0; }
  int w() const { return x1 - x0; }
  int h() const { return y1 - y0; }
  void unite(const IRect& o) {
    if (o.empty()) return;
    if (empty()) { *this = o; return; }
    x0 = tbMin(x0, o.x0); y0 = tbMin(y0, o.y0); x1 = tbMax(x1, o.x1); y1 = tbMax(y1, o.y1);
  }
  IRect intersect(const IRect& o) const {
    IRect r(tbMax(x0, o.x0), tbMax(y0, o.y0), tbMin(x1, o.x1), tbMin(y1, o.y1));
    if (r.empty()) return IRect();
    return r;
  }
  bool contains(int x, int y) const { return x >= x0 && x < x1 && y >= y0 && y < y1; }
};

enum class Align : uint8_t { Left, Center, Right };

struct Pt { float x, y; };

class Canvas {
 public:
  void attach(uint16_t* pixels, int w, int h);
  uint16_t* pixels() { return px_; }
  const uint16_t* pixels() const { return px_; }
  int width() const { return w_; }
  int height() const { return h_; }

  // ── quadro ──
  // Apaga (com 'bg') o que foi desenhado no quadro anterior e zera o registro.
  void beginFrame(Color bg = 0);
  // Área a enviar ao display: união do quadro anterior e do atual (coordenadas pares).
  IRect endFrame();
  void invalidateAll() { fullNext_ = true; }
  const IRect& drawnArea() const { return drawn_; }

  // ── estado ──
  void setClip(const IRect& r) { clip_ = r.intersect(IRect(0, 0, w_, h_)); }
  void resetClip() { clip_ = IRect(0, 0, w_, h_); }
  void setOrigin(float dx, float dy) { ox_ = dx; oy_ = dy; }
  float originX() const { return ox_; }
  float originY() const { return oy_; }
  void setGlobalAlpha(int a) { galpha_ = tbClamp(a, 0, 255); }  // aplicado a tudo (transições)
  int globalAlpha() const { return galpha_; }

  // ── primitivas (alfa 0..255) ──
  void clear(Color c);
  void fillRect(int x, int y, int w, int h, Color c, int a = 255);
  void vGradient(int x, int y, int w, int h, Color top, Color bottom, int a = 255);
  void fillRoundRect(float x, float y, float w, float h, float r, Color c, int a = 255);
  void strokeRoundRect(float x, float y, float w, float h, float r, float th, Color c, int a = 255);
  void fillCircle(float cx, float cy, float r, Color c, int a = 255);
  void ring(float cx, float cy, float r, float th, Color c, int a = 255);
  // Arco: ângulos em radianos, 0 = 12 horas, sentido horário. Pontas arredondadas.
  void arc(float cx, float cy, float r, float th, float start, float sweep, Color c, int a = 255);
  void line(float x0, float y0, float x1, float y1, float th, Color c, int a = 255);
  void fillEllipse(float cx, float cy, float rx, float ry, Color c, int a = 255);
  void fillTriangle(Pt p0, Pt p1, Pt p2, Color c, int a = 255);
  void fillPolygon(const Pt* p, int n, Color c, int a = 255);  // até 24 vértices, côncavo ok
  // Coração "de verdade" (dois lóbulos e ponta), 'size' = altura aproximada.
  void heart(float cx, float cy, float size, Color c, int a = 255);
  void star(float cx, float cy, float rOut, float rIn, Color c, int a = 255, float rot = 0.f);
  // Brilho radial suave (halo), queda quadrática.
  void glow(float cx, float cy, float r, Color c, int a = 255);

  // ── texto (y = linha de base) ──
  int text(float x, float y, const char* s, const Font& f, Color c, int a = 255, Align al = Align::Left);
  int textCentered(float y, const char* s, const Font& f, Color c, int a = 255) { return text((float)SCR_CX, y, s, f, c, a, Align::Center); }
  static int textWidth(const char* s, const Font& f);
  // Maior fonte da lista (da maior para a menor) em que o texto cabe em maxW.
  static const Font& fitFont(const char* s, int maxW, const Font* const* fonts, int n);

  // utilidades
  uint16_t get(int x, int y) const { return (unsigned)x < (unsigned)w_ && (unsigned)y < (unsigned)h_ ? px_[y * w_ + x] : 0; }
  uint32_t crc() const;

  template <class F> void shade(float bx0, float by0, float bx1, float by1, Color c, int a, F sdf);

 private:
  void mark(int x0, int y0, int x1, int y1);
  bool region(float bx0, float by0, float bx1, float by1, IRect& out);
  void glyph(const Font& f, const Glyph& g, int x, int y, Color c, int a);

  uint16_t* px_ = nullptr;
  int w_ = 0, h_ = 0;
  IRect clip_, drawn_, prevDrawn_;
  bool fullNext_ = true;
  float ox_ = 0.f, oy_ = 0.f;
  int galpha_ = 255;
};

// Mistura um pixel RGB565 com uma cor (alfa 0..256).
inline void blend565(uint16_t& d, int sr, int sg, int sb, int a) {
  int dr = (d >> 11) & 31, dg = (d >> 5) & 63, db = d & 31;
  // arredondamento ao mais próximo (sem desvio de matiz em alfas baixos sobre fundo claro)
  int ia = 256 - a;
  dr = (dr * ia + sr * a + 128) >> 8; dg = (dg * ia + sg * a + 128) >> 8; db = (db * ia + sb * a + 128) >> 8;
  d = (uint16_t)((dr << 11) | (dg << 5) | db);
}

// Percorre a caixa e pinta com cobertura derivada da distância assinada (px; < 0 = dentro).
template <class F> void Canvas::shade(float bx0, float by0, float bx1, float by1, Color c, int a, F sdf) {
  a = a * galpha_ / 255;
  if (a <= 0) return;
  bx0 += ox_; bx1 += ox_; by0 += oy_; by1 += oy_;
  IRect r;
  if (!region(bx0, by0, bx1, by1, r)) return;
  int sr = (int)((c >> 19) & 31), sg = (int)((c >> 10) & 63), sb = (int)((c >> 3) & 31);
  uint16_t solid = to565(c);
  int a256 = a + (a >> 7);  // 255 -> 256
  bool any = false;
  int mx0 = r.x1, mx1 = r.x0, my0 = r.y1, my1 = r.y0;
  for (int y = r.y0; y < r.y1; y++) {
    uint16_t* row = px_ + y * w_;
    float fy = (float)y + 0.5f - oy_;
    for (int x = r.x0; x < r.x1; x++) {
      float d = sdf((float)x + 0.5f - ox_, fy);
      if (d >= 0.5f) continue;
      int cov = d <= -0.5f ? 256 : (int)((0.5f - d) * 256.f);
      int aa = (a256 * cov) >> 8;
      if (aa >= 256) row[x] = solid;
      else if (aa > 0) blend565(row[x], sr, sg, sb, aa);
      else continue;
      any = true;
      if (x < mx0) mx0 = x;
      if (x >= mx1) mx1 = x + 1;
      if (y < my0) my0 = y;
      my1 = y + 1;
    }
  }
  if (any) mark(mx0, my0, mx1, my1);
}

}  // namespace tb
