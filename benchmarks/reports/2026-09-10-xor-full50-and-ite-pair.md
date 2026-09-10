# Full50 XOR neutrality and ITE pair-load cost

## Authority and outcome

Current6730 remains unpromoted/unfuzzed; production c2a95a6c and the fixed
Kissat denominator7239.010s are unchanged. The <=1.01 aggregate target,
scheduling/CDCL parity and memory competitiveness remain unmet. No reference
refresh, Rank21 probe, FlatLists redesign, source composition or denied
transfer/push retry occurred. This checkpoint follows the frozen
[Factor/XOR report](2026-09-10-factor-xor-cost.md) and audit6978.

All measurements use full-width cpu-dense-hwulab-q arrays without node
constraints: one solver/task,12 logical CPUs,48000MiB,1200 solver-only seconds,
16000 checker seconds and a five-hour allocation. Immutable pins/runners are
independent of other campaigns. Exact expected solves permit development while
proof checks run; unchecked certificates are never called checked acceptance.

## XOR6950: all50 is aggregate-neutral

[6982 full comparison](2026-09-10-xor-presence-full50-6982/summary.json) and
[per-rank table](2026-09-10-xor-presence-full50-6982/comparison.tsv) freeze
all50 expected solves (25SAT/25UNSAT) from job1679487. Every certificate hash and every
non-timing statistic matches checked6730 full6743; no timeout or solve loss.
All50 peak RSS values are also identical to6730; maximum15.272984GiB does not
establish native memory parity.36 certificates were checked at this snapshot.

| View | XOR6950 seconds | Current6730 seconds | Fixed Kissat seconds |
|---|---:|---:|---:|
| All50 | 8300.569 | 8300.164 | 7239.010 |
| Original40 | 7680.531 | 7678.989 | 6810.066 |
| Added10 | 620.038 | 621.175 | 428.944 |

All50 is1.146644223x fixed Kissat and1.000048794x current6730: just0.405s
adverse on8300s, aggregate-neutral. The earlier repeated Rank47/42 gains and
neutral38 remain valid regional evidence, not an overall speedup. Keep6950 as
an isolated regional-cost candidate, not the current authority or a promotion.
No repeat-to-select full50 rerun is justified by this neutral observation.
This is a cross-window sensitivity/memory view, not a simultaneous causal
whole-program cost estimate. All adverse ranks remain in the table.

Ranks31/35 contribute430.146s of the1061.559s net native gap in this view.
Their conflicts are9,019,812/4,540,757 and8,942,669/4,751,990 respectively
(Tactix/Kissat). Ranks42/43 also have extra closures and changed search work;
the prior report owns those schedule observations. Thus the remaining gap is
not established as pure per-instruction cost. Reported propagation currencies
are not assumed equal across solvers, and this aggregate view cannot assign
exclusive percentages to scheduling, search work and unit cost.

## ITE6969: smallest independent loop change

[Build6969](../runs/20260910-6969-ite-pair-hoist-build/manifest.json) hoists
the immutable outer conditional-pair values outside the inner comparison loop.
The per-key list is never changed during pairing, including output-vector
growth. Candidate encounter/order, complement tests, four-clause verification,
deduplication, gate order, budgets/ticks, guard, proof and API are unchanged.
No new representation, sorting strategy or scheduling choice is introduced.
Therefore the calculus and guard owners need no semantic edit.

Prior compiled evidence6970/6971 moves the outer loads out of the hot inner
prefix (11 to7 instructions); enclosing extraction16014 to16095bytes, the
membership lambda unchanged. This is a compiler premise, not a speed claim.
[6998 linked-code control](../runs/20260910-6969-ite-pair-hoist-build/kernel-code-6998.json)
finds exact addresses/sizes/bytes/operands for five selected propagation and
reconnect bodies. It does not prove every CDCL body, cache behavior or native
scheduling parity.

Unit-test consideration was explicit for this change. New maintained
test_ite_pair.cpp exercises560 actual public extraction cases: all condition/
branch signs, duplicate and reversed inputs, all missing-clause subsets,
repeated extraction, output growth and exact256/over256 conditional-list hubs.
Small original formulas independently enumerate models to check emitted ITEs.
Complete ordered gate payload/tick digest551691671817ff7b matches both actual
objects; arena/watch arena/trail stay unchanged. Existing Congruence and1200
index-cleanup cases also agree in
[6985](../runs/20260910-6969-ite-pair-hoist-build/control-6985.json).

[6988](../runs/20260910-6969-ite-pair-hoist-build/budget-control-6988.json)
adds448 actual-source budget, termination and unfinished-index cases, both
orders, collection/pairing effort boundaries, exact/over hub cap and resumption
on the same index. Full gate/tick/interruption/cut digest3b02f64fd29a9493 agrees.
Independent Release/BC/trace suites113/113/112 pass. Release and BC SHA256 are
eb8d72738d9fe0f0505245f5d15fede28ddfc3f30956b4f9fb0326aabd2fbb2a.

[6990 proofs](../runs/20260910-6990-ite-pair-proofs/summary.json) checks384
ITE-dependent certificates:192 cake_lpr LRAT and192 DRAT-trim, with two known
SAT negative controls rejected. Eight shared ITE definitions per output cover
all signs, root-effective3/4/5, duplicates and reversed encounters. Opposite
original output units are assigned only after extraction; the new equivalence
then closes the contradiction. All old/new inputs/proofs/statistics are exact.
The legacy LRAT checker is not an acceptance oracle. This is fixed coverage,
not a replacement for unavailable Claude-owned source fuzz.

## ITE measurement

[B/C6993](../runs/20260910-6991-ite-pair-BC-gate/summary-6993.json) freezes
66 expected outcomes:17SAT/16UNSAT per arm,33 exact non-timing-statistic and
certificate pairs,64 checked at sealing. Job1679540 remains independent of
XOR full50. [6996](../runs/20260910-6994-ite-pair-runtime/solver-checkpoint-6996.json)
and [6997](../runs/20260910-6994-ite-pair-runtime/timer-cost-6997.json) freeze
all18 UNSAT timing solves, no timeout/loss, exact complete6730 work and proof
bytes/hash;10 checked at the solver snapshot. Jobs1679606/1679612/1679618
started05:29:18/05:36:18/05:43:32EDT, reversing the middle arm order. Each
advanced after exact solver outcomes, without waiting for proof checking.

| Rank | Paired wall median6969/6730 | Full paired range | Congruence median |
|---|---:|---:|---:|
| 47 | 0.994525845 | 0.993529038--0.999188186 | 0.993229025 |
| 42 | 0.994634331 | 0.992633267--1.001430158 | 0.992245750 |
| 38 | 0.998080104 | 0.996351524--0.998377293 | 1.043478261 |

All three47/38 wall pairs improve;42 has two improvements and one adverse pair.
All47 Congruence pairs improve;42's Congruence range crosses1. Every arm's
full spread is below0.79%, and every paired peak RSS is identical. Solve/PAR-2
sums are1701.906s candidate/1707.059s previous. Retain6969 as an isolated
modest-cost candidate for full50 sensitivity/memory measurement after resumption;
do not promote, compose with6950 or replace current6730 on these subsets.

Rank38's Congruence timer is only23--24ms. Its slot is71--87ms adverse in every
pair (median1.003665847x); the wall improvement comes from outside-slot time,
not faster ITE. Other unchanged-rule timings also move. Selected linked kernel
identity does not imply all code placement/cache effects are identical, and
no placement adjustment was attempted. Do not attribute every wall delta to
the hoisted loads or treat this subset as a full50/native parity result.

## Classified failures and hygiene

[6999 classification](../runs/20260910-6969-ite-pair-hoist-build/fixture-classification-6999.json)
preserves initial failed fixtures6983/6984. The test incorrectly assumed four
conditional entries per signed output; the unchanged baseline emitted65 gates.
Actual extraction adds two entries, so128 gates reach256 entries and129 exceed
the cap. Corrected6985 passes both implementations; neither failed attempt
reached the candidate. No solver change was made to repair a test expectation.
The first watcher timestamp included an offset unsupported by its parser;
corrected local-time invocation monitors the same job, without resubmission.

Only6990's961990bytes of verified, regenerable fixed CNF/proof/negative-control
files were removed. Their generators, logs, binaries and hashes are retained.
No live checker scratch, unique failure evidence, baseline, source or pin was
removed. Draft TeX initially rejected an unescaped placeholder; it was corrected
before publication, without affecting solver artifacts. Existing Factor timing6936 is now18/18 checked clean; rejected timing
decision unchanged. XOR B/C6957 is now66/66 checked clean. Later published
validation is owned by the final audit rather than rewriting timing snapshots.

## Handoff

Final audit7000 owns immutable bindings, current published check states, queue
ownership and the live deadline monitor.262 benchmark-tool and22 SLURM tests
pass; living docs remain within747/350/671/560/600-line caps. Only the current
PROGRESS resume prefix changes; its historical tail and frozen reports remain
unchanged. Andrew's LaTeX/PDF is local and unsent under
report-to-andrew/20260910-0545-xor-full-ite; no push retry.

Wind-down started05:45EDT. No new experiments were started afterward; the final
predeclared wave was already running. Start no new tasks after06:00EDT Sep10;
leave existing proof checks and cleanup running independently. After explicit
resumption, first recheck published validations for6980/6991/6994, then prepare
one Tactix-only6969 full50 gate against unchanged7239.010s Kissat and checked
6730/6743. Keep6950 separate and retain every adverse rank. After that gate,
prioritize larger Congruence closure/forward costs from existing6945/6965
profiles rather than assuming repeated tiny extraction wins close the gap.
Ranks31/35 and42/43 scheduling/search differences remain separate obligations.
Preserve4/32/stronger33/49/46 closures and N1--N4; Rank21 stays shelved.
