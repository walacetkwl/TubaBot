// Ponte WebAssembly: o MESMO núcleo C++ (core/) compilado para o navegador.
// O JavaScript só apresenta o framebuffer e encaminha entradas; regras,
// jogos, tempo e desenho acontecem aqui.
#include "../core/app.h"

extern "C" {
// Importações fornecidas pelo sim.js
__attribute__((import_name("js_storage_load"))) int js_storage_load(int kind, uint8_t* buf, int cap);
__attribute__((import_name("js_storage_save"))) int js_storage_save(int kind, const uint8_t* buf, int len);
__attribute__((import_name("js_storage_erase"))) void js_storage_erase(int kind);
__attribute__((import_name("js_log"))) void js_log(const char* msg);
__attribute__((import_name("js_reboot"))) void js_reboot();
__attribute__((import_name("js_display"))) void js_display(int contrast, int rotate);
void __cxa_pure_virtual() { __builtin_trap(); }
}

inline void* operator new(size_t, void* p) noexcept { return p; }

using namespace tb;

class SimPlatform : public Platform {
 public:
  bool apActive = false, staConfigured = false, wifiBroken = false;
  char staSsid[33] = {0};
  uint32_t serial = 0xAB12;

  bool storageLoad(BlobKind k, uint8_t* buf, size_t cap, size_t* len) override {
    int n = js_storage_load((int)k, buf, (int)cap);
    if (n <= 0) return false;
    *len = (size_t)n; return true;
  }
  bool storageSave(BlobKind k, const uint8_t* buf, size_t len) override { return js_storage_save((int)k, buf, (int)len) == 1; }
  void storageErase(BlobKind k) override { js_storage_erase((int)k); }
  void apOpen() override { apActive = true; js_log("[rede] rede própria aberta (simulada)"); }
  void apClose() override { apActive = false; js_log("[rede] rede própria fechada (simulada)"); }
  void netStatus(NetStatus& s) override {
    s = NetStatus();
    s.apActive = apActive; s.radioOn = apActive || staConfigured;
    StrBuf a(s.apSsid, sizeof s.apSsid); a.put("TubaBot-").hex(serial, 4);
    StrBuf p(s.apPass, sizeof s.apPass); p.put("sim-").hex(serial * 2654435761u, 8);
    tbStrlcpy(s.apIp, "192.168.4.1", sizeof s.apIp);
    StrBuf m(s.mdns, sizeof s.mdns); m.put("tubabot-").hex(serial, 4).put(".local");
    for (char* c = s.mdns; *c; c++) if (*c >= 'A' && *c <= 'Z') *c = (char)(*c + 32);
    s.apSecondsLeft = 0;
    s.staConfigured = staConfigured;
    tbStrlcpy(s.staSsid, staSsid, sizeof s.staSsid);
    s.sta = !staConfigured ? StaState::Off : (wifiBroken ? StaState::Failed : StaState::Connected);
    if (s.sta == StaState::Connected) tbStrlcpy(s.staIp, "192.168.0.42", sizeof s.staIp);
  }
  bool wifiSet(const char* ssid, const char*) override {
    staConfigured = true; tbStrlcpy(staSsid, ssid, sizeof staSsid);
    js_log("[rede] rede doméstica salva (senha não é exibida)");
    return true;
  }
  void wifiForget() override { staConfigured = false; staSsid[0] = 0; }
  void sysInfo(SysInfo& s) override { s = SysInfo(); s.simulated = true; tbStrlcpy(s.chip, "Simulador WASM", sizeof s.chip); tbStrlcpy(s.build, "sim", sizeof s.build); }
  void requestReboot() override { js_reboot(); }
  void applyDisplay(uint8_t contrast, bool rotate) override { js_display(contrast, rotate ? 1 : 0); }
  void setTimezone(const char*) override {}
  bool busy() override { return false; }
  void log(const char* msg) override { js_log(msg); }
};

alignas(SimPlatform) static unsigned char platMem[sizeof(SimPlatform)];
alignas(App) static unsigned char appMem[sizeof(App)];
static SimPlatform* plat = nullptr;
static App* app = nullptr;
static char inBuf[16384];
static char outBuf[24576];
static char pathBuf[128], methodBuf[8], tokenBuf[40];

#define EXPORT(n) __attribute__((export_name(n)))

extern "C" {
EXPORT("sim_init") void sim_init(int model, uint32_t seed, int dev, double now) {
  bool keepNet = plat != nullptr;
  SimPlatform saved;
  if (keepNet) { saved.apActive = plat->apActive; saved.staConfigured = plat->staConfigured; saved.wifiBroken = plat->wifiBroken; tbStrlcpy(saved.staSsid, plat->staSsid, sizeof saved.staSsid); }
  plat = new (platMem) SimPlatform();
  if (keepNet) { plat->staConfigured = saved.staConfigured; plat->wifiBroken = saved.wifiBroken; tbStrlcpy(plat->staSsid, saved.staSsid, sizeof plat->staSsid); }
  app = new (appMem) App(*plat);
  app->begin(model ? Model::Full : Model::Lite, (ms_t)now, seed, dev != 0);
}
EXPORT("sim_touch") void sim_touch(int level, double t) { app->touchSample(level != 0, (ms_t)t); }
EXPORT("sim_tick") int sim_tick(double now) { return app->tick((ms_t)now) ? 1 : 0; }
EXPORT("sim_presented") void sim_presented(double t) { app->framePresented((ms_t)t); }
EXPORT("sim_frame") const uint8_t* sim_frame() { return app->frame().buf; }
EXPORT("sim_buzzer") int sim_buzzer() { return app->buzzer() ? 1 : 0; }
EXPORT("sim_motor") int sim_motor() { return app->motor() ? 1 : 0; }
EXPORT("sim_screen") int sim_screen() { return (int)app->screen(); }
EXPORT("sim_screen_name") const char* sim_screen_name() { return screenName(app->screen()); }
EXPORT("sim_expr_name") const char* sim_expr_name() { return exprName(app->face().expression()); }
EXPORT("sim_hold_ms") double sim_hold_ms(double now) { return (double)app->input().holdMs((ms_t)now); }
EXPORT("sim_pressed") int sim_pressed() { return app->input().pressed() ? 1 : 0; }
EXPORT("sim_frame_interval") int sim_frame_interval(double now) { return (int)app->frameIntervalMs((ms_t)now); }
EXPORT("sim_in_game") int sim_in_game() { return app->screen() == Screen::Game ? 1 : 0; }
EXPORT("sim_inbuf") char* sim_inbuf() { return inBuf; }
EXPORT("sim_inbuf_size") int sim_inbuf_size() { return (int)sizeof inBuf; }
EXPORT("sim_pathbuf") char* sim_pathbuf() { return pathBuf; }
EXPORT("sim_methodbuf") char* sim_methodbuf() { return methodBuf; }
EXPORT("sim_tokenbuf") char* sim_tokenbuf() { return tokenBuf; }
EXPORT("sim_outbuf") const char* sim_outbuf() { return outBuf; }
EXPORT("sim_token") const char* sim_token() { return app->sessionToken(); }
EXPORT("sim_pin") const char* sim_pin() { return app->pairPin(); }
EXPORT("sim_api") int sim_api(int bodyLen, int viaAP, int devTools, double now) {
  StrBuf out(outBuf, sizeof outBuf);
  ApiCtx ctx; ctx.viaAP = viaAP != 0; ctx.devTools = devTools != 0; ctx.token = tokenBuf;
  return app->api(methodBuf, pathBuf, inBuf, (size_t)bodyLen, ctx, out, (ms_t)now);
}
EXPORT("sim_open_config") void sim_open_config(double now) { app->openConfig((ms_t)now); }
EXPORT("sim_set_wifi_broken") void sim_set_wifi_broken(int b) { plat->wifiBroken = b != 0; }
EXPORT("sim_flush") void sim_flush(double now) { app->flush((ms_t)now, true); }
EXPORT("sim_frame_crc") uint32_t sim_frame_crc() { return crc32(app->frame().buf, FB_BYTES); }
EXPORT("sim_storage_writes") int sim_storage_writes() { return (int)app->storageWrites(); }
}

// Rotinas mínimas de C exigidas pelo compilador em modo freestanding.
extern "C" {
void* memset(void* d, int v, size_t n) { tb::tbMemset(d, v, n); return d; }
void* memcpy(void* d, const void* s, size_t n) { tb::tbMemcpy(d, s, n); return d; }
void* memmove(void* d, const void* s, size_t n) {
  uint8_t* a = (uint8_t*)d; const uint8_t* b = (const uint8_t*)s;
  if (a < b) { while (n--) *a++ = *b++; } else { a += n; b += n; while (n--) *--a = *--b; }
  return d;
}
int memcmp(const void* a, const void* b, size_t n) { return tb::tbMemcmp(a, b, n); }
}
