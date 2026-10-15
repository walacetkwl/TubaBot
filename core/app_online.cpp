// Atualização pela internet, telemetria para o painel da TubaLabs e idioma.
//
// Atualização: o TubaBot consulta um manifesto JSON (servidor da TubaLabs) ao
// conectar no Wi-Fi de casa, a cada 6 h e quando o usuário toca "Buscar
// atualizações". Havendo versão nova, ele avisa na tela; o usuário pode
// atualizar agora ou agendar ("em 1 hora" até 3 vezes, ou "esta noite", às 3h,
// quando instala sozinho) — nunca ignorar. A versão pendente fica gravada: se
// a unidade desligar, o aviso volta. Quem não estava conectado recebe o aviso
// assim que conectar.
//
// Manifesto (exemplo em docs/futuras-atualizacoes.html):
//   {"version":"3.2.0","url":"https://.../tubabot-3.2.0-app-ota.bin",
//    "sha256":"<64 hex>","size":1500000,"notes":{"pt-BR":"...","en":"...","es-419":"..."}}
//
// Telemetria: sem dados pessoais (nada de apelido, rede, áudio ou toque).
// Só o necessário para o painel: série, modelo, versão, se está ativado e se
// houve interação hoje / nos últimos 7 dias. O usuário pode desligar em Sistema.
#include "app.h"
#include "app_ids.h"

namespace tb {

static const ms_t UPD_PERIOD_MS = 6ull * 3600 * 1000;
static const ms_t TEL_PERIOD_MS = 10ull * 60 * 1000;

// Compara versões "a.b.c" numericamente (<0, 0, >0).
int versionCompare(const char* a, const char* b) {
  for (int k = 0; k < 4; k++) {
    int x = 0, y = 0;
    while (*a >= '0' && *a <= '9') x = x * 10 + (*a++ - '0');
    while (*b >= '0' && *b <= '9') y = y * 10 + (*b++ - '0');
    if (x != y) return x < y ? -1 : 1;
    if (*a == '.') a++;
    if (*b == '.') b++;
    if (!*a && !*b) return 0;
  }
  return 0;
}

static bool validVersion(const char* v) {
  if (!v[0]) return false;
  int dots = 0;
  for (const char* p = v; *p; p++) {
    if (*p == '.') dots++;
    else if (*p < '0' || *p > '9') return false;
  }
  return dots >= 1 && dots <= 3;
}

void App::applyLanguage() {
  setLang((Lang)set_.language);
  if (lastLang_ != set_.language) {
    lastLang_ = set_.language;
    canvas_.invalidateAll();
    forceFrame_ = true;
  }
}

// ── rede ────────────────────────────────────────────────────────────────────
void App::updateNet(ms_t now) {
  if (now >= netPollAt_) {
    netPollAt_ = now + 3000;
    NetStatus ns; plat_.netStatus(ns);
    bool up = ns.sta == StaState::Connected;
    if (up && !staUp_) {                    // acabou de conectar: busca atualização e dá sinal de vida
      updCheckAt_ = now + 4000;
      telAt_ = now + 8000;
      event("internet conectada");
    }
    staUp_ = up;
  }
  if (!set_.updVersion[0] || screen_ == Screen::Ota || updInstallReq_) return;
  if (updDueMs_ && now < updDueMs_) return;        // adiado sem relógio válido
  // hora de instalar / perguntar?
  bool due = set_.updDueMin == 0;
  if (!due && clock_.valid()) due = (uint32_t)(clock_.epoch(now) / 60) >= set_.updDueMin;
  if (!due) return;
  if (set_.updAuto) {
    if (!updUrl_[0]) {                      // reiniciou depois de agendar: busca o manifesto de novo
      if (staUp_ && !updChecking_ && (!updCheckAt_ || now >= updCheckAt_ + 60000)) updCheckAt_ = now;
      return;
    }
    const char* why = nullptr;
    bool busyUser = scene_.active() || (now - lastInteraction_ < 20000 && screen_ != Screen::UpdateOffer);
    if (!otaAllowed(&why) || busyUser) {    // em uso: tenta de novo em 30 min
      if (clock_.valid()) { set_.updDueMin = (uint32_t)(clock_.epoch(now) / 60) + 30; markSettings(now); }
      else updDueMs_ = now + 30ull * 60000;
      return;
    }
    updInstallReq_ = true;
    event("atualização: instalando");
    return;
  }
  // perguntar: só em momento oportuno (sem alerta, jogo, foco, cena ou outra janela)
  bool home = screen_ == Screen::Face || screen_ == Screen::Clock || screen_ == Screen::Today;
  if (!updOffer_ && home && !alertN_ && !scene_.active() && !medalVisible(now) &&
      !(pomo_.phase() == PomoPhase::Focus && pomo_.state() == RunState::Running))
    offerUpdate(now);
}

void App::offerUpdate(ms_t now) {
  updOffer_ = true;
  returnScreen_ = screen_ == Screen::UpdateOffer ? homePage_ : screen_;
  transDir_ = 0;
  gotoScreen(Screen::UpdateOffer, now);
  out_.play(Pattern::Reminder, now);
  event("atualização disponível: perguntando");
}

// choice: 0 agora, 1 em 1 hora, 2 esta noite (03:00 local)
void App::scheduleUpdate(int choice, ms_t now) {
  updOffer_ = false;
  if (!set_.updVersion[0]) return;
  bool clk = clock_.valid();
  uint32_t nowMin = clk ? (uint32_t)(clock_.epoch(now) / 60) : 0;
  updDueMs_ = 0;
  if (choice == 0) {
    set_.updAuto = 1; set_.updDueMin = 0;
    if (!staUp_) toast(tr("Sem internet: instala ao conectar"), now, 3000);
  } else if (choice == 1 && set_.updSnoozes < 3) {
    set_.updSnoozes++; set_.updAuto = 0;
    set_.updDueMin = clk ? nowMin + 60 : 0;
    if (!clk) updDueMs_ = now + 3600000ull;
    toast(tr("Aviso de novo em 1 hora"), now, 2500);
  } else {
    set_.updAuto = 1;
    if (clk) {
      int off = TZ_TABLE[set_.tzIndex].offsetMin;
      int64_t localMin = (int64_t)nowMin + off;
      int64_t dayStart = localMin - (localMin % 1440);
      int64_t target = dayStart + 3 * 60;                 // 03:00 de hoje...
      if (target <= localMin + 30) target += 1440;         // ...ou de amanhã
      set_.updDueMin = (uint32_t)(target - off);
    } else { set_.updDueMin = 0; updDueMs_ = now + 6ull * 3600000; }
    toast(tr("Instala esta noite, às 3h"), now, 2500);
  }
  markSettings(now);
  gotoScreen(returnScreen_ == Screen::UpdateOffer ? homePage_ : returnScreen_, now);
}

void App::checkUpdatesNow(ms_t now) {
  if (!plat_.serviceUrl(Service::UpdateManifest)[0]) { toast(tr("Atualizações online em breve"), now, 2500); updResult_ = -1; return; }
  NetStatus ns; plat_.netStatus(ns);
  staUp_ = ns.sta == StaState::Connected;
  if (!staUp_) { toast(tr("Conecte o Wi-Fi de casa"), now, 2500); updResult_ = -1; return; }
  updManual_ = true;
  updCheckAt_ = now;
  updChecking_ = false;
  toast(tr("Buscando atualizações..."), now, 1500);
}

bool App::updateWant(ms_t now, char* url, size_t cap) {
  const char* base = plat_.serviceUrl(Service::UpdateManifest);
  if (!base[0] || !staUp_ || updChecking_) return false;
  if (!updCheckAt_) updCheckAt_ = now + 20000;     // primeira busca pouco depois de ligar
  if (now < updCheckAt_) return false;
  updChecking_ = true;
  updCheckAt_ = now + 120000;                       // sem resposta: nova tentativa em 2 min
  StrBuf s(url, cap);
  s.put(base);
  // identifica modelo/versão para o servidor poder fazer lançamentos graduais
  bool hasQ = false;
  for (const char* p = base; *p; p++) if (*p == '?') hasQ = true;
  s.put(hasQ ? "&" : "?").put("fw=").put(TB_VERSION).put("&model=").put(fac_.model == Model::Pro ? "pro" : "lite");
  if (fac_.serial[0]) {
    s.put("&serial=");
    for (const char* p = fac_.serial; *p; p++) if ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '-') s.put(*p);
  }
  return !s.overflow;
}

void App::updateManifest(const char* body, size_t len, int httpCode, ms_t now) {
  updChecking_ = false;
  updLastCheck_ = now;
  bool manual = updManual_;
  updManual_ = false;
  JsonVal root;
  char ver[16] = {0}, url[sizeof updUrl_] = {0}, sha[65] = {0};
  int64_t size = 0;
  bool ok = httpCode == 200 && body && jsonParse(body, len, root) && root.isObject() &&
            root.get("version").asString(ver, sizeof ver) && validVersion(ver);
  if (!ok) {
    updResult_ = -1;
    updCheckAt_ = now + 30ull * 60000;
    if (manual) toast(tr("Não foi possível buscar agora"), now, 2500);
    event("atualização: falha ao buscar");
    return;
  }
  updCheckAt_ = now + UPD_PERIOD_MS;
  if (versionCompare(ver, TB_VERSION) <= 0) {
    updResult_ = 1;
    if (set_.updVersion[0]) { set_.updVersion[0] = 0; set_.updDueMin = 0; set_.updSnoozes = 0; set_.updAuto = 0; markSettings(now); }
    if (manual) toast(tr("Você já tem a versão mais nova"), now, 2500);
    return;
  }
  bool urlOk = root.get("url").asString(url, sizeof url) && (tbStrStarts(url, "https://") || tbStrStarts(url, "http://") || tbStrStarts(url, "sim://"));
  bool shaOk = root.get("sha256").asString(sha, sizeof sha) && tbStrlen(sha) == 64;
  for (int i = 0; shaOk && sha[i]; i++) {
    char c = sha[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) shaOk = false;
  }
  root.get("size").asInt(size);
  if (!urlOk || !shaOk) { updResult_ = -1; event("atualização: manifesto incompleto"); return; }
  tbStrlcpy(updUrl_, url, sizeof updUrl_);
  tbStrlcpy(updSha_, sha, sizeof updSha_);
  updSize_ = size > 0 && size < 0x7FFFFFFF ? (uint32_t)size : 0;
  updNotes_[0] = 0;
  JsonVal notes = root.get("notes");
  if (notes.isObject()) {
    if (!notes.get(langKey(lang())).asString(updNotes_, sizeof updNotes_)) notes.get("pt-BR").asString(updNotes_, sizeof updNotes_);
  } else if (notes.isString()) notes.asString(updNotes_, sizeof updNotes_);
  updResult_ = 2;
  if (!tbStreq(set_.updVersion, ver)) {             // versão nova anunciada: pergunta já
    tbStrlcpy(set_.updVersion, ver, sizeof set_.updVersion);
    set_.updDueMin = 0; set_.updSnoozes = 0; set_.updAuto = 0;
    markSettings(now);
    updOffer_ = false;
    char b[48]; StrBuf e(b, sizeof b); e.put("atualização disponível: ").put(ver); event(b);
  } else if (manual && !set_.updAuto) {
    set_.updDueMin = 0; updOffer_ = false;          // "buscar" com pendência: pergunta de novo
    markSettings(now);
  }
}

bool App::updateInstallWant(char* url, size_t ucap, char* sha, size_t scap, uint32_t* size) {
  if (!updInstallReq_ || !updUrl_[0]) return false;
  updInstallReq_ = false;
  tbStrlcpy(url, updUrl_, ucap);
  tbStrlcpy(sha, updSha_, scap);
  if (size) *size = updSize_;
  return true;
}

// ── telemetria ──────────────────────────────────────────────────────────────
void App::writeTelemetry(JsonOut& j, ms_t now) {
  const Progress& Pr = prog_;
  SysInfo si; plat_.sysInfo(si);
  uint16_t today = (uint16_t)(localDay(now) + 1);
  bool todayOk = Pr.actDay == today;
  int days = 0;
  if (todayOk) { for (int i = 0; i < 7; i++) if ((Pr.actMask >> i) & 1u) days++; }
  else if (Pr.actDay) {
    int diff = (int)(uint16_t)(today - Pr.actDay);
    for (int i = 0; i < 7 - diff && diff > 0 && diff < 7; i++) if ((Pr.actMask >> i) & 1u) days++;
  }
  j.kint("schema", 1);
  j.kstr("serial", fac_.serial);
  j.kstr("deviceId", si.deviceId);
  j.kstr("model", fac_.model == Model::Pro ? "pro" : "lite");
  j.kstr("body", BODIES[fac_.body < BODY_COUNT ? fac_.body : 3].key);
  j.kbool("factoryProfile", fac_.present);
  j.kstr("fw", TB_VERSION);
  j.kstr("lang", langKey((Lang)set_.language));
  j.kbool("activated", set_.setupDone != 0);
  j.kint("uptimeMin", (int64_t)((now - 0) / 60000));
  j.kbool("interactedToday", todayOk && (Pr.actMask & 1u));
  j.kint("interactionsToday", todayOk ? Pr.interToday : 0);
  j.kint("activeDays7", days);
  j.kint("bond", Pr.bond);
  j.kint("pets", Pr.pets);
  j.kint("pomodoros", Pr.pomodorosDone);
  j.kint("games", Pr.gamesPlayed);
  j.kint("legendary", legendsSeen());
  j.kstr("pendingUpdate", set_.updVersion);
  if (caps_.battery && power_.known && power_.percent >= 0) j.kint("batteryPct", power_.percent);
}

bool App::telemetryWant(ms_t now, char* url, size_t ucap, StrBuf& body) {
  const char* base = plat_.serviceUrl(Service::Telemetry);
  if (!base[0] || !staUp_ || set_.noTelemetry || !telAt_ || now < telAt_) return false;
  ms_t back = TEL_PERIOD_MS << (telFails_ < 4 ? telFails_ : 4);   // falhas: espaça até ~2,7 h
  telAt_ = now + back;
  tbStrlcpy(url, base, ucap);
  JsonOut j(body);
  j.beginObj();
  writeTelemetry(j, now);
  j.endObj();
  return !body.overflow;
}

void App::telemetryResult(int httpCode, ms_t now) {
  (void)now;
  if (httpCode >= 200 && httpCode < 300) telFails_ = 0;
  else if (telFails_ < 8) telFails_++;
}

}  // namespace tb
