# Checked learned-sort timing and the next Rank32 boundary

## Outcome

Retain isolated6169 for full-view evaluation. Three predeclared, independent
Rank4 pairs all improve over exact predecessor6107: median ratio0.961681,
range0.955738–0.968067 (3.83% median reduction). All six UNSAT proofs are
independently checked and cleaned. Every non-timing statistic and emitted
proof byte/hash matches the unprofiled6107 record and every other replica.
This is a local unit-cost improvement, not closure of the Rank1–50 goal.
Accepted production and the unapproved FlatLists redesign remain unchanged.

## Matched timing

| Wave | Candidate s | Previous s | Ratio |
|---|---:|---:|---:|
| 0 |7.579|7.829|0.968067|
| 1 |7.579|7.930|0.955738|
| 2 |7.529|7.829|0.961681|

[6176 summary](../runs/20260909-6176-learned-sort-rank4-runtime/summary.json)
owns all three pairs, hashes and placements. Jobs1677116/1677118/1677120
each run two concurrent single-threaded tasks,12 logical CPUs and48000MiB
per task, with proof enabled and no sampling profiler or address-space limit.
All were placed on node018 in separate L3 domains; the middle wave reverses
the two arms' L3 domains. No node constraint or throttle was requested.
Each later wave was submitted after expected solver outcomes, not checking.
Bounded accounting confirms all six tasks COMPLETED/0:0.

Per-arm full range/median is0.66% candidate and1.29% predecessor, both below
the2% threshold. Candidate executes about67.18 billion instructions versus
70.49 billion for its predecessor (approximately4.70% fewer); reported clocks
are3.800GHz throughout. Peak solver RSS is28036KiB in each task. These
same-window counters support a cost improvement at unchanged search work;
they do not imply a global propagation/scheduling advantage.

## Cumulative guard and full fifty

[6179 BC checkpoint](../runs/20260909-6171-learned-sort-BC-gate/solver-checkpoint.json)
has66 expected outcomes,64 independently checked, and exact predecessor work
and proof replay for every arm. Candidate/production ratios are B0.726331 and
C0.928345. The two pending C proofs prevent checked gate closure, not further
development. The unchanged102/102 independent Release suites and433 actual
sort fixtures remain the source gate; this turn changes no solver code.

Candidate-only [full6181](../runs/20260909-6181-learned-sort-ranks1-50/manifest.json),
job1677122, is running against the unchanged7239.010-second fixed Kissat50
vector. It uses the existing preparation tool and runner,50 full-width tasks,
12CPU/48000MiB,1200-second solver and16000-second checker budgets,5h allocation.
A new6180 authority supplies the tool's existing `production` ancestor-field
spelling; the frozen6169 manifest is not rewritten. At the
[6186 audit](2026-09-09-sort-timing-validation.json),47 solver records exist,
35 are checked, and all47 exactly replay6107 work/proof. No partial aggregate
or global speedup is reported; source fuzz and memory gates remain open.

The original matched41–50 campaign6108 is now20/20 checked and clean on the
same twelve-CPU/48000MiB contract as the first forty. No cpu-q timing replaces
that reference. The fresh accepted/current-Kissat reevaluation6121 is99/100
checked: all50 Kissat certificates pass; only accepted Tactix Rank43 remains.
[The updated fifty-row table](2026-09-09-fresh-rank50-6185/comparison.md) retains
every rank and suppresses the aggregate until the last certificate is checked.

## Rank32 extension

Rank32 still costs371.876 versus274.456 frozen-reference seconds on6107.
Its full proof and non-timing statistics equal6049; the earlier root-lock
repair matched all69828932 decisions through300000 conflicts. New
[6183](../runs/20260909-6183-rank32-current-prefix/manifest.json), job1677171,
extends current semantic6107/native to one million conflicts. Existing trace
pins are reused; no new observer or solver code is introduced. Rolling hashes
must first reproduce the cached69828932-row hash10d4b4ca139060f5 on both arms;
only decision rows from300000 onward are written. If that check fails, the
earlier closure cannot be inherited and the investigation moves earlier.

This paired diagnostic obtains missing native suffix evidence, not another
Kissat timing reference. Exact requested stops, cleanup and complete suffix
comparison are required before interpreting it. The trace represents semantic
6107 rather than the new sorting binary; the full6181 parent replay establishes
the corresponding unchanged observable work/proof, not a whole-state theorem.
Next locate the first worse-side portable mechanism, retaining the general
Reduce and Substitute root closures and both-side N1 reconstruction.

## Validation failures, hygiene and limitations

The new sealing scripts initially assumed provisional-v1 records contained
final-only binary identity and then command fields, causing KeyError before
any summary was created. Inspection of the actual producer corrected the
schema handling; seven actual-script fixtures cover provisional/mixed/final
records, absent/incorrect final digests and rejected certificates. Final
identity checking remains mandatory. All39 benchmark and13 watcher tests pass.
No solver/checker failure occurred.

Existing campaigns, fixed references and submitted pins are hash-unchanged.
Checked outputs are runner-attested clean; live checker scratch and diagnostic
streams remain owned by their campaigns. Only generated test-fixture copies
were automatically cleaned; no campaign data or manual files were deleted.
No Claude-owned fuzz artifacts were inspected, and denied transfer/push
requests were not retried. Reports are local, not sent to Andrew. No promotion.
