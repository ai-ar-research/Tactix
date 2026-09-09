# Rank-32 retention drift: common learning, different reason protection

## Outcome and decision

The runtime goal remains unmet. Retained5962 is unchanged at
7603.117/6810.066 = **1.1164527627191867x** fixed Kissat. New
[6037 checked-gate seal](2026-09-08-retained5962-checked-gates.json) establishes
66/66 B28+C5, 40/40 full-view and 40/40 fixed-reference certificates checked and
clean, preserving every original solver-time snapshot. B/C ratios versus
production remain 0.743989304578/0.931615141723. No implementation promotion,
new runtime gain, or current-source fuzz pass is claimed.

The next repair target is Reduce's treatment of retained root reasons, not
learning, Vivify budgeting, heap ties, or the reduction percentage. Full replay
now traces the changed victim set to reduction51/conflict240062. A fixed
root-reason/unit-proof test is still required before changing the solver.

## Validated evidence

All runs below are separate proof-on stopped diagnostics at300000 conflicts:
12 logical CPUs,48000MiB,cpu-dense-hwulab-q,1200s solver,25min allocation,
two full-width tasks,no node constraint. Partial certificates are hashed and
deleted, not accepted as final proofs. All six task records report exit0 and
scratch deletion. The ongoing5784 cpu-q runner,manifests,pins and resources
were not changed. Native reruns add missing causal fields, not timing samples.

| Campaign/job | New evidence | Validation |
|---|---|---|
| 6035/1676555 | Standalone raw/sorted/shrunk/final learned clauses at240062–247206 | Every6024 trace and partial certificate exact |
| 6038/1676601 | Reduce census,eligibility,ranks,victims at245473–270978 | Every6035 trace and partial certificate exact |
| 6042/1676657 | Full Reduce and learned-stage history through270978 | Unchanged6038 traces/proof exact; old Reduce window byte-exact; old learning window exact after sequence normalization |

6035 uses new diagnostic-only6030 clones of6016, with no rule/policy changes.
6038/6042 reuse those immutable binaries without rebuilding. Tactix's full
trace suite passes100/100. Actual native callbacks pass the valid fixture and
five malformed-window controls. Actual Tactix/native fixtures cover inclusive
window endpoints, final-stage dispatch, empty clauses and2048-literal clauses;
the original strict parser rejects eight malformed rows.6044 adds streaming
window, missing-row, malformed-learning and duplicate-multiplicity controls;
its Reduce reader also reproduces all four previously validated6038 tables.
Existing Reduce comparator tests pass. New solver tests were considered:
these observer-only changes use the existing100-test pin; a semantic repair
will require new root/non-root/pending reason and proof witnesses.

## The causal chain

Let L be the canonical learned clause
`-93630,-4349,-4166,-47,51,53,4172`. Both solvers create L identically at
conflict245473,decision65220441,level1549. All four stages at that conflict
match exactly. Across6042's entire observed window, raw,shrunk and final
canonical learning rows agree; `learned_sorted` has intermediate callback
differences, first at17632, with matching raw/final results. These annotations
do not support a learning-rule repair.

At Reduce54/conflict261698, L has glue3,used29,reason0 and rank3019 in both
solvers. Tactix's pool4457 yields3020 victims; native's4456 yields3019.
Tactix deletes L, native retains it. The one extra Tactix candidate is after
L at rank3223 and survives. Thus L's metadata/rank is not the cause.

The expanded history closes the population ancestry:

| Reduce/conflict | Incoming population | Candidate pool T/N | Victims T/N | Outgoing difference |
|---|---|---|---|---|
| 51/240062 | identical21982 copies | 5030/5031 | 3395/3396 | T retains extra A |
| 52/247206 | T extra A | 5484/5483 | 3706/3706 | T extra B |
| 53/254418 | T extra B | 4101/4100 | 2775/2775 | T extra C |
| 54/261698 | T extra C | 4457/4456 | 3020/3019 | T extra C; native extra L |

A=`-449407,-449396,-326375,201473,201492`;
B=`-485618,-364380,-72128,70014,484780`;
C=`-414683,-204813,204815,314009`.
Every incoming multiset difference equals the previous outgoing survivor
difference through55. Common candidate-rank projections at52–55 are exact.
This establishes continuity, not the absence of hypothetical cancelling edits.

All clause populations and victim sets match through50. Earlier reason-flag
differences at37,39,43 do not change candidate or victim sets. At51, the only
candidate-membership difference is `-141144,141120,141161`: size3,glue3,used3
in both arms; Tactix reason1/protected, native reason0/candidate rank4526.
It survives in both, but its admission changes the total victim count and
deletes A only in native. Twenty-four reason/eligibility mismatches are retained
in full in `reason-frontier.json`, rather than hidden by rank differences.

Source-backed inference: native `kissat_flush_and_mark_reason_clauses` flushes
root assignments before locking, and asserts marked assignments have level>0.
Tactix keeps root identities and `lock_reasons` scans the entire retained trail
without excluding level0. This explains the observed protection pattern, but
the trace does not contain assignment levels. Tactix already emits named root
units and its collector retires garbage root reasons to UNIT_REASON; a repair
must establish those proof/lifetime preconditions, not blindly remove locks.

## Next action and limitations

On a NEW5962 successor, construct an actual root-propagated redundant-clause
witness and compare current Tactix/native candidate admission. Preserve
non-root and pending learned-clause locks; test public enumeration purity,
stale payload revalidation, garbage collection and DRAT/LRAT unit provenance.
Implement only the complete general transition, then require the Rank32 causal
replay, B28+C5 and full40 gates. Continue development while proofs check.
Current-source fuzz remains unverified; the denied Claude transfer must not be
retried or bypassed. No fuzz jobs/artifacts were inspected. No manual deletion
or push occurred; retained traces and pins remain reproduction authority.

Primary evidence: `20260908-6035-learned-stage/summary.json`,
`20260908-6038-learned-retention/{summary,population-analysis}.json`,
`20260908-6042-retention-history/{summary,origin-analysis,reason-frontier}.json`
under `benchmarks/runs/`. Helpers6033,6040,6041,6044,6045,6047 own validation;
their outputs are frozen and must not be rerun in place.
