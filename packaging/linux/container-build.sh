#!/bin/bash
set -euo pipefail
apt-get -o Acquire::ForceIPv4=true -o Acquire::http::Timeout=30 -o Acquire::Retries=1 update
DEBIAN_FRONTEND=noninteractive apt-get -o Acquire::ForceIPv4=true -o Acquire::http::Timeout=30 -o Acquire::Retries=1 install -y --no-install-recommends build-essential qt6-base-dev libqt6opengl6-dev libmpv-dev libsdl2-dev dpkg-dev rpm squashfs-tools curl ca-certificates openjdk-17-jre-headless qt6-image-formats-plugins qt6-wayland ffmpeg xvfb xauth
mkdir -p /tmp/build
rm -rf /tmp/linux-native
cp -a /src/linux-native /tmp/linux-native
cd /tmp/build
qmake6 /tmp/linux-native/cloudstream-linux.pro CONFIG+=release
make -j4
cp cloudstream-linux /out/cloudstream-linux-ubuntu24.04
sleep infinity
