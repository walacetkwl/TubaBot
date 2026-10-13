// TubaBot — firmware ESP32-C3 (TubaBot Lite e TubaBot).
// O comportamento todo vive em core/ (o mesmo código do simulador); aqui ficam
// só os adaptadores de hardware, rede e o console serial de recuperação.
#include <Arduino.h>
#include "board_profile.h"
#include "hw_io.h"
#include "oled_ssd1306.h"
#include "esp_platform.h"
#include "app.h"

static OutputsHw outputsHw;
static TouchInput touchIn;
static Oled oled;
static EspPlatform plat;
static tb::App app(plat);
static bool screenPowered = true;
static char line[64];
static int lineLen = 0;

static void printHelp() {
  Serial.println(F("Comandos: ajuda | status | config | wifi-esquecer | i2c | toque | oled-teste | reiniciar"));
  Serial.println(F("  config        reabre a rede própria do TubaBot (não apaga nada)"));
  Serial.println(F("  wifi-esquecer apaga só a rede de casa (apelido, recordes e conquistas ficam)"));
}

static void command(const char* c, uint64_t now) {
  if (!strcmp(c, "ajuda") || !strcmp(c, "help")) printHelp();
  else if (!strcmp(c, "config")) { app.openConfig(now); Serial.println(F("rede própria aberta")); }
  else if (!strcmp(c, "wifi-esquecer")) { app.forgetWifi(); Serial.println(F("rede de casa esquecida")); }
  else if (!strcmp(c, "reiniciar")) { app.flush(now, true); ESP.restart(); }
  else if (!strcmp(c, "i2c")) {
    uint8_t f[16]; int n = Oled::scan(f, 16);
    Serial.printf("I2C (SDA %d, SCL %d): %d dispositivo(s):", TB_PIN_SDA, TB_PIN_SCL, n);
    for (int i = 0; i < n; i++) Serial.printf(" 0x%02X", f[i]);
    Serial.printf("\nOLED: %s (0x%02X), erros %lu, último envio %lu us\n", oled.ok() ? "ok" : "AUSENTE", oled.address(), (unsigned long)oled.errors(), (unsigned long)oled.lastPushUs());
  } else if (!strcmp(c, "toque")) {
    Serial.printf("touch GPIO%d: %s, bordas %lu\n", TB_PIN_TOUCH, touchIn.rawLevel() ? "TOCADO" : "livre", (unsigned long)touchIn.edges());
  } else if (!strcmp(c, "oled-teste")) {
    char b[64]; tb::StrBuf sb(b, sizeof b); tb::ApiCtx ctx; ctx.token = app.sessionToken();
    app.api("POST", "/api/action", "{\"action\":\"diag.oled\"}", 23, ctx, sb, now);
    Serial.println(b);
  } else if (!strcmp(c, "status")) {
    static char out[4096]; tb::StrBuf sb(out, sizeof out); tb::ApiCtx ctx;
    app.api("GET", "/api/status", "", 0, ctx, sb, now);
    Serial.println(out);
    Serial.printf("heap livre %u (mín %u)\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap());
  } else if (c[0]) { Serial.printf("comando desconhecido: %s\n", c); printHelp(); }
}

static void serialPoll(uint64_t now) {
  while (Serial.available()) {
    int ch = Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') { line[lineLen] = 0; command(line, now); lineLen = 0; }
    else if (lineLen < (int)sizeof line - 1) line[lineLen++] = (char)ch;
  }
}

void setup() {
  outputsHw.begin();          // 1º: buzzer e motor inativos (sem pulsos de teste)
  Serial.begin(115200);
  touchIn.begin();
  bool oledOk = oled.begin();
  plat.attach(&app);
  plat.begin(&oled);          // não liga rádio sem necessidade; nunca bloqueia
#if TB_MODEL_FULL
  const tb::Model model = tb::Model::Full;
#else
  const tb::Model model = tb::Model::Lite;
#endif
  app.begin(model, nowMs(), esp_random(), false);  // carrega preferências ANTES do bipe de abertura
  Serial.printf("\n%s %s | OLED %s | toque GPIO%d\n", model == tb::Model::Full ? "TubaBot" : "TubaBot Lite", TB_VERSION,
                oledOk ? "ok" : "AUSENTE (seguindo sem visor)", TB_PIN_TOUCH);
  printHelp();
}

void loop() {
  uint64_t now = nowMs();
  touchIn.drain(app);
  bool changed = app.tick(now);
  outputsHw.write(app.buzzer(), app.motor());   // política já aplicada no núcleo; Lite nem tem pinos
  bool off = app.screen() == tb::Screen::ScreenOff;
  if (off != !screenPowered) { oled.setPower(!off); screenPowered = !off; }
  if (changed && oled.ok() && oled.push(app.frame().buf)) app.framePresented(nowMs());
  oled.service((uint32_t)now);
  plat.loop(now);
  serialPoll(now);
  delay(1);  // cede tempo ao Wi-Fi; nada no laço bloqueia por mais que um envio ao OLED
}
