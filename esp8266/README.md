# Bloqueador de Anúncios com ESP8266 (Wemos/Lolin D1 Mini)

Alternativa ao celular: um ESP8266 roda um servidor DNS "sinkhole" direto
(sem sistema operacional, sem Android, sem essa dor de cabeça de porta
privilegiada que travou a versão com celular — veja o porquê no
[README principal](../README.md)). Custa poucos reais, consome pouquíssima
energia e funciona 24h sem problema.

A lista de bloqueio fica gravada na flash do ESP8266 como um array
ordenado de hashes, e a busca é feita com busca binária direto na flash
— não precisa caber tudo na RAM, então dá pra ter uma lista de
dezenas/centenas de milhares de domínios mesmo com a pouca RAM do
ESP8266 (~80KB).

## O que você precisa

- Uma placa Wemos/Lolin D1 Mini (ou qualquer ESP8266 com pelo menos 4MB
  de flash).
- Cabo USB (de dados, não só de carga).
- [Arduino IDE](https://www.arduino.cc/en/software) instalado no seu PC.
- Python 3 (pra gerar a lista de bloqueio).

## 1. Instalar o suporte a ESP8266 no Arduino IDE

1. Arduino IDE → Preferências → em "URLs Adicionais para Gerenciadores
   de Placas", adicione:
   ```
   https://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
2. Ferramentas → Placa → Gerenciador de Placas → procure "esp8266" →
   instale o pacote da comunidade ESP8266.
3. Ferramentas → Placa → selecione **"LOLIN(WEMOS) D1 R2 & mini"**.

## 2. Instalar o plugin de upload do LittleFS

O sketch guarda a lista de bloqueio na flash (LittleFS), que é enviada
separadamente do código. Como fazer isso depende da versão do Arduino IDE:

- **Arduino IDE 2.x**: instale o plugin
  [arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload)
  (siga as instruções de instalação do próprio repositório — é uma
  extensão que se instala na pasta de plugins do Arduino IDE 2).
- **Arduino IDE 1.8.x**: instale o plugin clássico
  [arduino-esp8266fs-plugin](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin),
  que adiciona a opção "ESP8266 LittleFS Data Upload" no menu Ferramentas.

## 3. Gerar a lista de bloqueio

```bash
cd esp8266
python3 tools/gen_blocklist.py
```

Isso baixa a lista pública do [StevenBlack/hosts](https://github.com/StevenBlack/hosts)
e gera `blocklist.bin` na pasta atual. Mova esse arquivo pra dentro de
`esp8266/data/`:

```bash
mv blocklist.bin data/
```

> Quer usar outra lista (ex: uma baixada do [oisd.nl](https://oisd.nl/)
> ou [firebog.net](https://firebog.net/))? Baixe o arquivo no formato
> "hosts" e rode `python3 tools/gen_blocklist.py minha_lista.txt`.

## 4. Configurar o sketch

Abra `bloqueador_esp8266.ino` no Arduino IDE e ajuste no topo do
arquivo:

- `WIFI_SSID` / `WIFI_PASSWORD`: sua rede Wi-Fi.
- `LOCAL_IP` / `GATEWAY` / `SUBNET`: um IP fixo pro ESP8266 dentro da
  sua rede (fora da faixa de DHCP do roteador, pra não conflitar).
- `UPSTREAM_DNS`: o DNS de verdade pra onde mandar tudo que não está
  bloqueado (padrão: `1.1.1.1`, Cloudflare).

## 5. Gravar no ESP8266

1. Conecte a placa via USB, selecione a porta certa em Ferramentas → Porta.
2. Em Ferramentas, escolha um layout de flash com espaço suficiente pro
   filesystem (ex: "4MB (FS:3MB OTA:~512KB)") — quanto maior a parte
   reservada pro FS, maior a lista de bloqueio que cabe.
3. Primeiro envie o filesystem: Ferramentas → "ESP8266 LittleFS Data
   Upload" (1.8.x) ou o comando equivalente do plugin (2.x).
4. Depois envie o sketch normalmente (botão de Upload).
5. Abra o Monitor Serial (115200 baud) pra ver o IP atribuído e
   confirmar que a lista de bloqueio carregou.

## 6. Apontar sua rede pro ESP8266

Mesma lógica do projeto original: configure o DNS do seu roteador (ou
da TV/dispositivo específico) pro IP fixo que você definiu em
`LOCAL_IP`. Veja a seção "5. Apontar sua rede" no
[README principal](../README.md) — o procedimento é idêntico, só troca
o IP do celular pelo IP do ESP8266.

## Painel de status

Acesse `http://<ip-do-esp8266>/` no navegador pra ver quantas consultas
foram feitas, quantas foram bloqueadas, memória livre, etc.

## Limitações

- Processa uma consulta DNS por vez (não é paralelo) — tranquilo pra
  uma casa, não serve pra um ambiente com centenas de dispositivos.
- Sem HTTPS/admin com senha no painel de status — não exponha a porta
  80 pra internet.
- TTL das respostas bloqueadas é fixo em 60s.
