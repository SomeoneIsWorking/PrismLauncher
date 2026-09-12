"""Focused checks for fork release selection and Flatpak data migration."""

from __future__ import annotations

import hashlib
import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from migrate_flatpak_data import migrate
from prism_fork_update import install_update, release_asset, version_tuple


class ReleaseTests(unittest.TestCase):
    def test_release_asset_requires_expected_fork_and_digest(self) -> None:
        digest = hashlib.sha256(b"bundle").hexdigest()
        asset = {
            "name": "PrismLauncher-12.0.8-x86_64.flatpak",
            "browser_download_url": "https://github.com/SomeoneIsWorking/PrismLauncher/releases/download/12.0.8/PrismLauncher-12.0.8-x86_64.flatpak",
            "digest": f"sha256:{digest}",
            "size": 6,
        }
        release = {"tag_name": "12.0.8", "assets": [asset]}
        self.assertEqual(release_asset(release, "x86_64")[2:], (digest, 6))
        self.assertGreater(version_tuple("12.0.8"), version_tuple("12.0.7"))
        asset["browser_download_url"] = (
            "https://github.com/other/PrismLauncher/releases/download/12.0.8/PrismLauncher-12.0.8-x86_64.flatpak"
        )
        with self.assertRaisesRegex(RuntimeError, "outside the expected fork"):
            release_asset(release, "x86_64")

    def test_missing_asset_or_digest_refuses(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "0 matching"):
            release_asset({"tag_name": "12.0.8", "assets": []}, "x86_64")
        with self.assertRaises(ValueError):
            version_tuple("12.0.8-rc1")

    def test_corrupt_download_never_invokes_flatpak(self) -> None:
        with (
            tempfile.TemporaryDirectory() as temporary,
            patch.dict("os.environ", {"XDG_CACHE_HOME": temporary}),
            patch("urllib.request.urlopen", return_value=io.BytesIO(b"bad")),
            patch("subprocess.run") as flatpak,
        ):
            with self.assertRaisesRegex(RuntimeError, "SHA-256"):
                install_update("https://example.invalid/bundle", "0" * 64, 3)
            flatpak.assert_not_called()


class MigrationTests(unittest.TestCase):
    def test_copy_rewrites_launcher_paths_and_preserves_source(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "flatpak" / "PrismLauncher"
            target = root / "native" / "PrismLauncher"
            instance = source / "instances" / "pack"
            instance.mkdir(parents=True)
            (source / "prismlauncher.cfg").write_text(
                f"JavaPath={source}/java/bin/java\n"
            )
            (instance / "instance.cfg").write_text(f"JavaPath={source}/java/bin/java\n")
            (instance / "world.dat").write_bytes(b"world")
            migrate(source, target)
            self.assertEqual(
                (target / "instances/pack/world.dat").read_bytes(), b"world"
            )
            self.assertIn(str(target), (target / "prismlauncher.cfg").read_text())
            self.assertIn(
                str(target), (target / "instances/pack/instance.cfg").read_text()
            )
            self.assertIn(str(source), (source / "prismlauncher.cfg").read_text())
            with self.assertRaisesRegex(RuntimeError, "refusing to merge"):
                migrate(source, target)
            with self.assertRaisesRegex(RuntimeError, "separate directories"):
                migrate(source, source / "nested")


if __name__ == "__main__":
    unittest.main()
