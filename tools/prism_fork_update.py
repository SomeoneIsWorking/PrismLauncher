#!/usr/bin/env python3
"""Install stable Flatpak releases from SomeoneIsWorking/PrismLauncher."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

APP_ID = "org.prismlauncher.PrismLauncher"
REPOSITORY = "SomeoneIsWorking/PrismLauncher"
API_URL = f"https://api.github.com/repos/{REPOSITORY}/releases/latest"
ARCHES = {"x86_64": "x86_64", "aarch64": "aarch64"}
VERSION = re.compile(r"^[0-9]+(?:\.[0-9]+){2}$")


def version_tuple(value: str) -> tuple[int, int, int]:
    if not VERSION.fullmatch(value):
        raise ValueError(f"Unsupported release version: {value!r}")
    major, minor, patch = map(int, value.split("."))
    return major, minor, patch


def installed_version() -> str:
    result = subprocess.run(
        ["flatpak", "list", "--user", "--app", "--columns=application,version"],
        check=True,
        capture_output=True,
        text=True,
    )
    rows = [row.split("\t") for row in result.stdout.splitlines()]
    matches = [row[1] for row in rows if len(row) == 2 and row[0] == APP_ID]
    if len(matches) != 1:
        raise RuntimeError(
            f"Expected one user Flatpak installation of {APP_ID}; found {len(matches)}"
        )
    version_tuple(matches[0])
    return matches[0]


def release_asset(release: dict, arch: str) -> tuple[str, str, str, int]:
    tag = release["tag_name"]
    version_tuple(tag)
    name = f"PrismLauncher-{tag}-{arch}.flatpak"
    matches = [asset for asset in release["assets"] if asset["name"] == name]
    if len(matches) != 1:
        raise RuntimeError(
            f"Release {tag} has {len(matches)} matching {arch} Flatpak bundles"
        )
    asset = matches[0]
    url = f"https://github.com/{REPOSITORY}/releases/download/{tag}/{name}"
    if asset["browser_download_url"] != url:
        raise RuntimeError("Release asset URL is outside the expected fork")
    digest = asset.get("digest", "")
    if not re.fullmatch(r"sha256:[0-9a-f]{64}", digest):
        raise RuntimeError("Release asset lacks a SHA-256 digest")
    size = asset.get("size")
    if not isinstance(size, int) or not 0 < size <= 500 * 1024 * 1024:
        raise RuntimeError("Release asset size is absent or outside the Flatpak limit")
    return tag, url, digest.removeprefix("sha256:"), size


def fetch_release() -> dict:
    request = urllib.request.Request(
        API_URL,
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "prism-fork-update",
        },
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def install_update(url: str, digest: str, expected_size: int) -> None:
    cache = (
        Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache"))
        / "prism-fork-update"
    )
    cache.mkdir(parents=True, exist_ok=True)
    bundle = cache / "release.flatpak"
    partial = cache / "release.flatpak.part"
    hasher = hashlib.sha256()
    size = 0
    installed = False
    try:
        with (
            urllib.request.urlopen(url, timeout=60) as response,
            partial.open("wb") as output,
        ):
            while chunk := response.read(1024 * 1024):
                size += len(chunk)
                if size > expected_size:
                    raise RuntimeError(
                        "Downloaded Flatpak exceeds the release asset size"
                    )
                hasher.update(chunk)
                output.write(chunk)
        if size != expected_size:
            raise RuntimeError(
                "Downloaded Flatpak size does not match the GitHub release"
            )
        if hasher.hexdigest() != digest:
            raise RuntimeError(
                "Downloaded Flatpak SHA-256 does not match the GitHub release"
            )
        partial.replace(bundle)
        subprocess.run(
            [
                "flatpak",
                "install",
                "--user",
                "--noninteractive",
                "--reinstall",
                "--bundle",
                str(bundle),
            ],
            check=True,
        )
        installed = True
    finally:
        partial.unlink(missing_ok=True)
        if installed:
            bundle.unlink(missing_ok=True)


def enable_timer() -> None:
    if not shutil.which("systemctl"):
        raise RuntimeError("systemd user timers are unavailable")
    data_home = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share"))
    config_home = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    script = data_home / "prism-fork-update" / "prism_fork_update.py"
    unit_dir = config_home / "systemd/user"
    if any(char in str(script) for char in (" ", "%", '"', "\n")):
        raise RuntimeError(f"Unsupported systemd unit path: {script}")
    unit_dir.mkdir(parents=True, exist_ok=True)
    script.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(Path(__file__), script)
    script.chmod(0o755)
    (unit_dir / "prism-fork-update.service").write_text(
        "[Unit]\nDescription=Update Prism Launcher from SomeoneIsWorking releases\n"
        "[Service]\nType=oneshot\nExecStart=" + str(script) + " check --apply\n",
        encoding="utf-8",
    )
    (unit_dir / "prism-fork-update.timer").write_text(
        "[Unit]\nDescription=Check Prism Launcher fork releases daily\n"
        "[Timer]\nOnStartupSec=10m\nOnCalendar=daily\nPersistent=true\nUnit=prism-fork-update.service\n"
        "[Install]\nWantedBy=timers.target\n",
        encoding="utf-8",
    )
    subprocess.run(["systemctl", "--user", "daemon-reload"], check=True)
    subprocess.run(
        ["systemctl", "--user", "enable", "--now", "prism-fork-update.timer"],
        check=True,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("check", "enable-timer"))
    parser.add_argument(
        "--apply", action="store_true", help="Install an available update"
    )
    args = parser.parse_args()
    if args.command == "enable-timer":
        installed_version()
        enable_timer()
        print("Daily Prism Launcher fork update timer enabled")
        return 0
    current = installed_version()
    arch = ARCHES.get(platform.machine())
    if arch is None:
        raise RuntimeError(f"No Flatpak release for architecture {platform.machine()}")
    tag, url, digest, size = release_asset(fetch_release(), arch)
    if version_tuple(tag) <= version_tuple(current):
        print(f"Prism Launcher {current} is current (fork release {tag})")
    elif args.apply:
        install_update(url, digest, size)
        if installed_version() != tag:
            raise RuntimeError(
                f"Flatpak install returned success but version is not {tag}"
            )
        print(f"Updated Prism Launcher to {tag}")
    else:
        print(f"Prism Launcher {tag} is available from {REPOSITORY}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (
        OSError,
        KeyError,
        ValueError,
        RuntimeError,
        subprocess.CalledProcessError,
    ) as error:
        print(f"Prism fork update failed: {error}", file=sys.stderr)
        sys.exit(1)
