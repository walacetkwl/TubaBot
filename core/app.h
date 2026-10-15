// Máquina de estados do TubaBot. Recebe toque, nível sonoro, energia e o
// relógio monotônico; produz a imagem 240×320 (com a área alterada) e o áudio
// sintetizado. O perfil de fábrica (Lite/Pro, cor, padrões) chega pela Platform.
// É o MESMO código no firmware, nos testes e no simulador (WebAssembly).
#pragma once
#include "gfx.h"
#include "touch.h"
#include "sensors.h"
#include "outputs.h"
#include "settings.h"
#include "timekeeping.h"
#include "games.h"
#include "face.h"
#include "json.h"
#include "ui.h"
#include "qr.h"
#include "factory.h"
#include "i18n.h"
#include "scenes.h"

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
  uint32_t heapFree = 0, heapMin = 0, psramFree = 0, flashSize = 0, sketchSize = 0, sketchFree = 0;
  char chip[24] = {0};
  char build[40] = {0};
  char deviceId[20] = {0};   // identificador do chip (MAC), p/ unidades sem número de série
  bool simulated = false;
};

enum class Service : uint8_t { UpdateManifest, Telemetry };

int versionCompare(const char* a, const char* b);   // "3.1.0" vs "3.0.2": <0, 0, >0

class Platform {
 public:
  virtual bool storageLoad(BlobKind k, uint8_t* buf, size_t cap, size_t* len) = 0;
  virtual bool storageSave(BlobKind k, const uint8_t* buf, size_t len) = 0;
  virtual void storageErase(BlobKind k) = 0;
  virtual void apOpen() = 0;
  virtual void apClose() = 0;
  virtual void netStatus(NetStatus& s) = 0;
  virtual bool wifiSet(const char* ssid, const char* pass) = 0;  // a senha nunca volta pela API
  virtual void wifiForget() = 0;
  virtual void sysInfo(SysInfo& s) = 0;
  virtual void requestReboot() = 0;
  virtual void applyDisplay(uint8_t brightness, bool rotate180) = 0;  // brilho 0 = tela desligada
  virtual void setTimezone(const char* posix) = 0;
  virtual bool busy() = 0;  // gravação/OTA em andamento
  virtual void log(const char* msg) = 0;
  virtual void rtcWrite(int64_t epoch) { (void)epoch; }  // relógio com bateria (se houver)
  // Perfil de fábrica (texto, ver factory.h). false = não gravado.
  virtual bool factoryLoad(char* buf, size_t cap, size_t* len) { (void)buf; (void)cap; *len = 0; return false; }
  // Serviços da TubaLabs na internet (vazio = desativado até existir servidor):
  // manifesto de atualização e coleta de telemetria. Ver docs/futuras-atualizacoes.html.
  virtual const char* serviceUrl(Service s) { (void)s; return ""; }
};

enum class Screen : uint8_t {
  Intro, FirstUse, Face, Clock, Today, Launcher, MoreApps, Quick, Pomodoro, Timers, TimerSet, Stopwatch, Alarms, AlarmEdit,
  GamesMenu, Game, Expressions, Breath, Level, Dice, Weather, Stats,
  Appearance, ColorPick, Overlays, Volume, Brightness, Wifi, System, Config, Info, Torch,
  Confirm, RecoveryConfirm, Resume, ScreenOff, Ota,
  Legends, Language, Update, UpdateOffer, Count
};
const char* screenName(Screen s);

static const uint16_t INTRO_FULL_MS = 2800, INTRO_SHORT_MS = 900;

enum class AlertType : uint8_t { Timer, PomoFocusDone, PomoBreakDone, Reminder, Alarm };
struct Alert { AlertType type; uint8_t index; ms_t at; };

enum class HwButton : uint8_t { Boot, Power };

struct ApiCtx {
  bool viaAP = true;
  bool devTools = false;
  const char* token = nullptr;
};

struct WeatherData {
  bool valid = false;
  int16_t tempC10 = 0, hiC10 = 0, loC10 = 0;
  uint8_t code = 0, humidity = 0, isDay = 1;
  struct Day { uint8_t code; int16_t hiC10, loC10; } days[3] = {};
  int nDays = 0;
  ms_t updatedAt = 0;
};
const char* weatherText(uint8_t wmo);
Icon weatherIcon(uint8_t wmo, bool day);

class App {
 public:
  static const int kTimers = 3;
  static const int kAlertQueue = 8;
  static const int kHits = 32;
  static const int kListMax = 40;
  static const int kLaps = 5;

  explicit App(Platform& p) : plat_(p) {}

  // O buffer de imagem (240×320 RGB565) vem da plataforma.
  void attachFramebuffer(uint16_t* px) { canvas_.attach(px, SCR_W, SCR_H); }
  // Inicialização: carrega preferências ANTES de qualquer saída.
  void begin(const HwCaps& caps, ms_t now, uint32_t seed, bool devBuild = false);

  // ── entradas ──
  void touchSample(bool down, int x, int y, ms_t t);
  void motionSample(int ax, int ay, int az, ms_t t) { if (caps_.imu) motion_.sample(ax, ay, az, t); }
  void soundLevel(int level, ms_t t) { if (caps_.mic && set_.micReactions) sound_.sample(level, t); }
  void powerUpdate(const PowerState& p, ms_t now);
  void hwButton(HwButton b, bool longPress, ms_t now);
  void rtcTime(int64_t epoch, ms_t now);  // hora lida do RTC no boot (não regrava)

  // ── laço ──
  bool tick(ms_t now);                 // true = um quadro foi desenhado
  const IRect& dirty() const { return dirty_; }
  void framePresented(ms_t t);
  uint32_t frameIntervalMs(ms_t now) const;
  void audioRender(int16_t* out, int n) { out_.synth().render(out, n); }
  bool motor() const { return out_.motor(); }

  // ── atualização pela internet (o firmware baixa; o núcleo decide quando e pergunta ao usuário) ──
  bool updateWant(ms_t now, char* url, size_t cap);                 // buscar o manifesto agora?
  void updateManifest(const char* body, size_t len, int httpCode, ms_t now);
  // Instalação autorizada (usuário tocou "Atualizar agora" ou chegou a hora agendada):
  // devolve URL do binário, SHA-256 (hex) e tamanho uma única vez; o firmware chama enterOta().
  bool updateInstallWant(char* url, size_t ucap, char* sha, size_t scap, uint32_t* size);
  void checkUpdatesNow(ms_t now);                                   // "Buscar atualizações"
  const char* pendingUpdate() const { return set_.updVersion; }
  // ── telemetria (painel da TubaLabs; nada de dados pessoais) ──
  bool telemetryWant(ms_t now, char* url, size_t ucap, StrBuf& body);
  void telemetryResult(int httpCode, ms_t now);
  void writeTelemetry(JsonOut& j, ms_t now);

  // ── clima (o firmware faz o HTTPS; o núcleo decide e interpreta) ──
  bool weatherWant(ms_t now, char* url, size_t cap);
  void weatherResult(const char* body, size_t len, int httpCode, ms_t now);
  const WeatherData& weather() const { return weather_; }

  // ── consulta ──
  Canvas& canvas() { return canvas_; }
  const Canvas& canvas() const { return canvas_; }
  Screen screen() const { return screen_; }
  const HwCaps& caps() const { return caps_; }
  const FactoryConfig& factory() const { return fac_; }
  const char* factoryIssue() const { return facWhy_; }
  bool lightTheme() const { return set_.theme != 0; }
  bool dockVisible(ms_t now) const { return screen_ == Screen::Face && now < dockUntil_; }
  const Settings& settings() const { return set_; }
  const Progress& progress() const { return testMode_ ? progTest_ : prog_; }
  OutputManager& outputs() { return out_; }
  TouchClassifier& input() { return input_; }
  MotionSensor& motion() { return motion_; }
  SoundSensor& sound() { return sound_; }
  CivilClock& clock() { return clock_; }
  const PowerState& power() const { return power_; }
  uint8_t brightnessApplied() const { return appliedBright_; }

  // ── ações (serial/USB, API, testes) ──
  void openConfig(ms_t now);
  void forgetWifi();
  void resetPreferences(ms_t now);
  void eraseAll(ms_t now);
  void factoryReset(ms_t now);   // volta aos padrões de fábrica DESTA unidade (apaga dados do usuário)
  bool otaAllowed(const char** reason) const;
  void enterOta(ms_t now);
  void otaProgress(int permille) { otaPermille_ = permille; forceFrame_ = true; }
  void otaFailed(ms_t now);
  void setTime(int64_t epoch, ms_t now);

  int api(const char* method, const char* path, const char* body, size_t bodyLen, const ApiCtx& ctx, StrBuf& out, ms_t now);
  const char* sessionToken() const { return token_; }
  const char* pairPin() const { return pin_; }

  uint32_t storageWrites() const { return writes_; }
  void flush(ms_t now, bool force);
  bool storageRecovered() const { return recovered_; }

  // testes
  Pomodoro& pomodoro() { return pomo_; }
  Countdown& timer(int i) { return timers_[i]; }
  Stopwatch& stopwatch() { return sw_; }
  JumpGame& jump() { return jump_; }
  ReflexGame& reflex() { return reflex_; }
  FishGame& fish() { return fish_; }
  TimingGame& timing() { return timing_; }
  DiveGame& dive() { return dive_; }
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
  // ponto de toque de um botão visível (testes/automação): false se não existe
  bool buttonPos(int id, int& x, int& y) const;
  int listIndexOf(int id) const;
  bool listItemPos(int id, int& x, int& y);
  Expr baseExpr() const { return baseExpr_; }
  // reações lendárias
  bool sceneActive() const { return scene_.active(); }
  SceneId sceneId() const { return scene_.id(); }
  bool medalVisible(ms_t now) const { return medalAt_ && now - medalAt_ < kMedalMs; }
  void playLegend(SceneId s, ms_t now, bool natural, Screen back = Screen::Face);
  int legendsSeen() const;
  static const uint32_t kMedalMs = 4200;

 private:
  // fluxo
  void handleTouch(const TouchEvent& e, ms_t now);
  void handleMotion(MotionEv e, ms_t now);
  void handleSound(SoundEv e, ms_t now);
  void onFaceTouch(const TouchEvent& e, ms_t now);
  void onButton(int id, ms_t now);
  void onListPick(int id, ms_t now);
  void onAlertTouch(const TouchEvent& e, ms_t now);
  void goHome(ms_t now, Screen page = Screen::Face);
  void goBack(ms_t now);
  void ackAlert(ms_t now, bool snooze);
  void pushAlert(AlertType t, uint8_t idx, ms_t now);
  void startAlertOutput(ms_t now);
  void updateTimeServices(ms_t now);
  void updateAlarms(ms_t now);
  void updatePersonality(ms_t now);
  void updateBrightness(ms_t now);
  void onGameOutcome(const GameOutcome& o, ms_t now);
  void unlock(UnlockId id, ms_t now);
  void checkRadiant(ms_t now);
  void addPet(ms_t now);
  void react(Expr e, ms_t now, uint16_t ms, Pattern snd = Pattern::None);
  void toast(const char* msg, ms_t now, uint16_t ms = 1800);
  void event(const char* msg);
  void applyPrefs(ms_t now);
  void markSettings(ms_t now) { setDirty_ = true; lastChange_ = now; }
  void markProgress(ms_t now) { if (!testMode_) { progDirty_ = true; lastChange_ = now; } }
  Progress& P() { return testMode_ ? progTest_ : prog_; }
  void writeCheckpoint(ms_t now);
  void loadAll(ms_t now);
  bool checkpointPending() const;
  void resumeFromCheckpoint(ms_t now);
  void wake(ms_t now);
  void sleepScreen(ms_t now);
  void noteInteraction(ms_t now);
  void setInputModeFor(Screen s);
  bool quietNow(ms_t now) const;
  Expr idleExpression(ms_t now);
  bool bedtimeNow(ms_t now, int* minsToBed = nullptr);
  int64_t localDay(ms_t now) const;
  int localMinute(ms_t now) const;          // -1 sem relógio
  void maybeLegend(ms_t now);
  SceneId chooseLegend(ms_t now, bool soundTriggered);
  void endScene(ms_t now);
  void abortScene(ms_t now);
  void noteActivity(ms_t now);
  void updateNet(ms_t now);
  void offerUpdate(ms_t now);
  void scheduleUpdate(int choice, ms_t now);   // 0 agora, 1 em 1 h, 2 esta noite
  void applyLanguage();
  int availableExprs(Expr* out, int max) const;
  void buildList(ms_t now, bool keepScroll = false);
  void cycleSetting(int id, ms_t now);
  void startBreath(ms_t now);
  void rollDice(ms_t now);
  bool homeScreen(Screen s) const { return s == Screen::Face || s == Screen::Clock || s == Screen::Today; }
  bool listScreen(Screen s) const;
  bool gridScreen(Screen s) const { return s == Screen::Launcher || s == Screen::MoreApps; }
  void applyTheme();
  void confirm(int action, ms_t now);
  void setSlider(int id, int x, bool commit, ms_t now);
  int nextAlarmIndex(ms_t now, int* minutesAway) const;
  FaceStyle faceStyle() const;
  // desenho
  void render(ms_t now);
  void hit(int id, float cx, float cy, float r);
  void hitRect(int id, float x, float y, float w, float h);
  void button(int id, float cx, float cy, float r, Color bg, Icon ic, Color fg);
  void pillButton(int id, float cx, float cy, float w, float h, Color bg, const char* label, Color fg);
  int hitTest(int x, int y) const;
  void drawFaceOverlays(ms_t now);
  void drawDock(ms_t now);
  void drawFaceScreen(ms_t now);
  void drawIntro(ms_t now);
  void drawUpdate(ms_t now);
  void drawUpdateOffer(ms_t now);
  void drawMedal(ms_t now);
  void drawClock(ms_t now);
  void drawToday(ms_t now);
  void drawGridScreen(const char* title, ms_t now);
  void drawListScreen(const char* title, Color col, ms_t now);
  void drawColorPick(ms_t now);
  void drawLevelScreen(int which, ms_t now);
  void drawQuick(ms_t now);
  void drawPomodoro(ms_t now);
  void drawTimers(ms_t now);
  void drawStopwatch(ms_t now);
  void drawAlarms(ms_t now);
  void drawAlarmEdit(ms_t now);
  void drawExpressions(ms_t now);
  void drawBreath(ms_t now);
  void drawLevel(ms_t now);
  void drawDice(ms_t now);
  void drawWeather(ms_t now);
  void drawStats(ms_t now);
  void drawConfigPages(bool firstUse, ms_t now);
  void drawInfo(ms_t now);
  void drawConfirm(ms_t now);
  void drawRecovery(ms_t now);
  void drawResume(ms_t now);
  void drawAlert(ms_t now);
  void drawHoldHint(ms_t now);
  void drawToast(ms_t now);
  void drawBackChevron(const char* title = nullptr, Color col = 0);
  void drawQr(const char* text, float cx, float cy, float size);
  // API
  int apiStatus(JsonOut& j, ms_t now);
  void writePrefs(JsonOut& j);
  void writeProgress(JsonOut& j);
  int applyPrefsJson(const JsonVal& v, JsonOut& err, int& errCount, ms_t now, bool dryRun);
  int apiAction(const JsonVal& v, JsonOut& j, ms_t now, bool devTools);

  Platform& plat_;
  HwCaps caps_;
  bool devBuild_ = false;
  Canvas canvas_;
  IRect dirty_;
  TouchClassifier input_;
  MotionSensor motion_;
  SoundSensor sound_;
  PowerState power_;
  OutputManager out_;
  Settings set_;
  Progress prog_, progTest_;
  bool testMode_ = false;
  TimeCheckpoint ckpt_;
  CivilClock clock_;
  bool clockFromRtc_ = false;
  FactoryConfig fac_;
  const char* facWhy_ = nullptr;
  Rng rng_;
  FaceRenderer face_;
  Pomodoro pomo_;
  Countdown timers_[kTimers];
  int timerSel_ = 0;
  Stopwatch sw_;
  uint64_t laps_[kLaps] = {0};
  int lapN_ = 0;
  ms_t reminderDue_[REMINDER_COUNT] = {0, 0, 0};
  bool reminderPending_[REMINDER_COUNT] = {false, false, false};
  int64_t alarmFiredMinute_[ALARM_COUNT] = {0, 0, 0};
  ms_t alarmSnoozeUntil_[ALARM_COUNT] = {0, 0, 0};
  int editAlarm_ = 0;

  JumpGame jump_; ReflexGame reflex_; FishGame fish_; TimingGame timing_; DiveGame dive_;
  Game* game_ = nullptr;
  GameId gameId_ = GameId::Jump;

  Screen screen_ = Screen::Intro;
  Screen returnScreen_ = Screen::Face;   // Confirm/Recovery/ScreenOff/Torch
  Screen homePage_ = Screen::Face;
  ms_t screenAt_ = 0, bootAt_ = 0, lastInteraction_ = 0;
  int transDir_ = 0;                     // −1/+1: deslizar a página que entra
  // botões / listas
  struct Hit { int16_t id; int16_t x0, y0, x1, y1; bool circle; };
  Hit hits_[kHits];
  int hitN_ = 0;
  int pressedHit_ = -1;
  ui::ListView list_;
  ui::ListItem items_[kListMax];
  char itemLabels_[kListMax][40];
  char itemSubs_[kListMax][40];
  int itemN_ = 0;
  int pressedItem_ = -1;
  bool listTouch_ = false;
  int exprSel_ = 0, infoPage_ = 0, configPage_ = 0, quickDrag_ = -1, quickArm_ = -1;  // controle deslizante: armado no toque, arrastando
  int confirmAction_ = 0;
  // alertas
  Alert alerts_[kAlertQueue];
  int alertN_ = 0;
  bool alertPausedGame_ = false;
  // personalidade / rosto
  Expr baseExpr_ = Expr::Happy;
  ms_t exprUntil_ = 0, nextIdleExpr_ = 0, lastPetAt_ = 0, lookUntil_ = 0;
  int lookX_ = 0, lookY_ = 0;
  int energy_ = 80;
  ms_t lastMinuteTick_ = 0, danceSince_ = 0, danceCredit_ = 0;
  ms_t nextDrift_ = 0;
  bool lowBatWarned_ = false;
  ms_t wakeGuard_ = 0;
  bool suppressSwipe_ = false;
  ms_t dockUntil_ = 0;          // controles sobre o rosto (aparecem ao tocar)
  ms_t nextClockAct_ = 0;       // relógio "Tuba": próxima olhadinha/caretinha
  int64_t sleepDay_ = -1;       // dia (local) do sorteio da hora de dormir
  int bedMin_ = 20 * 60 + 60, wakeMin_ = 6 * 60 + 30;
  // respiração / sorteio
  bool breathOn_ = false;
  uint8_t breathMode_ = 0;
  ms_t breathStart_ = 0;
  int breathPhaseLast_ = -1;
  uint8_t diceMode_ = 0;
  int diceResult_ = 0;
  ms_t rollUntil_ = 0;
  // clima
  WeatherData weather_;
  ms_t weatherTryAt_ = 0;
  bool weatherFailed_ = false;
  // toasts e diálogos
  char toast_[48] = {0};
  ms_t toastUntil_ = 0, recoveryUntil_ = 0;
  int otaPermille_ = 0;
  ms_t diagUntil_ = 0;
  // brilho
  uint8_t appliedBright_ = 255;
  bool appliedRotate_ = false;
  int appliedTheme_ = -1;
  uint32_t appliedAccent_ = 0;
  // persistência
  bool setDirty_ = false, progDirty_ = false, ckptDirty_ = false;
  ms_t lastChange_ = 0, lastSave_ = 0;
  uint32_t writes_ = 0;
  bool recovered_ = false;
  // API
  char token_[17] = {0};
  char pin_[7] = {0};
  char lastEvent_[48] = {0};
  uint32_t eventSeq_ = 0;
  bool forceFrame_ = true;
  ms_t lastRender_ = 0;
  // reações lendárias
  ScenePlayer scene_;
  bool sceneNew_ = false;          // primeira vez: medalha ao terminar
  Screen sceneBack_ = Screen::Face;
  ms_t medalAt_ = 0;
  SceneId medalScene_ = SceneId::Relogio;
  ms_t nextLegendCheck_ = 0;
  int64_t legendRollDay_ = -999999;
  bool legendToday_ = false;
  int legendAtMin_ = -1;
  // rede, atualização e telemetria
  bool staUp_ = false;
  ms_t netPollAt_ = 0;
  ms_t updCheckAt_ = 0, updLastCheck_ = 0, updDueMs_ = 0;
  bool updChecking_ = false, updManual_ = false, updInstallReq_ = false, updOffer_ = false;
  int8_t updResult_ = 0;           // 0 nunca, 1 em dia, 2 disponível, -1 falha
  char updUrl_[160] = {0};
  char updSha_[65] = {0};
  uint32_t updSize_ = 0;
  char updNotes_[120] = {0};
  ms_t telAt_ = 0;
  uint8_t telFails_ = 0;
  int lastLang_ = -1;
  ms_t lastActAt_ = 0;
  // QR (cache)
  QrCode qr_;
  char qrText_[96] = {0};
};

}  // namespace tb
