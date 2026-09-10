# Reconnect timing and stronger fixed inprocessing proofs

## Authority and scope

Current6730 remains unpromoted and unfuzzed; production is unchanged. The
full-50 result remains 8300.164 / 7239.010 = 1.146588277 times fixed Kissat,
above the 1.01 target. Neither global scheduling/CDCL nor memory parity is
established. This checkpoint prices isolated6902, not a new fixed-reference run.

Candidate6902 skips the binary-only reconnect word scan while preserving the
old order-bit normalization and mixed-list path. Its full-state tests, three
independent build suites and 128 reconnect-dependent proofs are owned by
[the preceding report](2026-09-10-watch-rebuild-oracle.md). No solver source,
policy, layout, tick budget, proof delta or public contract changed here.

## Timing gate

Campaign `20260910-6912-watch-rebuild-runtime` compares6902 against6730 on
ranks32/39/38 in three waves, reversing the middle order. Jobs1679284,
1679290 and1679300 were submitted once at03:14:53,03:22:31 and03:31:45EDT.
Each later wave required exact preceding solver statistics and certificate
bytes, not completion of proof checking. Resources remain12CPU/48000MiB,
1200solver-only seconds,16000checker seconds and5h allocation, full width
on cpu-dense-hwulab-q without a node constraint or throttle.

**Decision: reject6902; retain6730.** The frozen
[strict6914](../runs/20260910-6912-watch-rebuild-runtime/solver-checkpoint-6914.json)
and [phase-pricing6915](../runs/20260910-6912-watch-rebuild-runtime/timer-cost-6915.json)
contain all18 expected UNSAT outcomes, exact complete reported work and
certificate bytes, with no timeout or solve loss. Four certificates were
checked/clean at6914's snapshot; the remainder remain independently pending.

| Rank | Paired median6902/6730 | Full paired range | Slot median | Outside-slot median |
|---|---:|---:|---:|---:|
| 32 | 0.995094165 | 0.990179464--0.995843447 | 0.996856743 | 0.993506994 |
| 39 | 1.011564597 | 1.008975477--1.014781997 | 1.006779261 | 1.014689248 |
| 38 | 1.072128491 | 1.069465714--1.073150954 | 1.024106759 | 1.075349368 |

All three32 pairs improve, but all39 and38 pairs regress. Every arm's absolute
spread is below0.66%. Peak RSS is exactly equal in every pair:587236/435116/86520
KiB for32/39/38. Paired solve/PAR-2 sums are3443.973s candidate/3357.573s previous.
Substitute timers improve on32/39 (medians0.982027/0.988612), but39 Vivify
regresses1.026166 and38 outside-slot cost regresses1.075349.
The large38 loss is predominantly outside inprocessing. This is an observed
whole-binary tradeoff, not proof of an intrinsic reconnect or code-placement
cause. No alignment tuning, repeat-to-select, composition or full50 adoption.

## Stronger selected proof acceptance

[6919](../runs/20260910-6919-strong-inprocessing-proofs/summary.json) executes
the unchanged fixed Sweep and Vivify fixture matrices against actual6730 and
6902 objects. All216 proofs pass:108 LRAT through cake_lpr and108 DRAT through
DRAT-trim. Both satisfiable negative controls reject. Input, proof and fixture
state agree across arms and with the selected historical evidence.

There are60 Sweep and48 Vivify proofs per arm. This closes the selected fixed
proof gap after the legacy C LRAT checker's false acceptance, not all historical
LRAT evidence or current-source fuzz coverage. The defect and its22-byte SAT
reproducer remain classified and frozen in6911; no checker was patched.
Current timing/B-C proof pipelines use DRAT-trim and are unaffected.

The10 maintained strict-oracle unit/integration tests remain unchanged, as do
the fixture matrices. No new parser behavior was added; existing period-range,
timer-matrix and result-classification tests cover the unchanged tooling. Any
subsequent implementation change must explicitly reconsider unit coverage.

## Next unit-cost lead: Factor

[6920](../runs/20260910-6920-factor-cost-annotation/summary.json) annotates
existing checked Rank32 samples; no solver rerun was needed.
[6921](../runs/20260910-6920-factor-cost-annotation/location-6921.json) binds
the assembly, source, counts and interpretations. Both solvers report86
Factor applications,7357 accepted factorizations and428217051 logical ticks.
Tactix's14.406s rule timer is steady-clock time; Kissat's8.80s is nested CPU
time. Those are not a precise same-operation unit-price ratio.

| Sampled region | Factor self periods | Program periods |
|---|---:|---:|
| Large candidate scan/rejection/matching | 30.5253% | 0.51239% |
| Packed binary candidate count | 6.8996% | 0.11581% |
| First-touch push return and count reload | 12.7094% | 0.21334% |

The1101 self samples locate code, not removable-work ceilings. The large scan
closely follows useful native work. The first-touch path is a bounded
compile-only dataflow lead: inspect whether a separate-vector push forces
avoidable cofactor pointer/count reloads. Sample skid and call-return
attribution prevent treating these percentages as predicted gains.

Kissat and CaDiCaL both record first-discovery order before incrementing the
cofactor count. Preserve that order, predicates, marks, overflow behavior,
tie selection and budget boundaries in any equivalence test. Tactix already
has an open-addressed remainder pool, lazy per-watch remainder reuse, byte
marks/activity, clause-header reservations and packed binary scans. Do not
reimplement those optimizations or assume FlatLists/redesign approval.

## Validation, hygiene and handoff

All262 benchmark-tool and22 SLURM tests pass. Living docs remain747/350/671/
560/600 lines, within their caps; no automatic compaction. Audit6923 records
final published check states, job accounting, immutable
dependencies, the deadline monitor and aggregate tests; landing6924 stages only
that owned scope. Successful writers and all reference/campaign authorities
remain frozen. Raw annotations stay local/hash-bound, not whitespace-edited.

6919 removed172844bytes of verified regenerable fixed CNFs/proofs;6920 removed
557169956bytes of its temporary profile expansion. Retained logs, generators,
objects and compressed profile reproduce that work. No active checker scratch,
failure reproducer or source was removed. No fuzz launch/access, denied
transfer/push retry, promotion or new reference run occurred.

The Andrew report is local and unsent at
`report-to-andrew/20260910-0334-watch-rebuild-timing/report.pdf`.
Winddown is05:45EDT and no new tasks after06:00EDT September10; monitor81311,
pid271019, does not cancel jobs automatically. Preserve the current4/32/33/49/46
gains, N1--N4 and SP-SLOT-5; Rank21 remains shelved.
