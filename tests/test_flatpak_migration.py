"""Focused checks for Flatpak data migration."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from migrate_flatpak_data import migrate


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
