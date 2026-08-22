#!/usr/bin/env python3
"""Build the deterministic, pre-publication Agon Pingo3D API candidate."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import stat
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Sequence


BUNDLE_ROOT = "agon-pingo3d-api-candidate"
CANDIDATE_STATUS = "candidate-not-for-publication"
MANIFEST_FORMAT = "agon-pingo3d-api-candidate-manifest-v1"
FIXED_ZIP_TIMESTAMP = (1980, 1, 1, 0, 0, 0)
REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


@dataclass(frozen=True)
class PayloadSpec:
    source: str
    destination: str


REQUIRED_PAYLOADS = (
    PayloadSpec("LICENSE", "LICENSE"),
    PayloadSpec("3d/api/README.md", "README.md"),
    PayloadSpec("3d/src/asm/agon/3d.inc", "include/3d.inc"),
    PayloadSpec(
        "3d/src/asm/agon/3d_sincos_table.inc",
        "include/3d_sincos_table.inc",
    ),
    PayloadSpec(
        "3d/src/asm/agon/3d_fsim.inc",
        "include/3d_fsim.inc",
    ),
    PayloadSpec(
        "3d/src/asm/agon/3d_pose_cycle.inc",
        "include/3d_pose_cycle.inc",
    ),
    PayloadSpec(
        "3d/scripts/generate_pose_cycle.py",
        "tools/generate_pose_cycle.py",
    ),
    PayloadSpec(
        "3d/scripts/test_generate_pose_cycle.py",
        "tools/test_generate_pose_cycle.py",
    ),
    PayloadSpec("3d/docs/3d.md", "docs/3d.md"),
    PayloadSpec("3d/docs/TODO.md", "docs/TODO.md"),
    PayloadSpec("3d/docs/provenance.md", "docs/provenance.md"),
)


class CandidateBuildError(RuntimeError):
    """Raised when the candidate archive cannot be built safely."""


def _read_payloads(repository_root: Path) -> dict[str, bytes]:
    missing = [
        spec.source
        for spec in REQUIRED_PAYLOADS
        if not (repository_root / spec.source).is_file()
    ]
    if missing:
        details = "\n".join(f"  - {path}" for path in sorted(missing))
        raise CandidateBuildError(
            "missing required candidate-package inputs:\n" + details
        )

    payloads: dict[str, bytes] = {}
    for spec in REQUIRED_PAYLOADS:
        source = repository_root / spec.source
        try:
            payloads[spec.destination] = source.read_bytes()
        except OSError as error:
            raise CandidateBuildError(
                f"could not read candidate-package input {spec.source}: {error}"
            ) from error
    return payloads


def _manifest(payloads: Mapping[str, bytes]) -> dict[str, object]:
    return {
        "bundle_root": BUNDLE_ROOT,
        "format": MANIFEST_FORMAT,
        "payloads": {
            path: {
                "sha256": hashlib.sha256(content).hexdigest(),
                "size": len(content),
            }
            for path, content in sorted(payloads.items())
        },
        "status": CANDIDATE_STATUS,
    }


def _manifest_bytes(manifest: Mapping[str, object]) -> bytes:
    text = json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    return text.encode("utf-8")


def _zip_info(member_name: str) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(member_name, date_time=FIXED_ZIP_TIMESTAMP)
    info.compress_type = zipfile.ZIP_STORED
    info.create_system = 3
    info.external_attr = (stat.S_IFREG | 0o644) << 16
    info.extra = b""
    info.comment = b""
    return info


def _archive_bytes(payloads: Mapping[str, bytes], manifest: bytes) -> bytes:
    members = dict(payloads)
    members["MANIFEST.json"] = manifest

    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, mode="w", compression=zipfile.ZIP_STORED) as archive:
        archive.comment = b""
        for destination, content in sorted(members.items()):
            member_name = f"{BUNDLE_ROOT}/{destination}"
            archive.writestr(_zip_info(member_name), content)
    return buffer.getvalue()


def build_api_candidate(
    output: Path,
    *,
    repository_root: Path = REPOSITORY_ROOT,
) -> dict[str, object]:
    """Build *output* without changing any package source files."""

    root = Path(repository_root).resolve()
    destination = Path(output)
    destination_resolved = destination.resolve()
    input_paths = {
        (root / spec.source).resolve()
        for spec in REQUIRED_PAYLOADS
    }
    if destination_resolved in input_paths:
        raise CandidateBuildError(
            f"refusing to overwrite candidate-package input: {destination}"
        )

    payloads = _read_payloads(root)
    manifest = _manifest(payloads)
    archive = _archive_bytes(payloads, _manifest_bytes(manifest))

    try:
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(archive)
    except OSError as error:
        raise CandidateBuildError(
            f"could not write candidate archive {destination}: {error}"
        ) from error
    return manifest


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        required=True,
        type=Path,
        help="destination ZIP path",
    )
    args = parser.parse_args(argv)

    try:
        build_api_candidate(args.output)
    except CandidateBuildError as error:
        parser.exit(1, f"error: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
