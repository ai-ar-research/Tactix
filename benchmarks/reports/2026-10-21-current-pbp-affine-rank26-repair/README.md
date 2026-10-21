# Current-PBP affected-cell repair

Disposition: **PASS**.

The frozen four-set audit found one row with nonzero affine root/search service
or published affine reasons: B28+C5 rank 26,
`tierB/or_randxor_k3_n540_m540.cnf.xz`. Slurm job `1795283` reran only current
production, public, and hardcoded for that affected row in one sequential
`cpu-q` allocation (four cores, 32 GiB). Compatible completed rows and the
unchanged paper arm were reused.

All three arms returned UNSAT, emitted the identical 124,199,466-byte PBP
certificate with SHA-256
`820c0a5b301002819b3781c2932e0c5a9aefe31e1de8333a3ed1eb062d8a802d`,
and passed VeriPB plus CakePB. They matched at 452,976 conflicts, 564,665
decisions, and 34,386,726 propagations. Production and hardcoded normalized
terminal statistics were exact after excluding wall-clock-only fields. Public
made 2,145,877 requests with zero fallbacks and zero declines.

| Arm | Solve time | Peak RSS |
| --- | ---: | ---: |
| production | 16.535 s | 95,992 KiB |
| public | 16.335 s | 96,600 KiB |
| hardcoded | 15.786 s | 96,024 KiB |

Public/production was 0.987904 and production/hardcoded was 1.047447, both
inside the 1.05 ceiling. The exercised trajectory had two affine root services,
1,111 affine search services, and 20 published affine reasons.

Job `1795283` completed `0:0` in 4m41s. Every task records primary and derived
artifact deletion with no cleanup pending. A terminal audit found no generated
CNF, PBP, proof, OPB, LRAT, or DSR file. The immutable raw campaign remains at
`benchmarks/runs/20261021-current-pbp-affine-rank26-repair`; its manifest
SHA-256 is
`67a1067aba0cc02845834402159986d31d0940afab6a65660f83af56b783e09a`.
