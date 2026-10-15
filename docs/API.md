# API local do TubaBot v2

Implementada uma única vez em `core/app_api.cpp` e usada igual pelo ESP32 (HTTP) e pelo simulador (postMessage → WebAssembly). Respostas sempre em JSON UTF-8 com `"ok": true|false`.

## Sessão e segurança

- `GET /api/session` → `{needPin}`. Na rede própria do Tuba (`192.168.4.1`) não há PIN: a senha WPA2 da rede, única por aparelho, já é a barreira.
- `POST /api/session` `{pin?}` → `{token}`. Pela rede de casa é preciso o PIN de 6 dígitos que aparece no visor ao tentar (presença física).
- Toda alteração (`POST`) exige o cabeçalho `X-Tuba-Token`. Requisições com `Origin` de outro host são recusadas (403).
- Corpo máximo 4096 bytes (413). JSON validado com profundidade limitada.
- Senhas de Wi-Fi nunca são devolvidas (status, prefs, wifi e backup não as contêm). A senha da rede própria só aparece no visor.
- Rotas de desenvolvedor (`dev.*`) só existem no simulador com ferramentas de desenvolvedor; no firmware `devTools` é sempre falso.

## Leitura (GET, sem token)

| Rota | Conteúdo |
|---|---|
| `/api/status` | modelo (`v2`), versão, apelido, tela, expressão, tempo de atividade, hora (válida?), rede, `caps` (alto-falante, motor, IMU, microfone, bateria, RTC), saídas (som/vibração permitidos, volume, brilho aplicado), Pomodoro, 3 timers, cronômetro, avisos pendentes, toque, `motion` (pose, inclinação), `sound` (nível, música), `power` (bateria %, mV, carregando, USB), `weather`, `nextAlarm`, armazenamento, último evento |
| `/api/frame` | miniatura da tela 117×117 (1 pixel a cada 4) em RGB565 little-endian, base64 — o "espelho" do portal |
| `/api/options` | cidades do clima (capitais) e cores dos olhos (`key`, `name`, `hex`) |
| `/api/prefs` | todas as preferências |
| `/api/records` | progresso e recordes |
| `/api/collection` | expressões básicas e as 10 desbloqueáveis com critério e progresso |
| `/api/wifi` | rede de casa: configurada?, nome, estado, IP (sem senha) |
| `/api/tz` | fusos suportados (identificador IANA + rótulo) |
| `/api/ota` | se uma atualização é permitida agora e por quê |

## Alteração (POST, com token)

| Rota | Corpo | Observações |
|---|---|---|
| `/api/prefs` | qualquer subconjunto de `prefs` | tudo-ou-nada: valida antes; 422 com `errors:[{field,msg}]` |
| `/api/time` | `{epoch, tz?}` | epoch em segundos (2020–2100); `tz` IANA da lista |
| `/api/action` | `{action, ...}` | ver abaixo |
| `/api/backup` | `{}` | preferências + progresso (sem segredos) |
| `/api/restore` | conteúdo de um backup | preferências validadas; progresso só aumenta (nunca apaga conquistas) |
| `/api/reset` | `{scope, confirm}` | `wifi`+`ESQUECER`, `prefs`+`REDEFINIR`, `all`+`APAGAR TUDO` |
| `/api/wifi` | `{ssid, pass}` | senha vazia ou WPA de 8–63 caracteres |
| `/update` (só firmware) | multipart com o `.bin` + cabeçalhos `X-Tuba-Token` e `X-Tuba-Size` | recusa se houver Pomodoro/timer/cronômetro/jogo em andamento; valida magic 0xE9, chip ESP32-S3 (id 9) e tamanho |

Ações (`/api/action`): `pomodoro.toggle|skip|cancel`, `timer.start|pause|resume|cancel` (`slot` 0–2, `seconds` opcional 1–86399), `stopwatch.toggle|reset`, `alert.ack`, `alarm.toggle` (`slot` 0–2), `alarm.snooze`, `face` (`expr` desbloqueada), `preview` (`pattern`: boot, tap, double, purr, love, menu, timer, pomodoro, reminder, point, gameover, record, unlock, test, alarm, boop, giggle, dizzy, startled, yawn, babble, swipe, charge, lowbattery, breath — obedece ao hardware e às preferências), `diag.oled` (padrão de teste da tela), `config.open`, `ap.close`, `setup.offline`, `screen.wake`, `screen.sleep`, `weather.refresh`, `reboot`.

## Preferências (`prefs`)

`setupDone`, `nickname` (≤12 caracteres, cabe na tela), `personality` (`tranquilo|brincalhao`), `brightness` (1–255; o nome `contrast` da v1 também é aceito), `rotate180`, `favoriteExpr`, `animLevel` (0–2), `screenOffS` (0 ou 30–3600), `shortIntro`, `sound`, `silent`, `vibration`, `pomodoro{focusMin 1–120, shortMin 1–60, longMin 1–90, cyclesBeforeLong 1–12, autoStart}`, `timerPresetsS[6]` (1–86399), `reminders[3]{label 1–16, intervalMin 5–240, enabled}`, `quiet{enabled,startH,endH}`, `tz`, `clock24h`, `jumpEasy`, `apTimeoutMin` (0–120), `weatherEnabled`, `ntpEnabled`, `volume` (0–100), `eyeColor` (`ciano|menta|lilas|rosa|ambar|azul|coral|branco`), `eyeGlow`, `watchFace` (0 digital, 1 analógico, 2 Tuba), `imuReactions`, `micReactions`, `tiltLook`, `faceDownSnooze`, `nightDim`, `wakeOnPickup`, `alarms[3]{label 1–12, hour 0–23, minute 0–59, days bitmask 0–127 (bit0 = domingo; 0 = uma vez), enabled}`, `weather{city (chave da lista ou `custom`), latE4, lonE4}` (graus × 10⁴).

Backups da v1 (`version: 1`) podem ser importados: os campos conhecidos são aplicados e os novos ficam no padrão.
