# Support-local SR producer performance gate

## Candidate and protocol

This gate measures immutable synthetic source
`992e04142e6ccaeed3e3a91477766c99f9905ac9`. Its independently rebuilt Release
binary has SHA-256
`96cdeac8c35a53981feaaf506ccebe8d413d1b860ce49a61c83c147246b8e0b6`.
The source replaces repeated whole-formula validation of the existing exact-row
symmetry producer with support-local validation. It does not add the general
colored-CNF symmetry producer.

Both candidate-only jobs used `cpu-dense-hwulab-q`, c12, 32 GiB, a 1,200-second
solver timeout, native DSR, and anonymous node-local scratch. They used retained
checked references and did not rerun a reference solver:

- B28+C5: job `1689071`, 33 tasks.
- Rank1--50: job `1689096`, 50 tasks.

The exact binary passed 148/148 Release tests and 147/147 sanitizer-applicable
tests before submission.

## Solver checkpoint

| Gate | Outcomes | Checked clean at checkpoint | Candidate seconds | Reference seconds | Ratio | Limit | Result |
|---|---:|---:|---:|---:|---:|---:|---|
| B28+C5 | 33/33 expected | 31/33 | 958.310 | 950.837 | 1.007859x | 1.01x | pass |
| Rank1--50 | 50/50 expected | 33/50 | 8,303.463 | 8,123.812 | 1.022114x | 1.01x | **fail** |

Proof checks remain background work and do not block diagnosis, but their
unfinished state also means this is not a proof-acceptance report. All solver
outcomes are present; the Rank failure is therefore not a partial-vector
artifact. B28+C5 rank 23 improves from 6.477 to 0.117 solver seconds because the
producer makes an effective SR transition, while most Rank entries pay small
overhead and several longer entries accumulate enough cost to fail the aggregate
limit.

## Decision

Do not land this candidate. Preserve its immutable source and run evidence for
diagnosis, remove the broad default-path overhead, and repeat both fixed gates
only after a causal repair. The failed candidate does not change the accepted
R16 base or authorize a full Tier-D run.
