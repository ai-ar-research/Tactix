# AM332--AM333: two LymphoSAT-owned rows as Lucky model constructors

Checkpoint 21 September 2026 UTC. Base commit `4523daae`. Third Phase 3 (WV2-89) increment.
All solver work on `cpu-dense-hwulab-q`, 12 CPUs, 32 GiB, 1,200 s solve limit; frozen checkers
dsr-trim `3b5c7737`, Trestle `19bf79f4`, dsr-compress `a44102ce`. Source owners:
`winners/lymphosat-sc2026/green/lymphosat/src/solver/solvers/{sgen,independent-set}/solver/`.

## Pins

| Pin | Release SHA-256 | Debug SHA-256 | Build job | Content |
|---|---|---|---|---|
| AM332 | `ca4b4cf0...` | `1f2ad6cf...` | `1714013` | instance 17 |
| AM333 | `0ec66e12...` (two independent builds byte-identical) | `0cfcf2f7...` | `1714363` | instances 17 + 18 |

## Instance 17: signed at-most-one block exact cover (`sgen1-sat-180-100`)

The row: 180 variables in 36 pairwise at-most-one cliques of 5, and 72 positive 5-clauses in
which every variable occurs exactly twice. LymphoSAT's sgen owner reads a binary `(a v b)` as
the at-most-one edge between the signed items `~a` and `~b`, requires blocks of exactly five
items, exactly twice as many covering clauses as blocks, and each item in one block and two
covering clauses; a model is an exact cover of all blocks and covering clauses by chosen items.
Tactix ports the recognizer exactly (on the live clause set; blocks are the connected
components of the edges) and the owner's search: minimum-remaining-rows column choice, rows
ordered by the sum of their columns' sizes then the DIMACS literal, then randomized restarts
reseeded by the owner's stride with random tie-breaks and shuffles. The node bound is the
owner's per-attempt node limit (2,000,000); restarts default to none because the owner's later
phases are wall-clock bounded; the owner's bounded local-repair phase is not ported. A completed
cover makes each chosen item true and every other item false and is validated as a total model.

Deterministic search on the row: 101,674 nodes, identical to a Python replica of the owner's
algorithm; `sgen1-sat-180-100` 969 s -> 0.04 s, model checked. `sgen1-unsat-121-100` (WV100)
is not recognized and is unchanged.

## Instance 18: independent set under a layered exact-count encoding (`1-ET-512-K-96`)

The owner recognizes the encoding from counts only: K = largest clause size - 1, N solves
variables - N = sum_{i<=N} (min(i, K) + 1), and the graph edges are the negative binaries among
the first N variables. It then identifies the graph as a member of its transposition or
Z-channel families and constructs optima by Hamming-weight decomposition and clique search.
Tactix keeps the recognition and drops the family library: a minimum-degree greedy (ties by
index) followed by a bounded swap local search (a free vertex enters; a vertex with one
selected neighbour is replaced when a second free vertex then fits; a random selected vertex
leaves when nothing moves) finds a K-vertex independent set; the state variables are completed
by the shared propagation helper (`lucky_complete_by_propagation`, `model.hpp`), the remainder is
false, and the total assignment is validated. Zero steps or rounds select the vertex count.

On the row: 512 vertices, 4,032 edges, target 96; greedy 94, two swap steps, three propagation
rounds; timeout -> 0.21 s, model checked (2,068,700 clauses).

Both strategies are typed and public (`TACTIX_ONLINE_PARAMETERS_EXACT_COVER_MODEL`,
`..._INDEPENDENT_SET_MODEL`), serialized for random policies (`exact-cover-model`,
`independent-set-model`, drawn from a derived stream at a rate that keeps every other Lucky
kind's coverage), on in DefaultPolicy's structural startup, and decline every other shape.

## Evidence

AM332 gates (`benchmarks/runs/20260921-am332-exact-cover-*`, jobs `1714033`--`1714036`) against
the AM331 gates: Rank 1--50 50/50 (+2.4%), B28+C5 33/33 (-0.1%), WV100 100/100 (-0.7%), all with
zero conflict-count changes; WV2-89 82 solved, 77 within 600 s, the only change being
`sgen1-sat-180-100` (935 s, 20.3M conflicts -> 0 s, 0 conflicts); no lost row. Fuzz
`...-am332-exact-cover-claude-fuzz` (`1714014`--`1714031`): failure-seeds PASS, controls OK, every
finished leg 0 failures. CTest (instances 17 + 18, job `1714361`): 171/171 Release, 170/170 Debug.
AM333 fuzz matrix `1714364`--`1714381`: failure-seeds PASS, controls OK, focused legs 42,000 / 27,500 /
21,000 / 14,000 cases with 0 failures, every finished nightly and AVX2 leg 0 failures (`dbg-random` 50,000 cases, 0 failures; all 18 jobs exited 0). AM333 gates
(`benchmarks/runs/20260921-am333-lymphosat-models-*`, jobs `1714420`--`1714423`) against AM331: Rank 1--50
50/50 (+0.1%), B28+C5 33/33 (-0.4%), WV100 100/100 (+1.8%), all with zero conflict-count changes;
WV2-89 **83 solved, 78 within 600 s** (from 82/76), gaining `1-ET-512-K-96` at 0.3 s and moving
`sgen1-sat-180-100` to 0 s; no lost row.

The five certificates that never verify inside the 57,600 s window (`oisc`, three Tseitin rows,
`ncc`) were cancelled by explicit ID on the AM330--AM332 gates once their solve records existed
(`NOTE-cancelled-checks.txt` in each directory) and receive a 600 s window from AM333 on
(`resource_contract.checker_timeout_overrides`, `tools/bench/runtime_task.py`).

## Open

Remaining WV2-89 rows: `ramsey_4_4_19` (declined owner rules), `sted5`, `FmlaEquivChain_4_8_8`,
`noL-11-4`, `puzzle32_sat` (LymphoSAT generator reconstructions with hand-built witnesses or PB
proofs), `snw_16_9`, `Circuit_multiplier24`, `ntil-90d-39`, `lockchart...L19` (plain CDCL), and
`hcp_CP18_18` / `6g_6color` over 600 s.
