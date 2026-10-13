#include "hw_io.h"
#include "app.h"
#include <esp_timer.h>

uint64_t nowMs() { return (uint64_t)(esp_timer_get_time() / 1000); }

// ── TTP223 ──────────────────────────────────────────────────────────────────
struct Edge { uint64_t t; bool level; };
static const int kRing = 32;
static volatile Edge ring[kRing];
static volatile uint32_t head = 0, tail = 0, edgeCount = 0;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static inline bool readLevel() {
  int v = digitalRead(TB_PIN_TOUCH);
  return TB_TOUCH_ACTIVE_HIGH ? v == HIGH : v == LOW;
}

static void IRAM_ATTR onTouchEdge() {
  uint64_t t = (uint64_t)(esp_timer_get_time() / 1000);
  bool lv = readLevel();
  portENTER_CRITICAL_ISR(&mux);
  uint32_t n = (head + 1) % kRing;
  if (n != tail) { ring[head].t = t; ring[head].level = lv; head = n; }  // fila cheia: descarta (o próximo poll ressincroniza)
  edgeCount++;
  portEXIT_CRITICAL_ISR(&mux);
}

void TouchInput::begin() {
  // TTP223 tem saída ativa (push-pull); o pull evita flutuação se o fio soltar.
  pinMode(TB_PIN_TOUCH, TB_TOUCH_ACTIVE_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TB_PIN_TOUCH), onTouchEdge, CHANGE);
}

void TouchInput::drain(tb::App& app) {
  for (;;) {
    Edge e;
    portENTER_CRITICAL(&mux);
    bool has = tail != head;
    if (has) { e.t = ring[tail].t; e.level = ring[tail].level; tail = (tail + 1) % kRing; }
    portEXIT_CRITICAL(&mux);
    if (!has) break;
    app.touchSample(e.level, e.t);
  }
  // amostra de segurança: corrige uma borda perdida (fila cheia/ruído)
  app.touchSample(readLevel(), nowMs());
}

bool TouchInput::rawLevel() const { return readLevel(); }
uint32_t TouchInput::edges() const { return edgeCount; }

// ── Buzzer ativo e motor ───────────────────────────────────────────────────
static inline void writePin(int pin, bool on, bool activeHigh) { digitalWrite(pin, (on == activeHigh) ? HIGH : LOW); }

void OutputsHw::begin() {
#if TB_HAS_BUZZER
  writePin(TB_PIN_BUZZER, false, TB_BUZZER_ACTIVE_HIGH);  // nível inativo antes de virar saída
  pinMode(TB_PIN_BUZZER, OUTPUT);
  writePin(TB_PIN_BUZZER, false, TB_BUZZER_ACTIVE_HIGH);
#endif
#if TB_HAS_MOTOR
  writePin(TB_PIN_MOTOR, false, TB_MOTOR_ACTIVE_HIGH);
  pinMode(TB_PIN_MOTOR, OUTPUT);
  writePin(TB_PIN_MOTOR, false, TB_MOTOR_ACTIVE_HIGH);
#endif
  // Lite: esses pinos NÃO são configurados como saída (hardware ausente).
}

void OutputsHw::write(bool buzzer, bool motor) {
#if TB_HAS_BUZZER
  if (buzzer != b_) { writePin(TB_PIN_BUZZER, buzzer, TB_BUZZER_ACTIVE_HIGH); b_ = buzzer; }
#else
  (void)buzzer;
#endif
#if TB_HAS_MOTOR
  if (motor != m_) { writePin(TB_PIN_MOTOR, motor, TB_MOTOR_ACTIVE_HIGH); m_ = motor; }
#else
  (void)motor;
#endif
}
