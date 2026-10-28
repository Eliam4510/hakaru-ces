#!/bin/sh
#  Uso (como root, en Ubuntu 24.04):  sh herramientas/instala_wsl.sh
# Entorno de Hakaru CES en WSL o Linux (Ubuntu 24.04): gcc para el arnés y todo lo que
# pide el Ndless SDK para construir su toolchain arm-none-eabi.
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y build-essential git wget curl ca-certificates python3 python3-dev php-cli \
    libmpfr-dev libmpc-dev libgmp-dev zlib1g-dev libpng-dev libboost-program-options-dev \
    texinfo bison flex libncurses-dev xz-utils bzip2 file
gcc --version | head -1
python3 --version
php --version | head -1
