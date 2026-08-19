# Progress

`DEVELOPMENT_PLAN.md` owns scope and gates, `docs/calculus.md` owns proof-rule
semantics, and `docs/implementation-notes.md` owns durable implementation
decisions. This file is only the current resume surface.

## Current state

- Phase 4 has met T3: the matched Tier B/C window solved 28/28 and 30/30,
  respectively, within the 1.10x Kissat gate; all results were checked.
- N1 and N2 are complete. N4 is immutable and its verification is pending on
  the cumulative general random-decision, post-Lucky compaction, Vivify-tier,
  and Walk compact-order replay repairs' checked 28B+fixed5C and frozen ranks
  21--40 gates. N3 awaits the canonical Tier-D gate.
- Phase 4.20 proof-rule and strategy surveys are drafted. Project review and
  the P1 compound-instance API decision remain open.
- The current diagnostic candidate release binary is
  `ded2310246672785734e10839a7ac2871d4a7f8aaf8a51757deb555d0d3f5712`.
  It closes all ten ranks 11--20; the cumulative ranks 1--20 debug view has 19
  checked answers and one reproduced near-cap memory unknown at rank 10.

<!-- track-sched:begin -->
The scheduling-parity register is `docs/scheduling-parity-ledger.md`. P4-CVG is
closed; sparse-GC's repaired implementation and the Reduce candidate population
are reconciled as closed. `SP-RANDEC-1`, `SP-COMPACT-1`, `SP-COMPACT-2`,
`SP-VIVIFY-1`, and `SP-WALK-WATCH-1` verification is temporarily pending. The cumulative
candidate removes formula-shape random-policy overrides and the extra
post-Lucky compaction, poses focused tiers at every Vivify consumer, and
replays Walk binaries in Kissat's compact-literal encounter order. Reduce-time
variable compaction preserves each directed binary prefix and cannot invoke an
optional arena-ordering policy during its representation-only collection.
The rejected physical-order candidate is superseded by inherited source-order
replay for appended replacements. Release is 72/72, Trace is 71/71, and
ASAN-O3 is 72/72. The cumulative Slurm pin closed Rank 29 but failed the fixed
Tier-C correctness gate on `asconhashv12`; its proof-population repair is now
the active N2 prerequisite. Any new missing or cruder reference mechanism
returns N4 verification to pending.
<!-- track-sched:end -->

## Active measurement

The canonical SC24--26 Tier-D campaigns are:

- baselines: `benchmarks/baselines/sc24-26-32g-5000s/`
- Tactix: `benchmarks/runs/20260813-tierd-tactix-sc24-26-32g-5000s/`

They cover the 1,166-instance union with a 5,000 s solve limit and 32 GiB
process limit. The baseline has five arms: Kissat 4.0.4, kissat-sc2024,
AE-Kissat-MAB, Satsuma+Kissat, and LymphoSAT. LymphoSAT contributes SAT results
only to VBS comparisons.

Each solver run records solve/check time and peak RSS, verdict, proof size/hash,
and immutable solver/checker provenance. SAT models and the SC2026-selected
UNSAT pipeline are checked. Each checker stage deletes its consumed proof, and
the task removes all remaining proof scratch before publishing its JSON result;
the aggregate rejects any surviving certificate.

Do not analyze partial results. After both campaigns and their aggregate jobs
finish, audit completeness and cleanup, compare Tactix with Kissat annually and
on the union, then select cases where Kissat solves quickly and Tactix times out
or is materially slower for iterative divergence analysis.

### Pre-Phase-4.5 debug contract

For all newly submitted Tactix convergence/debug campaigns before Phase 4.5,
use a 16 GiB process limit (two CPUs at 8 GiB/core) and a 1200 s solver timeout.
Do not rerun the frozen reference solvers: simulate their matched view from
`benchmarks/baselines/sc24-26-32g-5000s/` by exact 1200 s time truncation and
the recorded peak-RSS test for 16 GiB. Label the memory projection as simulated;
it cannot identify the exact crossing time or prove an OOM. These faster runs
are iteration evidence and do not supersede the canonical 32 GiB/5000 s gates.

Submit the debug arrays with no `%N` concurrency throttle or hard node
constraint, and expose every task so SLURM can use all available capacity,
including `node017,node018`. Preserve one single-threaded solver per task,
immutable pins, correctness checking, and proof cleanup.

For this debugging phase as a whole, every candidate runs against shipped
Tactix on all of Tier B plus a predeclared mechanism-relevant Tier C subset in
the same window. The Tier C subset includes known productive, worst-cost, and
low/no-yield witnesses when available. Checked proof/model failure or an
unexplained solve loss blocks promotion to the targeted Tier-D cases; this
iteration guard does not replace the full Tier C or canonical Tier-D gates.

The active divergence round targets the twenty predeclared Tier-D ranks 21--40
Kissat-over-Tactix gaps. These are improvement witnesses, not regression-test
members: general repairs are guarded by Tier B and selected Tier C, while the
Tier-D set remains cumulative where practical to expose both closed and still
open gaps. Different gaps may require different source-backed mechanisms.

### Current ten-target divergence round (2026-08-17)

The frozen target list and shipped comparison are in
`benchmarks/runs/20260817-0030-tierd-top10-hybrid/`. The final candidate-only
gate is `benchmarks/runs/20260817-1325-tierd-top10-final/`: all ten targets solve
under 16 GiB/1200 s, all four UNSAT proofs and six SAT models validate against
the original CNFs, and no certificate residue remains. Solve-time PAR-2 is
1469.070 s; maximum peak RSS is 16,118,220 KiB. The frozen shipped Tactix
32 GiB/5000 s results project to zero eligible solves under the simulated
16 GiB/1200 s view.

Ranks 1--4 close through universal startup Congruence/Factor admission; ranks
5 and 10 through proof-format-aware binary identity, exact Substitute watch
packing, output-sensitive watch growth, and streamed dense scratch; rank 6
through Factor's stamped binary relation and cached live degree; and ranks 7--8
through compact/paged Congruence storage. Rank 9's common-state trace matches
Kissat for the first 99,913 decisions and 79 conflicts but exposes 3,107,244
physical variables versus 1,766,908 active variables. Post-Lucky compaction
removes that representation gap, and persistent sparse BVE scratch reduces
identical BVE work from 913.347 s to 109.162 s. The final rank-9 solve is
223.611 s at 5,827,952 KiB; rank 10 is 106.013 s at 16,118,220 KiB.

The final iteration guard is
`benchmarks/runs/20260817-1325-BC-final-regression/`: 28/28 Tier B and the
frozen 5/5 mechanism-relevant Tier C cases are proof/model clean with zero
residue. Against the preceding pin, Tier-B aggregate solve time improves from
1275.910 s to 956.795 s; Tier C remains flat at 1442.447 s versus 1447.383 s.

### Next ten-target divergence round (2026-08-17)

Ranks 11--20 were predeclared from the same frozen eligible Kissat-over-Tactix
view, excluding the closed ranks 1--10. Shipped Tactix closed 6/10 with PAR-2
10576.236 s in `benchmarks/runs/20260817-1350-tierd-next10-predeclared/`; the
preceding checkpoint closed 9/10 with PAR-2 4140.349 s. The final candidate
closes 10/10 with a 1693.565 s solve-time sum in
`benchmarks/runs/20260817-2220-tierd-cumulative20-backbone-ramp-final/`.

Rank 12 now uses a scoped exact live-binary index, occurrence-driven gate
rescheduling, and direct structural lemma emission in DRAT/proofless modes;
native LRAT retains antecedent replay. Its checked UNSAT solve is 13.644 s
versus a shipped timeout and Kissat's 5.574 s. Rank 16 retires DIMACS-header
variables that never occur while preserving their external model identities.
This removes the first traced semantic mismatch (1,416 versus 1,086 active
variables) and yields a checked SAT solve in 74.158 s versus a shipped timeout
and Kissat's 124.551 s.

The original rank-19 watch mismatch had two earlier causes. Substitute rebuilt
moved binaries from an undirected image rather than each retiring directed
source list, and stable heap fill used activation order rather than Kissat's
first-import internal-index order. After those repairs, the remaining tie came
from Backbone: Tactix independently rolled back every successful probe, while
Kissat retains one assumption trail and learns the first repeated binary
predecessor. The canonical V2 batch now follows that algorithm, including its
private meter and 100-times-computation/1000 round ramp; the public literal
instance stays independent for N1. It derives two units on `knight_18`, fixes
20 root variables, matches all preprocessing formula counts and the stable
top-32 heap, and then matches Kissat for 85,969 decisions through conflict
1,339. The checked SAT solve is 542.977 s with a verified model.

The final iteration guard is
`benchmarks/runs/20260817-2220-BC-next10-backbone-ramp-final/`: Tier B is 28/28
and the frozen Tier C subset is 5/5, with solve-time sums 765.943 s and
1415.191 s. All proofs/models validate and certificate residue is empty. The
cumulative ranks 1--20 campaign has 19 checked answers, PAR-2 5499.998 s, and
zero residue. Rank 10 alone is memory-unknown at 49.165 s; the accepted
pre-repair binary reproduces the same `bad_alloc` at 49.818 s, so this is the
agreed near-cap debug classification rather than a source regression.

### T105 V6a disposition and ranks 21--30 (2026-08-18)

Repaired Backbone does not make V6a redundant on `T105`. Paired same-node job
`1631961` in `benchmarks/runs/20260818-0110-T105-backbone-v6a-ablation/`
records zero Backbone failed literals in both arms. V6a-on derives 501 units and
solves checked UNSAT in 365.861 s; V6a-off derives none and takes 430.693 s,
with 346,574 versus 495,955 conflicts and a 66,107,192 versus 166,616,967-byte
proof. V6a is retained with one complete linear pass per admitted Congruence
application and separate `congruence_y1_ticks` accounting.

The post-repair iteration guard is
`benchmarks/runs/20260818-0125-BC-v6a-budget-final/` (job `1631978`): Tier B
is 28/28 and the frozen Tier C subset is 5/5, with solve-time sums 630.845 s
and 1285.260 s. All 17 SAT models and 16 UNSAT proofs validate, peak RSS is
2,001,780 KiB, and every task reports complete certificate cleanup.

The next ten gaps are frozen, before any work on them, in
`benchmarks/runs/20260818-0120-tierd-ranks21-30-predeclared/`. Selection is the
fastest completed Kissat-4.0.4 eligible view at the freeze, excluding ranks
1--20; because the canonical baseline was still running, this is an immutable
available-results ranking rather than a claim about the eventual global top 30.
No baseline job was rerun.

| rank | expected | instance basename | Kissat s | shipped Tactix s |
|---:|:---:|---|---:|---:|
| 21 | SAT | `ac3887576c2ce781202b9fe0fdc31420-pj2008_k80.cnf.xz` | 106.338 | 1889.889 |
| 22 | UNSAT | `113e692e379518bc3277855277dcc1b8-oisc-subrv-sll-nested-8.cnf.xz` | 196.556 | timeout |
| 23 | SAT | `994783ef4ed3a0366842e1b6f9128a6f-noL-11-6.sanitized.cnf.xz` | 212.567 | 1242.639 |
| 24 | UNSAT | `29b573e0f7b922c4bd0a675db005265f-b18.cnf.xz` | 221.203 | 1504.167 |
| 25 | SAT | `180e6d8b61fe6bab26126b78591e324e-baseballcover11with22_and2positions.cnf.xz` | 263.773 | 3209.790 |
| 26 | SAT | `0a27eb7c16c1e69ff4d087d217ac89cb-noL-11-0.sanitized.cnf.xz` | 266.921 | 2116.037 |
| 27 | UNSAT | `5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz` | 278.187 | timeout |
| 28 | SAT | `9541d12f3e3d9201085c13f25145d2f7-cabp-V-nos6.mtx.rnd-k275.sanitized.cnf.xz` | 286.337 | 1290.399 |
| 29 | SAT | `16564a6acba6b9fc887c447d4e507767-ER_400_20_7.apx_2_DS-ST.cnf.xz` | 294.468 | 1780.110 |
| 30 | SAT | `ab17eed90e4da47767f72f518e3b4a26-vdwb_k6_n250.sanitized.cnf.xz` | 304.761 | 1386.023 |

Ranks 31--40 are frozen in the same immutable manifest as ranks 21--30. The
pre-Walk-repair cumulative candidate job `1633266` solved and checked ranks 21,
23, 25, 28, 29, and 38 under 16 GiB/1200 s; the other fourteen timed out. Its
28B+5C guard job `1633265` was 28/28 Tier B and 4/5 Tier C, with only
`mp1-Nb5T15` timing out. That regression is now localized to Walk's compact
binary replay: job `1633433` restores the first divergent watch order, matches
all 66 Kissat analysis records through conflict 2008, and solves the traced
instance SAT in 24.708 s. This is mechanism evidence, not promotion; the
current cumulative candidate still needs checked 28B+5C and ranks 21--40.

Rank 29 exposed a second compaction-order issue after the post-Lucky lifecycle
repair. Before conflict 1000 Reduce, Tactix and Kissat had the same directed
binary order; the old variable remap reconstructed both directions from an
undirected stream and reversed that order immediately afterwards. Preserving
each directed binary prefix repaired it. A broader experiment that also
preserved every large-watch vector was rejected: although it repaired the
observed conflict-1006 pair, it introduced an earlier conflict-1003 semantic
divergence and slowed the checked solve. The accepted general repair instead
prevents representation-only variable compaction from invoking the
solve-scoped optional arena-ordering policy, matching Kissat's retained arena
sequence before large clauses are rewatched. Its regression installs a
synthetic reverse ordering and retires an unrelated variable; it contains no
benchmark, rank, literal, conflict, hash, or deadline condition.

Trace job `1633732` matches all 16,032 normalized decision, implication,
conflict, and analysis records from conflicts 980--1100 and all 4,666 score
bumps from conflicts 980--1044, including both formerly divergent watch
frontiers. Focused runtime job `1633733` nevertheless times out at 1200.004 s
after 27,240,776 conflicts, so this candidate is rejected pending the first
later traced divergence. Diagnostic job `1633808` covers conflicts 1080--1400.
Checked 28B+5C job `1633745` and frozen ranks 21--40 job `1633760` remain in
flight as cumulative regression evidence, not promotion gates for this pin.
The B/C job has since completed 28/28 Tier B but only 4/5 Tier C:
`mp1-Nb5T15` falls from a checked 14.349 s SAT solve to timeout, while
`mod4block` rises from 305.421 s to 790.619 s. It independently rejects the
candidate. Its completed Tier-D array closes only ranks 21, 25, 28, and 38,
losing predecessor ranks 23 and 26 and failing to recover rank 29.

The first later representation divergence is conflict 1211, scan `846`, after
all 40,382 portable semantic records through conflict 1400 still match. A
replacement with clause ID 92812 inherits source position 92810 but the
rejected patch keeps it after ID 92811; Kissat retains it at the source
position and visits it first. The new general candidate orders every live
clause by the existing inherited source-position key during compaction. Its
synthetic regression distinguishes physical `102,103,104`, reverse-policy
`104,103,102`, and inherited `104,102,103` order. Trace job `1633836` matches
Kissat exactly for 53,910 portable and 223,125 normalized-watch records over
conflicts 980--1400, including the repaired conflict-1211 frontier. The old
order stays latent until conflict 2272, where encounter and then raw learned
order differ. Focused runtime job `1633837` still times out at 1200.004 s after
28,656,897 conflicts with zero residue, so the pin is not promoted and no
cumulative arrays are submitted. Continuation job `1633890` matches another
107,804 portable records before the same conflict-2272 semantic split and
localizes its next cause to Reduce at conflict 2001: Kissat stably partitions
the swept tail into irredundant then redundant clauses before rewatching;
Tactix retained the interleaving. Trace job `1633950` proves the first general
tail-partition patch inert because root assignments route Reduce through the
source-position replay in `kernel_sparse_collect_root`; obsolete runtime job
`1633951` was cancelled. The composed candidate now preserves rewrite position
within each redundancy class and stably partitions the classes. Separate
non-root and rewritten-root mixed-tail tests distinguish physical, policy, and
reference order; Release 72/72, Trace 71/71, and ASAN-O3 72/72 are clean.
Trace job `1633964` rejects that candidate at the unchanged conflict-2001
watch frontier, after 63,075 matching normalized watch records in the focused
window; obsolete runtime `1633965` was cancelled with zero certificate
residue. The actual general defect is collection admission: Reduce marks its
victims before entering `kernel_sparse_collect_root`, but that path formerly
compacted only if root simplification independently changed another clause.
Thus its 22 conflict-2001 victims remained garbage in the arena and no sparse
rewatch occurred. The new candidate collects whenever pre-existing garbage or
root simplification requires it. A generic test uses an unrelated root unit and
makes an already-marked victim the sole reason to collect, while also checking
the stable mixed-tail replay. Release 72/72, Trace 71/71, and ASAN-O3 72/72 are
clean. Trace job `1633999` then makes all 61,653 portable records exact through
conflict 2400 and equalizes the watch-stream lengths at 297,047, but the first
watch mismatch at conflict 2001 reveals a replacement inherited before the
collection still physically appended behind two later clauses. Runtime
`1634000` was cancelled with zero residue. The follow-up now replays the swept
tail by persistent source-position key, using one decorated lookup per clause,
then stably partitions the redundancy classes. The generic garbage-only test
also places an appended replacement before a physical middle clause without
any witness constant. Claude Opus diagnosed one concurrent Trace-suite abort
as fixed `/tmp` filenames racing across build directories, not solver state;
all remaining fixed-name test artifacts are process-unique. Serial trace stress
passes 30/30, concurrent Release/Trace/ASAN stress passes 60/60, and sequential
Release 72/72, Trace 71/71, and ASAN-O3 72/72 are clean. Focused trace job
`1634075` matches all 61,653 portable and 297,047 normalized-watch records
exactly through conflict 2400, closing both the conflict-2001 source-order
frontier and its conflict-2272 consequence. Checked runtime job `1634074`
solves Rank 29 SAT in 130.767 s at 68,524 KiB and verifies the model, versus
the frozen eligible Kissat result of 294.468 s, with zero certificate residue.
Unthrottled cumulative jobs `1634094` (28B+5C) and `1634095` (frozen ranks
21--40) completed under 16 GiB/1200 s. The Tier-D job checks ranks 21, 23, 25,
28, 29, and 38 (six of twenty), adding 23 and 29 with no predecessor losses;
all six answers validate and certificate residue is empty. The paired guard is
28/28 Tier B but only 4/5 Tier C: `asconhashv12` solves UNSAT in 101.224 s,
then drat-trim rejects its proof at unit `-1997`. The pin is therefore rejected
despite Rank 29's closure.

Hundreds of surplus deletions first exposed non-idempotent large-clause
retirement and a stale deferred-OTFS reason; both are repaired with generic
tests and a compile-time proof-population shadow. Job `1634219` falsifies those
as Ascon's blocker: production/shadow reproduce the same rejected proof in
96.988/97.438 s without a shadow abort. Raw-clause seeding then catches a real
generic N2 defect on `dupbinary.cnf`: the parser stored `-4 3` from raw
`-4 3 3`, but DRAT never received that exact deletion multiset. Both formats
now add every stored narrowed input clause at `finish_input`; LRAT keeps hints
and DRAT preserves solver ids. Shadow/Release/Trace/ASAN-O3 pass 72/72, 72/72,
71/71, and 72/72. Job `1634231` nevertheless falsifies restatement as Ascon's
blocker: both arms add 643 clauses and still fail at `-1997` in 96.588/97.139 s
without an unbacked deletion. Forward job `1634233` verifies every earlier add:
unit `-1997` itself (checker id 1354312, proof line 768571) is the first invalid
inference. Adjacent equivalence adds and 39 V6a/Y1 units make Congruence's
binary-resolution unit channel the leading source. Isolation job `1634236`
turns only Y1 off and checks UNSAT in 107.956 s plus 60.224 s verification;
the 63,635,534-byte proof is valid with zero residue, so Y1 is necessary on the
bad trajectory. Shadow job `1634243` reaches the unchanged failure without
aborting: both premises of every direct Y1 unit exist in the checker. Thus Y1
changes upstream root state but is not the bad unit's direct emitter. Label the
Congruence unit channel in shadow job `1634245`: `-1997` is a value-derived
`Canon::Unit`, not direct Y1. Compile-time forced replay campaign
`20260818-2212-ascon-value-unit-replay`, job `1634264`, completes valid:
`-1997` is an XOR value-unit and RUP fails inside Tactix. Dropping that one
unjustified inference yields checked UNSAT in 95.785 s plus 57.775 s
verification; its 15,604,302-byte proof verifies with zero residue. This is a
solver-side XOR `Canon::Unit` defect; do not promote the diagnostic fail-closed
guard. Gate-dump job `1634270` finds `1997=XOR(341,1562,1844)`, with `341=0`
and `1562≡1844`: the equivalent pair cancels, but the separate valued input
made the code bypass the mandatory parity-elimination ladder. The repair now
routes every XOR with a nonempty cancelled set through that ladder, including
mixed valued-plus-cancelled gates; its regression and all four suites pass.
Focused job `1634293` verifies; paired guard `1634307` is 33/33. Tier-D job
`1634294` retains ranks 21,23,25,28,29,38 with no gains/losses and zero residue.
The cumulative pin is accepted; V6a stays on.

## Gates

- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: matched Tier B/C Kissat coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26, and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; blocks Phase 4.5.
- [ ] T5: exceed the frozen VBS or ship the best verified effort.

## Next engineering work

1. Rank-22 repair `a9f751c` stops analysis/OTFS conflict 526 from consuming its
   propagation-charged random lifetime and passes 72/72+71/71. `1634419`
   matches through conflict 533/decision 3859653; `1634436` differs at stable
   decision 23748; `1634438` finds four missing bumps; `1634440` excludes their
   direct reason; `1634446` excludes root transactions; `1634449` proves binary sharing.
   `1634451` pins mismatch; `1634453` root parity; `1634457` isolates c611; `1634459` confirms c612 parity; `1634461` pins reduce-end rewatch. `c5a6928` fixes admission; `1634463` exposes profile overrides; `1634464` matches c1000 pose; `1634465` times out cleanly. `bcfe4a0` maps VMTF cursor; `1634466` confirms c1001. `115f40e` compacting Reduce; `1634467` matches c1000. `1634479` exact through c1198; `1634482` heap pose differs; `1634542` c1001 exact; `1634551` d23722 heap differs; `1634558` reinsert mismatch; `1634561` same reason; `1634566` exonerates virtual list; `1634569` pins immediate `-28`; `1634574` pins c611 order. `1634577` localizes first split to Sweep: K end 264 hash `9db104b9`, T 260 `7ff52c81`. Selected binary-add observer passes 71/71+72/72/static/build; pins K `b65aecfe`, T `fa186938`. Next capture `-28` additions.
2. Review the Phase 4.20 surveys and decide P1's compound-instance API.
3. Continue Phase 4.25 Stage A: localize each frozen miss, repair/disposition it, and run both guards.

No performance feature is implemented until the Tier-D comparison and divergence plan are reviewed.
