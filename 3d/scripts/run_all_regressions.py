#!/usr/bin/env python3
"""Run all host and emulator-backed Pingo3D regression suites."""

from __future__ import annotations

import subprocess
from pathlib import Path


PROJECT = Path(__file__).resolve().parent.parent
PYTHON = PROJECT.parent / ".venv/bin/python"


def main() -> None:
    for runner in (
        "test_generate_pose_cycle.py",
        "test_generate_fsim_vectors.py",
        "test_build_api_candidate.py",
        "test_camera_smoothness.py",
        "run_fsim_regression.py",
        "run_math_regression.py",
        "run_transform_regression.py",
        "run_pose_regression.py",
    ):
        subprocess.run(
            [str(PYTHON), str(PROJECT / "scripts" / runner)],
            check=True,
        )


if __name__ == "__main__":
    main()
