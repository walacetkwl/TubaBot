// Desenho de todas as telas do TubaBot (320×240, paisagem, tema claro/escuro).
#include "app.h"
#include "app_ids.h"

namespace tb {

using namespace ui;

static const char* const WEEKDAYS[] = {"dom", "seg", "ter", "qua", "qui", "sex", "sáb"};
static const char* const WEEKDAYS_FULL[] = {"domingo", "segunda", "terça", "quarta", "quinta", "sexta", "sábado"};
static const char* const MONTHS[] = {"jan", "fev", "mar", "abr", "mai", "jun", "jul", "ago", "set", "out", "nov", "dez"};
static const char* const UNLOCK_CRITERIA[UNL_COUNT] = {
  "1º carinho", "1º Pomodoro completo", "10 corais no Pula, Tuba!", "5 peixes na Hora do Bote", "5 reflexos válidos",
  "98% no Tempo Certo", "Dançar 30 s com música", "3 sessões de respiração", "20 peixes no Mergulho", "Todas as outras"};
static const float CX = (float)SCR_CX, CY = (float)SCR_CY;
static const float W = (float)SCR_W, H = (float)SCR_H;

static bool luminanceDark(Color c) {
  int r = (int)((c >> 16) & 255), g = (int)((c >> 8) & 255), b = (int)(c & 255);
  return (r * 299 + g * 587 + b * 114) / 1000 < 150;
}

// ── botões ──────────────────────────────────────────────────────────────────
void App::hit(int id, float cx, float cy, float r) {
  if (hitN_ >= kHits) return;
  Hit& h = hits_[hitN_++];
  h.id = (int16_t)id; h.circle = true;
  h.x0 = (int16_t)(cx - r); h.x1 = (int16_t)(cx + r); h.y0 = (int16_t)(cy - r); h.y1 = (int16_t)(cy + r);
}
void App::hitRect(int id, float x, float y, float w, float h) {
  if (hitN_ >= kHits) return;
  Hit& t = hits_[hitN_++];
  t.id = (int16_t)id; t.circle = false;
  t.x0 = (int16_t)x; t.y0 = (int16_t)y; t.x1 = (int16_t)(x + w); t.y1 = (int16_t)(y + h);
}
void App::button(int id, float cx, float cy, float r, Color bg, Icon ic, Color fg) {
  roundButton(canvas_, cx, cy, r, bg, ic, fg, pressedHit_ == id);
  hit(id, cx, cy, r + 6.f);
}
void App::drawBackChevron(const char* title, Color col) {
  canvas_.fillCircle(22.f, 20.f, 15.f, pressedHit_ == B_BACK ? SURF2 : SURF);
  drawIcon(canvas_, Icon::Back, 21.f, 20.f, 15.f, MUTED);
  hit(B_BACK, 22.f, 20.f, 24.f);
  if (title) header(canvas_, title, col ? col : TEXT);
}

static void fmtTime(StrBuf& s, int h, int m, bool h24) {
  if (!h24) { h = h % 12; if (h == 0) h = 12; }
  s.numPad(h, h24 ? 2 : 1).put(':').numPad(m, 2);
}

// ── QR Code ─────────────────────────────────────────────────────────────────
void App::drawQr(const char* text, float cx, float cy, float size) {
  if (!tbStreq(text, qrText_)) {
    tbStrlcpy(qrText_, text, sizeof qrText_);
    if (!qrEncodeText(text, qr_)) qr_.size = 0;
  }
  if (!qr_.size) return;
  int quiet = 2, n = qr_.size + quiet * 2;
  int m = tbMax(2, (int)size / n);
  int total = m * n;
  int x0 = (int)cx - total / 2, y0 = (int)cy - total / 2;
  canvas_.fillRoundRect((float)x0 - 4.f, (float)y0 - 4.f, (float)total + 8.f, (float)total + 8.f, 10.f, 0xFFFFFF);
  for (int y = 0; y < qr_.size; y++)
    for (int x = 0; x < qr_.size; x++)
      if (qr_.module(x, y)) canvas_.fillRect(x0 + (x + quiet) * m, y0 + (y + quiet) * m, m, m, 0x000000);
}

// ── rosto (página inicial, tela cheia) ──────────────────────────────────────
// Informações opcionais sobre o rosto: hora (centro), clima (esquerda),
// bateria (direita, só no Pro). Todas desligadas de fábrica.
void App::drawFaceOverlays(ms_t now) {
  char b[16];
  const int a = 190;
  if (set_.showClock && clock_.valid()) {
    CivilClock::Local L = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin);
    StrBuf s(b, sizeof b); fmtTime(s, L.hour, L.minute, set_.clock24h);
    canvas_.text(CX, 22.f, b, FONT_M, MUTED, a, Align::Center);
  }
  if (set_.showWeather && set_.weatherEnabled && weather_.valid) {
    drawIcon(canvas_, weatherIcon(weather_.code, weather_.isDay), 18.f, 16.f, 18.f, MUTED, a);
    StrBuf s(b, sizeof b); s.num((weather_.tempC10 + (weather_.tempC10 >= 0 ? 5 : -5)) / 10).put("°");
    canvas_.text(32.f, 22.f, b, FONT_S, MUTED, a);
  }
  if (caps_.battery && set_.showBattery && power_.known && power_.battery) {
    drawBattery(canvas_, W - 22.f, 16.f, 22.f, power_.percent, power_.charging, MUTED, a);
    StrBuf s(b, sizeof b); s.num(power_.percent).put('%');
    canvas_.text(W - 38.f, 21.f, b, FONT_S, MUTED, a, Align::Right);
  }
}

// Controles que aparecem ao tocar no rosto e somem sozinhos (coluna à direita).
void App::drawDock(ms_t now) {
  if (!dockUntil_) return;
  float t = (float)(now + 4000 - dockUntil_);  // tempo desde que apareceu
  float in = fclamp(t / 180.f, 0.f, 1.f);
  float out = dockUntil_ > now ? fclamp((float)(dockUntil_ - now) / 300.f, 0.f, 1.f) : 0.f;
  float k = fminf_(in, out);
  if (k <= 0.f) return;
  int a = (int)(255.f * k);
  float x = W - 25.f + 30.f * (1.f - easeOut(k));
  canvas_.fillRoundRect(x - 23.f, 50.f, 46.f, 140.f, 23.f, SURF, (int)(235.f * k));
  roundButton(canvas_, x, 74.f, 17.f, SURF2, Icon::Tomato, acc(C_POMO), pressedHit_ == B_DOCK_POMO, a);
  roundButton(canvas_, x, 120.f, 20.f, ACCENT, Icon::Menu, ON_ACCENT, pressedHit_ == B_DOCK_MENU, a);
  roundButton(canvas_, x, 166.f, 17.f, SURF2, Icon::Shark, acc(C_WEATHER), pressedHit_ == B_DOCK_GAMES, a);
  if (k > 0.5f) {
    hit(B_DOCK_POMO, x, 74.f, 22.f);
    hit(B_DOCK_MENU, x, 120.f, 24.f);
    hit(B_DOCK_GAMES, x, 166.f, 22.f);
  }
}

void App::drawFaceScreen(ms_t now) {
  if (scene_.active()) scene_.draw(canvas_, now, faceStyle());   // reação lendária no lugar do rosto
  else face_.draw(canvas_, now, faceStyle());
  // progresso discreto do Pomodoro/timer: linha fina na borda inferior
  int pm = -1; Color col = C_TIMER;
  if (pomo_.active()) { pm = pomo_.permille(now); col = pomo_.phase() == PomoPhase::Focus ? C_POMO : C_SW; }
  else for (int i = 0; i < kTimers; i++)
    if (timers_[i].state() == RunState::Running || timers_[i].state() == RunState::Paused) { pm = timers_[i].permilleDone(now); break; }
  if (pm >= 0) {
    canvas_.fillRect(0, SCR_H - 3, SCR_W, 3, SURF2);
    canvas_.fillRect(0, SCR_H - 3, SCR_W * tbClamp(pm, 0, 1000) / 1000, 3, acc(col));
  }
  drawFaceOverlays(now);
  if (!scene_.active()) drawDock(now);
}

// ── abertura ────────────────────────────────────────────────────────────────
// Barbatana curva (bordas em curvas de Bézier, duas tonalidades) que surge do
// mar, desliza com inclinação proporcional à velocidade, deixa rastro de espuma,
// mergulha com respingos e dá lugar ao rosto. Tudo é função do tempo (sem estado).
static Pt bez2(Pt a, Pt c, Pt b, float t) {
  float u = 1.f - t;
  return Pt{u * u * a.x + 2.f * u * t * c.x + t * t * b.x, u * u * a.y + 2.f * u * t * c.y + t * t * b.y};
}

static void drawFin(Canvas& cv, float ox, float oy, float sc, float lean) {
  float sn = fsin(lean), cs = fcos(lean);
  auto tf = [&](Pt p) {   // gira em torno da base (lean > 0 = ponta para a frente)
    float x = p.x * sc, y = p.y * sc;
    return Pt{ox + x * cs - y * sn, oy + x * sn + y * cs};
  };
  const Pt P0 = {20.f, 0.f}, C0 = {17.f, -40.f}, TIP = {-13.f, -54.f}, C1 = {-9.f, -20.f}, P2 = {-25.f, 2.f};
  Pt v[24]; int n = 0;
  for (int i = 0; i <= 10; i++) v[n++] = tf(bez2(P0, C0, TIP, (float)i / 10.f));       // bordo de ataque (convexo)
  for (int i = 1; i <= 9; i++) v[n++] = tf(bez2(TIP, C1, P2, (float)i / 9.f));         // bordo de fuga (côncavo)
  v[n++] = tf(Pt{-25.f, 18.f}); v[n++] = tf(Pt{20.f, 18.f});                            // base (sob a água)
  cv.fillPolygon(v, n, 0x7F9CBB);
  // metade da frente mais clara, separada por uma curva suave
  n = 0;
  for (int i = 0; i <= 10; i++) v[n++] = tf(bez2(P0, C0, TIP, (float)i / 10.f));
  for (int i = 1; i <= 8; i++) v[n++] = tf(bez2(TIP, Pt{1.f, -24.f}, Pt{3.f, 18.f}, (float)i / 8.f));
  v[n++] = tf(Pt{20.f, 18.f});
  cv.fillPolygon(v, n, 0xDCE9F7);
  // brilho no bordo de ataque
  Pt h0 = tf(bez2(P0, C0, TIP, 0.25f)), h1 = tf(bez2(P0, C0, TIP, 0.75f));
  cv.line(h0.x, h0.y, h1.x, h1.y, 2.2f * sc, 0xFFFFFF, 170);
}

void App::drawIntro(ms_t now) {
  float T = (float)(set_.shortIntro ? INTRO_SHORT_MS : INTRO_FULL_MS);
  float t = (float)(now - bootAt_);
  float tA = T * 0.44f, tB = T * 0.63f;     // nadar | mergulhar | rosto
  Color eye = exprColorRgb(set_.eyeColor, set_.theme != 0);
  float ph = t / 520.f;
  float drop = t > tB ? 80.f * easeInOut((t - tB) / (T - tB) * 1.6f) : 0.f;   // o mar desce ao final
  auto surf = [&](float x, float yb, float k) { return yb + drop + 5.f * fsin(ph + k + x * 0.03f) + 2.f * fsin(ph * 1.7f + x * 0.071f + k); };
  auto sea = [&](float yb, float k, Color col, int a) {
    Pt v[24]; int n = 0;
    for (int x = 0; x <= SCR_W; x += 16) v[n++] = Pt{(float)x, surf((float)x, yb, k)};
    v[n++] = Pt{W, H + 4.f}; v[n++] = Pt{0.f, H + 4.f};
    canvas_.fillPolygon(v, n, col, a);
  };
  const float YB = 160.f, YF = 176.f;
  sea(YB, 0.f, 0x1563A8, 150);                                 // mar ao fundo
  // trajetória da barbatana: surge, desliza (rápido no meio, devagar nas pontas) e mergulha
  auto finX = [&](float tt) { return tt < tA ? -24.f + 222.f * easeInOut(tt / tA) : 198.f + 34.f * easeOut((tt - tA) / (tB - tA)); };
  float fx = finX(t);
  float vx = (finX(t + 16.f) - finX(t - 16.f)) / 32.f;        // px/ms
  float rise = 1.f - easeOut(t / (T * 0.16f));
  float dive = t > tA ? fclamp((t - tA) / ((tB - tA) * 0.8f), 0.f, 1.f) : 0.f;
  dive = dive * dive * (3.f - 2.f * dive);
  float fy = YF + 8.f + 80.f * rise + 96.f * dive + 5.f * fsin(t / 150.f) * (1.f - dive);
  float lean = -fclamp(vx * 0.7f, -0.38f, 0.38f) + 0.10f * fsin(t / 210.f) + 0.95f * dive;
  if (t < tB && fy < H + 40.f) {
    // rastro de espuma atrás da barbatana
    for (int i = 1; i <= 6; i++) {
      float wx = fx - 16.f - 13.f * (float)i - 4.f * fsin(t / 90.f + (float)i);
      float a = (1.f - (float)i / 7.f) * (1.f - rise) * (1.f - dive) * fclamp(vx * 3.f, 0.f, 1.f);
      canvas_.fillEllipse(wx, surf(wx, YF, 1.f) + 1.f, 9.f - 0.9f * (float)i, 2.8f, 0xFFFFFF, (int)(235.f * a));
    }
    drawFin(canvas_, fx, fy, 1.3f, lean);
  }
  sea(YF, 1.f, 0x0A3A6E, 255);                                 // mar da frente (cobre a base da barbatana)
  for (int x = 0; x < SCR_W; x += 8) {                          // crista clara das ondas
    float y0 = surf((float)x, YF, 1.f), y1 = surf((float)(x + 8), YF, 1.f);
    canvas_.line((float)x, y0, (float)(x + 8), y1, 1.6f, 0x5CC8FF, 150);
  }
  if (t < tA + (tB - tA) * 0.4f) {                              // esteira em "V" e "bigode" de água na base
    float k = (1.f - rise) * (1.f - dive) * fclamp(vx * 4.f, 0.2f, 1.f);
    float bx = fx + 22.f, by = surf(bx, YF, 1.f) + 3.f;
    canvas_.arc(bx, by + 2.f, 9.f, 2.4f, -1.3f, 1.6f, 0xFFFFFF, (int)(220.f * k));
    for (int sgn = -1; sgn <= 1; sgn += 2) {
      float x0 = fx - 8.f, x1 = fx - 70.f;
      canvas_.line(x0, surf(x0, YF, 1.f) + 3.f, x1, surf(x1, YF, 1.f) + 3.f + 7.f * (float)sgn, 1.6f, 0xFFFFFF, (int)(120.f * k));
    }
  }
  // mergulho: respingos em parábola e anel de espuma
  if (t > tA + (tB - tA) * 0.35f) {
    float p = (t - (tA + (tB - tA) * 0.35f)) / ((T - tA) * 0.55f);
    if (p < 1.f) {
      float sx = finX(tA + (tB - tA) * 0.45f) + 4.f, sy = surf(sx, YF, 1.f);
      canvas_.fillEllipse(sx, sy + 2.f, 10.f + 46.f * p, 3.f + 7.f * p, 0xFFFFFF, (int)(170.f * (1.f - p)));
      canvas_.fillEllipse(sx, sy + 2.f, 6.f + 40.f * p, 2.f + 5.f * p, 0x0A3A6E, (int)(255.f * (1.f - p)));
      for (int i = 0; i < 9; i++) {
        float ang = -1.25f + 2.5f * (float)i / 8.f;
        float sp = 0.9f + 0.25f * (float)(i % 3);
        float dx = 70.f * sp * fsin(ang) * p, dy = -95.f * sp * fcos(ang) * p + 150.f * p * p;
        float r = 2.2f + (float)(i % 3) * 0.9f;
        canvas_.fillCircle(sx + dx, sy + dy, r * (1.f - 0.5f * p), i % 2 ? (Color)0xFFFFFF : (Color)0xBFE6FF, (int)(235.f * (1.f - p)));
      }
    }
  }
  // bolhas subindo onde mergulhou
  if (t > tA && t < T * 0.9f) {
    float p = (t - tA) / (T * 0.9f - tA);
    for (int i = 0; i < 5; i++) {
      float q = fclamp(p * 1.4f - 0.08f * (float)i, 0.f, 1.f);
      if (q <= 0.f || q >= 1.f) continue;
      float bx = 214.f + 10.f * fsin(q * 9.f + (float)i * 1.7f) + 7.f * (float)(i - 2);
      canvas_.ring(bx, YF + 40.f + drop - 46.f * q, 2.5f + (float)(i % 3), 1.4f, 0xFFFFFF, (int)(200.f * (1.f - q)));
    }
  }
  // nome
  if (t > tA + (tB - tA) * 0.3f) {
    float p = fclamp((t - (tA + (tB - tA) * 0.3f)) / ((tB - tA) * 0.9f), 0.f, 1.f);
    float fade = t > tB ? 1.f - fclamp((t - tB) / ((T - tB) * 0.22f), 0.f, 1.f) : 1.f;
    int a = (int)(255.f * fclamp(p * 1.6f, 0.f, 1.f) * fade);
    if (a > 0) {
      canvas_.textCentered(96.f + 18.f * (1.f - easeBack(p)), "TubaBot", FONT_XL, TEXT, a);
      canvas_.textCentered(120.f + 18.f * (1.f - easeBack(p)), "TubaLabs", FONT_S, MUTED, a * 3 / 4);
    }
  }
  if (t <= tB) return;
  // rosto: sobe da água abrindo os olhos
  float pe = t - tB - (T - tB) * 0.2f;
  if (pe <= 0.f) return;
  float pc = fclamp(pe / ((T - tB) * 0.4f), 0.f, 1.f);
  float sp = T / (float)INTRO_FULL_MS;
  int open = pe < 260.f * sp ? 1000 - (int)(pe * 1000.f / (260.f * sp)) : 0;
  int blink2 = pe > 520.f * sp && pe < 720.f * sp ? 1000 - (int)(fabsf_(pe - 620.f * sp) * 10.f / sp) : 0;
  FaceDef f = faceDef(baseExpr_);
  f.extras = FX_NONE;
  FaceXf xf; xf.s = 0.5f * (0.85f + 0.15f * easeBack(pc)); xf.cy = 88.f + 26.f * (1.f - easeOut(pc));
  canvas_.setGlobalAlpha((int)(255.f * fclamp(pc * 1.8f, 0.f, 1.f)));
  FaceRenderer::drawFace(canvas_, f, faceStyle(), now, xf, 0, 0, tbClamp(tbMax(open, blink2), 0, 1000), 0.f);
  canvas_.setGlobalAlpha(255);
  if (pc > 0.6f || set_.shortIntro) {
    char b[48]; StrBuf s(b, sizeof b);
    s.put(tr("Oi"));
    if (set_.nickname[0]) s.put(", ").put(set_.nickname);
    s.put('!');
    int a = set_.shortIntro ? 255 : (int)(255.f * fclamp((pc - 0.6f) * 2.5f, 0.f, 1.f));
    canvas_.textCentered(166.f, b, FONT_L, set_.theme ? TEXT : eye, a);
  }
}

// ── relógio (3 mostradores) ─────────────────────────────────────────────────
void App::drawClock(ms_t now) {
  char b[40];
  Color accent = ACCENT;
  if (!clock_.valid()) {
    canvas_.textCentered(118.f, "--:--", FONT_XXL, DIM);
    canvas_.textCentered(156.f, "Hora não ajustada", FONT_M, MUTED);
    canvas_.textCentered(180.f, "use o Wi-Fi ou o portal", FONT_S, DIM);
    pageDots(canvas_, 3, 1);
    return;
  }
  CivilClock::Local L = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin);
  int ms = (int)(now % 1000);
  if (set_.watchFace == 1) {
    // analógico (mostrador centralizado, data à direita)
    const float cx = 120.f, cy = 120.f, R = 100.f;
    for (int i = 0; i < 60; i++) {
      float a = (float)i * kTau / 60.f;
      bool hr = i % 5 == 0;
      float r0 = hr ? R - 12.f : R - 6.f;
      canvas_.line(cx + r0 * fsin(a), cy - r0 * fcos(a), cx + R * fsin(a), cy - R * fcos(a), hr ? 3.5f : 1.5f, hr ? TEXT : DIM);
    }
    float sec = (float)L.second + (float)ms / 1000.f;
    float mn = (float)L.minute + sec / 60.f;
    float hr = (float)(L.hour % 12) + mn / 60.f;
    float ah = hr * kTau / 12.f, am = mn * kTau / 60.f, as = sec * kTau / 60.f;
    canvas_.line(cx, cy, cx + 52.f * fsin(ah), cy - 52.f * fcos(ah), 8.f, TEXT);
    canvas_.line(cx, cy, cx + 80.f * fsin(am), cy - 80.f * fcos(am), 5.f, TEXT);
    canvas_.line(cx - 14.f * fsin(as), cy + 14.f * fcos(as), cx + 88.f * fsin(as), cy - 88.f * fcos(as), 2.f, accent);
    canvas_.fillCircle(cx, cy, 6.f, accent);
    canvas_.fillCircle(cx, cy, 2.5f, BG);
    StrBuf d(b, sizeof b); d.put(tr(WEEKDAYS_FULL[L.weekday]));
    canvas_.text(274.f, 106.f, b, FONT_M, MUTED, 255, Align::Center);
    StrBuf d2(b, sizeof b); d2.num(L.day).put(' ').put(tr(MONTHS[L.month - 1]));
    canvas_.text(274.f, 136.f, b, FONT_L, TEXT, 255, Align::Center);
  } else if (set_.watchFace == 2) {
    // "Tuba": rostinho à esquerda, hora à direita
    // rosto vivo (pisca, olha em volta, confere a hora, faz caretinhas)
    face_.drawAt(canvas_, now, faceStyle(), 84.f, 116.f, 0.40f, (uint16_t)(FX_BLUSH | FX_ZZZ | FX_SPARKLE));
    StrBuf s(b, sizeof b); fmtTime(s, L.hour, L.minute, set_.clock24h);
    canvas_.text(232.f, 130.f, b, FONT_XXL, TEXT, 255, Align::Center);
    StrBuf d(b, sizeof b); d.put(tr(WEEKDAYS[L.weekday])).put(", ").num(L.day).put(' ').put(tr(MONTHS[L.month - 1]));
    canvas_.text(232.f, 162.f, b, FONT_M, MUTED, 255, Align::Center);
  } else {
    // digital
    StrBuf s(b, sizeof b); fmtTime(s, L.hour, L.minute, set_.clock24h);
    const Font* fonts[2] = {&FONT_CLOCK, &FONT_XXL};
    const Font& f = Canvas::fitFont(b, 300, fonts, 2);
    canvas_.textCentered(140.f, b, f, TEXT);
    if (!set_.clock24h) canvas_.textCentered(44.f, L.hour < 12 ? "AM" : "PM", FONT_M, accent);
    StrBuf d(b, sizeof b); d.put(tr(WEEKDAYS_FULL[L.weekday])).put(", ").num(L.day).put(' ').put(tr(MONTHS[L.month - 1]));
    canvas_.textCentered(180.f, b, FONT_M, MUTED);
    float p = ((float)L.second + (float)ms / 1000.f) / 60.f;
    canvas_.fillRoundRect(80.f, 198.f, 160.f, 4.f, 2.f, SURF2);
    canvas_.fillCircle(80.f + 160.f * p, 200.f, 4.f, accent);
  }
  if (now - screenAt_ < 2500) pageDots(canvas_, 3, 1);
}

// ── Hoje ────────────────────────────────────────────────────────────────────
void App::drawToday(ms_t now) {
  char b[48];
  const char* greet = "Olá";
  if (clock_.valid()) {
    int h = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin).hour;
    greet = h < 5 ? "Boa noite" : h < 12 ? "Bom dia" : h < 18 ? "Boa tarde" : "Boa noite";
  }
  StrBuf g(b, sizeof b); g.put(tr(greet)); if (set_.nickname[0]) g.put(", ").put(set_.nickname);
  const Font* fonts[2] = {&FONT_L, &FONT_M};
  canvas_.text(16.f, 30.f, b, Canvas::fitFont(b, 200, fonts, 2), TEXT);
  if (clock_.valid()) {
    CivilClock::Local L = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin);
    StrBuf d(b, sizeof b); d.put(tr(WEEKDAYS[L.weekday])).put(", ").num(L.day).put(' ').put(tr(MONTHS[L.month - 1]));
    canvas_.text(W - 16.f, 30.f, b, FONT_S, MUTED, 255, Align::Right);
  }
  float y = 64.f;
  auto row = [&](Icon ic, Color col, const char* text, const char* sub) {
    canvas_.fillRoundRect(12.f, y - 19.f, W - 24.f, 38.f, 14.f, SURF);
    canvas_.fillCircle(32.f, y, 13.f, col);
    drawIcon(canvas_, ic, 32.f, y, 15.f, INK);
    canvas_.text(54.f, y + 6.f, text, FONT_M, TEXT);
    if (sub) canvas_.text(W - 24.f, y + 5.f, sub, FONT_S, MUTED, 255, Align::Right);
    y += 44.f;
  };
  if (set_.weatherEnabled && weather_.valid) {
    char t[32]; StrBuf s(t, sizeof t); s.num((weather_.tempC10 + (weather_.tempC10 >= 0 ? 5 : -5)) / 10).put("° · ").put(tr(weatherText(weather_.code)));
    row(weatherIcon(weather_.code, weather_.isDay), C_WEATHER, t, set_.weatherCity == 255 ? "local próprio" : CITY_TABLE[set_.weatherCity].name);
  }
  int mins = 0, na = nextAlarmIndex(now, &mins);
  if (na >= 0) {
    char t[32]; StrBuf s(t, sizeof t); fmtTime(s, set_.alarms[na].hour, set_.alarms[na].minute, set_.clock24h); s.put(" · ").put(set_.alarms[na].label);
    char u[32]; StrBuf su(u, sizeof u); su.put(tr("em ")); formatDuration(su, (ms_t)mins * 60000, false);
    row(Icon::Alarm, C_ALARM, t, u);
  }
  if (pomo_.active() && y < 200.f) {
    char t[32]; StrBuf s(t, sizeof t); s.put(tr(pomo_.phase() == PomoPhase::Focus ? "Foco " : "Pausa ")); formatDuration(s, pomo_.remainingMs(now), true);
    row(Icon::Tomato, C_POMO, t, pomo_.state() == RunState::Paused ? "pausado" : nullptr);
  }
  for (int i = 0; i < kTimers && y < 200.f; i++) if (timers_[i].state() == RunState::Running || timers_[i].state() == RunState::Paused) {
    char t[32]; StrBuf s(t, sizeof t); s.put(tr("Timer ")).num(i + 1).put("  "); formatDuration(s, timers_[i].remainingMs(now), true);
    row(Icon::Timer, C_TIMER, t, timers_[i].state() == RunState::Paused ? "pausado" : nullptr);
  }
  if (y < 110.f) {
    char t[40]; StrBuf s(t, sizeof t); s.put(tr("humor ")).num((P().mood + 100) / 2).put('%');
    row(Icon::Heart, C_STATS, "Tudo tranquilo por aqui", t);
  }
  if (caps_.battery && power_.known && power_.battery && y < 200.f) {
    char t[24]; StrBuf s(t, sizeof t); s.put(tr("Bateria ")).num(power_.percent).put('%');
    row(Icon::Bolt, C_VOL, t, power_.charging ? "carregando" : nullptr);
  }
  if (now - screenAt_ < 2500) pageDots(canvas_, 3, 2);
}

// ── menus ───────────────────────────────────────────────────────────────────
void App::drawGridScreen(const char* title, ms_t now) {
  (void)now;
  drawGrid(canvas_, items_, itemN_, pressedItem_);
  drawBackChevron(title);
}

void App::drawListScreen(const char* title, Color col, ms_t now) {
  (void)now;
  list_.draw(canvas_, items_, itemN_, pressedItem_);
  canvas_.fillRect(0, 0, SCR_W, ListView::kTop, BG);  // o cabeçalho fica por cima da lista
  drawBackChevron(title, col);
}

// ── cor das expressões (prévia ao vivo) ─────────────────────────────────────
void App::drawColorPick(ms_t now) {
  FaceXf xf; xf.s = 0.40f; xf.cx = CX; xf.cy = 78.f;
  FaceDef f = faceDef(face_.shown()); f.extras = (uint16_t)(f.extras & (FX_BLUSH | FX_SPARKLE));
  int blink = 0;
  FaceRenderer::drawFace(canvas_, f, faceStyle(), now, xf, 0, 0, blink, fsin((float)(now % 4200) / 4200.f * kTau));
  bool light = set_.theme != 0;
  for (int i = 0; i < EXPR_COLOR_COUNT; i++) {
    float x = 48.f + 56.f * (float)(i % 5), y = i < 5 ? 160.f : 206.f;
    Color c = exprColorRgb(i, light);
    if (i == set_.eyeColor) canvas_.ring(x, y, 22.f, 3.f, TEXT);
    canvas_.fillCircle(x, y, 17.f, c);
    if (c == 0xF1F5FB || (light && i == 2)) canvas_.ring(x, y, 17.f, 1.f, LINE);
    if (i == set_.eyeColor) drawIcon(canvas_, Icon::Check, x, y, 15.f, luminanceDark(c) ? (Color)0xFFFFFF : (Color)0x101318);
    hit(B_COLOR0 + i, x, y, 24.f);
  }
  canvas_.textCentered(134.f, EXPR_COLORS[set_.eyeColor].name, FONT_M, TEXT);
  drawBackChevron();
}

// ── painel rápido (deslizar para baixo no rosto) ────────────────────────────
void App::drawQuick(ms_t now) {
  char b[16];
  if (clock_.valid()) {
    CivilClock::Local L = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin);
    StrBuf s(b, sizeof b); fmtTime(s, L.hour, L.minute, set_.clock24h);
    canvas_.text(16.f, 32.f, b, FONT_L, TEXT);
  } else canvas_.text(16.f, 32.f, "Ajustes rápidos", FONT_M, TEXT);
  if (caps_.battery && power_.known && power_.battery) {
    drawBattery(canvas_, W - 24.f, 24.f, 26.f, power_.percent, power_.charging, MUTED);
    StrBuf s(b, sizeof b); s.num(power_.percent).put('%');
    canvas_.text(W - 42.f, 30.f, b, FONT_S, MUTED, 255, Align::Right);
  }
  struct Q { int id; Icon ic; bool on; };
  Q q[5] = {{B_Q_SILENT, Icon::Silent, (bool)set_.silent}, {B_Q_THEME, set_.theme ? Icon::Sun : Icon::Moon, false},
            {B_Q_MIC, Icon::Note, (bool)set_.micReactions}, {B_Q_TORCH, Icon::Brightness, false}, {B_Q_NET, Icon::Wifi, false}};
  for (int i = 0; i < 5; i++) {
    if (q[i].id == B_Q_MIC && !caps_.mic) continue;
    float x = 40.f + 60.f * (float)i;
    button(q[i].id, x, 80.f, 21.f, q[i].on ? ACCENT : SURF2, q[i].ic, q[i].on ? ON_ACCENT : TEXT);
  }
  slider(canvas_, SLIDER_X0, 140.f, SLIDER_W, 38.f, (set_.brightness - 10) * 1000 / 245, acc(C_BRIGHT), Icon::Brightness);
  hitRect(B_Q_BRIGHT, SLIDER_X0 - 12.f, 118.f, SLIDER_W + 24.f, 44.f);
  slider(canvas_, SLIDER_X0, 190.f, SLIDER_W, 38.f, set_.volume * 10, caps_.speaker ? acc(C_VOL) : DIM, set_.silent ? Icon::Silent : Icon::Speaker);
  hitRect(B_Q_VOL, SLIDER_X0 - 12.f, 168.f, SLIDER_W + 24.f, 44.f);
  hint(canvas_, "deslize para cima: fechar", 232.f);
}

// ── Volume / Brilho ─────────────────────────────────────────────────────────
void App::drawLevelScreen(int which, ms_t now) {
  (void)now;
  bool vol = which == 0;
  char b[16];
  int v = vol ? set_.volume : (set_.brightness * 100 + 127) / 255;
  Color col = acc(vol ? C_VOL : C_BRIGHT);
  drawBackChevron(vol ? "Volume" : "Brilho");
  StrBuf s(b, sizeof b); s.num(v).put('%');
  canvas_.textCentered(100.f, b, FONT_XL, vol && set_.silent ? DIM : TEXT);
  int pm = vol ? set_.volume * 10 : (set_.brightness - 10) * 1000 / 245;
  slider(canvas_, SLIDER_X0, 140.f, SLIDER_W, 44.f, pm, vol && !caps_.speaker ? DIM : col, vol ? (set_.silent ? Icon::Silent : Icon::Speaker) : Icon::Brightness);
  hitRect(vol ? B_SL_VOL : B_SL_BRIGHT, SLIDER_X0 - 6.f, 114.f, SLIDER_W + 12.f, 52.f);
  button(B_SL_MINUS, 20.f, 140.f, 13.f, SURF2, Icon::Minus, TEXT);
  button(B_SL_PLUS, W - 20.f, 140.f, 13.f, SURF2, Icon::Plus, TEXT);
  // alternador complementar
  const char* lbl = vol ? "Silencioso" : "Brilho reduzido à noite";
  bool on = vol ? set_.silent : set_.nightDim;
  canvas_.fillRoundRect(16.f, 180.f, W - 32.f, 40.f, 14.f, SURF);
  canvas_.text(32.f, 206.f, lbl, FONT_M, TEXT);
  toggle(canvas_, W - 44.f, 200.f, on, ACCENT);
  hitRect(B_SL_TEST, 16.f, 180.f, W - 32.f, 40.f);
  if (vol && !caps_.speaker) canvas_.textCentered(70.f, "sem alto-falante", FONT_S, WARN);
}

// ── Pomodoro ────────────────────────────────────────────────────────────────
void App::drawPomodoro(ms_t now) {
  char b[32];
  bool focus = pomo_.phase() == PomoPhase::Focus;
  Color col = acc(focus ? C_POMO : C_SW);
  drawBackChevron(focus ? "Pomodoro · Foco" : (pomo_.phase() == PomoPhase::ShortBreak ? "Pomodoro · Pausa" : "Pomodoro · Pausa longa"), col);
  const float rx = 112.f, ry = 136.f;
  progressRing(canvas_, rx, ry, 80.f, 10.f, pomo_.permille(now), col);
  StrBuf s(b, sizeof b); formatDuration(s, pomo_.remainingMs(now), true);
  canvas_.text(rx, ry + 10.f, b, FONT_XL, pomo_.state() == RunState::Paused ? MUTED : TEXT, 255, Align::Center);
  int cyc = set_.pomoCyclesBeforeLong ? pomo_.focusDone() % set_.pomoCyclesBeforeLong : 0;
  for (int i = 0; i < set_.pomoCyclesBeforeLong && i < 8; i++) {
    float x = rx - (float)(tbMin<int>(set_.pomoCyclesBeforeLong, 8) - 1) * 7.f + 14.f * (float)i;
    canvas_.fillCircle(x, ry + 34.f, 4.f, i < cyc ? acc(C_POMO) : SURF2);
  }
  if (pomo_.state() == RunState::Paused) canvas_.text(rx, ry - 26.f, "pausado", FONT_S, MUTED, 255, Align::Center);
  button(B_PLAY, 256.f, 112.f, 30.f, col, pomo_.state() == RunState::Running ? Icon::Pause : Icon::Play, INK);
  button(B_RESET, 222.f, 190.f, 19.f, SURF2, Icon::Cross, TEXT);
  button(B_SKIP, 290.f, 190.f, 19.f, SURF2, Icon::Skip, TEXT);
}

// ── Timers ──────────────────────────────────────────────────────────────────
void App::drawTimers(ms_t now) {
  char b[32];
  Countdown& t = timers_[timerSel_];
  Color col = acc(C_TIMER);
  drawBackChevron("Timer", col);
  const float rx = 112.f, ry = 136.f;
  progressRing(canvas_, rx, ry, 80.f, 10.f, t.permilleDone(now), col);
  StrBuf s(b, sizeof b); formatDuration(s, t.remainingMs(now), true);
  const Font* fonts[2] = {&FONT_XL, &FONT_L};
  canvas_.text(rx, ry + 10.f, b, Canvas::fitFont(b, 140, fonts, 2), t.state() == RunState::Done ? col : TEXT, 255, Align::Center);
  if (t.state() == RunState::Paused) canvas_.text(rx, ry + 34.f, "pausado", FONT_S, MUTED, 255, Align::Center);
  for (int i = 0; i < kTimers; i++) {
    float x = 222.f + 34.f * (float)i, y = 58.f;
    bool sel = i == timerSel_;
    canvas_.fillCircle(x, y, 14.f, sel ? col : SURF2);
    StrBuf n(b, sizeof b); n.num(i + 1);
    canvas_.text(x, y + 5.f, b, FONT_S, sel ? INK : TEXT, 255, Align::Center);
    if (timers_[i].state() == RunState::Running) canvas_.fillCircle(x + 10.f, y - 10.f, 4.f, OK);
    hit(B_TIMER_SEL0 + i, x, y, 17.f);
  }
  button(B_PLAY, 256.f, 124.f, 28.f, col, t.state() == RunState::Running ? Icon::Pause : Icon::Play, INK);
  button(B_TIMER_DUR, 222.f, 196.f, 18.f, SURF2, Icon::Timer, TEXT);
  if (t.state() == RunState::Running || t.state() == RunState::Paused) button(B_TIMER_CANCEL, 290.f, 196.f, 18.f, SURF2, Icon::Cross, TEXT);
  else button(B_TIMER_REPEAT, 290.f, 196.f, 18.f, SURF2, Icon::Reset, TEXT);
}

// ── Cronômetro ──────────────────────────────────────────────────────────────
void App::drawStopwatch(ms_t now) {
  char b[32];
  Color col = acc(C_SW);
  drawBackChevron("Cronômetro", col);
  uint64_t e = sw_.elapsedMs(now);
  uint64_t hh = e / 3600000, mm = (e / 60000) % 60, ss = (e / 1000) % 60, cs = (e / 10) % 100;
  StrBuf s(b, sizeof b);
  if (hh) s.unum(hh).put(':');
  s.numPad((int64_t)mm, 2).put(':').numPad((int64_t)ss, 2);
  const Font& f = hh ? FONT_XL : FONT_XXL;
  int w = Canvas::textWidth(b, f) + Canvas::textWidth(",00", FONT_L) + 4;
  float x0 = CX - (float)w * 0.5f;
  int tw = canvas_.text(x0, 120.f, b, f, TEXT);
  StrBuf c(b, sizeof b); c.put(',').numPad((int64_t)cs, 2);
  canvas_.text(x0 + (float)tw + 4.f, 120.f, b, FONT_L, col);
  float p = (float)(e % 60000) / 60000.f;
  canvas_.fillRoundRect(60.f, 136.f, 200.f, 4.f, 2.f, SURF2);
  if (e) canvas_.fillRoundRect(60.f, 136.f, 4.f + 196.f * p, 4.f, 2.f, col);
  if (sw_.limitHit()) canvas_.textCentered(164.f, "Limite atingido", FONT_S, WARN);
  else if (lapN_) {
    StrBuf l(b, sizeof b); l.put(tr("volta ")).num(lapN_).put("  "); formatDuration(l, laps_[lapN_ - 1], false);
    canvas_.textCentered(164.f, b, FONT_S, MUTED);
  }
  button(B_RESET, CX - 74.f, 204.f, 20.f, SURF2, Icon::Reset, TEXT);
  button(B_PLAY, CX, 204.f, 26.f, col, sw_.state() == RunState::Running ? Icon::Pause : Icon::Play, INK);
  button(B_LAP, CX + 74.f, 204.f, 20.f, sw_.state() == RunState::Running ? SURF2 : SURF, Icon::Plus, sw_.state() == RunState::Running ? TEXT : DIM);
}

// ── Alarmes ─────────────────────────────────────────────────────────────────
void App::drawAlarms(ms_t now) {
  (void)now;
  char b[40];
  Color col = acc(C_ALARM);
  drawBackChevron("Alarmes", col);
  for (int i = 0; i < ALARM_COUNT; i++) {
    const Alarm& a = set_.alarms[i];
    float y = 68.f + 54.f * (float)i;
    canvas_.fillRoundRect(12.f, y - 24.f, W - 24.f, 48.f, 16.f, SURF);
    StrBuf s(b, sizeof b); fmtTime(s, a.hour, a.minute, true);
    canvas_.text(26.f, y + 8.f, b, FONT_L, a.enabled ? TEXT : DIM);
    StrBuf d(b, sizeof b);
    if (!a.days) d.put(tr("uma vez"));
    else if (a.days == 0x7F) d.put(tr("todo dia"));
    else if (a.days == 0x3E) d.put(tr("seg a sex"));
    else if (a.days == 0x41) d.put(tr("fim de semana"));
    else for (int k = 0; k < 7; k++) if ((a.days >> k) & 1) d.put(tr(WEEKDAYS[k])).put(' ');
    canvas_.text(104.f, y - 2.f, a.label, FONT_S, a.enabled ? TEXT : DIM);
    canvas_.text(104.f, y + 15.f, b, FONT_S, MUTED);
    hitRect(B_ALARM_OPEN0 + i, 12.f, y - 24.f, W - 90.f, 48.f);
    toggle(canvas_, W - 40.f, y, a.enabled, col);
    hit(B_ALARM_TOGGLE0 + i, W - 40.f, y, 24.f);
  }
  if (!clock_.valid()) hint(canvas_, "ajuste a hora pelo Wi-Fi ou portal", 232.f);
}

void App::drawAlarmEdit(ms_t now) {
  (void)now;
  char b[16];
  Alarm& a = set_.alarms[editAlarm_];
  Color col = acc(C_ALARM);
  drawBackChevron(a.label, col);
  StrBuf h(b, sizeof b); h.numPad(a.hour, 2);
  canvas_.text(CX - 58.f, 138.f, b, FONT_XXL, TEXT, 255, Align::Center);
  canvas_.text(CX, 134.f, ":", FONT_XXL, MUTED, 255, Align::Center);
  StrBuf m(b, sizeof b); m.numPad(a.minute, 2);
  canvas_.text(CX + 58.f, 138.f, b, FONT_XXL, TEXT, 255, Align::Center);
  button(B_AL_H_PLUS, CX - 58.f, 62.f, 15.f, SURF2, Icon::Plus, TEXT);
  button(B_AL_H_MINUS, CX - 58.f, 164.f, 15.f, SURF2, Icon::Minus, TEXT);
  button(B_AL_M_PLUS, CX + 58.f, 62.f, 15.f, SURF2, Icon::Plus, TEXT);
  button(B_AL_M_MINUS, CX + 58.f, 164.f, 15.f, SURF2, Icon::Minus, TEXT);
  for (int k = 0; k < 7; k++) {
    float x = 40.f + 32.f * (float)k;
    bool on = (a.days >> k) & 1;
    canvas_.fillCircle(x, 212.f, 13.f, on ? col : SURF2);
    // inicial do dia no idioma atual (D S T Q Q S S / S M T W T F S / D L M M J V S)
    char ini[5] = {0};
    const char* wd = tr(WEEKDAYS[k]);
    int n = ((uint8_t)wd[0] & 0xE0) == 0xC0 ? 2 : 1;
    for (int i = 0; i < n; i++) ini[i] = wd[i];
    if (ini[0] >= 'a' && ini[0] <= 'z') ini[0] = (char)(ini[0] - 32);
    if ((uint8_t)ini[0] == 0xC3 && (uint8_t)ini[1] >= 0xA0) ini[1] = (char)((uint8_t)ini[1] - 0x20);  // á -> Á
    canvas_.text(x, 217.f, ini, FONT_S, on ? INK : MUTED, 255, Align::Center);
    hit(B_AL_DAY0 + k, x, 212.f, 16.f);
  }
  button(B_AL_DONE, W - 34.f, 120.f, 22.f, col, Icon::Check, INK);
}

// ── Expressões (coleção) ────────────────────────────────────────────────────
void App::drawExpressions(ms_t now) {
  Expr e = (Expr)exprSel_;
  int uid;
  const Progress& Pr = P();
  bool locked = exprIsUnlockable(e, &uid) && !((Pr.unlocked >> uid) & 1u);
  if (locked) {
    canvas_.fillCircle(CX, 96.f, 46.f, SURF);
    drawIcon(canvas_, Icon::Lock, CX, 94.f, 42.f, DIM);
    canvas_.textCentered(172.f, "Bloqueada", FONT_L, MUTED);
    canvas_.textCentered(196.f, UNLOCK_CRITERIA[uid], FONT_S, acc(C_EXPR));
  } else {
    FaceXf xf; xf.s = 0.5f; xf.cy = 104.f;
    FaceRenderer::drawFace(canvas_, faceDef(e), faceStyle(), now, xf);
    canvas_.textCentered(196.f, exprName(e), FONT_L, TEXT);
    pill(canvas_, CX, 222.f, 96.f, 28.f, acc(C_EXPR), "Usar", FONT_M, INK);
    hit(B_EXPR_USE, CX, 222.f, 30.f);
  }
  int unlocked = 0; for (int i = 0; i < UNL_COUNT; i++) if ((Pr.unlocked >> i) & 1u) unlocked++;
  char b[24]; StrBuf s(b, sizeof b); s.put(tr("Coleção ")).num(unlocked).put('/').num(UNL_COUNT);
  canvas_.text(W - 14.f, 26.f, b, FONT_S, DIM, 255, Align::Right);
  button(B_EXPR_PREV, 26.f, CY, 16.f, SURF, Icon::Back, MUTED);
  button(B_EXPR_NEXT, W - 26.f, CY, 16.f, SURF, Icon::Play, MUTED);
  drawBackChevron();
}

// ── Respiração ──────────────────────────────────────────────────────────────
void App::drawBreath(ms_t now) {
  static const uint16_t PH[3][4] = {{4000, 4000, 4000, 4000}, {4000, 7000, 8000, 0}, {5000, 0, 5000, 0}};
  static const uint8_t CYC[3] = {4, 4, 6};
  static const char* const MODE[3] = {"Caixa 4-4-4-4", "Relaxar 4-7-8", "Coerente 5-5"};
  static const char* const NAME[4] = {"Inspire", "Segure", "Expire", "Segure"};
  float r = 40.f; const char* label = "Toque para começar"; int secs = -1; int prog = 0;
  if (breathOn_) {
    uint32_t cyc = 0; for (int k = 0; k < 4; k++) cyc += PH[breathMode_][k];
    ms_t el = now - breathStart_;
    uint32_t in = (uint32_t)(el % cyc); int ph = 0;
    while (ph < 3 && in >= PH[breathMode_][ph]) { in -= PH[breathMode_][ph]; ph++; }
    float p = PH[breathMode_][ph] ? (float)in / (float)PH[breathMode_][ph] : 1.f;
    float full = 86.f, empty = 40.f;
    if (ph == 0) r = empty + (full - empty) * easeInOut(p);
    else if (ph == 1) r = full;
    else if (ph == 2) r = full - (full - empty) * easeInOut(p);
    else r = empty;
    label = NAME[ph];
    secs = (int)((PH[breathMode_][ph] - in + 999) / 1000);
    prog = (int)(el * 1000 / ((ms_t)cyc * CYC[breathMode_]));
  }
  Color col = C_BREATH;
  progressRing(canvas_, CX, 128.f, 104.f, 4.f, prog, acc(col));
  canvas_.glow(CX, 128.f, r + 24.f, col, 90);
  canvas_.fillCircle(CX, 128.f, r, col, 210);
  if (secs >= 0) {
    canvas_.textCentered(126.f, label, FONT_L, 0x06221F);
    char b[8]; StrBuf s(b, sizeof b); s.num(secs); canvas_.textCentered(150.f, b, FONT_M, 0x06221F);
  } else {
    canvas_.textCentered(134.f, "Toque", FONT_M, 0x06221F);
    canvas_.textCentered(228.f, label, FONT_S, MUTED);
  }
  canvas_.text(W - 14.f, 26.f, MODE[breathMode_], FONT_S, breathOn_ ? DIM : acc(col), 255, Align::Right);
  if (!breathOn_) hitRect(B_BREATH_MODE, W - 150.f, 4.f, 146.f, 34.f);
  button(B_BREATH_PLAY, W - 28.f, 210.f, 17.f, SURF2, breathOn_ ? Icon::Pause : Icon::Play, TEXT);
  drawBackChevron();
}

// ── Nível (só com sensor de movimento) ──────────────────────────────────────
void App::drawLevel(ms_t now) {
  (void)now;
  char b[24];
  drawBackChevron("Nível", acc(C_LEVEL));
  if (!motion_.active()) {
    drawIcon(canvas_, Icon::Level, CX, 110.f, 60.f, DIM);
    canvas_.textCentered(176.f, "Sem sensor de movimento", FONT_M, MUTED);
    return;
  }
  float gx = (float)motion_.lpX(), gy = (float)motion_.lpY(), gz = (float)motion_.lpZ();
  float bx = fclamp(-gx / 1000.f, -1.f, 1.f) * 80.f, by = fclamp(gy / 1000.f, -1.f, 1.f) * 80.f;
  float tilt = fsqrt(gx * gx + gy * gy);
  float deg = fatan2(tilt, fabsf_(gz)) * 180.f / kPi;
  bool ok = deg < 1.f;
  canvas_.ring(CX, 130.f, 86.f, 2.f, SURF2);
  canvas_.ring(CX, 130.f, 22.f, 2.f, ok ? OK : DIM);
  canvas_.fillCircle(CX + bx, 130.f + by, 18.f, ok ? OK : C_LEVEL, 220);
  int d10 = iround(deg * 10.f);
  StrBuf s(b, sizeof b); s.num(d10 / 10).put(',').num(d10 % 10).put("°");
  canvas_.text(W - 16.f, 228.f, b, FONT_L, ok ? OK : TEXT, 255, Align::Right);
}

// ── Sorteio ─────────────────────────────────────────────────────────────────
void App::drawDice(ms_t now) {
  static const char* const MODE[4] = {"Dado", "Cara ou coroa", "Sim ou não", "Número 1–100"};
  bool rolling = rollUntil_ > now;
  int v = diceResult_;
  if (rolling) {
    uint32_t r = (uint32_t)(now / 60) * 2654435761u;
    v = diceMode_ == 0 ? (int)(r % 6) + 1 : diceMode_ == 3 ? (int)(r % 100) + 1 : (int)(r % 2);
  }
  float wob = rolling ? 0.25f * fsin((float)(now % 300) / 300.f * kTau) : 0.f;
  const float dy = 122.f;
  const Color ink = 0x2B1D00;
  char b[24];
  if (diceMode_ == 0 && (v || rolling)) {
    float s = 92.f;
    Pt sq[4];
    for (int i = 0; i < 4; i++) {
      float a = wob + (float)i * kPi * 0.5f + kPi * 0.25f;
      sq[i] = Pt{CX + s * 0.70f * fsin(a), dy - s * 0.70f * fcos(a)};
    }
    canvas_.fillPolygon(sq, 4, C_DICE);
    static const int8_t PIPS[7][7][2] = {
      {}, {{0, 0}}, {{-1, -1}, {1, 1}}, {{-1, -1}, {0, 0}, {1, 1}}, {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}},
      {{-1, -1}, {1, -1}, {0, 0}, {-1, 1}, {1, 1}}, {{-1, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {1, 1}}};
    int pv = tbClamp(v, 1, 6);
    for (int i = 0; i < pv; i++) {
      float px = (float)PIPS[pv][i][0] * 23.f, py = (float)PIPS[pv][i][1] * 23.f;
      float ca = fcos(wob), sa = fsin(wob);
      canvas_.fillCircle(CX + px * ca - py * sa, dy + px * sa + py * ca, 8.f, ink);
    }
  } else {
    const char* txt = "?";
    bool known = rolling || diceResult_;
    if (known) {
      if (diceMode_ == 1) txt = v ? "Cara" : "Coroa";
      else if (diceMode_ == 2) txt = v ? "Sim" : "Não";
      else if (diceMode_ == 3) { StrBuf s(b, sizeof b); s.num(v); txt = b; }
    }
    if (diceMode_ == 0) canvas_.fillRoundRect(CX - 46.f, dy - 46.f, 92.f, 92.f, 18.f, C_DICE);
    else canvas_.fillCircle(CX, dy, 64.f * (1.f - fabsf_(wob) * 0.6f), C_DICE);
    const Font& f = diceMode_ == 3 && known ? FONT_XXL : FONT_XL;
    canvas_.textCentered(dy + (f.size > 40 ? 20.f : 11.f), txt, f, ink);
  }
  canvas_.text(W - 14.f, 26.f, MODE[diceMode_], FONT_M, TEXT, 255, Align::Right);
  hitRect(B_DICE_MODE, 120.f, 2.f, W - 122.f, 36.f);
  canvas_.textCentered(222.f, caps_.imu ? "toque ou sacuda · título: modo" : "toque para sortear · título: modo", FONT_S, MUTED);
  drawBackChevron();
}

// ── Clima ───────────────────────────────────────────────────────────────────
void App::drawWeather(ms_t now) {
  char b[48];
  Color col = acc(C_WEATHER);
  drawBackChevron("Clima", col);
  if (!set_.weatherEnabled) {
    drawIcon(canvas_, Icon::PartCloud, CX, 96.f, 64.f, TEXT);
    canvas_.textCentered(164.f, "Clima desligado", FONT_L, TEXT);
    canvas_.textCentered(190.f, "ative no portal (precisa do Wi-Fi de casa)", FONT_S, MUTED);
    return;
  }
  if (!weather_.valid) {
    drawIcon(canvas_, Icon::Cloud, CX, 96.f, 64.f, DIM);
    NetStatus ns; plat_.netStatus(ns);
    canvas_.textCentered(160.f, ns.sta == StaState::Connected ? "Buscando previsão..." : "Sem rede de casa", FONT_M, TEXT);
    if (weatherFailed_) canvas_.textCentered(184.f, "falha ao consultar; tentando de novo", FONT_S, WARN);
    button(B_WEATHER_REFRESH, CX, 212.f, 17.f, SURF2, Icon::Reset, TEXT);
    return;
  }
  drawIcon(canvas_, weatherIcon(weather_.code, weather_.isDay), 70.f, 98.f, 72.f, TEXT);
  StrBuf t(b, sizeof b); t.num((weather_.tempC10 + (weather_.tempC10 >= 0 ? 5 : -5)) / 10).put("°");
  canvas_.text(128.f, 116.f, b, FONT_XXL, TEXT);
  canvas_.text(130.f, 142.f, weatherText(weather_.code), FONT_M, col);
  StrBuf m(b, sizeof b); m.put(tr("máx ")).num(weather_.hiC10 / 10).put(tr("°  mín ")).num(weather_.loC10 / 10).put(tr("°  umid. ")).num(weather_.humidity).put('%');
  canvas_.text(130.f, 162.f, b, FONT_S, MUTED);
  static const char* const DAY[3] = {"hoje", "amanhã", "depois"};
  for (int i = 1; i < weather_.nDays && i < 3; i++) {
    float x = 30.f + 144.f * (float)(i - 1);
    canvas_.fillRoundRect(x - 18.f, 178.f, 136.f, 34.f, 12.f, SURF);
    drawIcon(canvas_, weatherIcon(weather_.days[i].code, true), x, 195.f, 20.f, TEXT);
    StrBuf d(b, sizeof b); d.put(tr(DAY[i])).put(' ').num(weather_.days[i].hiC10 / 10).put("°/").num(weather_.days[i].loC10 / 10).put("°");
    canvas_.text(x + 16.f, 200.f, b, FONT_S, TEXT);
  }
  StrBuf c(b, sizeof b); c.put(tr(set_.weatherCity == 255 ? "local próprio" : CITY_TABLE[set_.weatherCity].name)).put(tr(" · há "));
  ms_t age = now - weather_.updatedAt; c.num((int64_t)(age / 60000)).put(tr(" min · Open-Meteo"));
  canvas_.textCentered(232.f, b, FONT_S, DIM);
}

// ── Meu Tuba ────────────────────────────────────────────────────────────────
void App::drawStats(ms_t now) {
  char b[48];
  const Progress& Pr = P();
  drawBackChevron();
  FaceXf xf; xf.s = 0.28f; xf.cx = 70.f; xf.cy = 96.f;
  FaceDef f = faceDef(face_.shown()); f.extras = FX_NONE;
  FaceRenderer::drawFace(canvas_, f, faceStyle(), now, xf);
  canvas_.text(70.f, 162.f, set_.nickname[0] ? set_.nickname : "Meu Tuba", FONT_M, TEXT, 255, Align::Center);
  auto bar = [&](float y, const char* label, int pm, Color col) {
    canvas_.text(140.f, y, label, FONT_S, MUTED);
    canvas_.fillRoundRect(208.f, y - 10.f, 96.f, 10.f, 5.f, SURF2);
    canvas_.fillRoundRect(208.f, y - 10.f, 10.f + 86.f * (float)tbClamp(pm, 0, 1000) / 1000.f, 10.f, 5.f, col);
  };
  bar(66.f, "Humor", (Pr.mood + 100) * 5, 0xFFD54A);
  bar(92.f, "Energia", energy_ * 10, OK);
  bar(118.f, "Vínculo", Pr.bond, C_STATS);
  int unl = 0; for (int i = 0; i < UNL_COUNT; i++) if ((Pr.unlocked >> i) & 1u) unl++;
  StrBuf s(b, sizeof b); s.unum(Pr.pets).put(tr(" carinhos · ")).unum(Pr.pomodorosDone).put(tr(" pomodoros"));
  canvas_.text(140.f, 156.f, b, FONT_S, TEXT);
  StrBuf s2(b, sizeof b); s2.unum(Pr.gamesPlayed).put(tr(" partidas"));
  canvas_.text(140.f, 180.f, b, FONT_S, TEXT);
  StrBuf s3(b, sizeof b); s3.num(unl).put('/').num(UNL_COUNT).put(tr(" expressões"));
  canvas_.text(140.f, 204.f, b, FONT_S, TEXT);
}

// ── Configuração / primeiro uso (com QR Code) ───────────────────────────────
void App::drawConfigPages(bool firstUse, ms_t now) {
  (void)now;
  NetStatus ns; plat_.netStatus(ns);
  char b[96];
  int page = configPage_ % 3;
  Color col = ACCENT;
  const float tx = 176.f;   // coluna de texto (o QR fica à esquerda)
  if (!firstUse) drawBackChevron();
  canvas_.text(firstUse ? 16.f : 46.f, 27.f, firstUse ? "Bem-vindo!" : "Rede e portal", FONT_L, col);
  if (page == 0) {
    StrBuf q(b, sizeof b); q.put("WIFI:T:WPA;S:").put(ns.apSsid).put(";P:").put(ns.apPass).put(";;");
    drawQr(b, 86.f, 132.f, 148.f);
    canvas_.text(tx, 72.f, "1. Aponte a câmera", FONT_S, MUTED);
    canvas_.text(tx, 90.f, "ou entre na rede:", FONT_S, MUTED);
    canvas_.text(tx, 116.f, ns.apSsid, FONT_M, TEXT);
    StrBuf p(b, sizeof b); p.put(tr("senha ")).put(ns.apPass);
    canvas_.text(tx, 140.f, b, FONT_S, TEXT);
  } else if (page == 1) {
    const char* ip = ns.apIp[0] ? ns.apIp : "192.168.4.1";
    StrBuf q(b, sizeof b); q.put("http://").put(ip);
    drawQr(b, 86.f, 132.f, 148.f);
    canvas_.text(tx, 72.f, "2. Abra no navegador", FONT_S, MUTED);
    canvas_.text(tx, 102.f, ip, FONT_L, TEXT);
    canvas_.text(tx, 126.f, "(funciona sem internet)", FONT_S, DIM);
  } else {
    drawIcon(canvas_, Icon::Wifi, 86.f, 110.f, 60.f, ns.apActive ? col : DIM);
    canvas_.text(tx, 72.f, ns.mdns, FONT_S, MUTED);
    StrBuf s(b, sizeof b); s.put(tr("Casa: "));
    if (!ns.staConfigured) s.put(tr("não usada"));
    else if (ns.sta == StaState::Connected) s.put(ns.staIp);
    else if (ns.sta == StaState::Connecting) s.put(tr("conectando"));
    else s.put(tr("sem conexão"));
    canvas_.text(tx, 100.f, b, FONT_M, TEXT);
    if (!ns.apActive) canvas_.text(tx, 124.f, "Rede própria fechada", FONT_S, WARN);
    else if (ns.apSecondsLeft) { StrBuf s2(b, sizeof b); s2.put(tr("fecha em ")); formatDuration(s2, (ms_t)ns.apSecondsLeft * 1000, false); canvas_.text(tx, 124.f, b, FONT_S, MUTED); }
  }
  pageDots(canvas_, 3, page, 228.f);
  button(B_PAGE_NEXT, W - 24.f, 228.f - 52.f, 15.f, SURF, Icon::Play, MUTED);
  if (firstUse) { pill(canvas_, 236.f, 200.f, 120.f, 32.f, SURF2, "Usar offline", FONT_S, TEXT); hitRect(B_FIRST_OFFLINE, 176.f, 182.f, 120.f, 36.f); }
  else { pill(canvas_, 236.f, 200.f, 120.f, 32.f, SURF2, "Fechar rede", FONT_S, TEXT); hitRect(B_CFG_CLOSE, 176.f, 182.f, 120.f, 36.f); }
}

// ── Informações ─────────────────────────────────────────────────────────────
void App::drawInfo(ms_t now) {
  char b[64];
  static const char* const TITLE[7] = {"Sobre", "Fábrica", "Rede", "Hora", "Memória", "Sensores", "Recordes"};
  drawBackChevron(TITLE[infoPage_ % 7], acc(C_INFO));
  float y = 70.f;
  auto line = [&](const char* s, Color c = 0) { canvas_.textCentered(y, s, FONT_S, c ? c : TEXT); y += 24.f; };
  const Progress& Pr = P();
  switch (infoPage_ % 7) {
    case 0: {
      StrBuf t(b, sizeof b); t.put(tr("TubaBot ")).put(modelName(fac_.model));
      canvas_.textCentered(y + 4.f, b, FONT_L, TEXT); y += 34.f;
      StrBuf s(b, sizeof b); s.put(tr("versão ")).put(TB_VERSION); line(b, MUTED);
      StrBuf u(b, sizeof b); u.put(tr("ligado há ")); formatDuration(u, now - bootAt_, false); line(b);
      line("ESP32-S3 · tela 2,8\" 320×240", MUTED);
      if (recovered_) line("dados restaurados ao padrão", WARN);
      pillButton(B_INFO_UPDATE, CX, 198.f, 210.f, 30.f, set_.updVersion[0] ? ACCENT : SURF2, "Buscar atualizações", set_.updVersion[0] ? ON_ACCENT : TEXT);
      break;
    }
    case 1: {
      if (!fac_.present) { line("perfil de fábrica não gravado", WARN); line("usando padrões genéricos (Lite)", MUTED); if (facWhy_) { StrBuf w(b, sizeof b); w.put(tr("motivo: ")).put(facWhy_); line(b, DIM); } break; }
      StrBuf m(b, sizeof b); m.put(tr("modelo: ")).put(modelName(fac_.model)); line(b);
      StrBuf c(b, sizeof b); c.put(tr("cor física: ")).put(tr(BODIES[fac_.body].name)); line(b);
      StrBuf e(b, sizeof b); e.put(tr("expressão padrão: ")).put(tr(EXPR_COLORS[fac_.exprColor].name)); line(b);
      StrBuf t(b, sizeof b); t.put(tr("tema padrão: ")).put(tr(fac_.theme ? "claro" : "escuro")); line(b);
      if (fac_.serial[0]) { StrBuf s(b, sizeof b); s.put(tr("série: ")).put(fac_.serial); line(b, MUTED); }
      if (facWhy_) { StrBuf w(b, sizeof b); w.put(tr("aviso: ")).put(facWhy_); line(b, WARN); }
      break;
    }
    case 2: {
      NetStatus ns; plat_.netStatus(ns);
      line(ns.apActive ? "rede própria: ligada" : "rede própria: desligada");
      StrBuf s(b, sizeof b); s.put(tr("casa: "));
      if (!ns.staConfigured) s.put(tr("não usada")); else if (ns.sta == StaState::Connected) s.put(ns.staIp); else s.put(tr("desconectada"));
      line(b);
      line(ns.mdns, MUTED);
      line("funciona offline", MUTED);
      break;
    }
    case 3:
      if (clock_.valid()) {
        CivilClock::Local L = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin);
        StrBuf s(b, sizeof b); s.numPad(L.day, 2).put('/').numPad(L.month, 2).put('/').num(L.year).put(' ').numPad(L.hour, 2).put(':').numPad(L.minute, 2);
        line(b);
        line(TZ_TABLE[set_.tzIndex].label, MUTED);
        line(clockFromRtc_ ? "lida do relógio da placa (RTC)" : "ajustada pela rede/portal", MUTED);
      } else { line("hora não ajustada"); line("ajuste pelo Wi-Fi ou portal", MUTED); }
      if (!caps_.rtc) line("sem RTC: a hora volta pela rede ao religar", DIM);
      break;
    case 4: {
      SysInfo si; plat_.sysInfo(si);
      StrBuf s(b, sizeof b); s.put(tr("RAM livre ")).unum(si.heapFree / 1024).put(tr(" KB")); line(b);
      StrBuf p(b, sizeof b); p.put(tr("PSRAM livre ")).unum(si.psramFree / 1024).put(tr(" KB")); line(b);
      StrBuf f(b, sizeof b); f.put(tr("flash livre ")).unum(si.sketchFree / 1024).put(tr(" KB")); line(b);
      line(si.simulated ? "simulador" : si.chip, MUTED);
      break;
    }
    case 5: {
      StrBuf s(b, sizeof b); s.put(tr("toque: ")).unum(input_.rawEdges()).put(tr(" bordas")).put(input_.stuck() ? " (preso!)" : ""); line(b);
      StrBuf a(b, sizeof b);
      if (!caps_.mic) a.put(tr("microfone: ausente"));
      else if (!set_.micReactions) a.put(tr("som: desligado"));
      else if (sound_.active()) a.put(tr("som: nível ")).num(sound_.level()).put(tr(" (base ")).num(sound_.baseline()).put(')');
      else a.put(tr("som: sem leitura"));
      line(b);
      if (caps_.battery) {
        StrBuf p(b, sizeof b);
        if (power_.known && power_.battery) p.put(tr("bateria: ")).num(power_.percent).put("% ").num(power_.mv).put(tr(" mV"));
        else p.put(tr("bateria: sem leitura"));
        line(b);
      } else line("bateria: não se aplica (Lite)", MUTED);
      if (caps_.imu) {
        StrBuf m(b, sizeof b);
        if (motion_.active()) m.put(tr("movimento: ")).num(motion_.lpX()).put(' ').num(motion_.lpY()).put(' ').num(motion_.lpZ()).put(tr(" mg"));
        else m.put(tr("movimento: sem leitura"));
        line(b);
      }
      break;
    }
    default: {
      StrBuf s(b, sizeof b); s.put(tr("Pula: ")).num(Pr.jumpBest).put(tr("   Bote: ")).num(Pr.fishBest).put(tr("   Mergulho: ")).num(Pr.diveBest); line(b);
      StrBuf r(b, sizeof b); r.put(tr("Reflexo: ")); if (Pr.reflexBestMs) r.num(Pr.reflexBestMs).put(tr(" ms")); else r.put("--"); line(b);
      StrBuf t(b, sizeof b); t.put(tr("Tempo Certo: ")); if (Pr.timingBestPct100) t.fixed2(Pr.timingBestPct100).put('%'); else t.put("--"); line(b);
      StrBuf c(b, sizeof b); c.put(tr("carinhos ")).unum(Pr.pets).put(tr(" · boops ")).unum(Pr.boops); line(b, MUTED);
      break;
    }
  }
  pageDots(canvas_, 7, infoPage_ % 7, 228.f);
  button(B_PAGE_NEXT, W - 24.f, CY, 15.f, SURF, Icon::Play, MUTED);
}

// ── diálogos ────────────────────────────────────────────────────────────────
void App::drawConfirm(ms_t now) {
  (void)now;
  Icon ic = Icon::Stopwatch; Color col = acc(C_SW);
  const char* title = "Zerar cronômetro?"; const char* sub = nullptr;
  switch (confirmAction_) {
    case C_CANCEL_POMO: ic = Icon::Tomato; col = acc(C_POMO); title = "Cancelar Pomodoro?"; break;
    case C_FACTORY_RESET: ic = Icon::Reset; col = BAD; title = "Restaurar padrão de fábrica?"; sub = "apaga ajustes, recordes e Wi-Fi"; break;
    case C_WIFI_FORGET: ic = Icon::Wifi; col = BAD; title = "Esquecer rede de casa?"; sub = "apelido e recordes ficam"; break;
    default: break;
  }
  canvas_.fillCircle(CX, 62.f, 28.f, col);
  drawIcon(canvas_, ic, CX, 62.f, 32.f, INK);
  const Font* fonts[2] = {&FONT_L, &FONT_M};
  canvas_.textCentered(122.f, title, Canvas::fitFont(title, 300, fonts, 2), TEXT);
  if (sub) canvas_.textCentered(146.f, sub, FONT_S, MUTED);
  button(B_CANCEL, CX - 64.f, 196.f, 24.f, SURF2, Icon::Cross, TEXT);
  button(B_OK, CX + 64.f, 196.f, 24.f, BAD, Icon::Check, 0xFFFFFF);
}

void App::pillButton(int id, float cx, float cy, float w, float h, Color bg, const char* label, Color fg) {
  bool pr = pressedHit_ == id;
  const char* t = tr(label);
  const Font* fonts[2] = {&FONT_M, &FONT_S};
  pill(canvas_, cx, cy, w, h, pr ? mixColor(bg, TEXT, 50) : bg, t, Canvas::fitFont(t, (int)w - 16, fonts, 2), fg);
  hitRect(id, cx - w * 0.5f, cy - h * 0.5f - 4.f, w, h + 8.f);
}

// ── atualizações ────────────────────────────────────────────────────────────
void App::drawUpdate(ms_t now) {
  char b[64];
  drawBackChevron(tr("Atualizações"), acc(C_SW));
  bool pend = set_.updVersion[0] != 0;
  canvas_.fillCircle(CX, 76.f, 24.f, pend ? ACCENT : SURF2);
  drawIcon(canvas_, Icon::Download, CX, 76.f + 2.f * fsin((float)(now % 1600) / 1600.f * kTau) * (pend ? 1.f : 0.f), 26.f, pend ? ON_ACCENT : TEXT);
  StrBuf v(b, sizeof b); v.put(tr("Versão instalada")).put(' ').put(TB_VERSION);
  canvas_.textCentered(126.f, b, FONT_M, TEXT);
  const char* st = nullptr; Color sc = MUTED;
  char sb[64]; StrBuf s(sb, sizeof sb);
  if (pend) {
    s.put(tr("Nova versão")).put(' ').put(set_.updVersion).put(' ').put(tr("disponível")); st = sb; sc = acc(C_SW);
  } else if (updChecking_ || updManual_) st = tr("Buscando...");
  else if (updResult_ == 1) { st = tr("Tudo em dia"); sc = acc(C_SW); }
  else if (!plat_.serviceUrl(Service::UpdateManifest)[0]) st = tr("Atualizações online em breve");
  else if (!staUp_) st = tr("Conecte o Wi-Fi de casa");
  if (st) canvas_.textCentered(150.f, st, FONT_S, sc);
  if (pend && set_.updAuto && set_.updDueMin && clock_.valid()) {
    int64_t lm = (int64_t)set_.updDueMin + TZ_TABLE[set_.tzIndex].offsetMin;
    char tb2[40]; StrBuf w(tb2, sizeof tb2); w.put(tr("agendada para")).put(' ').numPad((int)((lm / 60) % 24), 2).put(':').numPad((int)(lm % 60), 2);
    canvas_.textCentered(170.f, tb2, FONT_S, MUTED);
  } else if (pend && updNotes_[0]) {
    canvas_.textCentered(170.f, updNotes_, FONT_S, MUTED);
  }
  if (pend) {
    pillButton(B_UPD_NOW, CX - 72.f, 204.f, 132.f, 32.f, ACCENT, "Atualizar agora", ON_ACCENT);
    pillButton(B_UPD_NIGHT, CX + 72.f, 204.f, 132.f, 32.f, SURF2, "Esta noite", TEXT);
  } else {
    pillButton(B_UPD_CHECK, CX, 204.f, 210.f, 32.f, SURF2, "Buscar atualizações", TEXT);
  }
}

void App::drawUpdateOffer(ms_t now) {
  char b[64];
  float bob = 3.f * fsin((float)(now % 1800) / 1800.f * kTau);
  canvas_.fillCircle(CX, 56.f + bob, 30.f, ACCENT);
  canvas_.ring(CX, 56.f + bob, 36.f + 3.f * fsin((float)(now % 1200) / 1200.f * kTau), 2.f, ACCENT, 90);
  drawIcon(canvas_, Icon::Download, CX, 56.f + bob, 32.f, ON_ACCENT);
  const char* title = tr("Atualização disponível");
  const Font* fonts[2] = {&FONT_L, &FONT_M};
  canvas_.textCentered(118.f, title, Canvas::fitFont(title, 300, fonts, 2), TEXT);
  StrBuf v(b, sizeof b); v.put(tr("TubaBot ")).put(set_.updVersion);
  canvas_.textCentered(142.f, b, FONT_M, acc(C_SW));
  canvas_.textCentered(164.f, updNotes_[0] ? updNotes_ : tr("melhorias e novidades"), FONT_S, MUTED);
  bool later = set_.updSnoozes < 3;
  if (later) {
    pillButton(B_UPD_NOW, 58.f, 206.f, 96.f, 34.f, ACCENT, "Agora", ON_ACCENT);
    pillButton(B_UPD_LATER, CX, 206.f, 96.f, 34.f, SURF2, "Em 1 hora", TEXT);
    pillButton(B_UPD_NIGHT, W - 58.f, 206.f, 96.f, 34.f, SURF2, "Esta noite", TEXT);
  } else {
    pillButton(B_UPD_NOW, CX - 70.f, 206.f, 128.f, 34.f, ACCENT, "Agora", ON_ACCENT);
    pillButton(B_UPD_NIGHT, CX + 70.f, 206.f, 128.f, 34.f, SURF2, "Esta noite", TEXT);
  }
}

void App::drawRecovery(ms_t now) {
  drawIcon(canvas_, Icon::Wifi, CX, 58.f, 44.f, ACCENT);
  canvas_.textCentered(118.f, "Abrir configuração?", FONT_L, TEXT);
  canvas_.textCentered(142.f, "rede própria do Tuba", FONT_S, MUTED);
  int pm = (int)(recoveryUntil_ > now ? (recoveryUntil_ - now) * 1000 / 6000 : 0);
  canvas_.fillRoundRect(60.f, 160.f, 200.f, 4.f, 2.f, SURF2);
  canvas_.fillRoundRect(60.f, 160.f, 200.f * (float)pm / 1000.f, 4.f, 2.f, ACCENT);
  button(B_REC_NO, CX - 64.f, 204.f, 24.f, SURF2, Icon::Cross, TEXT);
  button(B_REC_YES, CX + 64.f, 204.f, 24.f, ACCENT, Icon::Check, ON_ACCENT);
}

void App::drawResume(ms_t now) {
  (void)now;
  char b[48];
  canvas_.textCentered(34.f, "Sessão interrompida", FONT_L, TEXT);
  float y = 72.f;
  if (ckpt_.pomoActive) {
    StrBuf s(b, sizeof b); s.put(tr(ckpt_.pomoPhase == 0 ? "Foco " : "Pausa ")); formatDuration(s, ckpt_.pomoRemainingMs, true);
    canvas_.textCentered(y, b, FONT_M, acc(C_POMO)); y += 26.f;
  }
  for (int i = 0; i < kTimers && y < 140.f; i++) if (ckpt_.timerActive[i]) {
    StrBuf s(b, sizeof b); s.put(tr("Timer ")).num(i + 1).put(' '); formatDuration(s, ckpt_.timerRemainingMs[i], true);
    canvas_.textCentered(y, b, FONT_M, acc(C_TIMER)); y += 26.f;
  }
  if (ckpt_.swActive && y < 150.f) { StrBuf s(b, sizeof b); s.put(tr("Cronômetro ")); formatDuration(s, ckpt_.swElapsedMs, false); canvas_.textCentered(y, b, FONT_M, acc(C_SW)); }
  canvas_.textCentered(160.f, "(o tempo desligado não é contado)", FONT_S, MUTED);
  button(B_RESUME_NO, CX - 64.f, 200.f, 22.f, SURF2, Icon::Cross, TEXT);
  button(B_RESUME_YES, CX + 64.f, 200.f, 22.f, OK, Icon::Play, INK);
}

void App::drawAlert(ms_t now) {
  const Alert& a = alerts_[0];
  canvas_.fillRect(0, 0, SCR_W, SCR_H, BG);
  Color col = C_TIMER; Icon ic = Icon::Timer;
  const char* head = "Aviso";
  char b[48]; StrBuf s(b, sizeof b);
  switch (a.type) {
    case AlertType::Timer: head = "Timer"; s.put(tr("Timer ")).num(a.index + 1).put(tr(" acabou!")); break;
    case AlertType::PomoFocusDone: head = "Pomodoro"; s.put(tr("Foco concluído!")); col = C_POMO; ic = Icon::Tomato; break;
    case AlertType::PomoBreakDone: head = "Pomodoro"; s.put(tr("Pausa acabou!")); col = C_SW; ic = Icon::Tomato; break;
    case AlertType::Reminder: head = "Lembrete"; s.put(set_.reminders[a.index].label); col = C_INFO; ic = Icon::Bell; break;
    case AlertType::Alarm: {
      head = set_.alarms[a.index].label; col = C_ALARM; ic = Icon::Alarm;
      fmtTime(s, set_.alarms[a.index].hour, set_.alarms[a.index].minute, set_.clock24h);
      break;
    }
  }
  float p = 0.5f + 0.5f * fsin((float)((now - a.at) % 1200) / 1200.f * kTau);
  canvas_.strokeRoundRect(3.f, 3.f, W - 6.f, H - 6.f, 18.f, 4.f + 3.f * p, col, 120 + (int)(110.f * p));
  canvas_.glow(76.f, 100.f, 70.f, col, (int)(70.f * p));
  float wig = a.type == AlertType::Alarm ? 0.18f * fsin((float)(now % 300) / 300.f * kTau) : 0.f;
  canvas_.fillCircle(76.f, 100.f, 40.f, col);
  drawIcon(canvas_, ic, 76.f + 22.f * wig, 100.f, 44.f, INK);
  canvas_.text(134.f, 76.f, head, FONT_M, acc(col));
  const Font* fonts[4] = {&FONT_XL, &FONT_L, &FONT_M, &FONT_S};
  canvas_.text(134.f, 122.f, b, Canvas::fitFont(b, 172, fonts, 4), TEXT);
  bool snooze = a.type == AlertType::Reminder || a.type == AlertType::Alarm;
  if (snooze) {
    pill(canvas_, 100.f, 194.f, 132.f, 40.f, SURF2, a.type == AlertType::Alarm ? "Adiar 5 min" : "Adiar 10", FONT_S, TEXT);
    hitRect(B_ALERT_SNOOZE, 34.f, 174.f, 132.f, 40.f);
    pill(canvas_, 234.f, 194.f, 116.f, 40.f, col, "Ok", FONT_M, INK);
    hitRect(B_ALERT_OK, 176.f, 174.f, 116.f, 40.f);
  } else {
    pill(canvas_, CX, 194.f, 150.f, 40.f, col, "Ok", FONT_M, INK);
    hitRect(B_ALERT_OK, CX - 75.f, 174.f, 150.f, 40.f);
  }
  if (alertN_ > 1) { char c[8]; StrBuf s2(c, sizeof c); s2.put('+').num(alertN_ - 1); canvas_.text(W - 14.f, 28.f, c, FONT_S, MUTED, 255, Align::Right); }
  else if (caps_.imu && set_.faceDownSnooze && snooze) canvas_.textCentered(232.f, "vire para baixo: adiar", FONT_S, DIM);
}

void App::drawHoldHint(ms_t now) {
  if (input_.mode() == TouchMode::Game || !input_.pressed()) return;
  if (input_.stuck()) {
    canvas_.fillRoundRect(40.f, 82.f, 240.f, 70.f, 22.f, g_theme.toastBg, 240);
    canvas_.textCentered(112.f, "Toque contínuo?", FONT_M, WARN);
    canvas_.textCentered(136.f, "tire o dedo / limpe a tela", FONT_S, 0xC9D1DE);
    return;
  }
  ms_t h = input_.holdMs(now);
  if (!input_.moved() && h >= 3500 && screen_ != Screen::RecoveryConfirm) {
    int pm = (int)tbMin<ms_t>(1000, (h - 3500) * 1000 / 4500);
    canvas_.fillRoundRect(50.f, 180.f, 220.f, 44.f, 22.f, g_theme.toastBg, 235);
    canvas_.fillRoundRect(64.f, 214.f, 192.f * (float)pm / 1000.f, 3.f, 1.5f, ACCENT);
    canvas_.textCentered(206.f, pm >= 1000 ? "solte para configurar" : "segure para configurar...", FONT_S, 0xF3F5F8);
  }
}

void App::drawToast(ms_t now) {
  if (toastUntil_ <= now || screen_ == Screen::ScreenOff) return;
  int w = Canvas::textWidth(toast_, FONT_M) + 36;
  w = tbMin(w, 300);
  float a = fclamp((float)(toastUntil_ - now) / 250.f, 0.f, 1.f);
  canvas_.fillRoundRect(CX - (float)w * 0.5f, 8.f, (float)w, 36.f, 18.f, g_theme.toastBg, (int)(245.f * a));
  canvas_.textCentered(32.f, toast_, FONT_M, 0xF3F5F8, (int)(255.f * a));
}

// ── composição ──────────────────────────────────────────────────────────────
void App::render(ms_t now) {
  if (diagUntil_ > now) {
    // padrão de teste: barras de cor, gradiente, moldura e marcação do topo/esquerda
    static const Color BARS[7] = {0xFFFFFF, 0xFFFF00, 0x00FFFF, 0x00FF00, 0xFF00FF, 0xFF0000, 0x0000FF};
    for (int i = 0; i < 7; i++) canvas_.fillRect(6 + i * 44, 34, 44, 100, BARS[i]);
    canvas_.vGradient(6, 142, 308, 40, 0xFFFFFF, 0x000000);
    canvas_.strokeRoundRect(1.f, 1.f, W - 2.f, H - 2.f, 4.f, 2.f, 0xFFFFFF);
    canvas_.fillCircle(12.f, 12.f, 6.f, 0xFF0000);
    canvas_.text(24.f, 24.f, "topo-esquerda", FONT_S, 0xFFFFFF);
    canvas_.textCentered(214.f, "TESTE 320×240", FONT_L, 0xFFFFFF);
    return;
  }
  ms_t since = now - screenAt_;
  if (since < 220 && screen_ != Screen::Game && screen_ != Screen::Intro && !alertN_) {
    float p = easeOut((float)since / 220.f);
    canvas_.setGlobalAlpha(60 + (int)(195.f * p));
    canvas_.setOrigin((float)transDir_ * -60.f * (1.f - p), 0.f);
  }
  switch (screen_) {
    case Screen::Intro: drawIntro(now); break;
    case Screen::FirstUse: drawConfigPages(true, now); break;
    case Screen::Config: drawConfigPages(false, now); break;
    case Screen::Face: drawFaceScreen(now); break;
    case Screen::Clock: drawClock(now); break;
    case Screen::Today: drawToday(now); break;
    case Screen::Launcher: drawGridScreen("Menu", now); break;
    case Screen::MoreApps: drawGridScreen("Mais apps", now); break;
    case Screen::Legends: {
      char b[40]; StrBuf s(b, sizeof b); s.put(tr("Lendárias")).put("  ").num(legendsSeen()).put('/').num(SCENE_COUNT);
      drawListScreen(b, 0xFFD54A, now);
      break;
    }
    case Screen::Language: drawListScreen("Idioma", TEXT, now); break;
    case Screen::Update: drawUpdate(now); break;
    case Screen::UpdateOffer: drawUpdateOffer(now); break;
    case Screen::Quick: drawQuick(now); break;
    case Screen::Pomodoro: drawPomodoro(now); break;
    case Screen::Timers: drawTimers(now); break;
    case Screen::TimerSet: { char b[24]; StrBuf s(b, sizeof b); s.put(tr("Duração do timer ")).num(timerSel_ + 1); drawListScreen(b, acc(C_TIMER), now); break; }
    case Screen::Stopwatch: drawStopwatch(now); break;
    case Screen::Alarms: drawAlarms(now); break;
    case Screen::AlarmEdit: drawAlarmEdit(now); break;
    case Screen::GamesMenu: drawListScreen("Jogos", acc(C_GAMES), now); break;
    case Screen::Game: if (game_) game_->draw(canvas_, now); break;
    case Screen::Expressions: drawExpressions(now); break;
    case Screen::Breath: drawBreath(now); break;
    case Screen::Level: drawLevel(now); break;
    case Screen::Dice: drawDice(now); break;
    case Screen::Weather: drawWeather(now); break;
    case Screen::Stats: drawStats(now); break;
    case Screen::Appearance: drawListScreen("Aparência", TEXT, now); break;
    case Screen::ColorPick: drawColorPick(now); break;
    case Screen::Overlays: drawListScreen("Informações na tela", TEXT, now); break;
    case Screen::Volume: drawLevelScreen(0, now); break;
    case Screen::Brightness: drawLevelScreen(1, now); break;
    case Screen::Wifi: drawListScreen("Wi-Fi", TEXT, now); break;
    case Screen::System: drawListScreen("Sistema", TEXT, now); break;
    case Screen::Info: drawInfo(now); break;
    case Screen::Torch: canvas_.fillRect(0, 0, SCR_W, SCR_H, 0xFFFFFF); canvas_.textCentered(230.f, "toque para sair", FONT_S, 0x8A8A8A); break;
    case Screen::Confirm: drawConfirm(now); break;
    case Screen::RecoveryConfirm: drawRecovery(now); break;
    case Screen::Resume: drawResume(now); break;
    case Screen::ScreenOff: break;  // luz de fundo desligada (timers seguem)
    case Screen::Ota:
      drawIcon(canvas_, Icon::Reset, CX, 70.f, 50.f, ACCENT);
      canvas_.textCentered(136.f, "Atualizando...", FONT_L, TEXT);
      canvas_.fillRoundRect(60.f, 156.f, 200.f, 8.f, 4.f, SURF2);
      canvas_.fillRoundRect(60.f, 156.f, 8.f + 192.f * (float)tbClamp(otaPermille_, 0, 1000) / 1000.f, 8.f, 4.f, ACCENT);
      canvas_.textCentered(194.f, "não desligue", FONT_S, MUTED);
      break;
    default: break;
  }
  canvas_.setOrigin(0, 0);
  canvas_.setGlobalAlpha(255);
  if (alertN_ && screen_ != Screen::ScreenOff) { hitN_ = 0; drawAlert(now); }
  else if (screen_ != Screen::ScreenOff && screen_ != Screen::Ota) drawHoldHint(now);
  if (!alertN_ && screen_ != Screen::ScreenOff && screen_ != Screen::Ota && screen_ != Screen::Game) drawMedal(now);
  drawToast(now);
}

}  // namespace tb
