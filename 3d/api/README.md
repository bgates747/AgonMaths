# Agon Pingo3D API candidate

Status: **candidate, not approved for public distribution**.

This directory is the entry point for the vendorable Pingo3D eZ80 API. The
candidate builder packages the canonical implementation, its generated
trigonometric table, the optional flight-simulator and sampled-pose
companions, the pose-table generator, the normative specification,
provenance, and a checksum manifest. The remaining publication gates are
deliberately specific and live in `docs/TODO.md` in the candidate archive
(`3d/docs/TODO.md` in AgonMaths).

## Platform and scope

The assembly targets the Agon Light family in eZ80 ADL mode and uses ez80asm
syntax. Most scalar, trigonometric, vector, matrix, object-state, and
camera-state routines are renderer-independent integer code. Names beginning
with `p3d_pingo_`, object packet builders, and `p3d_object_sync16` implement the
current Pingo buffered-command contract; stock VDP firmware does not provide
that protocol.

The complete routine, numeric-domain, register, failure, and shadow-register
contracts are in `docs/3d.md` (`3d/docs/3d.md` in AgonMaths). The comments
immediately above each assembly label are the final calling-convention
reference.

## Include order

Vendor the `include/` directory without editing its files, then include the
core and generated table in this order:

```asm
    include "include/3d.inc"
    include "include/3d_sincos_table.inc"
```

For sampled periodic animation, add the optional helper and an
application-generated table:

```asm
    include "include/3d_pose_cycle.inc"
    include "my-pose-cycle.inc"
```

For the curated flight-simulator helpers and experimental packet builders, add
the independent optional companion after the same core/table pair:

```asm
    include "include/3d_fsim.inc"
```

Do not append either companion to the core automatically. Existing
applications include optional helpers separately, and an unconditional copy
would create duplicate labels. `3d_fsim.inc` and `3d_pose_cycle.inc` do not
depend on one another.

## Sampled pose cycles

`p3d_object_apply_pose_sample8` installs one of at most 256 records into an
existing `p3d` object state. Each 24-byte record deliberately carries two
representations:

| Bytes | Value |
|---:|---|
| 0..5 | Fine absolute Pingo Euler X/Y/Z, 32,767 units per turn |
| 6..23 | Matching row-major signed-Q15 local-to-world matrix |

Keeping both representations avoids runtime inverse trigonometry and does not
conflate Pingo's 32,767-unit wire-angle domain with the library's 32,768-unit
internal binary turn. Generate an application table with:

```sh
python tools/generate_pose_cycle.py my-pose-cycle.inc \
  --symbol my_pose --base-euler 0 0 0 --axis y --samples 256
```

The caller owns phase, rate, direction, pause, and wrap policy.

Run the packaged generator checks with:

```sh
python tools/test_generate_pose_cycle.py
```

## Flight-simulator companion

`3d_fsim.inc` contains the reusable, caller-owned subset of the qualified F2
flight work:

- signed-16 F2 angular-rate damping, moment application, and ±512 clamping;
- signed-Q8 drag extraction and ±8192 velocity clamping;
- distinct-buffer row-major Q15 matrix transpose;
- local-point-to-world composition through a retained `p3d_object` pose;
- coarse matrix-derived pitch and roll for a branch-free fallback horizon;
- caller-owned packet builders for Pingo object-active command 51, signed-24
  translation command 52, and projection-far command 53.

The companion allocates no flight state and sends no VDU bytes. Its builders
return the packet address and exact length for transport chosen by the caller.
The local-point-to-world helper uses `IX` as writable point storage; that vec3
must not overlap the retained object state supplied in `IY`.

Commands 51–53 remain experimental and are subject to `P3D-API-2`; command 52
uses the same raw translation-count unit as legacy command 17, and command 53
accepts 2..65535 while the experimental firmware rejects 0 and 1 without
changing the prior setting.

The complete `flight-input.inc` model, cockpit/chase view policy, fallback
backdrop renderer, terrain/LOD system, command-50 P3DM replacement loader, and
command-49 flat-pattern clients are genuine later work, not duplicates of the
early demos. They remain explicitly deferred because their current forms own
application globals, static packets, rendering policy, or a larger subsystem
rather than a standalone ABI.

## Width contracts

Logical public values are normally 16-bit register pairs, but that convention
does not prohibit intentional native ADL-width arithmetic. A documented
24-bit value may use all of `UHL:H:L`; a packed 32-bit value may use a
three-byte `ld (foo),hl` plus `ld (foo+3),a`, or retain its high byte in `A`
while propagating carry. The source marks such local contracts explicitly.
They must not be mechanically rewritten as 16-bit operations.

## Current transport boundary

`p3d_object_build_next_dirty16` builds caller-owned Pingo packets and
`p3d_object_sync16` sends them through `RST.LIL $18`. No camera packet builder
or `p3d_camera_sync16` exists in this candidate. Camera-state consumers
currently own their transport; `P3D-API-1` tracks the decision to either keep
that boundary or add and regress an application-neutral camera API. The
flight companion does not duplicate the application-local camera-sync
routines found during the roundup.

## Evidence already attached to this source

The current core has recorded zero-error emulator runs of 60,596 scalar,
71,834 transform, and 251,337 pose/camera/packet records. The latter gives at
least 10,000 deterministic cases per implemented public arithmetic, pose,
camera-state, and packet-builder operation. `p3d_object_sync16` is explicitly
outside that binary harness because it crosses the live, unacknowledged VDU
stream.

The sampled-pose generator has seven focused host tests. Its originating
Pingo applications passed the full 147-test application suite, all 18 binary
builds, deterministic regeneration, emulator visual review, and physical Agon
review. The loader itself does not yet have an isolated eZ80 ABI/canary
regression; that exact gap is `P3D-REG-1`.

The flight companion preserves qualified donor arithmetic bodies and packet
layouts, while adapting global pointers, static packet storage, and results to
caller-owned ABI boundaries. Its isolated real-eZ80 suite passes all 912,001
records with zero value, carry, active/index-register, shadow-register,
source/canary, or builder-return errors. That qualifies packet construction
only: physical flight-model qualification, command delivery, and promotion of
experimental commands 51–53 are not implied by inclusion in this candidate.
Redistribution treatment for the donor is `P3D-LIC-3`, and the protocol/public
API decision is `P3D-API-2`.

The emulator/profile-dependent FSIM qualification kit remains in the
AgonMaths source checkout under `3d/scripts/` and `3d/src/asm/tests/`; it is not
part of this consumer ZIP. Its generator imports the established pose oracle,
and its runner depends on local `ez80asm`, MOS, Fab, and combined-VDP profile
plumbing. Exact source paths, payload hashes, and results are retained in
`docs/provenance.md`.

## Build the candidate archive

From the AgonMaths root:

```sh
.venv/bin/python 3d/scripts/build_api_candidate.py \
  --output 3d/tgt/agon-pingo3d-api-candidate.zip
```

The ZIP is deterministic and contains `MANIFEST.json` with the byte size and
SHA-256 of every payload. It remains a local candidate until all publication
gates are closed and a public source revision is tagged.
