#include "games.h"

namespace tb {

const char* gameTitle(GameId g) {
  switch (g) {
    case GameId::Jump: return "Pula, Tuba!";
    case GameId::Reflex: return "Tuba Reflexo";
    case GameId::Fish: return "Hora do Bote";
    case GameId::Timing: return "No Tempo Certo";
    default: return "?";
  }
}

// ── Sprites próprios do TubaBot ─────────────────────────────────────────────
static const char* const SHARK_ROWS[] = {
  "......##........",
  ".....###........",
  "....####........",
  "#..##########...",
  "##########.###..",
  "###############.",
  "##.###########..",
  "#...########....",
  ".....#...#......",
};
const Sprite SPR_SHARK = {16, 9, SHARK_ROWS};

static const char* const FISH_ROWS[] = {
  "....######...",
  "#..########..",
  "##.######.##.",
  "#############",
  "##.#########.",
  "#..########..",
  "....######...",
};
const Sprite SPR_FISH = {13, 7, FISH_ROWS};

void drawSprite(Framebuffer& fb, const Sprite& s, int x, int y, bool on) {
  for (int j = 0; j < s.h; j++)
    for (int i = 0; i < s.w; i++)
      if (s.rows[j][i] == '#') fb.set(x + i, y + j, on);
}

static int32_t floorDiv1000(int32_t v) { return v >= 0 ? v / 1000 : -((-v + 999) / 1000); }

void Game::drawResultFooter(Framebuffer& fb) {
  fb.hline(0, 54, 128);
  fb.textCentered(56, "Toque: jogar de novo");
}

static void drawPausedOverlay(Framebuffer& fb) {
  fb.fillRect(24, 22, 80, 20, false);
  fb.rect(24, 22, 80, 20);
  fb.textCentered(25, "PAUSA");
  fb.textCentered(33, "toque: seguir");
}

static void drawIntro(Framebuffer& fb, const char* title, const char* l1, const char* l2, int32_t best, const char* bestLabel) {
  fb.textCentered(0, title);
  fb.hline(0, 9, 128);
  fb.textCentered(14, l1);
  if (l2) fb.textCentered(24, l2);
  char b[32]; StrBuf sb(b, sizeof b);
  sb.put("Recorde: ");
  if (bestLabel) sb.put(bestLabel); else sb.num(best);
  fb.textCentered(38, b);
  fb.hline(0, 54, 128);
  fb.textCentered(56, "Toque: começar");
}

// ════════════════════════════════════════════════════════════════════════════
// 9.1 Pula, Tuba!
// ════════════════════════════════════════════════════════════════════════════
void JumpGame::begin(ms_t now, Rng* rng, bool easy) {
  (void)now;
  rng_ = rng; easy_ = easy;
  phase_ = GamePhase::Intro; paused_ = false;
  y_ = vy_ = 0; score_ = 0; buffered_ = false;
  for (auto& o : obs_) o.active = false;
}

int32_t JumpGame::minGapSub(int32_t speedPx) const {
  // distância percorrida durante um voo completo + folga de reação + largura da hitbox
  int32_t t = airTimeMs() + p.reactionMarginMs;
  return speedPx * t + (kHitX1 - kHitX0 + 1) * 1000;
}

void JumpGame::startPlaying(ms_t now) {
  phase_ = GamePhase::Playing;
  pauseAccum_ = 0; paused_ = false;
  startT_ = gameTime(now);
  simT_ = 0;
  y_ = vy_ = 0; score_ = 0; buffered_ = false; scroll_ = 0;
  speed_ = p.speedStart;
  for (auto& o : obs_) o.active = false;
  // nenhum obstáculo imediato: o primeiro surge depois de firstObstacleDelayMs
  nextSpawnIn_ = speed_ * p.firstObstacleDelayMs;  // subpx
}

void JumpGame::doJump() { vy_ = p.jumpV * 1000; buffered_ = false; }

void JumpGame::press(ms_t t, ms_t now) {
  if (paused_) return;
  if (phase_ == GamePhase::Intro) { startPlaying(now); return; }
  if (phase_ == GamePhase::Result) {
    if (now - resultAt_ >= (ms_t)kResultGuardMs) startPlaying(now);
    return;
  }
  // Leva a simulação até o instante da entrada (nunca volta no tempo).
  ms_t target = gameTime(t) >= startT_ ? gameTime(t) - startT_ : 0;
  while (simT_ + kStepMs <= target && phase_ == GamePhase::Playing) { stepOnce(); simT_ += kStepMs; }
  if (phase_ != GamePhase::Playing) { resultAt_ = now; return; }
  if (!airborne()) doJump();
  else { buffered_ = true; bufferedAt_ = simT_; }  // vale só se aterrissar em até bufferMs
}

void JumpGame::spawn() {
  for (auto& o : obs_) {
    if (o.active) continue;
    o.active = true; o.scored = false;
    o.x = 130 * 1000;
    if (easy_ && score_ < 3) { o.w = 4; o.h = 6; }
    else if (score_ >= 10 && rng_->range(0, 3) == 0) { o.w = 10; o.h = 7; }
    else { o.w = (int16_t)rng_->range(4, 6); o.h = (int16_t)rng_->range(6, easy_ && score_ < 6 ? 7 : 9); }
    // próximo: gap mínimo seguro (usa a velocidade máxima possível até lá) + variação
    int32_t sp = tbMin(speed_ + p.speedStep * 2, p.speedMax);
    nextSpawnIn_ = minGapSub(sp) + o.w * 1000 + rng_->range(0, sp * 700);
    return;
  }
}

bool JumpGame::collides() const {
  int top = kGroundY - SPR_SHARK.h - y_ / 1000;
  int hx0 = kSharkX + kHitX0, hx1 = kSharkX + kHitX1;
  int hy0 = top + kHitY0, hy1 = top + kHitY1;
  for (const auto& o : obs_) {
    if (!o.active) continue;
    int ox0 = floorDiv1000(o.x), ox1 = ox0 + o.w - 1;
    int oy0 = kGroundY - o.h, oy1 = kGroundY - 1;
    if (hx0 <= ox1 && ox0 <= hx1 && hy0 <= oy1 && oy0 <= hy1) return true;
  }
  return false;
}

void JumpGame::stepOnce() {
  // física vertical (Euler semi-implícito, inteiros exatos)
  if (y_ > 0 || vy_ > 0) {
    vy_ -= p.gravity * kStepMs;           // subpx/s por passo (g*1000*step/1000)
    y_ += vy_ * kStepMs / 1000;
    if (y_ <= 0) {
      y_ = 0; vy_ = 0;
      if (buffered_ && simT_ - bufferedAt_ <= (ms_t)p.bufferMs) doJump();
      buffered_ = false;
    }
  }
  int32_t dx = speed_ * kStepMs;  // subpx
  scroll_ += dx;
  for (auto& o : obs_) {
    if (!o.active) continue;
    o.x -= dx;
    if (!o.scored && floorDiv1000(o.x) + o.w - 1 < kSharkX + kHitX0) {
      o.scored = true; score_++; sfx_ = 1;
      speed_ = tbMin(speed_ + p.speedStep, p.speedMax);
    }
    if (o.x < -20 * 1000) o.active = false;
  }
  nextSpawnIn_ -= dx;
  if (nextSpawnIn_ <= 0) spawn();
  if (collides()) {
    phase_ = GamePhase::Result;
    resultAt_ = startT_ + simT_ + pauseAccum_;  // aproximação em tempo de parede
    outcome_.finished = true; outcome_.valid = true; outcome_.score = score_;
    sfx_ = 2;
  }
}

void JumpGame::update(ms_t now) {
  if (phase_ != GamePhase::Playing || paused_) return;
  ms_t target = gameTime(now) - startT_;
  int guard = 0;
  while (simT_ + kStepMs <= target && phase_ == GamePhase::Playing && guard++ < 2000) { stepOnce(); simT_ += kStepMs; }
  if (phase_ == GamePhase::Result) resultAt_ = now;
}

void JumpGame::draw(Framebuffer& fb, ms_t now) {
  (void)now;
  if (phase_ == GamePhase::Intro) { drawIntro(fb, "PULA, TUBA!", "Toque para pular", "Segure: sair", best, nullptr); drawSprite(fb, SPR_SHARK, 4, 40); return; }
  // chão com textura que rola
  fb.hline(0, kGroundY, 128);
  int off = (scroll_ / 1000) % 12;
  for (int x = -off; x < 128; x += 12) { fb.set(x + 3, kGroundY + 2); fb.set(x + 8, kGroundY + 4); }
  for (const auto& o : obs_) {
    if (!o.active) continue;
    int ox = floorDiv1000(o.x);
    fb.fillRect(ox, kGroundY - o.h, o.w, o.h);
    if (o.w >= 10) fb.vline(ox + 4, kGroundY - o.h, 2, false), fb.vline(ox + 5, kGroundY - o.h, 2, false);
  }
  int top = kGroundY - SPR_SHARK.h - y_ / 1000;
  drawSprite(fb, SPR_SHARK, kSharkX, top);
  char b[12]; StrBuf sb(b, sizeof b); sb.num(score_);
  fb.text(127 - Framebuffer::textWidth(b), 0, b);
  if (phase_ == GamePhase::Result) {
    fb.fillRect(14, 8, 100, 40, false); fb.rect(14, 8, 100, 40);
    fb.textCentered(11, "Ops! Quase!");
    char r[32]; StrBuf s2(r, sizeof r); s2.put("Pontos: ").num(score_);
    fb.textCentered(22, r);
    StrBuf s3(r, sizeof r); s3.put(newRecord ? "Novo recorde!" : "Recorde: "); if (!newRecord) s3.num(best);
    fb.textCentered(33, r);
    fb.fillRect(0, 54, 128, 10, false);
    drawResultFooter(fb);
  }
  if (paused_) drawPausedOverlay(fb);
}

// ════════════════════════════════════════════════════════════════════════════
// 9.2 Tuba Reflexo
// ════════════════════════════════════════════════════════════════════════════
void ReflexGame::begin(ms_t now, Rng* rng, bool easy) {
  (void)now; (void)easy;
  rng_ = rng; sub_ = Sub::Ready; phase_ = GamePhase::Intro; paused_ = false;
}

void ReflexGame::press(ms_t t, ms_t now) {
  if (paused_) return;
  switch (sub_) {
    case Sub::Ready: case Sub::Done: case Sub::Early: case Sub::Timeout: case Sub::Interrupted:
      if (sub_ != Sub::Ready && now - resultAt_ < (ms_t)kResultGuardMs) return;
      // este toque INICIA a rodada; não pode servir de resposta
      sub_ = Sub::Waiting; phase_ = GamePhase::Playing;
      waitStart_ = now; waitMs_ = rng_->range(waitMinMs, waitMaxMs);
      signalRendered_ = shownKnown_ = false;
      return;
    case Sub::Waiting:
      sub_ = Sub::Early; break;
    case Sub::Signal:
      if (!shownKnown_ || t < shownAt_) { sub_ = Sub::Early; break; }
      lastMs_ = (int32_t)(t - shownAt_);
      sub_ = Sub::Done;
      outcome_.finished = true; outcome_.valid = true; outcome_.score = lastMs_;
      phase_ = GamePhase::Result; resultAt_ = now; sfx_ = 1;
      return;
  }
  // cedo demais: rodada inválida
  phase_ = GamePhase::Result; resultAt_ = now; sfx_ = 2;
  outcome_.finished = true; outcome_.valid = false;
}

void ReflexGame::update(ms_t now) {
  if (paused_) return;
  if (sub_ == Sub::Waiting && now - waitStart_ >= (ms_t)waitMs_) { sub_ = Sub::Signal; signalRendered_ = shownKnown_ = false; }
  if (sub_ == Sub::Signal && shownKnown_ && now - shownAt_ > (ms_t)timeoutMs) {
    sub_ = Sub::Timeout; phase_ = GamePhase::Result; resultAt_ = now;
    outcome_.finished = true; outcome_.valid = false;
  }
}

void ReflexGame::pause(ms_t now) {
  Game::pause(now);
  if (sub_ == Sub::Waiting || sub_ == Sub::Signal) {
    sub_ = Sub::Interrupted; phase_ = GamePhase::Result; resultAt_ = now;
    outcome_.finished = true; outcome_.valid = false;  // medição interrompida não gera recorde
  }
}

void ReflexGame::framePresented(ms_t t) {
  // Mede a partir da APRESENTAÇÃO do sinal no display, não de quando entrou na fila.
  if (sub_ == Sub::Signal && signalRendered_ && !shownKnown_) { shownKnown_ = true; shownAt_ = t; }
}

void ReflexGame::draw(Framebuffer& fb, ms_t now) {
  (void)now;
  char b[32];
  switch (sub_) {
    case Sub::Ready:
      drawIntro(fb, "TUBA REFLEXO", "Espere o AGORA!", "e toque rápido", best, nullptr);
      if (best) { fb.fillRect(0, 38, 128, 8, false); StrBuf s(b, sizeof b); s.put("Recorde: ").num(best).put(" ms"); fb.textCentered(38, b); }
      else { fb.fillRect(0, 38, 128, 8, false); fb.textCentered(38, "Recorde: --"); }
      break;
    case Sub::Waiting:
      fb.textCentered(22, "ESPERE...", 2);
      fb.textCentered(50, "não toque ainda");
      break;
    case Sub::Signal:
      fb.fillRect(0, 0, 128, 64);
      fb.textCentered(18, "AGORA!", 3, false);
      signalRendered_ = true;
      break;
    case Sub::Done: {
      StrBuf s(b, sizeof b); s.num(lastMs_).put(" ms");
      fb.textCentered(4, b, 2);
      const char* react = lastMs_ < 200 ? "Uau! Relâmpago!" : lastMs_ < 300 ? "Rápido!" : lastMs_ < 450 ? "Boa!" : "Dá pra melhorar";
      fb.textCentered(24, react);
      StrBuf s2(b, sizeof b);
      if (newRecord) s2.put("Novo recorde!"); else { s2.put("Melhor: "); if (best) s2.num(best).put(" ms"); else s2.put("--"); }
      fb.textCentered(36, b);
      drawResultFooter(fb);
      break;
    }
    case Sub::Early:
      fb.textCentered(8, "Cedo demais!", 1);
      fb.textCentered(22, "Rodada inválida");
      fb.textCentered(34, "Espere o AGORA!");
      drawResultFooter(fb);
      break;
    case Sub::Timeout:
      fb.textCentered(12, "Tempo esgotado");
      fb.textCentered(26, "Rodada inválida");
      drawResultFooter(fb);
      break;
    case Sub::Interrupted:
      fb.textCentered(12, "Rodada");
      fb.textCentered(22, "interrompida");
      fb.textCentered(34, "(sem recorde)");
      drawResultFooter(fb);
      break;
  }
}

// ════════════════════════════════════════════════════════════════════════════
// 9.3 Hora do Bote
// ════════════════════════════════════════════════════════════════════════════
bool FishGame::fullyInside(int left) {
  // TODOS os pixels do peixe (bounding box completa, cauda inclusa) no interior útil
  return left >= kInnerX0 && left + SPR_FISH.w - 1 <= kInnerX1;
}

int32_t FishGame::nextSpeed(int32_t v) const {
  int32_t vmax = speedMax * 1000;
  int32_t inc = (vmax - v) * growthPct / 100;
  if (inc < 1000) inc = 1000;  // nunca fica igual antes do teto
  return tbMin(v + inc, vmax);
}

void FishGame::begin(ms_t now, Rng* rng, bool easy) {
  (void)now; (void)easy;
  rng_ = rng; phase_ = GamePhase::Intro; paused_ = false; captures_ = 0;
}

void FishGame::newFish(ms_t gt) { segT0_ = gt; segX0_ = kSpawnX * 1000; hasPrev_ = false; catching_ = false; }

int32_t FishGame::fishXsubAt(ms_t gt) const {
  if (gt >= segT0_ || !hasPrev_) {
    ms_t dt = gt >= segT0_ ? gt - segT0_ : 0;
    return segX0_ + (int32_t)((int64_t)v_ * (int64_t)dt / 1000);
  }
  return prevX0_ + (int32_t)((int64_t)v_ * (int64_t)(gt - prevT0_) / 1000);
}

void FishGame::press(ms_t t, ms_t now) {
  if (paused_) return;
  if (phase_ != GamePhase::Playing) {
    if (phase_ == GamePhase::Result && now - resultAt_ < (ms_t)kResultGuardMs) return;
    phase_ = GamePhase::Playing; pauseAccum_ = 0; captures_ = 0; v_ = speedStart * 1000; lost_ = false;
    newFish(gameTime(now));
    return;
  }
  if (catching_) return;  // animação curta da mordida: toques ignorados, sem penalidade
  ms_t gt = gameTime(t);
  int left = floorDiv1000(fishXsubAt(gt));
  if (fullyInside(left)) {
    captures_++; sfx_ = 1;
    v_ = nextSpeed(v_);
    catching_ = true; catchAt_ = gameTime(now);
  } else {
    lost_ = true; lostX_ = left;
    phase_ = GamePhase::Result; resultAt_ = now; sfx_ = 2;
    outcome_.finished = true; outcome_.valid = true; outcome_.score = captures_;
  }
}

void FishGame::update(ms_t now) {
  if (phase_ != GamePhase::Playing || paused_) return;
  ms_t gt = gameTime(now);
  if (catching_) { if (gt - catchAt_ >= (ms_t)catchAnimMs) newFish(gt); return; }
  // peixe que passa direto continua cruzando a tela (não é erro)
  for (int guard = 0; guard < 8; guard++) {
    int32_t x = fishXsubAt(gt);
    if (x < 128 * 1000) break;
    ms_t cross = segT0_ + (ms_t)((int64_t)(128 * 1000 - segX0_) * 1000 / v_);
    prevT0_ = segT0_; prevX0_ = segX0_; hasPrev_ = true;
    segT0_ = cross; segX0_ = kSpawnX * 1000;
  }
}

void FishGame::draw(Framebuffer& fb, ms_t now) {
  if (phase_ == GamePhase::Intro) {
    drawIntro(fb, "HORA DO BOTE", "Toque com o peixe", "TODO dentro do quadro", best, nullptr);
    return;
  }
  char b[24]; StrBuf sb(b, sizeof b); sb.put("Peixes: ").num(captures_);
  fb.text(0, 0, b);
  fb.rect(kBoxX, kBoxY, kBoxSize, kBoxSize);
  // cantoneiras para destacar o quadro
  fb.hline(kBoxX - 2, kBoxY - 2, 5); fb.vline(kBoxX - 2, kBoxY - 2, 5);
  fb.hline(kBoxX + kBoxSize - 3, kBoxY + kBoxSize + 1, 5); fb.vline(kBoxX + kBoxSize + 1, kBoxY + kBoxSize - 3, 5);
  ms_t gt = gameTime(now);
  if (phase_ == GamePhase::Playing) {
    if (catching_) fb.textCentered(kBoxY + 12, "NHAC!");
    else drawSprite(fb, SPR_FISH, floorDiv1000(fishXsubAt(gt)), kFishY);
  } else if (phase_ == GamePhase::Result) {
    drawSprite(fb, SPR_FISH, lostX_, kFishY);
    fb.fillRect(10, 50 - 12, 108, 12, false);
    char r[32]; StrBuf s(r, sizeof r);
    if (newRecord) s.put("Novo recorde: ").num(captures_); else s.put("Total ").num(captures_).put(" | Rec ").num(best);
    fb.textCentered(42, r);
    fb.fillRect(0, 54, 128, 10, false);
    drawResultFooter(fb);
  }
  if (paused_) drawPausedOverlay(fb);
}

// ════════════════════════════════════════════════════════════════════════════
// 9.4 No Tempo Certo
// ════════════════════════════════════════════════════════════════════════════
int32_t TimingGame::proximity100(int32_t elapsedMs, int32_t targetMs) {
  if (targetMs <= 0) return 0;
  int64_t err = elapsedMs > targetMs ? elapsedMs - targetMs : targetMs - elapsedMs;
  if (err >= targetMs) return 0;
  // 10000 × (alvo − erro) / alvo, arredondado ao centésimo mais próximo (inteiro, sem float)
  int64_t num = (int64_t)10000 * (targetMs - err);
  return (int32_t)((num * 2 + targetMs) / (2 * (int64_t)targetMs));
}

void TimingGame::newTarget() { targetCs_ = rng_->range(kMinCs, kMaxCs); }

void TimingGame::begin(ms_t now, Rng* rng, bool easy) {
  (void)now; (void)easy;
  rng_ = rng; sub_ = Sub::Ready; phase_ = GamePhase::Intro; paused_ = false;
  newTarget();
}

void TimingGame::press(ms_t t, ms_t now) {
  if (paused_) return;
  switch (sub_) {
    case Sub::Ready:
      sub_ = Sub::Countdown; phase_ = GamePhase::Playing; countStart_ = now; goAt_ = now + 3000;
      return;
    case Sub::Countdown:
      return;  // ignorado: não fica pendurado para disparar no VAI
    case Sub::Running: {
      if (t < goAt_) return;
      elapsed_ = (int32_t)(t - goAt_);
      int32_t target = targetCs_ * 10;
      int32_t err = elapsed_ > target ? elapsed_ - target : target - elapsed_;
      int32_t pct = proximity100(elapsed_, target);
      sub_ = Sub::Done; phase_ = GamePhase::Result; resultAt_ = now; sfx_ = 1;
      outcome_.finished = true; outcome_.valid = true; outcome_.score = pct; outcome_.aux = err; outcome_.aux2 = targetCs_;
      return;
    }
    default:
      if (now - resultAt_ < (ms_t)kResultGuardMs) return;
      newTarget();  // novo alvo é MOSTRADO antes de começar
      sub_ = Sub::Ready; phase_ = GamePhase::Intro;
      return;
  }
}

void TimingGame::update(ms_t now) {
  if (paused_) return;
  if (sub_ == Sub::Countdown && now >= goAt_) sub_ = Sub::Running;
  if (sub_ == Sub::Running && now - goAt_ >= (ms_t)kRoundLimitMs) {
    elapsed_ = kRoundLimitMs; sub_ = Sub::Timeout; phase_ = GamePhase::Result; resultAt_ = now;
    outcome_.finished = true; outcome_.valid = false;
  }
}

void TimingGame::pause(ms_t now) {
  Game::pause(now);
  if (sub_ == Sub::Countdown || sub_ == Sub::Running) {
    sub_ = Sub::Interrupted; phase_ = GamePhase::Result; resultAt_ = now;
    outcome_.finished = true; outcome_.valid = false;
  }
}

void TimingGame::draw(Framebuffer& fb, ms_t now) {
  char b[40];
  switch (sub_) {
    case Sub::Ready: {
      fb.textCentered(0, "NO TEMPO CERTO");
      fb.hline(0, 9, 128);
      fb.textCentered(13, "Pare o tempo em:");
      StrBuf s(b, sizeof b); s.fixed2(targetCs_).put(" s");
      fb.textCentered(24, b, 2);
      StrBuf s2(b, sizeof b); s2.put("Recorde: ");
      if (best) s2.fixed2(best).put('%'); else s2.put("--");
      fb.textCentered(42, b);
      fb.hline(0, 54, 128);
      fb.textCentered(56, "Toque: começar");
      break;
    }
    case Sub::Countdown: {
      ms_t left = goAt_ > now ? goAt_ - now : 0;
      int digit = (int)((left + 999) / 1000);
      if (digit < 1) digit = 1;
      if (digit > 3) digit = 3;
      char d[2] = {(char)('0' + digit), 0};
      StrBuf s(b, sizeof b); s.put("Alvo ").fixed2(targetCs_).put(" s");
      fb.textCentered(0, b);
      fb.textCentered(18, d, 4);
      break;
    }
    case Sub::Running: {
      ms_t e = now >= goAt_ ? now - goAt_ : 0;
      StrBuf s(b, sizeof b); s.put("Alvo ").fixed2(targetCs_).put(" s");
      fb.textCentered(0, b);
      StrBuf s2(b, sizeof b); s2.fixed2((int64_t)(e / 10));
      fb.textCentered(20, b, 3);
      if (e < 400) fb.textCentered(48, "VAI!", 2);
      else fb.textCentered(52, "Toque para parar");
      break;
    }
    case Sub::Done: {
      int32_t target = targetCs_ * 10;
      int32_t diff = elapsed_ - target;
      StrBuf s(b, sizeof b); s.put("Alvo ").fixed2(targetCs_).put("  Você ").fixed2(elapsed_ / 10);
      fb.textCentered(0, b);
      int32_t pct = proximity100(elapsed_, target);
      StrBuf s2(b, sizeof b); s2.fixed2(pct).put('%');
      fb.textCentered(12, b, 2);
      StrBuf s3(b, sizeof b); s3.put("Diferença ").put(diff >= 0 ? '+' : '-').fixed2(tbAbs(diff) / 10).put(" s");
      fb.textCentered(32, b);
      StrBuf s4(b, sizeof b);
      if (newRecord) s4.put("Novo recorde!"); else { s4.put("Recorde: "); if (best) s4.fixed2(best).put('%'); else s4.put("--"); }
      fb.textCentered(42, b);
      drawResultFooter(fb);
      break;
    }
    case Sub::Timeout:
      fb.textCentered(12, "Passou de 20 s!");
      fb.textCentered(26, "Rodada encerrada");
      drawResultFooter(fb);
      break;
    case Sub::Interrupted:
      fb.textCentered(12, "Rodada interrompida");
      fb.textCentered(26, "(sem recorde)");
      drawResultFooter(fb);
      break;
  }
}

}  // namespace tb
