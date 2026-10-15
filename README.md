# TubaBot v3.1 — software (firmware, simulador e portal)

Companheiro de mesa da **TubaLabs** na placa **ES3C28P** (ESP32-S3 + tela IPS 2,8" com toque capacitivo, alto-falante, microfone, Wi-Fi e bateria opcional). A tela é usada **em paisagem, 320×240**, e no estado normal **é toda do rosto**.

Um único núcleo C++ (`core/`) contém toda a lógica — gestos, rosto com 34 expressões, **reações lendárias**, personalidade, idiomas (pt-BR/en/es), tema claro/escuro, cor das expressões, perfil de fábrica (Lite/Pro), cinco jogos, Pomodoro, timers, cronômetro, alarmes, lembretes, clima, alertas, persistência, desenho antisserrilhado e a API local. Esse mesmo núcleo é compilado:

| Destino | Como | Para quê |
|---|---|---|
| ES3C28P (ESP32-S3) | `firmware/` + PlatformIO (pioarduino) | o aparelho real |
| WebAssembly | `simulator/` (navegador) | testar tudo — temas, cores, Lite/Pro — antes das peças |
| Nativo (Mac/Linux) | `tests/` | testes automatizados |

```
core/             lógica compartilhada (C++17 sem STL, sem heap, sem exceções)
firmware/         adaptadores ES3C28P: ILI9341V, FT6336G, ES8311, bateria, NVS, Wi-Fi, HTTP, OTA, perfil de fábrica
firmware/dist     imagens prontas para gravar (inclusive pelo navegador)
firmware/fabrica  perfis de fábrica prontos (Lite/Pro × vermelho, azul, branco, grafite)
simulator/        simulador no navegador (abre com duplo clique em index.html)
web-ui/           portal de configuração (o mesmo arquivo no ESP32 e no simulador)
tests/            testes nativos, equivalência nativo×WASM e ponta a ponta
tools/            fabrica.py, build, fontes, galeria de telas, servidor do portal
docs/             hardware, bancada, fábrica, funcionalidades, API, validação, ideias, lista de materiais
```

**Comece por:**

- [`docs/lista_de_materiais.xlsx`](docs/lista_de_materiais.xlsx) — o que comprar (Mercado Livre / Shopee)
- [`README_MAC.md`](README_MAC.md) — simulador, testes e gravação
- [`docs/FABRICA.md`](docs/FABRICA.md) — como preparar cada unidade (Lite/Pro, cor, tema) sem mexer no código
- [`docs/HARDWARE.md`](docs/HARDWARE.md) e [`docs/BANCADA.md`](docs/BANCADA.md) — placa, montagem e conferência
- [`docs/FUNCIONALIDADES.md`](docs/FUNCIONALIDADES.md) — tudo o que o TubaBot faz
- [`docs/VALIDACAO.md`](docs/VALIDACAO.md) — o que foi testado e o que depende da placa
- [`docs/futuras-atualizacoes.html`](docs/futuras-atualizacoes.html) — atualizações pela internet, painel admin, telemetria e próximas ideias
- [`docs/PROTECAO.md`](docs/PROTECAO.md) — proteção do código e dos aparelhos (licença proprietária em `LICENSE`)

Galeria com 61 telas reais geradas pelo próprio núcleo: [`docs/telas.png`](docs/telas.png).

Versões anteriores ficam no histórico do Git: v1 (ESP32-C3 + OLED, commit `58f7b35`) e v2 (Waveshare AMOLED redonda, commit `47bd3e7`).
