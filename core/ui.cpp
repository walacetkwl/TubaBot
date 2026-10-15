#include "ui.h"

namespace tb {
namespace ui {

Theme g_theme;

static Color luminance(Color c) {
  int r = (int)((c >> 16) & 255), g = (int)((c >> 8) & 255), b = (int)(c & 255);
  return (Color)((r * 299 + g * 587 + b * 114) / 1000);
}

void setTheme(bool light, Color accent) {
  Theme& t = g_theme;
  t.light = light;
  if (light) {
    t.bg = 0xFFFFFF; t.surf = 0xF1F3F7; t.surf2 = 0xE2E6EC; t.line = 0xD3D8E0;
    t.text = 0x161A20; t.muted = 0x5B6370; t.dim = 0x9AA2AE; t.scrim = 0xFFFFFF; t.toastBg = 0x232832;
  } else {
    t.bg = 0x000000; t.surf = 0x14171D; t.surf2 = 0x232831; t.line = 0x2E343F;
    t.text = 0xF3F5F8; t.muted = 0x9AA3B2; t.dim = 0x5C6472; t.scrim = 0x000000; t.toastBg = 0x262C38;
  }
  t.accent = accent;
  t.onAccent = luminance(accent) > 150 ? (Color)0x101318 : (Color)0xFFFFFF;
}

Color acc(Color c) { return g_theme.light ? mixColor(c, 0x000000, 78) : c; }

void pill(Canvas& c, float cx, float cy, float w, float h, Color bg, const char* label, const Font& f, Color fg, int a) {
  c.fillRoundRect(cx - w * 0.5f, cy - h * 0.5f, w, h, h * 0.5f, bg, a);
  if (label) c.text(cx, cy + (float)(f.ascent - f.descent) * 0.36f, label, f, fg, a, Align::Center);
}

void roundButton(Canvas& c, float cx, float cy, float r, Color bg, Icon ic, Color fg, bool pressed, int a) {
  float rr = pressed ? r * 0.92f : r;
  c.fillCircle(cx, cy, rr, pressed ? mixColor(bg, g_theme.light ? 0x000000 : 0xFFFFFF, 40) : bg, a);
  drawIcon(c, ic, cx, cy, rr * 0.95f, fg, a);
}

void header(Canvas& c, const char* title, Color col) {
  const Font* fonts[2] = {&FONT_L, &FONT_M};
  c.text((float)SCR_CX, 27.f, title, Canvas::fitFont(title, 200, fonts, 2), col, 255, Align::Center);
}

void hint(Canvas& c, const char* text, float y) { c.textCentered(y, text, FONT_S, MUTED); }

void pageDots(Canvas& c, int n, int sel, float y, Color col) {
  if (!col) col = TEXT;
  float gap = 14.f, x0 = (float)SCR_CX - gap * (float)(n - 1) * 0.5f;
  for (int i = 0; i < n; i++) {
    if (i == sel) c.fillRoundRect(x0 + gap * (float)i - 7.f, y - 3.f, 14.f, 6.f, 3.f, col);
    else c.fillCircle(x0 + gap * (float)i, y, 2.8f, col, 110);
  }
}

void progressRing(Canvas& c, float cx, float cy, float r, float th, int permille, Color col, Color track) {
  c.ring(cx, cy, r, th, track ? track : SURF2);
  permille = tbClamp(permille, 0, 1000);
  if (permille > 0) c.arc(cx, cy, r, th, 0.f, kTau * (float)permille / 1000.f, col);
}

void slider(Canvas& c, float x, float y, float w, float h, int permille, Color col, Icon ic) {
  c.fillRoundRect(x, y - h * 0.5f, w, h, h * 0.5f, SURF2);
  float fw = h + (w - h) * (float)tbClamp(permille, 0, 1000) / 1000.f;
  c.fillRoundRect(x, y - h * 0.5f, fw, h, h * 0.5f, col);
  drawIcon(c, ic, x + h * 0.5f, y, h * 0.5f, luminance(col) > 150 ? (Color)0x101318 : (Color)0xFFFFFF);
}

void toggle(Canvas& c, float cx, float cy, bool on, Color col) {
  c.fillRoundRect(cx - 19.f, cy - 11.f, 38.f, 22.f, 11.f, on ? col : SURF2);
  c.fillCircle(on ? cx + 8.f : cx - 8.f, cy, 8.5f, on ? (Color)0xFFFFFF : (g_theme.light ? (Color)0xFFFFFF : MUTED));
}

// ── lista ──
float ListView::maxScroll() const {
  float content = (float)(n_ * kRowH + 8);
  float view = (float)(SCR_H - kTop);
  return content > view ? content - view : 0.f;
}

void ListView::reset(int n, int keepScrollPx) {
  n_ = n; vel_ = 0.f; dragging_ = snapping_ = false; dragDist_ = 0;
  scroll_ = fclamp((float)keepScrollPx, 0.f, maxScroll());
}

void ListView::onDown(int y, ms_t t) {
  dragging_ = true; snapping_ = false; vel_ = 0.f;
  dragY0_ = y; dragScroll0_ = scroll_; dragDist_ = 0; lastTick_ = t;
}

void ListView::onMove(int y, ms_t t) {
  (void)t;
  if (!dragging_) return;
  dragDist_ = tbMax(dragDist_, tbAbs(y - dragY0_));
  float s = dragScroll0_ - (float)(y - dragY0_);
  float m = maxScroll();
  if (s < 0.f) s *= 0.35f;                         // elástico nas pontas
  if (s > m) s = m + (s - m) * 0.35f;
  scroll_ = s;
}

void ListView::onUp(int vy) {
  if (!dragging_) return;
  dragging_ = false;
  vel_ = dragDist_ > 10 ? fclamp(-(float)vy, -2400.f, 2400.f) : 0.f;
  snapping_ = true;
}

void ListView::tick(ms_t now) {
  ms_t dt = now > lastTick_ ? now - lastTick_ : 0;
  if (dt > 100) dt = 100;
  lastTick_ = now;
  if (dragging_ || !snapping_) return;
  float f = (float)dt / 1000.f;
  float m = maxScroll();
  if (vel_ != 0.f) {
    scroll_ += vel_ * f;
    vel_ *= 1.f - fclamp(f * 4.f, 0.f, 1.f);
    if (fabsf_(vel_) < 40.f || scroll_ < -30.f || scroll_ > m + 30.f) vel_ = 0.f;
    return;
  }
  float target = fclamp(scroll_, 0.f, m);
  float d = target - scroll_;
  if (fabsf_(d) < 0.5f) { scroll_ = target; snapping_ = false; return; }
  scroll_ += d * fclamp(f * 14.f, 0.f, 1.f);
}

int ListView::hit(int x, int y) const {
  (void)x;
  if (y < kTop) return -1;
  float yy = (float)(y - kTop) + scroll_;
  int i = ifloor(yy / (float)kRowH);
  if (i < 0 || i >= n_) return -1;
  return i;
}

void ListView::scrollTo(int i) {
  float top = (float)(i * kRowH), bot = top + (float)kRowH;
  float view = (float)(SCR_H - kTop);
  if (top < scroll_) scroll_ = top;
  else if (bot > scroll_ + view) scroll_ = bot - view;
  scroll_ = fclamp(scroll_, 0.f, maxScroll());
  vel_ = 0; snapping_ = false;
}

void ListView::rowCenter(int i, int& x, int& y) const {
  x = SCR_CX;
  y = kTop + i * kRowH + kRowH / 2 - iround(scroll_);
}

void ListView::draw(Canvas& c, const ListItem* items, int n, int pressedIdx) const {
  c.setClip(IRect(0, kTop, SCR_W, SCR_H));
  for (int i = 0; i < n; i++) {
    float y0 = (float)(kTop + i * kRowH) - scroll_;
    if (y0 > (float)SCR_H || y0 + (float)kRowH < (float)kTop) continue;
    const ListItem& it = items[i];
    float cy = y0 + (float)kRowH * 0.5f;
    c.fillRoundRect(8.f, y0 + 3.f, (float)SCR_W - 16.f, (float)kRowH - 6.f, 14.f, i == pressedIdx ? SURF2 : SURF);
    c.fillCircle(30.f, cy, 14.f, it.color);
    drawIcon(c, it.icon, 30.f, cy, 17.f, INK);
    float tx = 52.f;
    bool twoLine = it.sub && (it.kind == RowKind::Link || it.kind == RowKind::Action);
    if (twoLine) {
      c.text(tx, cy - 2.f, it.label, FONT_M, TEXT);
      c.text(tx, cy + 14.f, it.sub, FONT_S, MUTED);
    } else {
      c.text(tx, cy + 6.f, it.label, FONT_M, TEXT);
    }
    switch (it.kind) {
      case RowKind::Toggle: toggle(c, (float)SCR_W - 38.f, cy, it.on, ACCENT); break;
      case RowKind::Value:
        if (it.sub) c.text((float)SCR_W - 20.f, cy + 5.f, it.sub, FONT_S, MUTED, 255, Align::Right);
        break;
      case RowKind::Link: drawIcon(c, Icon::Play, (float)SCR_W - 24.f, cy, 10.f, DIM); break;
      default: break;
    }
  }
  c.resetClip();
  // indicador de rolagem
  float m = maxScroll();
  if (m > 0.f) {
    float view = (float)(SCR_H - kTop);
    float h = view * view / (view + m);
    float y = (float)kTop + (view - h) * fclamp(scroll_ / m, 0.f, 1.f);
    c.fillRoundRect((float)SCR_W - 4.f, y, 3.f, h, 1.5f, DIM, 160);
  }
}

// ── grade ──
void drawGrid(Canvas& c, const ListItem* items, int n, int pressedIdx) {
  for (int i = 0; i < n; i++) {
    float cx = GridGeom::cx(i), cy = GridGeom::cy(i);
    float w = (float)GridGeom::kTileW, h = (float)GridGeom::kTileH;
    const ListItem& it = items[i];
    bool pr = i == pressedIdx;
    float s = pr ? 0.95f : 1.f;
    c.fillRoundRect(cx - w * 0.5f * s, cy - h * 0.5f * s, w * s, h * s, 16.f, pr ? SURF2 : SURF);
    c.fillCircle(cx, cy - 8.f, 15.f * s, it.color);
    drawIcon(c, it.icon, cx, cy - 8.f, 18.f * s, INK);
    c.text(cx, cy + 22.f, it.label, FONT_S, TEXT, 255, Align::Center);
    if (it.sub) {
      // valor discreto no canto (ex.: 60%)
      c.text(cx + w * 0.5f - 7.f, cy - h * 0.5f + 15.f, it.sub, FONT_S, MUTED, 220, Align::Right);
    }
  }
}

int gridHit(int n, int x, int y) {
  for (int i = 0; i < n; i++) {
    float cx = GridGeom::cx(i), cy = GridGeom::cy(i);
    if (fabsf_((float)x - cx) <= (float)GridGeom::kTileW * 0.5f + 2.f && fabsf_((float)y - cy) <= (float)GridGeom::kTileH * 0.5f + 4.f) return i;
  }
  return -1;
}

}  // namespace ui
}  // namespace tb
