# Satsuma symmetry practice in DefaultPolicy (items A, B, C, D)

User decision: plan §9.5 (commit `b1488cf83`) plus item A from the coordinator.
Owner source: `winners/satsuma-iter-kissat-sc2026/anders/satsuma-iter-kissat/src/solver/satsuma-dev/src`.
Run data: `benchmarks/runs/20261007-satsuma-bcd-validation/` (scripts, pins, validation, probes),
`benchmarks/runs/20261007-satsuma-bcd3-pair-*` (same-CPU pairs), `benchmarks/runs/20261007-satsuma-bcd3-shuffle/`.
The first candidate pin (`bcd`, pairs `20261007-satsuma-bcd-pair-*`) is superseded; see "Port defects found".

## What changed

| Item | Owner | Tactix |
|---|---|---|
| A | `satsuma.h:809`: iterate only when the pass produced `sbp.n_clauses() >= 1` | `RepeatTrigger::SymmetryBreakingOutput` on every SR stage: a round repeats only if it committed an SR consequence other than a ULC AMO binary (`sr_applied - sr_unique_literal_amo_binaries`). Pure closure, AMO completion and compaction do not earn a round. |
| B | sticky `reached_limits` (`satsuma.h:49,194,267,810`) | `stop_repeat_at_detection_limit` per stage; sequence-level and startup-wide sticky `detection_limit_reached`; a later stage finishes its first pass but never repeats (`structured_root_stage_after_feedback`). |
| C | large domain changes only the fixing pass (`satsuma.h:217,227`) | `large_domain_structured_root_preprocessing_stages` = the bounded-backtracking chain (AMO completion, row/product refinement, ULC matrices, graph, remaining graph); generator fixing selected by `schreier_domain_limit`. |
| D | `find_non_trivial_orbit` cut point | `cut_point = LargestOrbitLast`; residual reseed handoff stays off. |
| unchanged-work skip | (owner never runs a duplicate detection) | `UnchangedDetections`: an identical generated Orbitopal/GraphSymmetry request that ended without effect is skipped until the next effective transition. |

### Owner `reached_limits` sites and their Tactix equivalents

| Owner site | Limit | Tactix equivalent (`detection_limit_reached`) |
|---|---|---|
| `satsuma.h:168-176` | special detection skipped when graph vertices >= `graph_component_size_limit` (5M) | policy gate in `structured_root_stage_after_feedback`: orbitopal matrix detection off when `substitution_redundancy_symmetry_graph_vertices` > `maximum_graph_vertices` (AMO completion still runs, as `cnf.h ulc_add_amo`); also graph-size admission sites in `symmetry.cpp` and orbitopal refinement-graph admission |
| `symmetries.h:579` | DejaVu `get_reached_limit()` (budget, Schreier support, component) | backend `Status::Incomplete` |
| `symmetries.h:1468`, `3099` | Schreier cost limit in negation fixing / fix-and-cut | `fix_clauses` LimitReached (planning tick limit / group ticks) |
| `symmetries.h:2042`, `2951` | row-column / row `split_limit` = max(16n, 16Mi) | `RefinementProgressFeedback::stop` (same owner scale), and exhaustion of the shared max(16n,16Mi) window / 20e9 hard allowance |
| `satsuma.h:737` | conflict during preprocessing | structured root returns UNSAT; no repetition question arises |
| `satsuma.h:810` `n_variables() < 20000` | `formula` was cleared just before (`satsuma.h:777`), so the test always passes | none needed |

No owner limit site lacks an equivalent. Tactix-only caps that do **not** set the flag:
refinement-orbit cap (owner `row_orbit_limit` breaks silently), clique ticks/vertices,
product states, matrix/instance counts, structured-hint generator counts.
Owner hypergraph macros (`hypergraph.h`) shrink the owner's graph; Tactix's vertex count
is therefore an upper bound on the owner's for the gate.

## Port defects found by profiling

The first pin (`bcd`) ran C literally. Profiling both arms (`results/v1-superseded/perf`) showed two
implementation defects, both repaired in `bcd3` (pins `build/pin-satsuma-bcd-rel3`, `-relg3`):

1. **Quadratic ULC authentication.** On `pj2009_k80` (3.1M variables) 99.3% of cycles were in
   `authenticate_unique_literal_amo`: every generated AMO binary rescanned all variables and the
   whole arena. 31,513 binaries took 298 s and never finished startup (pair: timeout vs c2 139 s).
   Repair: only the pivot variable can carry the unit; large-clause occurrences come from a
   per-batch image (`UniqueLiteralOccurrenceCache`, rebuilt on any arena change, discarded between
   batches; a stale entry can only overcount and decline). Debug builds cross-check every cached
   decision against the scan. After: 115,488 binaries in 1.56 s, SAT in 88 s (c2 132 s).
2. **Missing owner graph-component gate.** On `cabp-V-nos6` the orbitopal steps added by C cost
   ~20 s with zero output; the owner skips special detection there because its graph has more than
   5M vertices (`satsuma.h:168-176`). Repair: the policy gate described above. After: startup
   2.23 s vs c2 1.83 s, same 86,308 conflicts.

## Validation

Round 5 (`results/debug5`, `results/asan5`; base `fff172b9d` archive + `pins/bcd4.patch`, untracked
checkers/reference clones/benchmarks linked read-only):

- Debug full CTest: 352/353 pass. The only failure, `strategy_config` line 351
  (`fastel_rounds > 0`), also fails on unpatched base (`results/base`); with that one assertion
  neutralized the rest of the test passes on both base and candidate (`results/masked-*`).
- ASan/UBSan focused CTest (30 entries: SR, structured root, sequence, scheduling, online, random,
  ULC, micro proofs): 29/30 pass; the failure is the same pre-existing `strategy_config` assertion.
- `micro_proofs` (symmetry/orbitopal/SST/ULC fixtures through the binary, native DSR, every lemma via
  `proof_oracle.py check-dsr`, dsr-trim + Trestle): pass in Debug and ASan.
- Structured-root DSR fixtures through the Debug binary, `check-dsr --every-lemma`: php(5,4),
  php(6,5), php(7,6) and php(6,5)+60,000 padding variables (large-domain program) all UNSAT and
  verified.
- Round 6 (`results/debug6`): the same change on the newer HEAD `100764994` (symmetry-use
  observation commits): Debug full CTest 353/354, the same pre-existing failure only; all four
  DSR fixtures verified.
- Pair runs check every SAT model and every completed proof through `runtime_task.py`.

## Same-window pairs

`make_pair.py` campaigns `benchmarks/runs/20261007-satsuma-bcd3-pair-<row>`: c2
(`build/pin-candidate-c2-rel`, `bec9eede…`) then bcd3 (`build/pin-satsuma-bcd-rel3`, c2 archive +
`pins/bcd3.patch`), same CPU, sequential, `--auto-proof --default-graph-labeling=on`, 8 CPUs /
32 GiB, `runtime_task.py`. Structured = `c phasecost structured`; valid = model or proof checked
inside the 1,800 s checker window (UNSAT `False` with a checked arm missing is a window expiry,
not a rejection).

| row (suite timeout) | c2 | bcd3 | structured ms c2 / bcd3 | SR applied (ULC AMO) c2 / bcd3 |
|---|---|---|---|---|
| cabp-V-nos6 k275 (1,200) | SAT 269.2 | SAT 263.1 | 1,948 / 2,471 | 0 / 0 |
| Break_20_72 (1,200) | SAT 666.0 | timeout | 349 / 590 | 0 / 31 (31) |
| SC25_Timetable_C_496 (600) | SAT 410.4 | timeout | 879 / 3,839 | 5 / 725 (720) |
| bphp_p23_h22 (600) | UNSAT 544.4 (window) | **UNSAT 2.5, verified** | 143 / 136 | 233 / 233 |
| ntil-90d-37 (600) | SAT 21.8 | timeout | 1,704 / 1,624 | 1 / 1 |
| ntil-90d-40 (1,200) | SAT 97.3 | timeout | 3,187 / 3,035 | 1 / 1 |
| noL-11-0 (600) | timeout | timeout | 101 / 75 | 1,093 / 1,093 |
| noL-11-8 (600) | timeout | timeout | – / 195 | – / 1,082 |
| Rank754 (5,000) | PENDING | PENDING | | |
| Rank896 (5,000) | PENDING | PENDING | | |
| b18 (1,200, overhead row) | UNSAT 219.0 | UNSAT 248.3 | 290 / 1,516 | 0 / 12,060 (12,060) |
| oski15a01b06s (1,200, overhead row) | UNSAT 360.8 (window) | UNSAT 439.5 (window) | 4,615 / 9,124 | 1,517 / 1,503 |
| b19_1 (1,200, overhead row) | UNSAT 507.9 | UNSAT 488.2 | 621 / 2,600 | 0 / 24,064 (24,064) |
| pj2009_k80 (1,200, overhead row) | SAT 131.5 | SAT 87.9 | 470 / 1,594 | 0 / 115,488 (115,488) |

The earlier `bcd` pin's pairs (`20261007-satsuma-bcd-pair-*`) agree on every row the two repairs
do not touch (ntil, noL, tt496, Break_20_72, b18) and show the two defects on pj2009/cabp.

## Startup overhead

`results/overhead2` (same CPU, c2 then bcd3, 300 s cap, `TACTIX_SLOT_STATS=1`; diagnostic
exception to the runner rule because the runner scrubs `TACTIX_*`). Ticks are shared SR-discovery
ticks; c2 reports only its root total.

| row | c2 startup s / ticks / passes | bcd3 startup s / ticks / passes | bcd3 SR applied (ULC AMO) | bcd3 stages at a limit | unchanged skips |
|---|---|---|---|---|---|
| cabp-nos6 | 1.83 / 22M / 2 | 2.23 / 168M / 4 | 0 (0) | 2 | 3 |
| Break_20_72 | 0.36 / 578M / 3 | 0.61 / 335M / 4 | 31 (31) | 2 | 2 |
| SC25_Timetable_C_496 | 0.96 / 19M / 3 | 3.92 / 162M / 4 | 725 (720) | 1 | 0 |
| bphp_p23_h22 | 0.14 / 35M / 5 | 0.14 / 34M / 5 | 233 (0) | 0 | 4 |
| ntil-90d-37 | 2.01 / 63M / 5 | 1.83 / 60M / 4 | 1 (0) | 2 | 2 |
| ntil-90d-40 | 2.40 / 69M / 5 | 2.26 / 65M / 4 | 1 (0) | 2 | 2 |
| noL-11-0 | 0.10 / 7M / 6 | 0.07 / 7M / 6 | 1093 (1089) | 0 | 3 |
| noL-11-8 | 0.35 / 45M / 6 | 0.19 / 25M / 4 | 1082 (1063) | 1 | 0 |
| Rank754 (Timetable C_495) | 0.95 / 19M / 3 | 3.83 / 161M / 4 | 728 (723) | 1 | 0 |
| Rank896 (pcmax m19) | 0.24 / 17M / 5 | 0.17 / 10M / 4 | 0 (0) | 0 | 1 |
| b18 | 0.30 / 5M / 2 | 1.74 / 47M / 4 | 12,060 (12,060) | 0 | 2 |
| oski15a01b06s | 4.46 / 7,645M / 5 | 8.92 / 5,955M / 4 | 1,503 (0) | 2 | 1 |
| b19_1 | 0.55 / 13M / 2 | 2.55 / 82M / 4 | 24,064 (24,064) | 0 | 2 |
| pj2009_k80 | 0.47 / 0M / 3 | 1.56 / 272M / 4 | 115,488 (115,488) | 2 | 2 |

A and B do not add cost: they only remove rounds. Where repetition changed, bcd3 runs fewer
passes and fewer ticks (noL-11-8 0.35 -> 0.19 s, Rank896 0.24 -> 0.17 s, ntil 2.0-2.4 ->
1.8-2.3 s), and on every large row where symmetry yields nothing each stage runs exactly once,
with 1-3 identical detections skipped as unchanged. The added startup time on the large rows is C: owner-faithful AMO completion
(b18, b19, pj2009: all committed output) plus one structured-detection pass below the graph limit
(oski06 +4.5 s, tt496/Rank754 +2.9 s, cabp +0.4 s). Where symmetry is useless the remaining
overhead is 0.25-2.0 s on 100-1,200 s runs.

## Noise vs mechanism

Profiles first (plan §8.3): `results/perf2/*.{flat,children,stats}.txt`, same CPU, .text-identical `-g`
pins (`pin-satsuma-bcd-c2g` = c2 `.text` `221def52…`; `pin-satsuma-bcd-relg3` = rel3 `.text` `9df1d9b3…`),
`cycles:u`. For tt496, ntil37, ntil40, Break_20_72 and b18 the symbol distributions of both arms
match within a few points (`kernel_propagate` 33–57%, then analysis/minimization); per-pass
timing (`c timing`) scales with run length; no inprocessing pass spends extra time without output.
The only pass that grows is structured startup (+0.2 to +3 s), and all of its extra work is
committed output (ULC AMO binaries kept as clauses, graph units).

Shuffle probe (`benchmarks/runs/20261007-satsuma-bcd3-shuffle`, 5 renamed/sign-flipped/
clause-shuffled copies x both binaries, suite timeouts, 8 CPUs/32 GiB):

| row | c2 solved, median s | bcd3 solved, median s | verdict |
|---|---|---|---|
| tt496 (600 s) | 1/5, 600 | 1/5, 600 | luck: the original-file c2 solve is one trajectory |
| ntil-90d-37 (600 s) | 0/5, 600 | 0/5, 600 | luck: c2's 22 s needs the original ordering |
| ntil-90d-40 (1,200 s) | 0/5, 1,200 | 3/5, 1,110 | luck; bcd3 better on shuffles |
| Break_20_72 (1,200 s) | 1/5, 1,200 | 4/5, 924 | luck; bcd3 better on shuffles |
| b18 (1,200 s, UNSAT) | 5/5, 276 | 5/5, 258 | noise |

The first copies' `mc-FAIL` marks are a harness gap (the checker's `modelcheck.py` module was
missing from the probe pins until it was copied in); later copies verify `OK`. Model validity is
established by the runner pairs.

## Open items

- Rank754/Rank896 5,000 s pairs and the oski15a01b06s profile/shuffle probe were still running at
  the first commit; results are appended here when complete.
- Structured startup above the graph limit is cheap, but below it the C pass costs up to +4.5 s
  without output (oski06) because each sibling orbitopal step (product, ULC matrices, rows)
  builds its own formula snapshot and refinement graph; the owner builds one graph per pass.
  Sharing one snapshot across sibling detections while the formula is unchanged is the general
  repair; it is not part of this change.
- `strategy_config` line 351 (`fastel_rounds > 0`) fails on base `fff172b9d` already
  (`results/base`); owner of `524b7b48b` should repair the fixture or the expectation.
- Owner `skip_generic` (`satsuma.h:186`: no generic detection in an iteration whose special
  detection produced more than one predicate clause) is not modelled; not in this scope.
