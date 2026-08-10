# Tier-A convergence with Kissat

**Date:** 2026-08-09

**Tactix range:** `167d185` through `ca8b341`, plus the validated
`ORDER-REDUCE-CADENCE-1` increment

**Kissat reference:** `8af8e56f174b`, plus the diagnostic-only event-trace
overlay

## Outcome

The Tier-A investigation changed from aggregate benchmarking into a
first-divergence loop: run one cheap instance, compare a portable execution
stream, classify the first mismatch by strength, repair it only when Tactix is
not strictly better, and repeat from the beginning. That loop has moved the
hard `tr-n300-s055` case from an unexplained timeout/regression to this state:

- Tactix and Kissat have identical portable event streams for 286,987 records,
  reaching conflict 1,000 and decision 1,384. This includes
  lifecycle order, decisions, watch visits, implications, selected conflicts,
  raw/sorted/shrunk/final learned clauses, conflict numbers, levels, and the
  post-analysis ranking snapshots.
- Exact OTFS reuse takes Tactix to 25.11 s, 760,223 conflicts and 45,223,940
  propagations, from 29.03 s / 815,632 / 48,240,090 on the preceding pin.
  Closing the next reduction-cadence edge changes the final trajectory to
  23.97 s, 850,793 conflicts and 50,653,751 propagations. Kissat takes 22.78 s,
  927,770 conflicts and 41,180,601 propagations. Every DRAT proof verifies.
- The 500,000-record extension localizes the next divergence exactly at
  conflict 1,000: Kissat's `reduceinit=1000` admits Reduce before the mode
  switch, while Tactix's deliberately retained arithmetic cadence starts at
  2,000 and therefore switches first. This is the previously measured accepted
  reduction-cadence deviation, not a new Analyze bug or an unexplained event.
- The faithful cadence is faster and lower-memory on both selected Tier-A
  guards, so it replaces the arithmetic default. It also takes more search
  work: final Tactix performs about 23.0% more propagations despite 8.3% fewer
  conflicts than Kissat. The new trace confirms common Reduce→SwitchMode order
  and localizes the next edge inside Reduce: Tactix retains 905 redundant
  clauses where Kissat retains 633. Candidate eligibility/retention is now the
  only open frontier—not Tier B or Tier C.

This is semantic convergence, not a claim that Tactix should or does reproduce
Kissat's instruction-by-instruction trace. The solvers have different arenas,
proof plumbing, counters, simplification implementations, and intentional
strict-subclause behavior. The gate compares state that can legitimately be
made common and requires every difference on that surface to be localized and
explained.

## Why these Tier-A fixtures

The initial 200-instance Tier-A run identified cheap failures without paying
for broader tiers. The investigation then deliberately narrowed the surface:

| fixture | role |
|---|---|
| `tr-n170-s053` | UNSAT startup/search fixture; exposed analysis-side trail reuse and early ranking changes |
| `tr-n220-s013` | SAT companion; exposed initial activation order while retaining an independent model oracle |
| `tr-n280-s052` | UNSAT trace localizer; cheap enough to repeatedly raise the detailed-event cap |
| `tr-n260-s002` | production trajectory gate; sharply exposed the OTFS duplicate-watch/lifetime bug |
| `tr-n300-s055` | remaining hard production gate and the final one-at-a-time trace target |

Each traced UNSAT result was checked with `drat-trim`; the SAT companion was
checked against the original CNF with a complete assignment. Solver execution
and proof checking ran under SLURM on immutable build directories. Runtime
comparisons used sequential arms on the same node. Because the measurements
come from several windows, conflict/decision/propagation counts are the
deterministic evidence; small timing differences between separate windows are
directional only.

## The diagnostic mechanism

`tools/solvers/variants/kissat-event-trace/` adds a narrow overlay to the local
Kissat tree. Tactix's corresponding implementation is
`src/policies/event_trace.cpp`, and `tools/bench/compare_event_trace.py`
normalizes both TSV streams using permanent external variable identities.
The common vocabulary covers:

- startup, early/late Lucky, preprocessing, restart, and other macro events;
- decisions, ordered watch visits, reasoned implications, and conflict clauses;
- raw 1-UIP, level-sorted, post-shrink, and final learned clauses;
- conflict/decision/level trajectory and post-analysis ranking state.

The comparator writes a bounded-context JSON first-divergence record. A
divergence cannot pass `--require-explained` without a disposition, cause, and
evidence handle. During final review, the comparator itself was tightened:
snapshotless `watch`, `imply`, and learned-stage rows are now treated as detail
records, so their deliberately absent state fields cannot create a false
macro-state mismatch. `analysis_end` remains snapshot-bearing and continues to
compare ranking/formula state. The frozen 250k streams now report, without an
exception or disposition:

```text
first divergence: no divergence
macro order: no divergence
macro state: no divergence
trajectory: no divergence
portable semantic streams are identical
```

Job `1605945` then raised the cap to 500,000. The first record mismatch is
286,987, where Tactix emits `switch_begin` and Kissat emits `reduce_begin`, both
at conflict 1,000 with the same 1,384 decisions, level 12, ranking, and formula
population (1,278 irredundant / 957 redundant clauses). The explained record
cites the already-banked reduction cadence. The later watch-blocker delta is a
consequence of Kissat having deleted 324 redundant clauses before entering
stable mode, not another root.

After the cadence became default, job `1605950` repeated the trace. Both
solvers now emit `reduce_begin` then `reduce_end` then `switch_begin` at
conflict 1,000. The first mismatch moves one record to `reduce_end`: Tactix has
905 redundant clauses, Kissat 633, from the same pre-reduce population of 957.
The following watch-order delta is downstream of those different survivors.
This opens `REDUCE-CANDIDATE-1`; it does not reopen the cadence/precedence fix.

Production Tactix is built with `TACTIX_ENABLE_EVENT_TRACE=OFF`; the trace
class becomes inline no-ops and the implementation is not linked. For the
final binary, `nm` finds no `EventTrace`/`event_trace` symbol and `strings`
finds no trace schema, environment variable, or learned-stage label. Thus the
mode adds no branch, state scan, or string footprint to the production path.

## Divergence-to-repair ledger

The important point is not that every difference was copied. At each edge the
question was whether existing Tactix behavior could be strictly better. A
missing reference event, a lost strict subclause, a duplicate implied clause,
or an earlier cutoff was repaired. A provably stronger derived clause was
retained.

| first localized cause | strength judgment and disposition | increment |
|---|---|---|
| CaDiCaL-style analysis-side trail reuse changed the first backtrack relative to Kissat | Neither reference behavior dominated. The project tie-break selects Kissat; the CaDiCaL behavior remains optional. | `SP-CHRONO-1`, `92cf488` |
| Tumble activation did not preserve surviving file-literal order | Not stronger; it only changed the initial VMTF/EVSIDS order. | `ORDER-TUMBLE-1`, `ce79d3e` |
| Eliminate remained due after preprocessing and ran after conflict 1 | Lost reference scheduling wire, not a strength improvement. | `ORDER-ELIM-INIT-1`, `f2e5ba1` |
| Reason-side bump admission used the unbacked rate 20 instead of Kissat's 10 | References conflict; Kissat tie-break applies. | `ORDER-BUMP-RATE-1`, `d43ce81` |
| Learned-clause watch selection/order differed | No formula-strength gain; it changed later propagation order. | `1457d78` |
| Input clauses were stored in a different order and large watches survived dense phases in mutated order | No strength gain; both were hidden search-state order. | `ORDER-WATCH-1`, `b8769a9` + `0140635` |
| A lower learned literal did not close a shrink walk | Tactix lost a Kissat-valid strict-subclause derivation, so repair was mandatory. | `ANALYZE-SHRINK-1`, `447f1d5` |
| Recursive minimization stopped at a level absent from the learned clause | Earlier rejection could only retain a redundant literal. | `ANALYZE-MIN-1`, `4fc74d7` |
| Learned blocks were applied high-to-low instead of Kissat's low-to-high state-flow order | At conflict 352 Kissat learned a strict subclause; Tactix was not stronger. | `ANALYZE-BLOCK-ORDER-1`, `3d297e5` |
| OTFS reanalysis did not increment the global conflict clock | A lost event shifted every conflict-based deadline. | `ANALYZE-OTFS-COUNT-1`, `bb0f17f` |
| A fresh OTFS replacement retained a duplicate old reason and a different watch position | The replacement entails the old clause; retaining both added work, not strength. | `ANALYZE-OTFS-WATCH-1`, `4a08214` |
| Minimization used 900, recursively charged binary links, and rejected at the wrong boundary | Earlier cutoff only retained literals. Kissat's exact 1000 semantics won the reference tie. | `ANALYZE-MIN-DEPTH-1`, `7a52c53` |
| After OTFS, Tactix learned an exact copy of the already-materialized replacement | Exact duplicate is not stronger. Reuse it, but retain a post-minimization strict subclause when one exists. | `ANALYZE-OTFS-DEDUPE-1`, `ca8b341` |
| Kissat reduces at conflict 1,000 before switching; Tactix's arithmetic cadence is not due until 2,000 | The old default is not strictly better: on the current guards it uses less search work but more wall time and memory, and it loses the required order edge. The faithful cadence+ramp becomes default; arithmetic/flat remains the rollback arm because older Tier-B/C evidence conflicts. | `ORDER-REDUCE-CADENCE-1`, `1605945`–`1605948` |
| The common first Reduce retains 905 Tactix clauses versus 633 Kissat clauses | Open strength question. Both sets are redundant, so keeping more can improve propagation but increases memory/watch work; neither set logically strengthens the original formula. Instrument candidate rejection classes before choosing the Kissat population or accepting a stronger/weaker retention deviation. | `REDUCE-CANDIDATE-1`, frontier from `1605950` |

The trace instrumentation commits (`586c23a`, `687ba8f`, `8288e73`,
`4513902`, `cf515ac`, `2039ee1`) are diagnostic support, not solver-strategy
changes.

## Trace milestones

| cap / anchor | first mismatch before repair | result after repair |
|---|---|---|
| first decisions | trail-reuse, activation, elimination-clock, and bump-admission differences | common startup and early decision trajectory |
| conflict 128 / 50k records | post-shrink clause differed despite identical raw/sorted clauses | 76,216 watch visits, 20,644 implications and 370 analysis results match |
| conflict 352 / 100k records | Kissat's low-first block state removed `-89,-97`; Tactix retained them | 75,730 watch visits, 21,097 implications and 380 analysis results match |
| conflict 487 / 200k records | OTFS reanalysis missed conflict-clock increment | repeated analysis and schedules match |
| conflict 490 / 200k records | fresh OTFS representation added one later watch scan | exact shared watch order and replacement blocker; old reason retired |
| conflict 445 on `tr-n300-s055` | Tactix learned the 18-literal OTFS replacement a second time; Kissat reused it | no extra learned-stage event or duplicate clause |
| 250k records / conflict 889 | no portable mismatch remains | macro order, macro state and detailed trajectory all identical |
| record 286,987 / conflict 1,000 | Tactix switches mode; Kissat first reduces under its earlier default cadence | localized to the previously accepted reduction-cadence deviation |
| record 286,988 / conflict 1,000 | after common Reduce, Tactix has 905 redundant clauses and Kissat 633 | open reduction candidate/retention frontier; later watch order is downstream |

The conflict numbers in different rows refer to the fixture named by the
surrounding investigation: the 128/352/487/490 sequence is
`tr-n280-s052`; conflict 445 and the 889-record-cap endpoint are
`tr-n300-s055`.

## Production trajectory

The cumulative path was intentionally non-monotone. Some wrong behavior was
faster by luck; that was not a reason to keep a version that lost a reference
wire or failed to derive a strict subclause.

| cumulative Tactix state | `tr-n260-s002` conflicts / time | `tr-n300-s055` conflicts / time |
|---|---:|---:|
| pre-analysis-repair baseline | 204,250 / 4.8 s | 746,064 / 24.9 s |
| correct learned-block order | 240,661 / 5.99 s | 960,672 / 35.92 s |
| OTFS conflict clock | 284,783 / 7.09 s | 854,300 / 29.97 s |
| OTFS watch/lifetime | 204,614 / 4.81 s | 815,632 / 28.76 s |
| exact minimization depth | not rerun | 815,632 / 28.63 s |
| exact OTFS replacement reuse | not rerun | 760,223 / 25.11 s |
| Kissat reduction cadence + ramp | 235,806 / 5.26 s | 850,793 / 23.97 s |
| Kissat reference | 212,086 / about 3.85 s | 927,770 / about 22.7 s |

The OTFS comparison is job `1605944`. Relative to the prior
pin, exact OTFS reuse removes 55,409 conflicts, 74,325 decisions, 3,016,150
propagations, 55,538 learned clauses, and 3,915,668 proof bytes. The complete
run reuses 195 conflicts and records zero minimization-depth cutoffs. The final
intermediate production binary is
`build/pin-kissat-otfs-round-reuse-v7/tactix`, SHA-256
`e14cb2fa0f32cf5c0463f7986cdaa6c6defe10ff27ec6788f5bc49290ff14144`.

Jobs `1605947` and `1605948` then compare the old reduction default, the
faithful cadence+ramp arm, and Kissat on the two current Tier-A guards. The
faithful arm is 1.06 s faster and uses 23% less peak memory on n300; it is 0.11
s faster and uses 19% less memory on n260. Its higher deterministic work counts
mean this is not a universal-speedup claim. It is enough to overturn the old
default when combined with the exact conflict-1,000 order mismatch and the
project's Kissat tie-break. The historical Tier-B/C loss remains the explicit
reopening evidence, and the arithmetic/flat arm remains available.

The shipped cumulative binary is `build/pin-kissat-reduce-default/tactix`,
SHA-256
`4f143bf3cdd2e1a329145fbcacc59d7e9efbfcbebc043702512718026645ad0a`.
It contains neither event-trace symbols/strings nor the removed
`TACTIX_REDUCE_SQRT` compatibility knob.

## Correctness and regression gates

The cumulative result has these independent gates:

- job `1605942`: assertion-enabled focused Analyze test passes; all 63 Release
  tests pass;
- job `1605943`: Tactix and Kissat both finish UNSAT, both DRAT proofs verify,
  and the 250k portable streams match through conflict 889;
- job `1605945`: both complete proofs verify; the 500k extension remains exact
  for 286,987 records and explains the conflict-1,000 reduction-cadence edge;
- job `1605944`: old Tactix, new Tactix, and Kissat all finish UNSAT and all
  three production proofs verify;
- jobs `1605947` / `1605948`: both reduction arms and Kissat verify on both
  selected Tier-A guards; the faithful arm wins wall time and peak memory;
- job `1605950`: all 63 tests pass; both full traces verify; macro order matches
  through the first Reduce and the next state mismatch is localized at its
  candidate-retention result.

Focused tests preserve both sides of the strength decision. An exact OTFS
replacement is reused and its old reason is retired, while a replacement that
Tactix minimizes to a strict subclause is still learned. Earlier fixtures also
retain singleton-block minimization and root-false OTFS removal when they
produce strict subclauses. Completed neutral comparison arms were removed
rather than accumulated.

## What remains and the next gate

The convergence prefix is strong evidence, but it covers 1,000 of Tactix's
850,793 final conflicts. The first later divergence is now known: reduction
candidate retention. The solvers also retain intentionally non-portable
differences: internal arena identity, proof-chain construction, exact
propagation accounting, and the search cursor are not compared on snapshotless
detail rows. These are not licenses for an unexplained portable decision,
watch, implication, conflict,
learned clause, or lifecycle-order mismatch.

The next one-at-a-time gate is:

1. add diagnostic-only reduction records for each candidate's glue, used age,
   tier classification, reason lock, rank, and keep/delete disposition;
2. compare the first differing candidate at conflict 1,000 against Kissat's
   executable `reduce.c` path and its live per-mode tier thresholds;
3. decide whether Tactix's larger retained set can be justified as strictly
   better. If not, fix it to Kissat; if uncertain after the candidate trace,
   request a policy decision rather than silently selecting a set;
4. rerun `tr-n260-s002` only as a cheap regression guard after another actual
   increment;
5. keep Tier B and Tier C deferred until the remaining Tier-A runtime gap is
   localized or explained.

Relevant focused reports are
`2026-08-09-cross-solver-semantic-divergence.md`,
`2026-08-09-tier-a-watch-and-shrink-parity.md`,
`2026-08-09-tier-a-block-order-repair.md`,
`2026-08-09-tier-a-otfs-conflict-accounting.md`,
`2026-08-09-tier-a-otfs-watch-repair.md`, and
`2026-08-09-analyze-minimize-depth.md` in this directory.
