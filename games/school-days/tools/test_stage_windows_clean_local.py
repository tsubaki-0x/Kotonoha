"""Focused safety/reproducibility tests for the local-only Windows staging tool."""

from __future__ import annotations

import hashlib
import tempfile
from pathlib import Path
import unittest
import zipfile

import stage_windows_clean_local as pack


class WindowsLocalPackagingTests(unittest.TestCase):
    def test_forbidden_retail_gpk(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            (folder / "Script.GPK").write_bytes(b"synthetic marker; not retail")
            with self.assertRaisesRegex(ValueError, "Forbidden file"):
                pack.guard_payload(folder)

    def test_forbidden_personal_save_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            (folder / "Save").mkdir()
            (folder / "Save" / "slot.txt").write_text("fixture", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "Forbidden directory"):
                pack.guard_payload(folder)

    def test_secret_marker_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            (folder / "notes.txt").write_text(
                "-----BEGIN PRIVATE KEY-----\nfixture not a real key", encoding="ascii"
            )
            with self.assertRaisesRegex(ValueError, "Potential secret"):
                pack.guard_payload(folder)

    def test_unapproved_font_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            font = folder / "NotoSansJP-Regular.ttf"
            ofl = folder / "OFL.txt"
            font.write_bytes(b"faked font; not distributable")
            ofl.write_text("faked license", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "Unapproved font"):
                pack.validate_font(font, ofl)

    def test_duplicate_destination_not_overwritten(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            source = folder / "source"
            source.write_bytes(b"synthetic")
            stage = pack.Stage(folder / "staged")
            stage.destination.mkdir()
            stage.copy(source, "ok.txt", "fixture")
            with self.assertRaises(FileExistsError):
                stage.copy(source, "ok.txt", "fixture")
            self.assertEqual((stage.destination / "ok.txt").read_bytes(), b"synthetic")

    def test_deterministic_zip(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            stage = folder / "stage"
            (stage / "assets").mkdir(parents=True)
            (stage / "assets" / "demo.txt").write_bytes(b"synthetic")
            (stage / "A.txt").write_bytes(b"test")
            first = folder / "first.zip"
            second = folder / "second.zip"
            hash1 = pack.deterministic_zip(stage, first)
            hash2 = pack.deterministic_zip(stage, second)
            self.assertEqual(hash1, hash2)
            self.assertEqual(hash1, hashlib.sha256(first.read_bytes()).hexdigest().upper())
            with zipfile.ZipFile(first) as archive:
                self.assertEqual(archive.namelist(), ["A.txt", "assets/demo.txt"])
                self.assertTrue(all(item.date_time == (2020, 1, 1, 0, 0, 0)
                                    for item in archive.infolist()))


if __name__ == "__main__":
    unittest.main()
