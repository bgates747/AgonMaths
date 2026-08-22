#!/usr/bin/env python3
"""Generate, assemble, and run the real-eZ80 Pingo3D F2 regression."""

from __future__ import annotations

import argparse
import hashlib
import os
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path


PROJECT = Path(__file__).resolve().parent.parent
REPOSITORY = PROJECT.parent
PYTHON = REPOSITORY / ".venv/bin/python"
GENERATOR = PROJECT / "scripts/generate_fsim_vectors.py"
ASSEMBLY = PROJECT / "src/asm/tests/3d_fsim.asm"
TARGET = PROJECT / "tgt"

# This is the maintained, fail-closed combined Pingo/Wolf profile.  The older
# ignored AgonMaths profile predates the combined module and is not an identity
# authority for a new regression.
PROFILE = Path("/home/smith/Agon/mystuff/pingoasm/emulator")
PROFILE_MARKER = PROFILE / ".bespoke-vdp-profile"
FAB_LINK = PROFILE / "fab-agon-emulator"
VDP_LINK = PROFILE / "vdp_combined.so"
MOS_LINK = PROFILE / "mos_console8.bin"
MOS_MAP_LINK = PROFILE / "mos_console8.map"

VECTOR_MAGIC = b"FSV1"
RESULT_MAGIC = b"P3F1"
RECORD_SIZE = 64
RESULT_SIZE = 69
OP_NAMES = (
    "damp_rate",
    "clamp_rate",
    "apply_axis_moment",
    "drag_q8",
    "clamp_velocity",
    "transpose",
    "local_point_world",
    "horizon",
    "build_active_51",
    "build_translation_52",
    "build_projection_53",
)
REASON_NAMES = (
    (0x01, "value"),
    (0x02, "carry"),
    (0x04, "active/index ABI"),
    (0x08, "shadow ABI"),
    (0x10, "source/canary"),
    (0x20, "builder return"),
)


def run(command: list[str], *, cwd: Path | None = None) -> None:
    print("+", " ".join(command))
    subprocess.run(command, check=True, cwd=cwd)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def u24(payload: bytes, offset: int) -> int:
    return int.from_bytes(payload[offset:offset + 3], "little")


def validate_vector(path: Path) -> tuple[int, tuple[int, ...]]:
    payload = path.read_bytes()
    if len(payload) < 8 or payload[:4] != VECTOR_MAGIC:
        raise RuntimeError("bad fsim vector signature")
    records = struct.unpack_from("<I", payload, 4)[0]
    if records > 0xFFFFFF:
        raise RuntimeError(
            f"vector record count {records} exceeds target u24 capacity"
        )
    expected_size = 8 + records * RECORD_SIZE
    if len(payload) != expected_size:
        raise RuntimeError(
            f"vector length {len(payload)} != expected {expected_size}"
        )
    counts = [0] * len(OP_NAMES)
    previous_opcode = 0
    for offset in range(8, len(payload), RECORD_SIZE):
        opcode = payload[offset]
        if not 0 <= opcode < len(counts):
            raise RuntimeError(f"bad opcode {opcode} at vector offset {offset}")
        if opcode < previous_opcode:
            raise RuntimeError("fsim vectors are not grouped by opcode")
        previous_opcode = opcode
        counts[opcode] += 1
    return records, tuple(counts)


def resolve_runtime() -> tuple[Path, Path, Path, Path]:
    if not PROFILE_MARKER.is_file():
        raise RuntimeError(f"missing bespoke-profile marker: {PROFILE_MARKER}")
    marker = PROFILE_MARKER.read_text(encoding="ascii").strip()
    if marker != VDP_LINK.name:
        raise RuntimeError(
            f"profile marker names {marker!r}, expected {VDP_LINK.name!r}"
        )
    fab = FAB_LINK.resolve(strict=True)
    vdp = VDP_LINK.resolve(strict=True)
    mos = MOS_LINK.resolve(strict=True)
    mos_map = MOS_MAP_LINK.resolve(strict=True)
    for path in (fab, vdp, mos, mos_map):
        if not path.is_file():
            raise RuntimeError(f"runtime input is not a file: {path}")
    return fab, vdp, mos, mos_map


def tail(path: Path, limit: int = 8000) -> str:
    if not path.exists():
        return ""
    payload = path.read_bytes()[-limit:]
    return payload.decode("utf-8", errors="replace")


def run_emulator(
    *,
    fab: Path,
    vdp: Path,
    mos: Path,
    result: Path,
    expected_records: int,
    timeout: float,
) -> tuple[bytes, float]:
    with tempfile.TemporaryDirectory(prefix="agonmaths-fsim-") as temporary:
        root = Path(temporary)
        sdcard = root / "sdcard"
        working = root / "cwd"
        mapped_parent = sdcard / "mystuff/AgonMaths/3d"
        mapped_parent.mkdir(parents=True)
        working.mkdir()
        (mapped_parent / "tgt").symlink_to(
            TARGET.resolve(strict=True), target_is_directory=True,
        )
        (sdcard / "autoexec.txt").write_bytes(
            b"SET KEYBOARD 1\r\n"
            b"cd /mystuff/AgonMaths/3d/tgt\r\n"
            b"load 3d_fsim.bin\r\n"
            b"run\r\n"
        )
        stdout_path = root / "emulator.stdout"
        stderr_path = root / "emulator.stderr"

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
            str(fab),
            "--renderer", "sw",
            "--firmware", "console8",
            "--mos", str(mos),
            "--vdp", str(vdp),
            "--sdcard", str(sdcard),
            "--zero",
        ]
        print("+", " ".join(command))
        started = time.monotonic()
        with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
            process = subprocess.Popen(
                command,
                cwd=working,
                env=environment,
                stdout=stdout,
                stderr=stderr,
            )
            try:
                deadline = started + timeout
                payload = b""
                while time.monotonic() < deadline:
                    if process.poll() is not None:
                        raise RuntimeError(
                            f"emulator exited with {process.returncode}\n"
                            f"stdout tail:\n{tail(stdout_path)}\n"
                            f"stderr tail:\n{tail(stderr_path)}"
                        )
                    if result.exists():
                        payload = result.read_bytes()
                        if (
                            len(payload) == RESULT_SIZE
                            and payload[:4] == RESULT_MAGIC
                            and u24(payload, 4) == expected_records
                        ):
                            break
                    time.sleep(0.05)
                else:
                    raise RuntimeError(
                        f"timed out after {timeout:.1f}s waiting for result\n"
                        f"stdout tail:\n{tail(stdout_path)}\n"
                        f"stderr tail:\n{tail(stderr_path)}"
                    )
            finally:
                process.terminate()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        return payload, time.monotonic() - started


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--random-cases",
        type=int,
        default=10_000,
        help="diagnostic structural-case override; publication uses 10000",
    )
    parser.add_argument(
        "--word-stride",
        type=int,
        default=1,
        help="debug-only F2 input stride; publication evidence requires 1",
    )
    parser.add_argument("--timeout", type=float, default=600.0)
    args = parser.parse_args()

    TARGET.mkdir(parents=True, exist_ok=True)
    vector = TARGET / "3d_fsim.vec"
    binary = TARGET / "3d_fsim.bin"
    result = TARGET / "3d_fsim.result"
    if result.exists():
        result.unlink()

    run([
        str(PYTHON), str(GENERATOR),
        "--output", str(vector),
        "--random-cases", str(args.random_cases),
        "--word-stride", str(args.word_stride),
    ])
    expected_records, counts = validate_vector(vector)
    run(
        ["ez80asm", ASSEMBLY.name, str(binary.resolve())],
        cwd=ASSEMBLY.parent,
    )
    fab, vdp, mos, mos_map = resolve_runtime()
    vector_hash_before = sha256(vector)
    binary_hash_before = sha256(binary)
    payload, elapsed = run_emulator(
        fab=fab,
        vdp=vdp,
        mos=mos,
        result=result,
        expected_records=expected_records,
        timeout=args.timeout,
    )
    if sha256(vector) != vector_hash_before:
        raise RuntimeError("vector changed during emulator execution")
    if sha256(binary) != binary_hash_before:
        raise RuntimeError("binary changed during emulator execution")

    if len(payload) != RESULT_SIZE or payload[:4] != RESULT_MAGIC:
        raise RuntimeError("bad fsim result format")
    records = u24(payload, 4)
    errors = u24(payload, 7)
    errors_by_op = tuple(u24(payload, 10 + index * 3)
                         for index in range(len(OP_NAMES)))
    if records != expected_records:
        raise RuntimeError(f"result records {records} != expected {expected_records}")

    print(f"Runtime profile: {PROFILE}")
    print(f"  Fab: {fab}")
    print(f"  VDP: {vdp}")
    print(f"  MOS: {mos}")
    print(f"  map: {mos_map}")
    print(
        f"Vector: {vector.stat().st_size:,} bytes, SHA-256 {vector_hash_before}"
    )
    print(
        f"Binary: {binary.stat().st_size:,} bytes, SHA-256 {binary_hash_before}"
    )
    print(f"Emulator wall time: {elapsed:.3f}s")
    if errors:
        details = ", ".join(
            f"{name}={count}"
            for name, count in zip(OP_NAMES, errors_by_op)
            if count
        )
        first_record = u24(payload, 43)
        first_opcode = payload[46]
        reason = payload[47]
        actual_carry = payload[48]
        reasons = ", ".join(
            name for bit, name in REASON_NAMES if reason & bit
        ) or f"unknown 0x{reason:02x}"
        print(f"FAIL: {errors} errors in {records} records ({details})")
        print(
            f"First failure: record={first_record}, opcode={first_opcode}, "
            f"reason={reasons}, carry={actual_carry}, "
            f"output={payload[49:69].hex()}"
        )
        return 1

    if any(errors_by_op):
        raise RuntimeError("zero total errors but nonzero per-op error count")
    print(f"PASS: {records:,} Pingo3D F2 records")
    print("Per-op records:")
    for name, count in zip(OP_NAMES, counts):
        print(f"  {name:24s} {count:9,d}")
    print(f"Result: {len(payload)} bytes, SHA-256 {sha256(result)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
