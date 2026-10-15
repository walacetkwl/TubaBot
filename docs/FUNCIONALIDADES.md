# TubaBot v2 — funcionalidades, gestos, regras e políticas

Tudo abaixo está implementado em `core/` e roda igual no firmware (ESP32-S3), no simulador (WebAssembly) e nos testes nativos. O que mudou em relação à v1 está marcado com **(novo)**.

## 1. A nova "cara": tela redonda AMOLED 466×466

- Fundo preto de verdade (pixel do AMOLED apagado): contraste máximo e economia de energia.
- Motor gráfico próprio com **antialiasing** em tudo (formas por campo de distância), fontes Nunito em 6 tamanhos com acentos pt-BR, 45 ícones vetoriais e QR Code.
- Só a área que mudou é enviada ao painel (retângulo com coordenadas pares, exigência do CO5300); telas paradas não gastam barramento.
- **Anti-marcação (burn-in):** o rosto e os mostradores do relógio "passeiam" alguns pixels a cada poucos minutos; a tela apaga no descanso; brilho reduzido à noite.
- Tema: cada app tem sua cor; a cor dos olhos é escolhida pelo usuário (8 opções: ciano, menta, lilás, rosa, âmbar, azul, coral, branco).

## 2. Toque na tela (substitui o TTP223 da cabeça)

Classificação **exclusiva** ao soltar (uma interação = no máximo um destes): toque, duplo toque, toque longo solto, deslize (4 direções), carinho, recuperação. Durante a pressão há retorno imediato: `Down`/`Move` (arrastar listas, controles deslizantes, olhos seguindo o dedo), `LongPress` (600 ms) e `PetStroke` (a cada 140 px de trajeto lento). Falhas curtas do controlador (< 40 ms) não dividem um toque.

| Onde | Toque | Duplo toque | Segurar parado | Acariciar (arrastar devagar) | Deslizar |
|---|---|---|---|---|---|
| Rosto | "boop": reage e olha para onde tocou | próxima expressão | 0,6–2 s: carinho · ≥ 2 s: amor (corações) | carinho + ronronar | ← → relógio / "Hoje" · ↑ apps · ↓ ajustes rápidos |
| Relógio / Hoje | — | — | relógio: troca o mostrador | — | ← → páginas · ↑ apps · ↓ ajustes |
| Apps e listas | abre o item / aperta o botão | — | — | arrastar rola a lista (com inércia) | → voltar |
| Jogos | ação imediata no instante real do toque | — | 1,5 s: sair | Mergulho: o dedo guia o Tuba | — |
| Qualquer lugar | — | — | ≥ 8 s e soltar: **abrir configuração** | — | — |

- Botão **BOOT** da placa: toque curto = voltar ao rosto/acordar; segurar 3 s = abrir configuração (recuperação mesmo com a tela de toque com defeito).
- Botão **PWR**: toque curto apaga/acende a tela.
- Toque contínuo por 20 s (sujeira/defeito): aviso "Toque contínuo" e nada é disparado até soltar.
- Tela girada 180° (preferência): o toque gira junto.
- Logo após acordar a tela, toques rápidos (ex.: duplo toque para acordar) não viram ação.

## 3. Navegação

- **Página inicial (carrossel):** Rosto ↔ Relógio ↔ Hoje.
- **Apps** (deslize para cima): Relógio, Pomodoro, Timers, Cronômetro, Alarmes **(novo)**, Jogos, Expressões, Respiração **(novo)**, Nível **(novo)**, Sorteio **(novo)**, Clima **(novo)**, Meu Tuba **(novo)**, Ajustes **(novo, no próprio aparelho)**, Informações. Lista com efeito "lupa" (o item central é o maior).
- **Ajustes rápidos** (deslize para baixo): silencioso, reagir a sons (liga/desliga o uso do microfone), lanterna **(novo)**, rede; brilho e volume por controle deslizante.
- Voltar: deslizar para a direita ou o "‹" na borda esquerda. Listas e apps passivos voltam ao rosto após 30 s sem toque.

## 4. Abertura

1. Mar animado com duas ondas; a **barbatana** atravessa a tela;
2. a barbatana mergulha (bolhas) enquanto **TubaBot** aparece;
3. os olhos abrem piscando na expressão favorita;
4. "Oi, <apelido>!" na cor dos olhos e uma "falinha" de robô.

Completa ≈ 2,8 s; curta ≈ 0,9 s. Um toque pula. Não depende de rede. O som de abertura respeita silencioso/volume salvos desde a primeira amostra.

## 5. Rosto, expressões e personalidade

- 34 expressões paramétricas: olhos com pálpebras inclinadas, bochechas, reflexo de luz, halo luminoso; transições suaves (interpolação de formato; troca de tipo passa por uma piscada), piscar simples/duplo, olhar ocioso, **respiração** (os olhos "respiram"), **olhar dirigido** (segue o dedo e a inclinação **(novo)**), **dança no ritmo** **(novo)**.
- Básicas: Feliz, Neutro, Surpreso, Apaixonado, Triste, Sonolento, Bravo, Piscadinhas, Carinho, Olhos em X. De contexto: Foco, Descanso, Acerto, Quase!, Comemoração, Esperando. Reações **(novo)**: Tonto, Assustado, Apertado (barulho), Carregando, Cansado, Curioso.
- **Coleção (10 desbloqueáveis):**

| Expressão | Critério |
|---|---|
| Tímido | primeiro carinho |
| Focado | primeiro Pomodoro completo |
| Vitorioso | 10 corais em uma partida de Pula, Tuba! |
| Satisfeito | 5 peixes na Hora do Bote |
| Espantado | 5 rodadas válidas de Reflexo (acumulado) |
| Precisão | ≥ 98% no Tempo Certo |
| Dançante **(novo)** | 30 s dançando com música (acumulado) |
| Zen **(novo)** | 3 sessões de respiração |
| Mergulhador **(novo)** | 20 peixes no Mergulho |
| Radiante **(novo)** | conquistar todas as outras (olhos arco-íris) |

- Personalidades Tranquilo/Brincalhão mudam ritmo de piscadas, alcance do olhar, chance de microreações e a "resposta" ao boop (risadinha, piscadinha, surpresa, curiosidade). Humor, energia e vínculo (−100..100, 0..100, 0..1000) aparecem em **Meu Tuba**.
- Energia baixa, madrugada (hora válida) ou bateria ≤ 10% → sonolento/cansado; Pomodoro em foco → Foco (sem microreações).

## 6. Sensores (novo)

### Movimento (QMI8658)
| Evento | Reação |
|---|---|
| Sacudir (3 picos fortes em 1 s) | olhos em espiral (Tonto) + som de tontura; no Sorteio, sorteia |
| Pegar no colo (após ≥ 2 s parado) | susto rápido + "falinha"; acende a tela (opcional) |
| Virar de cara para baixo | apaga a tela; se houver alarme/lembrete tocando, **adia** (opção) |
| Desvirar | acende |
| Deitar (tela para cima) | descansa e olha para cima |
| De ponta-cabeça | fica tonto |
| Inclinar | os olhos acompanham a gravidade |

### Som ambiente (2 microfones, ES7210)
Só o **nível** (≈ dBFS + 100, a cada 40 ms) chega ao núcleo — nenhum áudio é gravado, guardado ou transmitido. Pode ser desligado nos ajustes rápidos.
- Palma (salto ≥ 24 acima da base, curto) → susto; barulho alto contínuo (≥ 1,5 s) → olhos apertados.
- **Música** (ataques regulares de 250–1200 ms) → o rosto **dança na batida** estimada; conta para "Dançante".

### Energia (AXP2101) e relógio (PCF85063)
- Bateria (%), mV, carregando e USB na barra de status, no "Hoje" e no portal. Ao começar a carregar: expressão Carregando + som. Bateria ≤ 10%: aviso único + Cansado.
- **RTC com bateria (novo):** a hora sobrevive a desligar. Ajustar a hora (portal/NTP) grava no RTC; no boot o firmware lê o RTC. Um RTC zerado é ignorado (não inventa data).

## 7. Som (novo: alto-falante de verdade)

Sintetizador no núcleo (16 kHz, 2 vozes, ondas seno/triângulo/quadrada/ruído, envelope sem estalos): melodias de abertura, alarme, timer, Pomodoro, lembrete, recorde, conquista; reações (boop, risadinha, tontura, susto, bocejo, ronronar); **"falinha" de robô** gerada por semente. Volume 0–100% (curva perceptiva). Comunicação com a tarefa de áudio por fila sem travas.

### Política central (todas as saídas)
| Situação | Som | Vibração (motor opcional) | Visual |
|---|---|---|---|
| Sem alto-falante | nunca | se houver motor e estiver ligada | sempre |
| Som ligado, sem silencioso, volume > 0 | sim | se habilitada | sempre |
| Silencioso | nunca | se habilitada | sempre |
| Silencioso + vibração desligada | nunca | nunca | sempre |

Desligar som/vibração corta o que está tocando. Padrões comuns têm teto de 6 s; o **alarme** toca até 2 min e depois fica só visual. Sons de interface usam outro canal e não interrompem um alerta.

## 8. Os cinco jogos

A lógica dos quatro jogos da v1 é **a mesma** (mesmos testes de justiça, física e pontuação); o "mundo" 128×64 é ampliado 3× com gráficos coloridos (mar, areia, corais, tubarão e peixe vetoriais, bolhas, raios de luz).

- **Pula, Tuba!** — passo fixo de 4 ms; espaçamento sempre possível; buffer de 120 ms; hitbox honesta.
- **Tuba Reflexo** — mede a partir da **apresentação** do "AGORA!" no painel; antes do sinal = inválida.
- **Hora do Bote** — captura só com o peixe 100% dentro do quadro (1 px fora = erro); acelera por captura.
- **No Tempo Certo** — alvo 3,00–8,00 s; 3-2-1-VAI; proximidade em inteiros (7,43/7,50 → 99,06%).
- **Mergulho do Tuba (novo)** — peixes e águas-vivas descem; o Tuba é guiado pela **inclinação** (IMU) ou pelo dedo; 3 vidas, 1 s de invulnerabilidade após ser queimado; dificuldade cresce com a pontuação; regra de justiça: duas águas-vivas seguidas nunca fecham a passagem. Passo fixo de 10 ms (mesmo resultado a 16 ou 50 ms por quadro).

Alerta durante o jogo: Pula/Bote/Mergulho pausam (o tempo congela); Reflexo/Tempo Certo invalidam a rodada.

## 9. Tempo

- **Pomodoro** 25/5/15 (anel de progresso; botões cancelar · iniciar/pausar · pular). Continua fora da tela e aparece como anel fino em volta do rosto.
- **3 timers** simultâneos com atalhos e personalizado; anel âmbar em volta do rosto enquanto contam.
- **Cronômetro** com centésimos e **voltas** **(novo)**; zerar pede confirmação.
- **Alarmes (novo):** 3 alarmes com nome, hora, dias da semana (ou "uma vez"), tela de edição no aparelho e no portal; adiar 5 min (botão ou virar para baixo); alarme único se desliga ao confirmar; nunca dispara retroativamente quando a hora é ajustada; cada minuto dispara no máximo uma vez.
- **Lembretes** por intervalo (iguais à v1), esperando o fim do foco; período de descanso.
- **Retomada após falta de energia** (pausado, sem adivinhar o tempo desligado).

## 10. Apps novos

- **Hoje:** saudação pelo horário, data, clima, próximo alarme, Pomodoro/timers ativos, humor e bateria.
- **Respiração:** caixa 4-4-4-4, relaxar 4-7-8 e coerente 5-5; círculo que enche/esvazia, contagem e som suave a cada fase.
- **Nível (bolha):** em pé mostra o ângulo do horizonte; deitado vira nível de bolha 2D; fica verde abaixo de 1°.
- **Sorteio:** dado, cara ou coroa, sim ou não, número 1–100; toque ou **sacuda** para sortear.
- **Clima:** temperatura, condição, máx/mín, umidade e 2 dias seguintes; 27 capitais ou coordenadas próprias; atualiza a cada 30 min (5 min após falha, mantendo o último dado bom). Fonte: [Open-Meteo](https://open-meteo.com) (sem chave). O firmware faz o HTTPS numa tarefa separada (a tela nunca trava). O certificado não é fixado porque a CA do serviço pode mudar; a resposta passa pelo validador JSON do núcleo. Uso pessoal/não comercial conforme os termos do Open-Meteo.
- **Meu Tuba:** humor, energia, vínculo e estatísticas.
- **Lanterna:** tela branca no brilho máximo.
- **Ajustes no aparelho:** brilho, volume, cor e brilho dos olhos, mostrador, personalidade, animação, 24 h, apagar a tela, reações a movimento/sons, vibração, silencioso, girar 180°, rede e portal, informações.

## 11. Portal local e configuração

Rede `TubaBot-XXXX` com senha própria por aparelho, `http://192.168.4.1`, DNS cativo e mDNS (como na v1). **Novo:** o primeiro uso mostra **QR Codes** — um para entrar na rede do Tuba pela câmera do celular e outro para abrir o portal. Abas: Meu Tuba, Aparência (cor dos olhos, mostrador, brilho), Som e vibração (volume), Foco e tempo (Pomodoro, timers, **alarmes**, lembretes), Jogos, Coleção, Hora e data, **Sensores e clima**, Conectividade, Sistema, Diagnóstico. Espelho colorido da tela no topo. Backup/restauração (aceita backup da v1).

## 12. Não implementado (de propósito ou pendente)

- Reconhecimento de voz / "palavra de ativação": possível no ESP32-S3 (ESP-SR), mas exigiria gravar e processar áudio — fora da proposta de privacidade desta versão. Fica como ideia (ver `docs/IDEIAS.md`).
- Bluetooth (notificações do celular), cartão microSD e GPS (só na placa `-G`): não usados.
- Rollback automático de OTA depende do bootloader; a imagem é marcada como válida no boot.
