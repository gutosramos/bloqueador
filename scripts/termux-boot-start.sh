#!/data/data/com.termux/files/usr/bin/bash
#
# Copie este arquivo para ~/.termux/boot/start-adguardhome.sh
# (crie a pasta ~/.termux/boot/ se não existir).
#
# Isso exige o app "Termux:Boot" instalado (mesma loja/fonte do Termux).
# Ele faz o Termux acordar sozinho quando o celular reinicia (ex: após
# queda de energia) e garantir que o AdGuard Home volte a rodar.

# Evita que o Android suspenda o Termux em segundo plano
termux-wake-lock

# Garante que o supervisor de serviços (runsvdir) está de pé
export SVDIR="$PREFIX/var/service"
export LOGDIR="$PREFIX/var/log"
if ! pgrep -f "runsvdir $SVDIR" >/dev/null 2>&1; then
  "$PREFIX/bin/service-daemon" start || true
  sleep 2
fi

sv-enable adguardhome 2>/dev/null || true
sv up adguardhome
