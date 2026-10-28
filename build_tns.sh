#!/bin/sh
# ============================================================================
#  Compila hakaru_ces.tns con el Ndless SDK (en WSL, bajo /opt/Ndless).
#
#    sh build_tns.sh             la versión pública
#    sh build_tns.sh especial    la especial (necesita la carpeta privada
#                                especial/; con HAKARI=0 en el entorno, el gato)
#    sh build_tns.sh clean       borra los intermedios
# ============================================================================
set -e
export PATH="/opt/Ndless/ndless-sdk/toolchain/install/bin:/opt/Ndless/ndless-sdk/bin:$PATH"

if [ "$1" = "clean" ]; then
    make clean
    exit 0
fi

make clean > /dev/null
if [ "$1" = "especial" ]; then
    make ESPECIAL=1 HAKARI="${HAKARI:-1}"
else
    make
fi
ls -la hakaru_ces.tns
