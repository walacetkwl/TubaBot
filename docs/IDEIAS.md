# Ideias para as próximas versões (para conversarmos)

Nada disto está implementado. Ordem aproximada de valor ÷ esforço.

| Ideia | O que precisa | Comentário |
|---|---|---|
| **Palavra de ativação** ("Ei, Tuba") | ESP-SR (WakeNet) do ESP-IDF; ~1 MB de flash e PSRAM — cabem | Reconhecimento local, sem nuvem. Exige migrar o áudio para o pipeline do ESP-SR. |
| **Comandos de voz simples** ("timer de 5 minutos", "que horas são") | ESP-SR MultiNet (inglês/chinês nativos; português limitado) | Avaliar qualidade em português antes de prometer. |
| **Notificações do celular por Bluetooth** | BLE (ANCS no iPhone; app ponte no Android) | O rosto reage a mensagens; o S3 tem BLE 5. |
| **Calendário do dia** | URL iCal no portal | Mostrar o próximo compromisso no cartão Hoje. |
| **Integração com Home Assistant** | MQTT ou a API local que já existe | Expressões por automação ("porta aberta", "lavadora pronta"). |
| **Rotina de pausas ativas** | só software | Lembrete para levantar/beber água ligado ao Pomodoro. |
| **Bichinho com fome/sono** | só software | Necessidades leves que o carinho e os jogos atendem. |
| **Temas de cor do sistema** | só software | Além da cor dos olhos, cores da interface. |
| **Mais mostradores** | só software | Relógio de palavras, fases da lua, contagem regressiva para uma data. |
| **Cartão microSD** | a placa tem slot TF (conferir pinos no esquema) | Sons gravados, fotos e mostradores personalizados. |
| **Multiplayer entre dois TubaBots** | ESP-NOW (sem roteador) | Mandar carinho/emoji de um para o outro na mesma casa. |
