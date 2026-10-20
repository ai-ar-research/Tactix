# Streaming PBP destination-buffer screen

This causal checkpoint tested commit
`96dc1d0e0defeb0c56dcedf9cff623ad51cba9c4`, which configured a 1 MiB stdio
buffer for the final streaming PBP destination. The immediate predecessor was
`eed33c8ee54290bd6c30d18d248f4466774a52c1`. The hypothesis was that fewer
small writes would reduce PBP emission overhead without changing proof logic.

The frozen three-arm campaign is
`benchmarks/runs/20261020-cg-mdp-pbp-buffer-screen`. SLURM array `1794804` ran
the production candidate, its public-port twin, and the predecessor on node007
in the same window. Each task used `cpu-q`, 4 cores, 32 GiB, and a 120-second
solve cap, with no array throttle or node constraint. The all-user storage
governor job `1791687` remained live and was forbidden from pausing,
suspending, requeueing, or cancelling running jobs.

All three runs reached the expected 120-second diagnostic timeout. The
candidate and predecessor preserved the exact affine checkpoint trajectory:

- initialization work: 3,270,469
- search services: 57,662
- reasons published: 35,853
- service work: 194,300,672,066
- proof additions: 120,995
- resource declines: 1

The candidate recorded 34.630015274 seconds of affine-search overhead, versus
33.788557942 seconds for the predecessor. This is a 2.49% regression, so the
buffering change is rejected. The public arm had zero fallbacks and zero
declined requests, but this one timed checkpoint is not a public-interface or
release gate. No arm solved the instance, so there was no terminal UNSAT proof
to check in this campaign; the focused local PBP tests, including pinned
VeriPB and CakePB checks and negative controls, passed before launch.

Every result reports `artifact_deleted=true`,
`derived_artifacts_deleted=true`, and `cleanup_pending=false`; no expanded CNF,
PBP proof, or checker intermediate was retained. Home storage was 12% used
after completion.

Immutable evidence:

- campaign manifest SHA-256:
  `933f77f2d0e50ad5b333fd274d7898cff2ef9eb09cd57edb2e98f07992f0ba96`
- production result SHA-256:
  `562a2dff78eeccb7fdb8ba0b4864f2764dccd4ed1cf17a5c6310c9c069c939c1`
- public result SHA-256:
  `ca075d4794c7400eb5ae40fc5d38496ad9053332d5175f2290e5fa64825213b6`
- predecessor result SHA-256:
  `356432a3f929d5f99df4876383760c1cabeeab1952d138bca2da19b8e0fbbce2`

Disposition: **REJECTED**. Revert the 1 MiB destination-buffer change and keep
`eed33c8ee54290bd6c30d18d248f4466774a52c1` as the accepted development
checkpoint. This result does not satisfy or replace the full release gates.
