# Structured-SR progress before the AM8 handoff

Historical excerpt preserved verbatim from PROGRESS during the user-requested
16 September cleanup. These are old checkpoints, not current status. Paths in
the excerpt are relative to the repository root. Current state: [PROGRESS](../../PROGRESS.md).

**Pinned SR66 — complete proof encoding after Claude's repair:** Claude's
terminal handoff is `build/sr62-claude-sigfpe-repair/handoff.md`. Its duplicate
binary repair preserves multiplicity with clause vertices and fixes the SIGFPE.
Codex replaced the handoff's multiple-open-image restriction with a general
DSR encoding using two temporary clauses, retaining all legal SR tuples.
Job1692532 verifies all8 complete ASCII/binary certificates through Trestle,
including both signs, reuse, compaction and later solver extensions; wrong-map
and empty controls fail. The same causal application test fails against r2.
Nine focused Release suites and five ASan/UBSan suites pass. SR66 dual main
is `cc0bcce1…`, diagnostic `5f886ef8…`. The complete Release build passes; API/architecture/document checks pass after
one comment-only correction. Claude SR66 checkpoint is terminal with no correctness failure:2,713
Extension transactions occur in Trestle-verified UNSAT proofs. Its report
`build/sr66-claude-fuzz-checkpoint/report.md` retains natural-random Extension
and unmodified wire-base2 replay reach gaps; do not claim those gates passed.
All57 job1692536 reports49/57 solves, the same vector as SR64; checking continues.
B28+C5 job1692537 solves33/33 (1,100.833s versus950.837s retained);
Rank1–50 job1692538 solves48/50, losing10 to memory and16 to timeout.
Both losses occur in the preceding rowgroup candidate. Rank40 solves but its
Trestle stage exceeds32GiB (SLURM OOM); it is unverified, with cleanup complete.
`solver-gate-comparison.json` in the review directory records all50/PAR1;
current allocations are32GiB versus48,000MiB retained references.
Compatibility micro-proof job1692658 passes DSR/DRAT/native LRAT and model checks, with zero temporary entries
(after a missing pinned model-checker dependency was repaired). Rank10 hits
32GiB, matching the preceding refactor-rowgroup candidate; older orbitopal-kernel
and direct-SR references solve near16GiB. This inherited memory regression remains open. No commit/promotion.
Design, Claude review and evidence: `build/sr66-general-dsr-review/`.

**Current diagnostic evidence:** SR63 typed refinement-only/ULC-only scopes and
an initial AMO/refinement/compaction prefix reach37/57, 36 independently checked
(job1692374; rank60 checker timeout). SR64 adds a matrix-only ULC pass before refinement and reaches
**49/57 solves, 47 independently checked** (job1692437; ranks60/61 checker
timeouts); it loses75/77 to early ULC cost. SR63/SR64 union is51, not a single solver result. Both arrays are terminal
with cleanup complete. Historical SR63/64 main is `68ed31d6…`; SR64 witness `72849c81…`.
SR65/SR66 intended a10M initial ULC budget but referenced a20B constant.
SR67 job1692699 reaches50/57 with an actual10M (witness9cd0b1f5…),
recovering75/77 at8.430/8.729s but losing61; job1692682 traces
75/77 spending their window on repeated ULC checks. Other missing-row leads
and the reporting correction are in `build/sr67-missing-eight-review/design.md`.
SR68 job1692738 reaches51/57 with128 rounds (witness7b82cfcd…). SR69 job1692758
reaches53/57 by removing the fixing-record cap (witness836d4676…);
44/78/79 are checked;61/82/88/91 remain unsolved in that10s screen.
Claude dataflow review is terminal; its unsafe component suppression was rejected.
Design/test evidence is in `build/sr{63,64,65}-*-review/`. Production scheduling
is not promoted; SR66 repairs the SIGFPE, while fresh fuzz and full N1–N4/performance acceptance remain open.

**Resume — fixed-clause validation and complementary coverage:** SR61 keeps
all 31 SR59 solves with identical complete certificate hashes, and reduces
rank77's exact row-check spans from 1.869 to 0.958 seconds. Trace-off/on proofs
and every non-timing trace record match SR60; five focused release suites,
ASan/UBSan online tests and micro proof/model differentials pass. Full screen
job `1692309` has all31 independently checked solves, with identical certificates
to SR59. SR61 main is `48b004ca…`,
witness `9dd65e47…`, with immutable source/pins under the SR61 run authority.
The rule skips only redundant orbit records for clauses fixed pointwise by all
row permutations, after unchanged charge and stop polls. It retains full checks
for mixed clauses, including the extended missing-image negative control.

SR50 is the retained older 42-solve control. Current SR61 loses seventeen
of those fast solves and gains six complementary rows. Trace job1692370 on missing
rows24/34/52/81 completes: all reach Rows, then later generic chains or repeated
structured discovery consume the window. Rows is not simply starved entirely. The bounded design is
`build/sr61-loss-phase-review/design.md`; reuse historical SR50 evidence.
No identity dispatch or production promotion. The [fixed-clause/random-reach report](benchmarks/reports/2026-09-15-fixed-row-clauses-and-random-reach.md)
owns the current evidence; bounded raw records are in the named review directories.

SR55 confirms the first eight row units exactly match Satsuma; SR56 nonunit
residual fingerprints match after that round. SR57's configurable first-AMO
selection reaches 26 checked solves; SR58's ordered remainder reaches 28 checked
solves and the second six row units. Base order still differs from Satsuma.
SR59's exact packed binary index cuts first-transversal validation from about
3.65 to 0.41 seconds, preserving multiplicities and every charge/stop poll.
Dual SR59 main builds match `71541b5d…`, witnesses `7f84e5fb…`; focused release
and ASan/UBSan tests and complete micro proof/model comparisons pass.

SR53's candidate-image repair passes causal signed controls, original replay,
and Claude job `1692093` (1,014 generated cases, no solver failure). Codex job
`1692122` resolves its half-proof negative-control gap: both halves are valid
sufficient prefixes with identical checked LSR; empty proofs fail the complete
pipeline. Preserve the old RED checkpoint. Claude SR59 job1692308 is terminal: 421
generated cases and controls, no correctness failure; both new bases have
checked C API effects and online reconfiguration/resumption. CLI recorded modes
were sampled but never reached FixAndCut, an explicit acceptance gap. SR62
adds validated, configurable joint-positive resource sampling while preserving
the independent boundary arm. Strengthened 4,096-seed tests observe applied
FixAndCut work under both new bases; release and ASan/UBSan random suites pass.
Current SR62 main is `f8daabae…`, witness `ce31db72…`; immutable authority is
`build/pin-sr62-random-joint-limits/manifest.json`. Claude design/handoff CLI95832
is terminal and its feedback is addressed in the SR62 review directory. Fresh
Claude-owned fuzz is terminal with the SIGFPE described above; later acceptance
legs were skipped. No CLI coverage claim is established by that checkpoint.

SR60 added optional row-phase tracing; job `1692304` completes with
identical trace-off/on and SR59 certificates on ranks77/20. The five focused
suites and micro proof/model differentials pass. Its main is `160401bf…`,
witness `d52e94eb…`; immutable source and runner are under
`build/tactix-sr60-row-phase-trace/manifest.json`. Rank77 checks 135 row matrices:
131 rejected candidates cost 1.706 seconds in exact validation, four accepted
ones 0.163 seconds. Matrix completion costs 0.588 seconds and column ordering
0.704 seconds. This is a lead for general exact-check efficiency; keep rejection
semantics and work/stop accounting intact. The
[tracing/index report](benchmarks/reports/2026-09-15-first-amo-packed-binary-and-row-tracing.md)
owns detailed evidence. No commit or promotion; full N1–N4 and production
performance gates remain open.

The retained SR50 ten-second diagnostic vector reaches **42/57**, with
all 42 certificates now independently checked. Against
SR43 it gains nine rows and loses 47/60/91. SR49's typed pointwise root
stabilizer is wired through C/C++, `sr14` replay and seeded random generation;
its 22 solves independently check. Release and ASan/UBSan online/API/random
micro suites pass, including explicit maps omitted by restricted discovery.
SR51 reaches 24/57, all checked, and rank77 reaches eight actual row units after
the matching 27-level generic chain. It then repeats Rows without compaction.
SR52 selects one pass per structured invocation and leaves closure to the outer
sequence; job `1691908` reaches 24/57, all checked, and rank77 times out after Rows.
This negative result is retained; production defaults have not been promoted.
SR53 retains all 24 SR52 solves and complete proof hashes, all checked. SR54
inserts compact Reduce after Rows: still 24 solves, but rank77 now finishes a
second generic chain before the following Rows call stalls. Trace `1692032`
identified the second-chain divergence subsequently narrowed by SR55–SR58.
The [root stabilizer/SR repair report](benchmarks/reports/2026-09-15-root-stabilizer-and-sr-candidate-repair.md)
owns the detailed evidence and limits.
Evidence owners are `build/sr48-prefix-admission-review/` through the named
SR49, SR50, SR51 and SR52 review directories. Previous frozen
[Reduce/support report](benchmarks/reports/2026-09-15-reduce-progress-and-generator-support.md)
retains the independent Reduce repair and SR45 certificate identity evidence.

| Increment | Immutable authority | Outcome |
|---|---|---|
| SR10 startup program | `build/sr10-wv57-20260914-2045-authority`, binary `6144c5ef…` | Job `1690013`: 37/57 provisional UNSAT, 20 timeouts; proof checks continue independently |
| SR11 root-closure proof repair | `build/sr11-wv57-proofroot-20260914-2210-authority`, binary `ea746d35…` | Job `1690082`: repaired rank 5 checks in 0.316 s |
| SR13 direct product detector | `build/sr13-product-20260914-2300-authority`, binary `73fd5808…` | Job `1690160`: control checks in 0.667 s; four MVRoundRobin rows retain identical SR counts and time out |
| SR14 replayed stabilizer descent | `build/sr14-stabilizer-20260914-2335-authority`, binary `06427966…` | Job `1690169`: cancelled after 0.8--33.5 GiB growth; largest task OOM; recorded task cleanup is clean |
| SR15 bounded descent | `build/sr15-bounded-stabilizer-20260914-2355-authority`, binary `b7504404…` | Job `1690174`: four diagnostics finish in 0.217--0.868 s; each consumes six candidates/descents, finds no product shape, and stops at the exact typed limit |
| SR16--SR18 deeper product diagnostics | immutable authorities under `build/sr16-*`, `build/sr17-*`, and `build/sr18-*` | Jobs `1690184`, `1690188`, and `1690189`: cancelled after repeated 12--27 GiB growth before a useful product result; one bounded-exit leak was repaired and ASan/UBSan now pass |
| SR19--SR21 row-orbit diagnostics | authorities under `build/sr19-*`, `build/sr20-*`, and `build/sr21-*` | Jobs `1690194`, `1690195`, and `1690197`: generator-subgroup orbits, semantic-clause deduplication, and skipping startup ULC each find no rank-77 row instance; 17.3--20.5 s, 116--126 MiB, clean anonymous scratch |
| SR22 pre-refined DejaVu negative control | `build/sr22-prerefined-dejavu-20260914-2900-authority`, binary `83730c8c…` | Job `1690198`: cancelled after reaching 12.4 GiB in 23 s; no named CNF, proof, model, or checker artifact; the experimental backend call was reverted |
| Pinned Satsuma symmetry trace | `build/satsuma-schreier-trace-r77-20260914-1945`, binary `64a77d73…` | Job `1690199`: rank 77 becomes contradiction in 3.270 s through five complete clausal Schreier rounds and four row rounds; 160+20 units; zero row-column structures |
| SR23 dense-binary graph | `build/sr23-dense-binary-20260914-2025-authority`, binary `acf13114…` | Rank-77 job `1690206` admits the graph at 455 MiB sampled VM and enumerates 54 graph-SR candidates, but all are semantic no-ops. Required 10-second job `1690207` is 11/57 provisional UNSAT; all 42 rows reaching graph SR have zero effective graph transitions. Fifteen rows exhaust the window earlier. |
| SR24 incremental clausal Schreier fixing | `build/sr24-incremental-sst-20260914-2049-authority`, binary `34208aa3…` | Job `1690269`: 22/57 provisional UNSAT; 13 pass the complete DSR-to-LSR-to-Trestle pipeline and nine pass `dsr-trim` before Trestle's 120-second limit. There are 9,213 effective SR applications, but rank 77 spends 1.05 billion validation ticks and applies only 260 actions. |
| SR25 group-closure shortcut | `build/sr25-certified-sst-20260914-2101-authority`, binary `3b228843…` | Job `1690385`: **rejected**. It reaches 24/57 provisional UNSAT with zero generic validation ticks, but `dsr-trim` rejects ranks 37, 50, and 53. The first failure is a later-level binary with the wrong first-literal pivot. All 57 tasks clean their anonymous artifacts. |
| SR26 pivot-correct SST certificate | `build/sr26-sst-pivot-20260915-0130-authority`, binary `bc3fa235…` | Job `1690445`: 23/57 provisional UNSAT and every certificate passes `dsr-trim`; 14 finish Trestle within 120 seconds and nine Trestle checks time out. Ranks 37, 50, and 53 solve in 8.481, 4.273, and 4.624 seconds. Across all rows it applies 9,214 SR actions with zero generic validation ticks; rank 77 applies 226 before the 10-second limit. |
| SR27 two-pass fix-and-cut | `build/sr27-fix-and-cut-20260915-0305-authority`, binary `2da28d36…` | Job `1690512`: the accidental one-round diagnostic gains ranks 4, 5, 6, 8, 14 and 21 but reaches only 14/57. All 14 pass forward `dsr-trim`; 10 finish Trestle within 120 seconds and four reach its limit. |
| SR28 fix-and-cut scoped fixpoint | `build/sr28-fixpoint-20260915-0340-authority`, binary `1db95848…` | Job `1690569`: 18/57 provisional UNSAT, retaining all six new cut solves and recovering ranks 20, 28, 48 and 53. All 18 now pass the full pipeline, including four extended checks in job `1690626`. |
| SR29 clause-order control | `build/sr29-clause-order-20260915-0410-authority`, binary `638e4374…` | Job `1690630`: 17/57 provisional UNSAT. All 17 now pass the full pipeline; clause order loses rank 17 and does not recover SR26's large incremental-fixing cases. See the [scheduling report](benchmarks/reports/2026-09-15-sr-fix-and-cut-scheduling.md). |
| SR30/SR31 combined graph phases | `20260915-0430-sr30-combined-wv57-10s` / `20260915-0500-sr31-rescan-wv57-10s` | Each 24/57 UNSAT, 23 checked; rank 47 passes dsr-trim but exceeds the 1,200-second pipeline limit. Discard/rescan does not change the solve vector. |
| SR32 input-order graph composition | `20260915-0530-sr32-composed-order-wv57-10s` | 25/57 UNSAT, 24 checked; rank 47 has the same inconclusive checker timeout. |
| SR33 separate producer closures | `20260915-0600-sr33-full-root-wv57-10s`, job `1690858` | 27/57, all 27 independently checked. |
| SR34 repeated root sequence | `20260915-0645-sr34-root-rounds-wv57-10s`, job `1690918`, binary `f49bae30…` | Same 27 solves; all 27 now independently checked. Uses 3,600 seconds for the whole proof pipeline. |
| SR35 column-orientation control | `20260915-0710-sr35-column-rounds-wv57-10s`, job `1690976` | 21/57, no recovered SR34 losses; all 21 independently checked. Reverted this diagnostic change to Rows. |
| SR36 AMO-first clausal selection | `20260915-0810-sr36-amo-first-wv57-10s`, job `1691042`, binary `04ade639…` | 30/57, all 30 independently checked. Gains 4, 28, 35, 48 versus SR34; loses 31. |
| SR37 complete orbitopal sub-strategy | `20260915-0840-sr37-structured-program-wv57-10s`, job `1691107`, binary `6ea9da91…` | 26/57 provisional; 25 checked, one pending. Row gains coexist with generic losses; not promoted. |
| SR38 explicit budget continuation | `20260915-0940-sr38-budget-continuation-wv57-10s`, job `1691174`, binary `90ad8dc6…` | 34/57 provisional, 33 checked, one pending; gains 6, 14, 28, 48, 50, 53, 69, 70 versus SR37 and loses none. |

| SR39 immutable validation index | `20260915-1040-sr39-indexed-validation-wv57-10s`, job `1691239`, binary `a18a300d…` | Same 34 solves and complete proof hashes as SR38; 31 checked, three pending; shared solver time 46.042 s. |
| SR40 pointwise-fixed clause shortcut | `20260915-1120-sr40-fixed-clause-check-wv57-10s`, job `1691305`, binary `31242c40…` | Same 34 solves and complete proof hashes; 31 checked, three pending; shared solver time 45.044 s. |

| SR41 full selected-clause trace | `build/tactix-sr41-clause-trace-20260915-1210`, job `1691370` | First two full clause choices match; native next pair is (-43,-44). Three completed trace-off/on pairs retain identical certificates. |
| SR42 fix-and-cut first | `20260915-1230-sr42-cut-first-wv57-10s`, job `1691378`, binary `691d6dac…` | 35/57, 30 checked, five pending. Gains checked rank 46; no losses. Rank-77 first chain matches Satsuma's 27 representatives/orbit sizes. |
| SR43 row/fix-and-cut alternation | `20260915-1250-sr43-row-cut-wv57-10s`, job `1691443`, binary `b721b5e4…` | 36/57, 28 checked, eight pending. Gains checked rank 91; no losses versus SR42. |
| SR44 zero-victim root compaction | `20260915-1310-sr44-root-compaction-wv57-10s`, job `1691508`, binary `87a4e9a6…` | Negative control: 30/57, 18 checked, twelve pending; gains 37 but loses seven SR43 rows. Not promoted. |

| SR45 truthful Reduce progress | `20260915-1350-sr45-reduce-progress-wv57-10s`, job `1691573`, binary `deacdd8e…` | Same 30 solves and complete proof hashes as SR44; 25 checked, five pending. Effective zero-victim compaction is observed in 32 rows. |
| SR46 refinement-first control | `20260915-1410-sr46-refinement-first-wv57-10s`, job `1691630`, binary `061baceb…` | Negative at 13/57; all 13 checked. Product discovery still stops before row-orbit discovery after compaction. |
| SR47 generator-support tracing | `build/tactix-sr47-generator-support-trace-20260915-1435`, job `1691695`; completed controls `1691703` | Rank 77's next generic/orbitopal prefixes contain 128 fixed-only maps and zero open-variable maps. Completed ranks 20/50 retain identical trace-off/on proofs. |
| SR48 prefix admission | `build/tactix-sr48-prefix-admission`, job `1691707`, binary `1692514f…` | Cap128 drops 135 maps after compaction; cap512 retains all 263, including ten open-moving maps. Search completes in both. |
| SR49 root stabilizer | `20260915-1540-sr49-root-stabilizer-wv57-10s`, job `1691719`, binary `dd6ce0f1…` | 22/57, all checked; five gains versus SR43. Trace `1691776` confirms zero fixed-only maps after compaction. |
| SR50 independent Rows scope | `20260915-1610-sr50-row-first-wv57-10s`, job `1691780`, binary `8afb32a7…` | 42/57, all checked. Trace `1691837` finds AMO construction then fruitless ULC discovery on rank77. |
| SR51 generic before rows | `20260915-1640-sr51-generic-first-wv57-10s`, job `1691841`, binary `dcdb2d1a…` | 24/57, all checked. Trace1691903 reaches eight row units, then repeats on an unflushed formula. |
| SR52 one structured pass | `20260915-1710-sr52-single-pass-wv57-10s`, job `1691908`, binary `98111919…` | 24/57, all checked; rank77 still times out after Rows. |
| SR53 candidate repair | `20260915-1810-sr53-candidate-repair-wv57-10s`, job `1691969`, binary `18e04f9b…` | Same 24 solves and full certificate hashes; all checked. General SR replay reproducer also checks. |
| SR54 compact after Rows | `20260915-1830-sr54-post-row-compaction-wv57-10s`, job `1692031`, binary `690d25e5…` | Same 24 solves, all checked. Trace `1692032` reaches the second generic chain, then stalls in Rows. |


SR13--SR18 establish that direct and progressively deeper product
reconstruction does not produce the MVRoundRobin transition and can consume
most of the 32-GiB envelope. The bounded-exit leak found by ASan is fixed; all
completed and cancelled tasks used anonymous scratch and left no named formula
or proof artifact.

The [pinned Satsuma trace](benchmarks/reports/2026-09-14-satsuma-r77-row-trace.md)
changes the implementation target. Rank 77 starts with general clausal
Schreier fixing, then alternates complete generic and row rounds. The generic
rounds add 46, 39, 32, 25, and 18 units. Each begins with one AMO fixing and
finishes with binary-clause representative fixings through a single random
Schreier chain. The row rounds accept 9x754, 7x746, 5x738 and 3x730 matrices and
add 8+6+4+2 units. All 42 row-column candidates fail exact row-action
validation. Satsuma uses no hyperedge compression, but it does replace every
binary-clause vertex with a direct literal edge and uses a colored negation
gadget per variable. That yields 22,990 vertices and 482,600 edges; Tactix's
clause-vertex graph crosses its one-million-edge admission limit before generic
detection. The missing path is therefore the dense-binary graph, efficient
clausal fixing, preserved structured-refinement state, and alternation. Product
detection stays available for genuine product actions, but deeper product
tuning is paused for this family.
