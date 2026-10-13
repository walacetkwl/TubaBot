// Plataforma falsa para testes nativos (armazenamento em memória, rede simulada).
#pragma once
#include "../core/app.h"
#include <cstring>
#include <cstdio>

struct FakePlatform : public tb::Platform {
  uint8_t store[4][600];
  size_t storeLen[4] = {0, 0, 0, 0};
  bool apActive = false;
  int apOpens = 0, apCloses = 0, reboots = 0, saves = 0;
  bool staConfigured = false;
  char staSsid[33] = {0};
  char staPassStored[65] = {0};
  bool failSaves = false;
  bool quiet = true;

  bool storageLoad(tb::BlobKind k, uint8_t* buf, size_t cap, size_t* len) override {
    int i = (int)k;
    if (!storeLen[i] || storeLen[i] > cap) return false;
    memcpy(buf, store[i], storeLen[i]); *len = storeLen[i]; return true;
  }
  bool storageSave(tb::BlobKind k, const uint8_t* buf, size_t len) override {
    if (failSaves || len > sizeof store[0]) return false;
    memcpy(store[(int)k], buf, len); storeLen[(int)k] = len; saves++; return true;
  }
  void storageErase(tb::BlobKind k) override { storeLen[(int)k] = 0; }
  void apOpen() override { apActive = true; apOpens++; }
  void apClose() override { apActive = false; apCloses++; }
  void netStatus(tb::NetStatus& s) override {
    s = tb::NetStatus();
    s.apActive = apActive;
    strcpy(s.apSsid, "TubaBot-AB12"); strcpy(s.apPass, "tuba-7f3k-29qd"); strcpy(s.apIp, "192.168.4.1"); strcpy(s.mdns, "tubabot-ab12.local");
    s.staConfigured = staConfigured; strcpy(s.staSsid, staSsid);
    s.sta = staConfigured ? tb::StaState::Failed : tb::StaState::Off;
  }
  bool wifiSet(const char* ssid, const char* pass) override {
    staConfigured = true; strncpy(staSsid, ssid, 32); strncpy(staPassStored, pass, 64); return true;
  }
  void wifiForget() override { staConfigured = false; staSsid[0] = 0; staPassStored[0] = 0; }
  void sysInfo(tb::SysInfo& s) override { s = tb::SysInfo(); s.simulated = true; }
  void requestReboot() override { reboots++; }
  void applyDisplay(uint8_t, bool) override {}
  void setTimezone(const char*) override {}
  bool busy() override { return false; }
  void log(const char* m) override { if (!quiet) printf("  [log] %s\n", m); }
};
