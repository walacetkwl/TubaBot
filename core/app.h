// Máquina de estados do TubaBot. Recebe amostras do sensor e o relógio
// monotônico; produz o framebuffer e os níveis de buzzer/motor.
// É o MESMO código no firmware, nos testes e no simulador (WebAssembly).
#pragma once
#include "framebuffer.h"
#include "input.h"
#include "outputs.h"
#include "settings.h"
#include "timekeeping.h"
#include "games.h"
#include "face.h"
#include "json.h"

namespace tb {

// ── Porta para a plataforma (ESP32 ou simulador) ────────────────────────────
enum class StaState : uint8_t { Off, Connecting, Connected, Failed };
struct NetStatus {
  bool apActive = false;
  char apSsid[33] = {0};
  char apPass[65] = {0};
  char apIp[16] = {0};
  char mdns[40] = {0};
  uint32_t apSecondsLeft = 0;  // 0 = sem limite
  bool staConfigured = false;
  StaState sta = StaState::Off;
  char staSsid[33] = {0};
  char staIp[16] = {0};
  bool radioOn = false;
};
struct SysInfo {
  uint32_t heapFree = 0, heapMin = 0, flashSize = 0, sketchSize = 0, sketchFree = 0;
  char chip[24] = {0};
  char build[40] = {0};
  bool simulated = false;
};

class Platform {
 public:
  virtual bool storageLoad(BlobKind k, uint8_t* buf, size_t cap, size_t* len) = 0;
  virtual bool storageSave(BlobKind k, const uint8_t* buf, size_t len) = 0;
  virtual void storageErase(BlobKind k) = 0;
  virtual void apOpen() = 0;
  virtual void apClose() = 0;
  virtual void netStatus(NetStatus& s) = 0;
  // Rede doméstica (opcional). A senha nunca volta pela API.
  virtual bool wifiSet(const char* ssid, const char* pass) = 0;
  virtual void wifiForget() = 0;
  virtual void sysInfo(SysInfo& s) = 0;
  virtual void requestReboot() = 0;
  virtual void applyDisplay(uint8_t contrast, bool rotate180) = 0;
  virtual void setTimezone(const char* posix) = 0;
  virtual bool busy() = 0;  // gravação/OTA em andamento
  virtual void log(const char* msg) = 0;
};

enum class Screen : uint8_t {
  Intro, FirstUse, Face, Menu, Clock, Pomodoro, Timers, Stopwatch, GamesMenu, Game,
  Expressions, Config, Info, Actions, Confirm, RecoveryConfirm, Resume, ScreenOff, Ota,
};
const char* screenName(Screen s);

enum class AlertType : uint8_t { Timer, PomoFocusDone, PomoBreakDone, Reminder };
struct Alert { AlertType type; uint8_t index; ms_t at; };

struct ApiCtx {
  bool viaAP = true;       // requisição chegou pela rede própria do TubaBot
  bool devTools = false;   // ferramentas de desenvolvedor (apenas simulador)
  const char* token = nullptr;  // cabeçalho X-Tuba-Token
};

class App {
 public:
  static const int kTimers = 3;
  static const int kAlertQueue = 8;
  static const int kListMax = 12;

  explicit App(Platform& p) : plat_(p) {}

  // Inicialização: carrega preferências ANTES de qualquer saída (bipe de boot respeita silencioso).
  void begin(Model model, ms_t now, uint32_t seed, bool devBuild = false);
  // Entrada bruta do TTP223 (true = tocado), com timestamp da borda.
  void touchSample(bool level, ms_t t) { input_.sample(level, t); }
  // Avança a lógica; retorna true se o framebuffer mudou.
  bool tick(ms_t now);
  // O firmware/simulador avisa quando o quadro foi efetivamente apresentado.
  void framePresented(ms_t t);

  const Framebuffer& frame() const { return fb_; }
  bool buzzer() const { return out_.buzzer(); }
  bool motor() const { return out_.motor(); }
  Screen screen() const { return screen_; }
  Model model() const { return model_; }
  const Settings& settings() const { return set_; }
  const Progress& progress() const { return prog_; }
  OutputManager& outputs() { return out_; }
  GestureClassifier& input() { return input_; }
  CivilClock& clock() { return clock_; }
  // Intervalo sugerido até o próximo tick/quadro (ms).
  uint32_t frameIntervalMs(ms_t now) const;

  // Ações acessíveis por serial/USB e testes.
  void openConfig(ms_t now);
  void forgetWifi();
  void resetPreferences(ms_t now);
  void eraseAll(ms_t now);
  bool otaAllowed(const char** reason) const;
  void enterOta(ms_t now);
  void otaProgress(int permille) { otaPermille_ = permille; }
  void otaFailed(ms_t now);
  void setTime(int64_t epoch, ms_t now);

  // API local: devolve status HTTP; corpo JSON em 'out'.
  int api(const char* method, const char* path, const char* body, size_t bodyLen, const ApiCtx& ctx, StrBuf& out, ms_t now);
  const char* sessionToken() const { return token_; }
  const char* pairPin() const { return pin_; }

  // Persistência (teste/diagnóstico)
  uint32_t storageWrites() const { return writes_; }
  void flush(ms_t now, bool force);
  bool storageRecovered() const { return recovered_; }

  // Testes: acesso aos subsistemas
  Pomodoro& pomodoro() { return pomo_; }
  Countdown& timer(int i) { return timers_[i]; }
  Stopwatch& stopwatch() { return sw_; }
  JumpGame& jump() { return jump_; }
  ReflexGame& reflex() { return reflex_; }
  FishGame& fish() { return fish_; }
  TimingGame& timing() { return timing_; }
  Game* activeGame() { return game_; }
  GameId activeGameId() const { return gameId_; }
  FaceRenderer& face() { return face_; }
  int alertCount() const { return alertN_; }
  const Alert* currentAlert() const { return alertN_ ? &alerts_[0] : nullptr; }
  void startGame(GameId g, ms_t now);
  void gotoScreen(Screen s, ms_t now);
  const char* lastEvent() const { return lastEvent_; }
  uint32_t eventSeq() const { return eventSeq_; }
  bool testProgress() const { return testMode_; }

 private:
  // fluxo
  void handleGesture(const GestureEvent& e, ms_t now);
  void onFace(Gesture g, ms_t now);
  void onMenu(Gesture g, ms_t now);
  void onList(Gesture g, ms_t now);
  void activateMenuItem(int id, ms_t now);
  void activateAction(int id, ms_t now);
  void openActions(ms_t now);
  void ackAlert(ms_t now, bool snooze);
  void pushAlert(AlertType t, uint8_t idx, ms_t now);
  void startAlertOutput(ms_t now);
  void updateTimeServices(ms_t now);
  void updatePersonality(ms_t now);
  void onGameOutcome(const GameOutcome& o, ms_t now);
  void unlock(UnlockId id, ms_t now);
  void addPet(ms_t now);
  void toast(const char* msg, ms_t now, uint16_t ms = 1800);
  void event(const char* msg);
  void applyPrefs(ms_t now);
  void markSettings(ms_t now) { setDirty_ = true; lastChange_ = now; }
  void markProgress(ms_t now) { progDirty_ = true; lastChange_ = now; }
  void writeCheckpoint(ms_t now);
  void loadAll(ms_t now);
  bool checkpointPending() const;
  void resumeFromCheckpoint(ms_t now);
  void wake(ms_t now);
  void noteInteraction(ms_t now);
  void setInputModeFor(Screen s, ms_t now);
  bool reminderBlocked(ms_t now) const;
  Expr idleExpression(ms_t now);
  int availableExprs(Expr* out, int max) const;
  // desenho
  void render(ms_t now);
  void drawIntro(ms_t now);
  void drawList(const char* title, ms_t now);
  void drawHoldHint(ms_t now);
  void drawAlert(ms_t now);
  void drawStatusIcons();
  void drawConfigPages(bool firstUse, ms_t now);
  void drawInfo(ms_t now);
  void drawClock(ms_t now);
  void drawPomodoro(ms_t now);
  void drawTimers(ms_t now);
  void drawStopwatch(ms_t now);
  void drawExpressions(ms_t now);
  void drawResume(ms_t now);
  void drawConfirm(ms_t now);
  // API helpers
  int apiStatus(JsonOut& j, ms_t now);
  void writePrefs(JsonOut& j);
  void writeProgress(JsonOut& j);
  int applyPrefsJson(const JsonVal& v, JsonOut& err, int& errCount, ms_t now, bool dryRun);
  int apiAction(const JsonVal& v, JsonOut& j, ms_t now, bool devTools);

  Platform& plat_;
  Model model_ = Model::Lite;
  bool devBuild_ = false;
  Framebuffer fb_;
  Framebuffer prev_;
  GestureClassifier input_;
  OutputManager out_;
  Settings set_;
  Progress prog_;
  Progress progTest_;      // progresso de teste do simulador (separado)
  bool testMode_ = false;
  TimeCheckpoint ckpt_;
  CivilClock clock_;
  Rng rng_;
  FaceRenderer face_;
  Pomodoro pomo_;
  Countdown timers_[kTimers];
  int timerSel_ = 0;
  Stopwatch sw_;
  ms_t reminderDue_[REMINDER_COUNT] = {0, 0, 0};
  bool reminderPending_[REMINDER_COUNT] = {false, false, false};

  JumpGame jump_; ReflexGame reflex_; FishGame fish_; TimingGame timing_;
  Game* game_ = nullptr;
  GameId gameId_ = GameId::Jump;

  Screen screen_ = Screen::Intro;
  Screen returnScreen_ = Screen::Face;  // para Ações/Confirmação/ScreenOff
  ms_t screenAt_ = 0;
  ms_t bootAt_ = 0;
  ms_t lastInteraction_ = 0;
  // listas
  struct ListItem { const char* label; int id; };
  ListItem list_[kListMax];
  char listLabels_[kListMax][24];
  int listN_ = 0, listSel_ = 0;
  int menuSel_ = 0, gameSel_ = 0, exprSel_ = 0;
  int infoPage_ = 0, configPage_ = 0;
  int confirmAction_ = 0;
  bool clockSeconds_ = false;
  // alertas
  Alert alerts_[kAlertQueue];
  int alertN_ = 0;
  bool alertPausedGame_ = false;
  // personalidade / rosto
  Expr baseExpr_ = Expr::Happy;
  ms_t exprUntil_ = 0;    // reação temporária (carinho/amor) até este instante
  ms_t nextIdleExpr_ = 0;
  ms_t lastPetAt_ = 0;
  bool petCounted_ = false;
  int energy_ = 80;
  ms_t lastMinuteTick_ = 0;
  // toasts
  char toast_[40] = {0};
  ms_t toastUntil_ = 0;
  // recuperação
  ms_t recoveryUntil_ = 0;
  // OTA
  int otaPermille_ = 0;
  // persistência
  bool setDirty_ = false, progDirty_ = false, ckptDirty_ = false;
  ms_t lastChange_ = 0, lastSave_ = 0;
  uint32_t writes_ = 0;
  bool recovered_ = false;
  // sessão da API
  char token_[17] = {0};
  char pin_[7] = {0};
  bool netStatusCached_ = false;
  // log
  char lastEvent_[48] = {0};
  uint32_t eventSeq_ = 0;
  bool forceFrame_ = true;
  ms_t lastRender_ = 0;
  bool stuckShown_ = false;
  ms_t diagUntil_ = 0;   // padrão de teste do OLED (diagnóstico)
};

}  // namespace tb
