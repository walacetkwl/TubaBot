// Executa tests/scenario.h no núcleo nativo e imprime CRCs da imagem 320×240.
#include "fake_platform.h"
#include "scenario.h"
#include <cstdio>
#include <vector>
using namespace tb;

// Mesmos dados de rede do simulador (o QR Code da tela depende deles).
struct CrossPlatform : FakePlatform {
  void netStatus(NetStatus& s) override {
    s = NetStatus();
    s.apActive = apActive; s.radioOn = apActive;
    const uint32_t serial = 0xAB12;
    StrBuf a(s.apSsid, sizeof s.apSsid); a.put("TubaBot-").hex(serial, 4);
    StrBuf p(s.apPass, sizeof s.apPass); p.put("tuba-").hex(serial * 2654435761u, 4);
    for (char* c = s.apPass; *c; c++) if (*c >= 'A' && *c <= 'Z') *c = (char)(*c + 32);
    tbStrlcpy(s.apIp, "192.168.4.1", sizeof s.apIp);
    StrBuf m(s.mdns, sizeof s.mdns); m.put("tubabot-").hex(serial, 4).put(".local");
    for (char* c = s.mdns; *c; c++) if (*c >= 'A' && *c <= 'Z') *c = (char)(*c + 32);
  }
};

int main() {
  CrossPlatform p; std::vector<uint16_t> fb(SCR_W * SCR_H);
  p.setFactory(Model::Pro, 0, 0);
  App a(p); a.attachFramebuffer(fb.data());
  HwCaps c; c.motor = false; c.imu = false; c.rtc = false; c.battery = true; c.mic = true; c.speaker = true;
  a.begin(c, 0, 99, true);
  ms_t t = 0; size_t n = sizeof(SCENARIO) / sizeof(SCENARIO[0]);
  for (size_t i = 0; i < n; i++) {
    const long long* r = SCENARIO[i];
    ms_t target = (ms_t)r[0];
    while (t + 10 <= target) { t += 10; if (a.tick(t)) a.framePresented(t); }
    switch (r[1]) {
      case -1: printf("%llu %s %08x\n", (unsigned long long)target, screenName(a.screen()), (unsigned)a.canvas().crc()); break;
      case 0: case 1: a.touchSample(r[1] != 0, (int)r[2], (int)r[3], target); break;
      case 2: a.motionSample((int)r[2], (int)r[3], (int)r[4], target); break;
      case 3: a.soundLevel((int)r[2], target); break;
    }
  }
}
