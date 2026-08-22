# Pingo3D API TODO

This is the sole authoritative TODO for the AgonMaths Pingo3D project. The
current candidate must not be published as an Unlicense community package
until every publication gate below is either completed or explicitly resolved
in a dated development log.

## Publication gates

- [ ] **P3D-LIC-1** — Confirm redistribution permission and the required
  attribution/license treatment for the `p3d_umul16x16_32` algorithm credited
  to calc84maniac. Record the answer in `provenance.md` and in the package.
- [ ] **P3D-LIC-2** — Confirm redistribution permission and the required
  attribution/license treatment for the `p3d_lookat_isqrt24` algorithm credited
  to xeda112358. Record the answer in `provenance.md` and in the package.
- [ ] **P3D-LIC-3** — Confirm redistribution permission, license compatibility,
  and any required attribution for the source-faithful `3d_fsim.inc`
  adaptations from `pingo_terrain` commit
  `58f0b61734f35965752833c9f74b34cdd55ed296`. That donor checkout has no
  repository license file; inclusion in this Unlicense candidate does not by
  itself grant relicensing permission. Record the resolution in
  `provenance.md` and in the package.
- [ ] **P3D-REG-1** — Add an isolated eZ80 regression for
  `p3d_object_apply_pose_sample8` covering all 256 indices, state writes, dirty
  and carry results, preserved 24-bit registers, shadow registers, table
  immutability, and surrounding canaries. Its existing generator, application,
  emulator, and physical-hardware evidence remains valid.
- [ ] **P3D-API-1** — Resolve camera transport explicitly: either make the
  caller-owned boundary final and retain a normative erratum for the historical
  claim, or implement and regress an application-neutral camera packet builder
  and optional VDU-stream sender. There is no current `p3d_camera_sync16`
  implementation.
- [ ] **P3D-API-2** — Resolve the public status of experimental Pingo
  subcommands 51 (object active), 52 (signed-24 object translation), and 53
  (projection far). Either document a versioned firmware/feature-detection and
  fallback contract and complete the required emulator/physical promotion, or
  exclude those builders from the public release. Existing command-52/53
  source, assembly, bridge, sanitizer, integrated-flight evidence, and the
  new 912,001-record builder/ABI regression do not by themselves make
  stock-firmware support or a stable community protocol.
- [ ] **P3D-REL-1** — After the gates above close, run the focused candidate
  checks, confirm the recorded core-suite evidence still corresponds to the
  shipped code, build the manifest-bearing archive from a clean tree, and
  publish a source revision/tag containing the post-`138098b` roundup.

## Deferred flight-model notes (not candidate gates)

The complete `pingo_terrain` flight model is genuinely new work but is not
part of `3d_fsim.inc`. Its present `flight-input.inc` owns a 77-byte global
state block and directly names `object_state`, `object_ids`, `keyboard_masks`,
`obj_initial_x/y/z`, and `obj_ground_y`; it is single-instance and
nonreentrant. `flight_model_step` is not globally transactional: controls,
rates, or object attitude can change before a later arithmetic failure sets
the sticky flag. Extracting a caller-owned full-model ABI would be new,
unregressed code, so that work is deferred rather than silently advertised by
the curated companion.

One donor ambiguity must remain visible when that full model is revisited. The
target coordinated-yaw gate compares only the high byte of signed-Q8 airspeed,
so it activates at `0x0c00` (3072), while the host model and documentation use
the exact `flight_takeoff_speed_q8=3200` threshold. That inline block was not
vendored or changed. This discrepancy does not gate or imply an error in the
curated helpers.

The donor's exact ADL24 position integrator is also retained as provenance,
not copied into the companion. It correctly preloads `UDE` for sign extension,
then byte-loads `D:E`, and uses unsuffixed 24-bit loads/adds/stores on explicit
`dl` fields. Position addition consequently wraps modulo `2^24`. It was omitted
because it is tied to the global flight-state layout, not because native
24-bit arithmetic is undesirable.
