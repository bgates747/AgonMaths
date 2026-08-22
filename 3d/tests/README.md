# Generated Pingo3D regression vectors

Binary input/expected-output fixtures belong here. They are generated artifacts
owned by scripts in `../scripts/`.

Fixture formats and record sizes must be documented by their generators and
assembly consumers.

The scalar, transform, and pose runners write their disposable vector files
directly beneath `../tgt` for immediate visibility through the emulator's live
SD mapping.

The optional-FSIM runner likewise writes `../tgt/3d_fsim.vec`. Its `FSV1`
header carries a 32-bit record count followed by grouped 64-byte records for
the five F2 scalar operations, transpose, local-point-to-world composition,
matrix horizon extraction, and command-51–53 packet construction. The full
publication-strength fixture contains 912,001 records; it remains disposable
target output rather than a checked-in binary asset.
