// Preferências, progresso e checkpoints persistidos (versão + CRC).
// Segredos de Wi-Fi NÃO ficam aqui: o firmware os guarda à parte, para que
// "esquecer rede" nunca apague apelido, recordes ou conquistas.
#pragma once
#include "tb_base.h"
#include "outputs.h"

namespace tb {

static const int NICK_MAX_CHARS = 12;
static const int NICK_MAX_BYTES = 12 * 4 + 1;
static const int REMINDER_COUNT = 3;
static const int REMINDER_LABEL_BYTES = 16 * 2 + 1;  // até 16 caracteres (acentos ocupam 2 bytes)
static const int TIMER_PRESET_COUNT = 6;
static const int ALARM_COUNT = 3;
static const int ALARM_LABEL_BYTES = 12 * 2 + 1;
static const int EXPR_COLOR_COUNT = 10;
static const int BODY_COUNT = 4;
static const int WATCHFACE_COUNT = 3;

enum class Personality : uint8_t { Tranquilo = 0, Brincalhao = 1 };

struct Reminder {
  char label[REMINDER_LABEL_BYTES];
  uint16_t intervalMin;  // 5..240
  uint8_t enabled;
};

struct Alarm {
  char label[ALARM_LABEL_BYTES];
  uint8_t hour, minute;  // hora local
  uint8_t days;          // bit 0 = domingo ... bit 6 = sábado; 0 = uma vez (próxima ocorrência)
  uint8_t enabled;
};

struct Settings {
  // ── herdado da v1 (mesma ordem) ──
  uint8_t setupDone;
  char nickname[NICK_MAX_BYTES];
  uint8_t personality;
  uint8_t brightness;     // 1..255 (v1: "contraste")
  uint8_t rotate180;
  uint8_t favoriteExpr;
  uint8_t animLevel;      // 0 calma, 1 normal, 2 animada
  uint16_t screenOffS;    // 0 = nunca; senão 30..3600
  uint8_t shortIntro;
  uint8_t sound, silent, vibration;
  uint8_t pomoFocusMin, pomoShortMin, pomoLongMin, pomoCyclesBeforeLong, pomoAutoStart;
  uint32_t timerPresetS[TIMER_PRESET_COUNT];
  Reminder reminders[REMINDER_COUNT];
  uint8_t quietEnabled, quietStartH, quietEndH;
  uint8_t tzIndex;
  uint8_t clock24h;
  uint8_t jumpEasy;
  uint16_t apTimeoutMin;
  uint8_t weatherEnabled;
  uint8_t ntpEnabled;
  // ── novo na v2 ──
  uint8_t volume;         // 0..100
  uint8_t eyeColor;       // cor das expressões: índice em EXPR_COLORS
  uint8_t eyeGlow;        // halo luminoso nos olhos
  uint8_t watchFace;      // 0 digital, 1 analógico, 2 "Tuba"
  uint8_t imuReactions;   // reagir a sacudir/pegar/virar
  uint8_t micReactions;   // reagir a palmas/barulho/música (só nível sonoro)
  uint8_t tiltLook;       // olhos seguem a inclinação
  uint8_t faceDownSnooze; // virar de cara para baixo adia o alarme / silencia o aviso
  uint8_t nightDim;       // brilho reduzido no período de descanso
  uint8_t wakeOnPickup;   // pegar no colo acende a tela
  Alarm alarms[ALARM_COUNT];
  uint8_t weatherCity;    // índice em CITY_TABLE; 255 = coordenadas próprias
  int32_t weatherLatE4, weatherLonE4;  // graus × 10⁴ (cidade personalizada)
  // ── novo na v3 (ES3C28P) ──
  uint8_t theme;          // 0 escuro, 1 claro
  uint8_t showClock;      // informações discretas sobre o rosto (todas desligadas de fábrica)
  uint8_t showWeather;
  uint8_t showBattery;    // só existe no modelo Pro
  uint8_t reserved[12];
};

// Expressões que podem ser desbloqueadas (bits em Progress::unlocked).
enum UnlockId : uint8_t {
  UNL_TIMIDO = 0, UNL_FOCADO, UNL_VITORIOSO, UNL_SATISFEITO, UNL_ESPANTADO, UNL_PRECISAO,
  UNL_DANCANTE, UNL_ZEN, UNL_MERGULHADOR, UNL_RADIANTE, UNL_COUNT
};

struct Progress {
  // ── herdado da v1 ──
  uint32_t unlocked;
  uint32_t pets;
  uint32_t pomodorosDone;
  uint32_t reflexValid;
  uint16_t jumpBest;
  uint16_t fishBest;
  uint16_t reflexBestMs;
  uint16_t timingBestPct100;
  uint16_t timingBestErrMs;
  uint16_t timingBestTargetCs;
  uint32_t gamesPlayed;
  int16_t mood;              // -100..100
  uint16_t bond;             // 0..1000
  // ── novo na v2 ──
  uint16_t diveBest;
  uint16_t danceSeconds;     // acumulado
  uint16_t breathSessions;   // sessões de respiração concluídas
  uint16_t shakes;
  uint32_t boops;
  uint8_t reserved[12];
};

struct TimeCheckpoint {
  uint8_t valid;
  uint8_t pomoActive, pomoPhase, pomoFocusDone;
  uint32_t pomoRemainingMs;
  uint8_t timerActive[3];
  uint32_t timerRemainingMs[3];
  uint32_t timerDurationMs[3];
  uint8_t swActive;
  uint32_t swElapsedMs;
};

struct TzEntry { const char* iana; const char* label; int16_t offsetMin; const char* posix; };
extern const TzEntry TZ_TABLE[];
extern const int TZ_COUNT;
int tzIndexFromIana(const char* iana);

struct CityEntry { const char* key; const char* name; int32_t latE4, lonE4; uint8_t tz; };
extern const CityEntry CITY_TABLE[];
extern const int CITY_COUNT;
int cityIndexFromKey(const char* key);

// Cor das expressões: cada cor tem uma versão para o tema escuro e outra para o claro
// (ex.: "Branco" vira cinza-azulado no fundo branco; "Grafite" clareia no fundo preto).
struct ExprColorEntry { const char* key; const char* name; uint32_t dark, light; };
extern const ExprColorEntry EXPR_COLORS[EXPR_COLOR_COUNT];
int exprColorFromKey(const char* key);
inline uint32_t exprColorRgb(int i, bool light) { const ExprColorEntry& e = EXPR_COLORS[(unsigned)i < (unsigned)EXPR_COLOR_COUNT ? i : 0]; return light ? e.light : e.dark; }

// Variantes físicas (cor do corpo). Cada uma sugere a cor de expressão padrão.
struct BodyEntry { const char* key; const char* name; uint32_t rgb; uint8_t exprColor; };
extern const BodyEntry BODIES[BODY_COUNT];
int bodyFromKey(const char* key);

void defaultSettings(Settings& s);
void defaultProgress(Progress& p);
void sanitizeSettings(Settings& s);

// Blob persistido: [magic 'TUBA'][kind][version][len 2B][crc 4B][payload]
enum class BlobKind : uint8_t { Settings = 1, Progress = 2, Checkpoint = 3 };
static const int BLOB_HEADER = 12;
static const uint8_t SETTINGS_VERSION = 3;
static const uint8_t PROGRESS_VERSION = 2;
static const uint8_t CHECKPOINT_VERSION = 1;
static const size_t BLOB_MAX = 1024;

size_t encodeBlob(BlobKind k, uint8_t ver, const void* payload, size_t len, uint8_t* out, size_t cap);
enum class DecodeResult : uint8_t { Ok, Empty, BadMagic, BadKind, BadCrc, Migrated, TooNew };
DecodeResult decodeBlob(BlobKind k, uint8_t curVer, const uint8_t* in, size_t inLen, void* payload, size_t len);

}  // namespace tb
