#!/bin/sh
#  Uso (como root; tarda bastante: compila gcc para ARM):  sh herramientas/instala_ndless.sh
# El Ndless SDK en /opt/Ndless: la toolchain arm-none-eabi (build_toolchain.sh,
# lo más largo) y luego el SDK (genzehn, make-prg, libndls, libnspireio). El
# make de la raíz NO es paralelizable.
set -e
cd /opt
if [ ! -d Ndless ]; then
    git clone --recursive https://github.com/ndless-nspire/Ndless.git
fi
cd /opt/Ndless
git submodule update --init --recursive
cd /opt/Ndless/ndless-sdk/toolchain
./build_toolchain.sh
test -x /opt/Ndless/ndless-sdk/toolchain/install/bin/arm-none-eabi-gcc
export PATH="/opt/Ndless/ndless-sdk/toolchain/install/bin:/opt/Ndless/ndless-sdk/bin:$PATH"
cd /opt/Ndless
make
arm-none-eabi-gcc --version | head -1
ls /opt/Ndless/ndless-sdk/bin
echo "NDLESS LISTO"
