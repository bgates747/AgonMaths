#!/usr/bin/env python3
"""Generate, assemble, and run the object-pose regression suite in Fab."""

from __future__ import annotations

import argparse
import os
import struct
import subprocess
import sys
import time
from pathlib import Path

from generate_pose_vectors import (
    OP_COUNT,
    OP_NAMES,
    RECORD_SIZE,
    TAIL_OFFSET,
)


PROJECT = Path(__file__).resolve().parent.parent
REPOSITORY = PROJECT.parent
PYTHON = REPOSITORY / ".venv/bin/python"
GENERATOR = PROJECT / "scripts/generate_pose_vectors.py"
ASSEMBLY = PROJECT / "src/asm/tests/3d_pose.asm"
TARGET = PROJECT / "tgt"
PROFILE = PROJECT / "emulator/pingoasm"
FAB = PROFILE / "fab-agon-emulator"
VDP = PROFILE / "vdp_pingo.so"
MOS = PROFILE / "mos_console8.bin"
SDCARD = PROFILE / "sdcard"
RESULT_SIZE = 189
RESULT_MAGIC = b"P3P1"


def run(command: list[str], *, cwd: Path | None = None) -> None:
    print("+", " ".join(command))
    subprocess.run(command, check=True, cwd=cwd)


def validate_vectors(payload: bytes) -> tuple[int, list[int]]:
    if len(payload) < 4:
        raise RuntimeError("pose vector file is missing its count")
    records = struct.unpack_from("<I", payload)[0]
    expected_size = 4 + records * RECORD_SIZE
    if len(payload) != expected_size:
        raise RuntimeError(
            f"vector length {len(payload)} != expected {expected_size}"
        )
    counts = [0] * OP_COUNT
    previous_opcode = 0
    expected_tail = bytes(
        (0x5A ^ index) & 0xFF for index in range(RECORD_SIZE - TAIL_OFFSET)
    )
    for index, offset in enumerate(range(4, len(payload), RECORD_SIZE)):
        opcode = payload[offset]
        carry = payload[offset + 1]
        mode = payload[offset + 2]
        if not 0 <= opcode < OP_COUNT:
            raise RuntimeError(f"bad opcode {opcode} in record {index}")
        if opcode < previous_opcode:
            raise RuntimeError("pose vectors are not grouped by opcode")
        if carry not in (0, 1):
            raise RuntimeError(f"bad carry {carry} in record {index}")
        if mode not in (0, 1):
            raise RuntimeError(f"bad alias mode {mode} in record {index}")
        if mode and opcode != 0:
            raise RuntimeError(f"alias mode on unsupported opcode {opcode}")
        if payload[offset + 3] != 0xA5:
            raise RuntimeError(f"bad header canary in record {index}")
        if payload[
            offset + TAIL_OFFSET:offset + RECORD_SIZE
        ] != expected_tail:
            raise RuntimeError(f"bad tail canary in record {index}")
        previous_opcode = opcode
        counts[opcode] += 1
    return records, counts


def emulator_environment() -> dict[str, str]:
    environment = os.environ.copy()
    environment["SDL_VIDEODRIVER"] = "dummy"
    environment["SDL_AUDIODRIVER"] = "dummy"
    local_lib = str(Path.home() / ".local/lib")
    environment["LD_LIBRARY_PATH"] = (
        local_lib
        + (
            ":" + environment["LD_LIBRARY_PATH"]
            if environment.get("LD_LIBRARY_PATH")
            else ""
        )
    )
    return environment


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--random-cases", type=int, default=10_000)
    parser.add_argument("--timeout", type=float, default=600)
    parser.add_argument(
        "--only-op", type=int, choices=range(OP_COUNT),
        help="run only one opcode (diagnostic use)",
    )
    args = parser.parse_args()

    TARGET.mkdir(parents=True, exist_ok=True)
    vector = TARGET / "3d_pose.vec"
    binary = TARGET / "3d_pose.bin"
    result = TARGET / "3d_pose.result"
    if result.exists():
        result.unlink()

    generator_command = [
        str(PYTHON),
        str(GENERATOR),
        "--random-cases",
        str(args.random_cases),
        "--output",
        str(vector),
    ]
    if args.only_op is not None:
        generator_command.extend(["--only-op", str(args.only_op)])
    run(generator_command)
    vector_payload = vector.read_bytes()
    expected_records, counts_by_op = validate_vectors(vector_payload)
    if args.only_op is None and args.random_cases >= 10_000 and any(
        count < 10_000 for count in counts_by_op
    ):
        raise RuntimeError(
            f"every public operation needs 10,000 cases: {counts_by_op}"
        )

    run(
        ["ez80asm", ASSEMBLY.name, str(binary.resolve())],
        cwd=ASSEMBLY.parent,
    )

    autoexec = SDCARD / "autoexec.txt"
    autoexec.write_bytes(
        b"SET KEYBOARD 1\r\n"
        b"cd /mystuff/AgonMaths/3d/tgt\r\n"
        b"load 3d_pose.bin\r\n"
        b"run\r\n"
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
        env=emulator_environment(),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        deadline = time.monotonic() + args.timeout
        payload = b""
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError(f"emulator exited with {process.returncode}")
            if result.exists():
                payload = result.read_bytes()
                if (
                    len(payload) == RESULT_SIZE
                    and payload[:4] == RESULT_MAGIC
                    and int.from_bytes(payload[4:7], "little")
                    == expected_records
                ):
                    break
            time.sleep(0.05)
        else:
            raise RuntimeError("timed out waiting for pose regression result")
    finally:
        process.terminate()
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()

    magic = payload[:4]
    records = int.from_bytes(payload[4:7], "little")
    errors = int.from_bytes(payload[7:10], "little")
    if magic != RESULT_MAGIC:
        raise RuntimeError(f"bad result signature: {magic!r}")
    by_op = struct.unpack_from(f"<{OP_COUNT}H", payload, 10)
    if errors:
        details = ", ".join(
            f"{name}={count}"
            for name, count in zip(OP_NAMES, by_op)
            if count
        )
        print(f"FAIL: {errors} errors in {records} records ({details})")
        diagnostic_offset = 10 + OP_COUNT * 2 + OP_COUNT * 3
        first_index = int.from_bytes(
            payload[diagnostic_offset:diagnostic_offset + 3], "little",
        )
        first_opcode = payload[diagnostic_offset + 3]
        first_carry = payload[diagnostic_offset + 4]
        actual_primary = payload[
            diagnostic_offset + 5:diagnostic_offset + 5 + 64
        ]
        actual_output = payload[
            diagnostic_offset + 5 + 64:diagnostic_offset + 5 + 64 + 20
        ]
        record_offset = 4 + first_index * RECORD_SIZE
        record = vector_payload[
            record_offset:record_offset + RECORD_SIZE
        ]
        expected_primary = record[132:196]
        expected_output = record[196:216]
        primary_diffs = [
            (index, expected, actual_primary[index])
            for index, expected in enumerate(expected_primary)
            if actual_primary[index] != expected
        ]
        output_diffs = [
            (index, expected, actual_output[index])
            for index, expected in enumerate(expected_output)
            if actual_output[index] != expected
        ]
        print(
            "first failure:"
            f" record={first_index},"
            f" op={first_opcode}"
            f" ({OP_NAMES[first_opcode] if first_opcode < OP_COUNT else 'bad'}),"
            f" carry expected={record[1]} actual={first_carry}"
        )
        if primary_diffs:
            print(
                "  primary byte diffs:",
                ", ".join(
                    f"+{index} {expected:02x}->{actual:02x}"
                    for index, expected, actual in primary_diffs[:24]
                ),
            )
        if output_diffs:
            print(
                "  output byte diffs:",
                ", ".join(
                    f"+{index} {expected:02x}->{actual:02x}"
                    for index, expected, actual in output_diffs
                ),
            )
        return 1

    print(f"PASS: {records} pose records")
    timing_offset = 10 + OP_COUNT * 2
    milliseconds = tuple(
        int.from_bytes(payload[offset:offset + 3], "little")
        for offset in range(
            timing_offset, timing_offset + OP_COUNT * 3, 3,
        )
    )
    print("PRT timing (mixed regression workload; file I/O excluded):")
    for name, count, elapsed in zip(
        OP_NAMES, counts_by_op, milliseconds,
    ):
        if elapsed:
            per_frame = count * 1000 / (elapsed * 60)
            print(
                f"  {name:22s} {count:5d} records / {elapsed:5d} ms"
                f" = {per_frame:8.1f} records per 60 Hz frame"
            )
        else:
            print(f"  {name:22s} below 1 ms timer resolution")
    return 0


if __name__ == "__main__":
    sys.exit(main())
