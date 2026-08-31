# Codebase revisit and large-OTFS candidate — 2026-08-30

## Outcome

The final retained candidate includes the positive large
on-the-fly-strengthening repair and the subsequent Kissat-exact Congruence,
Kitten, and BVE lifecycle repairs. Homogeneous campaign
`20260830-2530-tierd-ranks1-40-bve-physical-definition`, job `1656982`, uses
one immutable Release binary (`d304e8e0...a691f0e5`) on all 40 ranks and
returns every expected verdict with no timeout. Solver time is 9,987.021
seconds: 0.976540x the previous fully checked homogeneous candidate,
0.889880x frozen accepted Tactix, and 1.301201x pinned Kissat. Geometric-mean
ratios are 1.015179x, 0.915676x, and 1.309210x respectively.

All 22 SAT models and 8/18 UNSAT proofs are independently verified with no
invalid result; the remaining ten proof checks continue under the project's
nonblocking policy. The provisional performance verdict is positive. Final
acceptance remains contingent on those proof checks and the separately owned
Claude fuzz gate.

## What changed and why it is general

Pinned Kissat shortens a large OTFS reason in place.  It retains the surviving
first watch, even when that literal is root-false; publishes the resolved
literal as removed; and publishes only the surviving old tail as added
material.  Its binary OTFS path instead drops root-false literals and retires
the old large clause completely.

Tactix packs an OTFS replacement into a new arena record.  Before this repair,
generic irredundant-clause creation marked every replacement literal added and
generic retirement marked every old literal removed.  That representation
leaked a semantic event Kissat never produced.  On Rank 15 it touched first
watch 981, admitted clause `[-923,89,90,981]` into a later Strengthen stream,
removed `-89` from victim 136723, and started the 6.2x trajectory gap.

The repair separates physical replacement from semantic notifications:

- large/binary selection uses the non-root antecedent size;
- large OTFS retains the old first watch and selects only its second watch;
- the packed add suppresses generic all-literal marks;
- the resolved literal and surviving tail receive the exact Kissat marks;
- retiring the old packed record is representation cleanup, not a second
  semantic clause deletion;
- binary OTFS retains the distinct full-retirement lifecycle.

There is no option, formula, rank, benchmark, size, density, classification,
admission, or effort predicate in this path.

## Exact causal evidence

Campaign `20260830-1100-tierd-rank15-otfs-causal-confirmation`, job `1656658`,
reaches the formerly divergent conflict 1,646,098 with no truncation.  Variable
981 is not touched since the Strengthen watermark; `-923`, 89, and 90 are.  The
selected victim has three touched literals and candidate index 12, matching
Kissat.  The decisive literal-90 stream is empty, the four-literal clause is
not a partner, and victim 136723 is retained unchanged.  The independent
checked Rank-15 production runs are 79.889 and 80.295 seconds, versus 497.086
seconds in the preceding candidate and 80.788 seconds in Kissat.

## Cumulative behavior

Largest improvements versus the preceding homogeneous candidate:

| Rank | Verdict | Before (s) | Candidate (s) | Ratio |
|---:|:---:|---:|---:|---:|
| 19 | SAT | 771.403 | 39.280 | 0.051x |
| 15 | SAT | 497.086 | 80.295 | 0.162x |
| 37 | UNSAT | 650.424 | 383.614 | 0.590x |
| 9 | SAT | 109.287 | 94.313 | 0.863x |
| 33 | UNSAT | 515.460 | 462.982 | 0.898x |

Largest regressions:

| Rank | Verdict | Before (s) | Candidate (s) | Ratio |
|---:|:---:|---:|---:|---:|
| 21 | SAT | 139.042 | 237.706 | 1.710x |
| 20 | SAT | 342.868 | 514.796 | 1.501x |
| 32 | UNSAT | 598.209 | 819.111 | 1.369x |
| 34 | SAT | 505.991 | 626.286 | 1.238x |
| 27 | UNSAT | 376.179 | 423.557 | 1.126x |

These are trajectory changes, not direct repair cost.  Rank 20 grows from
264,349 to 479,448 conflicts and from 552,514 to 862,184 BVE eliminations.
Rank 21 grows from 43.1M to 116.3M decisions; Sweep environments grow from
158,577 to 757,976 and Sweep-derived equivalences from 235 to 7,830.  Rank 32
grows from 9.27M to 15.25M conflicts, while Sweep ticks grow from 1.03B to
4.18B.  Rank 34 spends nearly all of its added 120 seconds in search rather
than inprocessing.  These ranks should be localized from the first common
state divergence; they are not grounds for restoring the wrong OTFS lifecycle.

## Revisit-codebase audit

The complete retained diff was reread against the pinned Kissat sources and
its production wiring was re-audited.

- Formula classification is unchanged: `classification.small` is write-only;
  `classification.bigbig` gates exactly ordinary fast binary jump-reason
  assignment.  No inprocessing consumer exists.
- Production BVE, Factor, forward subsumption, and Vivify remain uniformly
  enabled at 100‰, 50‰, 100‰, and 100‰.  Explicit typed/environment arms are
  diagnostic only.
- Kissat creation order remains the unconditional default.
  `TACTIX_ARENA_ORDER=1` remains the CaDiCaL diagnostic arm.
- The compact four-byte mixed BVE occurrence entry matches Kissat's 31-bit
  payload plus kind bit.  Binary deletion now compacts the physical stream;
  stale large references remain lazily validated, matching their different
  lifecycle.
- Every authored BVE elimination round collects once at its boundary, including
  a zero-yield round, instead of carrying a stale packed arena into Strengthen.
- Reorder stops at the 100th open clause literal and ties by permanent import
  order, mirroring Kissat's compact internal order rather than Tactix identity.
- The new selected-Strengthen and mark-state diagnostics are compile-time
  excluded from Release.  The immutable candidate contains none of their event
  strings, and its rebuilt SHA matches the campaign pin exactly.
- The `mark_added_material` clause-add parameter is intentionally narrow and
  used only by packed large OTFS reconstruction.  A future representation API
  split could make the seam more explicit, but changing it now would be
  cosmetic and would move the validated binary without improving behavior.

No production code was changed during this revisit.  The full Release build
and all 76 tests pass, including classification/configuration invariance,
kernel, BVE, Reorder, OTFS, proof, campaign-pinning, N1, and documentation
gates.  Focused ASan/UBSan OTFS/kernel/slot tests also passed before the
homogeneous run.  `git diff --check` is clean.

## Remaining work

1. **Closed:** all 18 independent UNSAT proof checks and all 22 SAT model
   checks pass. The homogeneous campaign is terminal valid and clean.
2. Preserve the repair and use Ranks 20, 21, 32, and 34 as general divergence
   targets.  Rank 21's Sweep expansion and Rank 32's extra search/Sweep phases
   are the clearest mechanism-level signals.
3. Representation/memory remains materially above Kissat on many ranks; the
   median candidate/Kissat peak-RSS ratio is about 1.99x.
4. Claude-owned fuzz remains an external authorization gate.  Codex has not
   inspected or substituted for its artifacts.
5. Commit and push only after the remaining correctness/authorization gates
   are resolved; do not mutate the validated production source beforehand.

## Diagnostic campaign construction lesson

The first attempted snapshots-off Rank-32 continuation exported
`EVENT_TRACE_KEEP_SNAPSHOTS=0`.  Both trace builds interpret this as a
presence-only flag, so the value enabled snapshots and repeated the expensive
observer.  Its clean common prefix remains useful, but it did not test the
declared mode.  Future campaign runners must omit presence-only variables for
false and assert that omission before submission; manifests must distinguish
requested from effective observer configuration.  This is an infrastructure
validation rule, not a solver strategy change.

The corrected omission run confirms that retained macro snapshots were not the
main cost: the common learned stream advances only to conflict 4,588.  Detailed
hooks were admitted from conflict zero, so even events later rejected by the
trajectory filter paid construction cost.  Subsequent divergence observers
must start their detail window inside the last proven common prefix and stop at
a nearby bound; conflict-zero full-detail reruns are prohibited.

The 4,500--10,000 window validates that method and extends exact learned-clause
agreement through conflict 8,026, but still times out before its target.  The
remaining cost is the retained conflict/analysis detail surface.  The next
observer must be a symmetric learned-hash-only diagnostic mode that rejects
other detail payloads before construction.  It must be tested in both overlays
and remain compiled out of the Release solver.

That learned-only mode is now implemented symmetrically.  It retains only the
canonical compact learned hash, rejects all other detail helpers before payload
construction, and keeps `analysis_end` only to enforce the requested stop.
Tactix behavior and Kissat overlay structure pass the focused 3/3 trace suite;
both fresh trace pins expose the new mode, while the Release SHA remains
unchanged.  Rank-32 job 1656710 is the single 8,000--250,000 validation and
divergence campaign.

That campaign reaches 250,000 conflicts on both arms in 222/248 seconds with
zero disallowed events.  It localizes the first state difference to conflict
9,465 while the canonical learned clause is still identical; order differs at
9,466 and learned-set content at 9,477.  This ordering points upstream of
semantic conflict analysis.  The next and only justified observer is a narrow
full-detail window over conflicts 9,463--9,478.

The narrow observer completes in 23.403/25.807 seconds and moves the boundary
upstream again. Decisions match through 1,794,937. At decision 1,794,938,
inside conflict 9,463, Tactix selects variable 23,769 and Kissat selects 23,765;
the two variables are selected in the opposite order on the next decision.
Saved phases agree and Kissat's random-decision sequence is inactive. The
first discrepancy is thus stable EVSIDS heap order, not phase choice or
conflict analysis. Any next diagnostic should expose only the score and heap
topology needed immediately before this pop.

That targeted observer completes in 16.642/15.991 seconds. The competing
variables have bit-identical score `89.170458499388999`, and the lazy-pop
algorithms make the same comparisons. Their pre-existing poses are reversed:
Tactix positions variables 23,769/23,765 at 2/5, while Kissat positions them at
5/2. The pre-pop heap sizes also differ by six (425,625 versus 425,631), and
their last members differ. The causal frontier therefore moves to heap
membership/reinsertion lifecycle before this decision. A single symmetric
endpoint heap-set dump is justified; a scoring, phase, or pop change is not.

The complete endpoint sets make the population defect exact. All 425,625
common variables have identical printed scores. Tactix has no extra member and
is missing six variables which Kissat reports active and unassigned: 120401,
120404, 417987, 417988, 418002, and 418015. Only 6,591 common positions differ,
so most physical topology is already exact; the local subtree changes follow
from those omissions. The next observer should retain heap lifecycle events
only when one of those six variables participates, from startup through
conflict 9,463.

The six-variable lifecycle observer completes cleanly in 16.241/15.340
seconds. All 76 selected-variable heap insertions agree, including the last
five at conflict 9,458. The first lifecycle difference occurs later in that
same conflict: after Tactix reaches level zero, it alone removes all six
variables from the score heap at propagation count 37,100,965; Kissat removes
none and retains all six active and unassigned at the endpoint. This rules out
score arithmetic and insertion as the owner. The causal frontier is now the
root-assignment/deactivation path immediately after the conflict-9,458
warmup/walk boundary. A selected root-assignment observer, including reason
and warming state, is justified before any production change. The symmetric
observer now passes the focused 3/3 suite and leaves Release byte-identical.
Rank-32 campaign `20260830-0800-tierd-rank32-six-root-assign` (job 1656720)
runs exactly the current/reference pair through conflict 9,464 and retains only
the six selected variables' heap lifecycle plus selected root assignments. It
completes cleanly in 16.241/15.240 seconds and records zero selected root
assignments on both arms; each trace is byte-identical to the preceding
lifecycle trace. Root fixing is therefore not the removal owner. Tactix reports
387 substitutions and zero BVE eliminations at termination, so the next
observer should expose only these variables' Substitute representative maps.

That observer completes cleanly in 16.141/15.290 seconds and moves the causal
frontier out of the heap entirely. The first Substitute pass at conflict 9,458
is exact. On the second pass, immediately after Sweep, Tactix maps the six
variables to representatives 120326, 120333, 315535, 315536, -315534, and
-315537; Kissat keeps all six self-representative. Tactix then retires them and
the stable heap loses exactly those entries. The next comparison is therefore
the lifecycle of only the twelve binary clauses encoding these equivalences,
which will distinguish extra Sweep products from different SCC traversal.

The twelve-pair lifecycle run resolves that choice. Tactix records 48 events;
Kissat records none. At conflict 9,458 and search tick 94,447,016, Tactix Sweep
creates both implication binaries for every one of the six equivalences. The
following Substitute pass adopts them and then retires the six variables.
Kissat never creates any selected pair. The remaining question is whether the
two Sweep calls received the same binary formula; an exact pre-Sweep binary-set
comparison is the narrow next observer.

The two exact pre-Sweep multisets show that Sweep input is already different.
At the first entry, 756,241 binary copies are common. Tactix has one extra pair,
`(-174966,174967)`; Kissat instead has four pairs through variable 174968:
`(-174968,115775)`, `(-174968,174966)`, `(-174968,174967)`, and
`(-174966,174968)`. The second entry's 22/35-copy difference is downstream of
the six Tactix substitutions. The causal frontier is now the earlier Probe
pipeline, beginning with Congruence, not Sweep's solver itself.

At the first Congruence entry, however, the two complete binary multisets are
exactly identical: 756,275 copies on each arm. The second Congruence entry is
already downstream and differs by 35/36 copies. Thus the first-round Probe
input is common, and the four-versus-one pre-Sweep delta is created inside that
round. The early Backbone entry is the next useful cut, after Congruence and
early Substitute but before Vivify.

The early-Substitute boundary makes the owner exact. All 756,182 Kissat binary
copies are common, while Tactix has only two additional clauses:
`(-174968,174966)` and `(-174966,174968)`. The immediately preceding
Congruence call has therefore introduced a Tactix-only equivalence
`174968 <-> 174966`. Early Substitute rewrites through it, later Sweep sees the
altered local formula and proves six more equivalences, and the heap divergence
follows. The next diagnostic should stay inside Congruence and retain only
these two binary creations.

The selected lifecycle resolves discovery versus lifetime. Both solvers create
the same two representative clauses at conflict 9,458 and search tick
94,447,016. Kissat deletes both inside that Congruence call at probing tick
20,611,019, before early Substitute; Tactix keeps them until Sweep. The initial
Kissat result was mechanically rejected because the campaign parser required
Tactix's optional 14th `rule` column instead of the 13 common required fields;
the archived metadata and parser are corrected without rerunning either arm.
The next one-task reference observer reports the physical copy count before
each selected add/delete. This tests whether Kissat's second dense entry is
retiring a duplicate, as `dense.c` indicates, before changing production code.

That copy-count task completes cleanly, but the watched count is zero at every
event because the relevant delete occurs while sparse watches are disconnected.
The Congruence boundary and delete-call graph identify `extract_binaries` suffix
deduplication as the remaining branch. More importantly, the observer labels
an unexported Kissat internal variable with `internal_index+1`; that fallback
can collide with a real external number and make different clauses appear to
be the selected pair. The successor retains the raw internal literal and raw
export-map entry for each event. Production stays unchanged until those
identities confirm that the compared lifecycle is real.

They do: the add/delete rows match at raw internal pairs `(304263,304258)` and
`(304259,304262)`, with nonzero raw exports for all endpoints. The fallback
hazard is not active for this pair. A final reference-only call-site label now
distinguishes Congruence extracted-suffix deduplication, the dense duplicate or
satisfied branches, and Substitute before production is touched.

The label is unambiguous: both generic Kissat deletes are immediately preceded
by `substitute`. Kissat creates these clauses as temporary representative
scaffolding after entering Substitute and removes them during binary rewriting;
it does not publish them from Congruence. Thus the Congruence-cleanup hypothesis
is falsified. Tactix's extra pre-Substitute inference is still the first formula
difference, and its producer site is now being labeled directly.

The producer is `add_equivalence_chained`. This excludes ternary strengthening,
the ordinary AND-gate proof path, and representative-star publication. The
remaining caller is either an XOR/ITE degeneracy or an XOR/ITE congruent-gate
match; one selected-pair branch label distinguishes those cases.

It is XOR degeneracy. Tactix canonicalizes an extracted XOR under the current
union-find to a single residual literal and emits the output equivalence. The
next selected payload records the original gate and every cancellation so the
same gate can be followed through Kissat's incremental XOR rewrite path.

The gate is XOR output 174966 over inputs 174968 and 174970. Tactix removes
174970 as root-false, leaving unary residual 174968; no representative pair
cancels. Kissat's update rule would merge the same unary XOR if it had the gate
and value. Since the earlier boundary compared only binaries, the next cut now
compares every live large clause that can encode the XOR at first Congruence
entry.

The first two-arm attempt preserves a valid 585,717-clause Kissat set, but the
Tactix arm exposes a diagnostic asymmetry: selected-variable slot boundaries
forwarded explicit binary snapshots but not formula snapshots. The helper is
now symmetric and tested; only the missing Tactix arm is refreshed, while the
valid Kissat artifact remains the comparison authority.

The refresh produces 585,717 Tactix clauses as well, and canonical external
clause multisets are exactly equal; only internal-representation hash fields
differ. The mechanism is in gate admission: Tactix indexed original root-valued
XOR/ITE clause shapes, whereas Kissat removes root-false literals, skips
root-satisfied clauses, and rejects effective clauses shorter than three.
Tactix now uses those same predicates. A focused effectively-binary-XOR test
and debug/trace/Release Congruence checks pass. The first validation compares
the complete early-Substitute binary sets before any homogeneous campaign.

That validation is exact at the first early-Substitute entry: both arms contain
756,182 binary copies with no multiset difference, and the two extra Tactix XOR
equivalence clauses are gone. The later six-variable split still occurs, so the
general fix is retained and the next cut moves just past the now-common
Substitute input, to early Backbone entry.

That boundary is exact too: 756,176 binary copies on each arm and no multiset
difference. Early Substitute therefore transforms the synchronized input in
the same way. The next cut is pre-Sweep, after Backbone and Vivify; an exact
result there would isolate the remaining six-variable split inside Sweep.

The first pre-Sweep binary cut is exact: both snapshots declare 756,245 copies,
and their canonical sorted external pairs have the same SHA-256
`358a6bd5...483c4`. The complete files have different hashes because they also
contain a second snapshot after divergence; that later state is not evidence
against the first-boundary equality. Since Sweep also consumes large clauses,
the next control captures the complete live large-clause multiset at this same
first entry. Only if both formula components agree will the remaining split be
assigned to Sweep's mechanism.

That two-arm control is campaign
`20260830-1070-tierd-rank32-pre-sweep-formula-set`, Slurm job `1656751`.
It reuses the immutable trace pins and changes no production strategy.

The control is exact. Both arms capture 580,534 irredundant and 3,436
redundant live large clauses, and the canonical
redundancy/size/external-literal multiset has matching SHA-256
`7e0dea9e...c681`. Together with the exact 756,245-copy binary snapshot, this
proves that the first Sweep receives the same full formula. The causal owner is
now Sweep itself. Campaign `20260830-1080-tierd-rank32-sweep-seed-frontier`,
job `1656753`, records the ordered seed/environment summaries needed to find
its first internal divergence without another production change.

The broad observer is deliberately rejected: both arms reach the 300 s cap at
conflict 2,003 after only 588/589 Sweep seed-environment prefixes. Generic
analysis snapshots outside the target conflict dominate that overhead. The
prefix neither reaches the common conflict-9,458 state nor supports a parity
claim. The successor enables the same Sweep summaries only inside the target
conflict window; this is an observer repair, not a solver rerun or strategy
variant. The windowed successor is job `1656755`, campaign
`20260830-1090-tierd-rank32-sweep-seed-frontier-windowed`.

That trace reaches the endpoint validly. At conflict 9,458 Tactix begins with
seed 120320 while Kissat begins with 120327, so the resumable schedule differs
before the current embedded solver is invoked. The conflict-0 prefix gives the
mechanism: the first 588 seed and environment summaries are identical. After
seed 34975, however, Tactix is at 1,000,001 cumulative Kitten ticks and Kissat
at exactly 1,000,000. Because Kissat polls with strict `>`, it processes seed
34979 too; Tactix has already crossed the budget and retains a different tail.
The tick/solve deltas differ from the first common seed 881. The next observer
therefore selects only seed 881 at conflict 0 and compares environment order,
queries, models, and flips before considering a production repair. Campaign
`20260830-1100-tierd-rank32-sweep-seed881-query`, job `1656757`, uses newly
pinned trace binaries after a diagnostic-only snapshot-window repair; focused
event and overlay tests pass.

The selected environment is exact: 256 variables, 235 clauses in identical
literal order, and 142 binary attempts with identical dispositions. Both
initial solves cost 312 ticks, but their SAT models differ immediately. The
cause is the embedded-Kitten lifecycle. Kissat constructs a fresh Kitten with
generator 0 and uses it directly for the first environment; `kitten_clear`
advances only subsequent environments. Tactix had initialized its first reset
to seed 1. The general repair changes the session sequence to 0, 1, 2, ... and
resets a new Sweep invocation to 0. Focused lifecycle assertions plus the full
100,000-formula SubSolver oracle pass. Only the changed Tactix selected-seed
arm needs refresh against the already-valid pinned Kissat trace. That one-task
validation is campaign `20260830-1110-tierd-rank32-sweep-seed0-validation`,
job `1656759`.

The changed Tactix arm is exact against the preserved Kissat authority:
256-variable import order, 142 binary attempts, 235 clause orders, 12 queries,
and 12 SAT models all match byte-for-byte. Seed 881 costs 5,745 ticks in both,
and Tactix now processes the same 589 initial seeds. This validates the
lifecycle repair at its causal origin. The next boundary carries it through
the prior Sweep resumptions and rechecks the complete formula plus retained
schedule at conflict 9,458 before aggregate scoring. The one-task changed-arm
campaign is `20260830-1120-tierd-rank32-seed0-target-boundary`, job `1656760`.

The solver reaches the selected target boundary but exits at its 120-second
diagnostic ceiling before conflict 9,464, so the whole observer is not marked
acceptance-valid. The captured boundary itself is complete and exact. Both
binary snapshots match Kissat at 756,245 and 755,972 copies (canonical hashes
`358a6bd5...483c4` and `b4b85eb0...df209`), all 583,970 large clauses match
(`7e0dea9e...c681`), and all 1,681 ordered seeds, environment dimensions, and
per-seed tick/solve deltas match. Both find 159 equivalences. Three diagnostic
rows reverse endpoint print order, but the post-Sweep clause state is exact.
The Rank-32 divergence is therefore closed at the mechanism and resulting
state levels. The repaired lifecycle now advances to release/cumulative gates
and one uniform 40-rank campaign.

## Fresh-Kitten cumulative and homogeneous result

Release SHA `de74215e...149649` passes all 76 tests. Cumulative campaign
`20260830-2300-BC-kitten-seed-guard` publishes all 66 solver results before
retaining validation: 33/33 expected verdicts per arm, with current Tactix at
1,697.264 s versus accepted at 2,439.221 s (0.695822x). Its two long T105 proof
checks remain deliberately nonblocking.

Homogeneous campaign `20260830-2310-tierd-ranks1-40-kitten-seed` also has all
40 expected solver verdicts. Current total is 10,226.941 s versus previous
10,179.368 s (1.004674x total, 0.999187x geometric mean), accepted 11,222.888
s (0.911258x), and Kissat 7,675.236 s (1.33246x). Rank 31 improves from
707.157 to 360.759 s and Rank 32 from 819.111 to 590.422 s. Rank 33 regresses
from 462.982 to 776.358 s, Rank 34 from 626.286 to 805.907 s, and Rank 35 from
450.248 to 539.739 s. Rank 36 remains solved and stable at 384.854 s versus
383.669 s previous and 368.510 s Kissat. The exact general lifecycle repair is
retained; no selector is introduced.

On Rank 33, corrected Tactix ends with 13.140M conflicts versus 6.714M in the
previous candidate and 6.343M in Kissat. A two-arm prefix trace reaches
conflicts 2,012/2,003 under a 300-second observer ceiling. One Tactix-only
conflict record at conflict 1,171 is an overlay asymmetry: both solvers enter
and leave the same analysis state and resume identically at conflict 1,172.
After masking only that row, every portable macro and search event through
Kissat conflict 2,003 is exact. Thus the Rank-33 amplification is not startup,
preprocessing, or early search; campaign
`20260830-2330-tierd-rank33-post2003` continues from conflict 1,950 to locate
the first later mechanism divergence.

That continuation reaches conflicts 3,809/3,071 at its 900-second ceiling.
One more Tactix-only large-conflict record at conflict 2,106 is another overlay
asymmetry: analysis input/output and the next conflict remain identical. After
masking only that row, the full portable overlap is exact through Kissat
conflict 3,071, with no macro-order or macro-state divergence. Serializing
large conflict payloads now dominates the observer. The next cut therefore
records only canonical learned-clause hashes from conflict 3,000; once that
stream differs, a narrow full-detail window can identify its owner without
another broad expensive trace.

The learned-only cut is valid in both arms through conflict 250,000 and is
nearly unperturbed (196.881 s Tactix, 195.136 s Kissat). Canonical learned
clauses match through conflict 15,347. At conflict 15,348 both clauses have
size 22 but different sets; Tactix enters with 5,646,516 decisions at level 42
and Kissat with 5,646,518 decisions at level 44. The first causal frontier is
therefore a two-decision search-control difference immediately before conflict
15,348. The next observer enables full decision/propagation/conflict/analysis
detail only for conflicts 15,347--15,349.

That narrow cut is valid in both arms. After conflict 15,347 the first 14
decision literals match. At recorded decision 5,646,489 (level 15), Tactix
selects variable 19,823 while Kissat selects -12,222; Tactix selects -12,222
one decision later. This is a stable variable-selection divergence. Because
both solvers have executed one Reorder by this point and Rank 15 independently
left Reorder's physical scan/weight/application order open after proving exact
input clause multisets, the next bounded observer compares the pre-increment
heap pop at decision counter 5,646,488. It will distinguish a stale assigned
member from different Reorder-produced score/order state.

The heap cut makes that distinction. Variable 19,823 has the same
`4.5496685432775584e+43` score in both solvers. Kissat removes it as already
assigned and eventually reaches root 12,222; Tactix sees 19,823 unassigned and
selects it. Thus neither its EVSIDS score nor Reorder's ranking owns this
boundary. The following implication cut finds that Kissat assigned 19,823 at
level 11 after common decision -25,805, through reason
`-12307,-12303,-12299,19823,25772`; Tactix's propagation closure has 10
different literals while Kissat's has 12. Physical implication order differs
even earlier, immediately after the first common post-conflict decision +9,908
(Tactix first propagates 9,977, Kissat 9,912), despite an exact conflict
15,347 and exact learned clause. The next observer hashes the portable active
assignment set at the common analysis endpoint and after each common decision.
That will separate harmless implication permutation from the first retained
trail-state difference before auditing the owning watch reconstruction or
propagation-order mechanism.

The first hash version exposed one representation-local root-fixed identity
still reachable from Tactix's queue; excluding level-zero assignments makes
the portable comparison exact. At analysis end 15,347, both solvers have the
same 15,052 signed non-root assignments and hash `05910eef13c8073e`. They also
match immediately after assigning the first common decision +9,908 (one
assignment, hash `44dedfd474063c75`). After propagating +9,908 and assigning
the next common decision, both have 21 assignments but their hashes differ:
Tactix alone contains 10,166 and Kissat alone contains 9,912. The first
semantic Rank-33 divergence is therefore one propagation closure from an
exact formula-control and assignment state. The next cut records only the
ordered watch list scanned for +9,908, including binaries
`(-9908,10166)` and `(-9908,9912)`, to distinguish formula membership from
watch reconstruction/order.

The +9,908 watch cut is valid in both arms. Both solvers scan 19 binary
watches for `-9908`, with all common entries in the same relative order, but
Tactix contains `(-9908,10166)` and lacks `(-9908,9912)` while Kissat has the
opposite membership. The large-watch membership and order also differ. This
is formula state, not merely propagation order. A lifecycle observer shows
that Tactix initially creates both binaries, then BVE deletes
`(-9908,9912)` at conflict 15,347. The corresponding Kissat lifecycle run
records both initial additions but does not reach the target BVE phase within
300 seconds; its result is an observer-cost timeout and is not used as solver
evidence.

The next cut uses a diagnostic-only `BVE_HEAP_ONLY` mode in both trace
implementations. It retains only the changing schedule state for external
variables 9,912 and 10,166 plus the stopping boundary, avoiding the lifecycle
observer's global overhead. Focused event-trace and overlay-static tests pass.
Campaign `20260830-2420-tierd-rank33-bve-heap9912-10166`, job `1656887`, is
valid in both arms (42.083 s Tactix, 39.730 s Kissat). The target candidates
have identical occurrence counts, relevance, scores, and initial positions.
Their later heap trajectories diverge; by the end of the round Tactix has
eliminated 9,912 and retained 10,166, while Kissat has retained 9,912 and
eliminated 10,166. This explains the binary deletion and the +9,908 propagation
split. The original interpretation that Kissat's schedule ended at 310,880
was wrong: that was merely the heap size at the last selected-variable update
row, not an end-of-schedule record.

The non-reference `bve_min_bound` selective admission cache and its kernel
storage are removed. `bve_retry` now directly mirrors `flags.eliminate`:
attempts clear it, semantic removals re-arm affected variables, and a bound
increase blanket-rearms every variable. Candidate enumeration remains
complete for N1. Focused BVE, slot-pose, event-trace, and overlay tests pass.
The next validation reruns only changed Tactix at the same Rank-33 boundary
against the preserved pinned Kissat trace before release and cumulative gates.

That first changed-arm validation, campaign
`20260830-2430-tierd-rank33-bve-retry-validation` (job `1656889`), is valid but
non-closing: its target heap trace is unchanged at the causal transition,
excluding the cache as the owner. Pinned Kissat clears every still-set
`flags.eliminate` bit when its resolution limit leaves
members in an incomplete elimination round, and does not advance the bound.
Tactix had retained that tail and advanced its bound anyway. The scheduler now
drops all remaining retry flags on the same incomplete-round condition and
gates bound advance on completion. A focused four-pair fixture forces a
one-candidate dose and pins both effects. Campaign
`20260830-2440-tierd-rank33-bve-incomplete-validation` (job `1656890`) reruns
only changed Tactix against the preserved job-1656887 Kissat trace. It is
valid but non-closing: wall time is 42.033 s and its 84-record trace digest is
exactly the same `be4763db...` as job `1656889`, including the target heap
trajectory. The incomplete-round repair is reference-exact but did not close
the split. The next observer records one canonical summary per
BVE schedule in both solvers, so the first retry-set divergence can be located
without paying for a global clause-lifecycle trace. Focused tests and both
instrumented builds pass. Campaign
`20260830-2450-tierd-rank33-bve-schedule-sets` (job `1656891`) ran exactly one
Tactix and one pinned-Kissat task with no production selector. Both arms are
valid (42.334 s and 39.679 s). The first schedule is the exact same
429,743-variable external set at bound 0: size, external-ID sum, mixed XOR, and
mixed sum all match. This corrects the earlier 310,880 interpretation and
excludes retry-set admission as the first-round cause. The first two observed
pops match, then the heap transition differs. The next summary adds ordered
topology and score-multiset fingerprints to distinguish initial construction
from dynamic heap updates. Its focused tests and both trace builds pass.
Campaign `20260830-2460-tierd-rank33-bve-topology` (job `1656893`) is valid in
both arms (42.382 s Tactix, 39.728 s Kissat). At the first round, insertion hash
`eaf80835b69cd9e3`, topology hash `1ad753a321a3f75d`, both score fingerprints,
and all set fingerprints match exactly. Compact order, initial scores, and
heap construction are therefore excluded. A dynamic elimination/update
difference after the initial common pops owns the split. The next observer
uses cumulative pop hashes to locate the first mismatching ordinal block.
Focused tests and both trace builds pass. Campaign
`20260830-2470-tierd-rank33-bve-pop-blocks` (job `1656895`) is valid in both
arms (41.832 s Tactix, 39.677 s Kissat). Cumulative external-ID hashes are
exact through pop 131,072 (`5811a78766ecde2c`) and differ by pop 135,168 while
heap sizes remain equal. The next cut emits individual pop rows only for
ordinals 131,073--135,168 to identify the first different root and its last
common predecessor. Campaign `20260830-2480-tierd-rank33-bve-pop-window` (job
`1656897`) is valid in both arms (41.933 s Tactix, 40.129 s Kissat). Pops match
through ordinal 132,360: both pop external variable 91,836 at score 3,999,990.5
and expose 91,843 as the next root. Processing 91,836 creates the first split.
At ordinal 132,361 Tactix pops 91,837 at score 3,999,991, while Kissat pops
91,843 at 3,999,990.5. The next cut records only candidate 91,836's elimination
plan and the update trajectories of heap entries 91,837 and 91,843. Campaign
`20260830-2490-tierd-rank33-bve-candidate91836` (job `1656899`) reaches the
target in both arms, but the Kissat arm hits the 100,000-row observer cap before
the stop boundary and is invalid overall. Its complete target prefix exposes
the mechanism: Kissat marks three opposite-side binary watch copies as AND/OR
gate members while Tactix marks only two. Tactix's sparse partner lookup kept
one index and silently demoted the duplicate copy to an antecedent, changing
both the duplicate resolvent and variable 91,837's score. Tactix now marks
every matching binary copy, exactly like Kissat's literal mark followed by its
physical watch walk. A direct duplicate-gate regression test and all focused
tests pass. Changed-arm campaign
`20260830-2500-tierd-rank33-bve-duplicate-gate-validation` (job `1656901`) is
valid (41.833 s) and closes the boundary. Gate counts are 1/3; all four
resolvent hashes and their order match the reference prefix; variable 91,837
finishes at score 3,999,989; and the next pops are 91,843 then 91,845. The
implementation expands each marked representative to all matching copies in
one linear side scan. It adds no admission, effort, formula, size, density,
classification, benchmark, or rank selector and preserves N1--N4.

Full Release validation then passed 76/76. Uniform campaign
`20260830-2510-tierd-ranks1-40-bve-duplicate-gate` (job `1656902`) pins
Release SHA `6ef2c6d5...da9bc7a2` and uses the unchanged 40-rank matrix; its
provisional timing does not wait for proof checking. During that immutable
run, a follow-through audit of pinned `ands.c` found the complementary
duplicate rule: Kissat leaves the first physical copy of an unrelated binary
partner as its antecedent but admits later copies to the gate after consuming
the literal mark. Tactix's sparse image instead retained the last copy and
left every unrelated copy as an antecedent. The image now retains the first
physical copy and the linear expansion reproduces both required- and
unrelated-partner semantics. A focused test distinguishes the resulting
binary resolvent content and passes. This follow-up is not present in job
`1656902`'s immutable pin and will receive its own gates after that baseline.
The same audit then found that pinned Kissat admits a physical large AND base
after root-false stripping even when only pivot plus one input remains, as
long as the opposite side has at least two distinct binary partners. Tactix
incorrectly skipped every two-active-literal base on the assumption that the
equivalence detector had handled it, but that detector sees only physical
binaries. AND detection now requires Kissat's two-distinct-partner precondition
and accepts the shortened large base. A focused test proves the otherwise
extra antecedent-by-antecedent resolvent is absent. Full Release validation is
again 76/76; the cumulative follow-up binary is `0ea7b7ca...a529994` with
worktree-diff SHA `9287df9d...fcbc33c`.

Job `1656902` is solver-complete with all 40 provisional verdicts expected.
Total solver wall time is 10,524.426 s: 1.02909x the previous homogeneous
candidate, 0.93776x accepted Tactix, and 1.37122x pinned Kissat; geometric-mean
ratios are 1.06488x, 0.96051x, and 1.37331x respectively. Rank 33 improves
776.358→469.639 s, Rank 34 improves 805.907→625.332 s, and Rank 35 improves
539.739→474.154 s. Rank 19 regresses 39.330→261.796 s via 1,342,198 conflicts
versus 122,019 previously, but remains well ahead of Kissat's 739.902 s;
Ranks 31, 32, and 40 also regress. This is evidence of downstream trajectory
sensitivity, not grounds to undo the proven duplicate-watch behavior. Eleven
proof checkers remained live at the solver-complete snapshot.

Validation correction: campaigns `1656902` and `1656942` pinned the
`checkmodel` executable but omitted its adjacent `modelcheck.py` module. All
22 SAT checker invocations therefore failed with an import error. The solver
verdicts and timing records remain usable only as observations; the exact
models disappeared with intentionally deleted task scratch, so neither
campaign is acceptance-valid and neither will be rewritten. UNSAT proof
checks are unaffected and continue. The next cumulative candidate must pin
`modelcheck.py` SHA `f70f0eef...1676cf` from the outset and pass all model and
proof checks before acceptance. No redundant reruns of the two superseded
intermediate binaries are scheduled.

Job `1656942` is solver-complete with all 40 expected verdicts, but remains
observer-invalid as described above. Its 10,553.166 s total is 1.00273x job
`1656902`, 0.94033x accepted Tactix, and 1.37496x Kissat; geometric-mean
ratios are 0.98045x, 0.94173x, and 1.34647x. The completed duplicate semantics
recover Rank 19 from 261.796→162.235 s, Rank 31 from 465.427→360.267 s, Rank
32 from 740.886→645.857 s, and Rank 35 from 474.154→376.276 s. Rank 37
regresses 420.727→747.713 s and Rank 33 gives back part of its prior gain at
639.537 s; Rank 36 remains stable at 384.237 s. The near-flat total plus better
geomean is retained as evidence of the exact general mechanism, while the
physical-definition correction becomes the next all-valid candidate.

The next fallback audit found a distinct physical/semantic image mismatch in
pinned `definition.c`. Kissat exports every physical large occurrence to
Kitten with only the pivot removed; root-false literals remain free subsolver
variables. Tactix exported the active resolution image, where those literals
had already been stripped, and could therefore manufacture an UNSAT core—and
a definition—that Kissat never finds. Definition import now iterates the live
physical clause directly, without a second stored clause image; resolution
continues using the root-filtered image. One-sided LRAT lifting likewise
starts from the physical original rather than citing root units used only by
resolution filtering. A focused fixture makes the physical environment SAT
and the old stripped environment UNSAT, and verifies that the formerly
skipped antecedent cross-resolvent remains. Focused and full Release tests pass
76/76. This cumulative source is Release SHA `d304e8e0...a691f0e5`; it is not
part of immutable job `1656942` and awaits validation after that run closes.

The SAT-checker packaging failure also exposed a missing recurrence guard.
Tracked Tier-D preparation now treats `checkmodel` and `modelcheck.py` as one
runtime bundle, refuses a missing module, and has a direct positive/negative
pinning regression. That test and the certificate-cleanup test pass. Campaign
`20260830-2530-tierd-ranks1-40-bve-physical-definition` (job `1656982`) pins
and hash-checks all four solver/checker artifacts before every task; it is the
only acceptance-eligible campaign in this sequence.

The superseded observer jobs completed 11/18 and 9/18 UNSAT proof checks
respectively. Their remaining proof-only tasks were then canceled because the
missing SAT models made acceptance impossible and seven job-`1656982` ranks
were resource-pending behind them. No acceptance-candidate task was touched;
all of its remaining ranks started immediately.

## Final BVE candidate and closeout audit

Job `1656982` completed all 40 solver tasks with the expected verdict and no
timeout. The immutable pin is Release SHA `d304e8e0...a691f0e5`; every task
hash-checks the solver, `drat-trim`, `checkmodel`, and its adjacent
`modelcheck.py`. At the 2026-08-30T14:09:53Z durable snapshot, all 22 SAT
models and 8/18 UNSAT proofs were verified, no check was invalid, and the ten
remaining proof tasks were still running.

| Class | Count | Candidate (s) | vs previous checked total / GM | vs Kissat total / GM |
|:--|--:|--:|--:|--:|
| SAT | 22 | 4,757.895 | 0.994554x / 1.051218x | 1.172498x / 1.247332x |
| UNSAT | 18 | 5,229.126 | 0.960707x / 0.972806x | 1.445579x / 1.389023x |
| All | 40 | 9,987.021 | 0.976540x / 1.015179x | 1.301201x / 1.309210x |

The cumulative source improves the immediate observer candidate by 5.36% in
total and 2.77% in geometric mean. It recovers Ranks 17, 27, 32, 33, and 37;
Rank 33 is now 390.433 seconds versus Kissat's 330.391, and Rank 36 remains
solved and near parity at 383.855 versus 368.510 seconds. Relative to the
previous fully checked homogeneous candidate, Rank 19 regresses
39.330→159.560 seconds and Rank 31 regresses 360.759→608.790 seconds. Those
trajectory costs do not invalidate the general repairs: the final total is
2.35% lower, all ranks solve, and both changed BVE semantics have direct
reference-derived fixtures.

The final changed-surface audit found no production formula, size, density,
rank, benchmark, identity, admission, or effort selector. `classification.small`
remains write-only; `classification.bigbig` still has exactly Kissat's ordinary
fast-binary jump-reason consumer. BVE, Factor, forward subsumption, and Vivify
remain enabled at 100‰, 50‰, 100‰, and 100‰. Kissat creation order remains the
unconditional default, with CaDiCaL order available only through the explicit
diagnostic arm. Compile-time event tracing remains useful parity scaffolding
and contributes no Release behavior. An unused local BVE signature helper was
confirmed to predate this work and was not removed as unrelated churn.

No production edit was made during the final cleanup pass. The remaining
changed code has a concrete parity, proof, test, or diagnostic owner. Full
Release validation is 76/76 in 20.15 seconds, configuration wiring reports 55
fields with zero failures, and `git diff --check` is clean. The candidate is not yet
committed or pushed: the ten proof checks remain authoritative, and the
required Claude-owned consolidation fuzz needs user authorization when the
user returns. No replacement fuzz was inspected or run.
