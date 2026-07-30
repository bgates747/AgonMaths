#!/usr/bin/env python3
"""Run all emulator-backed 3D regression suites."""

from __future__ import annotations

import subprocess
from pathlib import Path


PROJECT = Path(__file__).resolve().parent.parent
PYTHON = PROJECT.parent / ".venv/bin/python"


def main() -> None:
    for runner in (
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
