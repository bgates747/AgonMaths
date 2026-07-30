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
- `run_math_regression.py` generates the records, assembles the MOS runner,
  executes it in the project-local Pingo-compatible Fab profile, and checks
  the result file. It reports the original fixture-style PRT1 timing, including
  operand setup, validation, and loop bookkeeping.
- `run_transform_regression.py` performs the equivalent emulator-backed run
  for every transform API. It additionally reports a 20,000-call successful
  API budget after subtracting matched empty-loop PRT1 baselines.
- `run_all_regressions.py` runs both suites in sequence.
