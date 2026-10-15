# Proteção do código e dos aparelhos

Objetivo: dificultar cópias ("clones" do TubaBot com o nosso firmware) e garantir
que só firmware da TubaLabs rode nas unidades — sem atrapalhar o desenvolvimento.

Resumo honesto: **nenhuma proteção de software é absoluta**. O que realmente
protege um produto com ESP32-S3 são os recursos do próprio chip (Secure Boot +
criptografia da flash), um repositório privado e a parte jurídica (licença).
Ofuscar código não protege e atrapalha a manutenção — por isso não usamos.

## 1. Camadas

| Camada | O que impede | Estado |
|---|---|---|
| Repositório **privado** no GitHub, acesso só para a equipe | cópia do código-fonte | manter assim (não tornar público) |
| `LICENSE` proprietária + `NOTICE.md` | uso comercial do código / da marca; base para notificação e processo | **pronto** |
| **Flash Encryption** (ESP32-S3) | ler a flash de um TubaBot e copiar o firmware para outra placa | procedimento abaixo (produção) |
| **Secure Boot v2** (assinatura RSA-3072) | gravar firmware não assinado (pirata ou adulterado), inclusive por OTA | procedimento abaixo (produção) |
| JTAG e download serial restritos (eFuses) | depuração/extração pela USB | procedimento abaixo |
| Perfil de fábrica + **número de série** registrado (`tools/fabrica.py --serie auto`) | o servidor só entrega atualizações/serviços a séries conhecidas | pronto (registro); servidor: futuro |
| Atualização pela internet com SHA-256 + assinatura | instalar binário trocado no caminho | SHA-256 pronto; assinatura vem do Secure Boot |

## 2. Simulador e portal (cuidado)

O simulador (`simulator/`) é o **mesmo núcleo compilado em WebAssembly**. Quem
recebe o `index.html` + `tubabot.wasm` recebe, na prática, a lógica do aparelho
(dá para desmontar o WASM). Portanto:

- não publique o simulador em site aberto; compartilhe só por link privado (ex.: artefato privado do claude.ai) com quem precisa;
- para demonstrações públicas, use **vídeos/GIFs** gerados do simulador;
- o portal (`web-ui/portal.html`) vai dentro de cada aparelho — é esperado que seja visível; ele não contém regras de negócio, só chama a API.

## 3. Procedimento de produção (ESP32-S3)

> ⚠️ Os eFuses são **irreversíveis**. Faça primeiro em 2–3 placas de teste. Guarde
> as chaves num cofre (fora do Git!) com cópia de segurança: perder a chave de
> assinatura = nunca mais conseguir atualizar as unidades vendidas.

1. **Gerar a chave de assinatura** (uma vez, guardar com segurança):
   `espsecure.py generate_signing_key --version 2 --scheme rsa3072 tubalabs_secure_boot.pem`
2. **Habilitar no build de produção.** O Arduino-ESP32 vem com bootloader pré-compilado;
   para Secure Boot/Flash Encryption é preciso compilar o bootloader com o ESP-IDF
   (pioarduino suporta `framework = arduino, espidf` com `sdkconfig.defaults`):
   ```
   CONFIG_SECURE_BOOT=y
   CONFIG_SECURE_BOOT_V2_ENABLED=y
   CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME=y
   CONFIG_SECURE_BOOT_SIGNING_KEY="tubalabs_secure_boot.pem"
   CONFIG_SECURE_FLASH_ENC_ENABLED=y
   CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y
   CONFIG_SECURE_BOOT_ALLOW_JTAG=n
   CONFIG_SECURE_UART_ROM_DL_MODE_SECURE=y   # download pela USB só de imagens assinadas/criptografadas
   ```
   Crie um ambiente `[env:tubabot-producao]` no `platformio.ini` com esses ajustes e
   mantenha o `[env:tubabot]` atual para desenvolvimento (bancada).
3. **Partição `fabrica`**: com criptografia ligada, marque-a como `encrypted` na tabela de
   partições **ou** grave o perfil pelo próprio firmware (comando serial `fabrica-gravar`),
   que escreve já criptografado. O `tools/fabrica.py --gravar` (esptool direto) só serve
   para placas de desenvolvimento.
4. **Primeiro boot** de cada unidade: o bootloader gera a chave de criptografia dentro do
   chip, criptografa a flash e queima os eFuses. Depois disso, ler a flash devolve só dados
   cifrados e só firmware assinado com a nossa chave inicia.
5. **Atualizações**: todo binário publicado no servidor precisa ser assinado
   (`espsecure.py sign_data`). O aparelho confere o SHA-256 anunciado no manifesto e o
   bootloader confere a assinatura — binário adulterado não inicia.

## 4. Antes de vender (checklist)

- [ ] repositório privado; acesso revisado
- [ ] chave de assinatura gerada, guardada em cofre com backup
- [ ] ambiente de produção com Secure Boot v2 + Flash Encryption (release) testado em placas de teste
- [ ] OTA de uma versão assinada testada de ponta a ponta numa placa já protegida
- [ ] perfil de fábrica gravado pelo firmware e conferido (`fabrica`)
- [ ] série registrada em `firmware/fabrica/registro.csv` e enviada ao painel
- [ ] simulador não publicado em site aberto
