#!/bin/bash

set -xeuo pipefail

USE_APT_CLANG=${USE_APT_CLANG:-false}
ENABLE_STATIC=${ENABLE_STATIC:-false}

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root"
  exit 1
fi

if [ ! -f /etc/debian_version ]; then
  echo "This script is intended for Debian-based systems only."
  exit 1
fi

apt update
apt install -y \
  make \
  flex \
  bison \
  g++ \
  libfl-dev \
  libgmp-dev \
  libjemalloc-dev

if [ "$ENABLE_STATIC" = true ]; then
  apt install -y libc6-dev
fi

if [ "$USE_APT_CLANG" = true ]; then
  apt install -y clang
else
  echo "clang is not installed via apt, please install clang (19+) manually."
  echo "HINT: run USE_APT_CLANG=true ${0} to install clang via this script."
fi
