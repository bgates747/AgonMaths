# Generators

This directory owns generators for 3D regression vectors and generated
assembly test sources.

Generators must be deterministic, write only beneath this `3d/` project, and
record their output format in the corresponding test-source comments.

- `generate_math_vectors.py` creates deterministic binary oracle records for
  the implemented scalar primitives.
- `generate_sincos_table.py` creates the interleaved 256-entry signed-Q15
  source table and verifies its packed SHA-256.
- `generate_transform_vectors.py` creates deterministic oracle records for
  sine/cosine, translation, rotations, and matrix-vector multiplication.
- `generate_pose_vectors.py` creates 224-byte oracle records for inverse
  trigonometry, matrix composition, canonical Euler conversion, intrinsic
  matrix/object rotations, local object translation, persistent-velocity
  integration, object-state initialization, and Pingo create/dirty-command
  packet construction.
- `run_math_regression.py` generates the records, assembles the MOS runner,
  executes it in the project-local Pingo-compatible Fab profile, and checks
  the result file. It reports the original fixture-style PRT1 timing, including
  operand setup, validation, and loop bookkeeping.
- `run_transform_regression.py` performs the equivalent emulator-backed run
  for every transform API. It additionally reports a 20,000-call successful
  API budget after subtracting matched empty-loop PRT1 baselines.
- `run_pose_regression.py` runs the persistent pose suite, validates its
  grouped binary records, and reports per-operation PRT1 timing with file I/O
  paused.
- `run_all_regressions.py` runs all three suites in sequence.
