# WV100 structured-symmetry G2 schedule boundary

Date: 2026-09-13. This is the bounded G2 falsification result for
[the WV100 mechanism protocol](../../docs/design/2026-09-13-wv100-mechanism-integration.md).
It changes no Tactix rule or default schedule.

## Question and method

Do formula-derived structured automorphisms remain valid across Tactix's
default root preprocessing, and where must an initial symmetry action run?

An ignored C++ harness linked the accepted `a08f9f6a` solver objects (Release
binary SHA-256 `b63f30031229a36b9fea46e37590e892ab4a178a1239357ff3f80557548e1413`).
It found a strict positive ULC subset from clause incidence, inferred row
alignment only from unique cross-block negative-binary matchings, and proposed
adjacent row/column transpositions. Every proposed permutation first had to map
the complete original clause multiset, including multiplicity, exactly onto
itself. Names, paths and family labels were not detector inputs.

The harness then rebuilt the formula independently at every default program
boundary, ran the exact startup prefix ending there, materialized root units as
clauses, and tested the same accepted permutations against the complete live
kernel multiset. Its pinned executable SHA-256 was
`f751ca0882d026d25b28a490dccf455665ee40d4342a87cdf1d782410a1b9f1b`;
the harness source SHA-256 was
`37f615e578204bc8dbf2cb0968bbbf7b4c667b6f63fc0691cebb3f358fc9e698`.

Job array `1688177` ran on `cpu-dense-hwulab-q` with eight CPUs, 32 GiB and a
300-second process limit. Each compressed input was streamed through a pipe and
held in memory; no decompressed formula or proof file was created. The three
tasks took 2, 1 and 23 seconds. Slurm records `FAILED/1` because the harness
deliberately returned one after detecting a broken generator; the complete
logs and expected boundary assertions below are the result oracle.

## Result

| WV100 representative | Compressed input SHA-256 | ULC blocks | Validated generator basis | Through second `Backbone` | After `Factor` | Distinct clauses before / after Factor |
|---|---|---:|---:|---:|---:|---:|
| rank 2, `php_sudoku_p15_h14` | `bf4494ed542a98bcabd251c6a8a48b46dfd560b9808dd6af0fb5a4f721169a14` | 15 | 27 (14 row, 13 column) | 27 | 0 | 2,850 / 1,101 |
| rank 4, `cliquecolouring_n13_k8_c7` | `c652f5ba61111051771be8d9c53664c8b7f6be84280fd62c71fc31ccbf5dc58d` | 21 | 7 row | 7 | 0 | 6,196 / 2,365 |
| rank 17, `chnl-020x061` | `cfbba532a95ab9bb86b4fca88edc610eeaf902cf0aa535906c4d0ff7e4af1863` | 122 | 139 (120 row, 19 column) | 139 | 5 | 73,322 / 7,260 |

All validated generators survived root collection and every prefix ending at
`Congruence`, `Substitute`, `HyperBinaryRes`, `Backbone`, `Probe`, `Vivify`,
`Sweep`, the second `Substitute`, `TransitiveReduce`, `HyperTernaryRes`, and the
second `Backbone`. Factor was the first action to break any of them. The later
`SubsumeForward`, `Strengthen`, `BVE`, `Instantiate`, `BCE` and `CCE` prefixes
did not restore the original action.

As negative controls, the strict producer emitted no generator on the general
`R_18_4_4` SR example and no generator on the Johnson-style
`count_p2_M21` formula. Those declines are not survival evidence. The producer
also rejected any incidence-derived candidate that failed exact multiset
validation.

## Design consequence

G2 closes for the first structured producer: its initial default-policy slot
must precede Factor. Later explicit applications remain legal under N1, but
they must detect and validate automorphisms on the then-current formula; they
cannot reuse a pre-Factor generator. This result does not establish Johnson or
general-automorphism schedule boundaries, and it does not show a Tactix solve.
G0 obligation classification remains open before S1 solver mutation.
