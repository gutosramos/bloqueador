#!/data/data/com.termux/files/usr/bin/bash
#
# Instala o AdGuard Home dentro do Termux, sem precisar de root.
# Rode este script DENTRO do Termux, no celular antigo.
#
# Uso:
#   bash install-adguardhome.sh

set -euo pipefail

INSTALL_DIR="$HOME/AdGuardHome"
SERVICE_DIR="$PREFIX/var/service/adguardhome"

echo "==> Detectando arquitetura do celular..."
ARCH_RAW="$(uname -m)"
case "$ARCH_RAW" in
  aarch64) ARCH="arm64" ;;
  armv7l|armv8l) ARCH="armv7" ;;
  armv6l) ARCH="armv6" ;;
  i686) ARCH="386" ;;
  x86_64) ARCH="amd64" ;;
  *)
    echo "Arquitetura '$ARCH_RAW' não reconhecida. Veja os assets disponíveis em:"
    echo "https://github.com/AdguardTeam/AdGuardHome/releases/latest"
    exit 1
    ;;
esac
echo "    Arquitetura: $ARCH_RAW -> pacote linux_$ARCH"

echo "==> Instalando pacotes necessários (curl, tar, termux-services)..."
pkg update -y
pkg install -y curl tar termux-services termux-api

if [ -d "$INSTALL_DIR" ]; then
  echo "==> $INSTALL_DIR já existe, pulando download (apague a pasta para reinstalar)."
else
  URL="https://github.com/AdguardTeam/AdGuardHome/releases/latest/download/AdGuardHome_linux_${ARCH}.tar.gz"
  echo "==> Baixando AdGuard Home de:"
  echo "    $URL"
  TMP_TAR="$HOME/AdGuardHome.tar.gz"
  curl -fL --retry 3 -o "$TMP_TAR" "$URL"

  echo "==> Extraindo em $HOME..."
  tar -xzf "$TMP_TAR" -C "$HOME"
  rm -f "$TMP_TAR"
  chmod +x "$INSTALL_DIR/AdGuardHome"
fi

echo "==> Garantindo que o supervisor de serviços (runsvdir) está ativo..."
export SVDIR="$PREFIX/var/service"
export LOGDIR="$PREFIX/var/log"
if ! pgrep -f "runsvdir $SVDIR" >/dev/null 2>&1; then
  "$PREFIX/bin/service-daemon" start || true
  sleep 2
fi

echo "==> Criando serviço para o termux-services (mantém rodando e reinicia sozinho)..."
mkdir -p "$SERVICE_DIR/log"

cat > "$SERVICE_DIR/run" <<EOF
#!/data/data/com.termux/files/usr/bin/sh
exec 2>&1
cd "$INSTALL_DIR"
exec ./AdGuardHome --no-check-update -w "$INSTALL_DIR"
EOF
chmod +x "$SERVICE_DIR/run"

cat > "$SERVICE_DIR/log/run" <<EOF
#!/data/data/com.termux/files/usr/bin/sh
exec svlogd -tt "$INSTALL_DIR/logs"
EOF
chmod +x "$SERVICE_DIR/log/run"
mkdir -p "$INSTALL_DIR/logs"

echo "==> Aguardando o supervisor reconhecer o novo serviço..."
for i in $(seq 1 15); do
  [ -d "$SERVICE_DIR/supervise" ] && break
  sleep 1
done

if [ ! -d "$SERVICE_DIR/supervise" ]; then
  cat <<'EOF'

AVISO: o supervisor de serviços (runsvdir) não reconheceu o serviço a
tempo. Isso costuma acontecer na primeira instalação do termux-services.

Feche o Termux completamente (não só minimize, encerre o app de verdade),
abra de novo, e rode:

    sv-enable adguardhome
    sv up adguardhome

EOF
else
  echo "==> Habilitando e iniciando o serviço..."
  sv-enable adguardhome
  sv up adguardhome
fi

IP="$(ip route get 1 2>/dev/null | awk '{print $7; exit}' || echo 'SEU_IP_LOCAL')"

cat <<EOF

============================================================
 AdGuard Home instalado e rodando!
============================================================

1. No PRÓPRIO celular ou em outro dispositivo na mesma rede,
   abra no navegador:

       http://$IP:3000

2. Siga o assistente de configuração inicial:
     - Interface de admin: porta 3000 (pode manter)
     - Interface DNS: porta 53, escutando em 0.0.0.0 (todas as interfaces)
     - Crie um usuário/senha de administrador

3. Depois de configurado, aponte o DNS do seu roteador (ou de
   cada dispositivo) para:

       $IP

   Veja o README.md na raiz do projeto para o passo a passo
   completo de como configurar isso e manter o celular sempre
   ligado e acessível.

============================================================
EOF
