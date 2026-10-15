#include "ui.h"

namespace tb {
namespace ui {

void pill(Canvas& c, float cx, float cy, float w, float h, Color bg, const char* label, const Font& f, Color fg, int a) {
  c.fillRoundRect(cx - w * 0.5f, cy - h * 0.5f, w, h, h * 0.5f, bg, a);
  if (label) c.text(cx, cy + (float)(f.ascent - f.descent) * 0.36f, label, f, fg, a, Align::Center);
}

void roundButton(Canvas& c, float cx, float cy, float r, Color bg, Icon ic, Color fg, bool pressed, int a) {
  float rr = pressed ? r * 0.92f : r;
  c.fillCircle(cx, cy, rr, pressed ? mixColor(bg, 0xFFFFFF, 40) : bg, a);
  drawIcon(c, ic, cx, cy, rr * 0.95f, fg, a);
}

void title(Canvas& c, float y, const char* text, Color col) { c.textCentered(y, text, FONT_L, col); }

void hint(Canvas& c, const char* text, float y) { c.textCentered(y, text, FONT_S, MUTED); }

void pageDots(Canvas& c, int n, int sel, float y, Color col) {
  float gap = 18.f, x0 = (float)SCR_CX - gap * (float)(n - 1) * 0.5f;
  for (int i = 0; i < n; i++) {
    if (i == sel) c.fillRoundRect(x0 + gap * (float)i - 9.f, y - 4.f, 18.f, 8.f, 4.f, col);
    else c.fillCircle(x0 + gap * (float)i, y, 3.5f, col, 110);
  }
}

void appHeader(Canvas& c, Icon ic, const char* name, Color col) {
  int w = Canvas::textWidth(name, FONT_M);
  float x0 = (float)SCR_CX - (float)(w + 34) * 0.5f;
  drawIcon(c, ic, x0 + 12.f, 58.f, 24.f, col);
  c.text(x0 + 34.f, 67.f, name, FONT_M, col);
}

void progressRing(Canvas& c, float r, float th, int permille, Color col, Color track) {
  c.ring((float)SCR_CX, (float)SCR_CY, r, th, track);
  permille = tbClamp(permille, 0, 1000);
  if (permille > 0) c.arc((float)SCR_CX, (float)SCR_CY, r, th, 0.f, kTau * (float)permille / 1000.f, col);
}

void slider(Canvas& c, float x, float y, float w, int permille, Color col, Icon ic) {
  const float h = 44.f;
  c.fillRoundRect(x, y - h * 0.5f, w, h, h * 0.5f, SURF2);
  float fw = h + (w - h) * (float)tbClamp(permille, 0, 1000) / 1000.f;
  c.fillRoundRect(x, y - h * 0.5f, fw, h, h * 0.5f, col);
  drawIcon(c, ic, x + h * 0.5f, y, 24.f, permille > 60 ? (Color)0x101318 : TEXT);
}

void toggle(Canvas& c, float cx, float cy, bool on, Color col) {
  c.fillRoundRect(cx - 26.f, cy - 15.f, 52.f, 30.f, 15.f, on ? col : SURF2);
  c.fillCircle(on ? cx + 11.f : cx - 11.f, cy, 11.f, on ? (Color)0xFFFFFF : MUTED);
}

// ── lista ──
void ListView::reset(int n, int sel) {
  n_ = n; scroll_ = (float)tbClamp(sel, 0, tbMax(0, n - 1)); vel_ = 0.f;
  dragging_ = snapping_ = false; dragDist_ = 0;
}

int ListView::centered() const { return tbClamp(iround(scroll_), 0, tbMax(0, n_ - 1)); }

void ListView::onDown(int y, ms_t t) {
  dragging_ = true; snapping_ = false; vel_ = 0.f;
  dragY0_ = y; dragScroll0_ = scroll_; dragDist_ = 0; lastTick_ = t;
}

void ListView::onMove(int y, ms_t t) {
  (void)t;
  if (!dragging_) return;
  dragDist_ = tbMax(dragDist_, tbAbs(y - dragY0_));
  float s = dragScroll0_ - (float)(y - dragY0_) / (float)kItemH;
  float maxS = (float)tbMax(0, n_ - 1);
  if (s < 0.f) s *= 0.35f;                         // elástico nas pontas
  if (s > maxS) s = maxS + (s - maxS) * 0.35f;
  scroll_ = s;
}

void ListView::onUp(int vy) {
  if (!dragging_) return;
  dragging_ = false;
  vel_ = dragDist_ > 18 ? fclamp(-(float)vy / (float)kItemH, -14.f, 14.f) : 0.f;
  snapping_ = true;
}

void ListView::tick(ms_t now) {
  ms_t dt = now > lastTick_ ? now - lastTick_ : 0;
  if (dt > 100) dt = 100;
  lastTick_ = now;
  if (dragging_ || !snapping_) return;
  float f = (float)dt / 1000.f;
  float maxS = (float)tbMax(0, n_ - 1);
  if (vel_ != 0.f) {
    scroll_ += vel_ * f;
    vel_ *= 1.f - fclamp(f * 4.5f, 0.f, 1.f);
    if (fabsf_(vel_) < 1.2f || scroll_ < -0.4f || scroll_ > maxS + 0.4f) vel_ = 0.f;
    return;
  }
  float target = fclamp((float)iround(scroll_), 0.f, maxS);
  float d = target - scroll_;
  if (fabsf_(d) < 0.004f) { scroll_ = target; snapping_ = false; return; }
  scroll_ += d * fclamp(f * 14.f, 0.f, 1.f);
}

int ListView::hit(int x, int y) const {
  (void)x;
  float idx = scroll_ + (float)(y - SCR_CY) / (float)kItemH;
  int i = iround(idx);
  if (i < 0 || i >= n_) return -1;
  if (fabsf_(idx - (float)i) > 0.45f) return -1;
  return i;
}

void ListView::draw(Canvas& c, const ListItem* items, int n, ms_t now, int pressedIdx) const {
  (void)now;
  int first = tbMax(0, ifloor(scroll_) - 3), last = tbMin(n - 1, ifloor(scroll_) + 3);
  for (int i = first; i <= last; i++) {
    float dy = ((float)i - scroll_) * (float)kItemH;
    if (fabsf_(dy) > 230.f) continue;
    float k = fabsf_(dy) / 233.f;
    float sc = 1.f - 0.30f * k * k;
    int a = (int)(255.f * fclamp(1.15f - k * k * 1.1f, 0.f, 1.f));
    float y = (float)SCR_CY + dy;
    bool center = fabsf_(dy) < (float)kItemH * 0.5f;
    const ListItem& it = items[i];
    if (center) {
      float bw = 330.f, bh = 84.f;
      c.fillRoundRect((float)SCR_CX - bw * 0.5f, y - bh * 0.5f, bw, bh, bh * 0.5f, i == pressedIdx ? SURF2 : SURF, a);
    }
    float ix = (float)SCR_CX - 120.f * sc;
    c.fillCircle(ix, y, 30.f * sc, it.color, a);
    drawIcon(c, it.icon, ix, y, 34.f * sc, 0x0C0F14, a);
    const Font& f = center ? FONT_L : FONT_M;
    float tx = ix + 46.f * sc;
    if (it.sub && center) {
      c.text(tx, y - 4.f, it.label, f, TEXT, a);
      c.text(tx, y + 24.f, it.sub, FONT_S, MUTED, a);
    } else {
      c.text(tx, y + (float)f.ascent * 0.36f, it.label, f, center ? TEXT : MUTED, a);
    }
  }
}

}  // namespace ui
}  // namespace tb
