// Entrada do TTP223 (interrupção com timestamp) e saídas de buzzer/motor.
#pragma once
#include <Arduino.h>
#include "board_profile.h"

namespace tb { class App; }

// Relógio monotônico de 64 bits em ms (esp_timer não sofre rollover prático).
uint64_t nowMs();

class TouchInput {
 public:
  void begin();
  // Entrega ao núcleo as bordas capturadas pela interrupção, com seus instantes reais.
  void drain(tb::App& app);
  bool rawLevel() const;        // nível lógico atual (tocado = true)
  uint32_t edges() const;
};

class OutputsHw {
 public:
  // Deve ser a PRIMEIRA coisa do setup(): pinos em estado inativo, sem pulsos de teste.
  void begin();
  void write(bool buzzer, bool motor);
 private:
  bool b_ = false, m_ = false;
};
