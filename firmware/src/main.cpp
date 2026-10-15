// TubaBot v2 — firmware para a Waveshare ESP32-S3-Touch-AMOLED-1.75.
// O comportamento todo vive em core/ (o mesmo código do simulador e dos testes);
// aqui ficam só os adaptadores de hardware, rede e o console serial.
#include <Arduino.h>
#include <esp_heap_caps.h>
#include "board_profile.h"
#include "hw.h"
#include "esp_platform.h"
#include "app.h"

static Display screen;
static Sensors sensors;
static Audio audio;
static EspPlatform plat;
static tb::App app(plat);
static uint16_t* framebuffer = nullptr;
static char line[64];
static int lineLen = 0;
static uint64_t nextSound = 0;

static void printHelp() {
  Serial.println(F("Comandos: ajuda | status | config | wifi-esquecer | i2c | toque | imu | som | tela-teste | reiniciar"));
  Serial.println(F("  config        reabre a rede própria do TubaBot (não apaga nada)"));
  Serial.println(F("  wifi-esquecer apaga só a rede de casa (apelido, recordes e conquistas ficam)"));
}

static void command(const char* c, uint64_t now) {
  if (!strcmp(c, "ajuda") || !strcmp(c, "help")) printHelp();
  else if (!strcmp(c, "config")) { app.openConfig(now); Serial.println(F("rede própria aberta")); }
  else if (!strcmp(c, "wifi-esquecer")) { app.forgetWifi(); Serial.println(F("rede de casa esquecida")); }
  else if (!strcmp(c, "reiniciar")) { app.flush(now, true); ESP.restart(); }
  else if (!strcmp(c, "i2c")) {
    uint8_t f[24]; int n = Sensors::scanI2C(f, 24);
    Serial.printf("I2C (SDA %d, SCL %d): %d dispositivo(s):", TB_I2C_SDA, TB_I2C_SCL, n);
    for (int i = 0; i < n; i++) Serial.printf(" 0x%02X", f[i]);
    Serial.println(F("\nesperados: 0x18 ES8311, 0x20 TCA9554, 0x34 AXP2101, 0x40 ES7210, 0x51 PCF85063, 0x5A CST9217, 0x6B QMI8658"));
    Serial.printf("tela %s (último envio %lu us) | toque %s | IMU %s | PMU %s | RTC %s | alto-falante %s | microfones %s\n",
                  screen.ok() ? "ok" : "FALHA", (unsigned long)screen.lastPushUs(), sensors.touchOk() ? "ok" : "FALHA",
                  sensors.imuOk() ? "ok" : "FALHA", sensors.pmuOk() ? "ok" : "FALHA", sensors.rtcOk() ? "ok" : "FALHA",
                  audio.speakerOk() ? "ok" : "FALHA", audio.micOk() ? "ok" : "FALHA");
  } else if (!strcmp(c, "toque")) {
    Serial.printf("toque: %s em x=%d y=%d (canto superior esquerdo deve dar ~0,0; ajuste TB_TOUCH_MIRROR_*/SWAP em board_profile.h)\n",
                  sensors.touching() ? "TOCANDO" : "livre", sensors.lastX(), sensors.lastY());
  } else if (!strcmp(c, "imu")) {
    int x, y, z; sensors.lastAccel(x, y, z);
    Serial.printf("gravidade (mg, referencial da tela): x=%d y=%d z=%d | em pé deve dar y≈+1000; tela para cima z≈-1000 (ajuste TB_IMU_MAP)\n", x, y, z);
  } else if (!strcmp(c, "som")) {
    Serial.printf("nível do microfone: %d (0..100)\n", audio.micLevel());
    char b[64]; tb::StrBuf sb(b, sizeof b); tb::ApiCtx ctx; ctx.token = app.sessionToken();
    { const char* q = "{\"action\":\"preview\",\"pattern\":\"boot\"}"; app.api("POST", "/api/action", q, strlen(q), ctx, sb, now); }
  } else if (!strcmp(c, "tela-teste")) {
    char b[64]; tb::StrBuf sb(b, sizeof b); tb::ApiCtx ctx; ctx.token = app.sessionToken();
    { const char* q = "{\"action\":\"diag.oled\"}"; app.api("POST", "/api/action", q, strlen(q), ctx, sb, now); }
    Serial.println(b);
  } else if (!strcmp(c, "status")) {
    static char out[6144]; tb::StrBuf sb(out, sizeof out); tb::ApiCtx ctx;
    app.api("GET", "/api/status", "", 0, ctx, sb, now);
    Serial.println(out);
    Serial.printf("RAM livre %u (mín %u) · PSRAM livre %u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getFreePsram());
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
  motorBegin();               // 1º: motor (se houver) inativo
  Serial.begin(115200);
  framebuffer = (uint16_t*)heap_caps_malloc(tb::SCR_W * tb::SCR_H * 2, MALLOC_CAP_SPIRAM);
  if (!framebuffer) framebuffer = (uint16_t*)heap_caps_malloc(tb::SCR_W * tb::SCR_H * 2, MALLOC_CAP_8BIT);
  app.attachFramebuffer(framebuffer);
  sensors.begin();            // I²C: toque, IMU, PMU, RTC, expansor
  bool scrOk = screen.begin();
  plat.attach(&app);
  plat.begin(&screen, &sensors);  // não liga rádio sem necessidade; nunca bloqueia
  tb::HwCaps caps;
  caps.motor = TB_HAS_MOTOR;
  caps.imu = sensors.imuOk();
  caps.battery = sensors.pmuOk();
  caps.rtc = sensors.rtcOk();
  caps.speaker = true;        // o ES8311 é testado logo abaixo; sem alto-falante o som só não sai
  caps.mic = true;
  app.begin(caps, nowMs(), esp_random(), false);   // carrega preferências ANTES do som de abertura
  int64_t ep;
  if (sensors.rtcRead(ep)) app.rtcTime(ep, nowMs());
  audio.begin(&app);
  Serial.printf("\nTubaBot %s | tela %s | toque %s | IMU %s | RTC %s | áudio %s/%s\n", TB_VERSION, scrOk ? "ok" : "FALHA",
                sensors.touchOk() ? "ok" : "FALHA", sensors.imuOk() ? "ok" : "FALHA", sensors.rtcOk() ? "ok" : "FALHA",
                audio.speakerOk() ? "ok" : "FALHA", audio.micOk() ? "ok" : "FALHA");
  printHelp();
}

void loop() {
  uint64_t now = nowMs();
  sensors.poll(app, now);
  if (now >= nextSound) { nextSound = now + 40; int lv = audio.micLevel(); if (lv >= 0) app.soundLevel(lv, now); }
  if (app.tick(now) && screen.push(app.canvas().pixels(), app.dirty(), plat.rotate180())) app.framePresented(nowMs());
  motorWrite(app.motor());
  plat.loop(now);
  serialPoll(now);
  delay(1);  // cede tempo ao Wi-Fi
}
