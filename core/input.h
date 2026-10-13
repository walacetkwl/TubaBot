// Entrada do TTP223: amostras brutas -> debounce -> bordas -> classificação.
// Um único sensor. A classificação é EXCLUSIVA: cada interação gera no máximo
// um evento (simples OU duplo OU médio OU longo OU recuperação).
#pragma once
#include "tb_base.h"

namespace tb {

enum class Gesture : uint8_t {
  None,
  Tap,            // toque curto (emitido após a janela de duplo toque)
  DoubleTap,      // dois toques curtos dentro da janela
  Medium,         // segurou >= medium e soltou antes do longo
  Long,           // segurou >= long e soltou antes da recuperação
  RecoveryArmed,  // segurou >= recovery e soltou -> aguardando toque de confirmação
  // modo jogo
  GamePress,      // borda de toque imediata (timestamp da borda real)
  GameHoldExit,   // segurou o tempo de saída durante o jogo
  // diagnóstico
  StuckDetected,  // sensor ficou pressionado tempo demais; eventos suspensos até soltar
};

struct GestureEvent {
  Gesture g = Gesture::None;
  ms_t t = 0;  // instante da borda que originou o evento
};

struct InputTiming {
  uint16_t debounceMs = 20;
  uint16_t doubleWindowMs = 320;
  uint16_t mediumMs = 1000;
  uint16_t longMs = 3000;
  uint16_t recoveryMs = 8000;
  uint16_t gameExitMs = 1500;
  uint16_t stuckMs = 20000;
};

enum class InputMode : uint8_t { Normal, Game };

// Estado da pressão em andamento, para prévias visuais.
enum class HoldStage : uint8_t { None, Short, Medium, Long, Recovery, Stuck };

class GestureClassifier {
 public:
  InputTiming timing;

  void reset();
  void setMode(InputMode m, ms_t now);
  InputMode mode() const { return mode_; }

  // Amostra bruta (já com polaridade corrigida: true = tocado).
  void sample(bool level, ms_t t);
  // Avança o relógio e devolve eventos (até 4 por chamada).
  int poll(ms_t now, GestureEvent* out, int maxOut);

  // Descarta a interação atual (ex.: toque que acordou a tela).
  // Se keepRecovery=true, segurar até a recuperação ainda vale.
  void consumeCurrent(bool keepRecovery);

  bool pressed() const { return stable_; }
  ms_t holdMs(ms_t now) const { return stable_ ? now - pressStart_ : 0; }
  HoldStage holdStage(ms_t now) const;
  bool stuck() const { return stuck_; }
  uint32_t rawEdges() const { return rawEdges_; }

 private:
  void onPress(ms_t t);
  void onRelease(ms_t t, GestureEvent* out, int& n, int maxOut);
  void push(GestureEvent* out, int& n, int maxOut, Gesture g, ms_t t);
  void step(ms_t now);  // processa até 'now', enfileirando eventos internamente
  GestureEvent q_[8];
  int qn_ = 0;

  InputMode mode_ = InputMode::Normal;
  bool raw_ = false;
  ms_t rawChange_ = 0;
  bool stable_ = false;
  ms_t pressStart_ = 0;
  // Normal
  bool pendingTap_ = false;
  ms_t pendingTapAt_ = 0;   // instante da soltura do 1º toque
  ms_t pendingTapPress_ = 0;
  bool consumed_ = false;
  bool consumedKeepRecovery_ = false;
  // Jogo
  bool gameExitFired_ = false;
  bool gameIgnoreUntilRelease_ = false;
  // Sensor preso
  bool stuck_ = false;
  bool stuckReported_ = false;
  uint32_t rawEdges_ = 0;
};

}  // namespace tb
