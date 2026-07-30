# Generated assembly tests

eZ80 regression programs belong here. Their authoritative inputs and
generators belong in `../../../scripts/`.

The test programs should include the project-local API snapshot and 3D source
from `../agon/`.

`3d_math.asm` consumes the deterministic records produced by
`generate_math_vectors.py`.

`3d_transform.asm` consumes the 32-byte records produced by
`generate_transform_vectors.py`. Both runners print MOS-console summaries and
write compact result files for host automation. After a clean correctness
pass, the transform program also runs file-free successful-call timing loops;
the `P3T2` result contains both the mixed regression times and the
baseline-subtracted call times.

`3d_pose.asm` consumes the 224-byte records produced by
`generate_pose_vectors.py`. It compares complete 64-byte object states,
auxiliary-input immutability, pointer preservation, direct inverse-trig/Euler
outputs, Pingo packet bytes and builder return registers, carry, and
surrounding canaries. Its `P3P1` result includes per-op errors, PRT1
mixed-regression times, and the first failing record snapshot.
