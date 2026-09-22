# Bloqueador de Anúncios

Transforma um dispositivo que você já tem em um **servidor DNS bloqueador de
anúncios** pra toda a sua rede Wi-Fi. Depois de configurado, qualquer
dispositivo conectado no seu Wi-Fi (celulares, TVs, notebooks, consoles)
passa a ter anúncios e rastreadores bloqueados automaticamente, sem
instalar nada neles.

Duas formas de fazer isso, neste repositório:

- **[ESP8266](esp8266/)** (recomendado) — um microcontrolador tipo Wemos/Lolin
  D1 Mini rodando um DNS sinkhole próprio. Sem sistema operacional, sem
  restrição de porta, sem precisar de root — a porta 53 simplesmente funciona.
  Custa poucos reais e consome pouquíssima energia.
- **[Celular Android antigo](#celular-android-antigo-termux)** — usa
  [AdGuard Home](https://adguard.com/en/adguard-home/overview.html) rodando
  dentro do [Termux](https://termux.dev/). **Importante:** isso só funciona
  em Android relativamente antigo/permissivo — a partir do Android 10+ (e
  principalmente em aparelhos mais novos com mais "hardening", como
  Samsung recentes), o próprio Android bloqueia a porta 53 pra apps sem
  root, e não existe contorno sem root pra isso. Veja os detalhes na seção
  correspondente antes de tentar essa via.

## Celular Android antigo (Termux)

> ⚠️ **Leia antes de começar:** a partir do Android 10 (e especialmente em
> aparelhos mais recentes/com mais "hardening" de fabricante, como Samsung
> em versões recentes), o próprio sistema **bloqueia apps sem root de
> escutar na porta 53** (a porta padrão de DNS) — e não existe truque
> (proot, containers, etc.) que contorne isso de verdade, porque a
> restrição é aplicada pelo kernel real do aparelho. Testamos isso a fundo
> num Samsung Galaxy A54 (Android 16): sem root, o AdGuard Home nunca
> consegue abrir a porta 53 pra rede. Com root, também não rolou nesse
> caso, porque a Samsung removeu a opção de desbloqueio de bootloader a
> partir do One UI 8/Android 16, sem contorno confiável. Se seu celular é
> relativamente novo, tem grande chance de bater nesse mesmo problema —
> nesse caso, use o **[ESP8266](esp8266/)** em vez disso. Esse caminho aqui
> só é recomendado se você tem certeza que seu Android é antigo o
> suficiente pra não ter essa restrição (o jeito mais confiável de saber é
> tentar e ver se a porta 53 abre).

## Como funciona

```
[Roteador Wi-Fi] --DNS aponta para--> [Celular antigo rodando AdGuard Home]
        |                                        |
        +---- Notebook, TV, celulares -----------+
                (todo tráfego DNS passa pelo celular,
                 que filtra domínios de anúncios/rastreamento)
```

O celular antigo fica ligado 24h (no carregador), conectado ao Wi-Fi, e
responde às consultas DNS de todos os outros aparelhos da casa. Quando um
app tenta carregar um domínio de anúncio conhecido, o AdGuard Home
simplesmente não resolve o endereço — o anúncio nunca chega a ser baixado.

## Requisitos

- Um celular Android antigo (Android 7+ funciona bem; a partir do Android 10
  é obrigatório usar o ambiente Debian via `proot-distro` descrito abaixo).
- Pelo menos ~1GB de espaço livre (o ambiente Debian usado para rodar o
  AdGuard Home ocupa uns 300-500MB na primeira instalação).
- Carregador para deixá-lo ligado permanentemente.
- Wi-Fi doméstico com acesso às configurações do roteador (para trocar o DNS).
- **Termux** instalado a partir da [F-Droid](https://f-droid.org/en/packages/com.termux/)
  ou do [GitHub](https://github.com/termux/termux-app/releases) — **não use a
  versão da Play Store**, ela está descontinuada e desatualizada.
- **Termux:Boot** (mesma fonte acima) — garante que tudo volte a funcionar
  sozinho se o celular reiniciar (ex: falta de energia).

## Passo a passo

### 1. Preparar o celular

1. Instale o **Termux** e o **Termux:Boot** (F-Droid).
2. Conecte o celular ao Wi-Fi.
3. Nas configurações do celular, defina um **IP fixo** para ele na rede
   (ou reserve o IP dele no roteador, via DHCP reservation, usando o
   endereço MAC do celular). Isso evita que o DNS "quebre" quando o IP mudar.
4. Desative a otimização de bateria para o Termux e o Termux:Boot
   (Configurações → Bateria → Otimização de bateria → Termux → Não otimizar).
5. Deixe o celular sempre no carregador, com Wi-Fi sempre ativo.

### 2. Instalar o AdGuard Home

Abra o Termux no celular e rode:

```bash
pkg install -y git
git clone https://github.com/gutosramos/bloqueador bloqueador
cd bloqueador
bash scripts/install-adguardhome.sh
```

O script detecta a arquitetura do celular, instala um ambiente Debian
mínimo via `proot-distro` (necessário porque o Android moderno não deixa
rodar o binário oficial do AdGuard Home diretamente no Termux — veja a
nota no início do script se tiver curiosidade), baixa o AdGuard Home
dentro desse ambiente, e o configura como serviço (`termux-services`),
para que reinicie sozinho se cair.

> Se você não quiser usar `git`, basta copiar o conteúdo de
> `scripts/install-adguardhome.sh` para um arquivo no celular (ex: com o
> Termux `nano install.sh`) e rodar `bash install.sh`.

### 3. Configurar o AdGuard Home

Ao final do script, ele mostra o IP do celular. Em qualquer navegador na
mesma rede, acesse:

```
http://<ip-do-celular>:3000
```

Siga o assistente:

- **Interface de administração**: porta `3000` (padrão).
- **Interface DNS**: porta `53`, escutando em **todas as interfaces**
  (`0.0.0.0`) — assim outros aparelhos da rede conseguem consultar.
- Crie um usuário e senha de administrador.
- Na lista de "Filtros" (Filters → DNS blocklists), ative as listas
  padrão sugeridas (AdGuard DNS filter, EasyList, etc.) — já bloqueiam a
  grande maioria dos anúncios e rastreadores.

### 4. Ativar o auto-início no boot

Para que o AdGuard Home volte a funcionar sozinho se o celular reiniciar:

```bash
mkdir -p ~/.termux/boot
cp scripts/termux-boot-start.sh ~/.termux/boot/start-adguardhome.sh
chmod +x ~/.termux/boot/start-adguardhome.sh
```

Abra o app **Termux:Boot** uma vez (ele só precisa ser aberto uma vez para
o Android registrar a permissão de iniciar no boot).

### 5. Apontar sua rede para o celular

Você tem duas opções:

**Opção A — configurar no roteador (recomendado, cobre todos os dispositivos):**

1. Acesse a interface de administração do roteador (geralmente
   `192.168.0.1` ou `192.168.1.1`).
2. Procure a configuração de **DNS** (às vezes dentro de "DHCP" ou
   "WAN/Internet").
3. Defina o DNS primário como o **IP do celular**.
4. Salve e reinicie o roteador. Os dispositivos que já estavam conectados
   podem precisar desconectar/reconectar do Wi-Fi para pegar o novo DNS.

**Opção B — configurar manualmente em cada dispositivo:**

Se o seu roteador não permitir mudar o DNS, configure o DNS manualmente
nas configurações de Wi-Fi de cada celular/notebook/TV, apontando para o
IP do celular antigo.

### 6. Testar

- Acesse `http://<ip-do-celular>:3000` e veja o painel do AdGuard Home
  mostrando consultas DNS chegando dos outros dispositivos.
- Visite um site com bastante anúncio (ex: sites de notícias) em algum
  dispositivo da rede e veja se os banners somem.
- Teste em https://d3ward.github.io/toolz/adblock.html a partir de um
  dispositivo usando o novo DNS.

## Dicas e solução de problemas

- **Alguns anúncios em apps (não navegador) continuam aparecendo**: apps
  que embutem anúncios diretamente no próprio código (ex: alguns jogos)
  não dependem só de DNS externo — bloqueio de DNS reduz bastante, mas não
  elimina 100%.
- **Sites HTTPS com "DNS over HTTPS" embutido** (ex: alguns navegadores com
  DoH ativado por padrão, tipo Firefox/Chrome) podem ignorar o DNS da rede.
  Desative "DNS privado"/DoH nas configurações de rede desses
  navegadores/dispositivos para garantir que usem o AdGuard Home.
- **Celular esquenta ou trava**: normal em aparelhos muito antigos rodando
  24h. Deixe em local ventilado; considere subir o Android para uma ROM
  mais leve (ex: LineageOS) se o aparelho suportar.
- **Quero adicionar mais listas de bloqueio**: no painel do AdGuard Home,
  vá em Filters → DNS blocklists → Add blocklist, e cole a URL de listas
  como as do [oisd.nl](https://oisd.nl/) ou [firebog.net](https://firebog.net/).
- **Quero bloquear categorias específicas** (redes sociais, adultos, etc.):
  use a aba "Parental control" / "Filters" do AdGuard Home.
- **Verificar se o serviço está rodando** dentro do Termux:
  ```bash
  sv status adguardhome
  ```
- **Reiniciar o serviço manualmente**:
  ```bash
  sv restart adguardhome
  ```
- **Ver os logs do AdGuard Home** (útil quando o serviço não sobe):
  ```bash
  cat ~/AdGuardHome-logs/current
  ```
- **Entrar no ambiente Debian manualmente** (para depurar por dentro,
  ex: checar se o binário existe, testar rodar na mão, etc.):
  ```bash
  proot-distro login debian
  cd /root/AdGuardHome && ./AdGuardHome --no-check-update -w /root/AdGuardHome
  ```

## Estrutura deste repositório

```
.
├── README.md                       # este guia (celular/Termux)
├── scripts/
│   ├── install-adguardhome.sh      # instala e configura o AdGuard Home no Termux
│   └── termux-boot-start.sh        # script de auto-início (copiar p/ ~/.termux/boot/)
└── esp8266/                        # alternativa recomendada: DNS sinkhole em ESP8266
    ├── README.md                   # guia de instalação do ESP8266
    ├── bloqueador_esp8266.ino      # sketch Arduino
    ├── tools/gen_blocklist.py      # gera a lista de bloqueio (blocklist.bin)
    └── data/                       # onde fica o blocklist.bin antes do upload
```
