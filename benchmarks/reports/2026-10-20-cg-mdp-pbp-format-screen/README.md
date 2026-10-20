# Bounded affine PBP formatter screen

Commit `68af6bea9b41461ac385bd7c5077a144604a0193` replaces repeated
`fprintf` calls in the affine PBP reason hot path with `std::to_chars` and a
bounded 64 KiB incremental writer. It does not buffer an unbounded reason or
change proof logic. The comparison baseline is the accepted code checkpoint
`eed33c8ee54290bd6c30d18d248f4466774a52c1`.

Before cluster launch, the affine PBP fixture matched the predecessor
byte-for-byte (SHA-256
`693a583248edfdf94e1666c8dcc213fbe7735bf12dd1b8975f899ccdd5db8619`).
The focused PBP, pinned VeriPB/CakePB, negative-checker, public-correspondence,
affine transaction, and proof-output tests passed. A new unit regression
constructs and compares more than 64 KiB of parity-reason text across the
writer's flush boundary.

Two immutable matched campaigns ran on node007 through SLURM arrays `1794809`
and `1794812`. Each campaign paired the production candidate, its public-port
twin, and the predecessor in the same window. Every task used `cpu-q`, 4
cores, 32 GiB, and a 120-second solve cap, with no throttle or node constraint.
The all-user storage governor job `1791687` remained live and was forbidden
from pausing, suspending, requeueing, or cancelling running jobs.

## Result

Both campaigns reached the expected 120-second diagnostic timeout. Candidate
and predecessor reproduced the exact affine trajectory in both draws:

- initialization work: 3,270,469
- search services: 57,662
- reasons published: 35,853
- service work: 194,300,672,066
- proof additions: 120,995
- resource declines: 1

Measured affine-search overhead was:

| Draw | Candidate | Predecessor | Paired reduction |
| --- | ---: | ---: | ---: |
| `1794809` | 33.989906149 s | 34.143471577 s | 0.450% |
| `1794812` | 32.425545367 s | 32.554318492 s | 0.396% |

The mean paired reduction is 0.423%. Both public arms reported zero fallbacks
and zero declined requests. This is a consistent but small causal improvement;
it is accepted as a low-risk development checkpoint, not as a competitiveness
or release result. It remains far above the paper-screen target of 7.550 s.

No arm solved the selected instance, so these campaigns produced no terminal
UNSAT certificate to check. Every one of the six results reports
`artifact_deleted=true`, `derived_artifacts_deleted=true`, and
`cleanup_pending=false`; no expanded CNF, PBP proof, or checker intermediate
was retained. Home storage remained 12% used.

## Immutable evidence

First draw:

- manifest: `7741fab4f5ae15b404af7c097301195fe04c55e5a6b21700bb27368fdb5cf93b`
- candidate: `c6406a7ed3cb9920b96a1d3b4f21e8cec7e170c92d584cb74df2bc29a989b994`
- public: `bbd78232be520597fb816c71283cd8061d2ffa0774778ef26344b4a8dd37b364`
- predecessor: `4c4aaeee523794db42c75eb85a2366f03acb93f6990eaabfa8a1f4d98893724f`

Repeat draw:

- manifest: `3799ce286405e63d20d588e4e81e5c132c90c2eb07739b4d0ca9c0f234bfe02c`
- candidate: `2ca7db631d13e597ad0f616f628fd84b08e43f1b074f1d1761473a85a35334ab`
- public: `f3e63f29e002d8a1c576cbe0c9a56d21a2ebe2c0aecda5b512cc140afc831a1a`
- predecessor: `70f6c9b9b5e98259c0ea8d1c15ac55c4f39b703a1d618dfb0bab10e87667a177`

Disposition: **ACCEPTED DEVELOPMENT CHECKPOINT**. Full Debug/Release testing,
100,000-seed DefaultPolicy and random-policy fuzz, the four fixed regression
sets, full proof/model validation, public/static trajectory parity, and the
declared release performance gates remain incomplete and are not replaced by
this screen.
