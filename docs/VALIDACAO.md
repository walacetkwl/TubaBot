# Validação — o que foi provado, como, e o que depende das peças

Data: 13/10/2026 · versão do núcleo 0.9.0.

## Legenda

- **SIM** — implementado e testado (testes nativos e/ou simulador)
- **COMP** — compilado para RISC-V rv32imc contra os headers reais do Arduino-ESP32 2.0.17 (verificação, ver limites abaixo)
- **HW** — pendente de hardware/bancada
- **NÃO** — não implementado

## Resultados reais desta entrega

| Verificação | Comando | Resultado |
|---|---|---|
| Testes nativos do núcleo | `make test` | **34 testes, 21.158 verificações, 0 falhas** (também com AddressSanitizer + UBSan: 0 falhas) |
| Mesmo núcleo no simulador | `make crosscheck` | 10 checkpoints com CRC do framebuffer **idênticos** entre o binário nativo e o WebAssembly do simulador |
| Simulador e portal no navegador | `make e2e` (Chromium 141 headless) | 17/17 verificações, sem erros de JavaScript e sem nenhum recurso externo |
| Compilação do firmware | `tools/check_firmware_rv32.sh <arduino-esp32 2.0.17> full|lite` | 16/16 arquivos compilam nos dois modelos; avisos só de headers do framework e nenhum no código do TubaBot |
| Portal | `tools/embed_portal.py` | 40.051 → 13.393 bytes (gzip); verificação de recursos externos: nenhum |

### Limites honestos da compilação

O registro do PlatformIO e os servidores da Espressif estavam **bloqueados** no ambiente onde o projeto foi escrito (`dl.registry.platformio.org`, `api.registry.platformio.org`, `dl.espressif.com`, `espressif.github.io` → HTTP 403 do proxy). Por isso:

- **`pio run` não foi executado.** Nenhum `.bin` foi gerado nem é entregue — não há binário fabricado.
- A verificação usou o código-fonte do **Arduino-ESP32 2.0.17** (clonado do GitHub) com o `riscv64-unknown-elf-g++ 13.2` do Ubuntu, `-march=rv32imc_zicsr_zifencei -mabi=ilp32`, headers genéricos do newlib e da libstdc++ configurada para newlib. Ela prova que o código do TubaBot é aceito pelas APIs reais (WiFi, WebServer, DNSServer, ESPmDNS, Preferences, Update, Wire, esp_timer, esp_ota) e mede o código próprio: **~76 KB de .text** (núcleo ~62 KB + adaptadores ~14 KB). Não prova link, tamanho final da imagem, heap em execução nem compatibilidade com o toolchain `riscv32-esp`.
- Primeiro passo na bancada: `pio run -e tubabot -e tubabot_lite` no Mac (README_MAC.md) e anotar aqui tamanho da imagem e heap livre (`status` no console).

## Critérios de aceite (seção 15 do briefing)

| # | Critério | Status | Evidência |
|---|---|---|---|
| 1 | Mesmo core no simulador e no firmware; framebuffer e regras idênticos | SIM · COMP | `make crosscheck`; o firmware compila os mesmos `core/*.cpp` |
| 2 | Boot respeita silencioso desde a 1ª saída e funciona sem rede | SIM | `boot_silent_first_output`, `boot_full_plays_and_lite_never`, `first_use_opens_ap_and_offline_option` |
| 3 | Matriz Lite/completo/som/silencioso/vibração em todos os alertas; corte imediato | SIM | `alert_policy_matrix` (2×2×2×2 × 14 padrões), `alert_cut_immediately_when_disabled`, `lite_api_cannot_enable_hardware` |
| 4 | Toques simples/duplo/médio/longo, sensor preso, ruído, recuperação sem duplicar | SIM | `gesture_exclusive_classification`, `recovery_hold_8s_opens_config_once`, `screen_off_wake_consumes_gesture` |
| 5 | Jogo sem atraso de duplo clique e sem repetição ao segurar | SIM | `game_mode_immediate_no_repeat`, `fish_hold_does_not_autocapture_in_app` |
| 6 | Pula, Tuba!: trajetórias possíveis, espaçamento, pontuação única, buffer, FPS | SIM | `jump_always_possible_many_seeds` (2 conjuntos de física × 40 sementes), `jump_gap_and_trajectory`, `jump_score_once_and_fps_independent`, `jump_buffer_consumed_once` |
| 7 | Hora do Bote: 100% dentro; 1 px fora rejeita; acelera; nova borda | SIM | `fish_fully_inside_rule`, `fish_capture_speedup_and_miss` |
| 8 | Reflexo: antecipado inválido; mede após apresentação; início não conta | SIM | `reflex_rules` |
| 9 | No Tempo Certo: 3,00 e 8,00 possíveis, nunca fora; 3-2-1-VAI; 100%; 7,43/7,50; sem texto arredondado; limite | SIM | `timing_targets_and_formula` (20.000 sorteios), `timing_flow_countdown_and_limit`, `timing_press_held_through_go_does_not_stop` |
| 10 | Timers/Pomodoro fora da tela; pausar mantém; conclusão única; alerta em jogo justo | SIM | `timers_pomodoro_offscreen_pause_single_completion`, `alert_during_game_pauses_fairly` |
| 11 | Cronômetro sobe e não tem alarme | SIM | `stopwatch_counts_up_no_alarm` |
| 12 | Hora civil não altera durações; reboot sem hora não inventa data | SIM | `civil_time_independent_of_durations` |
| 13 | Preferências/conquistas sobrevivem; esquecer Wi-Fi preserva; reset total | SIM (NVS real: HW) | `persistence_survives_reboot_and_resets`, `corrupted_storage_recovers_defaults`, `power_loss_resume_offer`, `dev_unlock_uses_separate_test_profile` |
| 14 | Portal offline, sem assets externos, valida erros e acentos, reabre sem roteador | SIM (Wi-Fi real: HW) | `api_auth_validation_and_secrets`, `make e2e` (fases simulador e HTTP, inclusive PIN na rede de casa) |
| 15 | Build dos dois modelos, flash nas partições, falhas de periférico | COMP · HW | verificação rv32 dos dois modelos; imagem final e partições: HW |

## Funcionalidades por área

| Área | Status |
|---|---|
| Abertura (barbatana, TubaBot, olhos, saudação; curta/completa) | SIM |
| Expressões, coração verdadeiro, piscar/olhar natural, personalidade, carinho | SIM (aparência no OLED real: HW) |
| Coleção e desbloqueios | SIM |
| 4 jogos | SIM (sensação de toque e latência real: HW) |
| Pomodoro, 3 timers, cronômetro, lembretes, retomada após queda | SIM |
| Política central de som/vibração | SIM · COMP (níveis/polaridades: HW) |
| Portal (assistente, abas, backup/importação, reset em 3 níveis, diagnóstico) | SIM |
| Rede própria (AP com senha por aparelho, DNS cativo, mDNS), rede de casa opcional com tentativas espaçadas | COMP · HW |
| OTA local (token, recusa com tarefa ativa, magic/chip/tamanho, progresso no visor) | COMP · HW (queda de energia durante OTA: não validada) |
| Console serial de recuperação | COMP · HW |
| Driver OLED (0x3C/0x3D, páginas alteradas, recuperação do barramento, SH1106 opcional) | COMP · HW |
| Toque por interrupção com timestamp | COMP · HW |
| NTP opcional (desligado por padrão) | COMP · HW |
| Clima/previsão | NÃO (ver FUNCIONALIDADES.md §8) |

## Testes físicos pendentes

Veja `docs/BANCADA.md`. Resumo: OLED real e endereço; toque através do plástico e tempo de resposta; polaridades e tensões dos módulos; corrente do MV50; toques falsos com vibração; alimentação e reconexão USB; AP/captive portal em iPhone e Android; funcionamento contínuo (24 h); OTA interrompida; heap mínima ao longo do tempo; custo do envio I²C por quadro.
