#!/usr/bin/env python3
"""Bind an installed fork Flatpak bundle to its local update repository."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import urllib.request
from pathlib import Path

APP_ID = "org.prismlauncher.PrismLauncher"
FORK = "SomeoneIsWorking/PrismLauncher"
REMOTE = "prism-fork-local"


def command(*arguments: str) -> str:
    return subprocess.run(
        arguments, check=True, capture_output=True, text=True
    ).stdout.strip()


def verify_release_bundle(bundle: Path, version: str, architecture: str) -> None:
    expected_name = f"PrismLauncher-{version}-{architecture}.flatpak"
    if bundle.name != expected_name:
        raise RuntimeError(f"Expected the installed version's bundle: {expected_name}")
    request = urllib.request.Request(
        f"https://api.github.com/repos/{FORK}/releases/tags/{version}",
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "prism-fork-setup",
        },
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        release = json.load(response)
    matches = [asset for asset in release["assets"] if asset["name"] == expected_name]
    if len(matches) != 1:
        raise RuntimeError(
            f"Fork release {version} has no unique {architecture} Flatpak bundle"
        )
    asset = matches[0]
    expected_url = (
        f"https://github.com/{FORK}/releases/download/{version}/{expected_name}"
    )
    if asset["browser_download_url"] != expected_url:
        raise RuntimeError("Release bundle URL does not belong to the fork")
    digest = asset.get("digest", "")
    if not digest.startswith("sha256:") or len(digest) != 71:
        raise RuntimeError("Fork release bundle has no SHA-256 digest")
    if bundle.stat().st_size != asset["size"]:
        raise RuntimeError("Bundle size differs from the fork release")
    hasher = hashlib.sha256()
    with bundle.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            hasher.update(chunk)
    if hasher.hexdigest() != digest.removeprefix("sha256:"):
        raise RuntimeError("Bundle SHA-256 differs from the fork release")


def installed_ref() -> tuple[str, str]:
    rows = command(
        "flatpak", "list", "--user", "--app", "--columns=application,version,arch"
    ).splitlines()
    matches = [row.split("\t") for row in rows if row.startswith(APP_ID + "\t")]
    if len(matches) != 1 or len(matches[0]) != 3:
        raise RuntimeError(f"Expected one user installation of {APP_ID}")
    return matches[0][1], matches[0][2]


def unmask() -> None:
    if APP_ID in [
        line.strip() for line in command("flatpak", "mask", "--user").splitlines()
    ]:
        command("flatpak", "mask", "--user", "--remove", APP_ID)


def import_bundle(repository: Path, bundle: Path) -> None:
    """Import a verified bundle, then publish the repository's appstream branch.

    ``build-import-bundle`` alone leaves the repository without appstream
    metadata, so every later ``flatpak update`` without an explicit ref reports
    ``No such ref 'appstream2/x86_64' in remote``. ``build-update-repo`` is the
    canonical step that generates it.
    """
    command("flatpak", "build-import-bundle", str(repository), str(bundle))
    command("flatpak", "build-update-repo", str(repository))


def setup(bundle: Path, repository: Path) -> None:
    url = repository.as_uri()
    remotes = command("flatpak", "remotes", "--user", "--columns=name,url").splitlines()
    existing = [line for line in remotes if line.startswith(REMOTE + "\t")]
    if existing and existing != [f"{REMOTE}\t{url}"]:
        raise RuntimeError(f"Existing {REMOTE} remote points elsewhere")
    if command("flatpak", "info", "--user", "--show-origin", APP_ID) == REMOTE:
        if not existing:
            raise RuntimeError(
                f"Installed Flatpak names {REMOTE}, but its remote is absent"
            )
        unmask()
        return
    repository.mkdir(parents=True, exist_ok=True)
    if not (repository / "config").exists():
        command("ostree", "init", f"--repo={repository}", "--mode=archive-z2")
    import_bundle(repository, bundle)
    if not existing:
        command("flatpak", "remote-add", "--user", "--no-gpg-verify", REMOTE, url)

    # Flatpak keeps ~/.var/app data unless --delete-data is explicitly requested.
    command("flatpak", "uninstall", "--user", "--noninteractive", APP_ID)
    try:
        command("flatpak", "install", "--user", "--noninteractive", REMOTE, APP_ID)
    except subprocess.CalledProcessError:
        command(
            "flatpak", "install", "--user", "--noninteractive", "--bundle", str(bundle)
        )
        raise RuntimeError(
            "Fork repository installation failed; restored the original bundle"
        ) from None
    if command("flatpak", "info", "--user", "--show-origin", APP_ID) != REMOTE:
        raise RuntimeError("Flatpak origin is not the fork repository after setup")
    unmask()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "bundle", type=Path, help="Bundle for the currently installed fork release"
    )
    parser.add_argument(
        "--apply", action="store_true", help="Bind this Flatpak to the fork repository"
    )
    args = parser.parse_args()
    bundle = args.bundle.resolve(strict=True)
    version, architecture = installed_ref()
    verify_release_bundle(bundle, version, architecture)
    repository = Path.home() / ".var/app" / APP_ID / "data/prism-fork-repo"
    print(f"Verified fork release {version}; repository: {repository}")
    if args.apply:
        setup(bundle, repository)
        print("Flatpak now follows the local fork repository through Prism's updater")
    else:
        print("Dry run; pass --apply to configure")
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
        print(f"Fork Flatpak setup failed: {error}", file=sys.stderr)
        sys.exit(1)
