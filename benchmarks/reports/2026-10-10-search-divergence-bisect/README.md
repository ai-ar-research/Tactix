# Tier D lost SAT rows: first divergence and bisection

Where the trajectories of the no-MAB baseline `d170b4a5` and the final code `50c619192` first
differ on the 17 SAT rows the baseline solved and the final code lost
([comparison](../2026-10-10-tierd-no-mab-vs-final/README.md)), which commit causes each
divergence, and what would repair it. No source was changed.

## Result

Four mechanisms account for all 17 rows. Each divergence starts at startup or within the first
7,725 conflicts. The 10-01–10-02 shared-execution refactor, `a49191bfa`, the Walk-guidance
commits `0a35d3f74`/`7e202a0d5` and the restart-only theory `b0077afea` have no effect on any of
the 17 rows within the first 10,000 conflicts.

| Mechanism | Commit | Rows | First differing event |
|---|---|---|---|
| M1 FastEliminate self-strengthening | `efdced45a` | par32-4, sha1 zeroOut_18, Break_18_32, bp4_TCO_CSO_ZR, connm n1200, pcmax m15, SDP_126_17, SDP_140_19, bp4_TCO_CSO_IXA_LP_ZR, Circuit_multiplier36, as-p51 (after `8d13ca87a`), pcmax m12 (first early divergence only; see M2) | startup `bve_pair` outcome `kept` → `fastel-strengthened` |
| M2 onehot hint no longer installed before Walk | `84e8d197a` | st_540_25_12_12_9; pcmax m12 end to end (its trajectory diverges at conflict 2,694 and the `efdced45a` parent already times out) | `phase_pick` var 238 at conflict 2,788 |
| M3 residual search-seed handoff off | `3ecfa8dfe` (net, with `0a15ecaf1`) | noL-11-8, bp4_LPI_FPBEQ_ZR | first VMTF pick at conflict 0 |
| M4 singleton forcing SR unit admitted | `d576abaeb` | vdwb_k6_n300, 6g_6color_366_050_04 | startup `backbone_scheduled` count, which reflects one more fixed variable |

Full runs confirm the attribution. Each culprit's parent solves its rows with d170's exact
conflict count; pcmax m12 is the exception, already lost at `84e8d197a`. Each culprit alone
times out at 6,000 s, or solves only after 5,000 s with 2–5× the conflicts.

Only M1 is a Kissat port. M2–M4 were intended policy changes; M3 and M4 follow the
Satsuma-practice decisions already recorded. 8ce068dbf, which restored the FastEliminate guard
after c2.1, moves noL-11-8, bp4_LPI and 6g back onto the trajectories above; c2.1 itself differs
from the final code only on those three rows.

## Method

- **Pins.** Release pins were built from `git archive` of each commit (`-O3`, the campaign recipe).
  The trace-OFF builds of `d170b4a5` and `50c619192` reproduce the campaign pin hashes exactly
  (`2fc7421f…`, `d540ef32…`).
- **Trace build and determinism.** Trace-ON builds (`TACTIX_ENABLE_EVENT_TRACE=ON`) print the
  same final statistics as trace-OFF on pj2013_k9 and noL-11-8. Two runs of each endpoint on all
  17 rows give byte-identical traces.
- **Signal.** `TACTIX_EVENT_TRACE_TRAJECTORY_ONLY` records decisions, phase picks, conflicts,
  learned clauses and backbone rounds; runs stop after 10,000 conflicts (20,000 at the
  endpoints). Two traces count as the same if their records agree on event, conflicts, decisions,
  level, mode and payload. Clause `ref`/`id`, which are arena representation, and propagation
  counts are excluded.
- **Startup.** `TACTIX_EVENT_TRACE_COMPACT_PROBE` traces localize startup differences (Factor,
  BVE/FastEliminate). `TACTIX_EVENT_TRACE_BVE_EXTERNAL` gives pair-level BVE traces.
- **Bisection.** The lineage is the 221 src commits `d170b4a5..bfaf65491` (excluding the
  MAB-enable commit `387967628`, reverted by the next one), c2's two commits `76e161b24` and
  `1aad09b07`, and `6e7a24959..50c619192`. Two rounds sufficed: 20 spread commits, then every
  commit in the four transition intervals. Two commits did not build trace-ON (`ebbc73043`,
  `a854e981e` in round 1; `acac9843d`, `2b2d7ecb8` in round 2); their neighbours bracket the
  transitions.
- **Culprit and parent.** Every culprit was compared with its parent on the affected rows; each
  parent matches the reference and each culprit diverges. Full runs (no stop, 6,000 s) of each
  parent test whether it still solves with the baseline trajectory; full runs of each culprit test
  whether the culprit alone loses the row.
- **Resources.** All runs on `cpu-dense-hwulab-q` via owned sbatch wrappers.

## First divergence per row (d170 vs final)

"Startup" is the first difference in the compact startup trace. "Search" is the first differing
trajectory record.

| Row | Startup difference | Search: first differing record | Commit |
|---|---|---|---|
| par32-4 | FastEliminate pivot 2702 pair 2 strengthened; candidate ticks 141 → 139 | conflict 0, decision 70: `phase_pick` var 2606 → 2844 (default phase, different VMTF pick) | `efdced45a` |
| sha1 zeroOut_18 | FastEliminate pivot 13105 now eliminated | conflict 0, first decision: var 13405 → 13387 | `efdced45a` |
| st_540_25_12_12_9 | none | conflict 2,788: `phase_pick` var 238 target/saved +1 → −1 | `84e8d197a` |
| Break_18_32 | FastEliminate pivot 30016 ticks 77664 → 77666 | conflict 7,725: backbone round effort 15629 → 15633 | `efdced45a` |
| bp4_TCO_CSO_ZR | FastEliminate pivot 81831 ticks | conflict 2: decision 42 var 73507 → 29705 | `efdced45a` |
| connm n1200 | FastEliminate pivot 413 ticks | conflict 1: conflict clause literal order differs | `efdced45a` |
| noL-11-8 | none (fastel root-suppressed in both) | conflict 0, decision 13: var 1347 → 1349 | `3ecfa8dfe`/`0a15ecaf1` |
| pcmax m15 | FastEliminate pivot 369 pair 4 strengthened; candidate now eliminated | conflict 96: conflict clause literal order differs | `efdced45a` |
| SDP_126_17 | FastEliminate pivot 2101 now eliminated | conflict 0, decision 57 | `efdced45a` |
| SDP_140_19 | FastEliminate pivot 10658 now eliminated | conflict 0, decision 11 | `efdced45a` |
| bp4_LPI_FPBEQ_ZR | none | conflict 0, first decision: var 75610 → 78407 | `3ecfa8dfe`/`0a15ecaf1` |
| vdwb_k6_n300 | final run has no FastEliminate events | `backbone_scheduled` 600 → 598 (record 1) | `d576abaeb` |
| bp4_TCO_CSO_IXA_LP_ZR | FastEliminate pivot 88204 ticks | conflict 15: decision 97 | `efdced45a` |
| Circuit_multiplier36 | FastEliminate pivot 1108 ticks | conflict 484: learned clause shrinks to 19 literals instead of 20 | `efdced45a` |
| 6g_6color_366_050_04 | Factor seed differs (after the extra unit; fastel skipped) | `backbone_scheduled` 154656 → 154654 | `d576abaeb` |
| as-p51-l19-o8 | Factor seed differs | `backbone_scheduled` 71938 → 71790 (`8d13ca87a`), then conflict 1 (`efdced45a`) | `8d13ca87a`, `efdced45a` |
| pcmax m12 | FastEliminate pivot 357 ticks | conflict 1: conflict clause differs (`efdced45a`); `84e8d197a` alone diverges at 2,694 | `efdced45a` |

Final vs each stage (first 10k conflicts): the final trace equals the `efdced45a` trace on the 12
M1 rows, the `84e8d197a` trace on st_540, the `0a15ecaf1` (and `8ce068dbf`) trace on noL-11-8 and
bp4_LPI, and the `d576abaeb` trace on vdwb and 6g (`vs50c.tsv`).

## Mechanisms

### M1 — `efdced45a` Port FastEliminate self-strengthening before cap (12 rows)

**What changed.** In startup FastEliminate, when the raw product exceeds `fastelim` (8), the
feasibility scan now does what SC2024 `fastel.c`'s `can_fast_resolve_binary_large` and
`can_fast_resolve_large_large` do. If a resolvent subsumes a large parent, the parent is replaced
by the resolvent and the pair is not counted toward the cap.

**Evidence.** Pair traces of the parent `55d0160f0` and `efdced45a`:
- par32-4, pivot 2702, pair 2: binary (2702, −2035) × ternary (−2702, 2137, −2035) goes from
  `kept` to `fastel-strengthened`. The ternary becomes (2137, −2035).
- pcmax m15, pivot 369, pair 4: same pattern, and the candidate flips from not eliminated to
  eliminated.

The startup formula therefore differs by a few clauses (or one eliminated variable). The search
shows it as soon as a decision touches it: a different VMTF head or a different conflict clause
at conflicts 0–96. Circuit_multiplier36 shows it at 484; on Break_18_32 the first difference is a
backbone round's tick count at 7,725.

**Intent and fidelity.** A deliberate port. The observed transformation matches the reference
semantics: binary × large with the binary's other literal in the large clause strengthens the
large clause, the strengthening is counted, and the pair is not resolved. The commit was not
meant to be behavior-preserving.

**Causality.** Full runs of the parent `55d0160f0` reproduce d170's exact conflict counts on every
M1 row except as-p51, which already includes `8d13ca87a` and still solves:

| Row | d170 conflicts | Parent `55d0160f0` |
|---|---|---|
| par32-4 | 25,581,489 | 25,581,489 |
| sha1 | 7,690,141 | 7,690,141 |
| bp4_TCO_CSO_ZR | 6,540,977 | 6,540,977 |
| connm | 18,331,997 | 18,331,997 |
| pcmax m15 | 56,564,185 | 56,564,185 |
| SDP_126 | 4,072,338 | 4,072,338 |
| SDP_140 | 11,153,796 | 11,153,796 |
| IXA | 33,810,940 | 33,810,940 |
| Circuit_multiplier36 | 6,634,307 | 6,634,307 |

So the 193 lineage commits before `efdced45a` are trajectory-neutral end to end on these rows.
Break_18_32 also reproduces exactly (60,655,608 conflicts). as-p51 already includes `8d13ca87a`
and still solves (18,661,734 conflicts).

pcmax m12 is the exception: the parent times out at 6,000 s, at 99,352,687 conflicts against
d170's 84,275,281. The row already diverged at `84e8d197a` (conflict 2,694) and was lost before
`efdced45a`.

`efdced45a` alone, with every later commit absent, loses the rest:

| Row | `efdced45a` full run | d170 conflicts |
|---|---|---|
| par32-4 | timeout at 97,766,576 conflicts | 25,581,489 |
| sha1 | timeout at 17,350,772 | 7,690,141 |
| Break_18_32 | timeout at 69,380,165 | 60,655,608 |
| bp4_TCO_CSO_ZR | timeout at 45,285,926 | 6,540,977 |
| connm | timeout at 66,593,117 | 18,331,997 |
| SDP_126 | timeout at 23,403,218 | 4,072,338 |
| SDP_140 | timeout at 18,241,988 | 11,153,796 |
| IXA | timeout at 49,651,362 | 33,810,940 |
| pcmax m15 | SAT at 115,746,068 conflicts, 5,456 s | 56,564,185 |
| Circuit_multiplier36 | SAT at 33,994,396 conflicts, 5,883 s | 6,634,307 |
| as-p51 | SAT at 48,557,476 conflicts, 5,863 s | 20,651,512 |

The three late solves would still miss the 5,000 s limit; each needed 2–5 times d170's conflicts.
The parent took 2,466 s on pcmax m15 against d170's 2,319 s on cpu-q, so the hosts run at similar
speed. Each arm is one deterministic run, but on most rows the culprit's conflicts at timeout far
exceed d170's conflicts to solve, so these are not near-limit losses.

**Fix proposal (fidelity repair, not a revert).** The coupled mechanisms around Kissat's
strengthening are not reproduced:
- Kissat materializes the strengthened clause with `kissat_new_irredundant_clause`. It is
  appended to the arena and connected at the end of every occurrence list, and its literals are
  marked added (subsume flags).
- Kissat retires the old clause with `kissat_mark_clause_as_garbage`. That calls
  `kissat_mark_removed_literals` on all of its literals, which re-arms their `eliminate` flags for
  the next fastel round and for later BVE.
- Tactix routes the strengthening through `kernel_replace_clause`. That inherits the old clause's
  position (`kernel_inherit_clause_position`, the model for in-place shrinking) and arms BVE retry
  only for the literals that disappear.
- Related and pre-existing: `initial_fast_elimination_attempted_` is set once per startup phase
  and never re-armed. Kissat clears `eliminate` after each try but re-arms it whenever a neighbour
  clause is removed, so later rounds can retry a variable.

Proposal: give the fastel strengthening path new-clause plus garbage semantics (append order,
removed marks on all old literals, added marks on the new clause) and re-arm attempted variables
the way Kissat's `eliminate` flag is re-armed. Validate per-candidate outcomes and the
post-FastEliminate formula digest against a Kissat fastel trace (the `kissat-ticktrace` or
`kissat-bve-full-heap` variants) on par32-4 and pcmax m15 before any gate. Recovering d170's
exact trajectory is not the target; the d170 trajectory came from an incomplete port.

### M2 — `84e8d197a` Retire semantic constructors behind certified actions (st_540)

**First event.** Conflict 2,788 is the first restart after the switch to stable mode; the
rephase Walk slot runs there. The second `phase_pick` of var 238 reads target=+1, saved=+1 in the
parent `a2f90364c` and −1/−1 in `84e8d197a`. The journal shows only a `Restart` there, and nothing
else in that window.

**Phase source.**
- Parent: early Lucky runs the onehot model (`c onehot 1 calls, 0 domains`), which publishes a
  LocalSearch hint. Before each rephase Walk, `LocalSearchGuidance` copies that hint into saved and
  target. In 240 s: `localsearch 1 hints offered, 14 applied`.
- `84e8d197a` removes the early onehot call (`0 offered`). It also changes guidance to a private
  Walk seed that never writes saved/target unless Walk improves.
- Later commits offer a hint again. The final code reports 1 offered (79 phases) and 0 applied,
  because `555ecb4b0` removed DefaultPolicy structural Walk seeding. Its trajectory equals
  `84e8d197a`'s.

**Lead from the Tier D statistics.** This refutes `a49191bfa` as the cause: `a2f90364c`, the
parent of `84e8d197a`, is identical to d170, and `a49191bfa` comes later. The lead's mechanism
(hint applied before, not after) is confirmed.

**Intent.** Intended. The hint install is not a Kissat mechanism, and the hint came from a
zero-domain onehot model. d170's st_540 solve depended on a degenerate structural phase import.

**Causality.** The parent's full run solves with exactly d170's 66,295,216 conflicts.
`84e8d197a` times out at 6,000 s, at 102,096,890 conflicts.

**Fix.** None for fidelity. Installing structural hints into saved/target before Walk is a policy
choice that DefaultPolicy declined; it is still offered through the API.

### M3 — residual search-seed handoff off (noL-11-8, bp4_LPI)

**What changed.** `3ecfa8dfe` set `structured_root_residual_search_handoff` to false.
`0a15ecaf1` reverted the cut-point part of the same commit. d170 rebuilt the activation and import
seed from the post-symmetry residual formula (`c handoff 1 residual seeds, 1089 variables` on
noL-11-8). The final code keeps the parser's seed (no handoff line).

**First event.** At conflict 0 the decisions use default phases but pick different VMTF heads.

**Causality.** Full runs of `37226faa7`, the parent of `3ecfa8dfe`, reproduce d170 exactly
(noL-11-8 1,483,023 conflicts; bp4_LPI 16,993,100). `0a15ecaf1`, which carries the net change,
times out on both: noL-11-8 at 102,580,215 conflicts, bp4_LPI at 47,151,723.

**Intent.** Intended. This is the user's "faithful original seed" decision and the recorded
noL-11-8 exception.

**Fix.** Only if the gap must close: emulate Satsuma's actual hand-off, that is, the residual
CNF's clause presentation and Kissat's first-occurrence import order. The present live-formula
reseed does not reproduce that; that is why it was turned off.

### M4 — `d576abaeb` Always admit checked symmetry units (vdwb, 6g)

**What changed.** A singleton forcing SR row, formerly withheld under `RequireCompoundProgress`,
is now published.

**First event.** One more root-fixed variable: `backbone_scheduled` 600 → 598 on vdwb and
154656 → 154654 on 6g. On 6g the unit also triggers the post-symmetry FastEliminate guard, so
fastel is skipped (19 → 0 eliminated). The guard also fires on vdwb.

**Causality.** Full runs of `0a15ecaf1`, the parent of `d576abaeb`, reproduce d170 exactly
(vdwb 13,339,148 conflicts; 6g 1,644,645). `d576abaeb` times out on both: vdwb at 64,232,737
conflicts, 6g at 11,536,155.

Counters confirm the guard fires (`fastel 1 root-suppressed`) on both rows. On vdwb the parent's
fastel had eliminated nothing anyway (one minority decline), so on that row the unit itself, not
the skipped fastel, changes the trajectory.

**Intent.** Intended and Satsuma-faithful: Satsuma emits the unit and hands Kissat `fastel=0`.

**Fix.** Not the unit. With the unit and the guard we match Satsuma's pipeline except for the
search seed (M3: Satsuma's Kissat seeds from the residual CNF). M3's fix is therefore also the
coupled mechanism for M4.

### Secondary — `8d13ca87a` (as-p51)

Unique-literal AMO SR rows are now re-authenticated at publication. That changes the startup
units on as-p51 (148 fewer backbone candidates, record 1), and M1 then diverges at conflict 1.
The change was intended (soundness), and the parent still solves.

## Transient divergence (not in the final code)

`df450efa5` and `1b38deed7` (10-04) diverged at conflict 0 on six rows. By `46a7db1cd` those rows
match d170 again, so this transient change does not reach the final code.

## Evidence

All under `benchmarks/probes/scratch/search-divergence-bisect/`.

**Wrappers (sha256 prefix).**

| File | sha256 | Role |
|---|---|---|
| `build.sbatch` | `9aa7c0fe5b510b5c` | builds |
| `build2.sbatch` | `d4531add5434af2c` | rule-journal builds |
| `probe.sbatch` | `7ca09cb17bcaa880` | endpoint probes |
| `probe2.sbatch` | `dfffb3a80b567cc4` | bisection rounds |
| `probe3.sbatch` | `e2d7b0eb06d424f2` | startup traces |
| `probe4.sbatch` | `7d9a99fbf50b0312` | pair/window traces |
| `probe5.sbatch` | `fa1f141c584c2c5b` | full runs and journals; proof to `/dev/null`, checked equal to a real proof file |
| `cmp.py` | `d8bac2792e6453ac` | trace comparator |
| `round.sh`, `round2.sh`, `classify.sh`, `classify2.sh` | — | bisection drivers |

**Data.**

| Path | Contents |
|---|---|
| `rows.tsv` | the 17 rows |
| `s0/` | endpoint traces, d170 and final, two replicates each |
| `onoff/` | trace-ON/OFF equivalence |
| `firstdiv.tsv` | first divergence d170 vs final |
| `r1/classify.tsv`, `r2a`–`r2d/classify.tsv` | bisection tables |
| `vs50c.tsv` | final vs each stage |
| `s1/` | startup compact traces |
| `s2/` | BVE pair traces and the st_540 window |
| `s3/` | rule journals and statistics for `a2f90364c`/`84e8d197a` |
| `full/` | full-run statistics |

**Jobs.**

| Jobs | Purpose |
|---|---|
| 1765727, 1765731 | endpoint builds |
| 1765733, 1766133 | endpoint traces |
| 1766134 | ON/OFF check |
| 1766253/4 | round 1 |
| 1766758–1766802 | round 2 |
| 1766274 | startup traces |
| 1767675, 1767692 | pair and window traces |
| 1767845, 1767926 | journal |
| 1767886 | full runs |

## Limits

- The bisection signal covers the first 10,000 conflicts. Commits that act only later are masked
  on a row once an earlier commit has diverged.
- The full parent runs show that the M1–M4 parents still follow d170 to the solution, but they do
  not test later commits in isolation.
- The M1 fix is a fidelity proposal to verify against a Kissat trace. It is not a prediction that
  the 12 rows recover; their trajectories are chaotic in the startup formula.
