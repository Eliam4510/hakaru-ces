#!/bin/sh
# ============================================================================
#  Sesiones largas del "mono": millones de teclas al azar por toda la app,
#  con varias semillas. Uso:  sh mono.sh [teclas por sesion] [asan]
#  Con VERSION=especial, el de la versión especial (si está especial/).
# ============================================================================
N=${1:-1000000}
SUF=""; DIR=""
[ "$VERSION" = "especial" ] && { SUF="_especial"; DIR="/especial"; mkdir -p out/especial; }
if [ "$2" = "asan" ]; then sh build_host.sh asan > /dev/null 2>&1 || exit 1
else sh build_host.sh solo > /dev/null 2>&1 || exit 1; fi
for s in 11 222 3333 44444; do
    printf "semilla %-6s " "$s"
    HAKARU_SEMILLA=$s HAKARU_MONO=$N ./out/hakaru_test$SUF out$DIR 2>&1 \
        | grep -aE "FALLO|comprobaciones" | tr '\n' ' ' | iconv -f latin1 -t utf-8
    echo
done
