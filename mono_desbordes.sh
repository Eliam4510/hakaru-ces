#!/bin/sh
# Lista los textos que se salen de la pantalla en sesiones largas del mono.
# Con VERSION=especial, el de la versión especial (si está especial/).
SUF=""; DIR=""
[ "$VERSION" = "especial" ] && { SUF="_especial"; DIR="/especial"; mkdir -p out/especial; }
sh build_host.sh solo > /dev/null 2>&1 || exit 1
for s in 11 222 3333; do
    HAKARU_DEPURA=1 HAKARU_SEMILLA=$s HAKARU_MONO=${1:-1000000} ./out/hakaru_test$SUF out$DIR 2>&1 | grep -a 'desborde'
done | iconv -f latin1 -t utf-8 | sort | uniq -c | sort -rn | head -20
