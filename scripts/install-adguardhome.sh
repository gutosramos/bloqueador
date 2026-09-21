#!/data/data/com.termux/files/usr/bin/bash
#
# Instala o AdGuard Home dentro do Termux, sem precisar de root.
# Rode este script DENTRO do Termux, no celular antigo.
#
# Uso:
#   bash install-adguardhome.sh
#
# Por que via proot-distro (Debian)?
# O binário oficial do AdGuard Home para Linux não é compilado como PIE.
# A partir do Android 10, o sistema recusa executar esse tipo de binário
# diretamente dentro do Termux (erro "unexpected e_type: 2"). A forma
# suportada de contornar isso é rodá-lo dentro de um ambiente Debian
# via proot-distro, que emula um Linux completo e não tem essa restrição.

set -euo pipefail

DISTRO="debian"
CONTAINER_DIR="/root/AdGuardHome"
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

echo "==> Instalando pacotes necessários (proot-distro, termux-services)..."
pkg update -y
pkg install -y proot-distro termux-services termux-api

echo "==> Instalando ambiente Debian via proot-distro..."
echo "    (só baixa na primeira vez, ~300-500MB, pode demorar num celular antigo)"
if proot-distro list -q 2>/dev/null | grep -qx "$DISTRO"; then
  echo "    Debian já instalado, pulando."
else
  proot-distro install "$DISTRO"
fi

echo "==> Baixando e instalando o AdGuard Home dentro do Debian..."
URL="https://github.com/AdguardTeam/AdGuardHome/releases/latest/download/AdGuardHome_linux_${ARCH}.tar.gz"
proot-distro login "$DISTRO" -- sh -c "
  set -e
  if [ -x '$CONTAINER_DIR/AdGuardHome' ]; then
    echo '    $CONTAINER_DIR já existe, pulando download (apague a pasta dentro do Debian para reinstalar).'
  else
    apt-get update -qq
    apt-get install -y -qq curl ca-certificates
    curl -fL --retry 3 -o /root/AdGuardHome.tar.gz '$URL'
    tar -xzf /root/AdGuardHome.tar.gz -C /root
    rm -f /root/AdGuardHome.tar.gz
    chmod +x '$CONTAINER_DIR/AdGuardHome'
  fi
"

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
exec proot-distro login $DISTRO -- $CONTAINER_DIR/AdGuardHome --no-check-update -w $CONTAINER_DIR
EOF
chmod +x "$SERVICE_DIR/run"

mkdir -p "$HOME/AdGuardHome-logs"
cat > "$SERVICE_DIR/log/run" <<EOF
#!/data/data/com.termux/files/usr/bin/sh
exec svlogd -tt "$HOME/AdGuardHome-logs"
EOF
chmod +x "$SERVICE_DIR/log/run"

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

   (pode demorar alguns segundos a mais que o normal pra responder
   na primeira vez, por estar rodando dentro do ambiente Debian)

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
