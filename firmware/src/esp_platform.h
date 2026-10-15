// Implementação da porta tb::Platform para o ESP32-S3 da ES3C28P (TubaBot v3):
// NVS (Preferences), perfil de fábrica (partição própria), rede própria (AP + DNS
// cativo + mDNS), rede de casa opcional, servidor HTTP com o portal e a API
// compartilhada, OTA local, NTP opcional e clima (HTTPS numa tarefa separada).
#pragma once
#include <Arduino.h>
#include "app.h"
#include "hw.h"

class EspPlatform : public tb::Platform {
 public:
  void begin(Display* screen, Sensors* sensors);
  void attach(tb::App* app) { app_ = app; }
  void loop(uint64_t now);

  // tb::Platform
  bool storageLoad(tb::BlobKind k, uint8_t* buf, size_t cap, size_t* len) override;
  bool storageSave(tb::BlobKind k, const uint8_t* buf, size_t len) override;
  void storageErase(tb::BlobKind k) override;
  void apOpen() override;
  void apClose() override;
  void netStatus(tb::NetStatus& s) override;
  bool wifiSet(const char* ssid, const char* pass) override;
  void wifiForget() override;
  void sysInfo(tb::SysInfo& s) override;
  void requestReboot() override { rebootAt_ = millis() + 400; }
  void applyDisplay(uint8_t brightness, bool rotate180) override;
  bool factoryLoad(char* buf, size_t cap, size_t* len) override;
  bool factoryWrite(const char* text, size_t len);   // usado pelo comando serial "fabrica-gravar"
  bool rotate180() const { return rotate_; }
  void setTimezone(const char* posix) override;
  bool busy() override { return otaActive_ || saving_; }
  void log(const char* msg) override;

  bool apActive() const { return apActive_; }

 private:
  void setupServer();
  void handleApi();
  void handleRoot();
  void handleCaptive();
  void handleUpdateDone();
  void handleUpdateChunk();
  void startSta();
  void radioUpdate();
  bool requestViaAP();

  tb::App* app_ = nullptr;
  Display* screen_ = nullptr;
  Sensors* sensors_ = nullptr;
  bool rotate_ = false;
  void weatherLoop(uint64_t now);
  bool apActive_ = false, serverStarted_ = false, mdnsStarted_ = false;
  char ssid_[33] = {0}, apPass_[16] = {0}, mdns_[32] = {0};
  // rede de casa
  bool staConfigured_ = false;
  char staSsid_[33] = {0};
  tb::StaState sta_ = tb::StaState::Off;
  uint32_t staStartedAt_ = 0, staRetryAt_ = 0, staBackoff_ = 300000;
  // AP
  uint32_t apActivityAt_ = 0;
  // OTA
  bool otaActive_ = false, otaOk_ = false;
  char otaErr_[64] = {0};
  size_t otaWritten_ = 0, otaTotal_ = 0;
  bool saving_ = false;
  uint32_t rebootAt_ = 0;
  // NTP opcional
  bool ntpConfigured_ = false;
  uint32_t ntpLastSync_ = 0;
};
