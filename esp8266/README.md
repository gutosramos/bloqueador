# Bloqueador de Anúncios com ESP8266 (Wemos/Lolin D1 Mini)

Alternativa ao celular: um ESP8266 roda um servidor DNS "sinkhole" direto
(sem sistema operacional, sem Android, sem essa dor de cabeça de porta
privilegiada que travou a versão com celular — veja o porquê no
[README principal](../README.md)). Custa poucos reais, consome pouquíssima
energia e funciona 24h sem problema. Testado e validado num Wemos/Lolin
D1 Mini real (chip ESP8266EX, 4MB flash).

A lista de bloqueio fica gravada na flash do ESP8266 como um array
ordenado de hashes de 64 bits, e a busca é feita com busca binária
direto na flash — não precisa caber tudo na RAM, então dá pra ter uma
lista de dezenas/centenas de milhares de domínios mesmo com a pouca RAM
do ESP8266 (~80KB). Com a lista padrão (StevenBlack/hosts, ~76 mil
domínios) usa só uns 600KB de flash e ~30KB de RAM.

## O que você precisa

- Uma placa Wemos/Lolin D1 Mini (ou qualquer ESP8266 com pelo menos 4MB
  de flash).
- Cabo USB de dados (não só de carga).
- Python 3.
- [PlatformIO Core](https://platformio.org/) — instala e grava tudo via
  terminal, sem precisar do Arduino IDE gráfico:
  ```bash
  pip install --user platformio
  export PATH="$HOME/.local/bin:$PATH"   # adicione essa linha no seu .bashrc
  ```

> Prefere o Arduino IDE gráfico? Também funciona: instale o suporte a
> ESP8266 (URL de placas adicionais:
> `https://arduino.esp8266.com/stable/package_esp8266com_index.json`),
> abra `src/bloqueador_esp8266.ino`, e use um plugin de upload do
> LittleFS ([Arduino IDE 2.x](https://github.com/earlephilhower/arduino-littlefs-upload) /
> [1.8.x](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin))
> pra enviar a pasta `data/`. O resto do guia abaixo assume PlatformIO.

## 1. Configurar suas credenciais

```bash
cd esp8266
cp src/secrets.h.example src/secrets.h
```

Edite `src/secrets.h` com sua rede Wi-Fi e o IP fixo que você quer pro
ESP8266 (fora da faixa de DHCP do roteador, pra não conflitar). Esse
arquivo **não vai pro git** — fica só na sua máquina.

Se quiser trocar o DNS upstream (padrão: Cloudflare `1.1.1.1`), edite
`UPSTREAM_DNS` direto em `src/bloqueador_esp8266.ino`.

## 2. Gerar a lista de bloqueio

```bash
python3 tools/gen_blocklist.py
mv blocklist.bin data/
```

Isso baixa a lista pública do [StevenBlack/hosts](https://github.com/StevenBlack/hosts).
Quer usar outra (ex: [oisd.nl](https://oisd.nl/), [firebog.net](https://firebog.net/))?
Baixe o arquivo no formato "hosts" e rode
`python3 tools/gen_blocklist.py minha_lista.txt`.

## 3. Conectar e gravar

Conecte a placa via USB. No Linux, seu usuário provavelmente vai
precisar de permissão pra porta serial (grupo `dialout`):

```bash
sudo usermod -aG dialout $USER
# depois disso, feche e abra o terminal (ou rode os comandos abaixo
# prefixados com `sg dialout -c '...'` pra não precisar relogar)
```

Grave o sistema de arquivos (lista de bloqueio) e depois o firmware:

```bash
pio run -t uploadfs   # grava a lista de bloqueio
pio run -t upload     # grava o firmware
```

Acompanhe o boot pelo monitor serial:

```bash
pio device monitor -b 115200
```

Você deve ver algo como:

```
Lista de bloqueio carregada: 76230 dominios
Conectando ao Wi-Fi.....
Conectado! IP: 192.168.0.144
Servidor DNS ativo na porta 53. Painel de status em http://<ip>/
```

> Se ficar travado em "Conectando ao Wi-Fi" por mais de 30s, o próprio
> firmware reinicia sozinho e tenta de novo — confira se o SSID/senha em
> `secrets.h` estão certos (lembre que o ESP8266 só enxerga rede
> **2.4GHz**, não 5GHz).

## 4. Apontar sua rede pro ESP8266

Mesma lógica do projeto original: configure o DNS do seu roteador (ou
da TV/dispositivo específico) pro IP fixo que você definiu em
`secrets.h`. Veja a seção "5. Apontar sua rede" no
[README principal](../README.md) — o procedimento é idêntico, só troca
o IP do celular pelo IP do ESP8266.

## Testar

```bash
nslookup doubleclick.net <ip-do-esp8266>   # deve voltar 0.0.0.0
nslookup google.com <ip-do-esp8266>        # deve resolver normal
```

## Painel de status

Acesse `http://<ip-do-esp8266>/` no navegador pra ver quantas consultas
foram feitas, quantas foram bloqueadas, memória livre, etc.

## Limitações

- Processa uma consulta DNS por vez (não é paralelo) — tranquilo pra
  uma casa, não serve pra um ambiente com centenas de dispositivos.
- Sem HTTPS/admin com senha no painel de status — não exponha a porta
  80 pra internet.
- TTL das respostas bloqueadas é fixo em 60s.
- ESP8266 só conecta em Wi-Fi **2.4GHz** (não suporta 5GHz).
