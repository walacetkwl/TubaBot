// Adaptadores de hardware da Waveshare ESP32-S3-Touch-AMOLED-1.75.
// Só leitura/escrita de periféricos: nenhuma regra do TubaBot mora aqui.
#pragma once
#include <Arduino.h>
#include "app.h"

uint64_t nowMs();

// ── Tela AMOLED (CO5300 via QSPI) ──
class Display {
 public:
  bool begin();
  // Envia a área alterada do framebuffer (RGB565 nativo, 466×466). rotate180 gira por software.
  bool push(const uint16_t* fb, const tb::IRect& r, bool rotate180);
  void setBrightness(uint8_t b);   // 0 = desliga o painel
  bool ok() const { return ok_; }
  uint32_t lastPushUs() const { return lastUs_; }
  void testPattern();
 private:
  bool ok_ = false, on_ = true;
  uint8_t bright_ = 255;
  uint32_t lastUs_ = 0;
};

// ── Toque, movimento, energia, RTC, botões ──
class Sensors {
 public:
  void begin();
  void poll(tb::App& app, uint64_t now);   // chame a cada iteração do laço
  bool touchOk() const { return touchOk_; }
  bool imuOk() const { return imuOk_; }
  bool pmuOk() const { return pmuOk_; }
  bool rtcOk() const { return rtcOk_; }
  bool rtcRead(int64_t& epoch);
  void rtcWrite(int64_t epoch);
  int lastX() const { return lx_; }
  int lastY() const { return ly_; }
  bool touching() const { return touching_; }
  void lastAccel(int& x, int& y, int& z) const { x = ax_; y = ay_; z = az_; }
  static int scanI2C(uint8_t* found, int max);
 private:
  bool touchOk_ = false, imuOk_ = false, pmuOk_ = false, rtcOk_ = false, expOk_ = false;
  bool touching_ = false;
  int lx_ = 0, ly_ = 0, ax_ = 0, ay_ = 0, az_ = 0;
  uint64_t nextTouch_ = 0, nextImu_ = 0, nextPmu_ = 0, nextBtn_ = 0;
  uint64_t bootDownAt_ = 0, pwrDownAt_ = 0;
  bool bootLongSent_ = false;
};

// ── Áudio: ES8311 (alto-falante) + ES7210 (microfones) em I²S full-duplex ──
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

// ── Motor de vibração opcional ──
void motorBegin();
void motorWrite(bool on);
