// Desenho de todas as telas do TubaBot v2 (tela redonda 466×466).
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
  hit(id, cx, cy, r + 8.f);
}
void App::drawBackChevron() {
  canvas_.fillCircle(34.f, CY, 22.f, SURF);
  drawIcon(canvas_, Icon::Back, 32.f, CY, 22.f, MUTED);
  hit(B_BACK, 34.f, CY, 36.f);
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
  canvas_.fillRoundRect((float)x0 - 6.f, (float)y0 - 6.f, (float)total + 12.f, (float)total + 12.f, 14.f, 0xFFFFFF);
  for (int y = 0; y < qr_.size; y++)
    for (int x = 0; x < qr_.size; x++)
      if (qr_.module(x, y)) canvas_.fillRect(x0 + (x + quiet) * m, y0 + (y + quiet) * m, m, m, 0x000000);
}

// ── barra de status (topo) ──────────────────────────────────────────────────
void App::drawStatusBar(ms_t now, bool withTime) {
  char b[16];
  float y = 50.f;
  int w = 0;
  if (withTime && clock_.valid()) {
    CivilClock::Local L = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin);
    StrBuf s(b, sizeof b); fmtTime(s, L.hour, L.minute, set_.clock24h);
    w = Canvas::textWidth(b, FONT_S);
    canvas_.text(CX, y + 7.f, b, FONT_S, MUTED, 255, Align::Center);
  }
  float right = CX + (float)w * 0.5f + 14.f, left = CX - (float)w * 0.5f - 14.f;
  if (power_.known && power_.battery) { drawBattery(canvas_, right + 18.f, y, 30.f, power_.percent, power_.charging, MUTED); right += 44.f; }
  else if (power_.known && power_.charging) { drawIcon(canvas_, Icon::Bolt, right + 10.f, y, 18.f, MUTED); right += 26.f; }
  if (set_.silent) { drawIcon(canvas_, Icon::Silent, left - 10.f, y, 18.f, MUTED); left -= 26.f; }
  NetStatus ns; plat_.netStatus(ns);
  if (ns.sta == StaState::Connected) { drawIcon(canvas_, Icon::Wifi, left - 10.f, y, 18.f, MUTED); left -= 26.f; }
  int na = -1, mins = 0;
  na = nextAlarmIndex(now, &mins);
  if (na >= 0 && mins < 12 * 60) { drawIcon(canvas_, Icon::Alarm, left - 10.f, y, 18.f, MUTED); left -= 26.f; }
}

// ── rosto (página inicial) ──────────────────────────────────────────────────
void App::drawFaceScreen(ms_t now) {
  // anel de progresso discreto: Pomodoro (vermelho) e timers (âmbar)
  if (pomo_.active()) canvas_.arc(CX, CY, 226.f, 6.f, 0.f, kTau * (float)pomo_.permille(now) / 1000.f, pomo_.phase() == PomoPhase::Focus ? C_POMO : C_SW, 200);
  float r = pomo_.active() ? 216.f : 226.f;
  for (int i = 0; i < kTimers; i++)
    if (timers_[i].state() == RunState::Running || timers_[i].state() == RunState::Paused) {
      canvas_.arc(CX, CY, r, 5.f, 0.f, kTau * (float)timers_[i].permilleDone(now) / 1000.f, C_TIMER, timers_[i].state() == RunState::Paused ? 110 : 200);
      r -= 9.f;
    }
  face_.draw(canvas_, now, faceStyle());
  drawStatusBar(now, true);
  if (now - screenAt_ < 2500 && now - bootAt_ < 9000) hint(canvas_, "deslize: cima apps · baixo ajustes", 420.f);
}

// ── abertura ────────────────────────────────────────────────────────────────
void App::drawIntro(ms_t now) {
  int t = (int)(now - bootAt_);
  bool shortI = set_.shortIntro;
  int finEnd = shortI ? 350 : 1100, textEnd = shortI ? 600 : 1750;
  Color eye = EYE_COLORS[set_.eyeColor].rgb;
  // mar ao fundo
  float wavePh = (float)t / 600.f;
  for (int k = 0; k < 2; k++) {
    float yb = 300.f + 22.f * (float)k;
    Color col = k ? 0x0A3A6E : 0x1563A8;
    float px = 0.f, py = yb + 8.f * fsin(wavePh + (float)k);
    for (int x = 12; x <= 466; x += 12) {
      float y = yb + 8.f * fsin(wavePh + (float)k + (float)x * 0.025f);
      canvas_.line(px, py, (float)x, y, 6.f, col);
      px = (float)x; py = y;
    }
    canvas_.fillRect(0, (int)yb + 6, 466, 160, col, k ? 255 : 140);
  }
  if (t < finEnd) {
    float p = easeInOut((float)t / (float)finEnd);
    float x = -60.f + (CX + 40.f) * p;
    float bob = 4.f * fsin((float)t / 160.f);
    Pt fin[3] = {{x, 302.f + bob}, {x + 52.f, 302.f + bob}, {x + 38.f, 236.f + bob}};
    canvas_.fillPolygon(fin, 3, 0xDCE9F7);
    return;
  }
  if (t < textEnd) {
    float p = (float)(t - finEnd) / (float)(textEnd - finEnd);
    float sink = 60.f * easeInOut(p);
    Pt fin[3] = {{CX - 40.f, 302.f + sink}, {CX + 12.f, 302.f + sink}, {CX - 2.f, 236.f + sink}};
    canvas_.setClip(IRect(0, 0, 466, 300));
    canvas_.fillPolygon(fin, 3, 0xDCE9F7);
    canvas_.resetClip();
    for (int i = 0; i < 5; i++) canvas_.ring(CX - 30.f + 16.f * (float)i, 290.f - 90.f * p - 10.f * (float)(i % 2), 5.f + (float)(i % 3), 2.f, 0xFFFFFF, (int)(200.f * (1.f - p)));
    canvas_.textCentered(196.f, "TubaBot", FONT_XL, TEXT, (int)(255.f * easeOut(p)));
    return;
  }
  canvas_.textCentered(110.f, "TubaBot", FONT_L, TEXT);
  int pe = t - textEnd;
  int blink = pe < 260 ? 1000 - pe * 1000 / 260 : (pe < 360 ? (pe - 260) * 10 : (pe < 460 ? 1000 - (pe - 360) * 10 : 0));
  FaceDef f = faceDef(baseExpr_);
  f.extras = FX_NONE;
  FaceStyle st = faceStyle();
  FaceXf xf; xf.s = 0.8f; xf.cy = 200.f;
  FaceRenderer::drawFace(canvas_, f, st, now, xf, 0, 0, tbClamp(blink, 0, 1000), 0.f);
  if (pe > 500 || shortI) {
    char b[48]; StrBuf s(b, sizeof b);
    s.put("Oi");
    if (set_.nickname[0]) s.put(", ").put(set_.nickname);
    s.put('!');
    canvas_.textCentered(370.f, b, FONT_L, eye);
  }
}

// ── relógio (3 mostradores) ─────────────────────────────────────────────────
void App::drawClock(ms_t now) {
  char b[40];
  Color acc = EYE_COLORS[set_.eyeColor].rgb;
  float dx = 0, dy = 0;
  if (!clock_.valid()) {
    canvas_.textCentered(250.f, "--:--", FONT_XXL, DIM);
    canvas_.textCentered(310.f, "Hora não ajustada", FONT_M, MUTED);
    canvas_.textCentered(342.f, "use o portal ou o Wi-Fi", FONT_S, DIM);
    pageDots(canvas_, 3, 1);
    return;
  }
  CivilClock::Local L = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin);
  // anti-marcação: o mostrador passeia alguns pixels
  dx = (float)((int)((now / 120000) % 7) - 3) * 2.f; dy = (float)((int)((now / 180000) % 5) - 2) * 2.f;
  canvas_.setOrigin(canvas_.originX() + dx, canvas_.originY() + dy);
  int ms = (int)(now % 1000);
  if (set_.watchFace == 1) {
    // analógico
    for (int i = 0; i < 60; i++) {
      float a = (float)i * kTau / 60.f;
      bool hr = i % 5 == 0;
      float r0 = hr ? 196.f : 204.f, r1 = 214.f;
      canvas_.line(CX + r0 * fsin(a), CY - r0 * fcos(a), CX + r1 * fsin(a), CY - r1 * fcos(a), hr ? 5.f : 2.f, hr ? TEXT : DIM);
    }
    static const char* const NUM[4] = {"12", "3", "6", "9"};
    for (int i = 0; i < 4; i++) {
      float a = (float)i * kPi * 0.5f;
      canvas_.text(CX + 166.f * fsin(a), CY - 166.f * fcos(a) + 12.f, NUM[i], FONT_L, MUTED, 255, Align::Center);
    }
    StrBuf d(b, sizeof b); d.put(WEEKDAYS[L.weekday]).put(' ').num(L.day);
    canvas_.textCentered(CY + 92.f, b, FONT_S, acc);
    float sec = (float)L.second + (float)ms / 1000.f;
    float mn = (float)L.minute + sec / 60.f;
    float hr = (float)(L.hour % 12) + mn / 60.f;
    float ah = hr * kTau / 12.f, am = mn * kTau / 60.f, as = sec * kTau / 60.f;
    canvas_.line(CX, CY, CX + 110.f * fsin(ah), CY - 110.f * fcos(ah), 14.f, TEXT);
    canvas_.line(CX, CY, CX + 170.f * fsin(am), CY - 170.f * fcos(am), 9.f, TEXT);
    canvas_.line(CX - 30.f * fsin(as), CY + 30.f * fcos(as), CX + 188.f * fsin(as), CY - 188.f * fcos(as), 3.f, acc);
    canvas_.fillCircle(CX, CY, 10.f, acc);
    canvas_.fillCircle(CX, CY, 4.f, 0x000000);
  } else if (set_.watchFace == 2) {
    // "Tuba": rostinho em cima, hora embaixo
    FaceXf xf; xf.s = 0.48f; xf.cy = 150.f;
    FaceDef f = faceDef(face_.shown()); f.extras = (uint16_t)(f.extras & (FX_BLUSH | FX_ZZZ));
    FaceRenderer::drawFace(canvas_, f, faceStyle(), now, xf);
    StrBuf s(b, sizeof b); fmtTime(s, L.hour, L.minute, set_.clock24h);
    canvas_.textCentered(330.f, b, FONT_XXL, TEXT);
    StrBuf d(b, sizeof b); d.put(WEEKDAYS[L.weekday]).put(", ").num(L.day).put(' ').put(MONTHS[L.month - 1]);
    canvas_.textCentered(374.f, b, FONT_M, MUTED);
  } else {
    // digital
    StrBuf s(b, sizeof b); fmtTime(s, L.hour, L.minute, set_.clock24h);
    const Font* fonts[2] = {&FONT_CLOCK, &FONT_XXL};
    const Font& f = Canvas::fitFont(b, 400, fonts, 2);
    canvas_.textCentered(CY + 50.f, b, f, TEXT);
    if (!set_.clock24h) canvas_.textCentered(CY - 92.f, L.hour < 12 ? "AM" : "PM", FONT_M, acc);
    StrBuf d(b, sizeof b); d.put(WEEKDAYS_FULL[L.weekday]).put(", ").num(L.day).put(' ').put(MONTHS[L.month - 1]);
    canvas_.textCentered(CY + 104.f, b, FONT_M, MUTED);
    float a = ((float)L.second + (float)ms / 1000.f) * kTau / 60.f;
    canvas_.ring(CX, CY, 214.f, 2.f, SURF2);
    canvas_.fillCircle(CX + 214.f * fsin(a), CY - 214.f * fcos(a), 7.f, acc);
  }
  canvas_.setOrigin(canvas_.originX() - dx, canvas_.originY() - dy);
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
  StrBuf g(b, sizeof b); g.put(greet); if (set_.nickname[0]) g.put(", ").put(set_.nickname);
  const Font* fonts[2] = {&FONT_L, &FONT_M};
  canvas_.textCentered(108.f, b, Canvas::fitFont(b, 330, fonts, 2), TEXT);
  if (clock_.valid()) {
    CivilClock::Local L = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin);
    StrBuf d(b, sizeof b); d.put(WEEKDAYS_FULL[L.weekday]).put(", ").num(L.day).put(' ').put(MONTHS[L.month - 1]);
    canvas_.textCentered(142.f, b, FONT_S, MUTED);
  }
  float y = 186.f;
  auto row = [&](Icon ic, Color col, const char* text, const char* sub) {
    canvas_.fillRoundRect(70.f, y - 30.f, 326.f, 60.f, 30.f, SURF);
    canvas_.fillCircle(100.f, y, 22.f, col);
    drawIcon(canvas_, ic, 100.f, y, 26.f, 0x0C0F14);
    if (sub) { canvas_.text(132.f, y - 2.f, text, FONT_M, TEXT); canvas_.text(132.f, y + 20.f, sub, FONT_S, MUTED); }
    else canvas_.text(132.f, y + 9.f, text, FONT_M, TEXT);
    y += 70.f;
  };
  if (set_.weatherEnabled && weather_.valid) {
    char t[32]; StrBuf s(t, sizeof t); s.num((weather_.tempC10 + (weather_.tempC10 >= 0 ? 5 : -5)) / 10).put("° · ").put(weatherText(weather_.code));
    row(weatherIcon(weather_.code, weather_.isDay), C_WEATHER, t, set_.weatherCity == 255 ? "local personalizado" : CITY_TABLE[set_.weatherCity].name);
  }
  int mins = 0, na = nextAlarmIndex(now, &mins);
  if (na >= 0) {
    char t[32]; StrBuf s(t, sizeof t); fmtTime(s, set_.alarms[na].hour, set_.alarms[na].minute, set_.clock24h); s.put(" · ").put(set_.alarms[na].label);
    char u[32]; StrBuf su(u, sizeof u); su.put("em "); formatDuration(su, (ms_t)mins * 60000, false);
    row(Icon::Alarm, C_ALARM, t, u);
  }
  if (pomo_.active()) {
    char t[32]; StrBuf s(t, sizeof t); s.put(pomo_.phase() == PomoPhase::Focus ? "Foco " : "Pausa "); formatDuration(s, pomo_.remainingMs(now), true);
    row(Icon::Tomato, C_POMO, t, pomo_.state() == RunState::Paused ? "pausado" : nullptr);
  }
  for (int i = 0; i < kTimers && y < 380.f; i++) if (timers_[i].state() == RunState::Running || timers_[i].state() == RunState::Paused) {
    char t[32]; StrBuf s(t, sizeof t); s.put("Timer ").num(i + 1).put("  "); formatDuration(s, timers_[i].remainingMs(now), true);
    row(Icon::Timer, C_TIMER, t, timers_[i].state() == RunState::Paused ? "pausado" : nullptr);
  }
  if (y < 250.f) {
    char t[40]; StrBuf s(t, sizeof t); s.put("Humor ").num((P().mood + 100) / 2).put("% · vínculo ").num(P().bond / 10).put('%');
    row(Icon::Heart, C_STATS, "Tudo tranquilo por aqui", t);
  }
  if (power_.known && power_.battery && y < 400.f) {
    char t[24]; StrBuf s(t, sizeof t); s.put("Bateria ").num(power_.percent).put('%');
    canvas_.textCentered(398.f, t, FONT_S, power_.charging ? OK : MUTED);
  }
  if (now - screenAt_ < 2500) pageDots(canvas_, 3, 2);
}

// ── lançador e listas ───────────────────────────────────────────────────────
void App::drawLauncher(ms_t now) {
  list_.draw(canvas_, items_, itemN_, now, pressedItem_);
  canvas_.textCentered(40.f, "Apps", FONT_S, DIM);
  drawBackChevron();
}

void App::drawGamesMenu(ms_t now) {
  list_.draw(canvas_, items_, itemN_, now, pressedItem_);
  canvas_.textCentered(40.f, "Jogos", FONT_S, C_GAMES);
  drawBackChevron();
}

void App::drawSettings(ms_t now) {
  list_.draw(canvas_, items_, itemN_, now, pressedItem_);
  canvas_.textCentered(48.f, "Ajustes", FONT_S, DIM);
  drawBackChevron();
}

void App::drawTimerSet(ms_t now) {
  list_.draw(canvas_, items_, itemN_, now, pressedItem_);
  char b[24]; StrBuf s(b, sizeof b); s.put("Duração do timer ").num(timerSel_ + 1);
  canvas_.textCentered(40.f, b, FONT_S, C_TIMER);
  drawBackChevron();
}

// ── painel rápido ───────────────────────────────────────────────────────────
void App::drawQuick(ms_t now) {
  (void)now;
  char b[16];
  if (clock_.valid()) {
    CivilClock::Local L = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin);
    StrBuf s(b, sizeof b); fmtTime(s, L.hour, L.minute, set_.clock24h);
    canvas_.textCentered(84.f, b, FONT_L, TEXT);
  } else canvas_.textCentered(84.f, "Ajustes rápidos", FONT_M, TEXT);
  if (power_.known && power_.battery) drawBattery(canvas_, CX, 112.f, 34.f, power_.percent, power_.charging, MUTED);
  struct Q { int id; Icon ic; bool on; };
  Q q[4] = {{B_Q_SILENT, Icon::Silent, (bool)set_.silent}, {B_Q_MIC, Icon::Note, (bool)set_.micReactions},
            {B_Q_TORCH, Icon::Brightness, false}, {B_Q_NET, Icon::Wifi, false}};
  for (int i = 0; i < 4; i++) {
    float x = CX - 141.f + 94.f * (float)i;
    button(q[i].id, x, 180.f, 36.f, q[i].on ? C_FACE : SURF2, q[i].ic, q[i].on ? (Color)0x0C0F14 : TEXT);
  }
  slider(canvas_, 108.f, 270.f, 250.f, (set_.brightness - 10) * 1000 / 245, TEXT, Icon::Brightness);
  hitRect(B_Q_BRIGHT, 90.f, 240.f, 286.f, 60.f);
  slider(canvas_, 108.f, 340.f, 250.f, set_.volume * 10, caps_.speaker ? C_FACE : DIM, Icon::Speaker);
  hitRect(B_Q_VOL, 90.f, 310.f, 286.f, 60.f);
  hint(canvas_, "deslize para cima: fechar", 420.f);
}

// ── Pomodoro ────────────────────────────────────────────────────────────────
void App::drawPomodoro(ms_t now) {
  char b[32];
  bool focus = pomo_.phase() == PomoPhase::Focus;
  Color col = focus ? C_POMO : C_SW;
  progressRing(canvas_, 208.f, 14.f, pomo_.permille(now), col);
  appHeader(canvas_, Icon::Tomato, focus ? "Foco" : (pomo_.phase() == PomoPhase::ShortBreak ? "Pausa" : "Pausa longa"), col);
  StrBuf s(b, sizeof b); formatDuration(s, pomo_.remainingMs(now), true);
  canvas_.textCentered(CY + 26.f, b, FONT_XXL, pomo_.state() == RunState::Paused ? MUTED : TEXT);
  int cyc = set_.pomoCyclesBeforeLong ? pomo_.focusDone() % set_.pomoCyclesBeforeLong : 0;
  for (int i = 0; i < set_.pomoCyclesBeforeLong && i < 8; i++) {
    float x = CX - (float)(set_.pomoCyclesBeforeLong - 1) * 11.f + 22.f * (float)i;
    canvas_.fillCircle(x, CY + 66.f, 6.f, i < cyc ? C_POMO : SURF2);
  }
  button(B_RESET, CX - 110.f, 352.f, 34.f, SURF2, Icon::Cross, TEXT);
  button(B_PLAY, CX, 352.f, 46.f, col, pomo_.state() == RunState::Running ? Icon::Pause : Icon::Play, 0x0C0F14);
  button(B_SKIP, CX + 110.f, 352.f, 34.f, SURF2, Icon::Skip, TEXT);
  drawBackChevron();
}

// ── Timers ──────────────────────────────────────────────────────────────────
void App::drawTimers(ms_t now) {
  char b[32];
  Countdown& t = timers_[timerSel_];
  progressRing(canvas_, 208.f, 14.f, t.permilleDone(now), C_TIMER);
  for (int i = 0; i < kTimers; i++) {
    float x = CX - 64.f + 64.f * (float)i;
    bool sel = i == timerSel_;
    RunState st = timers_[i].state();
    Color bg = sel ? C_TIMER : SURF2;
    canvas_.fillCircle(x, 92.f, 24.f, bg);
    StrBuf s(b, sizeof b); s.num(i + 1);
    canvas_.text(x, 102.f, b, FONT_M, sel ? (Color)0x0C0F14 : TEXT, 255, Align::Center);
    if (st == RunState::Running) canvas_.fillCircle(x + 18.f, 74.f, 6.f, OK);
    hit(B_TIMER_SEL0 + i, x, 92.f, 30.f);
  }
  StrBuf s(b, sizeof b); formatDuration(s, t.remainingMs(now), true);
  const Font* fonts[2] = {&FONT_XXL, &FONT_XL};
  canvas_.textCentered(CY + 30.f, b, Canvas::fitFont(b, 330, fonts, 2), t.state() == RunState::Done ? C_TIMER : TEXT);
  if (t.state() == RunState::Paused) canvas_.textCentered(CY + 70.f, "pausado", FONT_S, MUTED);
  button(B_TIMER_DUR, CX - 110.f, 352.f, 34.f, SURF2, Icon::Timer, TEXT);
  button(B_PLAY, CX, 352.f, 46.f, C_TIMER, t.state() == RunState::Running ? Icon::Pause : Icon::Play, 0x0C0F14);
  if (t.state() == RunState::Running || t.state() == RunState::Paused) button(B_TIMER_CANCEL, CX + 110.f, 352.f, 34.f, SURF2, Icon::Cross, TEXT);
  else button(B_TIMER_REPEAT, CX + 110.f, 352.f, 34.f, SURF2, Icon::Reset, TEXT);
  drawBackChevron();
}

// ── Cronômetro ──────────────────────────────────────────────────────────────
void App::drawStopwatch(ms_t now) {
  char b[32];
  uint64_t e = sw_.elapsedMs(now);
  float a = (float)(e % 60000) / 60000.f;
  canvas_.ring(CX, CY, 208.f, 4.f, SURF2);
  if (e) canvas_.fillCircle(CX + 208.f * fsin(a * kTau), CY - 208.f * fcos(a * kTau), 9.f, C_SW);
  appHeader(canvas_, Icon::Stopwatch, "Cronômetro", C_SW);
  uint64_t hh = e / 3600000, mm = (e / 60000) % 60, ss = (e / 1000) % 60, cs = (e / 10) % 100;
  StrBuf s(b, sizeof b);
  if (hh) s.unum(hh).put(':');
  s.numPad((int64_t)mm, 2).put(':').numPad((int64_t)ss, 2);
  canvas_.textCentered(CY + 10.f, b, hh ? FONT_XL : FONT_XXL, TEXT);
  StrBuf c(b, sizeof b); c.put(',').numPad((int64_t)cs, 2);
  canvas_.textCentered(CY + 54.f, b, FONT_L, C_SW);
  if (sw_.limitHit()) canvas_.textCentered(CY + 88.f, "Limite atingido", FONT_S, WARN);
  else if (lapN_) {
    StrBuf l(b, sizeof b); l.put("volta ").num(lapN_).put("  "); formatDuration(l, laps_[lapN_ - 1], false);
    canvas_.textCentered(CY + 88.f, b, FONT_S, MUTED);
  }
  button(B_RESET, CX - 110.f, 352.f, 34.f, SURF2, Icon::Reset, TEXT);
  button(B_PLAY, CX, 352.f, 46.f, C_SW, sw_.state() == RunState::Running ? Icon::Pause : Icon::Play, 0x0C0F14);
  button(B_LAP, CX + 110.f, 352.f, 34.f, sw_.state() == RunState::Running ? SURF2 : SURF, Icon::Plus, sw_.state() == RunState::Running ? TEXT : DIM);
  drawBackChevron();
}

// ── Alarmes ─────────────────────────────────────────────────────────────────
void App::drawAlarms(ms_t now) {
  (void)now;
  char b[40];
  appHeader(canvas_, Icon::Alarm, "Alarmes", C_ALARM);
  for (int i = 0; i < ALARM_COUNT; i++) {
    const Alarm& a = set_.alarms[i];
    float y = 150.f + 82.f * (float)i;
    canvas_.fillRoundRect(64.f, y - 34.f, 338.f, 68.f, 34.f, SURF);
    StrBuf s(b, sizeof b); fmtTime(s, a.hour, a.minute, true);
    canvas_.text(92.f, y + 8.f, b, FONT_L, a.enabled ? TEXT : DIM);
    StrBuf d(b, sizeof b);
    if (!a.days) d.put("uma vez");
    else if (a.days == 0x7F) d.put("todo dia");
    else if (a.days == 0x3E) d.put("seg a sex");
    else if (a.days == 0x41) d.put("fim de semana");
    else for (int k = 0; k < 7; k++) if ((a.days >> k) & 1) d.put(WEEKDAYS[k]).put(' ');
    canvas_.text(196.f, y - 2.f, a.label, FONT_S, a.enabled ? TEXT : DIM);
    canvas_.text(196.f, y + 20.f, b, FONT_S, MUTED);
    hitRect(B_ALARM_OPEN0 + i, 64.f, y - 34.f, 270.f, 68.f);
    toggle(canvas_, 362.f, y, a.enabled, C_ALARM);
    hit(B_ALARM_TOGGLE0 + i, 362.f, y, 34.f);
  }
  hint(canvas_, clock_.valid() ? "toque no alarme para editar" : "ajuste a hora pelo portal", 410.f);
  drawBackChevron();
}

void App::drawAlarmEdit(ms_t now) {
  (void)now;
  char b[16];
  Alarm& a = set_.alarms[editAlarm_];
  canvas_.textCentered(84.f, a.label, FONT_M, C_ALARM);
  StrBuf h(b, sizeof b); h.numPad(a.hour, 2);
  canvas_.text(CX - 70.f, 232.f, b, FONT_XXL, TEXT, 255, Align::Center);
  canvas_.text(CX, 226.f, ":", FONT_XXL, MUTED, 255, Align::Center);
  StrBuf m(b, sizeof b); m.numPad(a.minute, 2);
  canvas_.text(CX + 70.f, 232.f, b, FONT_XXL, TEXT, 255, Align::Center);
  button(B_AL_H_PLUS, CX - 70.f, 118.f, 24.f, SURF2, Icon::Plus, TEXT);
  button(B_AL_H_MINUS, CX - 70.f, 272.f, 24.f, SURF2, Icon::Minus, TEXT);
  button(B_AL_M_PLUS, CX + 70.f, 118.f, 24.f, SURF2, Icon::Plus, TEXT);
  button(B_AL_M_MINUS, CX + 70.f, 272.f, 24.f, SURF2, Icon::Minus, TEXT);
  static const char* const D[7] = {"D", "S", "T", "Q", "Q", "S", "S"};
  for (int k = 0; k < 7; k++) {
    float x = CX - 147.f + 49.f * (float)k;
    bool on = (a.days >> k) & 1;
    canvas_.fillCircle(x, 324.f, 20.f, on ? C_ALARM : SURF2);
    canvas_.text(x, 332.f, D[k], FONT_S, on ? (Color)0x0C0F14 : MUTED, 255, Align::Center);
    hit(B_AL_DAY0 + k, x, 324.f, 24.f);
  }
  pill(canvas_, CX, 392.f, 170.f, 52.f, C_ALARM, "Salvar", FONT_M, 0x0C0F14);
  hit(B_AL_DONE, CX, 392.f, 60.f);
  drawBackChevron();
}

// ── Expressões (coleção) ────────────────────────────────────────────────────
void App::drawExpressions(ms_t now) {
  Expr e = (Expr)exprSel_;
  int uid;
  const Progress& Pr = P();
  bool locked = exprIsUnlockable(e, &uid) && !((Pr.unlocked >> uid) & 1u);
  if (locked) {
    canvas_.fillCircle(CX, 200.f, 96.f, SURF);
    drawIcon(canvas_, Icon::Lock, CX, 196.f, 80.f, DIM);
    canvas_.textCentered(340.f, "Bloqueada", FONT_L, MUTED);
    canvas_.textCentered(374.f, UNLOCK_CRITERIA[uid], FONT_S, C_EXPR);
  } else {
    FaceXf xf; xf.s = 0.74f; xf.cy = 206.f;
    FaceRenderer::drawFace(canvas_, faceDef(e), faceStyle(), now, xf);
    canvas_.textCentered(352.f, exprName(e), FONT_L, TEXT);
    pill(canvas_, CX, 398.f, 150.f, 48.f, C_EXPR, "Usar", FONT_M, 0x0C0F14);
    hit(B_EXPR_USE, CX, 398.f, 50.f);
  }
  int unlocked = 0; for (int i = 0; i < UNL_COUNT; i++) if ((Pr.unlocked >> i) & 1u) unlocked++;
  char b[24]; StrBuf s(b, sizeof b); s.put("Coleção ").num(unlocked).put('/').num(UNL_COUNT);
  canvas_.textCentered(60.f, b, FONT_S, DIM);
  button(B_EXPR_PREV, 44.f, CY - 20.f, 26.f, SURF, Icon::Back, MUTED);
  button(B_EXPR_NEXT, 422.f, CY - 20.f, 26.f, SURF, Icon::Play, MUTED);
}

// ── Respiração ──────────────────────────────────────────────────────────────
void App::drawBreath(ms_t now) {
  static const uint16_t PH[3][4] = {{4000, 4000, 4000, 4000}, {4000, 7000, 8000, 0}, {5000, 0, 5000, 0}};
  static const uint8_t CYC[3] = {4, 4, 6};
  static const char* const MODE[3] = {"Caixa 4-4-4-4", "Relaxar 4-7-8", "Coerente 5-5"};
  static const char* const NAME[4] = {"Inspire", "Segure", "Expire", "Segure"};
  float r = 70.f; const char* label = "Toque para começar"; int secs = -1; int prog = 0;
  if (breathOn_) {
    uint32_t cyc = 0; for (int k = 0; k < 4; k++) cyc += PH[breathMode_][k];
    ms_t el = now - breathStart_;
    uint32_t in = (uint32_t)(el % cyc); int ph = 0;
    while (ph < 3 && in >= PH[breathMode_][ph]) { in -= PH[breathMode_][ph]; ph++; }
    float p = PH[breathMode_][ph] ? (float)in / (float)PH[breathMode_][ph] : 1.f;
    float full = 168.f, empty = 74.f;
    if (ph == 0) r = empty + (full - empty) * easeInOut(p);
    else if (ph == 1) r = full;
    else if (ph == 2) r = full - (full - empty) * easeInOut(p);
    else r = empty;
    label = NAME[ph];
    secs = (int)((PH[breathMode_][ph] - in + 999) / 1000);
    prog = (int)(el * 1000 / ((ms_t)cyc * CYC[breathMode_]));
  }
  progressRing(canvas_, 218.f, 6.f, prog, C_BREATH);
  canvas_.glow(CX, CY, r + 40.f, C_BREATH, 90);
  canvas_.fillCircle(CX, CY, r, C_BREATH, 200);
  canvas_.textCentered(CY + (secs >= 0 ? -6.f : 10.f), label, secs >= 0 ? FONT_L : FONT_M, 0x06221F);
  if (secs >= 0) { char b[8]; StrBuf s(b, sizeof b); s.num(secs); canvas_.textCentered(CY + 36.f, b, FONT_M, 0x06221F); }
  canvas_.textCentered(70.f, MODE[breathMode_], FONT_S, breathOn_ ? DIM : C_BREATH);
  if (!breathOn_) hitRect(B_BREATH_MODE, CX - 120.f, 40.f, 240.f, 44.f);
  button(B_BREATH_PLAY, CX, 400.f, 30.f, SURF2, breathOn_ ? Icon::Pause : Icon::Play, TEXT);
  drawBackChevron();
}

// ── Nível (bolha) ───────────────────────────────────────────────────────────
void App::drawLevel(ms_t now) {
  (void)now;
  char b[24];
  if (!motion_.active()) {
    drawIcon(canvas_, Icon::Level, CX, 190.f, 90.f, DIM);
    canvas_.textCentered(290.f, "Sem leitura do sensor", FONT_M, MUTED);
    drawBackChevron();
    return;
  }
  float gx = (float)motion_.lpX(), gy = (float)motion_.lpY(), gz = (float)motion_.lpZ();
  bool flat = fabsf_(gz) > fabsf_(gy);
  float deg;
  if (flat) {
    // deitado: bolha 2D
    float bx = fclamp(-gx / 1000.f, -1.f, 1.f) * 150.f, by = fclamp(gy / 1000.f, -1.f, 1.f) * 150.f;
    float tilt = fsqrt(gx * gx + gy * gy);
    deg = fatan2(tilt, fabsf_(gz)) * 180.f / kPi;
    bool ok = deg < 1.f;
    canvas_.ring(CX, CY, 160.f, 3.f, SURF2);
    canvas_.ring(CX, CY, 40.f, 3.f, ok ? OK : DIM);
    canvas_.line(CX - 170.f, CY, CX + 170.f, CY, 2.f, SURF2);
    canvas_.line(CX, CY - 170.f, CX, CY + 170.f, 2.f, SURF2);
    canvas_.fillCircle(CX + bx, CY + by, 32.f, ok ? OK : C_LEVEL, 220);
  } else {
    // em pé: linha do horizonte
    float a = fatan2(gx, gy);
    deg = fabsf_(a) * 180.f / kPi;
    bool ok = deg < 1.f;
    float ca = fcos(a), sa = fsin(a);
    canvas_.line(CX - 190.f * ca, CY + 190.f * sa, CX + 190.f * ca, CY - 190.f * sa, 6.f, ok ? OK : C_LEVEL);
    canvas_.ring(CX, CY, 12.f, 3.f, TEXT);
  }
  int d10 = iround(deg * 10.f);
  StrBuf s(b, sizeof b); s.num(d10 / 10).put(',').num(d10 % 10).put("°");
  canvas_.textCentered(392.f, b, FONT_XL, deg < 1.f ? OK : TEXT);
  canvas_.textCentered(84.f, flat ? "Nível (deitado)" : "Nível (em pé)", FONT_S, DIM);
  drawBackChevron();
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
  char b[24];
  if (diceMode_ == 0 && (v || rolling)) {
    float s = 180.f;
    Pt sq[4];
    for (int i = 0; i < 4; i++) {
      float a = wob + (float)i * kPi * 0.5f + kPi * 0.25f;
      sq[i] = Pt{CX + s * 0.70f * fsin(a), CY - 10.f - s * 0.70f * fcos(a)};
    }
    canvas_.fillPolygon(sq, 4, C_DICE);
    static const int8_t PIPS[7][7][2] = {
      {}, {{0, 0}}, {{-1, -1}, {1, 1}}, {{-1, -1}, {0, 0}, {1, 1}}, {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}},
      {{-1, -1}, {1, -1}, {0, 0}, {-1, 1}, {1, 1}}, {{-1, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {1, 1}}};
    int pv = tbClamp(v, 1, 6);
    for (int i = 0; i < pv; i++) {
      float px = (float)PIPS[pv][i][0] * 44.f, py = (float)PIPS[pv][i][1] * 44.f;
      float ca = fcos(wob), sa = fsin(wob);
      canvas_.fillCircle(CX + px * ca - py * sa, CY - 10.f + px * sa + py * ca, 15.f, 0x2B1D00);
    }
  } else if (v || rolling || diceMode_ != 0) {
    const char* txt = nullptr;
    if (diceMode_ == 1) txt = v ? "Cara" : "Coroa";
    else if (diceMode_ == 2) txt = v ? "Sim" : "Não";
    else { StrBuf s(b, sizeof b); s.num(v); txt = b; }
    bool shown = rolling || diceResult_ || diceMode_ == 3 ? true : false;
    if (diceMode_ != 3 && !rolling && !diceResult_ && diceMode_ != 0) shown = false;
    canvas_.fillCircle(CX, CY - 10.f, 130.f * (1.f - fabsf_(wob) * 0.6f), C_DICE);
    if (shown && (rolling || diceResult_ || diceMode_ == 3)) canvas_.textCentered(CY + 8.f, (diceMode_ == 3 && !diceResult_ && !rolling) ? "?" : txt, diceMode_ == 3 ? FONT_XXL : FONT_XL, 0x2B1D00);
    else canvas_.textCentered(CY + 8.f, "?", FONT_XXL, 0x2B1D00);
  } else {
    canvas_.fillRoundRect(CX - 90.f, CY - 100.f, 180.f, 180.f, 30.f, C_DICE);
    canvas_.textCentered(CY + 18.f, "?", FONT_XXL, 0x2B1D00);
  }
  canvas_.textCentered(76.f, MODE[diceMode_], FONT_M, TEXT);
  hitRect(B_DICE_MODE, CX - 140.f, 44.f, 280.f, 48.f);
  canvas_.textCentered(400.f, caps_.imu ? "toque ou sacuda" : "toque para sortear", FONT_S, MUTED);
  canvas_.textCentered(426.f, "toque no título: modo", FONT_S, DIM);
  drawBackChevron();
}

// ── Clima ───────────────────────────────────────────────────────────────────
void App::drawWeather(ms_t now) {
  char b[40];
  if (!set_.weatherEnabled) {
    drawIcon(canvas_, Icon::PartCloud, CX, 170.f, 110.f, TEXT);
    canvas_.textCentered(282.f, "Clima desligado", FONT_L, TEXT);
    canvas_.textCentered(318.f, "ative no portal (precisa", FONT_S, MUTED);
    canvas_.textCentered(342.f, "da rede Wi-Fi de casa)", FONT_S, MUTED);
    drawBackChevron();
    return;
  }
  if (!weather_.valid) {
    drawIcon(canvas_, Icon::Cloud, CX, 170.f, 110.f, DIM);
    NetStatus ns; plat_.netStatus(ns);
    canvas_.textCentered(282.f, ns.sta == StaState::Connected ? "Buscando previsão..." : "Sem rede de casa", FONT_M, TEXT);
    if (weatherFailed_) canvas_.textCentered(316.f, "falha ao consultar; tentando de novo", FONT_S, WARN);
    button(B_WEATHER_REFRESH, CX, 380.f, 30.f, SURF2, Icon::Reset, TEXT);
    drawBackChevron();
    return;
  }
  drawIcon(canvas_, weatherIcon(weather_.code, weather_.isDay), CX, 118.f, 96.f, TEXT);
  StrBuf t(b, sizeof b); t.num((weather_.tempC10 + (weather_.tempC10 >= 0 ? 5 : -5)) / 10).put("°");
  canvas_.textCentered(258.f, b, FONT_XXL, TEXT);
  canvas_.textCentered(294.f, weatherText(weather_.code), FONT_M, C_WEATHER);
  StrBuf m(b, sizeof b); m.put("máx ").num(weather_.hiC10 / 10).put("°  mín ").num(weather_.loC10 / 10).put("°  umidade ").num(weather_.humidity).put('%');
  canvas_.textCentered(324.f, b, FONT_S, MUTED);
  for (int i = 1; i < weather_.nDays && i < 3; i++) {
    float x = CX - 70.f + 140.f * (float)(i - 1);
    drawIcon(canvas_, weatherIcon(weather_.days[i].code, true), x - 34.f, 368.f, 34.f, TEXT);
    StrBuf d(b, sizeof b); d.num(weather_.days[i].hiC10 / 10).put("°/").num(weather_.days[i].loC10 / 10).put("°");
    canvas_.text(x - 12.f, 376.f, b, FONT_S, TEXT);
  }
  StrBuf c(b, sizeof b); c.put(set_.weatherCity == 255 ? "local próprio" : CITY_TABLE[set_.weatherCity].name).put(" · ");
  ms_t age = now - weather_.updatedAt; c.put("há ").num((int64_t)(age / 60000)).put(" min");
  canvas_.textCentered(412.f, b, FONT_S, DIM);
  canvas_.textCentered(436.f, "dados: Open-Meteo.com", FONT_S, DIM);
  drawBackChevron();
}

// ── Meu Tuba ────────────────────────────────────────────────────────────────
void App::drawStats(ms_t now) {
  char b[48];
  const Progress& Pr = P();
  FaceXf xf; xf.s = 0.36f; xf.cy = 112.f;
  FaceDef f = faceDef(face_.shown()); f.extras = FX_NONE;
  FaceRenderer::drawFace(canvas_, f, faceStyle(), now, xf);
  canvas_.textCentered(196.f, set_.nickname[0] ? set_.nickname : "Meu Tuba", FONT_L, TEXT);
  auto bar = [&](float y, const char* label, int pm, Color col) {
    canvas_.text(96.f, y, label, FONT_S, MUTED);
    canvas_.fillRoundRect(206.f, y - 14.f, 170.f, 14.f, 7.f, SURF2);
    canvas_.fillRoundRect(206.f, y - 14.f, 14.f + 156.f * (float)tbClamp(pm, 0, 1000) / 1000.f, 14.f, 7.f, col);
  };
  bar(236.f, "Humor", (Pr.mood + 100) * 5, 0xFFD54A);
  bar(266.f, "Energia", energy_ * 10, OK);
  bar(296.f, "Vínculo", Pr.bond, C_STATS);
  int unl = 0; for (int i = 0; i < UNL_COUNT; i++) if ((Pr.unlocked >> i) & 1u) unl++;
  StrBuf s(b, sizeof b); s.unum(Pr.pets).put(" carinhos · ").unum(Pr.pomodorosDone).put(" pomodoros");
  canvas_.textCentered(340.f, b, FONT_S, TEXT);
  StrBuf s2(b, sizeof b); s2.unum(Pr.gamesPlayed).put(" partidas · ").num(unl).put('/').num(UNL_COUNT).put(" expressões");
  canvas_.textCentered(366.f, b, FONT_S, TEXT);
  drawBackChevron();
}

// ── Configuração / primeiro uso (com QR Code) ───────────────────────────────
void App::drawConfigPages(bool firstUse, ms_t now) {
  (void)now;
  NetStatus ns; plat_.netStatus(ns);
  char b[96];
  int page = configPage_ % 3;
  canvas_.textCentered(58.f, firstUse ? "Bem-vindo!" : "Rede e portal", FONT_M, C_FACE);
  if (page == 0) {
    StrBuf q(b, sizeof b); q.put("WIFI:T:WPA;S:").put(ns.apSsid).put(";P:").put(ns.apPass).put(";;");
    drawQr(b, CX, 196.f, 196.f);
    canvas_.textCentered(328.f, "1. Aponte a câmera ou entre em", FONT_S, MUTED);
    canvas_.textCentered(358.f, ns.apSsid, FONT_M, TEXT);
    StrBuf p(b, sizeof b); p.put("senha ").put(ns.apPass);
    canvas_.textCentered(388.f, b, FONT_S, TEXT);
  } else if (page == 1) {
    const char* ip = ns.apIp[0] ? ns.apIp : "192.168.4.1";
    StrBuf q(b, sizeof b); q.put("http://").put(ip);
    drawQr(b, CX, 196.f, 196.f);
    canvas_.textCentered(328.f, "2. Abra no navegador", FONT_S, MUTED);
    canvas_.textCentered(362.f, ip, FONT_L, TEXT);
    canvas_.textCentered(392.f, "(funciona sem internet)", FONT_S, DIM);
  } else {
    drawIcon(canvas_, Icon::Wifi, CX, 140.f, 70.f, ns.apActive ? C_FACE : DIM);
    canvas_.textCentered(212.f, ns.mdns, FONT_S, MUTED);
    StrBuf s(b, sizeof b); s.put("Casa: ");
    if (!ns.staConfigured) s.put("não usada");
    else if (ns.sta == StaState::Connected) s.put(ns.staIp);
    else if (ns.sta == StaState::Connecting) s.put("conectando");
    else s.put("sem conexão");
    canvas_.textCentered(250.f, b, FONT_M, TEXT);
    if (!ns.apActive) canvas_.textCentered(284.f, "Rede própria fechada", FONT_S, WARN);
    else if (ns.apSecondsLeft) { StrBuf s2(b, sizeof b); s2.put("fecha em "); formatDuration(s2, (ms_t)ns.apSecondsLeft * 1000, false); canvas_.textCentered(284.f, b, FONT_S, MUTED); }
  }
  pageDots(canvas_, 3, page, 300.f + 116.f);
  button(B_PAGE_NEXT, 422.f, CY, 26.f, SURF, Icon::Play, MUTED);
  if (firstUse) { pill(canvas_, CX, 446.f - 6.f, 190.f, 40.f, SURF2, "Usar offline", FONT_S, TEXT); hit(B_FIRST_OFFLINE, CX, 440.f, 34.f); }
  else { pill(canvas_, CX, 446.f - 6.f, 190.f, 40.f, SURF2, "Fechar rede", FONT_S, TEXT); hit(B_CFG_CLOSE, CX, 440.f, 34.f); drawBackChevron(); }
}

// ── Informações ─────────────────────────────────────────────────────────────
void App::drawInfo(ms_t now) {
  char b[64];
  static const char* const TITLE[6] = {"Sobre", "Rede", "Hora", "Memória", "Sensores", "Recordes"};
  canvas_.textCentered(76.f, TITLE[infoPage_], FONT_M, C_INFO);
  float y = 136.f;
  auto line = [&](const char* s, Color c = TEXT) { canvas_.textCentered(y, s, FONT_S, c); y += 32.f; };
  const Progress& Pr = P();
  switch (infoPage_) {
    case 0: {
      canvas_.textCentered(y + 8.f, "TubaBot", FONT_L, TEXT); y += 48.f;
      StrBuf s(b, sizeof b); s.put("versão ").put(TB_VERSION); line(b, MUTED);
      StrBuf u(b, sizeof b); u.put("ligado há "); formatDuration(u, now - bootAt_, false); line(b);
      line("ESP32-S3 · AMOLED 466×466", MUTED);
      if (recovered_) line("dados restaurados ao padrão", WARN);
      break;
    }
    case 1: {
      NetStatus ns; plat_.netStatus(ns);
      line(ns.apActive ? "rede própria: ligada" : "rede própria: desligada");
      StrBuf s(b, sizeof b); s.put("casa: ");
      if (!ns.staConfigured) s.put("não usada"); else if (ns.sta == StaState::Connected) s.put(ns.staIp); else s.put("desconectada");
      line(b);
      line(ns.mdns, MUTED);
      line("funciona offline", MUTED);
      break;
    }
    case 2:
      if (clock_.valid()) {
        CivilClock::Local L = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin);
        StrBuf s(b, sizeof b); s.numPad(L.day, 2).put('/').numPad(L.month, 2).put('/').num(L.year).put(' ').numPad(L.hour, 2).put(':').numPad(L.minute, 2);
        line(b);
        line(TZ_TABLE[set_.tzIndex].label, MUTED);
        line(clockFromRtc_ ? "lida do relógio da placa (RTC)" : "ajustada pelo portal/rede", MUTED);
      } else { line("hora não ajustada"); line("ajuste pelo portal", MUTED); }
      line(caps_.rtc ? "RTC com bateria: sim" : "sem RTC", MUTED);
      break;
    case 3: {
      SysInfo si; plat_.sysInfo(si);
      StrBuf s(b, sizeof b); s.put("RAM livre ").unum(si.heapFree / 1024).put(" KB"); line(b);
      StrBuf p(b, sizeof b); p.put("PSRAM livre ").unum(si.psramFree / 1024).put(" KB"); line(b);
      StrBuf f(b, sizeof b); f.put("flash livre ").unum(si.sketchFree / 1024).put(" KB"); line(b);
      line(si.simulated ? "simulador" : si.chip, MUTED);
      break;
    }
    case 4: {
      StrBuf s(b, sizeof b); s.put("toque: ").unum(input_.rawEdges()).put(" bordas").put(input_.stuck() ? " (preso!)" : ""); line(b);
      StrBuf m(b, sizeof b);
      if (motion_.active()) m.put("movimento: ").num(motion_.lpX()).put(' ').num(motion_.lpY()).put(' ').num(motion_.lpZ()).put(" mg");
      else m.put("movimento: sem leitura");
      line(b);
      StrBuf a(b, sizeof b);
      if (!set_.micReactions) a.put("som: desligado"); else if (sound_.active()) a.put("som: nível ").num(sound_.level()).put(" (base ").num(sound_.baseline()).put(')'); else a.put("som: sem leitura");
      line(b);
      StrBuf p(b, sizeof b);
      if (power_.known) { p.put("bateria: "); if (power_.battery) p.num(power_.percent).put("% ").num(power_.mv).put(" mV"); else p.put("ausente"); if (power_.charging) p.put(" carregando"); }
      else p.put("energia: sem leitura");
      line(b);
      break;
    }
    default: {
      StrBuf s(b, sizeof b); s.put("Pula: ").num(Pr.jumpBest).put("  Bote: ").num(Pr.fishBest).put("  Mergulho: ").num(Pr.diveBest); line(b);
      StrBuf r(b, sizeof b); r.put("Reflexo: "); if (Pr.reflexBestMs) r.num(Pr.reflexBestMs).put(" ms"); else r.put("--"); line(b);
      StrBuf t(b, sizeof b); t.put("Tempo Certo: "); if (Pr.timingBestPct100) t.fixed2(Pr.timingBestPct100).put('%'); else t.put("--"); line(b);
      StrBuf c(b, sizeof b); c.put("carinhos ").unum(Pr.pets).put(" · boops ").unum(Pr.boops); line(b, MUTED);
      break;
    }
  }
  pageDots(canvas_, 6, infoPage_, 420.f);
  button(B_PAGE_NEXT, 422.f, CY, 26.f, SURF, Icon::Play, MUTED);
  drawBackChevron();
}

// ── diálogos ────────────────────────────────────────────────────────────────
void App::drawConfirm(ms_t now) {
  (void)now;
  drawIcon(canvas_, confirmAction_ == C_CANCEL_POMO ? Icon::Tomato : Icon::Stopwatch, CX, 130.f, 70.f, confirmAction_ == C_CANCEL_POMO ? C_POMO : C_SW);
  canvas_.textCentered(232.f, confirmAction_ == C_CANCEL_POMO ? "Cancelar Pomodoro?" : "Zerar cronômetro?", FONT_L, TEXT);
  button(B_CANCEL, CX - 80.f, 330.f, 42.f, SURF2, Icon::Cross, TEXT);
  button(B_OK, CX + 80.f, 330.f, 42.f, BAD, Icon::Check, 0x0C0F14);
}

void App::drawRecovery(ms_t now) {
  drawIcon(canvas_, Icon::Wifi, CX, 120.f, 70.f, C_FACE);
  canvas_.textCentered(216.f, "Abrir configuração?", FONT_L, TEXT);
  canvas_.textCentered(250.f, "rede própria do Tuba", FONT_S, MUTED);
  int pm = (int)(recoveryUntil_ > now ? (recoveryUntil_ - now) * 1000 / 6000 : 0);
  progressRing(canvas_, 220.f, 6.f, pm, C_FACE);
  button(B_REC_NO, CX - 80.f, 336.f, 42.f, SURF2, Icon::Cross, TEXT);
  button(B_REC_YES, CX + 80.f, 336.f, 42.f, C_FACE, Icon::Check, 0x0C0F14);
}

void App::drawResume(ms_t now) {
  (void)now;
  char b[48];
  canvas_.textCentered(96.f, "Sessão interrompida", FONT_L, TEXT);
  float y = 150.f;
  if (ckpt_.pomoActive) {
    StrBuf s(b, sizeof b); s.put(ckpt_.pomoPhase == 0 ? "Foco " : "Pausa "); formatDuration(s, ckpt_.pomoRemainingMs, true);
    canvas_.textCentered(y, b, FONT_M, C_POMO); y += 34.f;
  }
  for (int i = 0; i < kTimers && y < 240.f; i++) if (ckpt_.timerActive[i]) {
    StrBuf s(b, sizeof b); s.put("Timer ").num(i + 1).put(' '); formatDuration(s, ckpt_.timerRemainingMs[i], true);
    canvas_.textCentered(y, b, FONT_M, C_TIMER); y += 34.f;
  }
  if (ckpt_.swActive && y < 250.f) { StrBuf s(b, sizeof b); s.put("Cronômetro "); formatDuration(s, ckpt_.swElapsedMs, false); canvas_.textCentered(y, b, FONT_M, C_SW); }
  canvas_.textCentered(286.f, "(o tempo desligado não é contado)", FONT_S, MUTED);
  button(B_RESUME_NO, CX - 80.f, 360.f, 40.f, SURF2, Icon::Cross, TEXT);
  button(B_RESUME_YES, CX + 80.f, 360.f, 40.f, OK, Icon::Play, 0x0C0F14);
  canvas_.textCentered(424.f, "descartar · retomar", FONT_S, DIM);
}

void App::drawAlert(ms_t now) {
  const Alert& a = alerts_[0];
  canvas_.fillCircle(CX, CY, 233.f, 0x000000);
  Color col = C_TIMER; Icon ic = Icon::Timer;
  const char* head = "Aviso";
  char b[48]; StrBuf s(b, sizeof b);
  switch (a.type) {
    case AlertType::Timer: head = "Timer"; s.put("Timer ").num(a.index + 1).put(" acabou!"); break;
    case AlertType::PomoFocusDone: head = "Pomodoro"; s.put("Foco concluído!"); col = C_POMO; ic = Icon::Tomato; break;
    case AlertType::PomoBreakDone: head = "Pomodoro"; s.put("Pausa acabou!"); col = C_SW; ic = Icon::Tomato; break;
    case AlertType::Reminder: head = "Lembrete"; s.put(set_.reminders[a.index].label); col = C_INFO; ic = Icon::Bell; break;
    case AlertType::Alarm: {
      head = set_.alarms[a.index].label; col = C_ALARM; ic = Icon::Alarm;
      fmtTime(s, set_.alarms[a.index].hour, set_.alarms[a.index].minute, set_.clock24h);
      break;
    }
  }
  float p = 0.5f + 0.5f * fsin((float)((now - a.at) % 1200) / 1200.f * kTau);
  canvas_.ring(CX, CY, 214.f, 10.f + 6.f * p, col, 120 + (int)(110.f * p));
  canvas_.glow(CX, 130.f, 110.f, col, (int)(70.f * p));
  float wig = a.type == AlertType::Alarm ? 0.18f * fsin((float)(now % 300) / 300.f * kTau) : 0.f;
  canvas_.fillCircle(CX, 130.f, 54.f, col);
  drawIcon(canvas_, ic, CX + 30.f * wig, 130.f, 60.f, 0x0C0F14);
  canvas_.textCentered(226.f, head, FONT_M, col);
  const Font* fonts[3] = {&FONT_XL, &FONT_L, &FONT_M};
  canvas_.textCentered(a.type == AlertType::Alarm ? 300.f : 280.f, b, Canvas::fitFont(b, 360, fonts, 3), TEXT);
  bool snooze = a.type == AlertType::Reminder || a.type == AlertType::Alarm;
  if (snooze) {
    pill(canvas_, CX - 86.f, 370.f, 150.f, 52.f, SURF2, a.type == AlertType::Alarm ? "Adiar 5 min" : "Adiar 10", FONT_S, TEXT);
    hitRect(B_ALERT_SNOOZE, CX - 161.f, 344.f, 150.f, 52.f);
    pill(canvas_, CX + 86.f, 370.f, 150.f, 52.f, col, "Ok", FONT_M, 0x0C0F14);
    hitRect(B_ALERT_OK, CX + 11.f, 344.f, 150.f, 52.f);
  } else {
    pill(canvas_, CX, 370.f, 170.f, 52.f, col, "Ok", FONT_M, 0x0C0F14);
    hitRect(B_ALERT_OK, CX - 85.f, 344.f, 170.f, 52.f);
  }
  if (alertN_ > 1) { char c[8]; StrBuf s2(c, sizeof c); s2.put('+').num(alertN_ - 1); canvas_.textCentered(432.f, c, FONT_S, MUTED); }
  else if (caps_.imu && set_.faceDownSnooze && snooze) canvas_.textCentered(432.f, "vire para baixo: adiar", FONT_S, DIM);
}

void App::drawHoldHint(ms_t now) {
  if (input_.mode() == TouchMode::Game || !input_.pressed()) return;
  if (input_.stuck()) {
    canvas_.fillRoundRect(70.f, 186.f, 326.f, 94.f, 30.f, 0x0B0F17, 240);
    canvas_.textCentered(226.f, "Toque contínuo?", FONT_M, WARN);
    canvas_.textCentered(258.f, "tire o dedo / limpe a tela", FONT_S, MUTED);
    return;
  }
  ms_t h = input_.holdMs(now);
  if (!input_.moved() && h >= 3500 && screen_ != Screen::RecoveryConfirm) {
    int pm = (int)tbMin<ms_t>(1000, (h - 3500) * 1000 / 4500);
    progressRing(canvas_, 226.f, 8.f, pm, C_FACE, 0x10141C);
    canvas_.fillRoundRect(94.f, 380.f, 278.f, 48.f, 24.f, 0x0B0F17, 230);
    canvas_.textCentered(412.f, pm >= 1000 ? "solte para configurar" : "segure para configurar...", FONT_S, C_FACE);
  }
}

void App::drawToast(ms_t now) {
  if (toastUntil_ <= now || screen_ == Screen::ScreenOff) return;
  int w = Canvas::textWidth(toast_, FONT_M) + 44;
  w = tbMin(w, 380);
  float a = fclamp((float)(toastUntil_ - now) / 250.f, 0.f, 1.f);
  canvas_.fillRoundRect(CX - (float)w * 0.5f, 92.f, (float)w, 50.f, 25.f, 0x222A3A, (int)(245.f * a));
  canvas_.textCentered(126.f, toast_, FONT_M, TEXT, (int)(255.f * a));
}

// ── composição ──────────────────────────────────────────────────────────────
void App::render(ms_t now) {
  if (diagUntil_ > now) {
    // padrão de teste do AMOLED: barras de cor, gradiente, bordas e cantos
    static const Color BARS[7] = {0xFFFFFF, 0xFFFF00, 0x00FFFF, 0x00FF00, 0xFF00FF, 0xFF0000, 0x0000FF};
    for (int i = 0; i < 7; i++) canvas_.fillRect(33 + i * 57, 60, 57, 170, BARS[i]);
    canvas_.vGradient(33, 240, 400, 60, 0xFFFFFF, 0x000000);
    canvas_.ring(CX, CY, 230.f, 3.f, 0xFFFFFF);
    canvas_.textCentered(360.f, "TESTE 466×466", FONT_L, TEXT);
    canvas_.textCentered(400.f, "topo", FONT_S, MUTED);
    return;
  }
  ms_t since = now - screenAt_;
  if (since < 260 && screen_ != Screen::Game && screen_ != Screen::Intro && !alertN_) {
    float p = easeOut((float)since / 260.f);
    canvas_.setGlobalAlpha(60 + (int)(195.f * p));
    canvas_.setOrigin((float)transDir_ * -80.f * (1.f - p), 0.f);
  }
  switch (screen_) {
    case Screen::Intro: drawIntro(now); break;
    case Screen::FirstUse: drawConfigPages(true, now); break;
    case Screen::Config: drawConfigPages(false, now); break;
    case Screen::Face: drawFaceScreen(now); break;
    case Screen::Clock: drawClock(now); break;
    case Screen::Today: drawToday(now); break;
    case Screen::Launcher: drawLauncher(now); break;
    case Screen::Quick: drawQuick(now); break;
    case Screen::Pomodoro: drawPomodoro(now); break;
    case Screen::Timers: drawTimers(now); break;
    case Screen::TimerSet: drawTimerSet(now); break;
    case Screen::Stopwatch: drawStopwatch(now); break;
    case Screen::Alarms: drawAlarms(now); break;
    case Screen::AlarmEdit: drawAlarmEdit(now); break;
    case Screen::GamesMenu: drawGamesMenu(now); break;
    case Screen::Game: if (game_) game_->draw(canvas_, now); break;
    case Screen::Expressions: drawExpressions(now); break;
    case Screen::Breath: drawBreath(now); break;
    case Screen::Level: drawLevel(now); break;
    case Screen::Dice: drawDice(now); break;
    case Screen::Weather: drawWeather(now); break;
    case Screen::Stats: drawStats(now); break;
    case Screen::Settings: drawSettings(now); break;
    case Screen::Info: drawInfo(now); break;
    case Screen::Torch: canvas_.fillCircle(CX, CY, 233.f, 0xFFFFFF); canvas_.textCentered(440.f, "toque para sair", FONT_S, 0x8A8A8A); break;
    case Screen::Confirm: drawConfirm(now); break;
    case Screen::RecoveryConfirm: drawRecovery(now); break;
    case Screen::Resume: drawResume(now); break;
    case Screen::ScreenOff: break;  // AMOLED apagado (timers seguem)
    case Screen::Ota:
      drawIcon(canvas_, Icon::Reset, CX, 150.f, 70.f, C_FACE);
      canvas_.textCentered(250.f, "Atualizando...", FONT_L, TEXT);
      progressRing(canvas_, 214.f, 12.f, otaPermille_, C_FACE);
      canvas_.textCentered(300.f, "não desligue", FONT_S, MUTED);
      break;
    default: break;
  }
  canvas_.setOrigin(0, 0);
  canvas_.setGlobalAlpha(255);
  if (alertN_ && screen_ != Screen::ScreenOff) { hitN_ = 0; drawAlert(now); }
  else if (screen_ != Screen::ScreenOff && screen_ != Screen::Ota) drawHoldHint(now);
  drawToast(now);
}

}  // namespace tb
