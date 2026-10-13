# Funcionalidades, gestos, regras dos jogos e matriz de alertas

Tudo abaixo está implementado em `core/` e roda igual no firmware, no simulador e nos testes.

## 1. Gestos (um único TTP223)

Tempos (`core/input.h`, ajustáveis): debounce 20 ms · janela de duplo toque 320 ms · média 1 s · longa 3 s · recuperação 8 s · saída de jogo 1,5 s · sensor preso 20 s.

**Classificação exclusiva:** cada interação gera no máximo UM evento, decidido ao soltar (simples OU duplo OU médio OU longo OU recuperação). Durante a pressão há só prévias visuais (olhos relaxados, corações, barra de configuração). Um toque de 8 s não gera carinho, amor nem nada além da recuperação. O instante de cada evento é o da borda real do sinal (a latência do debounce é descontada).

| Contexto | Toque curto | Duplo toque | Médio (≥1 s) | Longo (≥3 s) | ≥8 s e soltar |
|---|---|---|---|---|---|
| Rosto | próxima expressão | menu | carinho (olhos relaxados → ^^) | amor (olhos de coração) | confirmar configuração |
| Menu | próximo item | voltar ao rosto | selecionar | selecionar | idem |
| Informações | próxima página | voltar | página Rede: abrir configuração; demais: voltar | idem ao médio | idem |
| Pomodoro / Timers / Cronômetro | iniciar / pausar / retomar | ações (pular, cancelar, zerar, duração…) | voltar ao menu **sem** cancelar | idem | idem |
| Lista de ações | próximo | voltar | escolher | escolher | idem |
| Confirmação | não | não | **sim** | sim | idem |
| Alerta | reconhecer | reconhecer (uma única vez) | lembrete: adiar 10 min; outros: reconhecer e ir ao menu | reconhecer e ir ao menu | idem |
| Jogo | **ação imediata** na borda do toque (sem esperar duplo toque) | — | — | — | (segurar 1,5 s sai do jogo, sem pontuar) |
| Visor apagado | acorda; o gesto é consumido | — | — | — | recuperação continua valendo |
| Primeiro uso | próxima página | — | usar offline | usar offline | idem |

Recuperação: ao soltar após 8 s aparece "Abrir configuração? toque: confirmar" por 6 s. Um toque abre a rede própria (mesmo sem roteador, com senha antiga ou nunca cadastrado). Sem toque, desiste. Se o sensor estiver com defeito: comando `config` pelo USB/serial.

Sensor preso (≥20 s contínuos): aviso "Sensor preso? Solte", nenhum efeito disparado até soltar.

Menu: Rosto · Relógio · Pomodoro · Timers · Cronômetro · Jogos · Silencioso · Expressões · Configurar · Informações. Listas e menus voltam ao rosto após 30 s sem interação.

## 2. Abertura

1. Fundo preto, **barbatana branca** nadando com ondulação (~1,1 s);
2. a barbatana mergulha enquanto o texto **TubaBot** é revelado (~0,6 s);
3. dois olhinhos abrem piscando e assumem a expressão favorita (~0,7 s);
4. "Oi, <apelido>!" e entrada no rosto, primeiro uso ou retomada de sessão.

Completa ≈ 2,8 s; curta ≈ 0,9 s (opção no portal). Um toque pula a abertura. Não depende de rede. No completo, um padrão curto de bipes/vibração acompanha a abertura **depois** que as preferências salvas foram carregadas (respeita silencioso desde o primeiro bipe). No Lite, só animação.

## 3. Expressões e personalidade

Básicas (liberadas desde o início): Feliz, Neutro, Surpreso, Apaixonado (corações de verdade: dois lóbulos, entalhe e ponta), Triste, Sonolento, Bravo, Piscadinha esq./dir., Carinho, Olhos em X. Próprias de contexto: Foco, Descanso, Acerto, Quase!, Comemoração, Esperando.

Desbloqueáveis (tabela proposta, configurável em `core/app.cpp`):

| Expressão | Critério |
|---|---|
| Tímido | primeiro carinho concluído |
| Focado | primeiro Pomodoro (foco) completo |
| Vitorioso | 10 obstáculos em uma partida de Pula, Tuba! |
| Satisfeito | 5 peixes em uma partida de Hora do Bote |
| Espantado | 5 rodadas válidas (acumuladas) de Tuba Reflexo |
| Precisão | ≥ 98% em No Tempo Certo |

Carinhos contam com intervalo mínimo de 2 s (ruído/toques repetidos não multiplicam). Trocar personalidade, apelido ou Wi-Fi não apaga conquistas. O desbloqueio de teste do simulador usa um perfil separado, nunca gravado.

Personalidades **Tranquilo** e **Brincalhão** mudam frequência de piscadas, alcance do olhar, chance de piscada dupla e de microreações (piscadinha/surpresa) e o intervalo de troca espontânea de expressão — não mudam recursos. Humor, energia e vínculo são locais e limitados (−100..100, 0..100, 0..1000). Energia baixa ou madrugada (com hora válida) → sonolento; Pomodoro em foco → Foco (sem microreações); pausa → Descanso.

## 4. Os quatro jogos

### Pula, Tuba!
- Passo fixo de 4 ms, posições em subpixels inteiros: resultado idêntico a qualquer taxa de quadros (testado em 16 ms × 50 ms).
- Velocidade 80 px/s → teto 104 px/s (+2 px/s por obstáculo).
- **Ajuste em relação ao briefing:** impulso 140 px/s e gravidade 400 px/s² (voo ≈ 0,70 s, altura ≈ 24 px), em vez de 120/280 (voo ≈ 0,86 s). Mesma altura de salto, salto mais curto e "ágil". Os valores do briefing continuam testados (`jump_always_possible_many_seeds` roda as duas configurações) e são trocáveis em `JumpParams`.
- Primeiro obstáculo só depois de 1,5 s; início fácil: os 3 primeiros são 4×6 px.
- Espaçamento mínimo = velocidade × (voo + 300 ms de reação) + largura da hitbox, calculado com a maior velocidade possível até o obstáculo chegar; nunca há sequência impossível (40 sementes × 40 obstáculos, bot sem colisão).
- Hitbox honesta, menor que o sprite (exclui cauda e topo da barbatana).
- Buffer de 120 ms: toque pouco antes de aterrissar pula ao tocar o chão, uma única vez.
- 1 ponto quando o obstáculo passa inteiro pela hitbox, uma única vez.

### Tuba Reflexo
- Toque prepara; "ESPERE..." por 1,5–4 s aleatórios; "AGORA!" em tela invertida.
- Mede a partir do momento em que o quadro "AGORA!" foi **apresentado** no visor (o firmware avisa após o envio ao OLED; o simulador, após desenhar).
- Toque antes do sinal (ou antes de o sinal aparecer de fato) = "Cedo demais", rodada inválida. O toque que inicia não serve de resposta.
- Sem resposta em 3 s = rodada inválida. Mostra tempo, melhor marca e reação do Tuba.
- Precisão real inclui latência do TTP223 e do visor: é um jogo, não um instrumento calibrado.

### Hora do Bote
- Quadro externo 32×32 em (48,16); interior útil x = 49…78. Peixe 13×7 (bounding box cobre todos os pixels, cauda incluída).
- Captura só se **100%** do peixe estiver no interior útil no instante (timestamp) do toque: borda esquerda do sprite entre 49 e 66. Um pixel fora = erro (testado nas duas bordas).
- Cada captura = 1 ponto e acelera o próximo peixe: v ← v + 12% × (115 − v), mínimo +1 px/s (38 px/s inicial, janela inicial ≈ 470 ms).
- Novo peixe nasce fora da área; segurar o sensor não captura (só a borda do toque conta).
- Toque fora da janela encerra; deixar passar não é erro (o peixe volta a cruzar).
- Durante a animação "NHAC!" (350 ms) toques são ignorados sem penalidade.

### No Tempo Certo
- Alvo inteiro em centésimos, 300…800 (3,00 a 8,00 s), sorteio uniforme sem viés; mostrado antes de começar.
- Toque → 3, 2, 1 → VAI! (cronômetro começa em zero no VAI). Toques durante a contagem são ignorados e não ficam pendurados.
- Próximo toque para. `erro_ms = |obtido − alvo|`; `proximidade = clamp(100 × (1 − erro/alvo), 0, 100)` calculada em inteiros e arredondada ao centésimo, nunca a partir do texto da tela. 7,43/7,43 → 100,00%; 7,43/7,50 → 99,06%.
- Limite de 20 s: encerra como rodada inválida.
- Recorde: maior proximidade; desempate pelo menor erro absoluto.

Todos: instrução, início, resultado, recorde local, "Toque: jogar de novo" (ignora toques nos primeiros 500 ms do resultado), segurar 1,5 s sai. Alerta durante o jogo: Pula/Bote pausam de forma justa (o tempo do jogo congela) e retomam com um toque; Reflexo/Tempo Certo invalidam a rodada (sem recorde).

## 5. Tempo

- **Pomodoro:** 25/5/15, longa a cada 4 focos, início manual por padrão (automático opcional). Iniciar, pausar, retomar, pular etapa (não conta conquista), cancelar com confirmação. Continua fora da tela.
- **Timers:** 3 simultâneos (capacidade real). Atalhos 1, 5, 10, 15, 30 min + personalizado (1 s a 23h59min59s, validado no portal e no núcleo). Iniciar, pausar, retomar, repetir, cancelar; o aviso diz qual terminou.
- **Cronômetro:** conta para cima; zerar pede confirmação; não toca nada ao "terminar". Limite 99:59:59,99 — para ali e mostra "Limite atingido".
- **Lembretes:** 3 por intervalo (5–240 min), rótulo de até 16 caracteres, adiar 10 min. Durante um foco ficam pendentes até o fim do foco. Período de descanso opcional (só com hora válida).
- Tudo em tempo monotônico de 64 bits; mudar hora/fuso não altera durações; cada conclusão gera um único evento mesmo com quadro atrasado.
- **Perda de energia:** ao religar, "Sessão interrompida" oferece retomar (pausado, com o tempo do último checkpoint) ou descartar. O tempo desligado não é contado nem adivinhado.

## 6. Matriz de alertas (política central `core/outputs.h`)

| Perfil | Som | Vibração | Visual |
|---|---|---|---|
| Lite (qualquer configuração) | nunca | nunca | sempre |
| TubaBot, som ligado, silencioso desligado | sim | se habilitada | sempre |
| TubaBot em silencioso | nunca | se habilitada | sempre |
| TubaBot silencioso + vibração desligada | nunca | nunca | sempre |

Vale para boot, toque, carinho, amor, menu, pontos, fim de jogo, recorde, desbloqueio, timers, Pomodoro, lembretes, prévias e diagnóstico. A política é verificada ao iniciar **e** a cada atualização: desligar som/vibração corta a saída em andamento. Padrões são liga/desliga (buzzer ATIVO), repetem no máximo 3 vezes e têm teto absoluto de 6 s; a indicação visual fica até ser reconhecida. No Lite os pinos de buzzer/motor nem são configurados como saída.

Fila de avisos: até 8, os mais antigos são descartados se lotar (o atual permanece na tela).

## 7. Portal local

Rede `TubaBot-XXXX` com senha própria por aparelho (gerada na primeira inicialização e mostrada no visor), `http://192.168.4.1`, DNS cativo quando o sistema tenta detectar portal, mDNS `tubabot-xxxx.local`. Página única (~40 KB, 13 KB gzip) dentro da flash, sem CDN/fontes externas. Seções: Meu Tuba, Aparência, Som e vibração, Foco e tempo, Jogos, Coleção, Hora e data, Conectividade, Sistema e Diagnóstico (separado). Assistente de primeiro uso com "Usar offline". Rede de casa opcional; a senha nunca volta pela API.

Apelido: até 12 caracteres (acentos pt-BR desenhados pela própria fonte do Tuba), validado no núcleo (UTF-8, largura no visor).

## 8. Não implementado (de propósito ou pendente)

- **Clima/previsão:** não implementado. A preferência existe desligada; exigiria rede de casa, cliente HTTPS e verificação dos termos de uso do provedor para um produto comercial.
- **Coração/mensagem entre TubaBots pela internet:** descartado pelo briefing.
- **Buzzer passivo/melodias:** não aplicável ao buzzer comprado.
- **Rollback automático de OTA:** o firmware chama `esp_ota_mark_app_valid_cancel_rollback()`, mas o bootloader padrão do Arduino-ESP32 pode não ter rollback habilitado — pendente de verificação no aparelho.
