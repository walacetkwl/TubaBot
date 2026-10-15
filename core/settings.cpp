#include "settings.h"

namespace tb {

// Fusos suportados. O Brasil não usa horário de verão desde 2019, então todos
// os fusos brasileiros são deslocamentos fixos. O identificador IANA é o que o
// portal envia; a string POSIX é o que o firmware passa ao setenv("TZ").
const TzEntry TZ_TABLE[] = {
  {"America/Sao_Paulo", "Brasília (UTC-3)", -180, "<-03>3"},
  {"America/Manaus", "Manaus (UTC-4)", -240, "<-04>4"},
  {"America/Rio_Branco", "Rio Branco (UTC-5)", -300, "<-05>5"},
  {"America/Noronha", "Noronha (UTC-2)", -120, "<-02>2"},
  {"America/Cuiaba", "Cuiabá (UTC-4)", -240, "<-04>4"},
  {"America/Belem", "Belém (UTC-3)", -180, "<-03>3"},
  {"America/Fortaleza", "Fortaleza (UTC-3)", -180, "<-03>3"},
  {"America/Recife", "Recife (UTC-3)", -180, "<-03>3"},
  {"America/Bahia", "Salvador (UTC-3)", -180, "<-03>3"},
  {"UTC", "UTC", 0, "UTC0"},
};
const int TZ_COUNT = sizeof(TZ_TABLE) / sizeof(TZ_TABLE[0]);

int tzIndexFromIana(const char* iana) {
  for (int i = 0; i < TZ_COUNT; i++) if (tbStreq(TZ_TABLE[i].iana, iana)) return i;
  return -1;
}

// Capitais brasileiras para o clima (Open-Meteo, sem chave). 'tz' = índice em TZ_TABLE.
const CityEntry CITY_TABLE[] = {
  {"sao_paulo", "São Paulo", -235505, -466333, 0}, {"rio_de_janeiro", "Rio de Janeiro", -229068, -431729, 0},
  {"belo_horizonte", "Belo Horizonte", -199167, -439345, 0}, {"brasilia", "Brasília", -157939, -478828, 0},
  {"salvador", "Salvador", -129714, -385014, 8}, {"fortaleza", "Fortaleza", -37319, -385267, 6},
  {"recife", "Recife", -80476, -348770, 7}, {"porto_alegre", "Porto Alegre", -300346, -512177, 0},
  {"curitiba", "Curitiba", -254284, -492733, 0}, {"manaus", "Manaus", -31190, -600217, 1},
  {"belem", "Belém", -14558, -485044, 5}, {"goiania", "Goiânia", -166869, -492648, 0},
  {"sao_luis", "São Luís", -25307, -443068, 5}, {"maceio", "Maceió", -96658, -357353, 7},
  {"natal", "Natal", -57945, -352110, 6}, {"teresina", "Teresina", -50892, -428019, 6},
  {"joao_pessoa", "João Pessoa", -71195, -348450, 7}, {"aracaju", "Aracaju", -109472, -370731, 8},
  {"campo_grande", "Campo Grande", -204697, -546201, 4}, {"cuiaba", "Cuiabá", -156014, -560979, 4},
  {"florianopolis", "Florianópolis", -275954, -485480, 0}, {"vitoria", "Vitória", -203155, -403128, 0},
  {"porto_velho", "Porto Velho", -87612, -639004, 1}, {"rio_branco", "Rio Branco", -99749, -678243, 2},
  {"macapa", "Macapá", 349, -510694, 5}, {"boa_vista", "Boa Vista", 28235, -606758, 1},
  {"palmas", "Palmas", -101840, -483336, 0},
};
const int CITY_COUNT = sizeof(CITY_TABLE) / sizeof(CITY_TABLE[0]);
int cityIndexFromKey(const char* key) {
  for (int i = 0; i < CITY_COUNT; i++) if (tbStreq(CITY_TABLE[i].key, key)) return i;
  return -1;
}

// As 4 primeiras acompanham as cores físicas do TubaBot.
const ExprColorEntry EXPR_COLORS[EXPR_COLOR_COUNT] = {
  {"vermelho", "Vermelho", 0xFF4B4B, 0xD9232F},
  {"azul", "Azul", 0x4F9BFF, 0x1D5FD1},
  {"branco", "Branco", 0xF1F5FB, 0x7A8596},
  {"grafite", "Grafite", 0xAEB6C2, 0x30353D},
  {"ciano", "Ciano", 0x3EE6FF, 0x0092B0},
  {"verde", "Verde", 0x4BF0A6, 0x12965F},
  {"lilas", "Lilás", 0xB592FF, 0x6E43D8},
  {"rosa", "Rosa", 0xFF7AB6, 0xD23F7C},
  {"amarelo", "Amarelo", 0xFFD54A, 0xB98500},
  {"laranja", "Laranja", 0xFF9F43, 0xD35F12},
};
int exprColorFromKey(const char* key) {
  for (int i = 0; i < EXPR_COLOR_COUNT; i++) if (tbStreq(EXPR_COLORS[i].key, key)) return i;
  if (tbStreq(key, "preto")) return 3;
  return -1;
}

const BodyEntry BODIES[BODY_COUNT] = {
  {"vermelho", "Vermelho", 0xC9303A, 0},
  {"azul", "Azul", 0x2E6CCB, 1},
  {"branco", "Branco", 0xEEEEEA, 2},
  {"grafite", "Grafite", 0x3A3E45, 3},
};
int bodyFromKey(const char* key) {
  for (int i = 0; i < BODY_COUNT; i++) if (tbStreq(BODIES[i].key, key)) return i;
  if (tbStreq(key, "preto")) return 3;
  return -1;
}

void defaultSettings(Settings& s) {
  tbMemset(&s, 0, sizeof s);
  s.setupDone = 0;
  s.nickname[0] = 0;
  s.personality = (uint8_t)Personality::Tranquilo;
  s.brightness = 200;
  s.rotate180 = 0;
  s.favoriteExpr = 0;
  s.animLevel = 1;
  s.screenOffS = 300;
  s.shortIntro = 0;
  s.sound = 1; s.silent = 0; s.vibration = 1;
  s.pomoFocusMin = 25; s.pomoShortMin = 5; s.pomoLongMin = 15; s.pomoCyclesBeforeLong = 4; s.pomoAutoStart = 0;
  const uint32_t presets[TIMER_PRESET_COUNT] = {60, 300, 600, 900, 1800, 120};
  for (int i = 0; i < TIMER_PRESET_COUNT; i++) s.timerPresetS[i] = presets[i];
  tbStrlcpy(s.reminders[0].label, "Beber água", sizeof s.reminders[0].label);
  s.reminders[0].intervalMin = 60; s.reminders[0].enabled = 0;
  tbStrlcpy(s.reminders[1].label, "Alongar", sizeof s.reminders[1].label);
  s.reminders[1].intervalMin = 90; s.reminders[1].enabled = 0;
  tbStrlcpy(s.reminders[2].label, "Pausa", sizeof s.reminders[2].label);
  s.reminders[2].intervalMin = 120; s.reminders[2].enabled = 0;
  s.quietEnabled = 0; s.quietStartH = 22; s.quietEndH = 7;
  s.tzIndex = 0;
  s.clock24h = 1;
  s.jumpEasy = 1;
  s.apTimeoutMin = 15;
  s.weatherEnabled = 0;
  s.ntpEnabled = 1;   // sem RTC na ES3C28P: a hora vem da rede de casa quando houver
  s.volume = 60;
  s.eyeColor = 3;
  s.eyeGlow = 1;
  s.watchFace = 0;
  s.imuReactions = 1; s.micReactions = 1; s.tiltLook = 1; s.faceDownSnooze = 1; s.nightDim = 1; s.wakeOnPickup = 1;
  static const char* const AL[ALARM_COUNT] = {"Acordar", "Alarme 2", "Alarme 3"};
  static const uint8_t AH[ALARM_COUNT] = {7, 13, 18};
  for (int i = 0; i < ALARM_COUNT; i++) {
    tbStrlcpy(s.alarms[i].label, AL[i], sizeof s.alarms[i].label);
    s.alarms[i].hour = AH[i]; s.alarms[i].minute = 0; s.alarms[i].days = 0x3E; s.alarms[i].enabled = 0;  // seg a sex
  }
  s.weatherCity = 0;
  s.weatherLatE4 = CITY_TABLE[0].latE4; s.weatherLonE4 = CITY_TABLE[0].lonE4;
  s.theme = 0;
  s.showClock = 0; s.showWeather = 0; s.showBattery = 0;
}

void defaultProgress(Progress& p) {
  tbMemset(&p, 0, sizeof p);
  p.mood = 20;
  p.bond = 0;
}

template <typename T> static void clampField(T& v, T lo, T hi, T def) { if (v < lo || v > hi) v = def; }

void sanitizeSettings(Settings& s) {
  Settings d; defaultSettings(d);
  s.setupDone = s.setupDone ? 1 : 0;
  s.nickname[NICK_MAX_BYTES - 1] = 0;
  size_t nl = tbStrlen(s.nickname);
  int nc = utf8Count(s.nickname, nl);
  if (nc < 0 || nc > NICK_MAX_CHARS) s.nickname[0] = 0;
  clampField<uint8_t>(s.personality, 0, 1, d.personality);
  clampField<uint8_t>(s.brightness, 1, 255, d.brightness);
  s.rotate180 = s.rotate180 ? 1 : 0;
  clampField<uint8_t>(s.animLevel, 0, 2, d.animLevel);
  if (s.screenOffS != 0) clampField<uint16_t>(s.screenOffS, 30, 3600, d.screenOffS);
  s.shortIntro = s.shortIntro ? 1 : 0;
  s.sound = s.sound ? 1 : 0; s.silent = s.silent ? 1 : 0; s.vibration = s.vibration ? 1 : 0;
  clampField<uint8_t>(s.pomoFocusMin, 1, 120, d.pomoFocusMin);
  clampField<uint8_t>(s.pomoShortMin, 1, 60, d.pomoShortMin);
  clampField<uint8_t>(s.pomoLongMin, 1, 90, d.pomoLongMin);
  clampField<uint8_t>(s.pomoCyclesBeforeLong, 1, 12, d.pomoCyclesBeforeLong);
  s.pomoAutoStart = s.pomoAutoStart ? 1 : 0;
  for (int i = 0; i < TIMER_PRESET_COUNT; i++) clampField<uint32_t>(s.timerPresetS[i], 1, 86399, d.timerPresetS[i]);
  for (int i = 0; i < REMINDER_COUNT; i++) {
    Reminder& r = s.reminders[i];
    r.label[REMINDER_LABEL_BYTES - 1] = 0;
    if (utf8Count(r.label, tbStrlen(r.label)) < 0) tbStrlcpy(r.label, d.reminders[i].label, sizeof r.label);
    clampField<uint16_t>(r.intervalMin, 5, 240, d.reminders[i].intervalMin);
    r.enabled = r.enabled ? 1 : 0;
  }
  s.quietEnabled = s.quietEnabled ? 1 : 0;
  clampField<uint8_t>(s.quietStartH, 0, 23, d.quietStartH);
  clampField<uint8_t>(s.quietEndH, 0, 23, d.quietEndH);
  clampField<uint8_t>(s.tzIndex, 0, (uint8_t)(TZ_COUNT - 1), 0);
  s.clock24h = s.clock24h ? 1 : 0;
  s.jumpEasy = s.jumpEasy ? 1 : 0;
  clampField<uint16_t>(s.apTimeoutMin, 0, 120, d.apTimeoutMin);
  s.weatherEnabled = s.weatherEnabled ? 1 : 0;
  s.ntpEnabled = s.ntpEnabled ? 1 : 0;
  clampField<uint8_t>(s.volume, 0, 100, d.volume);
  clampField<uint8_t>(s.eyeColor, 0, EXPR_COLOR_COUNT - 1, d.eyeColor);
  s.eyeGlow = s.eyeGlow ? 1 : 0;
  clampField<uint8_t>(s.watchFace, 0, WATCHFACE_COUNT - 1, 0);
  s.imuReactions = s.imuReactions ? 1 : 0; s.micReactions = s.micReactions ? 1 : 0; s.tiltLook = s.tiltLook ? 1 : 0;
  s.faceDownSnooze = s.faceDownSnooze ? 1 : 0; s.nightDim = s.nightDim ? 1 : 0; s.wakeOnPickup = s.wakeOnPickup ? 1 : 0;
  for (int i = 0; i < ALARM_COUNT; i++) {
    Alarm& a = s.alarms[i];
    a.label[ALARM_LABEL_BYTES - 1] = 0;
    if (utf8Count(a.label, tbStrlen(a.label)) < 0) tbStrlcpy(a.label, d.alarms[i].label, sizeof a.label);
    clampField<uint8_t>(a.hour, 0, 23, d.alarms[i].hour);
    clampField<uint8_t>(a.minute, 0, 59, 0);
    a.days &= 0x7F;
    a.enabled = a.enabled ? 1 : 0;
  }
  if (s.weatherCity != 255) clampField<uint8_t>(s.weatherCity, 0, (uint8_t)(CITY_COUNT - 1), 0);
  clampField<int32_t>(s.weatherLatE4, -900000, 900000, d.weatherLatE4);
  clampField<int32_t>(s.weatherLonE4, -1800000, 1800000, d.weatherLonE4);
  s.theme = s.theme ? 1 : 0;
  s.showClock = s.showClock ? 1 : 0; s.showWeather = s.showWeather ? 1 : 0; s.showBattery = s.showBattery ? 1 : 0;
}

size_t encodeBlob(BlobKind k, uint8_t ver, const void* payload, size_t len, uint8_t* out, size_t cap) {
  if (cap < BLOB_HEADER + len || len > 0xFFFF) return 0;
  out[0] = 'T'; out[1] = 'U'; out[2] = 'B'; out[3] = 'A';
  out[4] = (uint8_t)k; out[5] = ver;
  out[6] = (uint8_t)(len & 0xFF); out[7] = (uint8_t)(len >> 8);
  uint32_t c = crc32((const uint8_t*)payload, len, (uint32_t)k * 131u + ver);
  out[8] = (uint8_t)c; out[9] = (uint8_t)(c >> 8); out[10] = (uint8_t)(c >> 16); out[11] = (uint8_t)(c >> 24);
  tbMemcpy(out + BLOB_HEADER, payload, len);
  return BLOB_HEADER + len;
}

DecodeResult decodeBlob(BlobKind k, uint8_t curVer, const uint8_t* in, size_t inLen, void* payload, size_t len) {
  if (!in || inLen == 0) return DecodeResult::Empty;
  if (inLen < (size_t)BLOB_HEADER || in[0] != 'T' || in[1] != 'U' || in[2] != 'B' || in[3] != 'A') return DecodeResult::BadMagic;
  if (in[4] != (uint8_t)k) return DecodeResult::BadKind;
  uint8_t ver = in[5];
  size_t plen = (size_t)in[6] | ((size_t)in[7] << 8);
  if (inLen < BLOB_HEADER + plen) return DecodeResult::BadCrc;
  uint32_t c = (uint32_t)in[8] | ((uint32_t)in[9] << 8) | ((uint32_t)in[10] << 16) | ((uint32_t)in[11] << 24);
  if (crc32(in + BLOB_HEADER, plen, (uint32_t)k * 131u + ver) != c) return DecodeResult::BadCrc;
  if (ver > curVer) return DecodeResult::TooNew;
  // Migração conservadora: copia o prefixo conhecido; campos novos ficam com os defaults
  // já carregados pelo chamador em 'payload'.
  tbMemcpy(payload, in + BLOB_HEADER, tbMin(plen, len));
  return (ver == curVer && plen == len) ? DecodeResult::Ok : DecodeResult::Migrated;
}

}  // namespace tb
