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

void defaultSettings(Settings& s) {
  tbMemset(&s, 0, sizeof s);
  s.setupDone = 0;
  s.nickname[0] = 0;
  s.personality = (uint8_t)Personality::Tranquilo;
  s.contrast = 200;
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
  s.ntpEnabled = 0;
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
  clampField<uint8_t>(s.contrast, 1, 255, d.contrast);
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
