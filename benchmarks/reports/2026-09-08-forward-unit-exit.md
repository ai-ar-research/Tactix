# Rank 31: a forward unit cuts off useful work

## Result and causal evidence

The first differing Strengthen boundary on Rank 31 is an implementation gap,
not an inadequate quota. The current candidate ends its forward round when
self-subsuming resolution produces a unit. Kissat's actual forward pass
propagates that unit through its connected watches and continues with the next
candidate. This explains a concrete way Tactix leaves useful simplification
undone. No solver repair or runtime attribution is claimed yet.

Authority: `benchmarks/runs/20260908-5764-rank31-forward-exit/summary.json`.
The diagnostic job1673370 uses a trace-only successor of candidate5741.
Its complete ordinary traces and partial proof bytes equal parent5755 exactly
through300000 conflicts. No native solver was rerun for this localization.

| Strengthen at conflict179823 | Candidates checked | Tail skipped | Steps spent / allowance | Exit |
|---|---:|---:|---:|---|
| First round | 32610 / 345695 | 313085 | 312049 / 35595073 | unit |
| Second round | 73331 / 562637 | 489306 | 909962 / 35595073 | unit |

The first unit-producing physical clause is `{-130574,130506,-145}`, with
`-145` already false; the flipped partner literal is `-130506`. The second
round reaches the analogous `{-108783,40355,-145}` case. These literals identify
the witness only; they must not parameterize the repair. The first round has
over99% of its allowance unused. The prior coarse boundary agrees on incoming
active/irredundant/redundant/binary counts and diverges on outgoing state,
41 decisions before the first differing branch choice. Equal counts do not
prove complete incoming formula/flag identity; repair replay must test closure.

## Actual-code controls and coverage hole

`benchmarks/runs/20260908-5760-forward-exit-observer/fixed-control.json` binds
the commands, objects, native library and fixtures. Both implementations see
a physical `(1 2 3)` with root `-1`, binary `(-2 3)`, and an independent tail
subsumed by `(4 5)`. Tail sizes3 and4 are tested. Both derive unit3; Tactix
checks only the first candidate and leaves the tail, while the actual native
forward pass subsumes it. No Python mirror or copied reference algorithm is
used. Native code is called directly from its pinned library/source.

The existing test named `test_unit_producing_strengthen_ends_round` derives
its unit during entry propagation, then tests deletion of an already-satisfied
clause. It does not reach the mid-round unit branch. New repair tests must
cover that branch and a still-productive tail explicitly.

The native fixture initially referenced an optional statistics field that is
absent in the pinned build. It now inspects actual tail garbage state. Its
first executable also passed a save-list to dense entry, removing binary
watches rather than retaining them as Eliminate does; the corrected null
save-list obtains the intended matching state. Original logs are retained.
Neither fixture correction changes the solver or weakens the expected tail
effect. All91 trace-build tests pass; the new observer is gated by explicit
diagnostic path/conflict and remains outside clean runtime measurements.

## Required complete repair

The next isolated successor must resume the same forward stream after a unit,
not restart all candidates or merely remove `ended`. It must:

- propagate through the current connected stream, preserving watch order and
  cleanup effects rather than silently substituting a different population;
- recheck each later candidate for garbage, root satisfaction, remaining unit
  or contradiction after roots change;
- name LRAT root consequences before deleting reasons; retain checked DRAT,
  root reasons, reconstruction and full-propagation obligations;
- retain strict between-candidate quota checks, interruption, retry flags,
  one action per candidate and the public instance surface.

Use focused SAT/model and UNSAT/proof cases for independent tails, chained
units, satisfied partners, root-false candidates, contradictory consequences,
large/binary connected entries, quota equality/exhaustion, interruption and
standalone/public calls. Benchmark names, deadlines and tuned constants are
not fixes. The new ledger entry is `SP-FORWARD-UNIT-CONTINUE-1`.

## Candidate5741 measurement and background correctness

Candidate-only replays5755/job1673256 and5756/job1673257 both match all parent
candidate traces and partial proof bytes; cached native controls are hash-bound.
This closes DefaultPolicy trajectory preservation for the public-target repair
on those stopped prefixes, not its complete runtime/fuzz gates.

The new full40 campaign5758/job1673329 runs only Tactix at12 logical CPUs and
48000MiB against frozen Kissat6810.066s. All40 expected solves total7807.759s,
or1.1465026918x;29 candidate certificates were checked at the frozen solver
checkpoint. The goal is not met and no runtime gain is claimed. Its score is computed by
`tools/bench/fixed_runtime.py`; only a complete40 view may create
`5758/solver-checkpoint.json`. Proof checks continue independently. The tool
has10 fixed tests covering candidate-only task identity, missing/duplicate
ranks, input/resource mismatches, timeout pricing, invalid times and rejected
certificates. No partial aggregate is reported. Fixed-reference comparisons
do not establish isolated unit-cost gains.

Helper5768 ranks all40 losses and compares predecessor work/proof records.
33 remain identical to5672. Rank32 is now the largest loss:633.485s versus
Kissat274.456s, against316.715s on5672, with changed work/proof and2.1071x native
peak RSS. Rank31 is next (+158.902s), then37 (+140.878s). This changes the next
priority: candidate-only5769/job1673375 matches all40102204 current Rank32
decisions against the cached native5597 full100K stream. New5771/job1673386
extends both to300K, records the suffix from100K, and binds the entire earlier
common prefix by digest. Its one native task supplies missing diagnostic
coverage, not a refreshed runtime measurement. Localize this changed-work
regression before banking root reuse; keep the proven Rank31 continuation
obligation. The root-reuse5718 parent has no full40 timing view, so this is not
an isolated timing attribution to public-target dispatch5741.

Regression5754/job1673255 has all66 expected solves; its independent checks
are sealed only on full completion by helper5767. Previous5678 is now66/66
checked and clean, B0.776185x/C1.024361x/combined0.934484x production.
Background Claude owner5765/session8493 covers5741 and owns its jobs,
oracles and cleanup; only its CLI response may be polled. Prior5715/4746 is
terminal PASS5672 and must not be polled or treated as5741 coverage.

Production remains unchanged; the runtime goal, memory, all proof/model gates
and current-source fuzz remain open. No push was attempted. No manual cleanup
was performed: new stopped jobs delete their own node-local inputs/partial
proofs after recording hashes, while unique traces, pins and fixture evidence
are retained. Earlier failed-fixture scratch remains an explicit audit item.
