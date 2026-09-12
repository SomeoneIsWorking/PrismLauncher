"""Validate the one-time fork Flatpak binding input."""

from __future__ import annotations

import hashlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from setup_fork_flatpak import verify_release_bundle


class SetupTests(unittest.TestCase):
    def test_release_digest_and_source(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary) / "PrismLauncher-12.0.7-x86_64.flatpak"
            bundle.write_bytes(b"bundle")
            asset = {
                "name": bundle.name,
                "browser_download_url": f"https://github.com/SomeoneIsWorking/PrismLauncher/releases/download/12.0.7/{bundle.name}",
                "size": 6,
                "digest": "sha256:" + hashlib.sha256(b"bundle").hexdigest(),
            }
            response = {"assets": [asset]}
            with patch(
                "urllib.request.urlopen",
                side_effect=lambda *args, **kwargs: io.BytesIO(
                    json.dumps(response).encode()
                ),
            ):
                verify_release_bundle(bundle, "12.0.7", "x86_64")
                asset["digest"] = "sha256:" + "0" * 64
                with self.assertRaisesRegex(RuntimeError, "SHA-256 differs"):
                    verify_release_bundle(bundle, "12.0.7", "x86_64")
                asset["browser_download_url"] = (
                    "https://github.com/other/release.flatpak"
                )
                with self.assertRaisesRegex(RuntimeError, "does not belong"):
                    verify_release_bundle(bundle, "12.0.7", "x86_64")


if __name__ == "__main__":
    unittest.main()
