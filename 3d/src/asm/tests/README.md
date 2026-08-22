# Generated Pingo3D assembly tests

eZ80 regression programs belong here. Their authoritative inputs and
generators belong in `../../../scripts/`.

The test programs should include the project-local API snapshot and Pingo3D
source from `../agon/`.

`3d_math.asm` consumes the deterministic records produced by
`generate_math_vectors.py`.

`3d_transform.asm` consumes the 32-byte records produced by
`generate_transform_vectors.py`. Both runners print MOS-console summaries and
write compact result files for host automation. After a clean correctness
pass, the transform program also runs file-free successful-call timing loops;
the `P3T2` result contains both the mixed regression times and the
baseline-subtracted call times.

`3d_pose.asm` consumes the 224-byte records produced by
`generate_pose_vectors.py`. It compares complete 64-byte object/camera slots,
auxiliary-input immutability, pointer preservation, direct inverse-trig/Euler
outputs, coarse/fine trig outputs, camera look-at state, Pingo packet bytes
and builder return registers, carry, and surrounding canaries. Its `P3P3`
result includes per-op errors, PRT1 mixed-regression times, and the first
failing record snapshot.

`3d_fsim.asm` consumes the grouped 64-byte `FSV1` records produced by
`generate_fsim_vectors.py`. It calls the actual core, generated table, and
optional `3d_fsim.inc`, covering all eleven companion entry points. The
correctness-only harness checks values, documented carry, full-width active
and shadow register contracts, source/object immutability, surrounding
work-area canaries, and packet-builder return pointers and lengths. Its
69-byte `P3F1` result uses 24-bit total-record, total-error, and per-operation
error counts plus first-failure metadata.
