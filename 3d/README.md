# Pingo3D

Self-contained eZ80-local integer arithmetic, transforms, and retained state
for Pingo-compatible 16-bit values. The physical `3d/` path is retained for
compatibility, but this project and its distributable API are named Pingo3D.

## Layout

```text
api/                Community candidate entry point
docs/               Design, numeric formats, and register contracts
scripts/            Authoritative regression-vector/code generators
src/asm/agon/       3D source and copied Agon API includes
src/asm/tests/      Generated assembly regression programs
tests/              Generated binary regression vectors
tgt/                Assembled binaries and other disposable build output
```

The implementation and generated trigonometric table are:

```text
src/asm/agon/3d.inc
src/asm/agon/3d_sincos_table.inc
```

The optional sampled-pose companion is:

```text
src/asm/agon/3d_pose_cycle.inc
```

The optional flight-simulator companion is:

```text
src/asm/agon/3d_fsim.inc
```

The design note is:

```text
docs/3d.md
```

The implementation and application-qualification milestone is recorded in:

```text
docs/devlog-2026-07-30.md
```

## Copied API includes

The shared Agon platform-support `.inc` files beside the Pingo3D sources are a
project-local snapshot copied from:

```text
../src/asm/agon/
```

They are duplicated deliberately so the Pingo3D project and its regression
program can assemble without reaching outside this subtree. `3d.inc` and
`docs/3d.md` remain authoritative here; `3d_sincos_table.inc` is generated
locally; and `3d_pose_cycle.inc` and `3d_fsim.inc` are the curated optional
companions described above rather than copied platform dependencies.

Refresh the shared API snapshot only as an explicit maintenance action:

```bash
cp ../src/asm/agon/*.inc src/asm/agon/
```

Review the resulting diff because a shared API refresh can change instruction
behavior or test dependencies.

## Current state

The library implements scalar arithmetic, coarse lookup and fine interpolated
forward/inverse trigonometry, vector transforms, row-major matrix/vector and matrix/matrix
composition, canonical Pingo Euler import/export, and persistent eZ80-side
object pose integration. A separate 36-byte camera state supports stateful
aim-at-point and aim-at-object solving with explicit upright or continuous
over-pole roll policy, pole-cone hysteresis, and a matching retained
fine-angle orientation matrix.

`3d_fsim.inc` curates the application-neutral part of the later
`pingo_terrain` flight work: F2 rate/drag/clamp kernels, matrix transpose,
local-point-to-world and matrix-horizon helpers, and caller-owned packet
builders for experimental Pingo subcommands 51, 52, and 53. It does not import
the single-instance flight model, input/view policy, backdrop drawing, terrain
streaming, static packet storage, or VDU sender.

`p3d_object_size` is a packed 64-byte state. Named `equ` offsets expose its
dirty flags, Pingo control/object/mesh/texture-bitmap IDs, absolute position,
absolute Pingo Euler cache, scale, local-to-world orientation, local linear and
angular velocities, and sub-bin angular residuals. Pingo remains write-only:
the eZ80 owns the state and `p3d_object_sync16` emits only dirty absolute
create/scale/rotation/translation commands.

The established emulator-backed suites pass 60,596 scalar records and 71,834
transform records. The matrix/object/camera pose suite contains 251,337
records, with at least 10,000 deterministic cases per public arithmetic, pose,
camera, and packet-builder operation. The transform runner follows correctness
with 20,000 successful, in-memory calls per entry point and reports
loop-baseline-subtracted 60 Hz budgets. A separate deterministic smoothness
test exercises camera fly-bys, scaled trajectories, render sampling, the
half-turn seam, and near-pole yaw precision. Long held-control pose records
cross every Q15 cardinal in both directions and verify that matrix-only
one-count saturation cannot stall a simulation tick.

The optional FSIM companion has its own correctness-only eZ80 suite. It passes
912,001 records with exhaustive signed-16 F2 inputs and at least 10,000
deterministic cases for each structural helper and command-51–53 packet
builder. This qualifies the packaged routines and their documented ABI; it
does not promote the experimental commands or qualify VDP delivery.

Consumers must include the implementation and its generated table, in that
order. Applications may then include either optional companion exactly once;
sampled-pose users also include their own generated pose data:

```asm
    include "3d.inc"
    include "3d_sincos_table.inc"
    include "3d_fsim.inc"            ; optional
    include "3d_pose_cycle.inc"       ; optional
    include "my-generated-poses.inc" ; optional application data
```

The two companions are independent. The command 51–53 builders in
`3d_fsim.inc` only construct caller-owned packets and remain experimental;
they never invoke `RST.LIL $18`.

The community-facing Pingo3D candidate-package entry point is
[`api/README.md`](api/README.md). It is deliberately marked as a candidate
until the specific gates in [`docs/TODO.md`](docs/TODO.md) are closed.

## Local development environment

The project-root Python 3.14 environment is `.venv` at the parent AgonMaths
root. The canonical
`/home/smith/Agon/mystuff/agon-utils` checkout is installed into it in editable
mode. From the AgonMaths root, invoke Python explicitly:

```bash
.venv/bin/python
```

The ignored `emulator/pingoasm` profile uses the owned Fab emulator and the
current mutable Pingo VDP module from
`/home/smith/Agon/mystuff/agon-vdp`. Its emulated SD card maps this project's
`tgt` directory at `/mystuff/AgonMaths/3d/tgt`. Launch it with:

```bash
/home/smith/Agon/mystuff/agon-dev-env/scripts/run_emulator.sh \
  /home/smith/Agon/mystuff/AgonMaths/3d/emulator/pingoasm
```

Each regression runner manages its emulator integration and runs headlessly;
the new FSIM runner additionally requires the maintained combined-VDP profile
and fails closed on its identity. Run these from the AgonMaths root; the
combined command regenerates all four fixtures before assembly:

```bash
.venv/bin/python 3d/scripts/run_math_regression.py
.venv/bin/python 3d/scripts/run_transform_regression.py
.venv/bin/python 3d/scripts/run_pose_regression.py
.venv/bin/python 3d/scripts/run_fsim_regression.py
.venv/bin/python 3d/scripts/run_all_regressions.py
```
