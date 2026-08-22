# Pingo3D generators and regressions

This directory owns generators for Pingo3D regression vectors and generated
assembly test sources.

Generators must be deterministic, write only beneath this `3d/` project, and
record their output format in the corresponding test-source comments.

- `generate_math_vectors.py` creates deterministic binary oracle records for
  the implemented scalar primitives.
- `generate_sincos_table.py` creates the interleaved 256-entry signed-Q15
  source table, interpolation sentinel, fine atan nodes, and coarse inverse
  tables, and verifies the canonical table's packed SHA-256.
- `generate_transform_vectors.py` creates deterministic oracle records for
  sine/cosine, translation, rotations, and matrix-vector multiplication.
- `generate_pose_vectors.py` creates 224-byte oracle records for inverse
  trigonometry, matrix composition, canonical Euler conversion, intrinsic
  matrix/object rotations, local object translation, persistent-velocity
  integration, object-state initialization, and Pingo create/dirty-command
  packet construction, plus camera-state initialization and stateful
  fine-angle aim-at-point/aim-at-object solving under both roll policies. Its
  directed object-step records include long held-control trajectories across
  every axis and the Q15 cardinal saturation boundaries.
- `generate_fsim_vectors.py` creates grouped 64-byte `FSV1` records for all
  eleven optional-FSIM entry points. It exhausts the signed-16 scalar inputs
  and all rate/demand/qualified-moment combinations, then supplies directed
  boundaries and at least 10,000 seeded cases for each matrix, point, horizon,
  and command-51–53 builder operation. `test_generate_fsim_vectors.py` owns six
  focused oracle/format checks.
- `generate_pose_cycle.py` creates optional 2–256-record sampled orientation
  tables. Each record pairs fine Pingo wire Euler words with the matching Q15
  local-to-world matrix; `test_generate_pose_cycle.py` owns its deterministic
  numerical and provenance regressions.
- `build_api_candidate.py` creates the deterministic, manifest-bearing
  Pingo3D community API candidate ZIP. Its payload contains the core/table and
  both optional companions, `3d_fsim.inc` and `3d_pose_cycle.inc`, without
  merging either into the core. `test_build_api_candidate.py` checks the exact
  contents, hashes, status marker, required-input failures, and
  reproducibility.
- `test_camera_smoothness.py` checks deterministic fly-by continuity, angle
  and step error, scale invariance, render-sampling holds, the signed seam,
  elevated tracking, and independent near-pole yaw normalization.
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
- `run_fsim_regression.py` regenerates the complete FSIM oracle, assembles the
  real eZ80 `3d_fsim.asm` consumer, and runs its correctness-only `P3F1` suite
  through the maintained fail-closed combined-VDP profile. It checks the
  profile marker and required runtime paths, input immutability, exact result
  format/counts, and vector/binary hashes before and after execution; it
  deliberately reports no optimization timing. Runtime binary identity is not
  pinned by this source-only runner.
- `run_all_regressions.py` runs the pose-generator and candidate-builder
  checks, the seven focused FSIM host-oracle checks, the smoothness check, and
  all four emulator-backed suites in sequence. The FSIM host checks remain
  separately callable through `test_generate_fsim_vectors.py`.
