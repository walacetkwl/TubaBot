# TubaBot v3.1 — funcionalidades, gestos, regras e políticas

Tudo abaixo está implementado em `core/` e roda igual no firmware (ES3C28P / ESP32-S3), no simulador (WebAssembly) e nos testes nativos. O que é novo na v3 está marcado com **(v3)**.

## 1. Tela: 2,8" IPS em paisagem, 320×240 **(v3)**

- A placa ES3C28P tem um painel 240×320; o firmware gira o controlador 90° e **toda a interface é desenhada em 320×240 (paisagem)**: rosto, menus, jogos, Pomodoro, configurações e simulador.
- Motor gráfico próprio com **antialiasing** em tudo (formas por campo de distância), fontes Nunito em 6 tamanhos (14 a 92 px) com acentos pt-BR, ícones vetoriais e QR Code.
- Só a área que mudou é enviada ao display (SPI a 40 MHz); o rosto anima a ~30 quadros/s.
- "Girar 180°" troca a rotação do controlador (o toque gira junto) — para montar o corpo com o USB do outro lado.

## 2. Rosto em tela cheia **(v3)**

- No estado normal **100% da tela é do rosto**: sem barra, sem ícones, sem informações.
- Os **controles aparecem quando você toca** no rosto — uma coluna discreta à direita com Pomodoro, **Menu** e Jogos — e **somem sozinhos** após ~4 s. Logo depois de ligar eles aparecem uma vez, para mostrar que existem.
- **Informações opcionais sobre o rosto** (Aparência → Informações na tela), cada uma com seu liga/desliga e **todas desligadas de fábrica**: hora (centro do topo), clima (canto esquerdo), bateria (canto direito, **só no Pro** — no Lite a opção nem aparece). São pequenas e em cor suave para não estragar o rosto.
- Enquanto um Pomodoro/timer corre, uma linha fina de progresso aparece na borda inferior.
- Sem interação, **qualquer tela volta ao rosto**: menus em 20 s, apps em 45 s (timers e Pomodoro continuam). Exceções: jogo em andamento, respiração em andamento, cronômetro rodando, tela de configuração por QR (3 min).

## 3. Tema claro e escuro **(v3)**

- **Escuro** (fundo preto) é o padrão de fábrica; **Claro** (fundo branco). Fica salvo.
- Não é inversão de cores: cada tema tem sua própria paleta de interface (superfícies, textos, linhas) e as **expressões são adaptadas** — no claro o halo luminoso dos olhos é desligado (no branco vira mancha) e os brilhos/estrelinhas usam um tom da cor do olho em vez de branco.
- Troca em Aparência → Tema, no painel rápido (deslizar para baixo no rosto) ou no portal.

## 4. Cor das expressões **(v3)**

- Independente do tema. As 4 primeiras acompanham as **cores físicas** do produto; cada cor tem uma versão para o tema escuro e outra para o claro:

| Cor | No tema escuro | No tema claro |
|---|---|---|
| Vermelho | vermelho vivo | vermelho mais fechado |
| Azul | azul claro | azul royal |
| **Branco** | branco frio | cinza-azulado (branco no fundo branco sumiria) |
| **Grafite** | prata (grafite no preto sumiria) | grafite escuro |
| Ciano, Verde, Lilás, Rosa, Amarelo, Laranja | versão luminosa | versão mais escura e legível |

- Tela **Cor das expressões** com prévia ao vivo do rosto; a cor também vira a cor de destaque da interface (botões, alternadores).
- O padrão de cada unidade vem do **perfil de fábrica** (ex.: TubaBot vermelho → expressões vermelhas).

## 5. Lite e Pro — propriedade de fábrica **(v3)**

- **O firmware é um só.** O modelo é definido no perfil de fábrica gravado na bancada — o usuário **não** escolhe.
- **Lite:** sem bateria. Leitura de bateria, aviso de bateria fraca, expressão "Cansado" por bateria, ícone e a opção "Mostrar bateria" ficam **ocultos/desativados** (também na API e no portal).
- **Pro:** bateria LiPo (carregador da própria placa). Nível em %/mV no "Hoje", no painel rápido, em Informações e (se ligado) sobre o rosto; aviso único ao chegar a 10%.

## 6. Configuração de fábrica × preferências do usuário **(v3)**

| Fábrica (por unidade, gravado por nós) | Usuário (pode mudar) |
|---|---|
| Modelo Lite/Pro | tema, cor das expressões |
| Cor física do corpo | informações na tela (hora, clima, bateria) |
| Cor padrão das expressões | volume, brilho, silencioso |
| Tema padrão | personalidade, animação, relógio, apelido... |
| Outros padrões (volume, brilho, personalidade, 24 h, fuso, informações na tela) e nº de série | Wi-Fi, alarmes, lembretes, timers |

- Perfil gravado numa **partição própria de 4 KB** (`fabrica`, endereço 0xFEF000) — não é apagado por OTA, nem por regravar o firmware completo, nem por "Restaurar padrão de fábrica". Detalhes e passo a passo: [`FABRICA.md`](FABRICA.md).
- **Restaurar padrão de fábrica** (Sistema, ou portal): apaga preferências, apelido, recordes, conquistas e Wi-Fi e volta ao primeiro uso **com os padrões daquela unidade** (um TubaBot vermelho volta vermelho).
- **Redefinir preferências** (portal): volta tema/cor/ajustes aos padrões da unidade, mantendo conquistas e recordes.
- Unidade sem perfil gravado: funciona como Lite genérico (grafite, escuro) e mostra o aviso em Informações → Fábrica.

## 7. Toque

Classificação **exclusiva** ao soltar (uma interação = no máximo um destes): toque, duplo toque, toque longo solto, deslize (4 direções), carinho, recuperação. Limiares ajustados para a tela de 2,8" (≈143 ppi).

| Onde | Toque | Duplo toque | Segurar parado | Acariciar (arrastar devagar) | Deslizar |
|---|---|---|---|---|---|
| Rosto | reage, olha para o dedo e **mostra os controles** | próxima expressão | 0,6–2 s: carinho · ≥ 2 s: amor | carinho + ronronar | ← → relógio / "Hoje" · ↑ menu · ↓ painel rápido |
| Relógio / Hoje | — | — | relógio: troca o mostrador | — | ← → páginas · ↑ menu · ↓ painel |
| Menus e apps | abre / aperta | — | — | arrastar rola a lista (com inércia) | → voltar |
| Jogos | ação imediata no instante real do toque | — | 1,5 s: sair | Mergulho: o dedo guia o Tuba | — |
| Qualquer lugar | — | — | ≥ 8 s e soltar: **abrir configuração** | — | — |

- Botão **BOOT**: curto no rosto = apaga/acende a tela; curto em outra tela = volta ao rosto; segurar 3 s = abre a configuração (recuperação mesmo com o toque com defeito).
- Toque contínuo por 20 s: aviso "Toque contínuo" e nada é disparado até soltar.

## 8. Menu **(v3)**

Menu principal em grade 3×3, feito para o dedo:

| | | |
|---|---|---|
| **Jogos** | **Pomodoro** | **Timer** |
| **Aparência** (tema, cor das expressões, informações na tela, brilho dos olhos, relógio) | **Volume** (com silencioso) | **Brilho** (com brilho reduzido à noite) |
| **Wi-Fi** (rede de casa, configurar pelo celular, rede do Tuba, esquecer) | **Sistema** (personalidade, animação, apagar a tela, 24 h, silencioso, reagir a sons, girar 180°, informações, restaurar padrão de fábrica) | **Mais apps** |

**Mais apps:** Cronômetro, Alarmes, Relógio, Clima, Respirar, Sorteio, Expressões, Meu Tuba, **Lendárias**. "Sobre" ficou em Sistema → Informações.
**Sistema** ganhou: **Idioma**, **Atualizações** e **Estatísticas anônimas**.
**Painel rápido** (deslizar para baixo no rosto): silencioso, tema claro/escuro, reagir a sons, lanterna, Wi-Fi + brilho e volume deslizantes.

## 9. Expressões e personalidade

- 34 expressões paramétricas com transições suaves, piscar simples/duplo, olhar para os lados, respiração, olhar que segue o dedo, dança no ritmo da música, "boop" (o rosto achata e volta). Básicas: Feliz, Neutro, Surpreso, Apaixonado, Triste, Sonolento (dormir/acordar), Bravo (irritação), Piscadinhas, Carinho, Olhos em X; de contexto: Foco (concentração), Descanso, Acerto, Quase! (derrota por pouco), Comemoração (vitória), Esperando; reações: Tonto, Assustado, Apertado, Carregando, Cansado, Curioso.
- **Coleção** de 10 desbloqueáveis (Tímido, Focado, Vitorioso, Satisfeito, Espantado, Precisão, Dançante, Zen, Mergulhador, Radiante).
- **Rotina do dia (v3.1):** a tela **nunca apaga** por padrão. Cada dia o Tuba sorteia quando fica com sono — entre **20:00 e 22:30** — e quando acorda (6:00–7:00). Até 45 min antes ele às vezes fica "cansadinho"; depois, sonolento com "zzz" (precisa da hora ajustada).
- **Abertura (v3.1):** barbatana curva em duas tonalidades surge do mar, desliza com inclinação proporcional à velocidade e rastro de espuma, mergulha com respingos; aparece o nome e o rosto sobe da água abrindo os olhos.
- **Relógio "Tuba" (v3.1):** o rostinho ao lado da hora é o rosto vivo — pisca, olha em volta, de vez em quando "confere" os números, pisca o olho, fica curioso; à noite, sonolento.
- Personalidades Tranquilo/Brincalhão mudam ritmo de piscadas, alcance do olhar, microreações e a resposta ao toque. Humor, energia e vínculo em **Meu Tuba**.

## 9b. Reações lendárias **(v3.1)**

Cenas raras em que o Tuba usa "mãos" (luvinhas na cor das expressões) e objetos:

| Cena | Como aparece |
|---|---|
| Olhar o relógio | um relógio de pulso sobe; ele confere a hora, se assusta e ri |
| Joinha | um polegar para cima aprova o seu dia |
| Peixinho passando | um peixe atravessa a tela; ele acompanha com os olhos e lambe os beiços |
| Pegou o peixe! | o peixe passa na altura da boca e ele dá o bote, mastiga, sobra a espinha (só depois de já ter visto o "peixinho passando") |
| Cafezinho | só de manhã (6h–11h): xícara com fumacinha, bebe de olhos fechados, "ahh..." |
| Meditação | mãos em mudra, olhos fechados, flutua devagar, "ommm" |
| Escada rolante | finge que foi embora descendo na diagonal, espia do canto, dá tchau e volta rindo |
| Que barulho! | com barulho alto: tampa os ouvidos com as mãos, olhos ><, treme e depois "ufa!" |
| Bocejo | ao anoitecer: boceja cobrindo a boca com a mão |

Regras: **não podem ser pedidas** por toques; vêm com a **afinidade** (vínculo: carinhos, Pomodoros, respirações e jogos). No máximo **uma por dia**, e nem todo dia (chance diária de 18% a 55% conforme a afinidade), só quando há alguém por perto (interação nos últimos 30 min, mas nunca logo após um toque) e nunca durante o foco. Prefere as ainda não vistas. Na primeira vez aparece a medalha **"Reação lendária desbloqueada!"** (tocar nela abre a coleção). Em **Mais apps → Lendárias** ficam as descobertas (tocar repete) e quantas secretas faltam. Tocar no rosto durante uma cena a encerra.

## 9c. Idiomas **(v3.1)**

Português (Brasil), inglês e espanhol (América Latina) em **Sistema → Idioma**; o padrão vem do perfil de fábrica (`idioma=`). Todas as telas passam por uma tabela de tradução (`core/i18n_table.inc`); `make i18n-audit` lista textos sem tradução.

## 9d. Atualizações e telemetria **(v3.1)**

- Com Wi-Fi de casa e o servidor configurado (`TB_UPDATE_URL`), o Tuba procura versões ao conectar, a cada 6 h e em **Buscar atualizações** (Sistema → Atualizações ou Informações → Sobre). Havendo versão nova, avisa na tela: **Agora**, **Em 1 hora** (até 3 vezes) ou **Esta noite** (instala sozinho às 3h). Não existe "ignorar": sem resposta em 60 s o aviso é adiado. O download confere SHA-256; falhou, tenta de novo em 1 h.
- Telemetria anônima a cada 10 min (série, modelo, versão, ativado, interação hoje/7 dias), desligável em Sistema → Estatísticas anônimas. Detalhes e o painel planejado em [`futuras-atualizacoes.html`](futuras-atualizacoes.html).

## 10. Som e microfone

- Sintetizador no núcleo (16 kHz, 2 vozes): melodias de abertura, alarme, timer, Pomodoro, lembrete, recorde, conquista; reações e "falinha" de robô. Volume 0–100%.
- Microfone da placa (via ES8311): **só o nível** chega ao núcleo — palma assusta, barulho alto incomoda, música faz dançar. Nada é gravado. Pode ser desligado (Sistema ou painel).
- Política central: sem alto-falante ou em silencioso → só visual.

## 11. Jogos

A lógica dos jogos é a mesma (mesmos testes de justiça, física e pontuação); o "mundo" 128×64 é ampliado 2,4× ocupando a largura da tela em paisagem.

- **Pula, Tuba!**, **Tuba Reflexo** (mede a partir da apresentação do "AGORA!"), **Hora do Bote** (peixe 100% dentro do quadro), **No Tempo Certo**, **Mergulho do Tuba** (o dedo guia o Tuba; palco 210×210 com placar e vidas nas laterais).

## 12. Tempo

- **Pomodoro** 25/5/15 (anel + botões ao lado), **3 timers**, **cronômetro** com voltas, **3 alarmes** com dias da semana, **lembretes** por intervalo, retomada após falta de energia (pausado).
- **Hora:** a ES3C28P não tem relógio com bateria. A hora vem da **rede de casa (NTP, ligado por padrão)** ou do portal; sem rede, aparece "Hora não ajustada" até ser ajustada (timers e Pomodoro não dependem dela).

## 13. Apps

Hoje, Relógio (digital, analógico e "Tuba"), Clima (Open-Meteo, 27 capitais), Respiração, Sorteio, Expressões, Meu Tuba, Lanterna, Informações (Sobre, Fábrica, Rede, Hora, Memória, Sensores, Recordes).

## 14. Portal local

Rede `TubaBot-XXXX` com senha própria, `http://192.168.4.1`, QR Codes no primeiro uso. Abas: Meu Tuba, Aparência (**tema**, **cor das expressões** com amostras no tema escolhido, **informações sobre o rosto**), Som, Foco e tempo, Jogos, Coleção, Hora e data, Sensores e clima, Conectividade, Sistema (dados da unidade, OTA, **Restaurar padrão de fábrica**), Diagnóstico. Espelho da tela 160×120 no topo.

## 15. Fora desta versão

- Sensor de movimento: a ES3C28P não tem. A lógica (sacudir, inclinar, Nível) continua no núcleo e testada, mas fica oculta (`caps.imu = false`).
- LED RGB da placa (mantido apagado), cartão microSD, Bluetooth, voz: ideias em [`IDEIAS.md`](IDEIAS.md).
