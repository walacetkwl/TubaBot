// Ponte WebAssembly: o MESMO núcleo C++ (core/) compilado para o navegador.
// O JavaScript só apresenta a imagem, toca o áudio e encaminha entradas;
// regras, jogos, tempo, desenho e síntese de som acontecem aqui.
#include "../core/app.h"

extern "C" {
__attribute__((import_name("js_storage_load"))) int js_storage_load(int kind, uint8_t* buf, int cap);
__attribute__((import_name("js_storage_save"))) int js_storage_save(int kind, const uint8_t* buf, int len);
__attribute__((import_name("js_storage_erase"))) void js_storage_erase(int kind);
__attribute__((import_name("js_log"))) void js_log(const char* msg);
__attribute__((import_name("js_reboot"))) void js_reboot();
__attribute__((import_name("js_display"))) void js_display(int brightness, int rotate);
__attribute__((import_name("js_rtc"))) void js_rtc(double epoch);
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
  void apOpen() override { if (!apActive) js_log("[rede] rede própria aberta (simulada)"); apActive = true; }
  void apClose() override { if (apActive) js_log("[rede] rede própria fechada (simulada)"); apActive = false; }
  void netStatus(NetStatus& s) override {
    s = NetStatus();
    s.apActive = apActive; s.radioOn = apActive || staConfigured;
    StrBuf a(s.apSsid, sizeof s.apSsid); a.put("TubaBot-").hex(serial, 4);
    StrBuf p(s.apPass, sizeof s.apPass); p.put("tuba-").hex(serial * 2654435761u, 4);
    for (char* c = s.apPass; *c; c++) if (*c >= 'A' && *c <= 'Z') *c = (char)(*c + 32);
    tbStrlcpy(s.apIp, "192.168.4.1", sizeof s.apIp);
    StrBuf m(s.mdns, sizeof s.mdns); m.put("tubabot-").hex(serial, 4).put(".local");
    for (char* c = s.mdns; *c; c++) if (*c >= 'A' && *c <= 'Z') *c = (char)(*c + 32);
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
  void applyDisplay(uint8_t b, bool r) override { js_display(b, r ? 1 : 0); }
  void setTimezone(const char*) override {}
  bool busy() override { return false; }
  void log(const char* msg) override { js_log(msg); }
  void rtcWrite(int64_t epoch) override { js_rtc((double)epoch); }
};

alignas(SimPlatform) static unsigned char platMem[sizeof(SimPlatform)];
alignas(App) static unsigned char appMem[sizeof(App)];
static SimPlatform* plat = nullptr;
static App* app = nullptr;
static uint16_t framebuffer[SCR_W * SCR_H];
static int16_t audioBuf[4096];
static char inBuf[16384];
static char outBuf[65536];
static char pathBuf[128], methodBuf[8], tokenBuf[40];
static char urlBuf[512];

#define EXPORT(n) __attribute__((export_name(n)))

extern "C" {
// caps: bit0 alto-falante, bit1 motor, bit2 IMU, bit3 microfone, bit4 bateria, bit5 RTC
EXPORT("sim_init") void sim_init(uint32_t seed, int dev, double now, int caps) {
  bool keepNet = plat != nullptr;
  SimPlatform saved;
  if (keepNet) { saved.staConfigured = plat->staConfigured; saved.wifiBroken = plat->wifiBroken; tbStrlcpy(saved.staSsid, plat->staSsid, sizeof saved.staSsid); }
  plat = new (platMem) SimPlatform();
  if (keepNet) { plat->staConfigured = saved.staConfigured; plat->wifiBroken = saved.wifiBroken; tbStrlcpy(plat->staSsid, saved.staSsid, sizeof plat->staSsid); }
  app = new (appMem) App(*plat);
  app->attachFramebuffer(framebuffer);
  HwCaps c;
  c.speaker = caps & 1; c.motor = (caps >> 1) & 1; c.imu = (caps >> 2) & 1; c.mic = (caps >> 3) & 1; c.battery = (caps >> 4) & 1; c.rtc = (caps >> 5) & 1;
  app->begin(c, (ms_t)now, seed, dev != 0);
}
EXPORT("sim_rtc_time") void sim_rtc_time(double epoch, double now) { app->rtcTime((int64_t)epoch, (ms_t)now); }
EXPORT("sim_touch") void sim_touch(int down, int x, int y, double t) { app->touchSample(down != 0, x, y, (ms_t)t); }
EXPORT("sim_motion") void sim_motion(int ax, int ay, int az, double t) { app->motionSample(ax, ay, az, (ms_t)t); }
EXPORT("sim_sound") void sim_sound(int level, double t) { app->soundLevel(level, (ms_t)t); }
EXPORT("sim_power") void sim_power(int battery, int pct, int charging, int usb, double now) {
  PowerState p; p.known = true; p.battery = battery != 0; p.percent = (int8_t)(battery ? pct : -1); p.charging = charging != 0; p.usb = usb != 0;
  p.mv = (uint16_t)(battery ? 3300 + pct * 9 : 0);
  app->powerUpdate(p, (ms_t)now);
}
EXPORT("sim_button") void sim_button(int id, int longPress, double now) { app->hwButton(id ? HwButton::Power : HwButton::Boot, longPress != 0, (ms_t)now); }
EXPORT("sim_tick") int sim_tick(double now) { return app->tick((ms_t)now) ? 1 : 0; }
EXPORT("sim_presented") void sim_presented(double t) { app->framePresented((ms_t)t); }
EXPORT("sim_frame") const uint16_t* sim_frame() { return framebuffer; }
EXPORT("sim_dirty") int sim_dirty(int i) { const IRect& d = app->dirty(); return i == 0 ? d.x0 : i == 1 ? d.y0 : i == 2 ? d.x1 : d.y1; }
EXPORT("sim_audio") const int16_t* sim_audio(int n) { if (n > 4096) n = 4096; app->audioRender(audioBuf, n); return audioBuf; }
EXPORT("sim_motor") int sim_motor() { return app->motor() ? 1 : 0; }
EXPORT("sim_sound_active") int sim_sound_active(double now) { return app->outputs().soundActive((ms_t)now) ? 1 : 0; }
EXPORT("sim_screen") int sim_screen() { return (int)app->screen(); }
EXPORT("sim_screen_name") const char* sim_screen_name() { return screenName(app->screen()); }
EXPORT("sim_expr_name") const char* sim_expr_name() { return exprName(app->face().shown()); }
EXPORT("sim_brightness") int sim_brightness() { return app->brightnessApplied(); }
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
EXPORT("sim_weather_want") const char* sim_weather_want(double now) { return app->weatherWant((ms_t)now, urlBuf, sizeof urlBuf) ? urlBuf : nullptr; }
EXPORT("sim_weather_result") void sim_weather_result(int len, int code, double now) { app->weatherResult(inBuf, (size_t)len, code, (ms_t)now); }
EXPORT("sim_open_config") void sim_open_config(double now) { app->openConfig((ms_t)now); }
EXPORT("sim_set_wifi_broken") void sim_set_wifi_broken(int b) { plat->wifiBroken = b != 0; }
EXPORT("sim_flush") void sim_flush(double now) { app->flush((ms_t)now, true); }
EXPORT("sim_frame_crc") uint32_t sim_frame_crc() { return app->canvas().crc(); }
EXPORT("sim_storage_writes") int sim_storage_writes() { return (int)app->storageWrites(); }
EXPORT("sim_pressed") int sim_pressed() { return app->input().pressed() ? 1 : 0; }
EXPORT("sim_hold_ms") double sim_hold_ms(double now) { return (double)app->input().holdMs((ms_t)now); }
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
