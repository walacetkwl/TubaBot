# API local do TubaBot

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
| `/api/status` | modelo, versão, apelido, tela, expressão, tempo de atividade, hora (válida?), rede, saídas permitidas, Pomodoro, 3 timers, cronômetro, avisos pendentes, sensor, armazenamento, último evento |
| `/api/frame` | framebuffer atual 128×64 em base64 (layout de páginas do SSD1306) — usado na prévia do portal |
| `/api/prefs` | todas as preferências |
| `/api/records` | progresso e recordes |
| `/api/collection` | expressões básicas e desbloqueáveis com critério e progresso |
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
| `/update` (só firmware) | multipart com o `.bin` + cabeçalhos `X-Tuba-Token` e `X-Tuba-Size` | recusa se houver Pomodoro/timer/cronômetro/jogo em andamento; valida magic 0xE9, chip ESP32-C3 e tamanho |

Ações (`/api/action`): `pomodoro.toggle|skip|cancel`, `timer.start|pause|resume|cancel` (`slot` 0–2, `seconds` opcional 1–86399), `stopwatch.toggle|reset`, `alert.ack`, `face` (`expr` desbloqueada), `preview` (`pattern`: boot, tap, double, purr, love, menu, timer, pomodoro, reminder, point, gameover, record, unlock, test — obedece ao perfil e às preferências), `diag.oled`, `config.open`, `ap.close`, `setup.offline`, `screen.wake`, `reboot`.

## Preferências (`prefs`)

`setupDone`, `nickname` (≤12 caracteres), `personality` (`tranquilo|brincalhao`), `contrast` (1–255), `rotate180`, `favoriteExpr`, `animLevel` (0–2), `screenOffS` (0 ou 30–3600), `shortIntro`, `sound`, `silent`, `vibration`, `pomodoro{focusMin 1–120, shortMin 1–60, longMin 1–90, cyclesBeforeLong 1–12, autoStart}`, `timerPresetsS[6]` (1–86399), `reminders[3]{label 1–16, intervalMin 5–240, enabled}`, `quiet{enabled,startH,endH}`, `tz`, `clock24h`, `jumpEasy`, `apTimeoutMin` (0–120), `weatherEnabled` (sem efeito: módulo não implementado), `ntpEnabled`.
