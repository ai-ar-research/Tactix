# Rank 541 direct-binary LSR checker diagnostic

## Outcome

Rank 541 remains open.  Checker-only CPU-Q job `1750998` used the exact
retained 247,511,403-byte Tactix certificate and exact original CNF.  Pinned
`dsr-trim` independently verified the certificate and emitted a binary LSR,
but pinned Trestle exhausted the rest of the shared 1,800-second allowance.
This is a clean checker timeout, not a proof rejection, solver rerun, crash, or
identity mismatch.

The direct-binary route removes text expansion from the experiment.  It does
not make the retained semantic core checkable inside the current cap:

| Stage | Exact result |
|---|---:|
| `dsr-trim` | verified in 363.628 s |
| Parsed DSR | 17,998,943 lines; 3,477,457 additions |
| Empty clause | after retained addition 3,476,435 |
| Binary LSR | 342,910,304 bytes; SHA-256 `78583599cc9436911771a4720967694d46010dd08ef013fd18c6b9e046a38334` |
| Direct binary Trestle | timeout after 1,436.299 s, after parsing the formula |
| Whole chain | not verified after 1,800.151 s |

The retained core differs from Rank 1129 by only 28,591 additions (0.83%):
Rank 1129 retained 3,447,844 additions and reached the same Trestle boundary.
The two crafted-family rows therefore expose the same general semantic-core
cost.  Conversion is not the decisive bottleneck.

## Saved comparison evidence

The frozen portfolio arms corroborate a representation-level problem.  Kissat
SC2024 and AE-Kissat-MAB emitted 245,536,154-byte and 221,402,637-byte DRAT
certificates and both exhausted 14,400 seconds in `drat-trim`.  Satsuma+Kissat
emitted a 270,688,506-byte DSR, retained almost 5.97 million additions, and
exhausted 13,443 seconds in Trestle after successful `dsr-trim`.  The only
checked arm is LymphoSAT's ordinary Kissat-PBP fallback: its 1,065,468,767-byte
PBP verified in 1,475.092 seconds.

No target-specific solver path, checker fallback, route retry, or certificate
contract change is justified.  The evidence does not identify a sound general
solver-side compaction that can be implemented in this increment.  A future
increment must first reduce the independently required semantic core or obtain
explicit approval for a different general certificate contract.

## Frozen evidence

[`result.json`](result.json) is the exact job result, SHA-256
`c6a5ebf58f041b0a9fa53d423a0df4798c29042025bcd5fb68e7e3978f05a466`.
The diagnostic source and SLURM wrapper are retained as
[`probe.py`](probe.py) and [`probe.sbatch`](probe.sbatch), SHA-256
`1385c4fccf6fb9121eb97bbfabb0b242497d63a39acaaaf6e75f0af8a6152c4d`
and `220ad1e22feb3ee4dc24a80d8cc4d5fb6146e2652884aafd7c6b6007640f7159`.
All input, proof, checker, and source-result hashes were checked before the
pipeline began and are recorded in the result.
