#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$ROOT/.." && pwd)
JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/temurin-17-jdk}" "$REPO_ROOT/gradlew" :provider-host:installDist -p "$REPO_ROOT" >/dev/null
BUILD_DIR="$ROOT/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
qmake6 ../cloudstream-linux.pro CONFIG+=release
make -j"${JOBS:-2}"
printf '%s\n' "$BUILD_DIR/cloudstream-linux"
