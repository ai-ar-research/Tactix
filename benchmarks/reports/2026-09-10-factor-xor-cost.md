# Factor counter cost and indexed XOR parity membership

## Authority and scope

Current6730 is unpromoted/unfuzzed; production remains c2a95a6c. Its checked
full50 result is still 8300.164 / 7239.010 = 1.146588277 times fixed Kissat,
above the 1.01 target. Global scheduling/CDCL and memory parity remain open.
Both experiments here branch independently from6730. No composition, production
source change, fixed-reference refresh, alignment tuning or FlatLists redesign.

All real-instance tasks use immutable pins, one solver/task, 12 logical CPUs,
48000MiB, 1200 solver-only seconds, 16000 checker seconds and a five-hour
allocation. Arrays are full-width on cpu-dense-hwulab-q without node constraints
or throttles. Later waves require exact expected solver outcomes, not completed
proof checks. Pending checks remain unverified and any invalidity stops dependents.

## Factor6925: reject as a runtime improvement

[Build6925](../runs/20260910-6925-factor-count-build/manifest.json) saves the
cofactor address and previous value across a push into a separate touched
vector. First-discovery publication still precedes the count write. Predicates,
unsigned arithmetic, discovery/tie order, ticks/budgets, proof and API are
unchanged. The actual count helper shrinks431 to419bytes; its enclosing Factor
body15593 to15561bytes. This is inspectable compiler output, not a runtime claim.

[Actual-object controls6928](../runs/20260910-6925-factor-count-build/control-6928.json)
cover270 grid cases: first/repeated candidate visits, touched-vector growth,
2--65 rows,3--19 columns, binary/ternary/five-literal clauses, reverse order,
effort cuts and second applications. Callback sequence plus selected physical
watch/arena/mark/root/counter digest is identical: fd9ab4194fd8cb35. This is not
a claim of every Kernel byte. Existing Factor/kernel suites and independent
Release/BC/trace suites113/113/112 pass.

[6932](../runs/20260910-6932-factor-count-proofs/summary.json) checks288 fixed
Factor-dependent proofs:144 cake_lpr LRAT,144 DRAT-trim, two negative controls
rejected. Unlimited cases introduce a fresh variable; later original roots
force a generated quotient conflict. All old/new input/proof/statistic outputs
are exact. [B/C6935](../runs/20260910-6933-factor-count-BC-gate/summary-6935.json)
has66 expected outcomes and33 exact work/certificate pairs,64 checked at sealing.

[6938](../runs/20260910-6936-factor-count-runtime/solver-checkpoint-6938.json)
and [6939](../runs/20260910-6936-factor-count-runtime/timer-cost-6939.json)
freeze all18 exact-work UNSAT solves, no timeout/solve loss, four proofs checked
at the timing snapshot. Three waves reverse the middle arm order.

| Rank | Paired wall median6925/6730 | Full paired range | Factor timer median |
|---|---:|---:|---:|
| 32 | 1.001800166 | 0.996930906--1.013625111 | 0.988313043 |
| 39 | 1.002363015 | 0.996596785--1.003551267 | 0.998531952 |
| 38 | 1.002182796 | 0.997278678--1.003034149 | 1.000000000 |

Every arm's full spread is below1.18%; peak RSS is identical in every pair.
Solve/PAR-2 sums are3356.730s candidate/3350.735s previous. Rank32's Factor
timer improves in all three pairs, but no tested rank has a favorable whole-solver
median. This is a small local cost reduction without a demonstrated useful
whole-binary tradeoff. Reject6925; no favorable-noise repeats or composition.

## Why Congruence and Rank47

[6944](../runs/20260910-6944-congruence-cost-selection/summary.json) selects
Rank47 using existing checked41--50 and native records. Both have five closures,
but current/native counts differ:35,374,938/38,194,759 gates and
2,009,250/2,090,302 merges. Current Congruence takes58.730s of a141.145s solve;
fixed native total is98.418s. Differently scoped native nested CPU timers are
not a precise equal-operation price. Ranks42/43 have extra current closures and
much more search work; those scheduling/CDCL obligations remain open.

[6947](../runs/20260910-6945-congruence-release-profiles/solver-summary-6947.json)
binds a new diagnostic pair on the unchanged binaries: both UNSAT, exact own
unprofiled counters/certificate bytes, every current non-timing statistic exact,
zero lost samples. Neither proof was checked at that snapshot. This is not a
reference refresh or repeated timing acceptance. The profile wrapper replaces
perf-stat with perf-record; probe counters are not solver IPC measurements.

Current gate extraction is5.53% of program self samples.
[6948/6949](../runs/20260910-6948-congruence-extraction-annotation/location-6949.json)
localize6751443230 periods,1.2905955% of program periods, to the inlined XOR
membership range. That range includes necessary and larger-arity work; it is
not a removable-time estimate. The separate hot ITE conditional pair scan is
not the XOR lookup and has not been redesigned.

## XOR6950: implementation and correctness

[Build6950](../runs/20260910-6950-xor-presence-build/manifest.json) reuses an
existing ternary-presence byte. For a distinct-variable ternary, sign pattern
indices0,3,5,6 encode even parity (mask0x69);1,2,4,7 encode odd parity (0x96).
Requiring the complete mask answers exactly the four prior membership queries.
These constants are truth-table-derived, not tuned. Record ordinals advance
even across skipped candidates; seen/gate order and upfront full-combination
ticks remain unchanged. Unfinished indices reject, and sizes4/5 retain exact
lookups. No new field, layout, index construction, policy, guard or proof delta.

The existing Kissat and CaDiCaL XOR extraction mechanisms were reviewed. Their
complete parity-set requirement is retained, without imposing their different
candidate-admission order on Tactix's stronger/current canonical extraction.
The calculus and guard owners need no semantic change for this isolated reuse.

[6951](../runs/20260910-6950-xor-presence-build/control-6951.json) runs3176 new
fixed cases against actual old/new Congruence objects: all256 ternary sign sets,
root-effective sizes3--5, duplicates, reverse encounters, repeated-variable and
larger interfering prefixes, full and every single-missing parity group of
sizes4/5. Complete ordered gate payload/tick digest cb9ff4553209f52b agrees;
public extraction leaves arena/watch arena/trail unchanged. Existing Congruence
and1200 index-cleanup cases agree; full independent suites113/113/112 pass.

[6953](../runs/20260910-6950-xor-presence-build/index-control-6953.json) adds1026
actual-source complete/unfinished index controls, including real budget and stop
cuts at the256th record. Digest941d9b9c2d472746 agrees. No copied implementation
or production test-only API is used.
[6956](../runs/20260910-6956-xor-presence-proofs/summary.json) verifies96 further
XOR-dependent proofs,48 cake_lpr LRAT/48 DRAT-trim, with two negatives rejected.
Both congruent outputs remain unassigned until the new equivalence is published;
opposite original output roots then close the proof. Both parities, root padding,
duplicates and reverse order have exact old/new input/proof/statistic outputs.

For both candidates, five selected linked propagation/reconnect bodies retain
exact addresses, sizes, bytes and operands (6943/6964). This is not all-CDCL
identity, cache-equivalence or native scheduling parity.

## XOR measurement

B/C6968 has all66 expected/all33 exact work/certificate pairs,64 checked at
sealing. [6962](../runs/20260910-6960-xor-presence-runtime/solver-checkpoint-6962.json)
and [6963](../runs/20260910-6960-xor-presence-runtime/timer-cost-6963.json)
freeze all18 expected UNSAT solves with exact complete6730 work and certificate
bytes. No timeout or solve loss; ten proofs checked/clean at the timing snapshot.
Jobs1679462/1679472/1679479 were submitted once at04:28:09/04:35:55/04:42:39EDT.

| Rank | Paired wall median6950/6730 | Full paired range | Congruence median |
|---|---:|---:|---:|
| 47 | 0.988786708 | 0.985633749--0.989966413 | 0.976548417 |
| 42 | 0.996915074 | 0.994408994--0.997333495 | 0.987139866 |
| 38 | 1.000130077 | 0.997097074--1.000160416 | 1.000000000 |

All three47/42 pairs improve; every arm's full spread is below0.72%. Rank38's
0.013% adverse median is small relative to its observed spread and mixed paired
range: treat this control as neutral, not a speedup. All paired peak RSS is
identical. Solve/PAR-2 sums are1701.090s candidate/1707.825s previous.
Rank47 slot median is0.985030074; outside-slot0.990798299, with an outside range
that crosses1. Do not attribute all wall-time change to the edited operation.

**Decision: retain6950 as an isolated candidate for a full50 measurement, not
production promotion or a new current runtime authority.** The repeated
whole-binary result and separate stage diagnostic support a modest useful
cost improvement; the full50, memory and current-source fuzz gates remain open.
No composition or favorable-noise repeats. Current6730 and the fixed denominator
remain unchanged.

[6967](../runs/20260910-6965-xor-stage-time/summary-6967.json) binds a separate
pair using existing Congruence stage timers on Rank47. Both replay exact complete
reported work and certificate bytes; all five cumulative rows have identical
processed-item counts, ending40063059. XOR extraction falls4372 to3024ms
(0.691674291x); instrumented apply falls62158 to60690ms (0.976382766x).
Index6462/6476ms and ITE9022/9068ms remain close; closure26999/26848ms and
forward15718/15580ms overlap and must not be summed. This supports the local
mechanism, not independent whole-solver performance acceptance or native parity.

## Next bounded lead: ITE outer-pair loads

Independent compile-only6969 branches from6730, not6950. The existing inner
ITE loop reloads both fields of its unchanged outer conditional pair for every
inner comparison. Hoisting those two source reads outside the inner loop needs
no new index, sort, persistent layout or pairing algorithm. Hub cutoff, gate
verification, seen/emission order and full logical effort charges are untouched.

[6970/6971](../runs/20260910-6969-ite-pair-hoist-build/inspection-6971.json)
confirm the actual compiled premise: the inner admission prefix goes from11 to7
instructions; the outer condition stays in a register and its saved rhs is
read only after complementary-condition admission. The enclosing extraction
body grows16014 to16095bytes, so this is not a whole-binary speed prediction.
No real solver ran6969. Before proceeding, add actualold/new repeated-pair,
duplicate, ordering, output-growth and256-cap boundary tests, then full builds,
strict ITE-dependent proofs and B/C. Existing generic tests are not claimed
sufficient. No structural redesign or composition is authorized by this result.

## Failures, validation and hygiene

6927's optional nonexistent test_rule_instances.cpp path stopped the harness
after old270/Factor tests passed, before candidate compilation. Failure logs
remain; fresh6928 uses the existing kernel suite.6942 used a wrong mangled
beyond-conflicts symbol; its preflight failed before object/output mutation or
solver execution, and fresh6943 uses the actual no-argument symbol.6959's
premature B/C seal at64/66 correctly failed before opening a summary;6968 seals
after66 exact results. Lasttwo tasks started later than array submission. None
is a solver failure or justification for weakening a gate.

The legacy LRAT-checker false acceptance remains classified6911. Strict
cake_lpr/DRAT-trim acceptance and negative controls are retained;384 new fixed
proofs do not establish all historical LRAT correctness or current-source fuzz.
Claude-owned fuzz remains unavailable; no launch, transfer retry or artifact
inspection occurred. No denied push retry or production promotion.

The262 benchmark-tool and22 SLURM tests pass. New solver cases explicitly cover
changed behavior/boundaries. Existing strict matrix/timer/oracle readers cover
unchanged analysis; the instrumented pair uses the eight maintained cumulative
timer tests. Rank47 has no BVE timer, so common-rule pricing requests Congruence,
Sweep,Vivify,Substitute rather than inventing a zero-cost BVE measurement.
Docs remain747/350/671/560/600 lines, within caps; no automatic compaction.

This checkpoint removes608678bytes of verified regenerable fixed CNFs/proofs
(6932:570144;6956:38534), plus240062556bytes of its own temporary profile
expansion6948. Logs, objects, generators, failed controls, raw/compressed profiles,
immutable pins and all live checker scratch remain. No broad cleanup.
Audit6940 owns final published checks, immutable bindings, job accounting and
deadline-monitor validation; landing6941 stages only audited paths.

Andrew's local, unsent deliverable is under
`report-to-andrew/20260910-0450-factor-xor-cost/`. Winddown remains05:45EDT,
no new tasks after06:00EDT September10. The monitor never cancels jobs.
Preserve4/32/stronger33/49/46 and N1--N4; Rank21 stays shelved.
