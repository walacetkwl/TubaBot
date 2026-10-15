#include "gfx.h"
#include "i18n.h"

namespace tb {

Color mixColor(Color a, Color b, int t) {
  t = tbClamp(t, 0, 256);
  int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
  int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
  return (Color)(((ar + (((br - ar) * t) >> 8)) << 16) | ((ag + (((bg - ag) * t) >> 8)) << 8) | (ab + (((bb - ab) * t) >> 8)));
}
Color scaleColor(Color c, int t) { return mixColor(0, c, t); }

const Glyph* Font::find(uint32_t cp) const {
  int lo = 0, hi = count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) >> 1;
    if (glyphs[mid].cp == cp) return &glyphs[mid];
    if (glyphs[mid].cp < cp) lo = mid + 1; else hi = mid - 1;
  }
  return nullptr;
}

void Canvas::attach(uint16_t* p, int w, int h) {
  px_ = p; w_ = w; h_ = h;
  resetClip();
  drawn_ = prevDrawn_ = IRect();
  fullNext_ = true;
}

void Canvas::mark(int x0, int y0, int x1, int y1) { drawn_.unite(IRect(x0, y0, x1, y1)); }

bool Canvas::region(float bx0, float by0, float bx1, float by1, IRect& out) {
  IRect r(ifloor(bx0) - 1, ifloor(by0) - 1, ifloor(bx1) + 2, ifloor(by1) + 2);
  out = r.intersect(clip_);
  return !out.empty();
}

void Canvas::beginFrame(Color bg) {
  IRect clr = fullNext_ ? IRect(0, 0, w_, h_) : prevDrawn_;
  uint16_t v = to565(bg);
  for (int y = clr.y0; y < clr.y1; y++) {
    uint16_t* row = px_ + y * w_;
    for (int x = clr.x0; x < clr.x1; x++) row[x] = v;
  }
  if (fullNext_) prevDrawn_ = IRect(0, 0, w_, h_);
  fullNext_ = false;
  drawn_ = IRect();
  ox_ = oy_ = 0.f;
  galpha_ = 255;
  resetClip();
}

IRect Canvas::endFrame() {
  IRect d = prevDrawn_;
  d.unite(drawn_);
  prevDrawn_ = drawn_;
  if (d.empty()) return d;
  // CO5300: janela com início e tamanho pares
  d.x0 &= ~1; d.y0 &= ~1;
  if (d.x1 & 1) d.x1++;
  if (d.y1 & 1) d.y1++;
  d.x1 = tbMin(d.x1, w_ + (w_ & 1)); d.y1 = tbMin(d.y1, h_ + (h_ & 1));
  if (d.x1 > w_) d.x1 = w_;
  if (d.y1 > h_) d.y1 = h_;
  return d;
}

void Canvas::clear(Color c) {
  uint16_t v = to565(c);
  for (int i = 0; i < w_ * h_; i++) px_[i] = v;
  mark(0, 0, w_, h_);
}

void Canvas::fillRect(int x, int y, int w, int h, Color c, int a) {
  a = a * galpha_ / 255;
  if (a <= 0 || w <= 0 || h <= 0) return;
  x += iround(ox_); y += iround(oy_);
  IRect r = IRect(x, y, x + w, y + h).intersect(clip_);
  if (r.empty()) return;
  uint16_t v = to565(c);
  int sr = (int)((c >> 19) & 31), sg = (int)((c >> 10) & 63), sb = (int)((c >> 3) & 31);
  int a256 = a + (a >> 7);
  for (int yy = r.y0; yy < r.y1; yy++) {
    uint16_t* row = px_ + yy * w_;
    if (a256 >= 256) for (int xx = r.x0; xx < r.x1; xx++) row[xx] = v;
    else for (int xx = r.x0; xx < r.x1; xx++) blend565(row[xx], sr, sg, sb, a256);
  }
  mark(r.x0, r.y0, r.x1, r.y1);
}

void Canvas::vGradient(int x, int y, int w, int h, Color top, Color bottom, int a) {
  if (h <= 0) return;
  for (int i = 0; i < h; i++) fillRect(x, y + i, w, 1, mixColor(top, bottom, h > 1 ? i * 256 / (h - 1) : 0), a);
}

static inline float len2(float x, float y) { return fsqrt(x * x + y * y); }

static inline float sdRoundBox(float px, float py, float hx, float hy, float r) {
  float qx = fabsf_(px) - (hx - r), qy = fabsf_(py) - (hy - r);
  float ox = fmaxf_(qx, 0.f), oy = fmaxf_(qy, 0.f);
  return len2(ox, oy) + fminf_(fmaxf_(qx, qy), 0.f) - r;
}

void Canvas::fillRoundRect(float x, float y, float w, float h, float r, Color c, int a) {
  if (w <= 0 || h <= 0) return;
  float hx = w * 0.5f, hy = h * 0.5f, cx = x + hx, cy = y + hy;
  r = fclamp(r, 0.f, fminf_(hx, hy));
  shade(x, y, x + w, y + h, c, a, [=](float px, float py) { return sdRoundBox(px - cx, py - cy, hx, hy, r); });
}

void Canvas::strokeRoundRect(float x, float y, float w, float h, float r, float th, Color c, int a) {
  float hx = w * 0.5f, hy = h * 0.5f, cx = x + hx, cy = y + hy, ht = th * 0.5f;
  r = fclamp(r, 0.f, fminf_(hx, hy));
  shade(x - ht, y - ht, x + w + ht, y + h + ht, c, a, [=](float px, float py) { return fabsf_(sdRoundBox(px - cx, py - cy, hx, hy, r)) - ht; });
}

void Canvas::fillCircle(float cx, float cy, float r, Color c, int a) {
  if (r <= 0) return;
  shade(cx - r, cy - r, cx + r, cy + r, c, a, [=](float px, float py) { return len2(px - cx, py - cy) - r; });
}

void Canvas::ring(float cx, float cy, float r, float th, Color c, int a) {
  float ht = th * 0.5f, R = r + ht;
  shade(cx - R, cy - R, cx + R, cy + R, c, a, [=](float px, float py) { return fabsf_(len2(px - cx, py - cy) - r) - ht; });
}

void Canvas::arc(float cx, float cy, float r, float th, float start, float sweep, Color c, int a) {
  if (sweep <= 0.f) return;
  if (sweep >= kTau - 1e-4f) { ring(cx, cy, r, th, c, a); return; }
  float ht = th * 0.5f, R = r + ht;
  // normaliza o início em [0, 2π)
  while (start < 0.f) start += kTau;
  while (start >= kTau) start -= kTau;
  float e = start + sweep;
  float sx = cx + r * fsin(start), sy = cy - r * fcos(start);
  float ex = cx + r * fsin(e), ey = cy - r * fcos(e);
  shade(cx - R, cy - R, cx + R, cy + R, c, a, [=](float px, float py) {
    float dx = px - cx, dy = py - cy;
    float ang = fatan2(dx, -dy);
    if (ang < 0.f) ang += kTau;
    float rel = ang - start;
    if (rel < 0.f) rel += kTau;
    if (rel <= sweep) return fabsf_(len2(dx, dy) - r) - ht;
    float d1 = len2(px - sx, py - sy), d2 = len2(px - ex, py - ey);
    return fminf_(d1, d2) - ht;
  });
}

static inline float sdSegment(float px, float py, float ax, float ay, float bx, float by) {
  float pax = px - ax, pay = py - ay, bax = bx - ax, bay = by - ay;
  float dd = bax * bax + bay * bay;
  float h = dd > 0.f ? fclamp((pax * bax + pay * bay) / dd, 0.f, 1.f) : 0.f;
  return len2(pax - bax * h, pay - bay * h);
}

void Canvas::line(float x0, float y0, float x1, float y1, float th, Color c, int a) {
  float ht = th * 0.5f;
  shade(fminf_(x0, x1) - ht, fminf_(y0, y1) - ht, fmaxf_(x0, x1) + ht, fmaxf_(y0, y1) + ht, c, a,
        [=](float px, float py) { return sdSegment(px, py, x0, y0, x1, y1) - ht; });
}

void Canvas::fillEllipse(float cx, float cy, float rx, float ry, Color c, int a) {
  if (rx <= 0 || ry <= 0) return;
  if (fabsf_(rx - ry) < 0.01f) { fillCircle(cx, cy, rx, c, a); return; }
  shade(cx - rx, cy - ry, cx + rx, cy + ry, c, a, [=](float px, float py) {
    float x = (px - cx), y = (py - cy);
    float k0 = len2(x / rx, y / ry);
    float k1 = len2(x / (rx * rx), y / (ry * ry));
    if (k1 < 1e-6f) return -fminf_(rx, ry);
    return k0 * (k0 - 1.f) / k1;
  });
}

// Distância assinada a um polígono qualquer (Inigo Quilez, licença MIT).
static float sdPoly(const Pt* v, int n, float px, float py) {
  float d = (px - v[0].x) * (px - v[0].x) + (py - v[0].y) * (py - v[0].y);
  float s = 1.f;
  for (int i = 0, j = n - 1; i < n; j = i, i++) {
    float ex = v[j].x - v[i].x, ey = v[j].y - v[i].y;
    float wx = px - v[i].x, wy = py - v[i].y;
    float ee = ex * ex + ey * ey;
    float t = ee > 0.f ? fclamp((wx * ex + wy * ey) / ee, 0.f, 1.f) : 0.f;
    float bx = wx - ex * t, by = wy - ey * t;
    float dd = bx * bx + by * by;
    if (dd < d) d = dd;
    bool c1 = py >= v[i].y, c2 = py < v[j].y, c3 = ex * wy > ey * wx;
    if ((c1 && c2 && c3) || (!c1 && !c2 && !c3)) s = -s;
  }
  return s * fsqrt(d);
}

void Canvas::fillPolygon(const Pt* p, int n, Color c, int a) {
  if (n < 3 || n > 24) return;
  Pt v[24];
  float x0 = p[0].x, y0 = p[0].y, x1 = x0, y1 = y0;
  for (int i = 0; i < n; i++) {
    v[i] = p[i];
    x0 = fminf_(x0, p[i].x); x1 = fmaxf_(x1, p[i].x); y0 = fminf_(y0, p[i].y); y1 = fmaxf_(y1, p[i].y);
  }
  shade(x0, y0, x1, y1, c, a, [&](float px, float py) { return sdPoly(v, n, px, py); });
}

void Canvas::fillTriangle(Pt p0, Pt p1, Pt p2, Color c, int a) {
  Pt v[3] = {p0, p1, p2};
  fillPolygon(v, 3, c, a);
}

void Canvas::heart(float cx, float cy, float size, Color c, int a) {
  // sdHeart (Inigo Quilez): espaço unitário com a ponta em y=0 e topo ≈ 1.
  float s = size;
  float baseY = cy + s * 0.5f;
  shade(cx - s * 0.62f, cy - s * 0.6f, cx + s * 0.62f, cy + s * 0.55f, c, a, [=](float px, float py) {
    float x = fabsf_(px - cx) / s, y = (baseY - py) / s;
    float d;
    if (y + x > 1.f) d = len2(x - 0.25f, y - 0.75f) - 0.35355339f;
    else {
      float m = fmaxf_(x + y, 0.f) * 0.5f;
      float a1 = x * x + (y - 1.f) * (y - 1.f), a2 = (x - m) * (x - m) + (y - m) * (y - m);
      d = fsqrt(fminf_(a1, a2)) * (x - y > 0.f ? 1.f : -1.f);
    }
    return d * s;
  });
}

void Canvas::star(float cx, float cy, float ro, float ri, Color c, int a, float rot) {
  Pt v[10];
  for (int i = 0; i < 10; i++) {
    float ang = rot + (float)i * kPi / 5.f;
    float r = (i & 1) ? ri : ro;
    v[i].x = cx + r * fsin(ang);
    v[i].y = cy - r * fcos(ang);
  }
  fillPolygon(v, 10, c, a);
}

void Canvas::glow(float cx, float cy, float r, Color c, int a) {
  a = a * galpha_ / 255;
  if (a <= 0 || r <= 1.f) return;
  IRect rr;
  if (!region(cx - r + ox_, cy - r + oy_, cx + r + ox_, cy + r + oy_, rr)) return;
  int sr = (int)((c >> 19) & 31), sg = (int)((c >> 10) & 63), sb = (int)((c >> 3) & 31);
  float inv = 1.f / r;
  bool any = false;
  for (int y = rr.y0; y < rr.y1; y++) {
    uint16_t* row = px_ + y * w_;
    float dy = (float)y + 0.5f - oy_ - cy;
    for (int x = rr.x0; x < rr.x1; x++) {
      float dx = (float)x + 0.5f - ox_ - cx;
      float t = 1.f - len2(dx, dy) * inv;
      if (t <= 0.f) continue;
      int aa = (int)(t * t * (float)a);
      if (aa <= 0) continue;
      blend565(row[x], sr, sg, sb, aa);
      any = true;
    }
  }
  if (any) mark(rr.x0, rr.y0, rr.x1, rr.y1);
}

// ── texto ───────────────────────────────────────────────────────────────────
void Canvas::glyph(const Font& f, const Glyph& g, int gx, int gy, Color c, int a) {
  (void)f;
  if (!g.w || !g.h) return;
  IRect r = IRect(gx, gy, gx + g.w, gy + g.h).intersect(clip_);
  if (r.empty()) return;
  int sr = (int)((c >> 19) & 31), sg = (int)((c >> 10) & 63), sb = (int)((c >> 3) & 31);
  uint16_t solid = to565(c);
  const uint8_t* bits = f.bits + g.off;
  for (int y = r.y0; y < r.y1; y++) {
    uint16_t* row = px_ + y * w_;
    int gyy = y - gy;
    for (int x = r.x0; x < r.x1; x++) {
      int idx = gyy * g.w + (x - gx);
      int v = (bits[idx >> 1] >> ((idx & 1) ? 0 : 4)) & 15;
      if (!v) continue;
      int aa = (v * 17 * a) / 255;  // 0..255
      aa += aa >> 7;
      if (aa >= 256) row[x] = solid; else blend565(row[x], sr, sg, sb, aa);
    }
  }
  mark(r.x0, r.y0, r.x1, r.y1);
}

int Canvas::textWidth(const char* s, const Font& f) {
  if (!s) return 0;
  s = tr(s);   // todo texto desenhado passa pela tradução (pt-BR é o texto-fonte)
  const char* end = s + tbStrlen(s);
  int w = 0;
  while (s < end) {
    uint32_t cp = utf8Next(s, end);
    const Glyph* g = f.find(cp);
    if (!g) g = f.find('?');
    if (g) w += g->adv;
  }
  return w;
}

int Canvas::text(float x, float y, const char* s, const Font& f, Color c, int a, Align al) {
  if (!s || !*s) return 0;
  s = tr(s);
  int w = textWidth(s, f);
  a = a * galpha_ / 255;
  int pen = iround(x + ox_);
  if (al == Align::Center) pen -= w / 2;
  else if (al == Align::Right) pen -= w;
  int base = iround(y + oy_);
  const char* end = s + tbStrlen(s);
  if (a <= 0) return w;
  while (s < end) {
    uint32_t cp = utf8Next(s, end);
    const Glyph* g = f.find(cp);
    if (!g) g = f.find('?');
    if (!g) continue;
    glyph(f, *g, pen + g->ox, base + g->oy, c, a);
    pen += g->adv;
  }
  return w;
}

const Font& Canvas::fitFont(const char* s, int maxW, const Font* const* fonts, int n) {
  for (int i = 0; i < n; i++) if (textWidth(s, *fonts[i]) <= maxW) return *fonts[i];
  return *fonts[n - 1];
}

uint32_t Canvas::crc() const { return crc32((const uint8_t*)px_, (size_t)w_ * h_ * 2); }

}  // namespace tb
