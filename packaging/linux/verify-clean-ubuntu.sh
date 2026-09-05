#!/bin/bash
set -euo pipefail
cp /src/packaging/linux/ubuntu.sources /etc/apt/sources.list.d/ubuntu.sources
mkdir -p /etc/ssl/certs
cp /host-ca.pem /etc/ssl/certs/ca-certificates.crt
apt-get -o Acquire::ForceIPv4=true -o Acquire::https::Timeout=30 -o Acquire::Retries=1 update
DEBIAN_FRONTEND=noninteractive apt-get -y --no-install-recommends install /out/cloudstream-pc_0.1.0~preview.2_amd64.deb xvfb xauth
test -f /usr/lib/x86_64-linux-gnu/qt6/plugins/iconengines/libqsvgicon.so
mkdir -p /tmp/test-home
set +e
HOME=/tmp/test-home timeout -k 5 12 xvfb-run -a /usr/bin/cloudstream-pc >/out/verification/clean-ubuntu24-launch.log 2>&1
rc=$?
set -e
printf 'Installed DEB launch exit: %s (124 means survived bounded test)\n' "$rc"
test "$rc" = 124
/usr/libexec/cloudstream/provider-host/bin/cloudstream-provider-host repository-candidates /usr/libexec/cloudstream/provider-host/lib/provider-host-4.8.0.jar >/out/verification/clean-ubuntu24-provider.json
dpkg-query -W cloudstream-pc libc6 libqt6core6t64 libqt6svg6 libmpv2 libsdl2-2.0-0 openjdk-17-jre-headless ffmpeg >/out/verification/clean-ubuntu24-versions.txt
apt-get check
