# TubaBot v2 — software (firmware, simulador e portal)

Companheiro de mesa da **TubaLabs**, agora com **tela AMOLED redonda colorida de 1,75" (466×466) e toque na tela**, no corpo do Neo Deskimon. Placa: **Waveshare ESP32-S3-Touch-AMOLED-1.75** (ESP32-S3, 8 MB PSRAM, IMU, microfones, alto-falante, bateria, RTC).

Um único núcleo C++ (`core/`) contém toda a lógica — gestos na tela, rosto com 34 expressões, personalidade, reações a movimento e som, cinco jogos, Pomodoro, timers, cronômetro, alarmes, lembretes, clima, alertas, persistência, desenho antisserrilhado e a API local. Esse mesmo núcleo é compilado:

| Destino | Como | Para quê |
|---|---|---|
| ESP32-S3 (Waveshare AMOLED 1.75) | `firmware/` + PlatformIO (pioarduino) | o aparelho real |
| WebAssembly | `simulator/` (navegador) | testar tudo antes das peças |
| Nativo (Mac/Linux) | `tests/` | testes automatizados |

`make crosscheck` roda a mesma sequência no núcleo nativo e no WebAssembly e confere que as telas são idênticas bit a bit.

```
core/        lógica compartilhada (C++17 sem STL, sem heap, sem exceções)
firmware/    adaptadores ESP32-S3: tela CO5300, toque CST9217, IMU, PMU, RTC, áudio, NVS, Wi-Fi, HTTP, OTA
firmware/dist imagens prontas para gravar (inclusive pelo navegador)
simulator/   simulador no navegador (abre com duplo clique em index.html)
web-ui/      portal de configuração (o mesmo arquivo no ESP32 e no simulador)
tests/       testes nativos, equivalência nativo×WASM e ponta a ponta
tools/       build, fontes, galeria de telas, servidor do portal
docs/        hardware, bancada, funcionalidades, API, validação, ideias, lista de materiais
```

**Comece por:**

- [`docs/lista_de_materiais.xlsx`](docs/lista_de_materiais.xlsx) — o que comprar (Mercado Livre / Shopee)
- [`README_MAC.md`](README_MAC.md) — simulador, testes e gravação
- [`docs/HARDWARE.md`](docs/HARDWARE.md) e [`docs/BANCADA.md`](docs/BANCADA.md) — montagem e conferência
- [`docs/FUNCIONALIDADES.md`](docs/FUNCIONALIDADES.md) — tudo o que o TubaBot faz
- [`docs/VALIDACAO.md`](docs/VALIDACAO.md) — o que foi testado e o que depende das peças

Galeria de telas geradas pelo próprio núcleo: [`docs/telas.png`](docs/telas.png).

A v1 (ESP32-C3 + OLED) continua no histórico do Git (commit `58f7b35`).
