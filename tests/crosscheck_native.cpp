// Executa tests/scenario.h no núcleo nativo e imprime CRCs do framebuffer.
#include "fake_platform.h"
#include "scenario.h"
#include <cstdio>
using namespace tb;
int main() {
  FakePlatform p; App a(p); a.begin(Model::Full, 0, 99, true);
  ms_t t = 0; size_t n = sizeof(SCENARIO) / sizeof(SCENARIO[0]);
  for (size_t i = 0; i < n; i++) {
    ms_t target = (ms_t)SCENARIO[i][0];
    while (t + 10 <= target) { t += 10; if (a.tick(t)) a.framePresented(t); }
    if (SCENARIO[i][1] >= 0) a.touchSample(SCENARIO[i][1] != 0, target);
    else printf("%llu %s %08x\n", (unsigned long long)target, screenName(a.screen()), (unsigned)crc32(a.frame().buf, FB_BYTES));
  }
}
