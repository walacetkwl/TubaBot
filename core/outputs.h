// Política central de som/vibração + sequenciadores liga/desliga não bloqueantes.
// TODA saída física (boot, carinho, teclas, jogos, timers, Pomodoro, lembretes,
// diagnóstico e prévias) passa por OutputManager. O buzzer comprado é ATIVO:
// só liga/desliga, sem controle de frequência/volume.
#pragma once
#include "tb_base.h"

namespace tb {

enum class Model : uint8_t { Lite = 0, Full = 1 };

struct AlertPrefs {
  bool sound = true;      // som habilitado
  bool silent = false;    // modo silencioso (corta som, mantém vibração se habilitada)
  bool vibration = true;  // vibração habilitada
};

// Política (tabela da seção 4 do briefing).
struct AlertPolicy {
  static bool soundAllowed(Model m, const AlertPrefs& p) { return m == Model::Full && p.sound && !p.silent; }
  static bool vibeAllowed(Model m, const AlertPrefs& p) { return m == Model::Full && p.vibration; }
};

enum class Pattern : uint8_t {
  None = 0,
  Boot, Tap, Double, Purr, Love, Menu,
  TimerDone, PomodoroDone, Reminder,
  GamePoint, GameOver, Record, Unlock,
  TestShort,  // diagnóstico/prévia
  Count
};
const char* patternName(Pattern p);
Pattern patternFromName(const char* s);

// Sequência: pares (on_ms, off_ms); termina em {0,0}.
struct Step { uint16_t on, off; };

class Sequencer {
 public:
  void start(const Step* seq, uint8_t repeats, ms_t now, uint32_t maxTotalMs);
  void stop() { active_ = false; level_ = false; }
  bool update(ms_t now);  // devolve nível atual
  bool active() const { return active_; }
  bool level() const { return level_; }
 private:
  const Step* seq_ = nullptr;
  uint8_t repeatsLeft_ = 0;
  uint8_t idx_ = 0;
  bool inOff_ = false;
  bool active_ = false;
  bool level_ = false;
  ms_t phaseStart_ = 0;
  ms_t startedAt_ = 0;
  uint32_t maxTotal_ = 0;
};

class OutputManager {
 public:
  // Duração máxima absoluta de qualquer padrão (proteção contra saída presa).
  static const uint32_t kMaxPatternMs = 6000;

  void configure(Model m, const AlertPrefs& p) { model_ = m; prefs_ = p; }
  void setPrefs(const AlertPrefs& p) { prefs_ = p; }  // corta imediatamente o que deixou de ser permitido
  Model model() const { return model_; }
  const AlertPrefs& prefs() const { return prefs_; }

  void play(Pattern p, ms_t now);
  void stopAll() { buzz_.stop(); vibe_.stop(); current_ = Pattern::None; }
  void update(ms_t now);

  // Níveis lógicos a aplicar nos pinos (o firmware converte polaridade).
  bool buzzer() const { return buzzOut_; }
  bool motor() const { return vibeOut_; }
  Pattern current() const { return current_; }
  bool busy() const { return buzz_.active() || vibe_.active(); }
  uint32_t buzzerOnCount() const { return buzzOnEdges_; }  // contadores para testes/diagnóstico
  uint32_t motorOnCount() const { return vibeOnEdges_; }

 private:
  Model model_ = Model::Lite;
  AlertPrefs prefs_;
  Sequencer buzz_, vibe_;
  Pattern current_ = Pattern::None;
  bool buzzOut_ = false, vibeOut_ = false;
  uint32_t buzzOnEdges_ = 0, vibeOnEdges_ = 0;
};

}  // namespace tb
