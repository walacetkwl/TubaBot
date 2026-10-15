#include "scenes.h"
#include "i18n.h"

namespace tb {

// ── catálogo ────────────────────────────────────────────────────────────────
static const SceneInfo INFO[SCENE_COUNT] = {
  {"relogio", "Olhar o relógio", "confere as horas de vez em quando", 4200, 60},
  {"joinha", "Joinha", "aprova o seu dia", 3200, 30},
  {"peixe", "Peixinho passando", "um peixe passa e dá água na boca", 5600, 120},
  {"peixe_come", "Pegou o peixe!", "às vezes o peixe não escapa", 6000, 300},
  {"cafe", "Cafezinho", "só pela manhã", 6500, 150},
  {"meditacao", "Meditação", "um momento de calma", 7600, 250},
  {"escada", "Escada rolante", "finge que foi embora...", 9000, 400},
  {"ouvidos", "Que barulho!", "quando alguém grita perto", 3800, 180},
  {"bocejo", "Bocejo", "quando a noite chega", 5000, 80},
};

const SceneInfo& sceneInfo(SceneId s) { return INFO[(int)s < SCENE_COUNT ? (int)s : 0]; }
SceneId sceneFromKey(const char* k) {
  for (int i = 0; i < SCENE_COUNT; i++) if (tbStreq(INFO[i].key, k)) return (SceneId)i;
  return SceneId::Count;
}

// ── deixas de som ───────────────────────────────────────────────────────────
struct Cue { uint16_t ms; Pattern p; };
static const Cue CUE_RELOGIO[] = {{200, Pattern::Boop}, {1600, Pattern::Startled}, {2700, Pattern::Giggle}};
static const Cue CUE_JOINHA[] = {{150, Pattern::Boop}, {600, Pattern::Unlock}};
static const Cue CUE_PEIXE[] = {{300, Pattern::Babble}, {2300, Pattern::Slurp}, {3500, Pattern::Slurp}};
static const Cue CUE_COME[] = {{1500, Pattern::Babble}, {1950, Pattern::Startled}, {2300, Pattern::Nom}, {3000, Pattern::Nom}, {3700, Pattern::Love}};
static const Cue CUE_CAFE[] = {{200, Pattern::Boop}, {1700, Pattern::Sip}, {2600, Pattern::Sip}, {3500, Pattern::Purr}};
static const Cue CUE_MEDIT[] = {{900, Pattern::Hum}, {3200, Pattern::Hum}, {5500, Pattern::Breath}};
static const Cue CUE_ESCADA[] = {{500, Pattern::Babble}, {4200, Pattern::Boop}, {4900, Pattern::Boop}, {6000, Pattern::Giggle}, {7300, Pattern::Giggle}};
static const Cue CUE_OUVIDOS[] = {{0, Pattern::Startled}, {2900, Pattern::Yawn}};
static const Cue CUE_BOCEJO[] = {{600, Pattern::Yawn}, {3400, Pattern::Purr}};
struct CueList { const Cue* c; uint8_t n; };
#define CL(a) {a, (uint8_t)(sizeof a / sizeof a[0])}
static const CueList CUES[SCENE_COUNT] = {CL(CUE_RELOGIO), CL(CUE_JOINHA), CL(CUE_PEIXE), CL(CUE_COME), CL(CUE_CAFE),
                                          CL(CUE_MEDIT), CL(CUE_ESCADA), CL(CUE_OUVIDOS), CL(CUE_BOCEJO)};
#undef CL

Pattern ScenePlayer::takeSound(ms_t now) {
  if (!on_) return Pattern::None;
  const CueList& L = CUES[(int)id_];
  ms_t t = now - t0_;
  if (cue_ < L.n && t >= L.c[cue_].ms) {
    Pattern p = L.c[cue_].p;
    cue_++;
    while (cue_ < L.n && t >= (ms_t)L.c[cue_].ms + 400) cue_++;  // quadros atrasados: não empilha sons antigos
    return p;
  }
  return Pattern::None;
}

// ── utilidades de animação ──────────────────────────────────────────────────
static float seg(float t, float a, float b) { return fclamp((t - a) / (b - a), 0.f, 1.f); }
// envelope 0→1→0: sobe em [a, a+in], fica, desce em [b-out, b]
static float env(float t, float a, float b, float in, float out) {
  if (t <= a || t >= b) return 0.f;
  return fminf_(seg(t, a, a + in), 1.f - seg(t, b - out, b));
}
static int blinkAt(float t, float t0) {
  float d = t - t0;
  if (d < 0.f || d > 170.f) return 0;
  return d < 70.f ? (int)(d * 1000.f / 70.f) : (d < 95.f ? 1000 : 1000 - (int)((d - 95.f) * 1000.f / 75.f));
}

struct Pal { Color hand, shade, hi; };
static Pal palette(const FaceStyle& st) {
  Pal p;
  p.hand = st.light ? mixColor(st.eye, 0x000000, 24) : mixColor(st.eye, 0xFFFFFF, 46);
  p.shade = mixColor(st.eye, 0x000000, st.light ? 120 : 150);
  p.hi = st.light ? mixColor(st.eye, 0xFFFFFF, 150) : (Color)0xFFFFFF;
  return p;
}

// Rosto da cena: mesmo desenho do rosto normal, com deslocamento/escala próprios.
struct FacePose {
  float dx = 0, dy = 0, s = FACE_SCALE, lookX = 0, lookY = 0, breathe = 0;
  int blink = 0;
};
static void face(Canvas& c, const FaceDef& f, const FaceStyle& st, ms_t now, const FacePose& p) {
  FaceXf xf; xf.cx = (float)SCR_CX + p.dx; xf.cy = FACE_CY + p.dy; xf.s = p.s;
  FaceRenderer::drawFace(c, f, st, now, xf, p.lookX, p.lookY, p.blink, p.breathe);
}
// posição da boca na tela (para língua, xícara, mão no bocejo)
static float mouthX(const FacePose& p) { return (float)SCR_CX + p.dx + p.lookX * 0.5f * p.s; }
static float mouthY(const FacePose& p) { return FACE_CY + p.dy + ((float)FaceRenderer::kMouthY - FACE_DESIGN_C) * p.s; }

static FaceDef def(Expr e) { return faceDef(e); }

// ── mãos estilizadas ("luvinhas" na cor das expressões) ────────────────────
static void fist(Canvas& c, float x, float y, float k, const Pal& P, int a) {
  c.fillRoundRect(x - 17.f * k + 2.f, y - 13.f * k + 3.f, 34.f * k, 28.f * k, 11.f * k, P.shade, a * 3 / 4);
  c.fillRoundRect(x - 17.f * k, y - 13.f * k, 34.f * k, 28.f * k, 11.f * k, P.hand, a);
  for (int i = 0; i < 3; i++) {
    float gx = x - 8.f * k + 8.f * k * (float)i + 4.f * k;
    c.line(gx, y - 12.f * k, gx, y - 5.f * k, 2.f * k, P.shade, a * 2 / 3);
  }
}
static void thumbsUp(Canvas& c, float x, float y, float k, float rot, const Pal& P, int a) {
  // punho de lado (dedos dobrados empilhados) + polegão para cima
  float sn = fsin(rot), cs = fcos(rot);
  auto R = [&](float px, float py) { return Pt{x + px * cs - py * sn, y + px * sn + py * cs}; };
  Pt c0 = R(-2.f * k, 6.f * k);
  c.fillRoundRect(c0.x - 19.f * k + 2.f, c0.y - 16.f * k + 3.f, 38.f * k, 32.f * k, 12.f * k, P.shade, a * 3 / 4);
  c.fillRoundRect(c0.x - 19.f * k, c0.y - 16.f * k, 38.f * k, 32.f * k, 12.f * k, P.hand, a);
  for (int i = 0; i < 3; i++) {                     // dobras dos dedos
    Pt g0 = R(2.f * k, -2.f * k + 8.f * k * (float)i), g1 = R(16.f * k, -2.f * k + 8.f * k * (float)i);
    c.line(g0.x, g0.y, g1.x, g1.y, 1.8f * k, P.shade, a * 2 / 3);
  }
  Pt t0 = R(-9.f * k, -6.f * k), t1 = R(-7.f * k, -24.f * k);
  c.line(t0.x + 2.f, t0.y + 3.f, t1.x + 2.f, t1.y + 3.f, 15.f * k, P.shade, a * 3 / 4);
  c.line(t0.x, t0.y, t1.x, t1.y, 15.f * k, P.hand, a);
  c.fillCircle(t1.x - 2.f * k, t1.y + 1.f * k, 2.4f * k, P.hi, a / 2);
}
static void openHand(Canvas& c, float x, float y, float k, float rot, const Pal& P, int a) {
  // palma + 4 dedos em leque + polegar
  for (int pass = 0; pass < 2; pass++) {
    Color col = pass ? P.hand : P.shade;
    float o = pass ? 0.f : 2.f;
    int aa = pass ? a : a * 3 / 4;
    for (int i = 0; i < 4; i++) {
      float ang = rot - 0.42f + 0.28f * (float)i;
      float len = (i == 0 || i == 3 ? 17.f : 21.f) * k;
      float bx = x + 8.f * k * fsin(ang), by = y - 8.f * k * fcos(ang);
      c.line(bx + o, by + o, bx + len * fsin(ang) + o, by - len * fcos(ang) + o, 8.f * k, col, aa);
    }
    float ta = rot - 1.25f;
    c.line(x + o, y + o, x + 20.f * k * fsin(ta) + o, y - 20.f * k * fcos(ta) + o, 8.5f * k, col, aa);
    c.fillCircle(x + o, y + o, 14.f * k, col, aa);
  }
}
static void mudra(Canvas& c, float x, float y, float k, bool right, const Pal& P, int a) {
  // mão apoiada com a palma para cima; polegar e indicador formam um círculo (mudra)
  float s = right ? -1.f : 1.f;
  c.fillEllipse(x + 2.f, y + 3.f, 18.f * k, 10.f * k, P.shade, a * 3 / 4);
  c.fillEllipse(x, y, 18.f * k, 10.f * k, P.hand, a);
  c.ring(x + 9.f * s * k, y - 11.f * k, 6.5f * k, 3.6f * k, P.hand, a);
  c.fillCircle(x + 9.f * s * k, y - 11.f * k, 1.8f * k, P.hi, a / 2);
}
static void palmSide(Canvas& c, float x, float y, float k, const Pal& P, int a) {
  c.fillRoundRect(x - 12.f * k + 2.f, y - 24.f * k + 3.f, 24.f * k, 46.f * k, 12.f * k, P.shade, a * 3 / 4);
  c.fillRoundRect(x - 12.f * k, y - 24.f * k, 24.f * k, 46.f * k, 12.f * k, P.hand, a);
  for (int i = 0; i < 3; i++) c.line(x - 7.f * k + 7.f * k * (float)i, y - 22.f * k, x - 7.f * k + 7.f * k * (float)i, y - 12.f * k, 1.8f * k, P.shade, a / 2);
}

// ── objetos ─────────────────────────────────────────────────────────────────
static void fish(Canvas& c, float x, float y, float k, bool right, float wig, int a) {
  float s = right ? 1.f : -1.f;
  Pt tail[3] = {{x - 12.f * s * k, y}, {x - 24.f * s * k, y - 9.f * k + wig}, {x - 24.f * s * k, y + 9.f * k + wig}};
  c.fillPolygon(tail, 3, 0xFF8A3D, a);
  c.fillEllipse(x, y, 16.f * k, 10.f * k, 0xFF9F43, a);
  c.arc(x + 1.f * s * k, y, 8.f * k, 1.6f * k, right ? kPi * 0.25f : -kPi * 0.75f, 1.6f, 0xFFD27A, a);
  c.fillCircle(x + 8.f * s * k, y - 2.f * k, 2.6f * k, 0xFFFFFF, a);
  c.fillCircle(x + 8.6f * s * k, y - 2.f * k, 1.3f * k, 0x1A1A1A, a);
}
static void fishBone(Canvas& c, float x, float y, float k, Color col, int a) {
  c.line(x - 14.f * k, y, x + 10.f * k, y, 2.f * k, col, a);
  for (int i = 0; i < 3; i++) {
    float rx = x - 8.f * k + 6.f * k * (float)i;
    c.line(rx, y - 5.f * k, rx, y + 5.f * k, 1.6f * k, col, a);
  }
  Pt tail[3] = {{x - 14.f * k, y}, {x - 20.f * k, y - 5.f * k}, {x - 20.f * k, y + 5.f * k}};
  c.fillPolygon(tail, 3, col, a / 2);
  c.ring(x + 14.f * k, y, 4.5f * k, 1.6f * k, col, a);
}
static void watch(Canvas& c, float x, float y, float k, float spin, int a) {
  c.fillRoundRect(x - 8.f * k, y - 26.f * k, 16.f * k, 52.f * k, 5.f * k, 0x4A3B33, a);    // pulseira
  c.fillCircle(x, y, 15.f * k, 0xC9971A, a);
  c.fillCircle(x, y, 12.5f * k, 0xFFF8E7, a);
  for (int i = 0; i < 12; i += 3) {
    float an = (float)i * kTau / 12.f;
    c.line(x + 9.f * k * fsin(an), y - 9.f * k * fcos(an), x + 11.f * k * fsin(an), y - 11.f * k * fcos(an), 1.4f * k, 0x333333, a);
  }
  c.line(x, y, x + 7.f * k * fsin(spin * 0.08f), y - 7.f * k * fcos(spin * 0.08f), 2.2f * k, 0x333333, a);
  c.line(x, y, x + 9.5f * k * fsin(spin), y - 9.5f * k * fcos(spin), 1.5f * k, 0xE04040, a);
  c.fillCircle(x, y, 1.8f * k, 0x333333, a);
}
static void cup(Canvas& c, float x, float y, float k, float tilt, ms_t now, bool steam, int a) {
  // tilt: inclina a xícara (bebendo); desenhada em volta do centro
  float sn = fsin(tilt), cs = fcos(tilt);
  auto R = [&](float px, float py) { return Pt{x + px * cs - py * sn, y + px * sn + py * cs}; };
  Pt body[8] = {R(-15.f * k, -12.f * k), R(15.f * k, -12.f * k), R(14.f * k, 2.f * k), R(11.f * k, 10.f * k),
                R(6.f * k, 14.f * k), R(-6.f * k, 14.f * k), R(-11.f * k, 10.f * k), R(-14.f * k, 2.f * k)};
  Pt hc = R(17.f * k, -1.f * k);
  c.ring(hc.x, hc.y, 6.5f * k, 3.2f * k, 0xEDE3D6, a);
  c.fillPolygon(body, 8, 0xF4ECE2, a);
  Pt top = R(0.f, -12.f * k);
  c.fillEllipse(top.x, top.y, 14.f * k * fmaxf_(cs, 0.3f), 3.5f * k, 0x6B3E26, a);
  Pt mark = R(0.f, 2.f * k);
  c.heart(mark.x, mark.y, 7.f * k, 0xFF7AB6, a * 2 / 3);
  if (!steam) return;
  for (int i = 0; i < 3; i++) {                                         // fumacinha
    float ph = (float)((now + (ms_t)i * 420) % 1300) / 1300.f;
    float sx = top.x - 7.f * k + 7.f * k * (float)i, sy = top.y - 4.f * k;
    float px = sx, py = sy;
    for (int j = 1; j <= 6; j++) {
      float q = (float)j / 6.f;
      float nx = sx + 4.f * k * fsin(q * 5.f + ph * kTau + (float)i), ny = sy - 26.f * k * q - 8.f * k * ph;
      c.line(px, py, nx, ny, 2.4f * k, 0xFFFFFF, (int)((float)a * 0.55f * (1.f - q) * (1.f - ph * 0.5f)));
      px = nx; py = ny;
    }
  }
}
static void tongue(Canvas& c, float x, float y, float k, float side, int a) {
  float tx = x + side * 12.f * k;
  c.fillEllipse(tx, y + 3.f * k, 8.f * k, 6.5f * k, 0xFF6F91, a);
  c.line(tx, y + 0.5f * k, tx, y + 5.f * k, 1.4f * k, 0xD94B6E, a);
}
static void label(Canvas& c, float x, float y, const char* s, const FaceStyle& st, int a) {
  if (a <= 0) return;
  c.text(x, y, tr(s), FONT_M, st.light ? mixColor(st.eye, 0x000000, 80) : st.fx, a, Align::Center);
}

// ── cenas ───────────────────────────────────────────────────────────────────
static void sceneRelogio(Canvas& c, float t, ms_t now, const FaceStyle& st, const Pal& P) {
  float up = easeBack(seg(t, 0.f, 520.f)) * (1.f - easeInOut(seg(t, 3500.f, 4100.f)));
  float wx = 278.f, wy = 286.f - 92.f * up;
  FacePose fp;
  float look = seg(t, 300.f, 600.f) * (1.f - seg(t, 3300.f, 3700.f));
  fp.lookX = 26.f * look; fp.lookY = 28.f * look;
  fp.blink = blinkAt(t, 1150.f);
  FaceDef f = def(Expr::Neutral);
  if (t > 1600.f && t < 2500.f) { f = def(Expr::Surprised); fp.dy = -3.f * fsin(seg(t, 1600.f, 1800.f) * kPi); }
  else if (t >= 2500.f && t < 3500.f) f = def(Expr::Happy);
  else if (t >= 3500.f) f = def(Expr::Happy);
  if (t >= 2500.f && t < 3000.f) f = def(Expr::WinkR);
  face(c, f, st, now, fp);
  if (up > 0.01f) {
    float spin = t * (t > 1600.f && t < 2500.f ? 0.03f : 0.006f);
    fist(c, wx, wy + 22.f, 1.1f, P, 255);
    watch(c, wx, wy - 4.f, 1.15f, spin, 255);
  }
  if (t > 1650.f && t < 2500.f) {
    int a = (int)(255.f * env(t, 1650.f, 2500.f, 120.f, 200.f));
    c.fillRoundRect(282.f, 30.f, 7.f, 22.f, 3.5f, st.fx, a);
    c.fillCircle(285.5f, 60.f, 4.f, st.fx, a);
  }
}

static void sceneJoinha(Canvas& c, float t, ms_t now, const FaceStyle& st, const Pal& P) {
  FacePose fp;
  float pop = easeBack(seg(t, 0.f, 480.f)) * (1.f - easeInOut(seg(t, 2550.f, 3150.f)));
  fp.dx = -6.f * pop; fp.lookX = -10.f * pop; fp.lookY = 6.f * pop;
  FaceDef f = t < 250.f ? def(Expr::Happy) : def(Expr::WinkR);
  f.mouth = Mouth::BigSmile;
  if (t > 2500.f) f = def(Expr::Happy);
  face(c, f, st, now, fp);
  if (pop > 0.01f) {
    float wob = 0.18f * fsin(t / 90.f) * (1.f - seg(t, 300.f, 900.f));
    float hx = 70.f, hy = 280.f - 86.f * pop;
    thumbsUp(c, hx, hy, 1.45f, wob - 0.08f, P, 255);
    float sp = env(t, 550.f, 1500.f, 150.f, 400.f);
    if (sp > 0.f) {
      float r = 9.f * sp;
      c.star(hx - 20.f, hy - 48.f, r, r * 0.42f, 0xFFD54A, (int)(255.f * sp), t / 300.f);
      c.star(hx + 12.f, hy - 56.f, r * 0.6f, r * 0.25f, st.fx, (int)(220.f * sp), -t / 260.f);
    }
  }
}

static void scenePeixe(Canvas& c, float t, ms_t now, const FaceStyle& st, const Pal& P) {
  (void)P;
  float p = seg(t, 200.f, 3600.f);
  float fx = -40.f + 400.f * p, fy = 30.f + 10.f * fsin(t / 260.f);
  FacePose fp;
  float follow = env(t, 200.f, 4200.f, 400.f, 500.f);
  fp.lookX = fclamp((fx - 160.f) / 160.f, -1.f, 1.f) * 30.f * follow;
  fp.lookY = -22.f * follow;
  fp.blink = blinkAt(t, 4500.f);
  FaceDef f = def(Expr::Happy);
  f.L.lidBottom = f.R.lidBottom = 20;
  bool lick = t > 2200.f && t < 4700.f;
  f.mouth = lick ? Mouth::Smile : Mouth::Small;
  if (t > 1400.f && t < 2200.f) f = def(Expr::Curious);
  face(c, f, st, now, fp);
  if (p > 0.f && p < 1.f) fish(c, fx, fy, 1.5f, true, 4.f * fsin(t / 70.f), 255);
  if (lick) {
    float side = fsin((t - 2200.f) / 2500.f * kTau * 2.f);
    tongue(c, mouthX(fp), mouthY(fp) - 4.f, 1.1f, side, (int)(255.f * env(t, 2200.f, 4700.f, 150.f, 250.f)));
  }
  label(c, 160.f, 228.f, "hmmm...", st, (int)(200.f * env(t, 2400.f, 4400.f, 300.f, 400.f)));
}

static void scenePeixeCome(Canvas& c, float t, ms_t now, const FaceStyle& st, const Pal& P) {
  (void)P;
  FacePose fp;
  // peixe entra pela direita na altura da boca, devagar; o Tuba dá o bote
  float fx = 350.f - 140.f * easeOut(seg(t, 0.f, 1800.f)), fy = 178.f + 6.f * fsin(t / 200.f);
  float lunge = seg(t, 1900.f, 2150.f);
  float back = seg(t, 2150.f, 2700.f);
  float lk = lunge * (1.f - back);
  fp.s = FACE_SCALE * (1.f + 0.12f * lk);
  fp.dx = 24.f * lk; fp.dy = 8.f * lk;
  fp.lookX = t < 2200.f ? 26.f : 0.f; fp.lookY = t < 2200.f ? 22.f : 0.f;
  FaceDef f = def(Expr::Curious);
  if (t > 1300.f && t < 1900.f) f = def(Expr::Focus);
  if (t >= 1900.f && t < 2200.f) { f = def(Expr::Surprised); f.mouth = Mouth::Open; }
  if (t >= 2200.f && t < 3500.f) {                                         // mastigando
    f = def(Expr::Pet);
    f.mouth = ((int)((t - 2200.f) / 170.f) & 1) ? Mouth::Flat : Mouth::Cat;
  }
  if (t >= 3500.f) f = def(Expr::Satisfied);
  face(c, f, st, now, fp);
  if (t < 2150.f) {
    float k = 1.4f;
    float gx = fx, gy = fy;
    if (t > 1950.f) { float q = seg(t, 1950.f, 2150.f); gx = flerp(fx, mouthX(fp), q); gy = flerp(fy, mouthY(fp), q); k = 1.4f * (1.f - 0.8f * q); }
    fish(c, gx, gy, k, false, 3.f * fsin(t / 60.f), 255);
  }
  if (t > 3700.f && t < 5600.f) {
    float q = seg(t, 3700.f, 5600.f);
    fishBone(c, 222.f + 20.f * q, 200.f + 50.f * q * q, 1.4f, st.fx, (int)(230.f * (1.f - q)));
  }
  label(c, 250.f, 224.f, "nham!", st, (int)(255.f * env(t, 2300.f, 3400.f, 120.f, 300.f)));
}

static void sceneCafe(Canvas& c, float t, ms_t now, const FaceStyle& st, const Pal& P) {
  float in = easeBack(seg(t, 0.f, 650.f)) * (1.f - easeInOut(seg(t, 5800.f, 6450.f)));
  float drink = env(t, 1400.f, 3400.f, 400.f, 400.f);
  FacePose fp;
  float look = env(t, 700.f, 1500.f, 200.f, 150.f);
  fp.lookX = 20.f * look; fp.lookY = 24.f * look;
  FaceDef f = def(Expr::Happy);
  if (drink > 0.3f) { f = def(Expr::Rest); f.mouth = Mouth::None; f.extras = FX_BLUSH; }
  else if (t > 3400.f && t < 4500.f) { f = def(Expr::Rest); f.mouth = Mouth::Cat; f.extras = FX_BLUSH; }
  else if (t >= 4500.f && t < 5600.f) f = def(Expr::Celebrate);
  face(c, f, st, now, fp);
  float cx = flerp(222.f, mouthX(fp) + 4.f, drink), cy = flerp(266.f - 62.f * in, mouthY(fp) + 6.f, drink);
  if (in > 0.01f) {
    cup(c, cx, cy, 1.4f, -0.55f * drink, now, drink < 0.5f, 255);
    float hx = cx + 32.f, hy = cy + 8.f;
    fist(c, hx, hy, 1.0f, P, 255);
  }
  label(c, 92.f, 226.f, "ahh...", st, (int)(255.f * env(t, 3400.f, 4600.f, 200.f, 300.f)));
}

static void sceneMeditacao(Canvas& c, float t, ms_t now, const FaceStyle& st, const Pal& P) {
  FacePose fp;
  float zen = env(t, 0.f, 7400.f, 700.f, 700.f);
  fp.dy = -10.f * zen * (0.5f + 0.5f * fsin(t / 900.f));
  fp.breathe = fsin(t / 1600.f * kPi);
  FaceDef f = t < 500.f || t > 6900.f ? def(Expr::Happy) : def(Expr::Zen);
  face(c, f, st, now, fp);
  float hands = easeOut(seg(t, 600.f, 1400.f)) * (1.f - easeInOut(seg(t, 6300.f, 7000.f)));
  if (hands > 0.01f) {
    float hy = 266.f - 58.f * hands + fp.dy * 0.6f;
    mudra(c, 46.f, hy, 1.2f, false, P, 255);
    mudra(c, 274.f, hy, 1.2f, true, P, 255);
  }
  int a = (int)(150.f * env(t, 1500.f, 6200.f, 600.f, 600.f) * (0.6f + 0.4f * fsin(t / 700.f)));
  label(c, 160.f, 30.f, "ommm", st, a);
}

static void sceneEscada(Canvas& c, float t, ms_t now, const FaceStyle& st, const Pal& P) {
  // corrimão da escada rolante na diagonal (aparece discreto)
  float rail = env(t, 300.f, 8800.f, 500.f, 600.f);
  if (rail > 0.f) {
    Color rc = st.light ? (Color)0xB8C2D0 : (Color)0x3A4560;
    c.line(40.f, 178.f, 360.f, 386.f, 7.f, rc, (int)(200.f * rail));
    for (int i = 0; i < 9; i++) {                                         // degraus passando (descendo)
      float q = ((float)i + (float)((ms_t)t % 1800) / 1800.f) / 9.f;
      float sx = 50.f + 300.f * q, sy = 196.f + 195.f * q;
      c.line(sx - 16.f, sy, sx + 12.f, sy, 2.f, rc, (int)(110.f * rail));
    }
  }
  FacePose fp;
  FaceDef f = def(Expr::Happy);
  const float DX = 270.f, DY = 176.f;   // mesma inclinação do corrimão
  if (t < 700.f) {
    fp.lookX = 18.f * seg(t, 200.f, 600.f); fp.lookY = 20.f * seg(t, 200.f, 600.f);
  } else if (t < 3700.f) {                                                 // descendo (velocidade constante + passinhos)
    float q = seg(t, 700.f, 3600.f);
    fp.dx = DX * q; fp.dy = DY * q - 3.f * fabsf_(fsin(t / 160.f));
    fp.s = FACE_SCALE * (1.f - 0.18f * q);
    fp.lookX = 18.f; fp.lookY = 20.f;
    f = def(Expr::Neutral);
  } else if (t < 6000.f) {                                                 // espiando no canto e dando tchau
    float peek = env(t, 3900.f, 6000.f, 450.f, 350.f);
    fp.dx = flerp(DX, 138.f, peek); fp.dy = flerp(DY, 112.f, peek);
    fp.s = FACE_SCALE * 0.82f;
    fp.lookX = -20.f; fp.lookY = -18.f;
    f = def(Expr::Happy);
  } else {                                                                 // volta rindo, subindo a escada
    float q = easeOut(seg(t, 6000.f, 8200.f));
    fp.dx = DX * (1.f - q); fp.dy = DY * (1.f - q) - 3.f * fabsf_(fsin(t / 140.f)) * (1.f - q);
    fp.s = FACE_SCALE * (0.82f + 0.18f * q);
    f = t < 8300.f ? def(Expr::Hit) : def(Expr::Happy);
  }
  c.setClip(IRect(0, 0, SCR_W, SCR_H));
  face(c, f, st, now, fp);
  float wave = env(t, 4100.f, 5900.f, 300.f, 300.f);
  if (wave > 0.01f) {
    float rot = 0.45f * fsin((t - 4100.f) / 150.f);
    openHand(c, 290.f, 252.f - 46.f * easeBack(fminf_(wave * 1.2f, 1.f)), 1.1f, rot, P, 255);
    label(c, 238.f, 120.f, "tchau!", st, (int)(255.f * env(t, 4300.f, 5700.f, 200.f, 300.f)));
  }
  label(c, 120.f, 60.f, "hehe", st, (int)(255.f * env(t, 6200.f, 8400.f, 200.f, 400.f)));
}

static void sceneOuvidos(Canvas& c, float t, ms_t now, const FaceStyle& st, const Pal& P) {
  FacePose fp;
  float cover = easeOut(seg(t, 200.f, 520.f)) * (1.f - easeInOut(seg(t, 2800.f, 3300.f)));
  FaceDef f = def(Expr::Startled);
  if (t > 450.f && t < 2900.f) {
    f = def(Expr::Squint); f.mouth = Mouth::Wobble; f.extras = FX_SWEAT;
    fp.dx = 2.5f * fsin(t / 22.f);
  } else if (t >= 2900.f) { f = def(Expr::Rest); f.mouth = Mouth::Small; }
  face(c, f, st, now, fp);
  if (cover > 0.01f) {
    float lx = flerp(-30.f, 30.f, cover), rx = flerp(350.f, 290.f, cover);
    palmSide(c, lx + fp.dx, 104.f, 1.15f, P, 255);
    palmSide(c, rx + fp.dx, 104.f, 1.15f, P, 255);
  }
  if (t > 500.f && t < 2700.f) {                                          // ondas de som
    int a = (int)(180.f * env(t, 500.f, 2700.f, 200.f, 300.f));
    for (int i = 0; i < 2; i++) {
      float r = 10.f + 8.f * (float)i + 4.f * fsin(t / 90.f);
      c.arc(160.f, 34.f, r, 2.5f, -0.7f, 1.4f, st.fx, a);
    }
  }
  label(c, 160.f, 228.f, "ufa!", st, (int)(255.f * env(t, 3000.f, 3800.f, 150.f, 250.f)));
}

static void sceneBocejo(Canvas& c, float t, ms_t now, const FaceStyle& st, const Pal& P) {
  FacePose fp;
  FaceDef f = def(Expr::Tired);
  float yawn = env(t, 500.f, 2400.f, 600.f, 500.f);
  if (yawn > 0.f) { f.L.lidTop = f.R.lidTop = (int16_t)(56 + (int)(30.f * yawn)); f.mouth = Mouth::Open; }
  if (t > 2400.f) f = def(Expr::Sleepy);
  fp.dy = -4.f * yawn;
  face(c, f, st, now, fp);
  float hand = env(t, 800.f, 2500.f, 350.f, 400.f);
  if (hand > 0.01f) openHand(c, mouthX(fp) + 4.f, flerp(270.f, mouthY(fp) + 10.f, easeOut(hand)), 1.0f, -0.15f, P, 255);
}

void ScenePlayer::draw(Canvas& c, ms_t now, const FaceStyle& st) const {
  if (!on_) return;
  float t = (float)(now - t0_);
  Pal P = palette(st);
  switch (id_) {
    case SceneId::Relogio: sceneRelogio(c, t, now, st, P); break;
    case SceneId::Joinha: sceneJoinha(c, t, now, st, P); break;
    case SceneId::PeixeOlha: scenePeixe(c, t, now, st, P); break;
    case SceneId::PeixeCome: scenePeixeCome(c, t, now, st, P); break;
    case SceneId::Cafe: sceneCafe(c, t, now, st, P); break;
    case SceneId::Meditacao: sceneMeditacao(c, t, now, st, P); break;
    case SceneId::Escada: sceneEscada(c, t, now, st, P); break;
    case SceneId::Ouvidos: sceneOuvidos(c, t, now, st, P); break;
    case SceneId::Bocejo: sceneBocejo(c, t, now, st, P); break;
    default: break;
  }
}

// ── medalha ─────────────────────────────────────────────────────────────────
void drawLegendMedal(Canvas& c, float k, const char* title, const char* name, Color bg, Color text, Color muted) {
  // k: 0..1 (entrada/saída). Cartão que desce do topo com medalha à esquerda.
  float y = -70.f + 82.f * easeBack(k);
  int a = (int)(255.f * fclamp(k * 1.5f, 0.f, 1.f));
  c.fillRoundRect(24.f + 2.f, y + 3.f, 272.f, 60.f, 18.f, 0x000000, a / 3);
  c.fillRoundRect(24.f, y, 272.f, 60.f, 18.f, bg, a);
  c.strokeRoundRect(24.f, y, 272.f, 60.f, 18.f, 2.f, 0xFFD54A, a);
  // medalha
  float mx = 56.f, my = y + 30.f;
  Pt l[4] = {{mx - 13.f, my - 26.f}, {mx - 3.f, my - 26.f}, {mx + 2.f, my - 8.f}, {mx - 8.f, my - 6.f}};
  Pt r[4] = {{mx + 3.f, my - 26.f}, {mx + 13.f, my - 26.f}, {mx + 8.f, my - 6.f}, {mx - 2.f, my - 8.f}};
  c.fillPolygon(l, 4, 0xFF5E57, a);
  c.fillPolygon(r, 4, 0x4C8DFF, a);
  c.fillCircle(mx, my + 4.f, 14.f, 0xFFD54A, a);
  c.ring(mx, my + 4.f, 10.5f, 2.f, 0xC9971A, a);
  c.star(mx, my + 4.f, 7.f, 3.f, 0xFFF4C2, a);
  c.text(80.f, y + 25.f, title, FONT_S, 0xFFD54A, a);
  const Font* fonts[2] = {&FONT_M, &FONT_S};
  c.text(80.f, y + 47.f, name, Canvas::fitFont(name, 206, fonts, 2), text, a);
  (void)muted;
}

}  // namespace tb
