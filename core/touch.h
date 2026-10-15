// Entrada do TubaBot: tela de toque capacitiva (FT6336G da ES3C28P, 320×240 em paisagem).
// Substitui o TTP223 da cabeça: agora o toque tem POSIÇÃO e MOVIMENTO.
//
// Amostras brutas (tocando?, x, y, t) -> eventos. A classificação ao soltar é
// EXCLUSIVA (uma interação vira no máximo UM destes: toque, duplo toque,
// toque longo solto, deslize, carinho ou recuperação). Durante a pressão há
// eventos contínuos (Down/Move/LongPress/PetStroke) para retorno imediato.
#pragma once
#include "tb_base.h"

namespace tb {

enum class TouchEv : uint8_t {
  None,
  Down,          // dedo encostou (sempre, com posição)
  Move,          // dedo moveu (posição atual; dx/dy desde o Down)
  Up,            // dedo saiu (sempre; vx/vy = velocidade de saída em px/s)
  Tap,           // toque curto parado
  DoubleTap,     // dois toques curtos (só no modo UiDouble)
  LongPress,     // segurou parado >= longMs (dispara DURANTE a pressão)
  LongRelease,   // soltou depois de um LongPress (holdMs em 'hold')
  SwipeLeft, SwipeRight, SwipeUp, SwipeDown,
  PetStroke,     // carinho em andamento: a cada petStepPx de trajeto lento
  Pet,           // soltou depois de um carinho
  RecoveryArmed, // segurou >= recoveryMs e soltou -> pedir confirmação
  GamePress,     // modo jogo: borda imediata (instante real do toque)
  GameHoldExit,  // modo jogo: segurou gameExitMs parado
  StuckDetected, // controlador relatando toque contínuo por tempo demais
};

struct TouchEvent {
  TouchEv type = TouchEv::None;
  ms_t t = 0;
  int16_t x = 0, y = 0;      // posição atual
  int16_t x0 = 0, y0 = 0;    // posição do Down
  int16_t vx = 0, vy = 0;    // velocidade (px/s) no Up
  uint32_t hold = 0;         // duração da pressão (ms) no LongRelease/Pet
};

struct TouchTiming {
  uint16_t releaseDebounceMs = 40;  // une falhas curtas do controlador
  uint16_t tapMaxMs = 350;
  uint16_t tapSlopPx = 12;          // movimento máximo para ainda ser "parado"
  uint16_t doubleWindowMs = 300;
  uint16_t longMs = 600;
  uint16_t recoveryMs = 8000;
  uint16_t gameExitMs = 1500;
  uint16_t stuckMs = 20000;
  uint16_t swipeMinPx = 40;
  uint16_t swipeMaxMs = 650;
  uint16_t petMinMs = 350;          // movimento mais lento que isso é carinho, não deslize
  uint16_t petStepPx = 70;
};

enum class TouchMode : uint8_t { Ui, UiDouble, Game };

class TouchClassifier {
 public:
  TouchTiming timing;

  void reset();
  void setMode(TouchMode m);
  TouchMode mode() const { return mode_; }
  void sample(bool down, int x, int y, ms_t t);
  int poll(ms_t now, TouchEvent* out, int maxOut);
  void consumeCurrent(bool keepRecovery);

  bool pressed() const { return down_; }
  ms_t holdMs(ms_t now) const { return down_ ? now - pressAt_ : 0; }
  bool moved() const { return moved_; }
  int x() const { return x_; }
  int y() const { return y_; }
  int downX() const { return x0_; }
  int downY() const { return y0_; }
  bool stuck() const { return stuck_; }
  bool petting() const { return petting_; }
  uint32_t rawEdges() const { return edges_; }

 private:
  void push(TouchEv e, ms_t t);
  void onPress(ms_t t);
  void onRelease(ms_t t);
  void step(ms_t now);

  TouchEvent q_[16];
  int qn_ = 0;
  TouchMode mode_ = TouchMode::Ui;
  // bruto
  bool raw_ = false;
  ms_t rawUpAt_ = 0;
  // estável
  bool down_ = false;
  ms_t pressAt_ = 0;
  int x_ = 0, y_ = 0, x0_ = 0, y0_ = 0, lastMoveX_ = 0, lastMoveY_ = 0;
  bool moved_ = false, longFired_ = false, petting_ = false;
  uint32_t path_ = 0, petPathMark_ = 0;
  bool consumed_ = false, consumedKeepRecovery_ = false;
  bool gameExitFired_ = false, gameIgnore_ = false;
  bool stuck_ = false, stuckReported_ = false;
  // duplo toque
  bool pendingTap_ = false;
  ms_t pendingTapAt_ = 0;
  int pendX_ = 0, pendY_ = 0;
  // velocidade
  struct Sm { ms_t t; int16_t x, y; };
  Sm hist_[6];
  int histN_ = 0;
  uint32_t edges_ = 0;
  int vx_ = 0, vy_ = 0;
  uint32_t lastHold_ = 0;
};

}  // namespace tb
