#!/usr/bin/env python3
"""Copy the Prism Flatpak's launcher data into a native Prism data directory."""

from __future__ import annotations

import argparse
import os
import shutil
import sys
from pathlib import Path

APP_ID = "org.prismlauncher.PrismLauncher"


def migrate(source: Path, target: Path) -> None:
    source = source.resolve()
    target = target.resolve()
    if not source.is_dir() or not (source / "prismlauncher.cfg").is_file():
        raise RuntimeError(f"Prism Flatpak data is absent: {source}")
    if source == target or source in target.parents or target in source.parents:
        raise RuntimeError("Source and native destination must be separate directories")
    if target.exists():
        raise RuntimeError(
            f"Native data already exists; refusing to merge or overwrite: {target}"
        )
    staging = target.with_name(target.name + ".migration")
    if staging.exists():
        raise RuntimeError(f"Previous migration staging directory exists: {staging}")
    target.parent.mkdir(parents=True, exist_ok=True)
    try:
        shutil.copytree(source, staging, symlinks=True)
        old = str(source).encode()
        new = str(target).encode()
        for config in staging.rglob("*.cfg"):
            if config.is_symlink():
                continue
            content = config.read_bytes()
            if old in content:
                config.write_bytes(content.replace(old, new))
        staging.rename(target)
    except Exception:
        if staging.exists():
            shutil.rmtree(staging)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--apply", action="store_true", help="Copy the data; default is a dry run"
    )
    parser.add_argument("--source", type=Path, help="Flatpak launcher data directory")
    parser.add_argument("--target", type=Path, help="Native launcher data directory")
    args = parser.parse_args()
    source = args.source or Path.home() / ".var/app" / APP_ID / "data/PrismLauncher"
    target = (
        args.target
        or Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share"))
        / "PrismLauncher"
    )
    if not source.is_dir() or not (source / "prismlauncher.cfg").is_file():
        raise RuntimeError(f"Prism Flatpak data is absent: {source}")
    if target.exists():
        raise RuntimeError(
            f"Native data already exists; refusing to merge or overwrite: {target}"
        )
    print(f"Flatpak data: {source}\nNative data: {target}")
    if args.apply:
        migrate(source, target)
        print("Migration copied; Flatpak data was preserved")
    else:
        print("Dry run; pass --apply to copy")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError) as error:
        print(f"Prism data migration failed: {error}", file=sys.stderr)
        sys.exit(1)
