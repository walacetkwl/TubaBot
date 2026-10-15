// Os cinco minijogos. Regras e física independem da taxa de quadros:
// - Pula, Tuba!: passo fixo de 4 ms em subpixels inteiros (1/1000 px);
// - Hora do Bote: posição analítica do peixe no TIMESTAMP da entrada;
// - Reflexo / No Tempo Certo: relógio monotônico em ms;
// - Mergulho (novo na v2): passo fixo de 10 ms, controle por inclinação ou dedo.
// A lógica dos quatro jogos da v1 é a MESMA (mesmos testes); o desenho foi
// refeito para a tela colorida: o "mundo" 128×64 da v1 é ampliado 3× (GameView).
#pragma once
#include "gfx.h"

namespace tb {

enum class GameId : uint8_t { Jump = 0, Reflex = 1, Fish = 2, Timing = 3, Dive = 4, Count = 5 };
const char* gameTitle(GameId g);

enum class GamePhase : uint8_t { Intro, Playing, Result };

// Resultado reportado ao app quando uma rodada termina.
struct GameOutcome {
  bool finished = false;   // houve uma rodada encerrada neste update
  bool valid = false;      // rodada válida (conta para recorde/conquista)
  int32_t score = 0;       // pontos, ms (reflexo) ou centésimos de % (tempo certo)
  int32_t aux = 0;         // erro absoluto (tempo certo)
  int32_t aux2 = 0;        // alvo em cs (tempo certo)
};

// Mundo lógico 128×64 da v1 -> tela (3×, centralizado).
struct GameView {
  static const int kScale = 3;
  static const int kX0 = SCR_CX - 64 * kScale, kY0 = SCR_CY - 32 * kScale;
  static float X(float lx) { return (float)kX0 + lx * (float)kScale; }
  static float Y(float ly) { return (float)kY0 + ly * (float)kScale; }
  static float S(float v) { return v * (float)kScale; }
};

class Game {
 public:
  virtual void begin(ms_t now, Rng* rng, bool easy) = 0;  // tela de introdução
  virtual void press(ms_t t, ms_t now) = 0;               // borda de toque validada
  virtual void update(ms_t now) = 0;
  virtual void draw(Canvas& c, ms_t now) = 0;
  // Controle contínuo (Mergulho): inclinação −1000..1000 e dedo (x na tela, −1 = sem dedo).
  virtual void setControl(int tilt, int touchX) { (void)tilt; (void)touchX; }
  virtual void pause(ms_t now) { paused_ = true; pausedAt_ = now; }
  virtual void resume(ms_t now) { if (paused_) { pauseAccum_ += now - pausedAt_; paused_ = false; } }
  virtual void framePresented(ms_t t) { (void)t; }
  virtual bool needsFastFrames() const { return phase_ == GamePhase::Playing; }
  GamePhase phase() const { return phase_; }
  bool paused() const { return paused_; }
  // Lê e limpa o último resultado.
  GameOutcome takeOutcome() { GameOutcome o = outcome_; outcome_ = GameOutcome(); return o; }
  // Efeito sonoro/háptico pendente: 0 nenhum, 1 ponto, 2 fim de rodada.
  int takeSfx() { int s = sfx_; sfx_ = 0; return s; }
  int32_t best = 0;       // recorde exibido (o app sincroniza com Progress)
  int32_t bestAux = 0;
  bool newRecord = false; // marcado pelo app ao registrar
 protected:
  ms_t gameTime(ms_t now) const { return (paused_ ? pausedAt_ : now) - pauseAccum_; }
  void drawResultFooter(Canvas& c);
  GamePhase phase_ = GamePhase::Intro;
  bool paused_ = false;
  ms_t pausedAt_ = 0, pauseAccum_ = 0;
  ms_t resultAt_ = 0;
  Rng* rng_ = nullptr;
  GameOutcome outcome_;
  int sfx_ = 0;
  static const int kResultGuardMs = 500;
};

// Sprites em texto: '#' = pixel aceso. Expostos para os testes verificarem o contorno.
struct Sprite { int w, h; const char* const* rows; };
extern const Sprite SPR_SHARK;
extern const Sprite SPR_FISH;
void drawSprite(Canvas& c, const Sprite& s, int x, int y, Color col);

// ── 9.1 Pula, Tuba! ─────────────────────────────────────────────────────────
struct JumpParams {
  int32_t speedStart = 80, speedMax = 104;  // px/s
  int32_t speedStep = 2;                    // +px/s por obstáculo
  int32_t jumpV = 140, gravity = 400;       // px/s, px/s² (briefing propôs 120/280; ver FUNCIONALIDADES.md)
  int32_t bufferMs = 120;                   // buffer de salto antes de aterrissar
  int32_t firstObstacleDelayMs = 1500;
  int32_t reactionMarginMs = 300;           // folga mínima entre obstáculos (além do tempo de voo)
};

class JumpGame : public Game {
 public:
  static const int kStepMs = 4;
  static const int kGroundY = 58;     // linha do chão
  static const int kSharkX = 14;      // posição horizontal fixa do tubarão
  // hitbox honesta, ligeiramente menor que o sprite (em pixels relativos ao sprite)
  static const int kHitX0 = 3, kHitX1 = 13, kHitY0 = 2, kHitY1 = 8;
  static const int kMaxObs = 4;
  JumpParams p;
  struct Obstacle { int32_t x; int16_t w, h; bool scored, active; };

  void begin(ms_t now, Rng* rng, bool easy) override;
  void press(ms_t t, ms_t now) override;
  void update(ms_t now) override;
  void draw(Canvas& c, ms_t now) override;

  // Acesso para testes
  int32_t sharkYsub() const { return y_; }
  bool airborne() const { return y_ > 0 || vy_ != 0; }
  int score() const { return score_; }
  int32_t speed() const { return speed_; }
  const Obstacle* obstacles() const { return obs_; }
  ms_t simTime() const { return simT_; }
  // Tempo de voo (ms) e distância mínima segura entre obstáculos (subpx) para a velocidade dada.
  int32_t airTimeMs() const { return 2000 * p.jumpV / p.gravity; }
  int32_t minGapSub(int32_t speedPx) const;
  bool collides() const;

 private:
  void startPlaying(ms_t now);
  void stepOnce();
  void doJump();
  void spawn();
  int32_t y_ = 0;        // altura acima do chão, subpx (>= 0)
  int32_t vy_ = 0;       // subpx/s (positivo = para cima)
  int32_t speed_ = 0;    // px/s
  int32_t scroll_ = 0;   // subpx (para o cenário)
  ms_t simT_ = 0;        // tempo de jogo já simulado
  ms_t startT_ = 0;
  int32_t nextSpawnIn_ = 0;  // subpx de deslocamento até o próximo obstáculo
  bool buffered_ = false;
  ms_t bufferedAt_ = 0;
  int score_ = 0;
  Obstacle obs_[kMaxObs];
  bool easy_ = true;
};

// ── 9.2 Tuba Reflexo ────────────────────────────────────────────────────────
class ReflexGame : public Game {
 public:
  enum class Sub : uint8_t { Ready, Waiting, Signal, Done, Early, Timeout, Interrupted };
  int32_t waitMinMs = 1500, waitMaxMs = 4000, timeoutMs = 3000;
  void begin(ms_t now, Rng* rng, bool easy) override;
  void press(ms_t t, ms_t now) override;
  void update(ms_t now) override;
  void draw(Canvas& c, ms_t now) override;
  void pause(ms_t now) override;
  void framePresented(ms_t t) override;
  bool needsFastFrames() const override { return sub_ == Sub::Waiting || sub_ == Sub::Signal; }
  Sub sub() const { return sub_; }
  int32_t lastMs() const { return lastMs_; }
  int32_t lastWait() const { return waitMs_; }
  ms_t signalShownAt() const { return shownAt_; }
 private:
  Sub sub_ = Sub::Ready;
  ms_t waitStart_ = 0;
  int32_t waitMs_ = 0;
  bool signalRendered_ = false;  // o quadro "AGORA!" já foi desenhado
  bool shownKnown_ = false;      // e já foi apresentado no display
  ms_t shownAt_ = 0;
  int32_t lastMs_ = 0;
};

// ── 9.3 Hora do Bote ────────────────────────────────────────────────────────
class FishGame : public Game {
 public:
  // Quadrado externo e interior útil (coordenadas inteiras).
  static const int kBoxX = 48, kBoxY = 16, kBoxSize = 32;
  static const int kInnerX0 = kBoxX + 1, kInnerX1 = kBoxX + kBoxSize - 2;  // 49..78
  static const int kFishY = 28;
  static const int kSpawnX = -16;
  int32_t speedStart = 38, speedMax = 115;   // px/s
  int32_t growthPct = 12;                    // aproximação assintótica por captura
  int32_t catchAnimMs = 350;

  void begin(ms_t now, Rng* rng, bool easy) override;
  void press(ms_t t, ms_t now) override;
  void update(ms_t now) override;
  void draw(Canvas& c, ms_t now) override;
  // Testes
  int32_t fishXsubAt(ms_t gameT) const;   // posição (subpx) da borda esquerda do sprite
  static bool fullyInside(int fishLeftPx);
  int captures() const { return captures_; }
  int32_t speedSub() const { return v_; }
  ms_t gameTimeOf(ms_t now) const { return gameTime(now); }
 private:
  void newFish(ms_t gt);
  int32_t nextSpeed(int32_t v) const;
  int captures_ = 0;
  int32_t v_ = 0;        // subpx/s
  ms_t segT0_ = 0;       // início do trecho atual
  int32_t segX0_ = 0;    // x (subpx) no início do trecho
  ms_t prevT0_ = 0; int32_t prevX0_ = 0; bool hasPrev_ = false;
  ms_t catchAt_ = 0;
  bool catching_ = false;
  bool lost_ = false;
  int32_t lostX_ = 0;
};

// ── 9.4 No Tempo Certo ──────────────────────────────────────────────────────
class TimingGame : public Game {
 public:
  enum class Sub : uint8_t { Ready, Countdown, Running, Done, Timeout, Interrupted };
  static const int kMinCs = 300, kMaxCs = 800;
  static const int32_t kRoundLimitMs = 20000;
  void begin(ms_t now, Rng* rng, bool easy) override;
  void press(ms_t t, ms_t now) override;
  void update(ms_t now) override;
  void draw(Canvas& c, ms_t now) override;
  void pause(ms_t now) override;
  bool needsFastFrames() const override { return sub_ == Sub::Countdown || sub_ == Sub::Running; }
  // proximidade em centésimos de % (0..10000), arredondada: 100 × (1 − erro/alvo)
  static int32_t proximity100(int32_t elapsedMs, int32_t targetMs);
  Sub sub() const { return sub_; }
  int32_t targetCs() const { return targetCs_; }
  int32_t elapsedMs() const { return elapsed_; }
  void forceTarget(int32_t cs) { targetCs_ = cs; }  // testes
  ms_t goAt() const { return goAt_; }
 private:
  void newTarget();
  Sub sub_ = Sub::Ready;
  int32_t targetCs_ = 500;
  ms_t countStart_ = 0, goAt_ = 0;
  int32_t elapsed_ = 0;
};

// ── Mergulho do Tuba (novo) ─────────────────────────────────────────────────
// Peixes e águas-vivas descem; o Tuba nada no fundo, guiado pela inclinação
// (IMU) ou pelo dedo. Peixe = 1 ponto; água-viva = perde uma vida (3 vidas).
// Justiça: nunca nascem duas águas-vivas que bloqueiem toda a passagem.
class DiveGame : public Game {
 public:
  static const int kW = 100, kH = 100;     // mundo lógico (px)
  static const int kStepMs = 10;
  static const int kSharkY = 88;           // centro do Tuba
  static const int kCatchDx = 11, kCatchDy = 7;
  static const int kMaxItems = 8;
  static const int kLives = 3;
  static const int kViewScale = 3;
  static const int kViewX0 = SCR_CX - kW * kViewScale / 2, kViewY0 = SCR_CY - kH * kViewScale / 2;
  struct Item { int32_t x, y; int16_t vy; bool jelly, active; };

  void begin(ms_t now, Rng* rng, bool easy) override;
  void press(ms_t t, ms_t now) override;
  void update(ms_t now) override;
  void draw(Canvas& c, ms_t now) override;
  void setControl(int tilt, int touchX) override { tilt_ = tilt; touchX_ = touchX; }

  int score() const { return score_; }
  int lives() const { return lives_; }
  int32_t sharkX() const { return sharkX_; }   // subpx (×1000)
  const Item* items() const { return items_; }
  ms_t simTime() const { return simT_; }
  int spawnIntervalMs() const;
  int fallSpeed() const;  // px/s

 private:
  void startPlaying(ms_t now);
  void stepOnce();
  void spawn();
  Item items_[kMaxItems];
  int32_t sharkX_ = 50000;
  int tilt_ = 0, touchX_ = -1;
  int score_ = 0, lives_ = kLives;
  ms_t simT_ = 0, startT_ = 0, nextSpawn_ = 0, invulnUntil_ = 0, lastJellyT_ = 0;
  int32_t lastJellyX_ = -100000;
  int flash_ = 0;
};

}  // namespace tb
