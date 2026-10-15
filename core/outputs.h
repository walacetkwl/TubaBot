// Política central de som/vibração do TubaBot v2.
// TODA saída física (abertura, reações, teclas, jogos, timers, Pomodoro,
// lembretes, alarmes, diagnóstico e prévias) passa por OutputManager.
// Agora o som é um alto-falante de verdade (sintetizador em audio.h, com
// volume); a vibração é OPCIONAL (motor MV50 no conector de expansão).
#pragma once
#include "tb_base.h"
#include "audio.h"

namespace tb {

// Recursos físicos presentes (vêm da gravação/detecção, nunca do portal).
struct HwCaps {
  bool speaker = true;   // alto-falante no conector MX1.25 da placa
  bool motor = false;    // módulo de vibração no GPIO16 (opcional)
  bool imu = true;       // QMI8658
  bool mic = true;       // ES7210 + 2 microfones
  bool battery = true;   // AXP2101 (bateria opcional)
  bool rtc = true;       // PCF85063
};

struct AlertPrefs {
  bool sound = true;      // sons habilitados
  bool silent = false;    // modo silencioso (corta som; mantém vibração se habilitada)
  bool vibration = true;  // vibração habilitada (se houver motor)
  uint8_t volume = 60;    // 0..100
};

struct AlertPolicy {
  static bool soundAllowed(const HwCaps& c, const AlertPrefs& p) { return c.speaker && p.sound && !p.silent && p.volume > 0; }
  static bool vibeAllowed(const HwCaps& c, const AlertPrefs& p) { return c.motor && p.vibration; }
};

enum class Pattern : uint8_t {
  None = 0,
  Boot, Tap, Double, Purr, Love, Menu,
  TimerDone, PomodoroDone, Reminder,
  GamePoint, GameOver, Record, Unlock,
  TestShort,
  // novos na v2
  Alarm, Boop, Giggle, Dizzy, Startled, Yawn, Babble, Swipe, Charge, LowBattery, Breath,
  Count
};
const char* patternName(Pattern p);
Pattern patternFromName(const char* s);
bool patternIsAlert(Pattern p);

// Sequência liga/desliga (motor): pares (on_ms, off_ms); termina em {0,0}.
struct Step { uint16_t on, off; };

class Sequencer {
 public:
  void start(const Step* seq, uint8_t repeats, ms_t now, uint32_t maxTotalMs);
  void stop() { active_ = false; level_ = false; }
  bool update(ms_t now);
  bool active() const { return active_; }
  bool level() const { return level_; }
 private:
  const Step* seq_ = nullptr;
  uint8_t repeatsLeft_ = 0, idx_ = 0;
  bool inOff_ = false, active_ = false, level_ = false;
  ms_t phaseStart_ = 0, startedAt_ = 0;
  uint32_t maxTotal_ = 0;
};

class OutputManager {
 public:
  // Teto absoluto de qualquer padrão comum; alarmes têm o seu (2 min, depois só visual).
  static const uint32_t kMaxPatternMs = 6000;
  static const uint32_t kMaxAlarmMs = 120000;

  void configure(const HwCaps& c, const AlertPrefs& p);
  void setPrefs(const AlertPrefs& p);  // corta imediatamente o que deixou de ser permitido
  const HwCaps& caps() const { return caps_; }
  const AlertPrefs& prefs() const { return prefs_; }

  void play(Pattern p, ms_t now, uint32_t seed = 0);
  void stopAll();
  void stopAlerts();
  void update(ms_t now);

  bool motor() const { return vibeOut_; }
  bool soundActive(ms_t now) const { return soundUntil_ > now; }
  Pattern current() const { return current_; }
  bool busy(ms_t now) const { return soundActive(now) || vibe_.active(); }
  uint32_t soundStarts() const { return soundStarts_; }   // contadores para testes/diagnóstico
  uint32_t motorOnCount() const { return vibeOnEdges_; }
  Pattern lastSound() const { return lastSound_; }

  Synth& synth() { return synth_; }

 private:
  HwCaps caps_;
  AlertPrefs prefs_;
  Synth synth_;
  Sequencer vibe_;
  Pattern current_ = Pattern::None, lastSound_ = Pattern::None;
  ms_t soundUntil_ = 0, alertUntil_ = 0;
  bool vibeOut_ = false;
  uint32_t soundStarts_ = 0, vibeOnEdges_ = 0;
};

}  // namespace tb
