#!/usr/bin/env python3
"""Focused host checks for the Pingo3D F2 vector oracle."""

from __future__ import annotations

import hashlib
import struct
import tempfile
import unittest
from pathlib import Path

import generate_fsim_vectors as fsim
import run_fsim_regression as runner


class FsimVectorGeneratorTests(unittest.TestCase):
    def test_f2_boundary_oracles(self) -> None:
        self.assertEqual(fsim.damp_rate(0), 0)
        self.assertEqual(fsim.damp_rate(1), 0)
        self.assertEqual(fsim.damp_rate(-1), 0)
        self.assertEqual(fsim.damp_rate(16), 15)
        self.assertEqual(fsim.damp_rate(-16), -15)
        self.assertEqual(fsim.damp_rate(-32768), -30720)
        self.assertEqual(fsim.clamp_rate(-32768), -512)
        self.assertEqual(fsim.clamp_rate(511), 511)
        self.assertEqual(fsim.clamp_rate(513), 512)
        self.assertEqual(fsim.drag_from_velocity_q8(-1), 1)
        self.assertEqual(fsim.drag_from_velocity_q8(-256), 1)
        self.assertEqual(fsim.drag_from_velocity_q8(255), 0)
        self.assertEqual(fsim.drag_from_velocity_q8(256), -1)
        self.assertEqual(fsim.clamp_velocity_q8(-8193), -8192)
        self.assertEqual(fsim.clamp_velocity_q8(8193), 8192)

    def test_apply_axis_covers_complete_qualified_domain(self) -> None:
        counts = [0] * fsim.OP_COUNT
        for record in fsim.f2_records():
            self.assertEqual(len(record), fsim.RECORD_SIZE)
            counts[record[0]] += 1
        self.assertEqual(counts[fsim.OP_DAMP_RATE], 65536)
        self.assertEqual(counts[fsim.OP_CLAMP_RATE], 65536)
        self.assertEqual(
            counts[fsim.OP_APPLY_AXIS_MOMENT],
            65536 * len(fsim.F2_MOMENTS) * len(fsim.DEMANDS),
        )
        self.assertEqual(counts[fsim.OP_DRAG_Q8], 65536)
        self.assertEqual(counts[fsim.OP_CLAMP_VELOCITY], 65536)

        self.assertEqual(fsim.apply_axis_moment(512, 1, 24), 504)
        self.assertEqual(fsim.apply_axis_moment(-512, -1, 24), -504)
        self.assertEqual(fsim.apply_axis_moment(32767, 1, 24), 512)
        self.assertEqual(fsim.apply_axis_moment(-32768, -1, 24), -512)
        self.assertEqual(fsim.apply_axis_moment(1, 0, 8), 0)

    def test_local_point_vectors_name_both_failure_stages(self) -> None:
        records = list(fsim.local_point_records(12))
        tags = {record[3] for record in records if record[1]}
        self.assertIn(fsim.CASE_MATRIX_FAILURE, tags)
        self.assertIn(fsim.CASE_TRANSLATION_FAILURE, tags)
        for record in records:
            self.assertEqual(record[2], 1)

        matrix_failure = next(
            record for record in records
            if record[3] == fsim.CASE_MATRIX_FAILURE
        )
        self.assertEqual(
            matrix_failure[fsim.EXPECTED_OFFSET:fsim.EXPECTED_OFFSET + 6],
            matrix_failure[28:34],
        )

        translation_failure = next(
            record for record in records
            if record[3] == fsim.CASE_TRANSLATION_FAILURE
        )
        self.assertEqual(
            struct.unpack_from("<hhh", translation_failure, fsim.EXPECTED_OFFSET),
            (1, 0, 0),
        )

    def test_horizon_saturates_negative_32768_before_asin(self) -> None:
        record = next(
            record for record in fsim.horizon_records(0)
            if record[3] == fsim.CASE_NEGATIVE_32768
        )
        pitch, roll = struct.unpack_from("<hh", record, fsim.EXPECTED_OFFSET)
        self.assertEqual(pitch, 8192)
        self.assertEqual(roll, 0)
        self.assertEqual(record[1:3], b"\x00\x01")

    def test_packet_oracles_are_exact_fixed_width(self) -> None:
        self.assertEqual(
            fsim.packet_active(0x1234, 0xABCD, 1),
            bytes.fromhex("17 00 a0 34 12 49 33 cd ab 01"),
        )
        xyz = bytes.fromhex("00 00 80 ff ff 7f 56 34 12")
        self.assertEqual(
            fsim.packet_translation(0x1234, 0xABCD, xyz),
            bytes.fromhex("17 00 a0 34 12 49 34 cd ab") + xyz,
        )
        self.assertEqual(
            fsim.packet_projection(0x1234, 0xFFFF),
            bytes.fromhex("17 00 a0 34 12 49 35 ff ff"),
        )

        active_records = list(
            fsim.packet_records(fsim.OP_BUILD_ACTIVE_51, 256)
        )
        self.assertEqual(len(active_records), 265)
        self.assertTrue(all(record[8] in (0, 1) for record in active_records))

    def test_structural_vectors_are_deterministic_and_grouped(self) -> None:
        def digest() -> bytes:
            records = []
            records.extend(fsim.transpose_records(16))
            records.extend(fsim.local_point_records(16))
            records.extend(fsim.horizon_records(16))
            for opcode in (
                fsim.OP_BUILD_ACTIVE_51,
                fsim.OP_BUILD_TRANSLATION_52,
                fsim.OP_BUILD_PROJECTION_53,
            ):
                records.extend(fsim.packet_records(opcode, 16))
            opcodes = [record[0] for record in records]
            self.assertEqual(opcodes, sorted(opcodes))
            self.assertTrue(all(
                len(record) == fsim.RECORD_SIZE for record in records
            ))
            self.assertTrue(all(record[60:64] == b"\xD1\xE2\xF3\x84"
                                for record in records))
            return hashlib.sha256(b"".join(records)).digest()

        self.assertEqual(digest(), digest())

    def test_runner_rejects_counts_above_target_u24_capacity(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            vector = Path(temporary_directory) / "too-large.vec"
            vector.write_bytes(fsim.HEADER.pack(fsim.MAGIC, 0x1000000))
            with self.assertRaisesRegex(RuntimeError, "u24 capacity"):
                runner.validate_vector(vector)


if __name__ == "__main__":
    unittest.main()
