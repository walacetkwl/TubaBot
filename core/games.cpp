#include "games.h"

namespace tb {

const char* gameTitle(GameId g) {
  switch (g) {
    case GameId::Jump: return "Pula, Tuba!";
    case GameId::Reflex: return "Tuba Reflexo";
    case GameId::Fish: return "Hora do Bote";
    case GameId::Timing: return "No Tempo Certo";
    case GameId::Dive: return "Mergulho do Tuba";
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

int32_t floorDiv1000(int32_t v) { return v >= 0 ? v / 1000 : -((-v + 999) / 1000); }

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

// ════════════════════════════════════════════════════════════════════════════
// Mergulho do Tuba (novo na v2)
// ════════════════════════════════════════════════════════════════════════════
int DiveGame::spawnIntervalMs() const { return tbMax(380, 900 - score_ * 20); }
int DiveGame::fallSpeed() const { return tbMin(85, 35 + score_ * 2); }

void DiveGame::begin(ms_t now, Rng* rng, bool easy) {
  (void)now; (void)easy;
  rng_ = rng; phase_ = GamePhase::Intro; paused_ = false;
  score_ = 0; lives_ = kLives; sharkX_ = kW * 500;
  for (auto& it : items_) it.active = false;
}

void DiveGame::startPlaying(ms_t now) {
  phase_ = GamePhase::Playing;
  pauseAccum_ = 0; paused_ = false;
  startT_ = gameTime(now);
  simT_ = 0; score_ = 0; lives_ = kLives; sharkX_ = kW * 500;
  for (auto& it : items_) it.active = false;
  nextSpawn_ = 800; invulnUntil_ = 0; lastJellyT_ = 0; lastJellyX_ = -100000; flash_ = 0;
}

void DiveGame::press(ms_t t, ms_t now) {
  (void)t;
  if (paused_) return;
  if (phase_ == GamePhase::Intro) { startPlaying(now); return; }
  if (phase_ == GamePhase::Result && now - resultAt_ >= (ms_t)kResultGuardMs) startPlaying(now);
  // jogando: o controle é contínuo (inclinação/dedo), o toque não é uma ação
}

void DiveGame::spawn() {
  int pct = score_ < 3 ? 0 : (score_ < 10 ? 25 : 35);
  bool jelly = rng_->range(0, 99) < pct;
  int32_t x = rng_->range(8, kW - 8) * 1000;
  // justiça: duas águas-vivas seguidas nunca fecham a passagem perto uma da outra
  if (jelly && simT_ - lastJellyT_ < 700 && tbAbs(x - lastJellyX_) < 34 * 1000) jelly = false;
  for (auto& it : items_) {
    if (it.active) continue;
    it.active = true; it.jelly = jelly; it.x = x; it.y = -6 * 1000;
    it.vy = (int16_t)(fallSpeed() + (jelly ? -5 : 0));
    if (jelly) { lastJellyT_ = simT_; lastJellyX_ = x; }
    return;
  }
}

void DiveGame::stepOnce() {
  // controle: dedo leva o Tuba até o ponto; inclinação dá a velocidade
  int32_t lim0 = kCatchDx * 1000, lim1 = (kW - kCatchDx) * 1000;
  if (touchX_ >= 0) {
    int32_t target = (int32_t)((int64_t)(touchX_ - kViewX0) * 1000000 / kViewScaleMilli);
    int32_t maxStep = 160 * kStepMs;  // 160 px/s
    int32_t d = target - sharkX_;
    sharkX_ += tbClamp(d, -maxStep, maxStep);
  } else {
    sharkX_ += (int32_t)tbClamp(tilt_, -1000, 1000) * 150 / 1000 * kStepMs;
  }
  sharkX_ = tbClamp(sharkX_, lim0, lim1);
  for (auto& it : items_) {
    if (!it.active) continue;
    it.y += (int32_t)it.vy * kStepMs;
    if (it.y > (kH + 8) * 1000) { it.active = false; continue; }
    if (tbAbs(it.x - sharkX_) <= kCatchDx * 1000 && tbAbs(it.y - kSharkY * 1000) <= kCatchDy * 1000) {
      it.active = false;
      if (!it.jelly) { score_++; sfx_ = 1; }
      else if (simT_ >= invulnUntil_) {
        lives_--; invulnUntil_ = simT_ + 1000; flash_ = 1;
        if (lives_ <= 0) {
          phase_ = GamePhase::Result; sfx_ = 2;
          outcome_.finished = true; outcome_.valid = true; outcome_.score = score_;
          return;
        }
        sfx_ = 3;
      }
    }
  }
  if (simT_ >= nextSpawn_) { spawn(); nextSpawn_ = simT_ + (ms_t)spawnIntervalMs(); }
}

void DiveGame::update(ms_t now) {
  if (phase_ != GamePhase::Playing || paused_) return;
  ms_t target = gameTime(now) - startT_;
  int guard = 0;
  while (simT_ + kStepMs <= target && phase_ == GamePhase::Playing && guard++ < 2000) { stepOnce(); simT_ += kStepMs; }
  if (phase_ == GamePhase::Result) resultAt_ = now;
}

}  // namespace tb