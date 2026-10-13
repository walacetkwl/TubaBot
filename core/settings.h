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

enum class Personality : uint8_t { Tranquilo = 0, Brincalhao = 1 };

struct Reminder {
  char label[REMINDER_LABEL_BYTES];
  uint16_t intervalMin;  // 5..240
  uint8_t enabled;
};

struct Settings {
  uint8_t setupDone;      // primeiro uso concluído (ou "usar offline")
  char nickname[NICK_MAX_BYTES];
  uint8_t personality;    // Personality
  // Aparência
  uint8_t contrast;       // 1..255
  uint8_t rotate180;      // orientação
  uint8_t favoriteExpr;   // Expression
  uint8_t animLevel;      // 0 = calma, 1 = normal, 2 = animada
  uint16_t screenOffS;    // 0 = nunca; senão 30..3600
  uint8_t shortIntro;
  // Som e vibração (ignorados no Lite)
  uint8_t sound, silent, vibration;
  // Foco e tempo
  uint8_t pomoFocusMin, pomoShortMin, pomoLongMin, pomoCyclesBeforeLong, pomoAutoStart;
  uint32_t timerPresetS[TIMER_PRESET_COUNT];  // último = personalizado
  Reminder reminders[REMINDER_COUNT];
  uint8_t quietEnabled, quietStartH, quietEndH;  // período de descanso (exige hora válida)
  // Hora
  uint8_t tzIndex;        // índice em TZ_TABLE
  uint8_t clock24h;
  // Jogos
  uint8_t jumpEasy;       // 1 = início fácil (padrão)
  // Conectividade (somente preferências; senhas ficam fora)
  uint16_t apTimeoutMin;  // 0 = não encerra sozinho
  uint8_t weatherEnabled; // módulo opcional de clima, desligado por padrão
  uint8_t ntpEnabled;     // sincronização automática opcional, desligada por padrão
  uint8_t reserved[8];
};

// Expressões que podem ser desbloqueadas (bits em Progress::unlocked).
enum UnlockId : uint8_t { UNL_TIMIDO = 0, UNL_FOCADO, UNL_VITORIOSO, UNL_SATISFEITO, UNL_ESPANTADO, UNL_PRECISAO, UNL_COUNT };

struct Progress {
  uint32_t unlocked;         // bitmask UnlockId
  uint32_t pets;             // carinhos concluídos (com intervalo mínimo)
  uint32_t pomodorosDone;    // focos concluídos de verdade
  uint32_t reflexValid;      // rodadas válidas de reflexo (acumulado)
  uint16_t jumpBest;
  uint16_t fishBest;
  uint16_t reflexBestMs;     // 0 = sem recorde
  uint16_t timingBestPct100; // melhor proximidade em centésimos de % (0..10000)
  uint16_t timingBestErrMs;  // desempate: menor erro absoluto
  uint16_t timingBestTargetCs;
  uint32_t gamesPlayed;
  int16_t mood;              // -100..100
  uint16_t bond;             // 0..1000
  uint8_t reserved[8];
};

// Checkpoint de sessões de tempo para retomada após perda de energia.
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

void defaultSettings(Settings& s);
void defaultProgress(Progress& p);
// Corrige faixas inválidas (vindas de flash corrompida, backup antigo etc.).
void sanitizeSettings(Settings& s);

// Blob persistido: [magic 'TUBA'][kind][version][len 2B][crc 4B][payload]
enum class BlobKind : uint8_t { Settings = 1, Progress = 2, Checkpoint = 3 };
static const int BLOB_HEADER = 12;
static const uint8_t SETTINGS_VERSION = 1;
static const uint8_t PROGRESS_VERSION = 1;
static const uint8_t CHECKPOINT_VERSION = 1;

size_t encodeBlob(BlobKind k, uint8_t ver, const void* payload, size_t len, uint8_t* out, size_t cap);
enum class DecodeResult : uint8_t { Ok, Empty, BadMagic, BadKind, BadCrc, Migrated, TooNew };
// Copia para 'payload' (tamanho atual 'len'). Payload menor de versão antiga é migrado
// preenchendo os campos novos com os defaults já presentes em 'payload'.
DecodeResult decodeBlob(BlobKind k, uint8_t curVer, const uint8_t* in, size_t inLen, void* payload, size_t len);

}  // namespace tb
