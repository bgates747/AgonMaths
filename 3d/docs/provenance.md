# Pingo3D source provenance

This document records the source lineage needed to review a community package.
It does not declare third-party material relicensed. The candidate remains
not-for-publication until `P3D-LIC-1`, `P3D-LIC-2`, and `P3D-LIC-3` in
[`TODO.md`](TODO.md) are resolved.

## Canonical implementation

The canonical core and generated trigonometric table are maintained in this
repository:

```text
3d/src/asm/agon/3d.inc
3d/src/asm/agon/3d_sincos_table.inc
```

The camera/fine-transform snapshot found in all current Pingo consumers came
from AgonMaths commit `138098bccaa23630de82d59f7cf607128d304c18`. Before this
roundup its hashes were:

| File | SHA-256 |
|---|---|
| `3d.inc` | `bd3899b757524bf3ecb2f15b193a62a696981188bdda693f0479ffc6c64ca97d` |
| `3d_sincos_table.inc` | `1924ccfb8d081830030cd9c810a23a10947722a6fb48e795136812e25beeb18d` |

The `3d.inc` hash changes in this roundup only because provenance and the
intentional ADL24 staging contract were added as comments; the instruction
stream is unchanged.

## Attributed arithmetic

### Unsigned 16-by-16 multiply

`p3d_umul16x16_32` follows `mul_16_32_calc84maniac` from:

```text
agon-utils/examples/matrix/src/asm/softfloat/f16_mul.inc
```

The donor entered agon-utils in commit
`26682638b83c432291919674a16b04c438ed142e` and its symbol credits
calc84maniac. Pingo3D makes the final low-word additions explicitly `.s`; the
instruction sequence is otherwise the same. The donor file does not state a
license or redistribution grant adjacent to this routine. `P3D-LIC-1` tracks
the required permission and attribution decision.

### Unsigned 24-bit integer square root

`p3d_lookat_isqrt24` is a namespaced adaptation of `sqrt24` in
`3d/src/asm/agon/fixed168.inc`. That source credits xeda112358 and cites:

```text
https://www.cemetech.net/forum/viewtopic.php?t=11178&postdays=0&postorder=asc&start=0
```

Pingo3D documents its narrower camera-solver bounds and genuine ADL 24-bit
ABI. The cited source does not establish a redistribution license in this
working tree. `P3D-LIC-2` tracks the required permission and attribution
decision.

## Sampled-pose companion

The optional `3d_pose_cycle.inc`, its deterministic generator, and focused
tests originate in pingoasm commit
`a6b0b3b320463cee336cc58fc9dda2ba11b7b3d0` (“Upgrade Earth Party with
drift-free pose cycles”). The helper's original path was
`apps/_common/pose-cycle.inc`; commit
`b79be8d326dccbb73700ac24f9ca94873402ff28` renamed it to
`tests/apps/_common/pose-cycle.inc` without changing its code.

The application-specific generated Earth tables and star/model assets were not
imported. Hardware acceptance of the generic loader/generator in that use is
recorded by pingoasm's `docs/devlog-2026-08-01.md`.

## Flight-simulator companion

The optional `3d_fsim.inc` is a curated, caller-owned API adaptation from
`pingo_terrain` commit
`58f0b61734f35965752833c9f74b34cdd55ed296` (“Milestone: working terrain
loader and visualizer prototype”). The fixed vendored companion has SHA-256
`72a9c488b5df41873253413292da9fb760716d0cac97d91db57f4868f64386a3`.
The final hash includes comment-only clarification of the qualified F2 input
and non-overlapping work/object pointer contracts. No instruction bytes were
changed for those clarifications, and no arithmetic was optimized during
extraction.

The relevant donor files at that exact commit are:

| Donor path | SHA-256 | Role in the roundup |
|---|---|---|
| `fixtures/berkeley-flight-fdm/src/flight-input.inc` | `c12c952f41f4dfc25d50549f09cbd299a98eb7eb7742085a70a1228bf8f826eb` | F2 kernels, matrix transpose, and complete deferred flight model |
| `fixtures/_common/aircraft-view.inc` | `b05ea34f01ee1cf91278716e24ece6b3c529f5ca30dbfbd1e70f24eb44c17a3c` | local-point-to-world composition and deferred cockpit/chase policy |
| `fixtures/koak-flight-streamed/src/flight-backdrop.inc` | `0d7952cc78daed69c390e087e3cee621129d856337d80b5d70139f34078f5899` | camera-matrix horizon contract and deferred regional-view drawing policy |
| `fixtures/berkeley-flight-fdm/src/flight-backdrop.inc` | `524ab3131b3ec1a983667353d0fd9168b4b0540a2549beb04ad28689a5271832` | byte-identical extraction body and earlier aircraft-matrix drawing policy |
| `fixtures/p3dm-loader-probe/src/p3dm-loader.inc` | `1de2eea715add03f9379548a7db823cfda9e6aef46dab1a00674782d87537d39` | static command-51 sender plus deferred command-50 loader |
| `fixtures/_common/pingo-wide-translation.inc` | `9491c080fcb56e54734ef21107ebf587aadcc9b58a7152547ec589efad91d3f3` | static command-52 sender |
| `fixtures/_common/pingo-projection.inc` | `f32206e4b48e72b9eb0ddfa0bfc3743766c09566b11ab52112693693eacdd3fc` | static command-53 sender |

The canonical command-51 donor is specifically lines 857–878 of
`p3dm-loader.inc`; that 22-line comment/body block has SHA-256
`df29b55865b798b202477e92d10ef28f7b3316138c1965efe7cd43f2c061075f`.
Only its packet layout is represented in the companion. The adjacent
command-50 mesh-replacement loader and the command-49 flat-pattern clients
remain outside this API.

The extraction deliberately distinguishes preserved leaf arithmetic from ABI
adaptation:

- `flight_damp_rate16`, `flight_clamp_rate16`,
  `flight_drag_from_velocity_de`, and `flight_clamp_velocity16` became the
  corresponding `p3d_fsim_f2_*` leaf routines with instruction bodies
  preserved exactly apart from namespace labels. The axis-moment wrapper
  retains the donor control flow while calling those namespaced leaves.
- `flight_build_orientation_transpose` retains the exact qualified byte
  mapping, but its fixed `object_state` and `flight_orientation_transpose`
  symbols became distinct caller pointers in `IY` and `IX`.
- `aircraft_view_build_world_point` became
  `p3d_fsim_local_point_to_world16`: the same qualified matrix-then-translation
  composition now operates on caller-owned work storage and a caller-selected
  object state. Its partial failure boundary is documented rather than
  presented as a transactional whole operation.
- The KOAK and Berkeley `backdrop_extract_attitude` bodies are identical.
  The KOAK caller expressly supplies `camera_state`'s matrix, establishing the
  generic matrix contract. `p3d_fsim_mat3_horizon16` retains the same
  `asin(-r1c2)` and `atan2(r1c0,r1c1)` arithmetic, including `-32768`
  saturation, while returning pitch and roll in registers instead of writing
  backdrop globals.
- The command-51, command-52, and command-53 clients retain their exact packet
  field widths and byte order, but fixed scene IDs, static packet fields, and
  immediate `RST.LIL $18` sends became caller inputs and
  caller-owned destination buffers. This source-faithful builder adaptation is
  safer for a standalone library and does not claim delivery acknowledgement.

Commands 51–53 remain experimental. Command 52 uses signed 24-bit
little-endian coordinates in the same raw translation-count unit as legacy
command 17. Command 53 accepts 2..65535; the donor records that 0 and 1 are
consumed but rejected while the prior setting remains active. `P3D-API-2`
tracks whether these builders receive a versioned public protocol contract and
promotion evidence or are excluded from release.

### Standalone companion qualification

The source checkout retains the deterministic qualification kit at:

```text
3d/scripts/generate_fsim_vectors.py
3d/scripts/test_generate_fsim_vectors.py
3d/scripts/run_fsim_regression.py
3d/src/asm/tests/3d_fsim.asm
```

The seven focused host-oracle tests pass. The full `FSV1` fixture contains
912,001 grouped 64-byte records: 65,536 each for damping, rate clamp, drag, and
velocity clamp; 589,824 for every signed input word crossed with demands
`-1/0/+1` and moments 8/12/24; and respectively 10,004, 10,006, 10,007,
10,009, 10,003, and 10,004 for transpose, local-point-to-world, matrix
horizon, command 51, command 52, and command 53.
Every command-51 record uses the documented active byte `0` or `1`.

The maintained combined-VDP real-eZ80 run passed all 912,001 records in 46.504
seconds with zero value, carry, active/index-register, shadow-register,
source/canary, or builder-return errors. Its reproducible artifacts are:

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `3d_fsim.vec` | 58,368,072 | `8ecb1ef624d8d94e502873f2ad41619f492470ca7f4453f64d3212c0ab1e5634` |
| `3d_fsim.bin` | 12,874 | `4e67bcb4521333a706e0f1b44fec68a041eaebdf37da6f7e96636f45a1a3c4b0` |
| `3d_fsim.result` | 69 | `1b3193fe38c4ea48747ada2e718e020e1cf80ff1029e7b2b3fd0065e99fdc774` |

Repeated vector generation and assembly were byte-identical. The 69-byte
`P3F1` result holds 24-bit total-record and total-error counts, eleven 24-bit
per-operation error counts, and first-failure metadata. This suite executes the
actual core, generated table, and packaged companion but never sends a command,
so it qualifies caller-owned packet construction rather than firmware
behavior.

The production-width audit reports zero wide-memory hazards or review items in
`3d_fsim.inc`. A joint include/harness audit classifies 104 intentional wide
accesses: 100 explicit `dl`/storage cases, three indirect cases, and one case whose
adjacent contract comment resolves the otherwise ambiguous classification;
again it reports zero hazards and zero outstanding review items.

The qualification kit is deliberately not copied into the community consumer
ZIP. The generator imports established helpers from
`generate_pose_vectors.py`, while the runner requires repository-local
`ez80asm`, MOS, Fab, and maintained combined-VDP profile plumbing. The archive
instead carries the qualified include and this evidence/provenance record.

### Explicitly deferred flight modules

The full 784-line `flight-input.inc` is genuinely new relative to the earlier
Pingo demo applications. It is not a duplicate and was not discarded. Its
present ABI directly names `object_state`, `object_ids`, `keyboard_masks`,
`obj_initial_x/y/z`, and `obj_ground_y`, allocates a 77-byte global block, and
is single-instance and nonreentrant. A full step is not globally
transactional: controls, rates, and object attitude may already have changed
when a later arithmetic failure sets its sticky flag. Extracting those pieces
into a caller-owned model would create new, unregressed code, so this roundup
preserves only the already separable kernels.

The exact donor ADL24 position integrator remains a reference in that file. It
preloads `UDE` for sign extension, then loads `D:E` bytewise and uses native
24-bit loads/adds/stores on explicit `dl` position fields, with position
addition wrapping modulo `2^24`. It was omitted only because it is coupled to
the global state layout; its native-width arithmetic is intentional and was
not rewritten.

One ambiguity is recorded for any future full-model extraction. The target
coordinated-yaw block compares only the airspeed high byte and therefore
activates at signed-Q8 3072, while the host model and prose use the exact
takeoff threshold 3200. That inline block is deliberately neither vendored nor
changed here. It is a deferred full-FDM qualification issue, not evidence of a
fault in the curated helper API.

The complete `aircraft-view.inc` and both `flight-backdrop.inc` variants are
likewise unique but policy-coupled. Their toggle keys, global camera/object
state, work buffers, viewport geometry, colors, static drawing packet, and VDU
sender remain with the applications. Terrain providers/managers, streamed
regional coordinates, and LOD policy form a still larger subsystem and are
also deferred. Repeated application-local `sync_camera_state` bodies already
appeared in earlier demo patterns, so no duplicate camera-sync routine was
imported; `P3D-API-1` continues to track that independent transport boundary.

The donor commit contains no `LICENSE`, `COPYING`, or `NOTICE` file.
`P3D-LIC-3` therefore requires an explicit permission, compatibility, and
attribution resolution before these adaptations can be distributed under the
candidate's root Unlicense.

## Wolf3DOrig boundary

The Wolf audit resolved into the separate `wolf3dorig/` peer library in the
AgonMaths source tree. Its historical sight handoff now lives under that
project's `wolf3dorig/docs/provenance/` tree. No Wolf routine or Wolf
provenance document is included in the Pingo3D candidate archive.

The audit found renamed copies of the existing Pingo3D multiply routines and
four genuine Wolf-domain fixed-point helpers: `w3d_slope8`,
`w3d_step_partial`, `w3d_actor_mul_q8_q12`, and `w3d_muldiv_u24`. Keeping
Wolf3DOrig independent resolves `P3D-SCOPE-1`: these routines will not be
folded into the Pingo/Q15 API merely because they share low-level arithmetic.
No proprietary Wolf assets or original reference source were copied into
Pingo3D.
