# Inventário: referências consultadas, decisões e contradições

## Referências efetivamente analisadas

| Fonte | Versão/acesso | Uso |
|---|---|---|
| YETI (`imsaar/yeti_claude`) | commit `ce4ccfcc37e9d13fc4488891b68a26164207d51e` (8/4/2026), clonado em 13/10/2026 | inspiração de comportamento; nenhum arquivo, código ou asset copiado |
| LP `tubalabs.com.br/tubabot/` — `feature-demos.js?v=2.2.0` | lido em 13/10/2026 | intenção de uso dos jogos, Pomodoro, personalidades e política de alertas |
| Prévia antiga (`…chatgpt.site`) | não acessada (restrita, segundo o briefing) | — |
| Anúncios do Mercado Livre | não acessados | características marcadas "a confirmar" |
| Arduino-ESP32 2.0.17 (código-fonte, GitHub) | tag `2.0.17` | verificação de compilação contra as APIs reais |

Licença da referência: Apache-2.0. Como nada foi copiado (fonte, sprites, sons e textos são do TubaBot; a tabela de fonte 5×7 é o desenho clássico de domínio público, digitado para o projeto), não há avisos de atribuição obrigatórios a preservar. Não há crédito promocional da referência na abertura nem no portal.

## README × código da referência (divergências encontradas)

- Gestos: o código emite **médio e depois longo na mesma pressão** (os dois disparam ao cruzar os limiares) e o toque simples só sai após 400 ms; o TubaBot classifica de forma exclusiva e decide na soltura.
- Buzzer: o código usa `ledcWriteTone` (frequências, tema Star Wars) — exige **buzzer passivo**; o TubaBot comprou buzzer **ativo** → padrões liga/desliga.
- Boot: a referência toca o som de abertura antes de existir preferência de silêncio e pode bloquear na conexão Wi-Fi antes de mostrar o rosto; o TubaBot carrega preferências antes de qualquer saída e nunca espera rede.
- AP: senha universal fixa (`123456789`) e SSID fixo; o TubaBot usa senha por aparelho e SSID com sufixo do MAC.
- Pinos: SDA 8 / SCL 9 (strapping no C3) e fallback do toque para GPIO5 (conflita com o buzzer).

## Matriz de funcionalidades

| Funcionalidade | Origem | Depende de | Decisão | Teste |
|---|---|---|---|---|
| Expressões + piscar/olhar | referência, LP | OLED | paramétricas, novas de contexto | telas + `same_inputs_same_frames_deterministic` |
| Olhos de coração | briefing | OLED | lóbulos + entalhe + ponta | `docs/telas.png` |
| Carinho/amor | referência, briefing | TTP223 | médio/longo exclusivos, prévia ao segurar | `recovery_hold…`, `persistence…` |
| Personalidades | LP | — | Tranquilo/Brincalhão mudam ritmo, não recursos | — (comportamental) |
| Desbloqueios | briefing | NVS | tabela proposta, perfil de teste separado | `dev_unlock_uses_separate_test_profile` |
| 4 jogos | LP + briefing | TTP223, OLED | regras do briefing; física do salto ajustada | §15 itens 5–9 |
| Pomodoro/timers/cronômetro/lembretes | LP + briefing | relógio monotônico | 3 timers; checkpoints | §15 itens 10–12 |
| Política de alertas | LP (`alertPolicy`) + briefing | buzzer ativo, MV50 | central, verificada a cada atualização | `alert_policy_matrix` |
| Portal local | referência + briefing | Wi-Fi AP | página única na flash, sem CDN | `make e2e` |
| Relógio por NTP | referência | rede de casa | opcional, desligado | HW |
| Clima | referência + LP | internet | **não implementado** | — |
| Mensagem entre TubaBots | ideia antiga | internet | **descartado pelo briefing** | — |

## Contradições concretas e como foram resolvidas

1. **Buzzer ativo × melodias** (LP sintetiza 660/880/990 Hz; YETI toca Star Wars): o firmware só faz liga/desliga ritmado; o simulador usa um tom fixo e diz isso na tela.
2. **Sensor de temperatura ausente**: nenhuma tela mostra temperatura; o sensor interno do chip não é usado como "temperatura do cômodo".
3. **Relógio sem bateria**: hora civil só é válida após ajuste; depois de perder energia aparece "Hora não ajustada". Timers e jogos usam tempo monotônico.
4. **Física do Pula, Tuba!** (briefing propôs 120/280; pediu "fácil, ágil e responsivo"): adotado 140/400, mantendo os valores do briefing testáveis.
5. **Toque durante a contagem do No Tempo Certo** (LP demo não trata): ignorado; precisa nova borda depois do VAI.
6. **Hora do Bote na LP** (peixe em senoide, `fishInside(x) = x-8>=50 && x+6<=78`, aceleração por quadro): substituído por travessia horizontal linear, bounding box completa e aceleração por captura independente de quadros.
7. **"Duplo toque em alerta"**: reconhece uma vez (sem ação duplicada), conforme o mapa do briefing.

## Pendências de hardware registradas

Ver `docs/HARDWARE.md` §3: controlador e endereço do OLED, jumpers do TTP223, polaridade/transistor do buzzer, driver/corrente do MV50, flash real da Super Mini.
