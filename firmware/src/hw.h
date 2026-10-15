// Adaptadores de hardware da ES3C28P (TubaBot v3).
// Só leitura/escrita de periféricos: nenhuma regra do TubaBot mora aqui.
#pragma once
#include <Arduino.h>
#include "app.h"

uint64_t nowMs();

// ── Tela ILI9341V (SPI), em paisagem 320×240 ──
class Display {
 public:
  bool begin();
  // Envia a área alterada do framebuffer (RGB565, 320×240).
  bool push(const uint16_t* fb, const tb::IRect& r);
  void setBrightness(uint8_t b);   // 0 = luz de fundo desligada
  void setFlipped(bool flip);      // "Girar 180°": rotação 3 em vez de 1 (repinta tudo)
  bool ok() const { return ok_; }
  uint32_t lastPushUs() const { return lastUs_; }
 private:
  bool ok_ = false, flipped_ = false, fullNext_ = true;
  uint8_t bright_ = 255;
  uint32_t lastUs_ = 0;
};

// ── Toque, bateria, botão ──
class Sensors {
 public:
  void begin();
  void poll(tb::App& app, uint64_t now);   // chame a cada iteração do laço
  bool touchOk() const { return touchOk_; }
  int lastX() const { return lx_; }
  int lastY() const { return ly_; }
  int rawX() const { return rx_; }
  int rawY() const { return ry_; }
  bool touching() const { return touching_; }
  int batteryMv() const { return batMv_; }
  static int scanI2C(uint8_t* found, int max);
 private:
  bool readTouch(int& x, int& y);
  bool touchOk_ = false, touching_ = false;
  int lx_ = 0, ly_ = 0, rx_ = 0, ry_ = 0;
  int batMv_ = 0;
  float batFilt_ = 0.f;
  uint64_t nextTouch_ = 0, nextBat_ = 0, nextBtn_ = 0;
  uint64_t bootDownAt_ = 0;
  bool bootLongSent_ = false;
};

// ── Áudio: ES8311 (alto-falante + microfone) em I²S full-duplex ──
class Audio {
 public:
  bool begin(tb::App* app);   // cria a tarefa de áudio
  bool speakerOk() const { return spkOk_; }
  bool micOk() const { return micOk_; }
  int micLevel() const { return level_; }   // 0..100 (≈ dBFS + 100), −1 sem leitura
 private:
  static void task(void* arg);
  tb::App* app_ = nullptr;
  bool spkOk_ = false, micOk_ = false;
  volatile int level_ = -1;
};

void ledOff();
