#!/usr/bin/env python3
"""Install the current Nobara build beside the existing CloudStream AppImage."""

import argparse
import hashlib
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
HOME = Path.home()
SOURCE_BINARY = REPO / "linux-native/build/cloudstream-linux"
SOURCE_HOST = REPO / "provider-host/build/install/cloudstream-provider-host"
SOURCE_ICON = REPO / "linux-native/assets/cloudstream-launcher.png"
INSTALL = HOME / ".local/share/cloudstream-pc-test"
APP_ID = "io.github.recloudstream.cloudstream-local-test"
MENU_ENTRY = HOME / f".local/share/applications/{APP_ID}.desktop"
DESKTOP_ENTRY = HOME / "Desktop/CloudStream PC (Test Build).desktop"
ICON = HOME / f".local/share/icons/hicolor/192x192/apps/{APP_ID}.png"


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def entry_text(wrapper: Path) -> str:
    return ("[Desktop Entry]\n"
            "Version=1.0\n"
            "Type=Application\n"
            "Name=CloudStream PC (Test Build)\n"
            "GenericName=Media Center\n"
            "Comment=Local test build; uses the existing CloudStream profile\n"
            f"Exec={wrapper}\n"
            f"TryExec={wrapper}\n"
            f"Icon={APP_ID}\n"
            "Terminal=false\n"
            "Categories=AudioVideo;Video;Player;\n"
            "Keywords=streaming;video;media;player;\n"
            "StartupNotify=true\n")


def check_sources() -> None:
    if not SOURCE_BINARY.is_file() or SOURCE_BINARY.open("rb").read(4) != b"\x7fELF":
        raise RuntimeError("Build the native app first: linux-native/build.sh")
    if not (SOURCE_HOST / "bin/cloudstream-provider-host").is_file():
        raise RuntimeError("Build the provider host first: :provider-host:installDist")
    if not list((SOURCE_HOST / "lib").glob("*.jar")):
        raise RuntimeError("Provider-host libraries are missing")
    if not SOURCE_ICON.is_file():
        raise RuntimeError("CloudStream icon is missing")


def atomic_text(path: Path, text: str, mode: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=path.parent,
                                     prefix=f".{path.name}.", delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(text)
    try:
        temporary.chmod(mode)
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def install() -> None:
    check_sources()
    INSTALL.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=".cloudstream-pc-test-stage-", dir=INSTALL.parent))
    try:
        (stage / "bin").mkdir()
        shutil.copy2(SOURCE_BINARY, stage / "bin/cloudstream-linux")
        shutil.copytree(SOURCE_HOST, stage / "libexec/cloudstream/provider-host")
        wrapper = stage / "bin/cloudstream-pc-test"
        wrapper.write_text("#!/bin/sh\nset -eu\n"
                           "if [ -z \"${JAVA_HOME:-}\" ] && [ -x /usr/lib/jvm/temurin-17-jdk/bin/java ]; then\n"
                           "    export JAVA_HOME=/usr/lib/jvm/temurin-17-jdk\n"
                           "fi\n"
                           "exec \"$(dirname \"$0\")/cloudstream-linux\" \"$@\"\n")
        wrapper.chmod(0o755)
        expected = digest(SOURCE_BINARY)
        if digest(stage / "bin/cloudstream-linux") != expected:
            raise RuntimeError("Staged binary does not match the build")
        if INSTALL.exists():
            backup = INSTALL.with_name(INSTALL.name + ".backup-" + str(os.getpid()))
            os.replace(INSTALL, backup)
            try:
                os.replace(stage, INSTALL)
            except BaseException:
                os.replace(backup, INSTALL)
                raise
            print(f"Previous test build preserved: {backup}")
        else:
            os.replace(stage, INSTALL)
    finally:
        if stage.exists():
            shutil.rmtree(stage)

    wrapper = INSTALL / "bin/cloudstream-pc-test"
    desktop = entry_text(wrapper)
    with tempfile.TemporaryDirectory(prefix="cloudstream-desktop-check-") as checkdir:
        draft = Path(checkdir) / f"{APP_ID}.desktop"
        draft.write_text(desktop)
        subprocess.run(["desktop-file-validate", str(draft)], check=True)
    ICON.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(SOURCE_ICON, ICON)
    atomic_text(MENU_ENTRY, desktop, 0o644)
    atomic_text(DESKTOP_ENTRY, desktop, 0o755)
    subprocess.run(["update-desktop-database", str(MENU_ENTRY.parent)], check=True)
    if shutil.which("kbuildsycoca6"):
        subprocess.run(["kbuildsycoca6", "--noincremental"],
                       check=True, stdout=subprocess.DEVNULL)
    if digest(INSTALL / "bin/cloudstream-linux") != expected:
        raise RuntimeError("Installed binary hash differs from the build")
    print(f"Installed: {INSTALL}")
    print(f"Desktop shortcut: {DESKTOP_ENTRY}")
    print(f"SHA-256: {expected}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="only validate build inputs")
    options = parser.parse_args()
    if options.check:
        check_sources()
        print("Native binary, provider-host distribution, and icon are present")
    else:
        install()
