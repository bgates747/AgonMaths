#!/usr/bin/env python3
"""Generate, assemble, and run the scalar regression suite in Fab."""

from __future__ import annotations

import os
import struct
import subprocess
import sys
import time
from pathlib import Path


PROJECT = Path(__file__).resolve().parent.parent
REPOSITORY = PROJECT.parent
PYTHON = REPOSITORY / ".venv/bin/python"
GENERATOR = PROJECT / "scripts/generate_math_vectors.py"
ASSEMBLY = PROJECT / "src/asm/tests/3d_math.asm"
TARGET = PROJECT / "tgt"
PROFILE = PROJECT / "emulator/pingoasm"
FAB = PROFILE / "fab-agon-emulator"
VDP = PROFILE / "vdp_pingo.so"
MOS = PROFILE / "mos_console8.bin"
SDCARD = PROFILE / "sdcard"


def run(command: list[str], *, cwd: Path | None = None) -> None:
    print("+", " ".join(command))
    subprocess.run(command, check=True, cwd=cwd)


def main() -> int:
    TARGET.mkdir(parents=True, exist_ok=True)
    vector = TARGET / "3d_math.vec"
    binary = TARGET / "3d_math.bin"
    result = TARGET / "3d_math.result"
    if result.exists():
        result.unlink()

    run([str(PYTHON), str(GENERATOR), "--output", str(vector)])
    vector_payload = vector.read_bytes()
    expected_records = struct.unpack_from("<I", vector_payload)[0]
    expected_size = 4 + expected_records * 16
    if len(vector_payload) != expected_size:
        raise RuntimeError(
            f"vector length {len(vector_payload)} != expected {expected_size}"
        )
    counts_by_op = [0] * 6
    previous_opcode = 0
    for offset in range(4, len(vector_payload), 16):
        opcode = vector_payload[offset]
        if not 0 <= opcode < len(counts_by_op):
            raise RuntimeError(f"bad opcode {opcode} at vector offset {offset}")
        if opcode < previous_opcode:
            raise RuntimeError("vectors are not grouped by opcode")
        previous_opcode = opcode
        counts_by_op[opcode] += 1
    run(
        ["ez80asm", ASSEMBLY.name, str(binary.resolve())],
        cwd=ASSEMBLY.parent,
    )

    autoexec = SDCARD / "autoexec.txt"
    autoexec.write_bytes(
        b"SET KEYBOARD 1\r\n"
        b"cd /mystuff/AgonMaths/3d/tgt\r\n"
        b"load 3d_math.bin\r\n"
        b"run\r\n"
    )

    environment = os.environ.copy()
    environment["SDL_VIDEODRIVER"] = "dummy"
    environment["SDL_AUDIODRIVER"] = "dummy"
    local_lib = str(Path.home() / ".local/lib")
    environment["LD_LIBRARY_PATH"] = (
        local_lib
        + (":" + environment["LD_LIBRARY_PATH"]
           if environment.get("LD_LIBRARY_PATH") else "")
    )

    command = [
        str(FAB.resolve()),
        "--renderer", "sw",
        "--firmware", "console8",
        "--mos", str(MOS.resolve()),
        "--vdp", str(VDP.resolve()),
        "--sdcard", str(SDCARD.resolve()),
        "--zero",
    ]
    print("+", " ".join(command))
    process = subprocess.Popen(
        command,
        cwd=PROFILE,
        env=environment,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        deadline = time.monotonic() + 15
        payload = b""
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError(f"emulator exited with {process.returncode}")
            if result.exists():
                payload = result.read_bytes()
                if (
                    len(payload) == 38
                    and payload[:4] == b"P3D1"
                    and struct.unpack_from("<H", payload, 4)[0]
                    == expected_records
                ):
                    break
            time.sleep(0.05)
        else:
            raise RuntimeError("timed out waiting for emulator result")
    finally:
        process.terminate()
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()

    magic, records, errors = struct.unpack_from("<4sHH", payload)
    if magic != b"P3D1":
        raise RuntimeError(f"bad result signature: {magic!r}")
    if errors:
        by_op = struct.unpack_from("<6H", payload, 8)
        names = ("umul", "smul", "sadd", "ssub", "sdiv32767", "smul_norm")
        details = ", ".join(
            f"{name}={count}" for name, count in zip(names, by_op) if count
        )
        print(f"FAIL: {errors} errors in {records} records ({details})")
        return 1
    print(f"PASS: {records} scalar arithmetic records")
    milliseconds = tuple(
        int.from_bytes(payload[offset:offset + 3], "little")
        for offset in range(20, 38, 3)
    )
    names = ("umul", "smul", "sadd", "ssub", "sdiv32767", "smul_norm")
    print("PRT timing (validation loop included):")
    for name, count, elapsed in zip(names, counts_by_op, milliseconds):
        if elapsed:
            per_frame = count * 1000 / (elapsed * 60)
            print(
                f"  {name:11s} {count:4d} ops / {elapsed:3d} ms"
                f" = {per_frame:8.1f} ops per 60 Hz frame"
            )
        else:
            print(f"  {name:11s} below 1 ms timer resolution")
    return 0


if __name__ == "__main__":
    sys.exit(main())
