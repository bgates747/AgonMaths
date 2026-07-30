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
