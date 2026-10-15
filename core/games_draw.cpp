// Visual colorido dos jogos (a lógica fica em games.cpp, inalterada da v1).
#include "games.h"
#include "ui.h"

namespace tb {

int32_t floorDiv1000(int32_t v);

using GV = GameView;
static const Color SEA_TOP = 0x1E6FB8, SEA_BOT = 0x0A2A55, SAND = 0xE9C46A, SAND_D = 0xC9A24E, CORAL = 0xFF6B5B, CORAL_L = 0xFF9B8A;
static const Color SHARK = 0x8FB3D9, SHARK_B = 0xE6F1FB, FISH = 0xFF9F43, FISH_L = 0xFFC37A, GOLD = 0xFFD54A;

void drawSprite(Canvas& c, const Sprite& s, int x, int y, Color col) {
  for (int j = 0; j < s.h; j++)
    for (int i = 0; i < s.w; i++)
      if (s.rows[j][i] == '#') c.fillRect(x + i, y + j, 1, 1, col);
}

// cantos arredondados: pinta de preto o que fica fora do retângulo arredondado
static void cornerMask(Canvas& c, float x, float y, float w, float h, float r) {
  float hx = w * 0.5f, hy = h * 0.5f, cx = x + hx, cy = y + hy;
  for (int k = 0; k < 4; k++) {
    float qx = (k & 1) ? x + w - r - 1 : x, qy = (k & 2) ? y + h - r - 1 : y;
    c.shade(qx, qy, qx + r + 1, qy + r + 1, 0x000000, 255, [=](float px, float py) {
      float ax = fabsf_(px - cx) - (hx - r), ay = fabsf_(py - cy) - (hy - r);
      float d = fsqrt(fmaxf_(ax, 0.f) * fmaxf_(ax, 0.f) + fmaxf_(ay, 0.f) * fmaxf_(ay, 0.f)) + fminf_(fmaxf_(ax, ay), 0.f) - r;
      return -d;
    });
  }
}
// palco: retângulo arredondado com gradiente de mar
static void stage(Canvas& c, float x, float y, float w, float h, float r, Color top, Color bot) {
  c.vGradient((int)x, (int)y, (int)w, (int)h, top, bot);
  cornerMask(c, x, y, w, h, r);
}
static void seaStage(Canvas& c) { stage(c, GV::X(0), GV::Y(0), GV::S(128), GV::S(64), 26.f, SEA_TOP, SEA_BOT); }

static void bubbles(Canvas& c, ms_t now, float x0, float y0, float w, float h, int n) {
  for (int i = 0; i < n; i++) {
    uint32_t r = (uint32_t)i * 2654435761u;
    float period = 2600.f + (float)(r % 1500);
    float p = (float)((now + r % 3000) % (ms_t)period) / period;
    float x = x0 + (float)((r >> 8) % (uint32_t)w) + 6.f * fsin(p * kTau * 2.f);
    float y = y0 + h - p * h;
    c.ring(x, y, 3.f + (float)(r % 4), 1.6f, 0xFFFFFF, (int)(110.f * (1.f - p)));
  }
}

static void sharkSide(Canvas& c, float x, float y, float s, bool happy) {
  // x,y: canto superior esquerdo da caixa 16×9 (unidades lógicas, s = px por unidade)
  auto X = [&](float v) { return x + v * s; };
  auto Y = [&](float v) { return y + v * s; };
  Pt tail[3] = {{X(2.5f), Y(5)}, {X(-0.5f), Y(1.2f)}, {X(0.3f), Y(8.6f)}};
  c.fillPolygon(tail, 3, SHARK);
  Pt fin[3] = {{X(6), Y(3)}, {X(8.5f), Y(-0.6f)}, {X(10.5f), Y(3.2f)}};
  c.fillPolygon(fin, 3, SHARK);
  c.fillEllipse(X(9), Y(5), 7.2f * s, 3.4f * s, SHARK);
  c.fillEllipse(X(10), Y(6.4f), 5.4f * s, 1.8f * s, SHARK_B);
  c.fillCircle(X(13.2f), Y(4.1f), 0.9f * s, 0x0C1A2B);
  c.fillCircle(X(13.0f), Y(3.8f), 0.32f * s, 0xFFFFFF);
  if (happy) c.arc(X(13.3f), Y(5.6f), 1.1f * s, 0.35f * s, kPi - 0.9f, 1.8f, 0x0C1A2B);
}

static void fishSide(Canvas& c, float left, float top, float s, Color body, Color fin) {
  auto X = [&](float v) { return left + v * s; };
  auto Y = [&](float v) { return top + v * s; };
  Pt tail[3] = {{X(3.2f), Y(3.5f)}, {X(0), Y(0.4f)}, {X(0), Y(6.6f)}};
  c.fillPolygon(tail, 3, fin);
  c.fillEllipse(X(7.6f), Y(3.5f), 5.4f * s, 3.4f * s, body);
  c.fillCircle(X(10.4f), Y(2.7f), 0.75f * s, 0x10141C);
  c.line(X(6.2f), Y(1.0f), X(6.2f), Y(6.0f), 0.5f * s, fin, 160);
}

static void jelly(Canvas& c, float x, float y, float s, ms_t now) {
  float wob = fsin((float)(now % 900) / 900.f * kTau);
  for (int i = 0; i < 4; i++) {
    float tx = x + (-4.5f + 3.f * (float)i) * s;
    c.line(tx, y, tx + wob * 1.2f * s, y + 6.f * s, 0.9f * s, 0xFF9AD5, 200);
  }
  c.shade(x - 6 * s, y - 6 * s, x + 6 * s, y + 0.5f * s, 0xFF7AC8, 235, [=](float px, float py) {
    float d = fsqrt((px - x) * (px - x) + (py - y) * (py - y)) - 6.f * s;
    return fmaxf_(d, py - y);
  });
  c.fillCircle(x - 2 * s, y - 3 * s, 1.1f * s, 0xFFFFFF, 160);
}

// ── telas comuns ────────────────────────────────────────────────────────────
static void introCard(Canvas& c, Icon ic, Color col, const char* title, const char* l1, const char* l2, const char* rec) {
  c.fillCircle((float)SCR_CX, 112.f, 44.f, col);
  drawIcon(c, ic, (float)SCR_CX, 112.f, 50.f, 0x0C0F14);
  c.textCentered(196.f, title, FONT_L, ui::TEXT);
  c.textCentered(240.f, l1, FONT_M, ui::MUTED);
  if (l2) c.textCentered(272.f, l2, FONT_M, ui::MUTED);
  if (rec) c.textCentered(314.f, rec, FONT_S, col);
  ui::pill(c, (float)SCR_CX, 364.f, 262.f, 56.f, col, "Toque para começar", FONT_M, 0x0C0F14);
  c.textCentered(424.f, "segure para sair", FONT_S, ui::DIM);
}

void Game::drawResultFooter(Canvas& c) {
  ui::pill(c, (float)SCR_CX, 392.f, 270.f, 54.f, ui::TEXT, "Toque: jogar de novo", FONT_M, 0x0C0F14);
}

static void resultCard(Canvas& c, const char* title, const char* l1, const char* l2, bool record, Color col) {
  c.fillRoundRect(78.f, 150.f, 310.f, 170.f, 36.f, 0x0B0F17, 235);
  c.strokeRoundRect(78.f, 150.f, 310.f, 170.f, 36.f, 3.f, record ? GOLD : col);
  c.textCentered(198.f, title, FONT_L, record ? GOLD : ui::TEXT);
  if (l1) c.textCentered(244.f, l1, FONT_M, ui::TEXT);
  if (l2) c.textCentered(284.f, l2, FONT_S, record ? GOLD : ui::MUTED);
}

static void pausedOverlay(Canvas& c) {
  c.fillCircle((float)SCR_CX, (float)SCR_CY, 233.f, 0x000000, 150);
  ui::roundButton(c, (float)SCR_CX, 200.f, 52.f, ui::TEXT, Icon::Play, 0x0C0F14);
  c.textCentered(300.f, "PAUSA", FONT_L, ui::TEXT);
  c.textCentered(338.f, "toque para seguir", FONT_S, ui::MUTED);
}

static void scoreTop(Canvas& c, const char* s, Color col) { c.textCentered(118.f, s, FONT_XL, col); }

// ════════ Pula, Tuba! ════════
void JumpGame::draw(Canvas& c, ms_t now) {
  char b[40];
  if (phase_ == GamePhase::Intro) {
    StrBuf r(b, sizeof b); r.put("Recorde: ").num(best);
    introCard(c, Icon::Shark, 0x5CC8FF, "Pula, Tuba!", "Toque para pular", "os corais", b);
    return;
  }
  c.vGradient((int)GV::X(0), (int)GV::Y(0), (int)GV::S(128), (int)GV::S(64), SEA_TOP, SEA_BOT);
  float sc = (float)(scroll_ / 1000);
  // raios de luz e bolhas
  for (int i = 0; i < 3; i++) {
    float x = GV::X(20.f + 44.f * (float)i - fmaxf_(0.f, (float)((int)sc % 44)) * 0.3f);
    Pt ray[4] = {{x, GV::Y(0)}, {x + 26.f, GV::Y(0)}, {x + 70.f, GV::Y(kGroundY)}, {x + 30.f, GV::Y(kGroundY)}};
    c.fillPolygon(ray, 4, 0xFFFFFF, 14);
  }
  bubbles(c, now, GV::X(4), GV::Y(4), GV::S(120), GV::S(50), 7);
  // fundo de areia
  c.fillRect((int)GV::X(0), (int)GV::Y(kGroundY), (int)GV::S(128), (int)GV::S(64 - kGroundY), SAND);
  int off = ((int)sc) % 12;
  for (int x = -off; x < 128; x += 12) c.fillCircle(GV::X((float)x + 4.f), GV::Y((float)kGroundY + 2.5f), 2.2f, SAND_D);
  for (const auto& o : obs_) {
    if (!o.active) continue;
    float ox = (float)floorDiv1000(o.x);
    c.fillRoundRect(GV::X(ox), GV::Y((float)(kGroundY - o.h)), GV::S((float)o.w), GV::S((float)o.h) + 2.f, 5.f, CORAL);
    c.fillRoundRect(GV::X(ox) + 3.f, GV::Y((float)(kGroundY - o.h)) + 3.f, GV::S((float)o.w) - 6.f, 6.f, 3.f, CORAL_L);
  }
  float top = (float)(kGroundY - SPR_SHARK.h) - (float)y_ / 1000.f;
  sharkSide(c, GV::X((float)kSharkX), GV::Y(top), (float)GV::kScale, phase_ != GamePhase::Result);
  cornerMask(c, GV::X(0), GV::Y(0), GV::S(128), GV::S(64), 26.f);
  StrBuf s(b, sizeof b); s.num(score_);
  scoreTop(c, b, ui::TEXT);
  if (phase_ == GamePhase::Result) {
    char l1[32], l2[32];
    StrBuf a(l1, sizeof l1); a.put("Pontos: ").num(score_);
    StrBuf r(l2, sizeof l2); if (newRecord) r.put("Novo recorde!"); else r.put("Recorde: ").num(best);
    resultCard(c, "Ops! Quase!", l1, l2, newRecord, 0x5CC8FF);
    drawResultFooter(c);
  } else {
    c.textCentered(380.f, "toque: pular · segure: sair", FONT_S, ui::DIM);
  }
  if (paused_) pausedOverlay(c);
}

// ════════ Tuba Reflexo ════════
void ReflexGame::draw(Canvas& c, ms_t now) {
  char b[40];
  switch (sub_) {
    case Sub::Ready: {
      StrBuf s(b, sizeof b); s.put("Recorde: "); if (best) s.num(best).put(" ms"); else s.put("--");
      introCard(c, Icon::Bolt, 0xFFD54A, "Tuba Reflexo", "Espere o AGORA!", "e toque rápido", b);
      break;
    }
    case Sub::Waiting: {
      float p = 0.5f + 0.5f * fsin((float)(now % 1000) / 1000.f * kTau);
      c.fillCircle((float)SCR_CX, (float)SCR_CY, 233.f, 0x2A0B12);
      c.ring((float)SCR_CX, (float)SCR_CY, 150.f + 10.f * p, 8.f, 0xFF5E57, 150);
      c.textCentered(250.f, "ESPERE...", FONT_XL, 0xFFB4A8);
      c.textCentered(310.f, "não toque ainda", FONT_M, ui::MUTED);
      break;
    }
    case Sub::Signal:
      c.fillCircle((float)SCR_CX, (float)SCR_CY, 233.f, 0x22C55E);
      c.textCentered(258.f, "AGORA!", FONT_XL, 0x062B14);
      signalRendered_ = true;
      break;
    case Sub::Done: {
      StrBuf s(b, sizeof b); s.num(lastMs_).put(" ms");
      Color col = lastMs_ < 250 ? ui::OK : lastMs_ < 400 ? 0xFFD54A : ui::WARN;
      c.textCentered(214.f, b, FONT_XXL, col);
      const char* react = lastMs_ < 200 ? "Uau! Relâmpago!" : lastMs_ < 300 ? "Rápido!" : lastMs_ < 450 ? "Boa!" : "Dá pra melhorar";
      c.textCentered(126.f, react, FONT_L, ui::TEXT);
      StrBuf s2(b, sizeof b);
      if (newRecord) s2.put("Novo recorde!"); else { s2.put("Melhor: "); if (best) s2.num(best).put(" ms"); else s2.put("--"); }
      c.textCentered(282.f, b, FONT_M, newRecord ? GOLD : ui::MUTED);
      drawResultFooter(c);
      break;
    }
    case Sub::Early:
      c.textCentered(170.f, "Cedo demais!", FONT_XL, ui::WARN);
      c.textCentered(230.f, "Rodada inválida", FONT_M, ui::TEXT);
      c.textCentered(270.f, "Espere o AGORA!", FONT_S, ui::MUTED);
      drawResultFooter(c);
      break;
    case Sub::Timeout:
      c.textCentered(190.f, "Tempo esgotado", FONT_L, ui::WARN);
      c.textCentered(240.f, "Rodada inválida", FONT_M, ui::MUTED);
      drawResultFooter(c);
      break;
    case Sub::Interrupted:
      c.textCentered(190.f, "Rodada interrompida", FONT_L, ui::TEXT);
      c.textCentered(240.f, "(sem recorde)", FONT_M, ui::MUTED);
      drawResultFooter(c);
      break;
  }
}

// ════════ Hora do Bote ════════
void FishGame::draw(Canvas& c, ms_t now) {
  char b[40];
  if (phase_ == GamePhase::Intro) {
    StrBuf s(b, sizeof b); s.put("Recorde: ").num(best).put(" peixes");
    introCard(c, Icon::Fish, FISH, "Hora do Bote", "Toque com o peixe", "TODO dentro do quadro", b);
    return;
  }
  seaStage(c);
  bubbles(c, now, GV::X(4), GV::Y(4), GV::S(120), GV::S(56), 5);
  // quadro alvo (interior útil x = 49..78)
  float bx = GV::X(kBoxX), by = GV::Y(kBoxY), bs = GV::S(kBoxSize);
  c.fillRoundRect(bx, by, bs, bs, 10.f, 0xFFFFFF, 22);
  c.strokeRoundRect(bx + 1.5f, by + 1.5f, bs - 3.f, bs - 3.f, 9.f, 3.f, GOLD);
  for (int k = 0; k < 4; k++) {
    float qx = (k & 1) ? bx + bs + 6.f : bx - 6.f, qy = (k & 2) ? by + bs + 6.f : by - 6.f;
    float sx = (k & 1) ? -1.f : 1.f, sy = (k & 2) ? -1.f : 1.f;
    c.line(qx, qy, qx + 16.f * sx, qy, 4.f, GOLD);
    c.line(qx, qy, qx, qy + 16.f * sy, 4.f, GOLD);
  }
  ms_t gt = gameTime(now);
  StrBuf sc(b, sizeof b); sc.num(captures_);
  scoreTop(c, b, ui::TEXT);
  if (phase_ == GamePhase::Playing) {
    if (catching_) {
      float p = (float)(gt - catchAt_) / (float)catchAnimMs;
      c.glow((float)SCR_CX, by + bs * 0.5f, 90.f, GOLD, (int)(160.f * (1.f - p)));
      c.textCentered(by + bs * 0.5f + 16.f, "NHAC!", FONT_XL, GOLD);
    } else {
      fishSide(c, GV::X((float)floorDiv1000(fishXsubAt(gt))), GV::Y((float)kFishY), (float)GV::kScale, FISH, FISH_L);
    }
    c.textCentered(380.f, "toque com o peixe no quadro", FONT_S, ui::DIM);
  } else if (phase_ == GamePhase::Result) {
    fishSide(c, GV::X((float)lostX_), GV::Y((float)kFishY), (float)GV::kScale, 0x9AA5B8, 0xC9D1DE);
    char l1[32], l2[32];
    StrBuf a(l1, sizeof l1); a.put("Peixes: ").num(captures_);
    StrBuf r(l2, sizeof l2); if (newRecord) r.put("Novo recorde!"); else r.put("Recorde: ").num(best);
    resultCard(c, "Escapou!", l1, l2, newRecord, FISH);
    drawResultFooter(c);
  }
  if (paused_) pausedOverlay(c);
}

// ════════ No Tempo Certo ════════
void TimingGame::draw(Canvas& c, ms_t now) {
  char b[40];
  const Color col = 0x4BF0A6;
  switch (sub_) {
    case Sub::Ready: {
      c.textCentered(110.f, "No Tempo Certo", FONT_L, ui::TEXT);
      c.textCentered(160.f, "Pare o tempo em:", FONT_M, ui::MUTED);
      StrBuf s(b, sizeof b); s.fixed2(targetCs_).put(" s");
      c.textCentered(270.f, b, FONT_XXL, col);
      StrBuf s2(b, sizeof b); s2.put("Recorde: "); if (best) s2.fixed2(best).put('%'); else s2.put("--");
      c.textCentered(318.f, b, FONT_S, ui::MUTED);
      ui::pill(c, (float)SCR_CX, 370.f, 262.f, 56.f, col, "Toque para começar", FONT_M, 0x0C0F14);
      c.textCentered(428.f, "segure para sair", FONT_S, ui::DIM);
      break;
    }
    case Sub::Countdown: {
      ms_t left = goAt_ > now ? goAt_ - now : 0;
      int digit = tbClamp((int)((left + 999) / 1000), 1, 3);
      float frac = (float)(left % 1000) / 1000.f;
      ui::progressRing(c, 200.f, 10.f, (int)(frac * 1000.f), col);
      char d[2] = {(char)('0' + digit), 0};
      c.textCentered(282.f, d, FONT_CLOCK, ui::TEXT);
      StrBuf s(b, sizeof b); s.put("Alvo ").fixed2(targetCs_).put(" s");
      c.textCentered(120.f, b, FONT_M, ui::MUTED);
      break;
    }
    case Sub::Running: {
      ms_t e = now >= goAt_ ? now - goAt_ : 0;
      StrBuf s(b, sizeof b); s.put("Alvo ").fixed2(targetCs_).put(" s");
      c.textCentered(120.f, b, FONT_M, ui::MUTED);
      StrBuf s2(b, sizeof b); s2.fixed2((int64_t)(e / 10));
      c.textCentered(270.f, b, FONT_XXL, ui::TEXT);
      if (e < 400) c.textCentered(350.f, "VAI!", FONT_XL, col);
      else c.textCentered(350.f, "toque para parar", FONT_M, ui::MUTED);
      break;
    }
    case Sub::Done: {
      int32_t target = targetCs_ * 10;
      int32_t diff = elapsed_ - target;
      int32_t pct = proximity100(elapsed_, target);
      Color pc = pct >= 9800 ? GOLD : pct >= 9000 ? col : ui::WARN;
      StrBuf s(b, sizeof b); s.put("Alvo ").fixed2(targetCs_).put(" · Você ").fixed2(elapsed_ / 10);
      c.textCentered(112.f, b, FONT_M, ui::MUTED);
      StrBuf s2(b, sizeof b); s2.fixed2(pct).put('%');
      c.textCentered(232.f, b, FONT_XXL, pc);
      StrBuf s3(b, sizeof b); s3.put("Diferença ").put(diff >= 0 ? '+' : '-').fixed2(tbAbs(diff) / 10).put(" s");
      c.textCentered(282.f, b, FONT_M, ui::TEXT);
      StrBuf s4(b, sizeof b);
      if (newRecord) s4.put("Novo recorde!"); else { s4.put("Recorde: "); if (best) s4.fixed2(best).put('%'); else s4.put("--"); }
      c.textCentered(322.f, b, FONT_S, newRecord ? GOLD : ui::MUTED);
      drawResultFooter(c);
      break;
    }
    case Sub::Timeout:
      c.textCentered(190.f, "Passou de 20 s!", FONT_L, ui::WARN);
      c.textCentered(240.f, "Rodada encerrada", FONT_M, ui::MUTED);
      drawResultFooter(c);
      break;
    case Sub::Interrupted:
      c.textCentered(190.f, "Rodada interrompida", FONT_L, ui::TEXT);
      c.textCentered(240.f, "(sem recorde)", FONT_M, ui::MUTED);
      drawResultFooter(c);
      break;
  }
}

// ════════ Mergulho do Tuba ════════
void DiveGame::draw(Canvas& c, ms_t now) {
  char b[40];
  if (phase_ == GamePhase::Intro) {
    StrBuf r(b, sizeof b); r.put("Recorde: ").num(best).put(" peixes");
    introCard(c, Icon::Level, 0x3EE6D0, "Mergulho do Tuba", "Incline (ou arraste)", "pegue peixes, fuja das águas-vivas", b);
    return;
  }
  const float s = (float)kViewScale, X0 = (float)kViewX0, Y0 = (float)kViewY0;
  stage(c, X0, Y0, (float)kW * s, (float)kH * s, 46.f, 0x15649F, 0x061A38);
  bubbles(c, now, X0 + 10.f, Y0 + 10.f, (float)kW * s - 20.f, (float)kH * s - 20.f, 6);
  for (const auto& it : items_) {
    if (!it.active) continue;
    float x = X0 + (float)it.x / 1000.f * s, y = Y0 + (float)it.y / 1000.f * s;
    if (it.jelly) jelly(c, x, y, s, now);
    else fishSide(c, x - 7.f * s, y - 3.5f * s, s * 0.9f, FISH, FISH_L);
  }
  bool blink = simT_ < invulnUntil_ && ((simT_ / 120) & 1);
  if (!blink) sharkSide(c, X0 + (float)sharkX_ / 1000.f * s - 8.f * s, Y0 + (float)kSharkY * s - 4.5f * s, s, phase_ != GamePhase::Result);
  StrBuf sc(b, sizeof b); sc.num(score_);
  c.text(X0 + 40.f, Y0 + 50.f, b, FONT_XL, ui::TEXT);
  for (int i = 0; i < kLives; i++) c.heart(X0 + 196.f + 26.f * (float)i, Y0 + 36.f, 20.f, i < lives_ ? (Color)0xFF5C9A : ui::SURF2);
  if (phase_ == GamePhase::Result) {
    char l1[32], l2[32];
    StrBuf a(l1, sizeof l1); a.put("Peixes: ").num(score_);
    StrBuf r(l2, sizeof l2); if (newRecord) r.put("Novo recorde!"); else r.put("Recorde: ").num(best);
    resultCard(c, "Fim do mergulho", l1, l2, newRecord, 0x3EE6D0);
    drawResultFooter(c);
  }
  if (paused_) pausedOverlay(c);
}

}  // namespace tb
