#!/bin/sh
# ============================================================================
#  La regresión «por la capa»: la suite conducida por ui_tecla (TECLAS, como
#  siempre) y la misma suite conducida por la capa de entrada (-DPOR_LA_CAPA:
#  una mano virtual pulsa cada tecla y hace clic en la cuña de cada flecha;
#  el bucle de bucle.c las entrega). Tienen que dar las mismas comprobaciones,
#  los mismos fallos (ninguno), los mismos BMP, byte a byte, y el mismo texto
#  salvo los tiempos de reloj (las líneas con «ms») y el informe de la capa:
#  un valor que el guion lee sin redibujar sale distinto por la capa, que
#  redibuja en su bucle (el mínimo del fijador de [69] lo hacía). Y cada
#  etiqueta de sección sale una sola vez.
#
#    sh compara_capa.sh              la suite entera (~200 MB de BMP por lado)
#    HAKARU_SOLO_E4=1 sh compara_capa.sh   solo la Etapa 4 (lo mismo que el arnés)
#    VERSION=especial sh compara_capa.sh   la de la versión especial (si está)
#
#  Deja las salidas en out/directa y out/capa y las borra si todo coincide.
# ============================================================================
set -e
SUF=""
[ "$VERSION" = "especial" ] && SUF="_especial"
sh build_host.sh solo > /dev/null
sh build_host.sh capa > /dev/null
rm -rf out/directa out/capa
mkdir -p out/directa out/capa
set +e
./out/hakaru_test$SUF out/directa > out/directa.txt 2>&1
r1=$?
./out/hakaru_capa$SUF out/capa > out/capa.txt 2>&1
r2=$?
set -e
a=$(tail -3 out/directa.txt | grep comprobaciones)
b=$(tail -3 out/capa.txt | grep comprobaciones)
echo "directa: $a (salida $r1)"
echo "capa:    $b (salida $r2)"
n1=$(ls out/directa/*.bmp 2>/dev/null | wc -l)
n2=$(ls out/capa/*.bmp 2>/dev/null | wc -l)
dif=0
for f in out/directa/*.bmp; do
    [ -e "$f" ] || continue
    case "$f" in *banco_hecho*) continue ;; esac     # los tiempos medidos en el PC: cambian en cada corrida
    g="out/capa/$(basename "$f")"
    if ! cmp -s "$f" "$g"; then
        dif=$((dif + 1))
        [ $dif -le 20 ] && echo "  distinto: $(basename "$f")"
    fi
done
echo "BMP: $n1 y $n2; distintos: $dif"
grep -av -e ' ms' -e 'por la capa:' out/directa.txt > out/directa_texto.txt || true
grep -av -e ' ms' -e 'por la capa:' out/capa.txt > out/capa_texto.txt || true
texto=$(diff out/directa_texto.txt out/capa_texto.txt | grep -c '^[<>]' || true)
echo "texto (sin los tiempos): $(wc -l < out/directa_texto.txt) y $(wc -l < out/capa_texto.txt) líneas; distintas: $texto"
[ "$texto" -eq 0 ] || diff out/directa_texto.txt out/capa_texto.txt | head -10
rm -f out/directa_texto.txt out/capa_texto.txt
# Cada sección, con su etiqueta una sola vez (la que sigue a la anterior lo
# dice: «[44] (sigue)»). Byte a byte: la salida va en Latin-1
rep=$(export LC_ALL=C; grep -a '^\[[0-9]' out/directa.txt | grep -av '^\[[^]]*\] (sigue)' | cut -d']' -f1 | sort | uniq -d | sed 's/$/]/' | tr '\n' ' ')
echo "etiquetas de sección repetidas: ${rep:-ninguna}"
if [ "$a" = "$b" ] && [ $r1 -eq 0 ] && [ $r2 -eq 0 ] && [ "$n1" = "$n2" ] && [ $dif -eq 0 ] && [ "$texto" -eq 0 ] && [ -z "$rep" ]; then
    echo "IGUALES"
    rm -rf out/directa out/capa
    exit 0
fi
echo "DISTINTOS (ver out/directa.txt y out/capa.txt)"
exit 1
