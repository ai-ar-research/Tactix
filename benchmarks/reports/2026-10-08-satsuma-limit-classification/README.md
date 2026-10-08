# Satsuma limit classification: the c3 clique-colouring regression

Candidate c3 (`b5ae7373f`, Satsuma A–D plus one-round pure literals) lost
`clqcl_50_6_5`, `clqcl_30_11_10` and `clqcl_100_6_5` from the WV2–89 gate, which
c2 (`76e161b24`) solved. This report gives the cause, the owner-source repair,
and the c3 findings on WV100 proof checks, EDP3-11000 and Timetable_C_392.

**Headline.**
- **Element.** Item (B), sticky `detection_limit_reached`, cut the output. Its
  mapping counted two *Tactix* scheduling windows as Satsuma's `reached_limits`:
  - the renewable startup progress window, below the cumulative hard
    allowance;
  - the orbitopal zero-progress work window, measured in degree/width ticks.
- **Not the cause.** (A) output-earned repetition, (D) `LargestOrbitLast`,
  the unchanged-detection skip and the graph-component gate. C does not apply:
  the clqcl rows have 1,065 to 6,050 variables, below 50,000.
- **Repair.** Only owner limits now end repetition. The pin with the repair
  reproduces c2's startup on every regressed row, byte for byte in SR counts
  and conflicts. clqcl_50 is 3.3 s (c2 3.2 s), clqcl_30 78.8 s (c2 83.6 s), and
  clqcl_100 31.4 s (c2 30.5 s).
- **Two more port defects, found from the same profiles:**
  - Satsuma's shallow-group skip of special detection was missing. Without it,
    EDP3's structured startup rose from 1.5 s to 8.3 s with no output.
  - Satsuma colors unused variables apart; Tactix did not. On Timetable_C_392
    this produced 162 spurious negation units.
- **Still open.** Satsuma reaches its 0.1 s clique-colouring refutations through
  its Johnson-action detector (`detect_johnson_arity2`), which Tactix lacks.

Run data: `benchmarks/runs/20261008-clqcl-diag` (reference logs, stage traces),
`20261008-sbwin-build` (pins, Debug/ASan), `20261008-sbwin-pair-*` (pairs).

## Method

- **Reference.** The SC2026 Satsuma binary (`0680c895…`) runs the exact
  `run_base.sh` `fix` command three ways: with `--silent`, without it, and with
  `--verbose`. Each run is bounded by SLURM to 120–300 s.
- **Tactix stage traces.** These use `TACTIX_SLOT_STATS=1` and
  `TACTIX_SR_TRACE=1` (level 2 for generator fingerprints on Timetable_C_392).
  The feedback stream passes through a FIFO filter that keeps only
  structured-stage actions. `tools/stage_detail.py` adds the shared discovery
  allowance to `stage_actions.py`. Each run is bounded at 60–150 s, a
  documented runtime_task exception.
- **Pairs.** Same CPU, `make_pair.py`, `runtime_task.py`, 8 CPUs and 32 GiB on
  the hwu partition, `--auto-proof --default-graph-labeling=on`. Each pair runs
  c2 (`bec9eede…`), then fix (`build/pin-sbwin4-rel`, `b125ba45…`). The fix
  source is HEAD `b1686763c` (src = c3) plus the repair.

## clqcl_50: what Satsuma does and what c3 did

Satsuma `--verbose` on clqcl_50:
- **Iteration 0.** It finds `candidate Johnson 50, ar 2`, then `found row
  6x100`, then `found row 5x100`, and adds orbitopal fixing for 6 Johnson
  columns. That gives 280 units, with generic detection skipped.
- **Iteration 1.** It finds `row-column 5x6` and 11 more units.
- **Iteration 2.** UNSAT. In total it makes 291 units in 0.10 s.

clqcl_30 (311 units), clqcl_100 (591 units) and cliquecolouring_n41 (198
units) follow the same pattern.

Tactix has no Johnson producer. Its stage-1 orbitopal steps find nothing: the
refinement row candidates reduce to 246×1 or 197×1. c2 reaches UNSAT through
the generic graph step 84, a DejaVu FixAndCut over 9 stage-2 passes:

| clqcl_50 | stage-2 passes | SR applied | detection-limit | conflicts | result |
|---|---:|---:|---|---:|---|
| c2 | 9 | 759 | (none) | 1,214 | UNSAT 3.2 s |
| bcd3 / pure1 / c3 | 1 | 203 | stage 2 = 1 | — | UNKNOWN 600 s |
| fix | 9 | 759 | 0 | 1,214 | UNSAT 3.3 s |

What bcd3 did in stage 2:
- **The pass.** Step 84 committed 89 symmetry-breaking units in pass 1, which
  is enough to earn a repeat under (A). Its FixAndCut planning then ended with
  `chain_planning_stop levels 43 ticks 196,853,667`.
- **Why planning stopped.** The planning limit is `min(configured 20e9,
  remaining allowance)`. The remaining allowance was the startup progress
  window, which started at `max(16·vars, 16 Mi)` and is renewed after progress.
  The backend therefore returned `Incomplete`, `symmetry.cpp` mapped that to
  "the owner's Schreier cost limit", and the stage stopped repeating.
- **The owner's limit is cumulative.** Satsuma's Schreier limit is
  `track->cost_estimate() > total_cost_limit`, with `total_cost_limit = 20e9`
  (`symmetries.h:1466-1468`, `3097-3099`; `satsuma.h:53`). Satsuma has no
  renewable window.
- **The progress window is a Tactix device.** It lets the root scheduler grant
  work in quanta, and every quantum boundary it passes is not an owner limit.

MVRoundRobin n16 v2/v3, n20 v2 and clqcl_100 show the second
misclassification:

```
orbitopal_progress_stop  producer rows  work 18,684,072  allowance 16,777,216  matrices 0
```

This is `RefinementProgressFeedback`, a zero-yield window in degree/width
ticks that is renewed by every admitted matrix. Its own comment said Tactix
"charges deterministic degree/width work instead of … split counts". The
owner's limit counts something else:
- It counts color splits of anchor individualizations (`initial_split_counter
  += ir_controller.get_number_of_splits()`, at `symmetries.h:2061`, `2172`
  and `2546`).
- DejaVu resets `s_splits` at every `move_to_child` (`dejavu/ir.h:1003`).
- Detection stops at `max(16·n, 16 Mi)` (`symmetries.h:2041`, `2950`).

Ticks exceed splits by roughly the average degree. On these rows Satsuma
iterates 4 or more times without reaching any limit.

## Repair (src)

1. **`allowance_limit_reached`.**
   - A new field on both enumeration results records "stopped because
     `maximum_detection_ticks` ran out". The sites are `charge`, the
     structured-discovery and finish-group allowance checks, planning cut short
     by the remaining allowance, the allowance-derived DejaVu generator cap,
     and orbitopal `with_allowance_limit`.
   - `OnlineStrategyExecutor` turns it into `detection_limit_reached` only when
     the binding allowance was the hard ceiling or the step's configured limit.
     The test is `window_bound`: shared discovery, an active progress window,
     an allowance limit below `hard_limit`, and a configured limit above the
     window at entry. Graph size, DejaVu limits and planning at its configured
     limit remain owner limits.
2. **Split limit.**
   - The DejaVu backend now counts splits for anchor partitions and
     `reduce_orbit`: individualization plus one per split-hook call.
   - Orbitopal accumulates them in `refinement_splits` and stops with the limit
     above `max(16·n, 16 Mi)`.
   - The tick window still stops a zero-yield candidate stream, but sets no
     flag.
3. **Shallow-group skip** (`symmetries.h:734`, `2019`, `2939`).
   - Row-column (product) and row detection skip when there are more than
     10,000 non-trivial equitable literal cells and the probed base length is
     below `4·log2(cells)`. No limit is recorded.
   - The skip belongs to the existing base-probe filter, and the probe result
     is cached per refinement graph.
4. **Unused variables** (`symmetries.h` `compute_from_hypergraph`,
   `unused_color`). Both symmetry graphs give each literal of an unassigned,
   clause-free variable its own color.

A, D, the unchanged-detection skip and the component gate are unchanged.

Tests:
- `sequence_followups`: a progress window below the hard ceiling repeats; the
  same stop at the ceiling does not.
- `online_inprocess`:
  - a 5,100-pair formula with one simultaneous involution (10,200 cells, base
    1) is skipped with the filter on, and examined with it off;
  - `(1|2)` plus an unused 3 has exactly one generator.
- Two API tests asserted a model that came from spurious SR units on unused
  variables:
  - `test_online_api` now asserts that the model satisfies the input.
  - `test_api_c` adds `(-2 | 4)`, so the cube repair of the retired member is
    deterministic.

## Findings (i)–(iii)

**(i) WV100 proof-check regressions.** These have the same cause as clqcl. In
c3, stage 2 stopped after one pass (detection-limit 1), and the refutation
moved from startup into search: 66k to 2.7M conflicts, with proofs to match.
With the fix every row is back to c2's startup:

| Row | c3 conflicts | fix conflicts | c2 check s | fix check s |
|---|---:|---:|---:|---:|
| MVRoundRobin_n16_d10_v2 | 66,326 | 1 | 21.6 | 21.6 |
| MVRoundRobin_n16_d10_v3 | 2,739,029 | 1 | 81.3 | 80.5 |
| MVRoundRobin_n20_d10_v2 | 1,314 | 1 | 61.2 | 60.9 |
| cliquecolouring_n41_k5_c4 | 1,208,433 | 43 | 1.4 | 1.6 |

**(ii) EDP3-11000.** The row has 90,956 variables, so the large-domain
program applies (C).
- **Owner.** Satsuma makes 1 generator and 1 unit over 2 passes. Its special
  detection costs 92 ms (`structs`) and its graph and refinement 404 ms.
- **c3.** Four orbitopal steps ran with zero output. Each built a refinement on
  90,956 non-trivial two-literal cells, with base length 1. Each reduced a
  2-row orbit to a 2 × 79,957 matrix (complete 185 ms) and ran column ordering
  until its tick cap (426 ms). Steps 97/90/86/82 took 1.38, 1.36, 0.51 and
  0.53 s.
- **The defect.** This is the missing shallow-group skip: Satsuma skips
  special detection on exactly this group shape.
- **With the skip.** Each step pays only its snapshot (≈190 ms), refinement
  preparation (≈250 ms) and the cell census or probe (≈100 ms). Structured
  time is 3.6 s (c3 8.3 s in the gate, c2 1.6 s), with conflicts identical at
  244,035.
- **Remainder.** The residual ≈2 s is the known per-step snapshot: Satsuma
  builds one graph per pass.

**(iii) Timetable_C_392.**
- **Owner.** Satsuma makes 2 DejaVu generators, 2 units, 555 ULC AMO binaries
  and 1 iteration.
- **c3.** c3 made 476 SR applications: 312 ULC AMO binaries, plus 164 graph
  units (162 negation and 2 binary). Level-2 fingerprints show 325 retained
  generators in stage 2: the 2 real ones (support about 3,000), 162 of support
  2 (`x ↔ ¬x`) and 161 of support 4. All of them move variables with no clause
  left after the stage-2 pure round.
- **The defect.** Satsuma colors such variables apart. This is a port defect.
- **With the fix.** 404 SR applications: 2 units and 402 ULC AMO binaries, a
  stage-2 repeat earned by the 2 units (as in Satsuma's iteration 1), and
  36.7 s against c2's 36.1 s.
- **Remainder.** The AMO count (402 against 555) is the open ULC-count gap
  already recorded for Rank754.

## Same-CPU pairs (c2 vs fix)

| Row (limit) | c2 | fix | c2 / fix conflicts | SR (ULC) c2 / fix | structured ms c2 / fix |
|---|---|---|---|---|---|
| clqcl_50_6_5 (600) | UNSAT 3.2, ✓ | UNSAT 3.3, ✓ | 1,214 / 1,214 | 759 / 759 | 3,120 / 3,230 |
| clqcl_30_11_10 (600) | UNSAT 83.6, ✓ | UNSAT 78.8, ✓ | 4,586,071 / 4,586,071 | 680 / 680 | 2,215 / 2,169 |
| clqcl_100_6_5 (600) | UNSAT 30.5, ✓ | UNSAT 31.4, ✓ | 213 / 213 | 1,513 / 1,513 | 30,417 / 31,277 |
| MVRoundRobin_n16_d10_v2 (1,200) | UNSAT 2.7, ✓ | UNSAT 2.5, ✓ | 1 / 1 | 21,827 (21,600) both | 2,609 / 2,392 |
| MVRoundRobin_n16_d10_v3 (1,200) | UNSAT 5.9, ✓ | UNSAT 5.2, ✓ | 1 / 1 | 48,923 (48,600) both | 5,816 / 5,059 |
| MVRoundRobin_n20_d10_v2 (1,200) | UNSAT 7.3, ✓ | UNSAT 6.3, ✓ | 1 / 1 | 34,459 (34,200) both | 7,156 / 6,212 |
| cliquecolouring_n41_k5_c4 (1,200) | UNSAT 1.0, ✓ | UNSAT 1.0, ✓ | 43 / 43 | 487 / 487 | 951 / 964 |
| EDP3-11000 (1,200) | SAT 58.5, ✓ | SAT 61.5, ✓ | 244,035 / 244,035 | 1 / 1 | 1,635 / 3,586 |
| SC25_Timetable_C_392 (1,200) | SAT 36.1, ✓ | SAT 36.7, ✓ | 187,872 / 198,394 | 2 / 404 (402) | 685 / 3,805 |
| bphp_p23_h22 (600) | UNSAT 536.7, check window expired | **UNSAT 2.4, ✓** | 38.6M / 239,889 | 233 / 233 | 142 / 141 |
| Break_20_72 (1,200) | SAT 656.3, ✓ | timeout | 20.0M / 38.4M | 0 / 5 (1) | 340 / 763 |
| ntil-90d-40 (1,200) | SAT 90.6, ✓ | timeout | 2.0M / 23.5M | 1 / 1 | 2,841 / 2,652 |
| Rank754 Timetable_C_495 (5,000) | timeout | **SAT 3,736.0, ✓** | 44.2M / 35.0M | 5 / 505 (501) | 966 / 4,834 |

✓ means the model or proof was checked by the runner. Each row is one draw per
arm.

- **bphp_p23_h22 and Rank754.** The A–D gains are kept: bphp 2.4 s, and
  Rank754 SAT 3,736 s with the model verified (bcd3 3,916 s).
- **Break_20_72 and ntil-90d-40.** These are not affected by the repair:
  - Their fix startup equals c3's (Break: 5 SR, 1 applied, 1 ULC; ntil: 1 SR).
  - c3 already timed out on both in its WV100 gate (37.0M and 22.8M
    conflicts).
  - The five-copy shuffle probes show both as original-file luck under A–D
    and one-round pure (`../2026-10-08-pure-round-shuffle-probe/README.md`).

## Validation

Focused Debug and ASan/UBSan CTest on the final source (`pins6`,
`d4cdd2ac…`):
- **CTest.** 30/30 pass in both builds: sequence, scheduling, online
  inprocess, strategy config, symmetry backend, orbitopal group, structured
  handoff, SR proof encoding, ULC RAT, micro proofs and others.
- **DSR fixtures.** The structured-root fixtures php(5,4), php(6,5), php(7,6)
  and php(6,5)+60k padding (the large domain) pass `check-dsr --every-lemma`
  (dsr-trim + Trestle) in both builds.
- **Full Debug CTest.** Before the API test edit: 354/355. The one failure,
  `api_n1`, is the retired-cube expectation described above.
- **API tests after the edit** (`apidiag2`, Debug). `test_online_api` and
  `test_api_c` pass both on base src (c3) and with the repair.

## Open items

- **Johnson actions.** Port `detect_johnson_arity2` with orbitopal fixing of
  Johnson columns. This is the owner mechanism for the clique-colouring
  family: 0.1 s and 2 iterations, against Tactix's 9 generic passes and up to
  31 s of startup on clqcl_100.
- **One refinement per pass.** Share one snapshot and refinement graph across
  the sibling orbitopal steps of a pass. This is the remaining ≈2 s on EDP3 and
  the earlier oski06 and tt496 overheads.
- **Split counter scope.** Tactix keeps one split counter per detector step;
  Satsuma keeps one per pass across row-column and row.
- **ULC AMO count.** Tactix makes 402 binaries on Timetable_C_392 and Satsuma
  555, the same gap as Rank754.
