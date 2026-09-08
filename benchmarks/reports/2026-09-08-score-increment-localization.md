# Rank33 score-increment localization and bounded memory attribution

## Outcome

The previous goal turn was progress: it established an actual score-ranking
reversal in Rank33's otherwise equal-membership heap. This turn localizes its
producer further: **the three selected bump admissions are identical, but the
increment is one decay generation ahead at conflict182093**. The preceding
extra-admission hypothesis is rejected. No scoring, scheduling, proof-rule or
production implementation is changed in this checkpoint.

The objective remains Rank1–40 aggregate time≤1.01x Kissat, with B28+C5 gates
and general component repairs. Latest complete solver view remains full5664,
**1.1283688258212774x**, all80 expected solves. Its frozen solver snapshot had
59/80 checked; the final-check sealer observed77/80 before this report. Goal not
achieved, no promotion. Source5672's independent runtime gates continue.

## Macro history and exact selected bumps

Run5694 (`20260908-5694-rank33-score-history`, job1672973), comparator5695 and
`score-bracket.json` from5701, reproduce all full1M parent5689 traces and proof
bytes per arm. All5,406 common named/counter/ordinal boundaries have equal
scores; all unmatched raw snapshot rows are retained separately, not discarded.
The last snapshot before the split is restart_end at conflict180773,
decision49323622: both score120438 as6.414611250162512e-300, heap position27264,
unassigned. That narrowed the update to180773–182093.

New observer5696 observes existing analyzed-variable `score_bump` callbacks
without enabling broad EventTrace, hashing the heap or scanning the formula.
Separate Tactix/native sources descend exactly from5649 and native5414
(native parent binary849db948…dc16). It records selected external identity,
conflict/decision counters, post-bump score, current increment, mode, position
and level. It changes only diagnostics and a new Tactix test; no source owned
by an earlier submitted job was edited.

The trace suite passes **89/89**. The new test runs three isolated processes:
exact selected rows and inclusive window boundaries with unchanged
score/increment/ticks/trail; missing output path; missing variable selector.
It also covers unrelated events, selected positive/negative external spelling,
unselected variables, invalid payloads, sequence and disabled global tracing.
Both native and Tactix builds succeed.

Freeze5697 initially rejected native configure's generated `src/makefile`
relocation. Successor5700 requires exact old-directory→new-directory substitution,
hashes the generated file, and verifies rather than rewrites the already-created
Tactix patch. No solver correctness failure occurred; the initial helper remains.
Authority: `20260908-5696-score-bump-observer/manifest.json`, per-arm patches,
all source hashes, binary hashes and build/test logs.

Run5698/job1672988 passes comparator5699's complete per-arm parent replay,
including heap/choice snapshots and proof bytes. **All523 selected admissions
match exactly in identity, conflict, decision and order.** The first unequal
value is admission520, at182093; the preceding selected bump at182089 matches.

| At selected bump | Tactix increment | Kissat increment |
|---|---:|---:|
| Conflict182089 | 1.035570341610101e39 | 1.035570341610101e39 |
| Conflict182093 | 1.207838275678788e39 | 1.1474463618948487e39 |

The latter ratio is1/0.95, one ordinary EVSIDS decay step. The same increment
excess accounts for the three score excesses. It is not an extra admission or
heap tie-order explanation. The exact update trigger still needs evidence.
Initial/root-conflict reuse, an empty/inactive bump transaction, and an OTFS
round are hypotheses, not established causes. Do not adjust the decay constant.

Run5704/job1673060 reuses5696's immutable binaries and observes only conflicts
182089–182093. It retains the full1M prefix endpoint, suppresses irrelevant
macro snapshots with an unreachable event-stop1000001, and disables watch,
implication and heap payloads. Predeclared9457 formula/binary dumps are omitted:
their selector sites differ when global tracing is active. Every other original
stream and proof byte must match. Only native analysis/end boundary additions
inside the five-conflict window may be normalized, using the existing strict
eight-fixture normalizer. Comparator5705 adds12 event-framing/window fixtures;
all20 pass. This diagnosis is not an acceptance timing.

Late outcome: both5704 arms reached the runner's1200-second timeout (exit124,
SLURM wrapper exit1); both removed scratch. These are explained resource exits,
not solver crashes/assertions or failed proof/model oracles. No complete replay
claim is made. Native CONTROL_ONLY admitted factor_* at19378 and exhausted its
100000-row cap before the selected window. Corrected5710/job1673072 adds the
existing SEARCH_ONLY filter, which rejects those rows before the cap;5711 keeps
the strict20-fixture replay validator. Sources/pins and5704 evidence are unchanged.
Tactix's partial5704 window shows a one-nonroot-literal binary conflict at182092,
then a learned root unit and nonempty-but-inactive analyzed list. Native source
reuses such a conflict, including a zero jump level. This is a source-backed
repair lead, not yet a validated paired trace. If5710 also times out, use a
shorter common endpoint with matched trace-on/off proof controls; do not keep
repeating an overlong observer or relax replay validation silently.

## Early Rank39 memory owners

Full memory probe5687/job1672962 was still queued under its unchanged five-hour
solve/check allocation while short diagnostics could start. Run5702/job1672996
therefore uses **two identical5649 trace binaries**, one with slot-memory tracing,
and stops both at100K conflicts. It neither replaces5687 nor reports a complete
verdict. Comparator5707 verifies exact decision/macro/boundary/prefix streams,
partial-proof bytes, pins/input and scratch cleanup before interpreting all422
ownership rows.

Initial comparator5703 rejected internal stages absent from parser5688's
begin/end-only assumption. Pinned producers explicitly emit Factor
admitted/applied, congruence-dense-resume entry/first/second-entry/second, and
vivify-dense-resume entry/resume. Successor5706 permits only those additional
name/stage pairs; all numeric/order/capacity checks remain. **24 fixtures pass**,
including invalid name/stage cross-combinations. Failed5703/5688 remain frozen;
5707 validates the prefix,5708 applies the complete schema to pending5687.

The early peak rises from243836 to **379040KiB within BVE**. At that BVE exit,
current RSS is260264KiB; reconstruction capacity is10076160 words and dense
capacity49354680 bytes. Later the largest sampled current RSS is323044KiB
(Congruence begin): allocator used231970KiB, allocator free99909KiB,
mapped2052KiB. This establishes early transient and retained-space components;
it does not identify every allocation behind BVE's internal peak or show that
all free allocator pages can be returned cheaply. Full Release peak is457020KiB
versus native170616KiB; early diagnostic RSS is not a full-run acceptance score.

Source review confirms BVE already has pooled antecedents/resolvents and
persistent scratch/SubSolver storage. Do not duplicate prior pooling, add
blanket allocator trimming, or infer a reconstruction/layout redesign without
its separate causal ownership and required design review. Next memory work
should distinguish BVE transient scratch, arena overlap, dense storage and
retention using the full5687 result and this bounded witness.

Full5687 subsequently completed its solver. Successor5709 validates all complete
non-timing statistics and proof bytes against full5664 and binds3160 ownership
rows. Its frozen `solver-checkpoint.json` remains provisional while checking runs.
The original5708 assumed every result carried `binary_sha256`; the pinned runner
omits that field from its distinct provisional schema after checking all pins.
5709 requires the exact provisional/final schema, arm/task/rank/input identity
and manifest/pin agreement; all13 new identity fixtures pass. Final records still
require their binary hash and all checked/cleanup conditions. No runner change.
The diagnostic peak is413452KiB. Highest sampled current RSS is384660KiB at
Congruence entry, with159744KiB allocator-used,130795KiB allocator-free and
108452KiB mapped. Maximum sampled allocator-free storage is169699KiB. These
are ownership clues, not a clean-memory improvement over the457020KiB Release
measurement or evidence that all retained pages can be released cheaply.

## Gates, tests and continuation

Propagation5652 now has **6/6 checked/clean** proofs and final summary; its
three-pair exact-work median remains0.9973786590x5626. Parent5651 B28+C5 is
**66/66 checked/clean**, no timeout/solve loss: B0.783706x, C1.036513x,
combined0.943621x production. The C slowdown is retained in the score, not hidden
by the combined result. Latest5678 is64/66 checked at this checkpoint. Its B28
solver outcomes allowed5679 wave0/job1673062 to start; waves1/2 wait only for
preceding solver outcomes, never their proof checks.

Wave0 subsequently completed both solves with identical work/proof bytes:
417.413/423.311s (candidate/parent), about1.4% faster, not replicated acceptance.
Wave1/job1673070 is submitted. Full5712/job1673074 now measures all unchanged40
pairs using tested5672, the same native binary, matrix, runner and12-CPU/48000MiB
contract; no throttle/node constraint. Snapshot5713 and sealer5714 own its results.

Fresh production CTest: **77/77 in6.21s**. New history/bump parsers contribute30
fixtures, window/normalization20, and complete memory schema24. Coverage review
requires the new observer unit test; configuration-only probes use parser tests
and exact replay instead of unnecessary solver unit-test changes. Documentation
checks cover status/report edits. No actual crash, rejected proof/model,
sanitizer issue or invariant failure was found.

Fuzz owner95829 returned terminal **PASS** on5649:70000 generated cases,63573
checked verdicts (37297 models,26276 proofs),zero solver failures and312 direct
reachability cases. The owner separately reported a fixture-only GCC13.1
libstdc++ input-iterator string-read UBSan finding; its test-only review copy
passed1200/1200. No old fuzz artifact/report was inspected by the parent.
New owner4746 runs changed-source checkpoint5715 on5672, with explicit
assignment-bound/root/chrono reachability and independent proof/model oracles.
Only its CLI response may be polled. The new prompt calls out the known fixture
issue for an isolated test-only workaround, never a submitted-source edit. Production
is unchanged; no files were manually deleted; unique sources/pins/traces and
failed observer expectations remain retained. Push remains unapproved.

Next: validate5710 with5711, identify the exact increment-advance transition,
then implement its smallest complete general repair with differential unit
coverage. In parallel use5709 for5687, advance5679 wave2 with5680 and score5681,
score full5712 with5713, and seal full5664 with5666 when all checks finish.
Never rerun frozen snapshots. No material deletion; benchmark residue checks
are limited to explicitly owned normal campaigns, never the opaque fuzz owner.
The Andrew checkpoint is `report-to-andrew/20260908-0857-score-increment/`;
the companion audit is `2026-09-08-score-increment-validation.json`.
