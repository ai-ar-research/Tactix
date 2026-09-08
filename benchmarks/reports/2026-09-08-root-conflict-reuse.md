# Root-conflict reuse and complete predecessor scoring

## Outcome

The Rank 1–40 target remains **≤1.01× Kissat**, with B28+C5 gates and general
component repairs. It is not achieved. The complete assignment-level5672 view,
[5712 solver checkpoint](../runs/20260908-5712-assignment-level-ranks1-40/solver-checkpoint.json),
is **7565.146/6810.066 = 1.1108770458×**, all 80 expected solves, 60 checks complete
at the frozen snapshot (62 at the later sealer observation). No production
source is promoted. Cross-window ratio changes do not attribute a speedup.

The preceding goal increment was progress: it implemented and tested isolated
root reuse5718 and submitted its gates. This continuation completes predecessor
scoring, verifies all40 parent work/proof identities, and starts the next
current-source causal diagnostic. The intervening plan-only reply confirmed
an existing requirement and was not runtime progress.

## General repair and evidence

[5718 manifest](../runs/20260908-5718-root-conflict-reuse/manifest.json) owns the
isolated source, immediate parent5672, cumulative/repair patches, initial and
final test logs, three build identities and fixed validation. Only
`src/rules/analyze/analyze.cpp` and `tests/test_root_conflict_reuse.cpp` change.
Release5718 and independent BC5720 have SHA-256
`fa53d8199674ac2d29b0244525c2d9bc1120de8a7b07e8cecde89e1bf8284115`;
trace5719 has `68575873544db2e7e4af4253b8a26841bcbb2f8ebe5b8f6b5fb9454510ac82ec`.
All submitted sources and pins are immutable.

Kissat's `one_literal_on_conflict_level` in pinned native5696 `src/analyze.c`
reuses a conflict with one top-level literal even when its second-highest true
level is zero. Its assignment path publishes the root unit and deactivates the
variable, without the learned-clause transaction. The outer analysis bumps only
a nonempty analyzed set. CaDiCaL's `find_conflict_level`/forced branch in pinned
`cadical-extdiag/src/analyze.cpp` likewise reuses the driver and constructs unit
proof hints; its chronological target differs. Kissat remains the tie-break.

Tactix excluded zero-jump reuse, unnecessarily entered learning and advanced the
EVSIDS increment. The repair removes that exclusion while retaining existing
watch repair, chronological-target selection, phase-saving order and OTFS
retirement. At true level zero it emits a named DRAT unit or LRAT unit with
root-unit hints in trail order followed by the conflict ID, assigns a unit
reason, and deactivates the variable. It does not increment learned counters
or add a fresh analyzed-variable transaction. Nonzero reuse is unchanged.
No new kernel primitive, policy constant, instance-specific branch or budget
change is introduced. Existing OTFS-round bump behavior remains intact.

The earlier score observer5698 completely replays its parent and matches all523
selected admissions; Tactix is one decay generation ahead at conflict182093.
However, **both5704 and corrected5710 terminate with exit124 at1200 seconds**.
Their scratch was removed. Partial window evidence at182092 motivates the repair
but neither is an accepted full replay; the strict validators are not relaxed.
The independent actual-code fixed fixture supplies the transition evidence.

[5726 causal summary](../runs/20260908-5726-root-reuse-rank33-prefix/summary.json)
subsequently validates both clean1M stops and exact complete native parent replay,
including all trace hashes and partial-proof bytes. The earlier31,494,584-literal
common digest agrees. Recorded decision suffixes agree for23,612,137 rows; the
first difference moves from182093 to **conflict227618, decision54966705, level204**:
Tactix chooses−233254, Kissat−232900. Thus the known decision split closes; the
entire1M trajectory does not. The suffix count must not be presented as a full
from-zero decision count, and earlier literal equality alone is not full-state
identity. Localize the new split before attributing it to another mechanism.

## Correctness and test consideration

Every change received a coverage review under plan §8.2. The semantic repair
adds288 cases: binary/large conflicts; depths1/7/103; focused/stable/no heuristics;
early/late root assignments; reuse enabled/disabled; canonical/explicit registry
dispatch; DRAT/LRAT. They check guards, bounded instance enumeration, state and
reason effects, chronology, unit IDs, decision-structure deactivation, counters,
score increment, reconstruction identity, scratch cleanup and proof hints.

[Fixed validation](../runs/20260908-5718-root-conflict-reuse/fixed-validation/summary.json)
compiles the fixture against actual parent5672 objects:288 predecessor controls
pass, and all24 selected predecessor transitions differ from the repair.
All24 current transitions exactly match a fixture invoking actual pinned Kissat
analysis, including stable increment and chronological retention. All288 complete
micro-proofs pass independent checkers:144 DRAT and144 LRAT. Generated CNF/proof
pairs were removed only after successful checking; hashes and checker logs remain.
This is fixed-test coverage, not a current-source fuzz pass.

Final suites: **91/91 in each independent Release build;90/90 trace**. The first
new fixture assertion incorrectly expected DefaultPolicy chronology when calling
the registry's canonical learned-unit path with reuse disabled. Source inspection
showed that path directly uses jump0. Only the test expectation changed; the
original test and failed logs are retained. This was not a solver correctness
failure. Production's existing suite subsequently passes77/77 in5.19 seconds.

New configuration-only comparisons reuse pinned runners and existing complete
stream/record validators. Localizer5733 additionally requires all40 parent work
statistics and proof hashes to remain identical; it passes. No new solver unit
tests are needed for those measurement wrappers. Documentation checks cover
calculus/implementation alignment; living docs remain within their size limits.

## Performance, memory and next experiments

[5679 three-pair checkpoint](../runs/20260908-5679-assignment-level-rank39-runtime/solver-checkpoint.json)
has exact cross-arm work and proof bytes, but median candidate/parent time is
**1.03047751×**, range0.98606698–1.04701441. Candidate shared-node spread is6.01%,
parent0.60%; only4/6 proofs were checked at its snapshot. Do not present the
first pair as a gain or the complete noisy result as an absolute speedup.
The assignment bound is not established as a performance win; retain its
identity as a separate candidate when assessing later cumulative gains.

[Latest losses](../runs/20260908-5712-assignment-level-ranks1-40/next-losses.json)
validate all40 work/proof identities against full5664 and rank remaining gaps:

| Rank | Tactix/Kissat seconds | Extra seconds | Peak-memory ratio |
|---|---|---|---|
|31|441.064/264.532|176.532|2.13676×|
|33|468.710/311.385|157.325|2.50917×|
|37|458.641/346.372|112.269|2.34713×|

Ranks31/33 have materially more conflicts and propagations; Rank37 has fewer
reported propagations yet is slower. This supports investigating both search
trajectory and implementation cost, not assigning the whole gap to budgeting.
Counter scopes differ across solvers, so these are not isolated unit-cost ratios.
Memory remains a separate open acceptance dimension.

[5687 memory summary](../runs/20260908-5687-rank39-memory-ownership/summary.json)
is now checked and clean, with exact full parent work/proof identity across3160
ownership rows. Diagnostic peak413452KiB is not a clean Release memory gain.
Largest sampled current RSS384660KiB includes130795KiB allocator-free storage;
do not confuse retained allocator space with live formula ownership.

Current5718 experiments:

- 5726/job1673168: complete Rank33 causal replay; first split now227618.
  Comparator5729 reports the earlier common literal digest separately from
  the23,612,137 common recorded suffix rows.
- 5727/job1673169: unchanged full B28+C5,66 arms, queued at this checkpoint.
- 5728: three Rank33 candidate/immediate-parent timing waves prepared. Submit5730
  wave0 after all B solver outcomes, then1/2 after predecessor solver outcomes.
- 5734 helper: full unchanged Rank1–40 on5718 after causal closure of the known
  182093 split, first timing pair and B solver outcomes. No campaign created yet.
- 5735/job1673171: newest-source Rank31 complete decision prefix through300K,
  selected from latest largest loss; compare5736. Historical5459 agreement
  through100K is not assumed to cover current5718.

All use full-width arrays,12 logical CPUs/48000MiB, no throttle/node constraint
or whole-node exclusivity. Proof checking stays asynchronous; pending verdicts
permit investigation, not checked-gate closure or promotion. Final B28+C5,
full40, current-source fuzz and component/memory obligations remain mandatory.

## Fuzz, hygiene and handoff

Only the Claude CLI owner is polled. Session4746 remains live on parent5672;
no fuzz jobs, logs or artifacts were inspected. It does **not** cover root5718.
Previous5657/95829 terminal PASS covers5649 only:70K cases,63573 checked verdicts,
zero solver failures and312 direct reachability cases. Its fixture-only GCC
string-read sanitizer issue and passing workaround are the owner's report.
Schedule a new changed-source checkpoint for5718; do not duplicate live5672.

No manual benchmark deletion occurred; the fixed validator deleted only its
288 verified CNF/proof pairs. Submitted pins, timeout evidence and unique raw
traces remain retained. Large raw streams stay local with durable hashes.
Push remains unapproved and was not retried. Local commit and Andrew report
complete this intentional checkpoint; the ≤1.01× goal stays active.
