#!/bin/sh
# ============================================================================
#  Compila y corre el arnés de pruebas en el PC (Linux o WSL, con gcc).
#
#    sh build_host.sh          compila y corre la batería entera
#    sh build_host.sh asan     igual, con AddressSanitizer y UBSan
#    sh build_host.sh arm      igual, con char sin signo, como en la Nspire
#    sh build_host.sh solo     compila sin correr
#    sh build_host.sh capa     compila out/hakaru_capa (-DPOR_LA_CAPA): la
#                              suite entera conducida por la capa de entrada
#                              (pulsa() y clic_en() en vez de ui_tecla); tiene
#                              que dar los mismos checks y los mismos BMP
#                              (compara_capa.sh)
#
#  Genera los BMP en ./out para revisar las pantallas sin calculadora.
#
#  La versión pública (ESPECIAL=0) es la de siempre: out/hakaru_test, con
#  sus BMP en out/. Si está la carpeta privada especial/ (no se publica), se
#  compila también la especial (ESPECIAL=1): out/hakaru_test_especial, con
#  Hakari, corre la batería entera (BMP en out/especial), y
#  out/hakaru_test_gato (HAKARI=0) solo lo que cambia con esa bandera
#  (HAKARU_SOLO_HUEVO; BMP en out/especial_gato).
# ============================================================================
set -e

SRC="tipos.c lu.c cx.c luc.c ca.c calca.c bode.c fasorial.c trazador.c ondas.c sim.c nolin.c barrido.c curva.c osc.c biblioteca.c gfx.c font6x8.c font6x12.c numfmt.c pasos.c algebra.c circuito.c red.c resuelve.c arbol.c proced.c esquema.c ejemplos.c lecciones.c archivo.c turbo.c kanji_img.c banco.c entrada.c bucle.c capmotor.c capmotor_dib.c capeq_texto.c ui.c host_test.c"
ESP_COMUN="especial/tetris.c especial/creditos.c especial/foto_img.c"
OUT="out/hakaru_test"
CHARSET="-finput-charset=UTF-8 -fexec-charset=ISO-8859-1"
FLAGS="-std=c99 -Wall -Wextra -Wshadow -Wconversion -Wno-missing-field-initializers -O2 $CHARSET"

case "$1" in
    arm)
        # En el ARM de la calculadora 'char' no tiene signo; en el PC sí.
        # Con esto el arnés corre con la misma regla que la Nspire.
        FLAGS="$FLAGS -funsigned-char -Wchar-subscripts"
        ;;
    asan)
        FLAGS="-std=c99 -Wall -Wextra -Wshadow -Wno-missing-field-initializers -O1 -g -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer $CHARSET"
        ;;
    capa)
        FLAGS="$FLAGS -DPOR_LA_CAPA"
        OUT="out/hakaru_capa"
        ;;
esac

mkdir -p out
echo "compilando..."
if [ -f especial/especial.h ]; then
    mkdir -p out/especial out/especial_gato
    EFLAGS="$FLAGS -DESPECIAL=1 -I. -Iespecial"
    gcc $FLAGS -DESPECIAL=0 -o "$OUT" $SRC -lm &
    p1=$!
    gcc $EFLAGS -DHAKARI=1 -o "${OUT}_especial" $SRC $ESP_COMUN especial/hakari.c especial/hakari_img.c -lm &
    p2=$!
    gcc $EFLAGS -DHAKARI=0 -o "${OUT}_gato" $SRC $ESP_COMUN especial/gato.c especial/gato_img.c -lm &
    p3=$!
    wait $p1
    wait $p2
    wait $p3
    echo "OK -> $OUT (pública), ${OUT}_especial (con Hakari) y ${OUT}_gato (con el gato)"
else
    gcc $FLAGS -DESPECIAL=0 -o "$OUT" $SRC -lm
    echo "OK -> $OUT"
fi

if [ "$1" = "solo" ] || [ "$1" = "capa" ]; then
    exit 0
fi

# Una tras otra: las pruebas de tiempos miden el reloj y fallan con otras
# baterías a la vez
r=0
"./$OUT" out || r=1
if [ -f especial/especial.h ]; then
    echo "--- versión especial (ESPECIAL=1, con Hakari) ---"
    "./${OUT}_especial" out/especial || r=1
    echo "--- versión especial con el gato (HAKARI=0): lo que cambia con la bandera ---"
    HAKARU_SOLO_HUEVO=1 "./${OUT}_gato" out/especial_gato || r=1
fi
exit $r
