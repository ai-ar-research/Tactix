# Progress and resume guide

<!-- track-sched:begin -->
## Active ordered goal

**Phase 1 comes first.** Independently check Rank 1--50 at 50/50 and B28+C5 at
33/33, each case within 1,200 seconds and 32 GiB. Aggregate solver time must be
below the frozen R16 gates: Rank below 8,115.735s and B28+C5 below 952.263s.
Preserve WV61 coverage, public rule selectability, native DSR proofs and original
CNF models. Rank 1 and Rank 6 overhead must stay repaired through general
mechanisms. Individual trajectory noise is acceptable when both full-suite gates
pass and no unexplained drastic regression remains.

**Phase 2 starts only after Phase 1 passes.** Reach independently checked WV100
100/100 within 1,200 seconds and 32 GiB per case by integrating each owning
baseline's techniques into Tactix. No sequential portfolio, external solver,
benchmark dispatch, or arbitrary benchmark-tuned cutoff is allowed. Every
integrated technique must be at least as sophisticated as its owning state-of-
the-art implementation, including Kissat, Satsuma and LymphoSAT mechanisms.

Use cpu-q for one-off profiles, correctness checks and coarse solved/timeout
iteration. Use matched allocations for close runtime claims and the final Rank
and B28+C5 gates. All solver runs use Slurm, immutable pins, native proof/model
oracles and bounded memory.

## Current checkpoint

AM149 remains the accepted Phase-1 authority. Rank 1--50 solved 50/50 in
7,945.569s and B28+C5 solved 33/33 in 893.232s under the matched c12 contract;
WV61 is 61/61 clean and Claude-owned fuzz is PASS. Authority:
`build/20260918-am149-helper-backed-cut-feedback-authority/manifest.json`.

AM160 is the first Phase-2 successor. LymphoSAT's retained owner-attribution
table is `benchmarks/runs/20260819-tierd-lymphosat-modlog/results-table.tsv`.
It identified `oddball-weighing` as owner of four open cases. Tactix now exposes
a general typed Lucky instance 10 for structurally recognized pair-choice and
signed-ternary blocks, with the owner's seeded annealing trajectory, bounded
admission, private auxiliary completion, whole-CNF validation, and H-only
guidance on failed search. There is no filename, rank, benchmark, or size-class
dispatch. Authority:
`build/20260918-am160-owner-exact-balanced-signature-authority/manifest.json`;
binary SHA-256 `f16414d77507593d77043f28e7979231896835271fdd9427c093b85302597ec6`.

AM161 job 1702525 solves WV89/92/94/100 in 2.169/2.420/3.272/3.923s. All four
original-CNF models pass, cleanup is clean, and each case used one restart,
26--225 annealing steps, and three completion rounds. AM166 job 1702708 adds
WV27 in 0.066s and 12.3 MiB with a verified original-CNF model. Its general typed Lucky11 action
recognizes ordered arithmetic-circuit input blocks, recovers the comparison
constant, runs the owner's deterministic 64-bit factoring strategy, privately
completes auxiliaries to a feedback fixpoint, and validates the whole live CNF.
Frozen binary SHA-256 is
`6340aec09e641a242e2afb271cdb2f67806d5bab2ee69ce65c696cca7d417e06`.
AM168 job 1702754 adds WV40 in 0.066s and AM170 job 1702885 adds WV73 in
1.319s; both original-CNF models pass and cleanup is clean. AM171 job 1702894
adds WV85 in 28.115s after 941,695,142 numeric key candidates; its 8,896-clause
original-CNF model passes. AM174 job 1702975 adds WV84: typed Lucky15 recognizes
the exact width-7, 128-column st-connectivity family and emits 7,861,711 native
RUP clauses in 9.231s and 13.5 MiB solver RSS. The 267.5 MB DSR certificate
passes dsr-trim and patched Trestle in 204.548s; cleanup is clean. Independently
checked coverage was 90/100 at that checkpoint.

The general AE-kissat-MAB successor adds simultaneous EVSIDS and CHB stable
heaps, the owner's age-weighted CHB updates, restart reward/momentum/adaptive
UCB selection, root restart on arm switches, and selected-heap TransitiveReduce
ordering. AM177 independently checks WV87 SAT in 1.620s and WV97 SAT in
179.719s. AM179 job 1703169 independently checks WV96 SAT in 1,197.329s; the
boundary-only predecessor times out, establishing selected-heap inprocessor
coupling as causal. Coverage is now 93/100. Open ranks are 1, 25, 26, 80, 83,
86 and 95.

SC2024's post-Probe fast variable elimination was tested as canonical BVE with
the owner's four-round schedule and admission bounds. Corrected BVE-only AM185
times out on WV95 both alone and with MAB, and MAB+fastel also times out on
WV96. The rejected variant has been removed rather than retained as another
strategy branch.

The native parity successor recognizes complete arity-2--4 XOR systems with
even incidence and odd right-hand-side sum. Its eager recognizer allocation is
now bounded by actual input size. WV80 checks, and AM197 independently verifies
WV83 and WV86: the 65.364/41.685s solves emit 1.741/2.625 GB DSR certificates,
and both pass dsr-trim and canonical Trestle. The
typed Lucky7 successor now recognizes complete bipartite exact-one incidence
systems and emits a modulo-three ER refutation when the color classes differ.
Default-policy job AM194 independently checks WV25/26 in
12.486/17.645s at 6 MiB solver RSS; both native DSR certificates pass dsr-trim
and Trestle. AM195's canonical-Trestle replication verifies WV25 in 814.133s
total and WV26 in 1,636.742s total. No filename, rank,
family, size-class dispatch, external solver stage, or sequential portfolio is
present. The remaining open owner gaps are WV1's SGEN counting proof and WV95.

AM189's independent parity fuzz campaign completed 10,000-case default and
random-policy quick legs with zero failures and found two bounded gating bugs.
The candidate-side eager-reservation defect is fixed in `2e429368`. The pinned
dsr-trim RAT-candidate bitmask defect is repaired by the repository checker
patch: capacity now grows past `mask_index`, and setup runs the minimized
24-clause certificate through dsr-trim and canonical Trestle. The owner found
zero canonical/optimized Trestle divergences across 31,949 streams. AM218 closes
its R1 scheduling finding: DefaultPolicy now offers the bounded typed parity
action before structured-root mutation, and the 3,000-variable reproducer
solves in 0.401s with native DSR verified by dsr-trim and canonical Trestle.
AM219 preserves the focused Phase-1 gate: Rank 1 solves in 1.018s and Rank 6 in
53.359s versus AM149's 1.018/52.802s, with identical decisions, conflicts and
propagations and independently verified proof/model results.

AM217 rejects a fixed random-seed explanation for WV95: all eight diagnostic
seeds time out at 1,200s, spanning 12.36--14.64 million conflicts. AM220 now
dumps SC2024's residual after preprocessing and before its first decision.
SC2024 solves the original in 113.43s and its corrected residual in 155.24s,
while disabling either fast elimination or its local subsumption times out at
1,200s. The integrated typed startup fast-elimination schedule now matches the
owner's 2,537 eliminated pivots; BVE costs 15ms and local SubsumeForward 148ms
on WV95. AM231 preserves Rank 1/6 at 1.069/53.461s, but exact owner scheduling
and disabling structured-root SR both still time out on WV95; the final
occurrence-compacted AM238 candidate also reaches the 1,200s limit. This
narrows the remaining gap to the post-preprocessing formula/search state rather
than fastel runtime or structured SR alone.

The cumulative successor Release suite passes all 169 tests. AM278 restores
WV99 through cross-stage compound SR feedback: one singleton cut remains
withheld and a later complete graph stage publishes one 761-literal certified
cut. WV99 checks SAT in 99.651s; Rank26 and C32 controls check in 312.170s and
46.778s. Material FastEliminate feedback now admits MAB at search entry only
for a minority reduction. A dominant reduction preserves a baseline
opportunity on its compact residual, while nonmaterial reduction retains the
normalized delayed gate. AM280 checks the immediate owner path on WV95 in
481.146s and the nonmaterial WV96 control in 961.748s. AM281 checks the refined
WV87/WV95/WV96/WV97 paths in 1.869/480.942/964.896/100.663s. These predicates
use producer outcomes only and contain no formula, benchmark, rank, size-class
or elapsed-time dispatch.

AM282 confirms the critical matched controls: Rank 1 checks in 1.018s and Rank
6 in 41.891s with 0.206s structured startup, improving on AM149's 52.802s
Rank-6 baseline and confirming the SR-overhead repair. Its uncapped FastEl pass,
however, spends 142--178 million resolutions on large Rank formulas and causes
avoidable aggregate overhead. AM283 shows that declining all near-universal
candidate domains recovers the Rank time but loses WV96, so that gate is
rejected. AM282 jobs 1704969/1704970/1704971 were cancelled after retaining
their completed artifacts once a validated successor was available.

AM284 replaces the unlimited startup exception with one standard preprocessing
BVE dose: 1,000,000 resolution ticks derived from the existing 10-million-tick
initial effort reference and 100-permille BVE share. Job 1705050 independently
checks Rank 7/8/10 in 74.770/91.438/128.486s while stopping at about one million
resolutions. C32 and WV95/96/97 check in 46.677/482.755/971.064/98.798s and
retain their complete required FastEl trajectories. All seven original-CNF
models pass, cleanup is clean, and the exact source passes all 169 repository
tests.

AM285 shows that resolution-only accounting still pays 18--33s to discover
3--53 million candidate variables before applying the cap. It also turns the
truncated Rank-9 producer into immediate MAB feedback. Rank 1/6 remain repaired
at 1.119/42.395s, but the later aggregate trajectory is rejected. Its jobs
1705074/1705075/1705076 were cancelled after retaining completed evidence.

AM286 charges the mandatory live-domain discovery lower bound and resolutions
to one typed preprocessing BVE dose, declining before allocation or mutation
when discovery cannot fit. Job 1705110 checks Rank 7/8/9/10/13/14 in
57.843/69.109/105.457/97.746/77.168/76.019s, while C32 and WV95/96/97 retain
their complete paths in 46.028/473.101/992.310/97.702s. AM286b recovers the
large Rank-18/20/22 regressions to 166.140/132.496/246.278s and preserves the
productive Rank-24 path at 231.526s. Rank 17 shows that discovery consuming
most of the dose still permits a weak partial mutation.

AM287 therefore reserves one transformation tick per discovered live variable
from the same dose. Rank 17 improves from AM286's 137.602s to 91.207s; Rank 24
remains productive at 235.086s; WV96 checks SAT in 1,006.269s with its complete
148,064 discovery ticks and 707,807 resolution ticks. The exact source passes
all 169 tests. The shared AM288 binary is frozen at
`ca9c4f93b264ff1ae2aeac4c08bab24cc2b4d3d1427ed6d233c1a44412d7accf`.
Full-width checked campaigns are submitted as jobs 1705159 (Rank 1--50,
matched c12), 1705160 (B28+C5, matched c12), and 1705161 (WV100, c8); every
case has 32 GiB and a 1,200s solve limit. Independently verified live 750 GB
guards may hold pending tasks but never interrupt running work; measured usage
at submission is about 196 GB.

AM288 now has complete solver output for both Phase-1 suites. Rank is 50/50 in
8,094.974s and B28+C5 is 33/33 in 1,099.746s; their native proofs are still
finishing. The B aggregate regression is concentrated in a duplicated
trajectory change: productive structured SR suppressed MAB but the later
FastEliminate pass still mutated the same residual whenever SR had not produced
ULC helpers. Reconstructing the AM288 and first repaired production trees from
their pinned patches shows that the initial broad producer-feedback predicate
and its parameter name were the only production-source differences.

AM289 job 1705512 confirms the repair on cpu-q: B7 falls from 186.209s to
7.276s, Rank32 from 753.374s to 366.005s, and the unchanged Rank29 and
WV95/96/97 paths solve in 47.533/473.646/1,032.522/99.054s. Rank33 moves from
324.157s to 472.297s, leaving the paired Rank32/33 result 239.229s faster.
Native proof checks for the two Rank controls remain active. A policy-level
regression test now exercises the zero-ULC productive-SR outcome through
`DefaultPolicy::solve`; the complete Release suite passes 169/169.

AM290 job 1705542 froze a matched c12 amendment for that broad predicate but
was cancelled before any task started when the faster cpu-q screen rejected
the candidate. AM291 shows why: suppressing FastEl after clause-only SR moves
B32 from 42.347s to 168.440s and would put the diagnostic B composite near
1,002.364s. The durable producer outcome is root-trail growth, not every SR
application. Every affected Rank case and the large B3/B7/B29 wins rebuild the
residual search seed; B32 does not. The refined policy therefore suppresses
FastEl only after structured root assignments, while clause-only SR retains the
owner pass. Its policy-level test uses a checked SR unit with no ULC output.
The paired policy-level regression now also applies a checked clause-only SR
step and verifies that FastEl still runs, directly covering the B32 failure
mode. The complete Release suite still passes 169/169.
AM292 confirms B7/B29 at 7.276/13.686s and restores the clause-only B32 control
from the broad gate's 168.440s to 46.079s. The exact renamed-telemetry Release
binary is `d5c04553d66ee451f73cdf5561121b9c8a7f7c6ab1b63eb1f42fbbeba4b1416b`
and passes all 169 tests. A fail-closed review found that the initial 13-case
AM293 selection omitted two Rank cases and one B case where AM288 reached its
ULC-suppression branch without a residual handoff, so its telemetry could not
prove agreement with the new root-trail predicate. Job 1705577 was cancelled
before any task started. The first corrected AM294 submission was likewise
cancelled before starting when the AM295 cpu-q preflight failed closed and
showed that its copied manifest lacked hashes for the three added inputs; no
solver ran under either defective manifest. Matched c12 AM296 job 1705592
binds all input hashes and covers the conservative complete set: eight Rank
and eight B cases. Its source bundle
reconstructs the current production tree from the AM288 patch and the same
four-file semantic/telemetry delta. Job 1705161 remains held so WV100 cannot
begin before this corrected Phase-1 composite passes.

Corrected cpu-q preflight AM297 job 1705594 checks all three ambiguous cases.
Rank26 confirms root-trail growth and retains suppression, solving in 300.043s.
Rank34 and B24 have helper-only ULC output, admit FastEl under the refined gate,
and solve in 670.369/13.386s; their FastEl passes cost only 0.106/0.137s.
AM288 matched times were 281.027/637.007/10.387s. Even conservatively mixing
the slower cpu-q values into the current diagnostic composites leaves about
196s of Rank margin and 75s of B28+C5 margin. All three original-CNF
model/proof checks pass and cleanup is clean.

AM296 matched c12 acceptance has complete solver output for all 16 selected
cases. Rank26 and Rank34 are final with verified original-CNF models at
278.462/616.250s. Rank27/31/32/33/35/37 solve in
392.277/397.310/345.469/441.323/523.524/442.441s and their native proof checks
remain active. All eight B amendments are independently final: ranks
3/4/7/12/22/24/28/29 solve in
3.071/10.032/6.627/5.825/5.325/12.487/22.652/13.838s. The fail-closed composite
has 32/33 B results; only AM288 C33's active proof check remains. The matched
solver projection is 872.481s for B28+C5, 79.782s below its gate, and
7,739.079s for Rank, 376.656s below its gate. AM296 remains the matched
acceptance authority; projections cannot replace the ten remaining final proof
records across AM288 and AM296.

Lucky16's focused unit test now also invokes its public registry instance
directly. The real-binary micro-proof test requires the signed-cardinality reach
counter, verifies its native DSR through dsr-trim and Trestle, and confirms that
an incomplete certificate is not reported VERIFIED. The frozen solver remains
byte-identical at the AM292/AM296 SHA-256.

Rank22 exposed a separate proof-elaboration bottleneck: the AM288 solver finishes
in 230.131s, but pinned dsr-trim repeatedly scans about 1.17 million global units
to emit refutations that often use one hint. Samples spend effectively all time
in `mark_dependencies` and the following full-trail hint scan, and historical
native checks time out even at 43,200s. The repository-pinned checker patch now
uses a sparse dependency worklist and ordered sparse hint emission in forward
mode, retaining the dense scan when that is cheaper. It produces byte-identical
LSR or rejection output on 14 local valid/invalid certificates, passes the full
checker setup including Trestle and malformed-proof controls, and adds a
many-root-unit sparse-dependency micro proof. The complete current-tree suite
passes 169/169 with the frozen solver still byte-identical. AM299 job 1705668 is
checking the exact 744,682,514-byte Rank22 certificate on cpu-q; its proof SHA
matches AM288, and profiling confirms the two full-trail scans are gone.

AM299/AM300 established that the experimental sparse elaborator is not an
acceptance path: it reports the exact Rank22 DSR as verified but canonical
Trestle rejects the resulting 1,355,412,830-byte LSR.  The result is retained as
negative evidence and does not amend Phase 1.  At the user's direction the
proof-checker implementation is now frozen; subsequent proof work uses the
original AM288-pinned dsr-trim (`3b5c7737...`) and canonical Trestle
(`19bf79f4...`) without source changes.  AM301 remains evidence that its exact
Rank47 stream happened to verify, but the original-checker path is being run as
the primary amendment authority.

The Phase-1 base jobs have now made the remaining validation boundary exact.
Rank18 finishes solving in 150.820s, then original forward elaboration succeeds
in 6,873.319s but Trestle exhausts the shared 16,000s budget; Rank22 finishes in
230.131s and original forward elaboration exhausts that budget.  Both are
validation failures, not solver timeouts.  Rank47's original forward elaboration
is still active, and C33 plus four AM296 Rank proofs remain in Trestle.  AM302
job 1705743 checks the retained exact Rank22 and Rank47 certificates with the
original checker's existing backward mode under one shared 43,200s budget.
AM303 job 1705754 deterministically regenerates Rank18's exact certificate
(`d0d93153...`, 1,634,186,473 bytes) with the frozen AM288 solver solely for
proof validation, then applies the same frozen backward-mode pipeline.  The
AM288 150.820s result remains its runtime authority.

At the user's later direction, the already-frozen WV100 c8 cpu-dense campaign
1705161 was released before the remaining Phase-1 proof checks close.  Its
immutable bundle uses the original dsr-trim SHA `3b5c7737...`, not the later
experimental binary.  After ranks 1--81 had either completed or begun, the
user selected cpu-q for the remaining WV acceptance work because this campaign
needs high-level solve/validation outcomes rather than matched runtime.  The 19
still-pending cpu-dense elements for ranks 82--100 were cancelled without
interrupting any running task.  Immutable AM304 job 1705800 now runs exactly
those ranks on cpu-q at c8, 32 GiB, and 1,200s with the identical AM288 solver,
original dsr-trim, canonical Trestle, inputs, and anonymous cleanup protocol.
The composite audit binds both manifests and assigns every rank to exactly one
campaign.  Independent 750GB guards remain live for both arrays and can hold
only pending tasks.

The cpu-q continuation immediately recovered the scheduling backlog.  Its
checked SAT results include WV98 in 266.122s, WV95 in 473.514s, and WV96 in
971.311s, all with independently verified original-CNF models.  The exact
dense/cpu-q composite is now 100/100 solver-complete in 2,517.573 aggregate
seconds and 95/100 final with no invalid result.  WV61's 1.468s UNSAT solve
elaborated in 16.944s and passed canonical Trestle in 4,473.208s; its exact
4,663,065-byte DSR and clean anonymous-artifact cleanup are recorded in the
frozen result.  WV81's 0.868s UNSAT solve
elaborated in 7.428s and passed canonical Trestle in 2,221.096s, with clean
anonymous-artifact cleanup.  WV60's 1.269s solve passed the same frozen pipeline in 5,506.034s.  The five remaining WV rows are live UNSAT proof
pipelines.  The fail-closed composite auditor has also
rehashed all 100 compressed inputs and both frozen bundles successfully in
deep-input mode.  AM296 Rank35 also completed its
original-checker pipeline: the
523.524s solve elaborated to a 3,194,954,201-byte LSR, and canonical Trestle
verified it in 5,057.515s.  Phase 1 therefore stands at Rank 47/50 and B28+C5
32/33 final.  Rank18/22/47 remain live in the frozen backward checker.  C33's
305.716s solve produced the exact 733,177,634-byte DSR; frozen original
`dsr-trim` verified it and emitted a 798,324,802-byte LSR in 365.062s, but
canonical Trestle exhausted the remaining shared 15,635 seconds after parsing
the formula.  AM305 job 1706070 regenerated the exact certificate hash on
`cpu-q`, then the unchanged original checker verified it in backward mode:
`dsr-trim` took 87.02s and emitted an 89,192,903-byte LSR, and canonical
Trestle took 329.69s.  The fail-closed Phase-1 audit now proves B28+C5 33/33 in
872.481 aggregate solver seconds, 79.782s below its 952.263s gate.  The proof
checker remains unchanged.

Claude session `be5fc4a2` completed its terminal fuzz report after random-policy job 1706066 found seed 296326708.  The localized proof contains an implied-unit
binary deletion followed by a legal SR unit.  The frozen checker rejects its
default conservative formula, which retains the deleted implied-unit reason;
the same frozen `dsr-trim` with its official `--delete-implied-units` mode
verifies the full UNSAT proof and unchanged canonical Trestle verifies the
resulting LSR.  The defect is therefore in the harness invocation rather than
the solver or SR rule.  Canonical benchmark, fuzz, competition, and micro-proof
paths now request that mode, with a causal complete-proof regression.  The
cancelled full fuzz legs are being rerun on the unchanged AM296 solver by
Claude session `0d119d00`; its terminal report remains required for promotion.

## Exact next action

Finish the frozen original-checker AM302/AM303 Rank proof amendments, then
fail-closed score the corrected Rank Phase-1 composite; the B28+C5 composite
already passes 33/33 in 872.481s.  In parallel, let dense job 1705161 through
rank 81 and cpu-q
continuation 1705800 for ranks 82--100 finish, then audit the exact composite
coverage and every SAT model or UNSAT DSR-to-LSR-to-Trestle result.  Use its
checked timeout set to select the next shared Tactix technique; do not change
the frozen proof checker, cancel running jobs, or submit another full Tier-D
campaign.  Keep both storage guards and all frozen authorities intact.
<!-- track-sched:end -->
