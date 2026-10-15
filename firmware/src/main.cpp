// TubaBot v3 — firmware para a ES3C28P (ESP32-S3 + tela 2,8" em paisagem 320×240).
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
static char line[256];
static int lineLen = 0;
static uint64_t nextSound = 0;

static void printHelp() {
  Serial.println(F("Comandos: ajuda | status | config | wifi-esquecer | i2c | toque | som | tela-teste | fabrica | fabrica-gravar ... | reiniciar"));
  Serial.println(F("  config         reabre a rede própria do TubaBot (não apaga nada)"));
  Serial.println(F("  wifi-esquecer  apaga só a rede de casa (apelido, recordes e conquistas ficam)"));
  Serial.println(F("  fabrica        mostra o perfil de fábrica desta unidade"));
  Serial.println(F("  fabrica-gravar modelo=pro cor=vermelho [expressao=..] [tema=escuro|claro] [serie=..]"));
}

static void api(const char* body) {
  char b[96]; tb::StrBuf sb(b, sizeof b); tb::ApiCtx ctx; ctx.token = app.sessionToken();
  app.api("POST", "/api/action", body, strlen(body), ctx, sb, nowMs());
  Serial.println(b);
}

// "fabrica-gravar chave=valor chave=valor ..." -> texto do perfil com CRC, validado pelo núcleo.
static void factoryWriteCmd(const char* args) {
  static char text[tb::FACTORY_MAX];
  tb::StrBuf s(text, sizeof text);
  s.put("TUBABOT-FABRICA 1\n");
  const char* p = args;
  while (*p) {
    while (*p == ' ') p++;
    const char* st = p;
    while (*p && *p != ' ') p++;
    if (p > st) s.putN(st, (size_t)(p - st)).put('\n');
  }
  uint32_t c = tb::crc32((const uint8_t*)text, s.len);
  s.put("crc=").hex(c, 8).put('\n');
  tb::FactoryConfig f; const char* why = nullptr;
  if (s.overflow || !tb::parseFactory(text, s.len, f, &why) || why) {
    Serial.printf("perfil recusado: %s\n", why ? why : "inválido");
    return;
  }
  // regrava em formato canônico (mesmo texto que o tools/fabrica.py produz)
  size_t n = tb::formatFactory(f, text, sizeof text);
  if (!n || !plat.factoryWrite(text, n)) { Serial.println(F("falha ao gravar a partição 'fabrica'")); return; }
  Serial.print(text);
  Serial.println(F("perfil gravado. Reiniciando..."));
  delay(200);
  ESP.restart();
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
    Serial.println(F("\nesperados: 0x18 ES8311 (áudio), 0x38 FT6336G (toque)"));
    Serial.printf("tela %s (último envio %lu us) | toque %s | alto-falante %s | microfone %s | bateria %d mV\n",
                  screen.ok() ? "ok" : "FALHA", (unsigned long)screen.lastPushUs(), sensors.touchOk() ? "ok" : "FALHA",
                  audio.speakerOk() ? "ok" : "FALHA", audio.micOk() ? "ok" : "FALHA", sensors.batteryMv());
  } else if (!strcmp(c, "toque")) {
    Serial.printf("toque: %s | tela x=%d y=%d (cru x=%d y=%d)\n", sensors.touching() ? "TOCANDO" : "livre",
                  sensors.lastX(), sensors.lastY(), sensors.rawX(), sensors.rawY());
    Serial.println(F("  canto superior esquerdo deve dar ~0,0 e o inferior direito ~319,239 (ajuste TB_TOUCH_MAP)"));
  } else if (!strcmp(c, "som")) {
    Serial.printf("nível do microfone: %d (0..100)\n", audio.micLevel());
    api("{\"action\":\"preview\",\"pattern\":\"boot\"}");
  } else if (!strcmp(c, "tela-teste")) {
    api("{\"action\":\"diag.oled\"}");
    Serial.println(F("  ponto vermelho no canto superior ESQUERDO, barras branco..azul da esquerda p/ a direita"));
  } else if (!strcmp(c, "fabrica")) {
    const tb::FactoryConfig& f = app.factory();
    if (!f.present) Serial.printf("perfil de fábrica AUSENTE (%s): usando padrões genéricos (Lite)\n", app.factoryIssue() ? app.factoryIssue() : "?");
    else Serial.printf("TubaBot %s | cor %s | expressões %s | tema %s | série %s%s%s\n", tb::modelName(f.model), tb::BODIES[f.body].name,
                       tb::EXPR_COLORS[f.exprColor].name, f.theme ? "claro" : "escuro", f.serial[0] ? f.serial : "-",
                       app.factoryIssue() ? " | aviso: " : "", app.factoryIssue() ? app.factoryIssue() : "");
  } else if (!strncmp(c, "fabrica-gravar ", 15)) factoryWriteCmd(c + 15);
  else if (!strcmp(c, "status")) {
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
  ledOff();
  Serial.begin(115200);
  // framebuffer 320×240 (150 KB): RAM interna se couber (mais rápido), senão PSRAM
  framebuffer = (uint16_t*)heap_caps_malloc(tb::SCR_W * tb::SCR_H * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!framebuffer) framebuffer = (uint16_t*)heap_caps_malloc(tb::SCR_W * tb::SCR_H * 2, MALLOC_CAP_SPIRAM);
  app.attachFramebuffer(framebuffer);
  sensors.begin();            // I²C: toque + codec; ADC da bateria; botão BOOT
  bool scrOk = screen.begin();
  plat.attach(&app);
  plat.begin(&screen, &sensors);  // não liga rádio sem necessidade; nunca bloqueia
  tb::HwCaps caps;
  caps.motor = false;
  caps.imu = false;           // a ES3C28P não tem sensor de movimento
  caps.rtc = false;           // nem relógio com bateria: a hora vem da rede (NTP) ou do portal
  caps.battery = true;        // conector de LiPo existe; o perfil de fábrica decide (Lite ignora)
  caps.speaker = true;
  caps.mic = true;
  app.begin(caps, nowMs(), esp_random(), false);   // carrega fábrica + preferências ANTES do som de abertura
  audio.begin(&app);
  const tb::FactoryConfig& f = app.factory();
  Serial.printf("\nTubaBot %s %s | %s | tela %s | toque %s | áudio %s/%s\n", tb::modelName(f.model), TB_VERSION,
                f.present ? tb::BODIES[f.body].name : "SEM PERFIL DE FÁBRICA", scrOk ? "ok" : "FALHA",
                sensors.touchOk() ? "ok" : "FALHA", audio.speakerOk() ? "ok" : "FALHA", audio.micOk() ? "ok" : "FALHA");
  printHelp();
}

void loop() {
  uint64_t now = nowMs();
  sensors.poll(app, now);
  if (now >= nextSound) { nextSound = now + 40; int lv = audio.micLevel(); if (lv >= 0) app.soundLevel(lv, now); }
  if (app.tick(now) && screen.push(app.canvas().pixels(), app.dirty())) app.framePresented(nowMs());
  plat.loop(now);
  serialPoll(now);
  delay(1);  // cede tempo ao Wi-Fi
}
