#!/usr/bin/env python3
"""Focused tests for the deterministic Pingo3D API candidate builder."""

from __future__ import annotations

import hashlib
import json
import tempfile
import unittest
import zipfile
from pathlib import Path

import build_api_candidate as candidate


REQUIRED_SOURCES = {
    "LICENSE": b"candidate license\n",
    "3d/api/README.md": b"# Agon Pingo3D candidate\n",
    "3d/src/asm/agon/3d.inc": b"; canonical p3d routines\n",
    "3d/src/asm/agon/3d_sincos_table.inc": b"; canonical table\n",
    "3d/src/asm/agon/3d_fsim.inc": b"; flight-simulator companion\n",
    "3d/src/asm/agon/3d_pose_cycle.inc": b"; pose-cycle helper\n",
    "3d/scripts/generate_pose_cycle.py": b"#!/usr/bin/env python3\n",
    "3d/scripts/test_generate_pose_cycle.py": b"#!/usr/bin/env python3\n",
    "3d/docs/3d.md": b"# Pingo3D API\n",
    "3d/docs/TODO.md": b"# Candidate gates\n",
    "3d/docs/provenance.md": b"# Provenance\n",
}

SOURCE_TO_DESTINATION = {
    "LICENSE": "LICENSE",
    "3d/api/README.md": "README.md",
    "3d/src/asm/agon/3d.inc": "include/3d.inc",
    "3d/src/asm/agon/3d_sincos_table.inc": "include/3d_sincos_table.inc",
    "3d/src/asm/agon/3d_fsim.inc": "include/3d_fsim.inc",
    "3d/src/asm/agon/3d_pose_cycle.inc": "include/3d_pose_cycle.inc",
    "3d/scripts/generate_pose_cycle.py": "tools/generate_pose_cycle.py",
    "3d/scripts/test_generate_pose_cycle.py": "tools/test_generate_pose_cycle.py",
    "3d/docs/3d.md": "docs/3d.md",
    "3d/docs/TODO.md": "docs/TODO.md",
    "3d/docs/provenance.md": "docs/provenance.md",
}


def write_fixture(root: Path) -> None:
    for relative_path, content in REQUIRED_SOURCES.items():
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)


class CandidateBuilderTests(unittest.TestCase):
    def test_archive_members_and_manifest_cover_every_payload(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_fixture(root)
            output = root / "dist" / "candidate.zip"

            returned_manifest = candidate.build_api_candidate(
                output,
                repository_root=root,
            )

            expected_payloads = {
                SOURCE_TO_DESTINATION[source]: content
                for source, content in REQUIRED_SOURCES.items()
            }
            prefix = candidate.BUNDLE_ROOT + "/"
            expected_members = sorted(
                prefix + path
                for path in (*expected_payloads, "MANIFEST.json")
            )

            with zipfile.ZipFile(output) as archive:
                self.assertEqual(archive.namelist(), expected_members)
                manifest = json.loads(
                    archive.read(prefix + "MANIFEST.json").decode("utf-8")
                )
                self.assertEqual(manifest, returned_manifest)
                self.assertEqual(
                    manifest["status"],
                    "candidate-not-for-publication",
                )
                self.assertEqual(
                    manifest["format"],
                    "agon-pingo3d-api-candidate-manifest-v1",
                )
                self.assertEqual(
                    manifest["bundle_root"],
                    candidate.BUNDLE_ROOT,
                )
                self.assertEqual(
                    set(manifest["payloads"]),
                    set(expected_payloads),
                )

                for path, content in expected_payloads.items():
                    self.assertEqual(archive.read(prefix + path), content)
                    self.assertEqual(
                        manifest["payloads"][path],
                        {
                            "sha256": hashlib.sha256(content).hexdigest(),
                            "size": len(content),
                        },
                    )

                for info in archive.infolist():
                    self.assertEqual(
                        info.date_time,
                        candidate.FIXED_ZIP_TIMESTAMP,
                    )
                    self.assertEqual(info.compress_type, zipfile.ZIP_STORED)
                    self.assertEqual((info.external_attr >> 16) & 0o777, 0o644)

    def test_readme_and_optional_companions_are_required(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_fixture(root)
            (root / "3d/api/README.md").unlink()
            (root / "3d/src/asm/agon/3d_fsim.inc").unlink()
            (root / "3d/src/asm/agon/3d_pose_cycle.inc").unlink()
            output = root / "candidate.zip"

            with self.assertRaises(candidate.CandidateBuildError) as raised:
                candidate.build_api_candidate(output, repository_root=root)

            message = str(raised.exception)
            self.assertIn("3d/api/README.md", message)
            self.assertIn("3d/src/asm/agon/3d_fsim.inc", message)
            self.assertIn("3d/src/asm/agon/3d_pose_cycle.inc", message)
            self.assertFalse(output.exists())

    def test_missing_required_inputs_fail_before_output_is_written(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_fixture(root)
            (root / "3d/docs/provenance.md").unlink()
            (root / "3d/scripts/generate_pose_cycle.py").unlink()
            output = root / "candidate.zip"

            with self.assertRaises(candidate.CandidateBuildError) as raised:
                candidate.build_api_candidate(output, repository_root=root)

            message = str(raised.exception)
            self.assertIn("missing required candidate-package inputs", message)
            self.assertIn("3d/docs/provenance.md", message)
            self.assertIn("3d/scripts/generate_pose_cycle.py", message)
            self.assertFalse(output.exists())

    def test_repeated_builds_are_byte_for_byte_identical(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_fixture(root)
            first = root / "first.zip"
            second = root / "second.zip"

            candidate.build_api_candidate(first, repository_root=root)
            candidate.build_api_candidate(second, repository_root=root)

            self.assertEqual(first.read_bytes(), second.read_bytes())


if __name__ == "__main__":
    unittest.main()
