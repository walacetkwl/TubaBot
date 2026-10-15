#include "esp_platform.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <time.h>
#include "board_profile.h"
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include "portal_gz.h"

using namespace tb;

static WebServer server(80);
static DNSServer dns;
static Preferences nvs;
static const IPAddress AP_IP(TB_AP_IP_OCTETS);
static const char* BLOB_KEYS[] = {"", "s", "p", "c"};

#ifndef TB_ENV_NAME
#define TB_ENV_NAME "?"
#endif

// ── armazenamento (NVS) ─────────────────────────────────────────────────────
bool EspPlatform::storageLoad(BlobKind k, uint8_t* buf, size_t cap, size_t* len) {
  if (!nvs.begin("tuba", true)) return false;
  size_t n = nvs.getBytesLength(BLOB_KEYS[(int)k]);
  bool ok = n > 0 && n <= cap && nvs.getBytes(BLOB_KEYS[(int)k], buf, n) == n;
  nvs.end();
  if (ok) *len = n;
  return ok;
}
bool EspPlatform::storageSave(BlobKind k, const uint8_t* buf, size_t len) {
  saving_ = true;
  bool ok = nvs.begin("tuba", false) && nvs.putBytes(BLOB_KEYS[(int)k], buf, len) == len;
  nvs.end();
  saving_ = false;
  return ok;
}
void EspPlatform::storageErase(BlobKind k) {
  if (nvs.begin("tuba", false)) { nvs.remove(BLOB_KEYS[(int)k]); nvs.end(); }
}

// ── identidade da rede própria ──────────────────────────────────────────────
void EspPlatform::begin(Display* screen, Sensors* sensors) {
  screen_ = screen; sensors_ = sensors;
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(ssid_, sizeof ssid_, "TubaBot-%02X%02X", mac[4], mac[5]);
  snprintf(mdns_, sizeof mdns_, "tubabot-%02x%02x", mac[4], mac[5]);
  // senha própria por aparelho (gerada uma vez, guardada na NVS) — nunca a senha universal da referência
  if (nvs.begin("tubawifi", false)) {
    String p = nvs.getString("appass", "");
    if (p.length() < 8) {
      static const char* A = "abcdefghjkmnpqrstuvwxyz23456789";
      char b[10];
      for (int i = 0; i < 9; i++) b[i] = i == 4 ? '-' : A[esp_random() % 31];
      b[9] = 0;
      nvs.putString("appass", b);
      p = b;
    }
    strlcpy(apPass_, p.c_str(), sizeof apPass_);
    String s = nvs.getString("ssid", "");
    staConfigured_ = s.length() > 0;
    strlcpy(staSsid_, s.c_str(), sizeof staSsid_);
    nvs.end();
  }
  WiFi.persistent(false);    // credenciais ficam só na nossa NVS
  WiFi.mode(WIFI_OFF);       // rádio desligado até ser necessário
  setupServer();
  // marca a imagem atual como válida (efeito apenas se o bootloader tiver rollback habilitado)
  esp_ota_mark_app_valid_cancel_rollback();
  if (staConfigured_) startSta();
}

void EspPlatform::radioUpdate() {
  bool sta = staConfigured_ && sta_ != StaState::Off;
  wifi_mode_t m = apActive_ ? (sta ? WIFI_AP_STA : WIFI_AP) : (sta ? WIFI_STA : WIFI_OFF);
  if (WiFi.getMode() != m) WiFi.mode(m);
}

void EspPlatform::apOpen() {
  apActivityAt_ = millis();
  if (apActive_) return;
  apActive_ = true;
  radioUpdate();
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid_, apPass_);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", AP_IP);  // DNS cativo; o endereço manual 192.168.4.1 sempre funciona
  if (!mdnsStarted_) mdnsStarted_ = MDNS.begin(mdns_);
  if (mdnsStarted_) MDNS.addService("http", "tcp", 80);
  if (!serverStarted_) { server.begin(); serverStarted_ = true; }
  log("rede própria aberta");
}

void EspPlatform::apClose() {
  if (!apActive_ || busy()) return;  // nunca fecha durante gravação/OTA
  dns.stop();
  WiFi.softAPdisconnect(true);
  apActive_ = false;
  radioUpdate();
  log("rede própria fechada");
}

void EspPlatform::startSta() {
  if (!staConfigured_) return;
  char pass[65] = {0};
  if (nvs.begin("tubawifi", true)) { nvs.getString("pass", pass, sizeof pass); nvs.end(); }
  sta_ = StaState::Connecting;
  radioUpdate();
  WiFi.begin(staSsid_, pass[0] ? pass : nullptr);  // não bloqueia: o estado é acompanhado em loop()
  memset(pass, 0, sizeof pass);
  staStartedAt_ = millis();
  if (!serverStarted_) { server.begin(); serverStarted_ = true; }
}

bool EspPlatform::wifiSet(const char* ssid, const char* pass) {
  if (!nvs.begin("tubawifi", false)) return false;
  bool ok = nvs.putString("ssid", ssid) > 0;
  nvs.putString("pass", pass);
  nvs.end();
  if (!ok) return false;
  staConfigured_ = true;
  strlcpy(staSsid_, ssid, sizeof staSsid_);
  staBackoff_ = 300000;
  WiFi.disconnect(false);
  startSta();
  return true;
}

void EspPlatform::wifiForget() {
  if (nvs.begin("tubawifi", false)) { nvs.remove("ssid"); nvs.remove("pass"); nvs.end(); }  // mantém a senha do AP
  staConfigured_ = false; staSsid_[0] = 0; sta_ = StaState::Off;
  WiFi.disconnect(false, true);
  radioUpdate();
}

void EspPlatform::netStatus(NetStatus& s) {
  s = NetStatus();
  s.apActive = apActive_;
  strlcpy(s.apSsid, ssid_, sizeof s.apSsid);
  strlcpy(s.apPass, apPass_, sizeof s.apPass);  // exibida só no VISOR (a API não devolve)
  strlcpy(s.apIp, "192.168.4.1", sizeof s.apIp);
  snprintf(s.mdns, sizeof s.mdns, "%s.local", mdns_);
  uint16_t tmin = app_ ? app_->settings().apTimeoutMin : 0;
  if (apActive_ && tmin) {
    uint32_t el = millis() - apActivityAt_, lim = (uint32_t)tmin * 60000u;
    s.apSecondsLeft = el >= lim ? 1 : (lim - el) / 1000;
  }
  s.staConfigured = staConfigured_;
  s.sta = sta_;
  strlcpy(s.staSsid, staSsid_, sizeof s.staSsid);
  if (sta_ == StaState::Connected) strlcpy(s.staIp, WiFi.localIP().toString().c_str(), sizeof s.staIp);
  s.radioOn = WiFi.getMode() != WIFI_OFF;
}

void EspPlatform::sysInfo(SysInfo& s) {
  s = SysInfo();
  s.heapFree = ESP.getFreeHeap(); s.heapMin = ESP.getMinFreeHeap(); s.psramFree = ESP.getFreePsram();
  s.flashSize = ESP.getFlashChipSize(); s.sketchSize = ESP.getSketchSize(); s.sketchFree = ESP.getFreeSketchSpace();
  snprintf(s.chip, sizeof s.chip, "%s rev%d", ESP.getChipModel(), ESP.getChipRevision());
  snprintf(s.build, sizeof s.build, "%s %s", TB_ENV_NAME, __DATE__);
}

void EspPlatform::applyDisplay(uint8_t b, bool r) { if (screen_) screen_->setBrightness(b); rotate_ = r; }
void EspPlatform::rtcWrite(int64_t epoch) { if (sensors_) sensors_->rtcWrite(epoch); }
void EspPlatform::setTimezone(const char* posix) { setenv("TZ", posix, 1); tzset(); }
void EspPlatform::log(const char* msg) { Serial.printf("[%8lu] %s\n", (unsigned long)millis(), msg); }

// ── HTTP ────────────────────────────────────────────────────────────────────
bool EspPlatform::requestViaAP() { return apActive_ && server.client().localIP() == WiFi.softAPIP(); }

void EspPlatform::handleRoot() {
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html; charset=utf-8", (const char*)PORTAL_GZ, PORTAL_GZ_LEN);
}

void EspPlatform::handleCaptive() {
  // Sondas de "portal cativo" dos sistemas -> página local. HTTPS não é interceptado.
  String host = server.hostHeader();
  if (apActive_ && host != "192.168.4.1" && !host.startsWith(mdns_)) {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
    return;
  }
  if (server.uri() == "/") { handleRoot(); return; }
  server.send(404, "application/json", "{\"ok\":false,\"error\":\"não encontrado\"}");
}

void EspPlatform::handleApi() {
  // 64 KB na PSRAM: cabe a miniatura da tela (/api/frame) e o status completo
  static char* out = (char*)ps_malloc(65536);
  static const size_t outCap = out ? 65536 : 0;
  if (!out) { server.send(500, "application/json", "{\"ok\":false,\"error\":\"sem memória\"}"); return; }
  apActivityAt_ = millis();
  if (server.clientContentLength() > TB_HTTP_MAX_BODY) { server.send(413, "application/json", "{\"ok\":false,\"error\":\"corpo grande demais\"}"); return; }
  // Origem: navegadores enviam Origin em POST; deve ser este mesmo aparelho.
  String origin = server.header("Origin");
  if (origin.length() && origin.indexOf(server.hostHeader()) < 0) { server.send(403, "application/json", "{\"ok\":false,\"error\":\"origem recusada\"}"); return; }
  const char* method = server.method() == HTTP_GET ? "GET" : server.method() == HTTP_POST ? "POST" : "OTHER";
  String body = server.hasArg("plain") ? server.arg("plain") : String();
  String token = server.header("X-Tuba-Token");
  ApiCtx ctx; ctx.viaAP = requestViaAP(); ctx.devTools = false; ctx.token = token.c_str();
  StrBuf sb(out, outCap);
  int code = app_->api(method, server.uri().c_str(), body.c_str(), body.length(), ctx, sb, nowMs());
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json; charset=utf-8", out);
}

void EspPlatform::handleUpdateChunk() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaOk_ = false; otaErr_[0] = 0; otaWritten_ = 0;
    const char* reason = nullptr;
    if (server.header("X-Tuba-Token") != app_->sessionToken()) { strlcpy(otaErr_, "sessão inválida", sizeof otaErr_); return; }
    if (!app_->otaAllowed(&reason)) { strlcpy(otaErr_, reason ? reason : "ocupado", sizeof otaErr_); return; }
    otaTotal_ = (size_t)server.header("X-Tuba-Size").toInt();
    if (otaTotal_ == 0 || otaTotal_ > (size_t)ESP.getFreeSketchSpace()) { strlcpy(otaErr_, "tamanho inválido ou maior que a partição", sizeof otaErr_); return; }
    otaActive_ = true;
    app_->enterOta(nowMs());
    if (!Update.begin(otaTotal_, U_FLASH)) { strlcpy(otaErr_, Update.errorString(), sizeof otaErr_); otaActive_ = false; return; }
  } else if (up.status == UPLOAD_FILE_WRITE && otaActive_ && !otaErr_[0]) {
    if (otaWritten_ == 0) {
      // valida a imagem: magic 0xE9 e chip ESP32-S3 (id 9 no cabeçalho estendido)
      if (up.currentSize < 16 || up.buf[0] != 0xE9 || up.buf[12] != 0x09 || up.buf[13] != 0x00) {
        strlcpy(otaErr_, "arquivo não é firmware para ESP32-S3", sizeof otaErr_); Update.abort(); otaActive_ = false; return;
      }
    }
    if (Update.write(up.buf, up.currentSize) != up.currentSize) { strlcpy(otaErr_, Update.errorString(), sizeof otaErr_); Update.abort(); otaActive_ = false; return; }
    otaWritten_ += up.currentSize;
    // a requisição é tratada de forma síncrona: atualiza o visor daqui mesmo
    app_->otaProgress((int)(otaWritten_ * 1000 / (otaTotal_ ? otaTotal_ : 1)));
    if (app_->tick(nowMs()) && screen_) screen_->push(app_->canvas().pixels(), app_->dirty(), rotate_);
  } else if (up.status == UPLOAD_FILE_END && otaActive_ && !otaErr_[0]) {
    if (otaWritten_ != otaTotal_) { strlcpy(otaErr_, "tamanho recebido diferente do anunciado", sizeof otaErr_); Update.abort(); }
    else if (!Update.end(true)) strlcpy(otaErr_, Update.errorString(), sizeof otaErr_);
    else otaOk_ = true;
    otaActive_ = false;
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (otaActive_) Update.abort();
    otaActive_ = false;
    strlcpy(otaErr_, "envio interrompido", sizeof otaErr_);
  }
}

void EspPlatform::handleUpdateDone() {
  char b[128];
  if (otaOk_) {
    server.send(200, "application/json", "{\"ok\":true}");
    log("atualização gravada; reiniciando");
    requestReboot();
  } else {
    snprintf(b, sizeof b, "{\"ok\":false,\"error\":\"%s\"}", otaErr_[0] ? otaErr_ : "falha");
    server.send(otaErr_[0] && !strcmp(otaErr_, "sessão inválida") ? 401 : 422, "application/json", b);
    if (app_->screen() == Screen::Ota) app_->otaFailed(nowMs());
  }
}

void EspPlatform::setupServer() {
  static const char* hdrs[] = {"X-Tuba-Token", "X-Tuba-Size", "Origin"};
  server.collectHeaders(hdrs, 3);
  server.on("/", HTTP_GET, [this]() { handleRoot(); });
  server.on("/update", HTTP_POST, [this]() { handleUpdateDone(); }, [this]() { handleUpdateChunk(); });
  for (const char* p : {"/generate_204", "/gen_204", "/hotspot-detect.html", "/connecttest.txt", "/ncsi.txt", "/fwlink", "/redirect"})
    server.on(p, [this]() { server.sendHeader("Location", "http://192.168.4.1/", true); server.send(302, "text/plain", ""); });
  server.onNotFound([this]() {
    if (server.uri().startsWith("/api/")) handleApi();
    else handleCaptive();
  });
}

// ── clima: GET HTTPS numa tarefa própria (o laço da tela nunca espera a rede) ──
static char wUrl[512];
static char* wBody = nullptr;
static volatile int wState = 0;   // 0 livre, 1 pedido, 2 resposta pronta
static volatile int wCode = 0;
static volatile size_t wLen = 0;
static void weatherTask(void*) {
  for (;;) {
    if (wState != 1) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
    NetworkClientSecure cli;
    // Dados públicos de previsão; a resposta passa pelo validador JSON do núcleo.
    // (Certificado não fixado: a CA do Open-Meteo pode mudar sem aviso — ver docs/FUNCIONALIDADES.md.)
    cli.setInsecure();
    HTTPClient http;
    http.setTimeout(8000);
    int code = 0; size_t n = 0;
    if (http.begin(cli, wUrl)) {
      code = http.GET();
      if (code == 200) {
        WiFiClient* st = http.getStreamPtr();
        uint32_t t0 = millis();
        while ((http.connected() || st->available()) && n < 8191 && millis() - t0 < 8000) {
          int a = st->available();
          if (a <= 0) { delay(5); continue; }
          n += st->readBytes(wBody + n, min((size_t)a, (size_t)8191 - n));
        }
      }
      http.end();
    }
    wBody[n] = 0; wLen = n; wCode = code; wState = 2;
  }
}

void EspPlatform::weatherLoop(uint64_t now) {
  if (!wBody) { wBody = (char*)ps_malloc(8192); if (!wBody) return; xTaskCreatePinnedToCore(weatherTask, "tb_weather", 8192, nullptr, 1, nullptr, 0); }
  if (wState == 2) { app_->weatherResult(wBody, wLen, wCode, now); wState = 0; }
  if (wState == 0 && app_->weatherWant(now, wUrl, sizeof wUrl)) wState = 1;
}

// ── laço ────────────────────────────────────────────────────────────────────
void EspPlatform::loop(uint64_t now) {
  if (app_) weatherLoop(now);
  if (apActive_) dns.processNextRequest();
  if (serverStarted_) server.handleClient();
  uint32_t ms = millis();
  // rede de casa: tentativas espaçadas, nunca infinitas nem bloqueantes
  if (sta_ == StaState::Connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      sta_ = StaState::Connected; staBackoff_ = 300000;
      if (!mdnsStarted_) { mdnsStarted_ = MDNS.begin(mdns_); if (mdnsStarted_) MDNS.addService("http", "tcp", 80); }
      log("rede de casa conectada");
    } else if (ms - staStartedAt_ > TB_STA_CONNECT_TIMEOUT_MS) {
      sta_ = StaState::Failed; WiFi.disconnect(false); staRetryAt_ = ms + staBackoff_;
      staBackoff_ = staBackoff_ * 2 > 1800000 ? 1800000 : staBackoff_ * 2;
      radioUpdate();
      log("rede de casa indisponível; seguindo offline");
    }
  } else if (sta_ == StaState::Connected && WiFi.status() != WL_CONNECTED) {
    sta_ = StaState::Failed; staRetryAt_ = ms + 30000; log("rede de casa caiu");
  } else if (sta_ == StaState::Failed && staConfigured_ && (int32_t)(ms - staRetryAt_) >= 0) {
    startSta();
  }
  // NTP opcional (desligado por padrão)
  if (app_ && app_->settings().ntpEnabled && sta_ == StaState::Connected) {
    if (!ntpConfigured_) { configTzTime(TZ_TABLE[app_->settings().tzIndex].posix, "pool.ntp.org", "time.google.com"); ntpConfigured_ = true; }
    time_t t = time(nullptr);
    if (t > 1700000000 && (ntpLastSync_ == 0 || ms - ntpLastSync_ > 6UL * 3600 * 1000)) { app_->setTime((int64_t)t, nowMs()); ntpLastSync_ = ms; }
  }
  // encerra a rede própria após inatividade configurada (nunca durante gravação/OTA)
  if (apActive_ && app_ && app_->settings().apTimeoutMin && !busy() && WiFi.softAPgetStationNum() == 0 &&
      ms - apActivityAt_ > (uint32_t)app_->settings().apTimeoutMin * 60000u) {
    apClose();
  }
  if (rebootAt_ && (int32_t)(ms - rebootAt_) >= 0) { Serial.flush(); ESP.restart(); }
}
