#include "face.h"

namespace tb {

#define EYE(w, h, lt, lb, sl, dx, dy, k) {w, h, lt, lb, sl, dx, dy, EyeKind::k}
static const FaceDef FACES[(int)Expr::Count] = {
  /* Happy     */ {EYE(26,30,0,14,0,0,0,Normal), EYE(26,30,0,14,0,0,0,Normal), Mouth::Smile, FX_NONE},
  /* Neutral   */ {EYE(26,30,0,0,0,0,0,Normal), EYE(26,30,0,0,0,0,0,Normal), Mouth::None, FX_NONE},
  /* Sad       */ {EYE(26,26,6,0,-6,0,4,Normal), EYE(26,26,6,0,-6,0,4,Normal), Mouth::Frown, FX_NONE},
  /* Surprised */ {EYE(28,34,0,0,0,0,-2,Normal), EYE(28,34,0,0,0,0,-2,Normal), Mouth::O, FX_NONE},
  /* Love      */ {EYE(28,28,0,0,0,0,0,Heart), EYE(28,28,0,0,0,0,0,Heart), Mouth::Smile, FX_HEARTS},
  /* Sleepy    */ {EYE(26,30,18,0,0,0,4,Normal), EYE(26,30,18,0,0,0,4,Normal), Mouth::Small, FX_ZZZ},
  /* Angry     */ {EYE(26,28,8,0,8,0,2,Normal), EYE(26,28,8,0,8,0,2,Normal), Mouth::Flat, FX_NONE},
  /* Dead      */ {EYE(22,22,0,0,0,0,0,X), EYE(22,22,0,0,0,0,0,X), Mouth::Wobble, FX_NONE},
  /* Blink     */ {EYE(26,3,0,0,0,0,0,Normal), EYE(26,3,0,0,0,0,0,Normal), Mouth::None, FX_NONE},
  /* WinkL     */ {EYE(24,24,0,0,0,0,0,ArcUp), EYE(26,30,0,8,0,0,0,Normal), Mouth::Smile, FX_NONE},
  /* WinkR     */ {EYE(26,30,0,8,0,0,0,Normal), EYE(24,24,0,0,0,0,0,ArcUp), Mouth::Smile, FX_NONE},
  /* Pet       */ {EYE(24,24,0,0,0,0,0,ArcUp), EYE(24,24,0,0,0,0,0,ArcUp), Mouth::Cat, FX_BLUSH},
  /* Focus     */ {EYE(26,16,0,0,3,0,2,Normal), EYE(26,16,0,0,3,0,2,Normal), Mouth::Flat, FX_NONE},
  /* Rest      */ {EYE(24,24,0,0,0,0,2,ArcUp), EYE(24,24,0,0,0,0,2,ArcUp), Mouth::Small, FX_NONE},
  /* Hit       */ {EYE(24,24,0,0,0,0,0,ArcUp), EYE(24,24,0,0,0,0,0,ArcUp), Mouth::BigSmile, FX_SPARKLE},
  /* NearMiss  */ {EYE(26,30,0,0,0,0,0,Normal), EYE(24,24,0,0,0,0,0,Line), Mouth::Wobble, FX_SWEAT},
  /* Celebrate */ {EYE(26,30,0,14,0,0,-1,Normal), EYE(26,30,0,14,0,0,-1,Normal), Mouth::BigSmile, FX_SPARKLE},
  /* Waiting   */ {EYE(24,28,0,0,0,6,-4,Normal), EYE(24,28,0,0,0,6,-4,Normal), Mouth::None, FX_DOTS},
  /* Shy       */ {EYE(22,24,4,0,-3,-5,5,Normal), EYE(22,24,4,0,-3,-5,5,Normal), Mouth::Small, FX_BLUSH},
  /* Focused   */ {EYE(26,18,0,0,5,0,1,Normal), EYE(26,18,0,0,5,0,1,Normal), Mouth::Flat, FX_SPARKLE},
  /* Victorious*/ {EYE(26,26,0,0,0,0,0,Star), EYE(26,26,0,0,0,0,0,Star), Mouth::BigSmile, FX_SPARKLE},
  /* Satisfied */ {EYE(24,24,0,0,0,0,1,ArcUp), EYE(24,24,0,0,0,0,1,ArcUp), Mouth::BigSmile, FX_BLUSH},
  /* Amazed    */ {EYE(28,28,0,0,0,0,0,Ring), EYE(28,28,0,0,0,0,0,Ring), Mouth::O, FX_NONE},
  /* Precision */ {EYE(26,26,0,0,0,0,0,Crosshair), EYE(26,26,0,0,0,0,0,Crosshair), Mouth::Flat, FX_NONE},
};
#undef EYE

static const FaceDef RELAXED = {{26, 30, 14, 4, 0, 0, 2, EyeKind::Normal}, {26, 30, 14, 4, 0, 0, 2, EyeKind::Normal}, Mouth::Cat, FX_BLUSH};

struct ExprMeta { const char* key; const char* name; };
static const ExprMeta META[(int)Expr::Count] = {
  {"happy","Feliz"},{"neutral","Neutro"},{"sad","Triste"},{"surprised","Surpreso"},{"love","Apaixonado"},
  {"sleepy","Sonolento"},{"angry","Bravo"},{"dead","Olhos em X"},{"blink","Piscada"},{"wink_l","Piscadinha esq."},
  {"wink_r","Piscadinha dir."},{"pet","Carinho"},{"focus","Foco"},{"rest","Descanso"},{"hit","Acerto"},
  {"near_miss","Quase!"},{"celebrate","Comemoração"},{"waiting","Esperando"},
  {"shy","Tímido"},{"focused","Focado"},{"victorious","Vitorioso"},{"satisfied","Satisfeito"},{"amazed","Espantado"},{"precision","Precisão"},
};

const FaceDef& faceDef(Expr e) { return FACES[(int)e < (int)Expr::Count ? (int)e : 0]; }
const char* exprName(Expr e) { return META[(int)e < (int)Expr::Count ? (int)e : 0].name; }
const char* exprKey(Expr e) { return META[(int)e < (int)Expr::Count ? (int)e : 0].key; }
Expr exprFromKey(const char* k) {
  for (int i = 0; i < (int)Expr::Count; i++) if (tbStreq(META[i].key, k)) return (Expr)i;
  return Expr::Count;
}
bool exprIsUnlockable(Expr e, int* id) {
  int i = (int)e - (int)Expr::Shy;
  if (i >= 0 && i < 6) { if (id) *id = i; return true; }
  return false;
}
Expr exprForUnlock(int id) { return (Expr)((int)Expr::Shy + id); }

// ── desenho ─────────────────────────────────────────────────────────────────
static void thickLine(Framebuffer& fb, int x0, int y0, int x1, int y1) {
  for (int o = -1; o <= 1; o++) { fb.line(x0 + o, y0, x1 + o, y1); fb.line(x0, y0 + o, x1, y1 + o); }
}

static void drawStar(Framebuffer& fb, int cx, int cy) {
  static const int8_t OUT[5][2] = {{0,-13},{12,-4},{8,11},{-8,11},{-12,-4}};
  static const int8_t IN[5][2] = {{3,-4},{5,2},{0,6},{-5,2},{-3,-4}};
  for (int k = 0; k < 5; k++) {
    int k2 = (k + 4) % 5;
    fb.fillTriangle(cx + OUT[k][0], cy + OUT[k][1], cx + IN[k][0], cy + IN[k][1], cx + IN[k2][0], cy + IN[k2][1]);
    fb.fillTriangle(cx, cy, cx + IN[k][0], cy + IN[k][1], cx + IN[k2][0], cy + IN[k2][1]);
  }
}

static void drawEye(Framebuffer& fb, int cx, int cy, const EyeShape& e, bool rightEye, int blink, int lookX, int lookY) {
  int h = e.h - (e.h - 2) * blink / 1000;
  if (h < 2) h = 2;
  int ex = cx + e.dx + lookX, ey = cy + e.dy + lookY;
  if (blink > 700 && e.kind != EyeKind::Normal) {  // piscar em olhos especiais: traço fechado
    fb.fillRoundRect(ex - e.w / 2 + 2, ey - 1, e.w - 4, 3, 1);
    return;
  }
  switch (e.kind) {
    case EyeKind::Normal: {
      int x0 = ex - e.w / 2, y0 = ey - h / 2;
      fb.fillRoundRect(x0, y0, e.w, h, tbMin<int>(e.w, h) / 3);
      int lid = e.lidTop * h / tbMax<int>(1, e.h);
      if (lid > 0 || e.slope != 0) {
        int inner = lid + tbMax<int>(e.slope, 0), outer = lid + tbMax<int>(-e.slope, 0);
        for (int i = 0; i < e.w; i++) {
          // pálpebra reta inclinada: canto interno = lado do nariz
          int t = rightEye ? (e.w - 1 - i) : i;  // 0 = externo ... w-1 = interno
          int cut = outer + (inner - outer) * t / tbMax(1, e.w - 1);
          if (cut > 0) fb.vline(x0 + i, y0 - 1, cut + 1, false);
        }
      }
      if (e.lidBottom > 0 && h > 6) {
        int ry = h / 2 + 2;
        int lb = e.lidBottom * h / tbMax<int>(1, e.h);
        fb.fillEllipse(ex, y0 + h - lb + ry, e.w / 2 + 4, ry, false);
      }
      break;
    }
    case EyeKind::Heart:
      fb.heart(ex, ey - 2, tbMax(3, (e.w / 4) * (1000 - blink / 2) / 1000));
      break;
    case EyeKind::X:
      thickLine(fb, ex - 8, ey - 8, ex + 8, ey + 8);
      thickLine(fb, ex - 8, ey + 8, ex + 8, ey - 8);
      break;
    case EyeKind::ArcUp:
      fb.fillEllipse(ex, ey + 4, e.w / 2, 10);
      fb.fillEllipse(ex, ey + 9, e.w / 2, 10, false);
      fb.fillRect(ex - e.w / 2 - 1, ey + 4, e.w + 2, 12, false);
      fb.fillEllipse(ex, ey + 4, e.w / 2, 10);
      fb.fillEllipse(ex, ey + 9, e.w / 2 - 1, 10, false);
      break;
    case EyeKind::Line:
      fb.fillRoundRect(ex - e.w / 2 + 2, ey - 1, e.w - 4, 4, 2);
      break;
    case EyeKind::Star:
      drawStar(fb, ex, ey);
      break;
    case EyeKind::Ring:
      fb.fillCircle(ex, ey, 14); fb.fillCircle(ex, ey, 11, false);
      fb.fillCircle(ex + lookX / 2, ey + lookY / 2, 3);
      break;
    case EyeKind::Crosshair:
      fb.circle(ex, ey, 12); fb.circle(ex, ey, 11);
      fb.hline(ex - 15, ey, 9); fb.hline(ex + 7, ey, 9);
      fb.vline(ex, ey - 15, 9); fb.vline(ex, ey + 7, 9);
      fb.fillCircle(ex, ey, 1);
      break;
  }
}

static void drawMouth(Framebuffer& fb, Mouth m, ms_t now) {
  const int cx = 64, cy = 54;
  switch (m) {
    case Mouth::None: break;
    case Mouth::Smile:
      for (int x = -6; x <= 6; x++) { int y = 3 - (x * x * 3) / 36; fb.set(cx + x, cy + y); fb.set(cx + x, cy + y - 1); }
      break;
    case Mouth::Small:
      for (int x = -3; x <= 3; x++) { int y = 1 - (x * x) / 9; fb.set(cx + x, cy + y + 1); }
      break;
    case Mouth::BigSmile:
      fb.fillEllipse(cx, cy - 1, 8, 6);
      fb.fillRect(cx - 9, cy - 8, 19, 7, false);
      break;
    case Mouth::Frown:
      for (int x = -6; x <= 6; x++) { int y = (x * x * 3) / 36; fb.set(cx + x, cy + y); fb.set(cx + x, cy + y + 1); }
      break;
    case Mouth::O:
      fb.circle(cx, cy + 1, 4); fb.circle(cx, cy + 1, 3);
      break;
    case Mouth::Flat:
      fb.hline(cx - 6, cy + 1, 13); fb.hline(cx - 6, cy + 2, 13);
      break;
    case Mouth::Cat:
      for (int x = 0; x <= 4; x++) { int y = (x == 0 || x == 4) ? 0 : (x == 2 ? 0 : 1); fb.set(cx - 4 + x, cy + y + 1); fb.set(cx + x, cy + y + 1); }
      fb.set(cx - 2, cy + 3); fb.set(cx + 2, cy + 3);
      break;
    case Mouth::Wobble: {
      int ph = (int)((now / 150) % 2);
      for (int x = -6; x <= 6; x++) fb.set(cx + x, cy + 1 + (((x + 6 + ph) / 2) % 2));
      break;
    }
  }
}

static void sparkle(Framebuffer& fb, int x, int y, int s) {
  fb.hline(x - s, y, 2 * s + 1); fb.vline(x, y - s, 2 * s + 1);
}

static void drawExtras(Framebuffer& fb, uint8_t ex, ms_t now) {
  if (ex & FX_BLUSH) {
    for (int k = 0; k < 3; k++) {
      fb.line(26 + k * 4, 49, 28 + k * 4, 46);
      fb.line(94 + k * 4, 49, 96 + k * 4, 46);
    }
  }
  if (ex & FX_ZZZ) {
    int ph = (int)((now / 400) % 3);
    fb.text(108, 10 - ph * 2, "z");
    if (ph > 0) fb.text(116, 4 - (ph - 1) * 2, "Z");
  }
  if (ex & FX_SPARKLE) {
    int ph = (int)((now / 250) % 2);
    sparkle(fb, 10, 10, 2 + ph); sparkle(fb, 118, 12, 3 - ph); sparkle(fb, 64, 5, 1 + ph);
  }
  if (ex & FX_HEARTS) {
    for (int k = 0; k < 3; k++) {
      int t = (int)(((now / 30) + (ms_t)k * 23) % 64);
      int x = k == 0 ? 8 : (k == 1 ? 120 : 64);
      int y = 60 - t;
      if (k == 2) y = 62 - t / 2 - 30;
      if (y > -6 && y < 64 && !(k == 2 && (y > 10))) fb.heart(x, y, 2);
    }
  }
  if (ex & FX_SWEAT) {
    fb.fillCircle(110, 14, 2); fb.fillTriangle(108, 13, 112, 13, 110, 8);
  }
  if (ex & FX_DOTS) {
    int n = (int)((now / 350) % 4);
    for (int k = 0; k < n; k++) fb.fillRect(56 + k * 6, 54, 3, 3);
  }
}

void FaceRenderer::drawFace(Framebuffer& fb, const FaceDef& f, ms_t now, int lookX, int lookY, int blink) {
  drawEye(fb, kLeftX, kEyeY, f.L, false, blink, lookX, lookY);
  drawEye(fb, kRightX, kEyeY, f.R, true, blink, lookX, lookY);
  if (blink < 500) drawMouth(fb, f.mouth, now);
  drawExtras(fb, f.extras, now);
}

// ── estado e animação ───────────────────────────────────────────────────────
void FaceRenderer::reset(ms_t now) {
  from_ = target_ = Expr::Happy; transMs_ = 0; transStart_ = now;
  nextBlink_ = now + 1500; nextLook_ = now + 2500; blinking_ = false;
  lookX_ = lookY_ = lookTX_ = lookTY_ = 0; microActive_ = false;
}

void FaceRenderer::setExpression(Expr e, ms_t now, bool instant) {
  if ((int)e >= (int)Expr::Count) return;
  if (e == target_ && !instant) return;
  from_ = instant ? e : target_;
  target_ = e;
  transStart_ = now;
  transMs_ = instant ? 0 : 260;
  microActive_ = false;
}

bool FaceRenderer::animating(ms_t now) const {
  return (now - transStart_ < transMs_) || blinking_ || (now - lookMoveStart_ < 220) || microActive_ ||
         (faceDef(target_).extras != FX_NONE) || petting_;
}

void FaceRenderer::update(ms_t now, Rng& rng, const PersonalityTuning& t, uint8_t animLevel) {
  // fator de animação: 0 calma (intervalos ×1,6), 1 normal, 2 animada (×0,7)
  int f = animLevel == 0 ? 160 : (animLevel == 2 ? 70 : 100);
  if (microActive_ && now >= microUntil_) microActive_ = false;
  if (!blinking_ && now >= nextBlink_) {
    blinking_ = true; blinkStart_ = now;
    secondBlink_ = (int)rng.range(0, 99) < t.doubleBlinkPct;
  }
  if (blinking_ && now - blinkStart_ >= 160) {
    if (secondBlink_) { secondBlink_ = false; blinkStart_ = now + 60; }
    else {
      blinking_ = false;
      nextBlink_ = now + (ms_t)rng.range(t.blinkMinMs, t.blinkMaxMs) * f / 100;
    }
  }
  if (now >= nextLook_) {
    lookFromX_ = lookX_; lookFromY_ = lookY_;
    if (rng.range(0, 2) == 0) { lookTX_ = 0; lookTY_ = 0; }
    else { lookTX_ = rng.range(-t.lookRangeX, t.lookRangeX); lookTY_ = rng.range(-t.lookRangeY, t.lookRangeY); }
    lookMoveStart_ = now;
    nextLook_ = now + (ms_t)rng.range(t.lookMinMs, t.lookMaxMs) * f / 100;
    if ((target_ == Expr::Happy || target_ == Expr::Neutral) && (int)rng.range(0, 99) < t.mischiefPct) {
      static const Expr micro[] = {Expr::WinkL, Expr::WinkR, Expr::Surprised};
      micro_ = micro[rng.range(0, 2)]; microActive_ = true; microUntil_ = now + 700;
    }
  }
  ms_t lt = now - lookMoveStart_;
  if (lt >= 200) { lookX_ = lookTX_; lookY_ = lookTY_; }
  else {
    // ease-out
    int p = (int)lt * 1000 / 200; p = 1000 - (1000 - p) * (1000 - p) / 1000;
    lookX_ = lookFromX_ + (lookTX_ - lookFromX_) * p / 1000;
    lookY_ = lookFromY_ + (lookTY_ - lookFromY_) * p / 1000;
  }
}

static int lerpI(int a, int b, int p) { return a + (b - a) * p / 1000; }
static EyeShape lerpEye(const EyeShape& a, const EyeShape& b, int p) {
  EyeShape r = b;
  r.w = (int8_t)lerpI(a.w, b.w, p); r.h = (int8_t)lerpI(a.h, b.h, p);
  r.lidTop = (int8_t)lerpI(a.lidTop, b.lidTop, p); r.lidBottom = (int8_t)lerpI(a.lidBottom, b.lidBottom, p);
  r.slope = (int8_t)lerpI(a.slope, b.slope, p); r.dx = (int8_t)lerpI(a.dx, b.dx, p); r.dy = (int8_t)lerpI(a.dy, b.dy, p);
  return r;
}

void FaceRenderer::draw(Framebuffer& fb, ms_t now) {
  int blink = 0;
  if (blinking_ && now >= blinkStart_) {
    int bt = (int)(now - blinkStart_);
    blink = bt < 70 ? bt * 1000 / 70 : (bt < 90 ? 1000 : tbMax(0, 1000 - (bt - 90) * 1000 / 70));
  }
  Expr cur = microActive_ ? micro_ : target_;
  const FaceDef* tgt = petting_ && target_ != Expr::Love ? &RELAXED : &faceDef(cur);
  ms_t tt = now - transStart_;
  int lx = lookX_, ly = lookY_;
  const FaceDef& src = faceDef(from_);
  if (tt < transMs_ && from_ != target_) {
    int p = (int)(tt * 1000 / transMs_);
    bool sameKind = src.L.kind == tgt->L.kind && src.R.kind == tgt->R.kind;
    if (sameKind) {
      FaceDef mix = *tgt;
      mix.L = lerpEye(src.L, tgt->L, p); mix.R = lerpEye(src.R, tgt->R, p);
      mix.mouth = p < 500 ? src.mouth : tgt->mouth;
      mix.extras = p < 500 ? src.extras : tgt->extras;
      drawFace(fb, mix, now, lx, ly, blink);
    } else {
      // troca de forma passando por uma piscada
      if (p < 500) drawFace(fb, src, now, lx, ly, tbMax(blink, p * 2));
      else drawFace(fb, *tgt, now, lx, ly, tbMax(blink, (1000 - p) * 2));
    }
    return;
  }
  drawFace(fb, *tgt, now, lx, ly, blink);
}

}  // namespace tb
