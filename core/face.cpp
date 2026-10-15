#include "face.h"

namespace tb {

#define EYE(w, h, lt, lb, sl, dx, dy, k) {w, h, lt, lb, sl, dx, dy, EyeKind::k}
static const FaceDef FACES[(int)Expr::Count] = {
  /* Happy     */ {EYE(96,120,0,36,0,0,0,Normal), EYE(96,120,0,36,0,0,0,Normal), Mouth::Smile, FX_NONE},
  /* Neutral   */ {EYE(96,120,0,0,0,0,0,Normal), EYE(96,120,0,0,0,0,0,Normal), Mouth::None, FX_NONE},
  /* Sad       */ {EYE(92,104,28,0,-24,0,12,Normal), EYE(92,104,28,0,-24,0,12,Normal), Mouth::Frown, FX_TEAR},
  /* Surprised */ {EYE(104,138,0,0,0,0,-8,Normal), EYE(104,138,0,0,0,0,-8,Normal), Mouth::O, FX_NONE},
  /* Love      */ {EYE(118,118,0,0,0,0,0,Heart), EYE(118,118,0,0,0,0,0,Heart), Mouth::Smile, FX_HEARTS},
  /* Sleepy    */ {EYE(96,120,74,0,0,0,14,Normal), EYE(96,120,74,0,0,0,14,Normal), Mouth::Small, FX_ZZZ},
  /* Angry     */ {EYE(96,110,30,0,32,0,8,Normal), EYE(96,110,30,0,32,0,8,Normal), Mouth::Flat, FX_ANGER},
  /* Dead      */ {EYE(80,80,0,0,0,0,0,X), EYE(80,80,0,0,0,0,0,X), Mouth::Wobble, FX_NONE},
  /* Blink     */ {EYE(96,14,0,0,0,0,0,Line), EYE(96,14,0,0,0,0,0,Line), Mouth::None, FX_NONE},
  /* WinkL     */ {EYE(92,92,0,0,0,0,0,ArcUp), EYE(96,120,0,28,0,0,0,Normal), Mouth::Smile, FX_NONE},
  /* WinkR     */ {EYE(96,120,0,28,0,0,0,Normal), EYE(92,92,0,0,0,0,0,ArcUp), Mouth::Smile, FX_NONE},
  /* Pet       */ {EYE(92,92,0,0,0,0,4,ArcUp), EYE(92,92,0,0,0,0,4,ArcUp), Mouth::Cat, FX_BLUSH},
  /* Focus     */ {EYE(96,64,0,0,12,0,8,Normal), EYE(96,64,0,0,12,0,8,Normal), Mouth::Flat, FX_NONE},
  /* Rest      */ {EYE(92,92,0,0,0,0,10,ArcDown), EYE(92,92,0,0,0,0,10,ArcDown), Mouth::Small, FX_NONE},
  /* Hit       */ {EYE(92,92,0,0,0,0,0,ArcUp), EYE(92,92,0,0,0,0,0,ArcUp), Mouth::BigSmile, FX_SPARKLE},
  /* NearMiss  */ {EYE(96,120,0,0,0,0,0,Normal), EYE(92,16,0,0,0,0,0,Line), Mouth::Wobble, FX_SWEAT},
  /* Celebrate */ {EYE(98,122,0,36,0,0,-4,Normal), EYE(98,122,0,36,0,0,-4,Normal), Mouth::BigSmile, FX_SPARKLE},
  /* Waiting   */ {EYE(88,110,0,0,0,24,-16,Normal), EYE(88,110,0,0,0,24,-16,Normal), Mouth::None, FX_DOTS},
  /* Shy       */ {EYE(84,96,16,0,-10,-20,20,Normal), EYE(84,96,16,0,-10,-20,20,Normal), Mouth::Small, FX_BLUSH},
  /* Focused   */ {EYE(96,72,0,0,18,0,4,Normal), EYE(96,72,0,0,18,0,4,Normal), Mouth::Flat, FX_SPARKLE},
  /* Victorious*/ {EYE(110,110,0,0,0,0,0,Star), EYE(110,110,0,0,0,0,0,Star), Mouth::BigSmile, FX_SPARKLE},
  /* Satisfied */ {EYE(92,92,0,0,0,0,4,ArcUp), EYE(92,92,0,0,0,0,4,ArcUp), Mouth::BigSmile, FX_BLUSH},
  /* Amazed    */ {EYE(108,108,0,0,0,0,0,Ring), EYE(108,108,0,0,0,0,0,Ring), Mouth::O, FX_NONE},
  /* Precision */ {EYE(104,104,0,0,0,0,0,Crosshair), EYE(104,104,0,0,0,0,0,Crosshair), Mouth::Flat, FX_NONE},
  /* Groovy    */ {EYE(92,92,0,0,0,0,0,ArcUp), EYE(92,92,0,0,0,0,0,ArcUp), Mouth::BigSmile, FX_NOTES},
  /* Zen       */ {EYE(92,92,0,0,0,0,6,ArcDown), EYE(92,92,0,0,0,0,6,ArcDown), Mouth::Small, FX_AURA},
  /* Diver     */ {EYE(100,100,0,0,0,0,0,Goggle), EYE(100,100,0,0,0,0,0,Goggle), Mouth::Smile, FX_BUBBLES},
  /* Radiant   */ {EYE(98,122,0,32,0,0,0,Normal), EYE(98,122,0,32,0,0,0,Normal), Mouth::BigSmile, FX_SPARKLE | FX_RAINBOW},
  /* Dizzy     */ {EYE(100,100,0,0,0,0,0,Spiral), EYE(100,100,0,0,0,0,0,Spiral), Mouth::Wobble, FX_STARS},
  /* Startled  */ {EYE(112,150,0,0,0,0,-12,Normal), EYE(112,150,0,0,0,0,-12,Normal), Mouth::O, FX_SWEAT},
  /* Squint    */ {EYE(90,90,0,0,0,0,0,Squint), EYE(90,90,0,0,0,0,0,Squint), Mouth::Frown, FX_NONE},
  /* Charging  */ {EYE(92,92,0,0,0,0,6,ArcDown), EYE(92,92,0,0,0,0,6,ArcDown), Mouth::Small, FX_BOLT},
  /* Tired     */ {EYE(92,104,56,0,0,0,12,Normal), EYE(92,104,56,0,0,0,12,Normal), Mouth::Flat, FX_NONE},
  /* Curious   */ {EYE(100,128,0,0,0,0,-6,Normal), EYE(88,96,22,0,-12,0,4,Normal), Mouth::Small, FX_NONE},
};
#undef EYE

static const FaceDef RELAXED = {{96, 120, 50, 30, 0, 0, 8, EyeKind::Normal}, {96, 120, 50, 30, 0, 0, 8, EyeKind::Normal}, Mouth::Cat, FX_BLUSH};

struct ExprMeta { const char* key; const char* name; };
static const ExprMeta META[(int)Expr::Count] = {
  {"happy","Feliz"},{"neutral","Neutro"},{"sad","Triste"},{"surprised","Surpreso"},{"love","Apaixonado"},
  {"sleepy","Sonolento"},{"angry","Bravo"},{"dead","Olhos em X"},{"blink","Piscada"},{"wink_l","Piscadinha esq."},
  {"wink_r","Piscadinha dir."},{"pet","Carinho"},{"focus","Foco"},{"rest","Descanso"},{"hit","Acerto"},
  {"near_miss","Quase!"},{"celebrate","Comemoração"},{"waiting","Esperando"},
  {"shy","Tímido"},{"focused","Focado"},{"victorious","Vitorioso"},{"satisfied","Satisfeito"},{"amazed","Espantado"},{"precision","Precisão"},
  {"groovy","Dançante"},{"zen","Zen"},{"diver","Mergulhador"},{"radiant","Radiante"},
  {"dizzy","Tonto"},{"startled","Assustado"},{"squint","Apertado"},{"charging","Carregando"},{"tired","Cansado"},{"curious","Curioso"},
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
  if (i >= 0 && i < 10) { if (id) *id = i; return true; }
  return false;
}
Expr exprForUnlock(int id) { return (Expr)((int)Expr::Shy + id); }
bool exprIsReaction(Expr e) { return (int)e >= (int)Expr::Dizzy && (int)e < (int)Expr::Count; }

// ── cores utilitárias ───────────────────────────────────────────────────────
static Color hue(int deg) {
  deg %= 360; if (deg < 0) deg += 360;
  int s = deg / 60, f = (deg % 60) * 255 / 60;
  int q = 255 - f;
  int r = 0, g = 0, b = 0;
  switch (s) { case 0: r = 255; g = f; break; case 1: r = q; g = 255; break; case 2: g = 255; b = f; break;
               case 3: g = q; b = 255; break; case 4: r = f; b = 255; break; default: r = 255; b = q; }
  return (Color)((r << 16) | (g << 8) | b);
}

static const Color C_PINK = 0xFF5C9A, C_BLUSH = 0xFF6FA0, C_GOLD = 0xFFD54A, C_WATER = 0x5CC8FF, C_RED = 0xFF4B4B;

// ── desenho ─────────────────────────────────────────────────────────────────
struct Ctx {
  Canvas& c; const FaceXf& xf; float offX, offY;
  float X(float x) const { return xf.cx + (x - (float)SCR_CX + offX) * xf.s; }
  float Y(float y) const { return xf.cy + (y - (float)SCR_CY + offY) * xf.s; }
  float S(float v) const { return v * xf.s; }
};

static void spiral(const Ctx& k, float cx, float cy, float r, float rot, Color col) {
  float px = cx, py = cy;
  for (int i = 1; i <= 36; i++) {
    float t = (float)i / 36.f;
    float ang = rot + t * kTau * 2.2f;
    float rr = r * t;
    float x = cx + rr * fsin(ang), y = cy - rr * fcos(ang);
    k.c.line(k.X(px), k.Y(py), k.X(x), k.Y(y), k.S(9.f), col);
    px = x; py = y;
  }
}

static void drawEye(const Ctx& k, float cx, float cy, const EyeShape& e, bool rightEye, int blink, float lookX, float lookY,
                    float breathe, Color col, const FaceStyle& st, ms_t now) {
  float w = (float)e.w, h0 = (float)e.h;
  float h = h0 * (1.f + breathe * 0.03f);
  float bl = (float)blink / 1000.f;
  h = h * (1.f - 0.88f * bl);
  if (h < 10.f) h = 10.f;
  float ex = cx + (float)e.dx + lookX;
  float ey = cy + (float)e.dy + lookY + (h0 - h) * 0.25f;
  if (st.glow && e.kind != EyeKind::X && e.kind != EyeKind::Spiral) {
    float lidv = e.kind == EyeKind::Normal ? (float)e.lidTop * h / h0 : 0.f;
    float vis = h - lidv;
    k.c.glow(k.X(ex), k.Y(ey - h * 0.5f + lidv + vis * 0.5f), k.S(fmaxf_(w, vis) * 0.95f), col, 75);
  }

  if (blink > 600 && e.kind != EyeKind::Normal) {  // olhos especiais piscam como traço
    k.c.fillRoundRect(k.X(ex - w * 0.42f), k.Y(ey - 7.f), k.S(w * 0.84f), k.S(14.f), k.S(7.f), col);
    return;
  }
  switch (e.kind) {
    case EyeKind::Normal: {
      float hx = w * 0.5f, hy = h * 0.5f;
      float rad = fminf_(hx, hy) * 0.78f;
      float top = ey - hy;
      float lid = (float)e.lidTop * h / h0;
      float slope = (float)e.slope;
      // pálpebra superior: reta do canto externo ao interno
      float xo = rightEye ? ex + hx : ex - hx, xi = rightEye ? ex - hx : ex + hx;
      float yo = top + lid + fmaxf_(-slope, 0.f) - 1.f, yi = top + lid + fmaxf_(slope, 0.f) - 1.f;
      bool hasLid = lid > 0.5f || slope != 0.f;
      float m = (yi - yo) / (xi - xo);
      float inv = 1.f / fsqrt(1.f + m * m);
      float lb = (float)e.lidBottom * h / h0;
      bool hasCheek = lb > 0.5f && h > 24.f;
      float cry = h * 0.40f, crx = hx * 1.12f, ccy = ey + hy - lb + cry;
      float s = k.xf.s;
      float sx = k.X(0), sy = k.Y(0);  // para converter tela -> espaço de desenho
      k.c.shade(k.X(ex - hx), k.Y(ey - hy), k.X(ex + hx), k.Y(ey + hy), col, 255, [=](float px, float py) {
        float x = (px - sx) / s, y = (py - sy) / s;  // espaço de desenho
        float qx = fabsf_(x - ex) - (hx - rad), qy = fabsf_(y - ey) - (hy - rad);
        float d = fsqrt(fmaxf_(qx, 0.f) * fmaxf_(qx, 0.f) + fmaxf_(qy, 0.f) * fmaxf_(qy, 0.f)) + fminf_(fmaxf_(qx, qy), 0.f) - rad;
        if (hasLid) {
          float ly = yo + m * (x - xo);
          d = fmaxf_(d, (ly - y) * inv);
        }
        if (hasCheek) {
          float ux = (x - ex) / crx, uy = (y - ccy) / cry;
          float k0 = fsqrt(ux * ux + uy * uy);
          float k1 = fsqrt(ux * ux / (crx * crx) + uy * uy / (cry * cry));
          float de = k1 > 1e-6f ? k0 * (k0 - 1.f) / k1 : -cry;
          d = fmaxf_(d, -de);
        }
        return d * s;
      });
      // brilho (reflexo) — dá vida ao olhar
      if (h > 50.f && lid < h * 0.35f) {
        float hr = w * 0.10f;
        float hxp = ex - hx * 0.42f, hyp = top + lid + h * 0.22f;
        k.c.fillCircle(k.X(hxp), k.Y(hyp), k.S(hr), 0xFFFFFF, 205);
        k.c.fillCircle(k.X(hxp + hr * 1.7f), k.Y(hyp + hr * 1.6f), k.S(hr * 0.45f), 0xFFFFFF, 150);
      }
      break;
    }
    case EyeKind::Heart: {
      float pulse = 1.f + 0.06f * fsin((float)(now % 900) / 900.f * kTau);
      k.c.heart(k.X(ex), k.Y(ey), k.S(w * 0.95f * pulse * (1.f - 0.4f * bl)), C_PINK);
      k.c.fillCircle(k.X(ex - w * 0.22f), k.Y(ey - w * 0.16f), k.S(w * 0.08f), 0xFFFFFF, 190);
      break;
    }
    case EyeKind::X: {
      float r = w * 0.38f;
      k.c.line(k.X(ex - r), k.Y(ey - r), k.X(ex + r), k.Y(ey + r), k.S(18.f), col);
      k.c.line(k.X(ex - r), k.Y(ey + r), k.X(ex + r), k.Y(ey - r), k.S(18.f), col);
      break;
    }
    case EyeKind::ArcUp:
      k.c.arc(k.X(ex), k.Y(ey + w * 0.22f), k.S(w * 0.40f), k.S(20.f), -1.15f, 2.3f, col);
      break;
    case EyeKind::ArcDown:
      k.c.arc(k.X(ex), k.Y(ey - w * 0.22f), k.S(w * 0.40f), k.S(18.f), kPi - 1.15f, 2.3f, col);
      break;
    case EyeKind::Line:
      k.c.fillRoundRect(k.X(ex - w * 0.45f), k.Y(ey - h * 0.5f), k.S(w * 0.9f), k.S(fmaxf_(h, 14.f)), k.S(8.f), col);
      break;
    case EyeKind::Star: {
      float rot = 0.12f * fsin((float)(now % 2000) / 2000.f * kTau);
      k.c.star(k.X(ex), k.Y(ey), k.S(w * 0.55f), k.S(w * 0.24f), C_GOLD, 255, rot);
      break;
    }
    case EyeKind::Ring:
      k.c.ring(k.X(ex), k.Y(ey), k.S(w * 0.40f), k.S(16.f), col);
      k.c.fillCircle(k.X(ex + lookX * 0.4f), k.Y(ey + lookY * 0.4f), k.S(w * 0.14f), col);
      break;
    case EyeKind::Crosshair: {
      float r = w * 0.36f;
      k.c.ring(k.X(ex), k.Y(ey), k.S(r), k.S(8.f), col);
      k.c.line(k.X(ex - r - 18), k.Y(ey), k.X(ex - r * 0.45f), k.Y(ey), k.S(8.f), col);
      k.c.line(k.X(ex + r * 0.45f), k.Y(ey), k.X(ex + r + 18), k.Y(ey), k.S(8.f), col);
      k.c.line(k.X(ex), k.Y(ey - r - 18), k.X(ex), k.Y(ey - r * 0.45f), k.S(8.f), col);
      k.c.line(k.X(ex), k.Y(ey + r * 0.45f), k.X(ex), k.Y(ey + r + 18), k.S(8.f), col);
      k.c.fillCircle(k.X(ex), k.Y(ey), k.S(7.f), C_RED);
      break;
    }
    case EyeKind::Spiral: {
      float rot = (float)(now % 1200) / 1200.f * kTau * (rightEye ? -1.f : 1.f);
      spiral(k, ex, ey, w * 0.46f, rot, col);
      break;
    }
    case EyeKind::Squint: {
      float r = w * 0.36f, dir = rightEye ? -1.f : 1.f;  // > <
      k.c.line(k.X(ex - r * dir), k.Y(ey - r), k.X(ex + r * dir), k.Y(ey), k.S(18.f), col);
      k.c.line(k.X(ex + r * dir), k.Y(ey), k.X(ex - r * dir), k.Y(ey + r), k.S(18.f), col);
      break;
    }
    case EyeKind::Goggle: {
      k.c.ring(k.X(ex), k.Y(ey), k.S(w * 0.50f), k.S(12.f), 0xFFB547);
      k.c.fillCircle(k.X(ex), k.Y(ey), k.S(w * 0.42f), 0x0E3B55);
      k.c.fillRoundRect(k.X(ex - w * 0.20f), k.Y(ey - w * 0.26f), k.S(w * 0.40f), k.S(w * 0.52f), k.S(w * 0.18f), col);
      k.c.fillCircle(k.X(ex - w * 0.08f), k.Y(ey - w * 0.12f), k.S(w * 0.06f), 0xFFFFFF, 200);
      break;
    }
  }
}

static void drawMouth(const Ctx& k, Mouth m, float dx, ms_t now, Color col) {
  const float cx = 233.f + dx, cy = (float)FaceRenderer::kMouthY;
  switch (m) {
    case Mouth::None: break;
    case Mouth::Smile: k.c.arc(k.X(cx), k.Y(cy - 26), k.S(34.f), k.S(11.f), kPi - 0.95f, 1.9f, col); break;
    case Mouth::Small: k.c.arc(k.X(cx), k.Y(cy - 12), k.S(18.f), k.S(9.f), kPi - 0.9f, 1.8f, col); break;
    case Mouth::Frown: k.c.arc(k.X(cx), k.Y(cy + 30), k.S(30.f), k.S(10.f), -0.9f, 1.8f, col); break;
    case Mouth::O: k.c.ring(k.X(cx), k.Y(cy), k.S(16.f), k.S(9.f), col); break;
    case Mouth::Open: k.c.fillEllipse(k.X(cx), k.Y(cy), k.S(20.f), k.S(24.f), col); break;
    case Mouth::Flat: k.c.line(k.X(cx - 26), k.Y(cy), k.X(cx + 26), k.Y(cy), k.S(10.f), col); break;
    case Mouth::Cat:
      k.c.arc(k.X(cx - 14), k.Y(cy - 8), k.S(14.f), k.S(8.f), kPi - 1.4f, 2.8f, col);
      k.c.arc(k.X(cx + 14), k.Y(cy - 8), k.S(14.f), k.S(8.f), kPi - 1.4f, 2.8f, col);
      break;
    case Mouth::BigSmile: {
      float s = k.xf.s, X0 = k.X(cx), Y0 = k.Y(cy - 14);
      float rx = 40.f * s, ry = 36.f * s;
      k.c.shade(X0 - rx, Y0, X0 + rx, Y0 + ry, col, 255, [=](float px, float py) {
        float ux = (px - X0) / rx, uy = (py - Y0) / ry;
        float k0 = fsqrt(ux * ux + uy * uy), k1 = fsqrt(ux * ux / (rx * rx) + uy * uy / (ry * ry));
        float de = k1 > 1e-6f ? k0 * (k0 - 1.f) / k1 : -ry;
        return fmaxf_(de, Y0 - py);
      });
      k.c.shade(X0 - rx, Y0, X0 + rx, Y0 + ry, C_PINK, 255, [=](float px, float py) {
        float ux = (px - X0) / rx, uy = (py - Y0) / ry;
        float k0 = fsqrt(ux * ux + uy * uy), k1 = fsqrt(ux * ux / (rx * rx) + uy * uy / (ry * ry));
        float de = k1 > 1e-6f ? k0 * (k0 - 1.f) / k1 : -ry;
        float dt = fsqrt((px - X0) * (px - X0) + (py - (Y0 + ry)) * (py - (Y0 + ry))) - 20.f * s;
        return fmaxf_(de + 6.f * s, dt);
      });
      break;
    }
    case Mouth::Wobble: {
      float ph = (float)(now % 600) / 600.f * kTau;
      float px = cx - 30, py = cy + 6.f * fsin(ph);
      for (int i = 1; i <= 10; i++) {
        float x = cx - 30 + i * 6.f, y = cy + 6.f * fsin(ph + (float)i * 0.9f);
        k.c.line(k.X(px), k.Y(py), k.X(x), k.Y(y), k.S(8.f), col);
        px = x; py = y;
      }
      break;
    }
  }
}

static void sparkle(const Ctx& k, float x, float y, float r, Color col, int a) {
  const Pt v[8] = {{k.X(x), k.Y(y - r)}, {k.X(x + r * 0.22f), k.Y(y - r * 0.22f)}, {k.X(x + r), k.Y(y)},
                   {k.X(x + r * 0.22f), k.Y(y + r * 0.22f)}, {k.X(x), k.Y(y + r)}, {k.X(x - r * 0.22f), k.Y(y + r * 0.22f)},
                   {k.X(x - r), k.Y(y)}, {k.X(x - r * 0.22f), k.Y(y - r * 0.22f)}};
  k.c.fillPolygon(v, 8, col, a);
}

static void note(const Ctx& k, float x, float y, float s, Color col, int a) {
  k.c.fillEllipse(k.X(x), k.Y(y), k.S(9.f * s), k.S(7.f * s), col, a);
  k.c.line(k.X(x + 8 * s), k.Y(y), k.X(x + 8 * s), k.Y(y - 32 * s), k.S(4.f * s), col, a);
  k.c.line(k.X(x + 8 * s), k.Y(y - 32 * s), k.X(x + 20 * s), k.Y(y - 24 * s), k.S(5.f * s), col, a);
}

static void drop(const Ctx& k, float x, float y, float r, Color col, int a) {
  k.c.fillCircle(k.X(x), k.Y(y), k.S(r), col, a);
  k.c.fillTriangle({k.X(x - r * 0.92f), k.Y(y - r * 0.38f)}, {k.X(x + r * 0.92f), k.Y(y - r * 0.38f)}, {k.X(x), k.Y(y - r * 2.4f)}, col, a);
}

static void drawExtras(const Ctx& k, uint16_t ex, ms_t now, Color col) {
  if (ex & FX_AURA) {
    float p = 0.5f + 0.5f * fsin((float)(now % 4000) / 4000.f * kTau);
    k.c.ring(k.X(233), k.Y(233), k.S(196.f + 8.f * p), k.S(6.f), col, 60 + (int)(60 * p));
    k.c.ring(k.X(233), k.Y(233), k.S(176.f + 6.f * p), k.S(3.f), col, 40 + (int)(40 * p));
  }
  if (ex & FX_BLUSH) {
    k.c.fillEllipse(k.X(112), k.Y(292), k.S(32.f), k.S(15.f), C_BLUSH, 120);
    k.c.fillEllipse(k.X(354), k.Y(292), k.S(32.f), k.S(15.f), C_BLUSH, 120);
  }
  if (ex & FX_ZZZ) {
    for (int i = 0; i < 3; i++) {
      int t = (int)((now + (ms_t)i * 700) % 2100);
      float p = (float)t / 2100.f;
      float x = 318 + 28 * i + 10 * fsin(p * kTau), y = 150 - 60 * p - 20 * i;
      int a = (int)(255 * (1.f - p));
      k.c.text(k.X(x), k.Y(y), i == 2 ? "Z" : "z", i == 0 ? FONT_M : (i == 1 ? FONT_L : FONT_XL), 0xA9B4FF, a, Align::Center);
    }
  }
  if (ex & FX_SPARKLE) {
    static const int16_t P[5][3] = {{70, 110, 22}, {395, 120, 26}, {233, 66, 16}, {60, 300, 14}, {408, 310, 18}};
    for (int i = 0; i < 5; i++) {
      float ph = fsin((float)((now + (ms_t)i * 377) % 1100) / 1100.f * kTau);
      sparkle(k, P[i][0], P[i][1], (float)P[i][2] * (0.7f + 0.3f * ph), i & 1 ? C_GOLD : 0xFFFFFF, 220);
    }
  }
  if (ex & FX_HEARTS) {
    for (int i = 0; i < 4; i++) {
      int t = (int)((now + (ms_t)i * 650) % 2600);
      float p = (float)t / 2600.f;
      float x = (i & 1 ? 380.f : 86.f) + 16.f * fsin(p * kTau * 1.5f + (float)i), y = 380.f - 280.f * p;
      k.c.heart(k.X(x), k.Y(y), k.S(30.f + 6.f * (float)(i % 2)), C_PINK, (int)(230 * (1.f - p * p)));
    }
  }
  if (ex & FX_SWEAT) drop(k, 392, 120 + (float)(now % 1400) / 1400.f * 14.f, 13, C_WATER, 220);
  if (ex & FX_TEAR) {
    float p = (float)(now % 1800) / 1800.f;
    drop(k, 120, 268 + 70 * p, 10, C_WATER, (int)(230 * (1.f - p)));
  }
  if (ex & FX_DOTS) {
    int n = (int)((now / 350) % 4);
    for (int i = 0; i < n; i++) k.c.fillCircle(k.X(205 + i * 28), k.Y(320), k.S(8.f), col);
  }
  if (ex & FX_NOTES) {
    for (int i = 0; i < 3; i++) {
      int t = (int)((now + (ms_t)i * 800) % 2400);
      float p = (float)t / 2400.f;
      float x = (i == 1 ? 390.f : 70.f + 20.f * (float)i) + 12.f * fsin(p * kTau), y = 140.f - 60.f * p + (float)i * 40.f;
      note(k, x, y, 1.f, i == 1 ? C_GOLD : 0xB592FF, (int)(255 * (1.f - p)));
    }
  }
  if (ex & FX_STARS) {
    float base = (float)(now % 1600) / 1600.f * kTau;
    for (int i = 0; i < 4; i++) {
      float a = base + (float)i * kTau / 4.f;
      float x = 233.f + 150.f * fsin(a), y = 96.f + 26.f * fcos(a);
      k.c.star(k.X(x), k.Y(y), k.S(15.f), k.S(6.f), C_GOLD, 230);
    }
  }
  if (ex & FX_ANGER) {
    float x = 372, y = 96, s = 1.f + 0.1f * fsin((float)(now % 500) / 500.f * kTau);
    for (int i = 0; i < 4; i++) {
      float a = (float)i * kPi * 0.5f + 0.785f;
      k.c.arc(k.X(x + 14 * s * fsin(a)), k.Y(y - 14 * s * fcos(a)), k.S(11.f * s), k.S(6.f), a + kPi - 0.8f, 1.6f, C_RED);
    }
  }
  if (ex & FX_BOLT) {
    int a = 140 + (int)(115 * (0.5f + 0.5f * fsin((float)(now % 1200) / 1200.f * kTau)));
    Pt b[6] = {{k.X(240), k.Y(56)}, {k.X(206), k.Y(116)}, {k.X(230), k.Y(116)}, {k.X(220), k.Y(160)}, {k.X(262), k.Y(96)}, {k.X(238), k.Y(96)}};
    k.c.fillPolygon(b, 6, C_GOLD, a);
  }
  if (ex & FX_BUBBLES) {
    for (int i = 0; i < 5; i++) {
      int t = (int)((now + (ms_t)i * 520) % 2600);
      float p = (float)t / 2600.f;
      float x = 60.f + (float)i * 86.f + 10.f * fsin(p * kTau * 2.f + (float)i), y = 420.f - 340.f * p;
      k.c.ring(k.X(x), k.Y(y), k.S(8.f + 3.f * (float)(i % 3)), k.S(3.f), C_WATER, (int)(200 * (1.f - p)));
    }
  }
}

void FaceRenderer::drawFace(Canvas& c, const FaceDef& f, const FaceStyle& st, ms_t now, const FaceXf& xf,
                            float lookX, float lookY, int blink, float breathe) {
  Ctx k{c, xf, 0.f, 0.f};
  Color col = st.eye;
  if (f.extras & FX_RAINBOW) col = hue((int)((now / 12) % 360));
  drawExtras(k, (uint16_t)(f.extras & (FX_AURA | FX_BLUSH)), now, col);
  float by = breathe * 3.f;
  drawEye(k, (float)kLeftX, (float)kEyeY + by, f.L, false, blink, lookX, lookY, breathe, col, st, now);
  drawEye(k, (float)kRightX, (float)kEyeY + by, f.R, true, blink, lookX, lookY, breathe, col, st, now);
  if (st.mouth && blink < 500) drawMouth(k, f.mouth, lookX * 0.5f, now, col);
  drawExtras(k, (uint16_t)(f.extras & ~(FX_AURA | FX_BLUSH)), now, col);
}

// ── estado e animação ───────────────────────────────────────────────────────
void FaceRenderer::reset(ms_t now) {
  from_ = target_ = Expr::Happy; transMs_ = 0; transStart_ = now;
  nextBlink_ = now + 1500; nextLook_ = now + 2500; blinking_ = false;
  lookX_ = lookY_ = 0; lookTX_ = lookTY_ = 0; microActive_ = false; lookExt_ = false;
  bounceAt_ = 0; dance_ = false; lastUpdate_ = now;
}

void FaceRenderer::setExpression(Expr e, ms_t now, bool instant) {
  if ((int)e >= (int)Expr::Count) return;
  if (e == target_ && !instant) return;
  from_ = instant ? e : (microActive_ ? micro_ : target_);
  target_ = e;
  transStart_ = now;
  transMs_ = instant ? 0 : 240;
  microActive_ = false;
}

bool FaceRenderer::animating(ms_t now) const {
  (void)now;
  return true;  // respiração e brilho: o rosto está sempre vivo (o quadro só é enviado se mudar a área)
}

void FaceRenderer::update(ms_t now, Rng& rng, const PersonalityTuning& t, uint8_t animLevel) {
  int f = animLevel == 0 ? 160 : (animLevel == 2 ? 70 : 100);
  ms_t dt = now > lastUpdate_ ? now - lastUpdate_ : 0;
  if (dt > 200) dt = 200;
  lastUpdate_ = now;
  if (microActive_ && now >= microUntil_) microActive_ = false;
  if (!blinking_ && now >= nextBlink_) {
    blinking_ = true; blinkStart_ = now;
    secondBlink_ = (int)rng.range(0, 99) < t.doubleBlinkPct;
  }
  if (blinking_ && now >= blinkStart_ && now - blinkStart_ >= 170) {
    if (secondBlink_) { secondBlink_ = false; blinkStart_ = now + 70; }
    else { blinking_ = false; nextBlink_ = now + (ms_t)rng.range(t.blinkMinMs, t.blinkMaxMs) * f / 100; }
  }
  if (now >= nextLook_) {
    if (rng.range(0, 2) == 0) { lookTX_ = 0; lookTY_ = 0; }
    else { lookTX_ = rng.range(-t.lookRangeX, t.lookRangeX); lookTY_ = rng.range(-t.lookRangeY, t.lookRangeY); }
    nextLook_ = now + (ms_t)rng.range(t.lookMinMs, t.lookMaxMs) * f / 100;
    if ((target_ == Expr::Happy || target_ == Expr::Neutral) && !lookExt_ && (int)rng.range(0, 99) < t.mischiefPct) {
      static const Expr micro[] = {Expr::WinkL, Expr::WinkR, Expr::Surprised, Expr::Curious};
      micro_ = micro[rng.range(0, 3)]; microActive_ = true; microUntil_ = now + 800;
      from_ = target_; transStart_ = now; transMs_ = 160;
    }
  }
  // olhar: alvo externo (dedo/inclinação) tem prioridade; aproximação exponencial
  float tx = lookExt_ ? (float)extX_ * 46.f / 1000.f : (float)lookTX_;
  float ty = lookExt_ ? (float)extY_ * 30.f / 1000.f : (float)lookTY_;
  float k = fclamp((float)dt / (lookExt_ ? 70.f : 110.f), 0.f, 1.f);
  lookX_ += (tx - lookX_) * k;
  lookY_ += (ty - lookY_) * k;
}

static int lerpI(int a, int b, float p) { return a + iround((float)(b - a) * p); }
static EyeShape lerpEye(const EyeShape& a, const EyeShape& b, float p) {
  EyeShape r = b;
  r.w = (int16_t)lerpI(a.w, b.w, p); r.h = (int16_t)lerpI(a.h, b.h, p);
  r.lidTop = (int16_t)lerpI(a.lidTop, b.lidTop, p); r.lidBottom = (int16_t)lerpI(a.lidBottom, b.lidBottom, p);
  r.slope = (int16_t)lerpI(a.slope, b.slope, p); r.dx = (int16_t)lerpI(a.dx, b.dx, p); r.dy = (int16_t)lerpI(a.dy, b.dy, p);
  return r;
}

void FaceRenderer::draw(Canvas& c, ms_t now, const FaceStyle& st) {
  int blink = 0;
  if (blinking_ && now >= blinkStart_) {
    int bt = (int)(now - blinkStart_);
    blink = bt < 70 ? bt * 1000 / 70 : (bt < 95 ? 1000 : tbMax(0, 1000 - (bt - 95) * 1000 / 75));
  }
  Expr cur = microActive_ ? micro_ : target_;
  const FaceDef* tgt = petting_ && target_ != Expr::Love ? &RELAXED : &faceDef(cur);
  float breathe = fsin((float)(now % 4200) / 4200.f * kTau);
  FaceXf xf;
  xf.cx += (float)driftX_; xf.cy += (float)driftY_;
  if (dance_) {
    float ph = (float)dancePhase_ / 1000.f;
    xf.cy -= 16.f * fabsf_(fsin(ph * kPi));
    xf.cx += 10.f * fsin(ph * kPi);
  }
  if (bounceAt_ && now - bounceAt_ < 320) {
    float p = (float)(now - bounceAt_) / 320.f;
    xf.s = 1.f - 0.07f * fsin(p * kPi) * (1.f - p);
  }
  ms_t tt = now - transStart_;
  const FaceDef& src = faceDef(from_);
  if (tt < transMs_ && from_ != cur) {
    float p = easeOut((float)tt / (float)transMs_);
    bool sameKind = src.L.kind == tgt->L.kind && src.R.kind == tgt->R.kind;
    if (sameKind) {
      FaceDef mix = *tgt;
      mix.L = lerpEye(src.L, tgt->L, p); mix.R = lerpEye(src.R, tgt->R, p);
      mix.mouth = p < 0.5f ? src.mouth : tgt->mouth;
      mix.extras = p < 0.5f ? src.extras : tgt->extras;
      drawFace(c, mix, st, now, xf, lookX_, lookY_, blink, breathe);
    } else if (p < 0.5f) {
      drawFace(c, src, st, now, xf, lookX_, lookY_, tbMax(blink, (int)(p * 2000.f)), breathe);
    } else {
      drawFace(c, *tgt, st, now, xf, lookX_, lookY_, tbMax(blink, (int)((1.f - p) * 2000.f)), breathe);
    }
    return;
  }
  drawFace(c, *tgt, st, now, xf, lookX_, lookY_, blink, breathe);
}

}  // namespace tb
