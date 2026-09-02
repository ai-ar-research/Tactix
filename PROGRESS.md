# Progress

`DEVELOPMENT_PLAN.md` owns scope and evaluation policy; `docs/calculus.md`
owns proof semantics; `docs/implementation-notes.md` owns engineering
invariants; and `docs/scheduling-parity-ledger.md` owns N4. Frozen campaign
manifests and dated reports own experiment detail. This file is the compact
resume surface.

## Current resume anchor

- RESUME HERE (2026-09-02, second wrap). Accepted pin: `origin/main` =
  `bdb0d44` (Release SHA `1aaad2bb...`): front-end unit-cost increments 1+2,
  gated 40/40 with every counter and certificate identical to `4701`
  (campaign `4750`, 9,511.6 s vs 9,597.0 s) and B28+C5 66/66 at 0.547
  (`4751`); fuzz quick/random legs clean (jobs `1659954`/`1659955`).
  Kissat `1930` is 7,675.2 s; T4-K needs <= 8,442.7 s. Evidence:
  `benchmarks/reports/2026-09-02-frontend-unit-cost.md`.
  NEXT (deterministic, identity-verifiable unit cost at matching work):
  (1) Sweep's `sweep_reschedule_representatives` does a linear find, erase,
  and insert on a schedule vector of up to 3.9M entries per rescheduled
  representative (Rank 22: 121k reschedules, ~100 s inclusive, 70 s self,
  plus 40 s memmove in profile `4752`; Kissat's `schedule_inner` is an O(1)
  queue move) -- replace with an order-identical O(1) front stack plus
  tombstones; verify by identity on Rank 22 and small ranks. (2) Factor
  3.0x, BVE 1.9x, Congruence 2.1x at identical work on Ranks 31/33/37
  (7,953 vs 7,874 factorings; 364,546 vs 368,873 eliminations; 67,292 vs
  67,115 merges); profile `4752` on Rank 33 charges much of it to shared
  kernel rebuilds (`kernel_collect_garbage` 18 s, `kernel_rebuild_large_
  watches` 10 s, dense occurrence rebuilds, logical-position sorts) -- root
  the next profile at those helpers per calling rule. Per-rule excess over
  all 40 ranks: BVE +223 s, Factor +186 s, Vivify +135 s (work differs),
  Substitute +104 s, Congruence +64 s.
- SWEEP RESCHEDULE O(1) (local, identity-verified): `sweep_reschedule_
  representatives` did a linear find, erase, and insert on the schedule
  vector per rescheduled representative (Rank 22: 121k reschedules on a
  3.9M-entry schedule). The schedule is now a LIFO reschedule front consumed
  before the tail with slot-addressed tombstones, reproducing the former
  consumption order exactly (`sweep_next_scheduled` is the test-visible
  peek). Identity probe `4753` (job `1660064`, Ranks 22/21/39/24/9/4 vs the
  `bdb0d44` pin): byte-identical proofs, models, and counters; Rank 22
  493.6 -> 392.2 s, Rank 21 315.5 -> 311.8 s, others within noise. Gates
  and fuzz on the pinned commit are next.
- FRONT-END UNIT COST, increment 2 (local): `ingest_clause` dedups large
  clauses in place with a parse-scoped literal mark array (no per-clause
  vectors or sort; first-occurrence order kept) and activates from the
  normalized clause; `kernel_record_import` returns early for an
  already-ranked variable. Identity probe `4749` (job `1659944`, Ranks
  1/4/5/10/12 vs the `4701` pin, cumulative with increment 1): byte-identical
  proofs, models, and counters; parse 36.6 -> 24.7 s on Rank 5 and 29.9 ->
  20.3 s on Rank 10 (Kissat: 20.4 / 17.2 s). Next: fuzz legs + Rank 1--40 +
  B28+C5 on the pinned commit; identity oracle = `4701` certificate SHAs.
- GATE OUTCOME for the no-glue + watch-match binary `1a705dbf`: Rank 1--40
  `4744` is 40/40 (Rank 19 SAT in 1,010.4 s) but totals 10,054.8 s versus
  the accepted 9,597.0 s (Rank 35 +306 s, Rank 19 +759 s, Rank 17 +145 s;
  Rank 34 -495 s, Rank 37 -162 s); B28+C5 `4745` is 66/66 expected verdicts
  at 0.668 paired PAR-2 (checkers nonblocking). Rank 27's conflict count is
  identical to `f3ac562` (7,441,126): the watch-match's Rank-27 gain existed
  only with glue. Not pushed; the earlier "9,413 s" baseline figure was
  wrong (`4701` aggregates to 9,596.978 s).
- ACTIVE (user directive 2026-09-03: "do the full watch-layout match; do not
  merge before it"): replaced the partial binary-first reorder at canonical
  `substitute_apply` end with a full `kernel_rebuild_large_watches(k)` call --
  the same rebuild Congruence/Vivify/Factor already use and the exact analogue
  of kissat's terminal `kissat_watch_large_clauses`, matching not just the
  binary-first property but the large-watch arena order too. This reverses the
  lazy in-place watch retention, so Rank 27 is the regression to watch.
  Release 75/76 (doc-size only); focused watch/substitute/backbone tests pass.
  Isolated validation in flight on trace pin `d242bed5`/release `45b80584`:
  Rank-27 boundary `4740` (job `1659814`, main risk), Rank-19 binscan `4741`
  (job `1659815`, expect full word-match to kissat), Rank-19 solve `4742`
  (job `1659817`, decisive -- does it get under the 1,200 s gate?). If Rank 27
  holds and Rank 19 converges, commit and run the full Rank-1--40 + B+C gates
  and fuzz, then push the stack. The parity-ledger-deviation path is dropped.
- FULL WATCH-LAYOUT MATCH RESULT: conflict-0 binscan `4741` is now a COMPLETE
  match (non-binary-first 0/0, total scanned words 494,841 = 494,841 exactly,
  no differing scan anywhere; the reorder-only 2-word residual is gone). Rank
  27 `4740` does NOT regress -- it IMPROVES to 5,533,826 conflicts (versus
  reorder-only 5,793,285 and glue-only 6,804,239), so the lazy in-place watch
  order was itself costing Rank 27. The full match supersedes the partial
  `reorder_binaries_first` (method and test removed as dead code). BUT Rank 19
  `4742` STILL fails: the Tactix arm ran past 1,848 s (over the 1,200 s gate)
  and did not converge to kissat's 303 s despite the byte-identical conflict-0
  watch layout. This DEFINITIVELY rules out the watch layout as Rank 19's
  owner: the residual is the glue repair's downstream Reduce-retention effect
  on a chaotic SAT instance (confirmed by 4738/4739's stable-mode heap-pick
  divergence). The full watch match is a strict improvement worth keeping; it
  is necessary but not sufficient for Rank 19.
- PIVOTAL EXPERIMENT running: attach + full-watch-match WITHOUT the glue
  repair (worktree at f3ac562 + kernel_rebuild_large_watches, build 1a705dbf),
  Rank-19 solve `4743` (job `1659819`). The glue repair (14f9417) is faithful
  but is the confirmed sole Rank-19 owner (watch layout now ruled out). Since
  it is a NEW repair this session and not previously accepted, declining it to
  hold 40/40 is legitimate. If 4743 solves under 1,200 s (near the ~227 s
  pre-glue baseline), the clean path is: keep attach (pushed) + the faithful
  watch-match (improves Rank 27), drop glue, and validate a 40/40 + B+C
  gate-passing stack -- genuine further gap closure. Then measure Rank-27 and
  run the full gates on the no-glue+watch-match stack.
- `4742` result (attach+glue+watch-match, full match): Rank 19 TIMED OUT at
  2,700 s -- WORSE than reorder-only's 1,974 s, even though the full match
  improves Rank 27. On this chaotic SAT instance a more-faithful change does
  not monotonically help; it just reshuffles the trajectory onto a different
  (here longer) path. This is decisive evidence the Rank-19 cost is chaos, not
  a targetable mechanism. Whether dropping glue recovers it is `4743`.


- IN PROGRESS (staged fix for the Rank-19 block): added
  `WatchLists::reorder_binaries_first` and call it for every list at
  canonical `substitute_apply` completion, restoring the binary-first watch
  invariant kissat gets from its terminal `kissat_watch_large_clauses`
  without a full arena rebuild (preserving the lazy in-place positions). It is
  a no-op on already-ordered lists. Release is 75/76 (doc-size only) and all
  watch-adjacent focused tests pass. Isolated validation running: binscan
  `4733` (job `1659697`, must show ZERO non-binary-first Tactix lists at
  Rank-19 conflict 0) and Rank-27 regression boundary `4734` (job `1659699`,
  must stay converged). Promote to the gates only if both are clean;
  otherwise revert.
- `4733` is a strong PASS: Tactix non-binary-first scans dropped from 10 to
  ZERO (matching kissat), the conflict-0 binary-scan row counts now match
  exactly (165,908 each, was 165,327 vs 165,908), and total scanned words
  agree to within 2 of 494,843 (a single 1-word clause-membership difference
  on literal 96125, unrelated to ordering). The backbone tick model is
  converged. Awaiting the Rank-27 regression boundary `4734`.
- `4734` PASSES: Rank 27 with the reorder returns expected UNSAT at exactly
  5,793,285 conflicts, byte-identical to the pre-reorder glue pin `4712`, so
  the reorder does NOT regress the validated Rank-27 trajectory. Both
  isolated validations clean, so the fix is committed as `6f5d329` and
  promoted to the full gates. In flight on pin `6f5d329`: the decisive
  Rank-19 solve `4735` (job `1659700`; pinned Kissat solves it SAT in
  302.673 s / 701,597 conflicts, the pre-reorder pin timed out at 2,700 s),
  homogeneous Rank-1--40 gate `4736` (job `1659702`), frozen B28+C5 gate
  `4737` (job `1659735`), and bounded fuzz legs (jobs `1659736`/`1659737`).
  If Rank 19 solves and the gates stay green, push the full stack
  (`f3ac562`..`6f5d329`).
- PARTIAL: the reorder UNBLOCKS the Rank-19 hang but does not close the gate.
  `4735` returns expected SAT in 1,974.645 seconds (was a >2,700-second
  timeout on the glue-only pin), so Rank 19 now terminates and both fuzz legs
  (jobs `1659736`/`1659737`) are 10,000-case zero-failure PASSES -- the
  reorder is sound. But the homogeneous gate's 1,200-second solver timeout
  still trips it (`4736` task 18 is UNKNOWN at 1,200.005 seconds), and
  1,974 seconds is far above the ~227-second pre-glue baseline and Kissat's
  303 seconds. A large residual search-trajectory divergence remains on
  Rank 19 beyond the backbone tick model. The reorder is a correct, sound,
  Kissat-faithful improvement that fixes a real watch-order defect with no
  Rank-27 regression, so RETAIN it; but the glue+reorder stack still fails
  the homogeneous gate and stays local. NEXT: localize the residual Rank-19
  1,974s-versus-303s divergence (a fresh trajectory investigation), or accept
  the glue repair's Rank-19 cost as a documented parity-ledger deviation.
- Gate outcome for the reorder pin `6f5d329` (all provisional records in;
  checkers nonblocking): homogeneous Rank-1--40 `4736` is 39/40 with Rank 19
  the SOLE failure (no new regressions from the reorder; total 10,267.7 s),
  and frozen B28+C5 `4737` is 66/66 valid. Origin/main stays at the
  gate-passing `f3ac562`; the residual Rank-19 divergence is the next
  missing-coupled-mechanism to localize before the glue+reorder stack can
  pass the homogeneous gate.
- `4737` is now FINAL: the B28+C5 regression gate (the task's designated
  regression test) is 66/66 valid at candidate/shipped ratio 0.575068 for the
  full attach+glue+reorder stack -- comfortably under the frozen 1.10 limit
  and better than the glue-only 0.622. The stack therefore PASSES the B+C
  regression gate outright; only the homogeneous Rank-1--40 coverage check
  fails, solely on Rank 19's 1,974-second runtime exceeding the 1,200-second
  timeout.
- Origin/main is `f3ac562` (pushed, fully validated): the attach-time
  inactive-variable deactivation repair. Pinned Kissat is
  `8af8e56f174b778aef3aa45af9f739b2a5f492c2`. Rank 27 improved 483.655 to
  468.777 seconds; homogeneous Rank-1--40 `4701` (40/40) and B28+C5 `4702`
  (66/66) are both FINAL valid; the thorough three-leg fuzz (200k+20k+10k) is
  a terminal PASS.
- `14f9417` (glue preservation on forward/root-flush shrinks) and the two
  diagnostic-observer commits `ee54ac3`/`b70880e`/`00541e7` are LOCAL and
  unpushed. `14f9417` is reference-verified correct (zero metadata deviation
  at the 110,362 and 3,415 boundaries) and improves Rank 27 to a provisional
  428.017 seconds, but its homogeneous gate `4713` is 39/40: Rank 19 (SAT)
  times out because the repair unmasks a latent binary-propagator
  tick-accounting divergence. B28+C5 `4714` is 66/66 valid. Do NOT revert the
  glue repair (faithful-port rule); hold it local until the block is fixed.
- EXACT NEXT ACTION (Rank-19 sole owner, data-level root cause now fixed by
  campaign `4732`): enforce Tactix's binary-first watch-list invariant to
  match kissat. Per-literal binary-scan journals at conflict 0 show kissat
  has ZERO non-binary-first watch lists (its lists keep all binaries before
  any large watch, as `kissat_watch_large_clauses` guarantees), while Tactix
  has exactly 10 lists where a binary follows a large watch. On those 10
  lists Tactix's `propagate_literal_binary` cannot stop early and scans the
  full list, charging extra backbone ticks locally (example: literal 16229,
  Tactix scans 621 words / stop_early=0 versus kissat 6 words / stop_early=1);
  this perturbs where the 200,000-tick effort limit truncates the first
  Backbone computation and cascades. It is a watch-ORDERING invariant, not a
  flag flip: making Tactix trust a global stop-early flag would skip the real
  binary that sits after the large watch and be UNSOUND. It is pre-existing
  and latent (conflict 0 is preprocessing, identical on both the pre-glue and
  glue binaries; `f3ac562` solves Rank 19 only because its trajectory never
  reaches the sensitive state). Find where Tactix appends a binary after a
  large watch (or where a rebuild loses binary-first order) and enforce the
  invariant matching `kissat_watch_large_clauses`; stage it under isolated
  measurement, then re-run the Rank-1--40 and B+C gates on `14f9417`. This is
  the staged arena/watch representation item (task priority 3): do NOT rush a
  hot-path watch-layer change without full 40-rank + B+C revalidation.
  SOURCE-LEVEL ROOT CAUSE (fully traced): kissat's canonical Substitute ends
  with `kissat_watch_large_clauses` (watch.c), a full rebuild that appends
  large watches after all binaries, so every list is binary-first as a side
  effect. Tactix's canonical Substitute is deliberately in-place/lazy
  (`substitute.cpp:1626`: "Keep them in place instead of ... rebuilding every
  watch list ... loses the in-place and delayed positions this session exists
  to retain") -- an ACCEPTED decision from the Rank-27 lifecycle work -- so an
  in-place rewrite can leave a binary after a large watch on some lists (10 at
  Rank-19 conflict 0). Resolving this is a PLAN-LEVEL tradeoff (reintroduce
  kissat's terminal binary-first rebuild versus keep lazy Substitute), handled
  through `revise-plan`/`calculus` with full revalidation that it does not
  regress the Rank-27 lazy-Substitute convergence, not a mechanical edit.
- No retained repair has a formula, rank, benchmark, identity, size, density,
  admission, effort, classification, or strategy gate. Preserve N1--N4.
  Production uses Kissat creation order unconditionally; `TACTIX_ARENA_ORDER=1`
  is the only CaDiCaL diagnostic arm. BVE/Factor/forward-subsume/Vivify stay
  uniformly enabled at 100/50/100/100 permille.
- Known debt: `PROGRESS.md` and `docs/implementation-notes.md` exceed the
  doc-size gate (the sole failing Release test, 75/76). This session appended
  a long localization narrative; compact via the explicit `compact-docs`
  archive process when requested, not automatically.

## Latest candidate performance

- Rank-27 campaign `4630` materially merged the search-trajectory gap:
  expected UNSAT in 363.325 s / 4,215,088 conflicts / 163,165,809 decisions,
  only 2.29% more conflicts than pinned Kissat but 1.3973x its time. The
  independent checker later VERIFIED its 1,399,546,677-byte DRAT in 2979.451 s
  and deleted all derived artifacts; no work waited on it. Paired `perf`
  campaign `4632` then showed that Tactix does less search work (2.307B versus
  2.666B propagations) while using 2.45x RSS. Its largest implementation-only
  owner is `kernel_collect_garbage` (7.73% samples), with dense-occurrence and
  large-watch rebuilds another 3.68%; the sparse collector's per-live-clause
  hash dominates that owner. The general repair replaces the hash with a
  temporary sentinel in the already-garbage physical source record; it changes
  neither immutable proof IDs nor surviving order/cursors. Release is 75/76
  (all solver/correctness tests pass; only known doc-size debt fails). Uniform
  checked Rank-27 campaign `4633` first failed before staging because three
  empty harness-owned directories were absent; retry the unchanged pin and
  consume its provisional timing immediately without waiting for its checker.
  The unchanged retry is Slurm job `1659099`.
- The next isolated representation stage removes Vivify's obsolete cross-band
  reclamation transaction. Pinned Kissat marks unit/binary/subsumed Vivify
  sources garbage in place and does not collect at Vivify exit; Tactix's large
  Vivify rewrites are already in-place and its terminals have the same lazy
  garbage/watch lifecycle. The rule now leaves those records for the next real
  reference collection, and the policy-only alternate registry dispatch was
  removed with it. A regression requires unchanged arena size/generation and
  the original source Refs to remain garbage after a mixed unit/satisfied
  schedule. Release remains 75/76 with every solver/correctness test passing.
  Do not time this stage until `4633` publishes, so its effect stays isolated.
- `4633` now provisionally validates expected UNSAT in 339.581 s and 709,948
  KiB with all 4,215,088 conflicts, 163,165,809 decisions, 2,307,113,614
  propagations, and every rule/effort counter exact to `4630`. The sparse
  sentinel alone saves 23.744 s (6.53%) and 23,684 KiB; the remaining time gap
  to pinned Kissat is 1.3060x. Its checker is nonblocking. Pin and run the
  separately tested no-Vivify-collection stage as campaign `4634`, Slurm job
  `1659100`.
- Prepared but did not yet pin the next isolated stage: canonical Substitute
  already rewrites every surviving large image in its source allocation, and
  Kissat leaves unit/binary/deleted sources as lazy garbage, so Tactix's old
  post-Substitute collection and its append-replacement order/forwarding
  scaffolding were obsolete. They are removed; a regression requires a
  collapsed large-to-binary source to remain garbage at the same Ref with
  unchanged arena size/generation. All solver/correctness tests pass. Time it
  only after `4634` publishes, as separate campaign `4635`.
- `4634` provisionally returns exact-trajectory UNSAT in 335.571 s and 690,480
  KiB. Conflicts, decisions, propagations, and every rule/effort counter match
  `4630`/`4633`. Matching Kissat's lazy Vivify lifecycle saves another 4.010 s
  (1.18%) and 19,468 KiB; Rank-27 time is now 1.2906x pinned Kissat with
  1.0229x conflicts. Its checker remains nonblocking. Submitted the validated
  no-Substitute-collection pin as uniform full-width Slurm job `1659101`
  (`4635`), without a throttle or node constraint; use provisional timing
  immediately and never wait for its proof checker.
- Prepared the next isolated lifecycle stage after pinning `4635`: Kissat's
  fused forward pass marks subsumed/collapsed records garbage but does not
  collect between forward simplification and BVE. Tactix's split
  SubsumeForward and Strengthen rules each owned an implementation-only
  collector. Removed both round/instance collectors; Eliminate's existing
  post-BVE dense collector remains the physical owner. Regressions require
  unchanged arena size/generation and readable garbage source Refs. Full
  Release validation is 75/76 with every correctness/proof/model/N1--N4 test
  green and only known doc-size debt failing; focused ASan/UBSan passes with
  leak detection disabled because ptrace makes LSan unavailable here. Do not
  pin this stage until `4635` publishes.
- `4635` is a valid negative result: UNSAT in 349.306 s / 658,892 KiB with
  4,205,423 conflicts, 172,418,161 decisions, and 2,523,114,209 propagations.
  Removing canonical Substitute collection saves 5.537 s inside Substitute
  and 31,588 KiB, but adds 9.252M decisions and 216.001M propagations, making
  the solve 13.735 s (4.09%) slower than exact-trajectory `4634`. This proves
  the collection still compensates for a remaining append-only physical-order
  discrepancy. Reject the no-collection mechanism and restore the collector;
  retain only the removal of unreachable order/forwarding scaffolding. Its
  proof checker remains nonblocking. Rebuild and test the already-prepared
  forward-lifecycle stage on restored `4634` semantics before pinning `4636`.
- Restored the canonical Substitute collector and revalidated the isolated
  forward-lifecycle binary: Release 75/76 (only doc-size debt) and focused
  ASan/UBSan all pass. Submitted it as uniform full-width campaign `4636`,
  Slurm job `1659102`, without throttle or node constraint. Publish solver
  timing immediately and leave its proof checker nonblocking.
- Prepared the next isolated stage after pinning `4636`. Accepted Kissat has
  arena collections only in Reduce and Eliminate; TransitiveReduce deletes
  binary watch entries and Probe has no generic collection. Removed Tactix's
  `phase_dosed` bookkeeping and Probe-exit collection while retaining the real
  post-BVE Eliminate collector. A scheduling regression makes TransitiveReduce
  productive beside unrelated lazy large garbage and requires unchanged arena
  size/generation. Full Release remains 75/76 (only doc-size debt) and focused
  ASan/UBSan scheduling passes. Do not pin until `4636` publishes.
- `4636` is a decisive negative intermediate: valid UNSAT in 453.462 s with
  6,154,501 conflicts, 205,669,331 decisions, and 3,396,816,941 propagations.
  Removing the Strengthen collection without its adjacent reference reconnect
  adds 117.891 s and 1.939M conflicts. Source audit explains the failure:
  Kissat explicitly flushes/reconnects the complete irredundant dense
  population after fused forward and before scheduling BVE; Tactix's old arena
  collection accidentally supplied that rebuild. Added a non-relocating
  `kernel_rebuild_dense_occurrences` at BVE entry while retaining lazy forward
  garbage. Release is 75/76 and focused ASan/UBSan passes. Restore the generic
  Probe collection temporarily so this repaired forward lifecycle is isolated
  in `4637`; its proof remains nonblocking.
- Submitted the repaired forward-to-BVE lifecycle as uniform full-width
  campaign `4637`, Slurm job `1659103`, without throttle or node constraint.
  Consume provisional solver counters immediately and never wait for proof.
- Submitted coarse one-row second-BVE schedule fingerprint `4638` as Slurm job
  `1659104` alongside `4637`. It emits no per-conflict trace or proof and will
  distinguish exact reconnect state from a later divergence in about one
  minute, without extending the full solve's critical path.
- `4637` is valid but still negative: expected UNSAT in 446.154 seconds,
  6,028,067 conflicts, 191,403,629 decisions, and 3,514,186,809
  propagations. The explicit dense reconnect recovers only 7.308 seconds and
  126,434 conflicts from incomplete `4636`; it remains 110.583 seconds and
  1.813M conflicts behind accepted `4634`. Its checker is running and is not
  on the critical path. Coarse `4638` nevertheless proves that the complete
  second-BVE initial schedule and formula fingerprint at conflict 60,618 are
  exact: all eight set/order/topology/score digests, 28,585 scheduled
  candidates, and active/irred/red/binary scalars match their authority.
  Submitted `4639` (job `1659105`) to capture only the eight Strengthen/BVE
  begin/end scalar rows and stop at Eliminate end, distinguishing a within-BVE
  mutation difference from a later sparse-resume or physical-arena effect.
- `4639` completes valid/clean: both Strengthen/BVE rounds at conflict 60,618
  are exact through every formula scalar and propagation increment. `4640`
  first differs only at the next Eliminate entry: current Tactix reaches
  conflict 110,362 / decision 34,088,059, while the older accepted trace is at
  110,360 / 43,723,723. The pinned Kissat reference itself reaches 110,362 /
  35,395,104, so the lazy path is substantially closer to Kissat than the old
  compensating trajectory. Its macro conflict/decision stream is exact through
  restart end at conflict 107,953 and first differs at the following restart.
- The forward attribution was wrong. Paired `4641` and valid control `4643`
  prove that, in the current tree, Strengthen collection/reconnect,
  lazy/reconnect, and collection/no-reconnect all produce the same 3,109-row
  trace through second Eliminate entry. The coincident change behind
  `4636`/`4637` is canonical Substitute after its surviving large rewrites
  became in-place: restoring its implementation-only collector composed the
  new physical representation with an obsolete relocation boundary. Pinned
  Kissat does not collect there. Removed that collector again, restored the
  stable-arena lazy-source regression, and passed 72/73 Release tests (only
  known doc-size debt) plus focused ASan/UBSan Substitute/Strengthen/scheduling.
  Initial `4645` job `1659113` failed its copied wrapper's stale predecessor
  SHA assertion before any solver/checker ran. Corrected only that assertion
  and resubmitted the unchanged combined lazy Substitute + lazy forward pin as
  full-width job `1659115`; publish solver timing immediately and never wait
  for its proof checker.
- Bounded retry `4646` cleanly supplies the pinned-Kissat per-conflict scalar
  authority missing from `4644`. The pre-candidate Tactix and Kissat streams
  are exact through conflict 107,953; their first checkpoint split is within
  conflict 107,954 at 30,977,043 versus 30,986,296 decisions. This is not yet
  the promoted candidate's owner. Submitted Tactix-only `4647`, job `1659116`,
  to capture the same O(1) checkpoints for the lazy-Substitute candidate while
  `4645` solves. It opens no decision/watch/clause trace or proof artifact.
- `4647` completes valid/clean in 93.775 seconds. The promoted candidate and
  Kissat remain exact in conflict/decision checkpoints through conflict
  107,953 (30,970,286 decisions) and first split at conflict 107,954. Tactix
  uses 6,757 decisions in that interval versus Kissat's 16,010, so this local
  CDCL split is favorable, not an inferior mechanism. Do not zoom into it;
  use later coarse inprocessing/search boundaries to find where any inferior
  work first appears.
- Submitted current-candidate full sparse-boundary campaign `4648` as
  full-width one-task job `1659117`. It reuses the already-valid pinned-Kissat
  journal from `4495` and records only Tactix rule/macro begin/end scalars from
  conflict 107,948 through ordinary UNSAT. It emits no per-conflict,
  per-decision, candidate, watch, clause, heap, proof, or model trace.
- `4645` is decisively negative despite its favorable early boundary: valid
  expected UNSAT in 568.929 seconds / 8,981,020 conflicts / 190,116,593
  decisions / 3,390,239,227 propagations and 689,120 KiB. Relative to
  lazy-Substitute-only `4635`, it adds 219.623 seconds and 4.776M conflicts.
  Reject the lazy-forward stage and its compensating pre-BVE reconnect;
  restore both forward collectors while retaining lazy canonical Substitute.
  Focused Release kernel/scheduling/Subsume/Strengthen/Substitute tests pass.
  `4648` remains useful: it will locate the first late inferior boundary on
  the rejected pin without waiting for `4645`'s proof checker.
- Banked the restored production state as Release campaign `4649`, job
  `1659118`: canonical Substitute stays lazy/in-place; Strengthen and the
  diagnostic SubsumeForward regain their packed-arena collectors; the
  compensating pre-BVE reconnect is gone. The complete Release suite is 72/73
  with only known doc-size debt. Consume provisional Rank-27 timing
  immediately and never wait for its proof checker.
- Submitted matching high-level boundary campaign `4650` as job `1659119`.
  It records only rule/macro begin/end scalars for the restored production
  state and reuses `4495`'s valid Kissat authority. This distinguishes which
  inferior boundary survives restoration before any detailed logging.
- Live `4648` coarse rows already explain how the rejected lazy-forward pin
  becomes inferior. It is 1.307M decisions ahead at second Eliminate entry and
  still 334k ahead at Probe 120,310 entry, but that Probe exits with 205 more
  active variables, 423 more irredundant clauses, 836 more redundant clauses,
  and 694 fewer binaries than Kissat. By the next Probe it is 827k decisions
  behind. Thus the first demonstrated late inferior transition is Probe
  120,310 output, followed by plain CDCL amplification; wait only for `4650`'s
  corresponding high-level rows, not for any proof, before choosing a narrower
  inprocessing owner.
- `4648` is terminal valid/clean: expected UNSAT, 2,164 structurally valid
  rows from conflict 110,362 through 8,699,969, zero stderr/residue, and no
  proof artifact. Its terminal status does not change the live finding above;
  the rejected pin's first demonstrated inferior transition remains Probe
  120,310 output.
- `4649` cleanly reproduces lazy-Substitute-only `4635`: expected UNSAT in
  346.598 seconds, 4,205,423 conflicts, 172,418,161 decisions,
  2,523,114,209 propagations, and 658,892 KiB. Every solver/rule/effort counter
  and RSS is exact to `4635`; the 2.708-second wall difference is noise. The
  forward rollback is therefore banked, but this state remains 1.3330x pinned
  Kissat time. Proof checking remains asynchronous.
- Source audit found the next general physical discrepancy: Kissat's dense BVE
  collector copies live clause records while preserving literal order and the
  `searched` cursor, whereas Tactix's shared collector installed sparse
  two-watches, sorted literals, and reset the cursor even with `k.dense` open.
  Split the dense path so it preserves clause pose and only rebuilds the full
  occurrence image after Ref relocation; sparse resume still owns sorting,
  rewatching, and cursor reset. Full Release is 72/73 (doc-size debt only), and
  focused ASan/UBSan kernel/BVE/forward/Substitute/scheduling tests pass.
- Submitted the isolated dense-collector-pose repair as uniform full-width
  Rank-27 Release campaign `4651`, job `1659120`. Consume solver timing and
  trajectory immediately; its proof checker is never a prerequisite.
- Submitted bounded checkpoint companion `4652`, job `1659121`, to compare
  only O(1) per-conflict scalars from 107,948 through the next Eliminate. This
  determines whether the dense-copy repair changes the post-60,618 trajectory
  before `4651` finishes, without detailed logging or proof work.
- `4650` is terminal valid/clean and confirms the inferior Probe-120,310
  output survives forward restoration. `4651` publishes expected UNSAT in
  340.541 seconds with counters exactly matching `4649`; its proof checker
  remains nonblocking. `4652` validly reproduces the same trajectory through
  the next Eliminate. Both are intermediate because they predate a required
  dense-entry transition discovered by the focused sanitizer gate.
- Completed the general dense lifecycle repair: every dense entry now owns
  Kissat's sparse-large-watch flush; dense collection preserves literal pose
  and `searched`; Congruence explicitly republishes its complete sparse
  resume; Factor and Vivify tolerate an already-disconnected entry. Focused
  ASan/UBSan passes and Release is 75/76, with only known doc-size debt.
  Submitted fresh uniform Rank-27 Release `4653` as job `1659122` and O(1)
  checkpoint companion `4654` as job `1659123`; there is no new scheduling or
  input-dependent gate.
- Pre-promotion ownership audit invalidated `4653`/`4654` before either
  produced a result; both jobs were cancelled after 45 seconds. A retained
  full-occurrence index can coexist with rule-resumed sparse watches, so
  collection mode must be keyed by `large_watches_connected`, not merely
  `k.dense`. Repaired that general state distinction and added a regression
  that relocates a clause after an in-phase sparse resume. Focused sanitizer
  checks and the complete Release suite again pass except known doc-size debt.
  Submitted fresh corrected `4655` Release as job `1659124` and `4656` O(1)
  checkpoints as job `1659125`, both full-width and unthrottled.
- `4656` completes valid/clean in 96.946 seconds with 2,091 checkpoints. The
  complete lifecycle repair changes search before conflict 107,948: decision
  count there is 33,668,837 versus predecessor `4652`'s 30,930,984. It reaches
  the next Eliminate 159 conflicts earlier at decision 34,079,172, but has
  spent 13.679M more propagations by that boundary. This is a coarse signal,
  not yet a promotion/rejection verdict; consume `4655` Release next.
- Pinned Kissat source narrows physical ownership: Eliminate enters dense mode
  once around its whole pipeline, but Probe does not; Congruence, Vivify,
  Sweep, and Factor each own separate dense/resume pairs. Tactix's phase-long
  Probe `OccIndex` is an implementation cache, not a physical dense lifetime.
  Scoped automatic entry flushing to Eliminate while retaining the existing
  rule-owned Probe transitions. Added a regression that opening the Probe
  cache preserves connected sparse watches. ASan/UBSan and 75/76 Release tests
  pass (doc-size only). Submitted `4657` Release as job `1659127` and `4658`
  checkpoints as job `1659126`; `4655` remains an immutable full-phase-flush
  control and its proof is irrelevant.
- `4658` completes valid/clean in 92.577 seconds with 1,999 checkpoints and is
  exact to predecessor `4652`/`4649` at every tuple through the next Eliminate
  entry (110,362 / 34,088,059 / 293,115,233). This confirms the scoped repair
  avoids the harmful Probe perturbation; any effect begins inside that
  Eliminate or later. Consume `4657` Release before adding another observer.
- Full-Probe-flush control `4655` is decisively negative: valid expected UNSAT
  in 433.621 seconds / 6,154,025 conflicts / 197,136,856 decisions /
  3,255,981,834 propagations / 678,260 KiB. It is 25.1% slower and adds 1.949M
  conflicts versus `4649`, confirming the pinned-source ownership correction.
  Reject this broad transition; its checker remains nonblocking. `4657` is the
  only live candidate.
- Eliminate-scoped `4657` is also negative: valid expected UNSAT in 347.661
  seconds / 4,277,107 conflicts / 172,653,411 decisions / 2,566,129,286
  propagations / 664,120 KiB. Versus `4649` it adds 71,684 conflicts, 235,250
  decisions, and 43.015M propagations. Reject automatic phase-entry flushing;
  its checker remains nonblocking. The retained repair is implementation-only
  packing semantics: preserve clause pose/cursors and forward the sparse watch
  image already owned by the current architecture, without inventing a new
  physical dense transition.
- Completed the narrower packing repair with a sanitizer-covered connected-
  sparse case: live large watches are forwarded to relocated refs, garbage
  watches are dropped, and clause pose/cursors remain unchanged. Full Release
  is 75/76 with only doc-size debt. Submitted uniform `4659` Release as job
  `1659129`.
- Coarse conflict-110,362 comparison localizes the next recorded inferior
  inprocessing transition. Tactix enters Eliminate only 42 active variables
  above Kissat, but first Strengthen worsens that gap by 4 and first BVE by 58;
  round-two work recovers 10, leaving a 94-variable exit gap. First BVE also
  removes 246 fewer binaries relative to its input. Prepared symmetric one-row
  BVE schedule fingerprint `4660` as full-width job `1659128` before any
  member/candidate logging.
- `4660` completes 2/2 valid/clean in 104.185/95.353 seconds. Each arm
  schedules every active variable, but Tactix/Kissat have 132,334/132,288
  members and differing set, topology, and score fingerprints. Thus first BVE
  inherits a formula/active-identity difference rather than losing eligibility
  at scheduling. One bounded paired member dump is the next zoom; no candidate
  stream is yet justified.
- Submitted that exact first-BVE member/position/occurrence/score snapshot as
  full-width two-arm job `1659131` (`4661`). It exits before the first BVE
  candidate and produces no proof. The user's unrelated job `1657786` remains
  running independently on node010 and is untouched.
- `4661` completes 2/2 valid/clean in 104.863/96.128 seconds. The schedules
  share 132,287 variables; Tactix has 47 unique members and Kissat one. Among
  common variables, only 1,455 physical occurrence tuples differ, whereas
  38,433 scores differ and 36,980 of those score differences occur with
  identical occurrences. Another 93,852 common variables match both tuple and
  score exactly. The enormous topology difference is therefore predominantly
  inherited CDCL heuristic state, not a BVE eligibility or occurrence-score
  construction defect. Resume higher-level localization before candidate
  tracing; retain the much smaller formula-population difference as secondary.
- `4659` provisionally returns expected UNSAT but decisively rejects the
  connected-sparse forwarding hypothesis: 518.073 seconds / 7,867,007
  conflicts / 206,372,527 decisions / 3,450,323,652 propagations / 688,240
  KiB. It is 171.475 seconds and 3.662M conflicts worse than `4649`.
  Proof checking remains nonblocking. Preserve the memory-safety regression,
  but collection after an in-phase sparse resume must take the ordinary sparse
  rebuild lifecycle; only a truly disconnected dense collection may preserve
  the dense clause pose/cursor image.
- Repaired that state distinction: `dense_collection` now requires both a
  live occurrence index and physically disconnected large watches. A retained
  Probe cache beside connected sparse watches takes the ordinary rewatch path,
  resets `searched`, forwards no stale entries, and rebuilds the cache after
  relocation. The sanitizer regression now checks relocated sparse Refs.
  Focused ASan/UBSan passes and full Release is 75/76 (doc-size only).
  Submitted uniform Rank-27 Release `4662` as job `1659133`; consume its solver
  result immediately and leave proof checking nonblocking.
- Submitted bounded checkpoint companion `4663` as job `1659134`. It records
  one scalar row per conflict only from 107,948 to the next Eliminate entry and
  produces no proof, allowing early trajectory disposition while `4662` runs.
- `4663` completes valid/clean in 93.738 seconds with 1,999 checkpoints. Its
  complete trace is byte-identical to `4658`/`4652`/`4649`, ending at
  110,362 / 34,088,059 / 293,115,233. The corrected physical-state test
  restores the accepted prefix; consume `4662` for the later trajectory.
- Submitted paired compact-map campaign `4664` as full-width job `1659135`.
  It records only nonidentity representative maps at conflict 120,310 and
  exits at the first Probe Substitute end (5,000-row hard cap, no proof),
  directly testing the first 50-variable relative retirement loss without a
  broad search or candidate trace.
- `4662` provisionally validates the corrected collection mechanism: expected
  UNSAT in 344.756 seconds / 4,205,423 conflicts / 172,418,161 decisions /
  2,523,114,209 propagations / 658,880 KiB. Every trajectory counter is exact
  to accepted `4649`; the 1.842-second wall improvement is not promoted beyond
  noise. Bank the physical-state distinction and memory-safety regression.
  Its proof remains nonblocking while Rank-27 localization continues.
- `4664` is observer-invalid and was cancelled: the existing compact-Probe
  mode still retained BVE diagnostic families, so Kissat exhausted the
  5,000-row cap at conflict 15,345 before the selected conflict. No semantic
  claim. Added a symmetric trace-only Substitute filter that retains only
  `trace_begin`, nonidentity `substitute_map` rows, and the explicit terminal
  match; rebuild and retry the same bounded question.
- Trace-only filter checks pass for Tactix and the overlay, and a fresh pinned
  Kissat diagnostic build succeeds. Submitted corrected paired retry `4665`
  as full-width job `1659137`; it retains the same target and hard cap but now
  rejects every unrelated event family before emission.
- `4665` completes 2/2 valid/clean in 172.012/164.430 seconds with only 422
  total trace lines. Tactix/Kissat map 78/128 variables. They share 50 mapped
  variables and every common literal-to-representative map is exact; Tactix
  has 28 unique mapped variables and Kissat 78. Thus the net 50-variable loss
  is a different SCC domain/input graph, not common-class orientation. Inspect
  one Kissat-only class at the pre-call binary implication boundary next.
- Submitted selected pre-Substitute binary snapshot `4666` as full-width job
  `1659139` for Kissat-only class `93112 -> 92573`. It records only binaries
  touching those two variables at conflict 120,310 first-Substitute begin and
  exits immediately, testing the two required implication edges without a
  broad formula dump.
- `4666` is observer-invalid: both arms hit the 300-second process cap with
  header-only event files. The globally opened target-only trace is too costly
  on this prefix. No semantic claim. Retry the identical selected-binary
  question through the existing late-open boundary path so trace machinery is
  dormant until the first-Substitute entry.
- Submitted late-open retry `4667` as full-width job `1659141`. It leaves the
  global trace unopened through search, materializes only the two-variable
  binary snapshot at the exact boundary, and exits immediately.
- `4667` completes 2/2 valid/clean in 120.573/128.019 seconds. At first-
  Substitute entry, both solvers contain `(-92573 v 93112)`, but only Kissat
  contains `(-93112 v 92573)`. The missing implication fully explains why
  Tactix cannot form class `93112 -> 92573`; Substitute SCC traversal is not
  the owner. Bracket that edge at Probe begin and Congruence end next.
- Submitted four late-open snapshots `4668` as full-width job `1659143`:
  paired Probe begin and paired Congruence end for only the selected edge.
  This distinguishes inherited CDCL/formula state from loss in Congruence
  without tracing either rule internally.
- `4668` is partially valid but campaign-incomplete. Tactix Congruence end
  validly lacks `(-93112 v 92573)` in 119.879 seconds. Tactix late-open does
  not attach to outer `probe_begin`, and Kissat's generated Probe macro had a
  late hook only at rule begin, so the unsupported tasks were cancelled or
  timed out. No cross-solver transition claim. Added the symmetric Kissat
  rule-end late hook; retry paired Congruence begin/end.
- Static overlay validation passes and the rebuilt Kissat diagnostic is pinned
  at SHA `5ac5436741a7386a908dc7da507035bd1f58e1298c4a2659c8cffe5c554436cb`.
  Submitted corrected four-arm Congruence begin/end bracket `4669` as
  full-width Slurm array job `1659147`, with late-open selected-variable
  snapshots, no proof, no broad trace, no throttle, and no node constraint.
- `4669` completes 4/4 valid/clean in 120.357--128.047 seconds. Tactix lacks
  `(-93112 v 92573)` at both Congruence begin and end; Kissat contains it at
  both boundaries. The SCC-input divergence is therefore inherited by
  Congruence, not created or deleted there. Add generic late-open hooks to the
  existing top-level macro observer and bracket the preceding Eliminate end
  before opening a selected binary-lifecycle stream.
- The generic Kissat top-level macro observer now has symmetric dormant
  late-open begin/end hooks; its static check and fresh diagnostic build pass.
  Submitted paired selected-edge Eliminate-end snapshot `4670` as full-width
  Slurm array job `1659151`, with no proof, broad trace, throttle, or node
  constraint.
- `4670` is partial-observer-valid and causally decisive: Kissat validly lacks
  `(-93112 v 92573)` at Eliminate end in 124.332 seconds, then contains it at
  the later `4669` Congruence begin. Its creator is therefore the intervening
  CDCL interval. The original Tactix arm exposed that late-open covered rule
  slots but not top-level macros and was cancelled without a semantic claim;
  `4669` already proves Tactix still lacks the edge at the later endpoint.
  Added and unit-tested the missing generic Tactix top-level late-open hook.
  Trace only the selected binary lifecycle through that interval next.
- Submitted paired selected-edge CDCL lifecycle campaign `4671` as full-width
  Slurm array job `1659153`. It records only add/delete events for
  `(-93112 v 92573)`, remains globally dormant otherwise, and exits at
  Congruence begin with no proof, broad trace, throttle, or node constraint.
- Submitted paired one-row last-exact-boundary campaign `4672` as full-width
  Slurm array job `1659155`. It late-opens only at `restart_end` conflict
  107,953 and compares scalar state digest, mode, population, queue/heap, and
  top-32 order before the conflict-107,954 path-length split; no detailed
  decision/watch/clause trace or proof is produced.
- `4671` completes 2/2 valid/clean in 118.199/128.200 seconds. Kissat adds
  `(-93112 v 92573)` as a CDCL binary at conflict 110,565; Tactix emits no
  lifecycle event for the pair through Congruence begin. This closes the edge
  provenance chain: the later 50-variable Substitute deficit is downstream of
  a learned-clause trajectory difference, not an inprocessing deletion or SCC
  implementation defect. Use `4672` to classify the hidden state at the last
  exact conflict/decision boundary before detailed conflict logging.
- `4672` completes 2/2 valid/clean in 101.568/111.498 seconds. At restart end
  107,953 both arms are exact in conflicts, decisions, level 1, focused mode,
  active/unassigned population, heap population, formula scalars, search
  cursor, and the complete ranked top-32. The digest differs alongside the
  known physical trail population (1,865/594) and a one-entry queue-count
  convention (132,369/132,368), not a visible branch-ranking discrepancy.
  Before any decision stream, compare one rolling hash at the 6,757-decision
  common-length prefix of the next conflict interval plus each arm's final
  interval count/hash.
- Added a dormant, trace-only rolling decision-prefix summary to both
  diagnostic solvers; focused builds, static overlay checks, and the Tactix
  trace test pass. Submitted `4673` as full-width Slurm array job `1659157`.
  It hashes decisions only while the conflict counter is 107,953, records one
  common-prefix hash at 6,757 decisions and one final row, then exits; no
  individual decision rows, proof, throttle, or node constraint.
- Opportunistic nonblocking inspection finds rejected control `4659`
  complete: `drat-trim` independently verifies its 2,227,275,109-byte DRAT
  certificate in 3,121.163 seconds with exit 0, and the runner leaves zero
  proof residue. No trajectory work waited on the checker.
- Submitted paired one-row semantic formula/root fingerprint `4674` as
  full-width Slurm array job `1659159`. It performs one bounded scan only at
  restart end 107,953 and exits, distinguishing formula-content divergence
  from physical watch/propagation order without clause-row logging or proof.
- `4673` completes 2/2 valid/clean in 101.321/108.682 seconds and rejects an
  identical-decision-prefix hypothesis. The first accepted decision after the
  matched restart already differs: Tactix chooses `125243`, exactly its ranked
  search cursor/top variable, while Kissat chooses `-188274`; the 6,757-prefix
  hashes are `9b7ea2ae...` versus `4cbb66ef...`. Compare only the first few
  random-sequence state/RNG rows around the restart reuse pick next. This is a
  branch-selection divergence before propagation, not a later watch conflict.
- Added a four-row-cap standalone random-sequence state observer carrying
  remaining lifetime, deadline, sequence count, and exact RNG state; trace
  builds and tests pass. Submitted paired restart-reuse campaign `4675` as
  full-width Slurm array job `1659161`, stopping at conflict 107,954 with no
  global trace, proof, throttle, or node constraint.
- `4674` completes 2/2 valid/clean in 102.463/113.330 seconds. Irredundant and
  redundant clause counts and hashes and the binary multiset hash are all
  exact (`560165`, `20499`, `5d56e896...`, `b1e3ba8d...`,
  `d834df38...`). Only representation-local root/value accounting differs.
  Combined with `4673`'s first-decision mismatch, this rules out semantic
  formula content and leaves random/branch selection lifecycle as the direct
  owner; physical watch propagation is downstream, not the first split.
- `4675` completes 2/2 valid/clean in 103.641/112.685 seconds and identifies a
  causal general CDCL mechanism discrepancy. Entry random state is exact and
  overdue in both arms: remaining 0, deadline 107,951, sequence 17, RNG
  `2e0f36c8ff77424a`, retained level 1. Kissat's restart-reuse call starts
  sequence 18 (length 14, next deadline 108,668) and consumes its first draw;
  Tactix reports `inactive` for both the reuse pick and next ordinary pick.
  The ordinary pre-decision level-zero rule is correct for `kissat_decide`, but
  wrong when reused for restart's direct `next_decision_variable` call. Add an
  explicit restart-reuse pick context, with no formula/rank gate, and validate
  exact trajectory closure before measuring Rank 27.
- Repaired the general branching API: `pick_for_restart_reuse` preserves the
  ordinary policy default but lets `DefaultBranching` apply Kissat's direct
  level-at-most-one random-sequence admission only in restart trail reuse;
  ordinary decisions retain their pre-frame level-zero rule. Added regressions
  for ordinary retained-level-one, restart-reuse level-one, and rejected
  restart-reuse level-two contexts. Focused Release/trace strategy, restart,
  and event-trace builds/tests pass. Validate the exact conflict-107,953 random
  transition and downstream checkpoint prefix before full timing.
- Submitted two immutable repaired-source causal validations at full width,
  with no proof, broad trace, throttle, or node constraint: `4676` (Slurm
  `1659163`) checks sequence 18 and first decision `-188274` at restart reuse,
  while `4677` (Slurm `1659164`) compares the existing scalar checkpoint
  stream through the next Eliminate entry against the copied Kissat authority.
  Continue immediately from their solver results; proof checking is not part
  of either campaign.
- `4676` and `4677` complete valid/clean and causally close the restart split.
  The repaired arm starts sequence 18 with remaining 14 / deadline 108,668,
  consumes the same RNG draw, and makes Kissat's first decision `-188274`.
  It then matches all 2,171 copied-authority conflict checkpoints with zero
  decision differences through Eliminate entry at conflict 110,362; the exact
  endpoint is 35,395,104 decisions / 300,522,504 propagations. Both observers
  report zero stderr and leave zero proof residue. Verify the already-owned
  learned binary and Substitute consequence next, then time Rank 27.
- Submitted downstream consequence checks at full width: `4678` (Slurm
  `1659165`) requires the repaired arm to learn `(-93112 v 92573)` at conflict
  110,565, and `4679` (Slurm `1659166`) requires the first target Substitute
  call to recover 128 mapped variables / 256 literal maps. Both are bounded,
  observer-only, proof-free jobs with zero-residue requirements.
- `4678` and `4679` are observer-invalid/time-limited and clean: their stale
  fixed downstream endpoints are not reached within the 300-second process
  cap, so neither is a valid endpoint campaign. The selected lifecycle in
  `4678` nevertheless records `add-new (-93112 v 92573)` at conflict 110,565,
  while `4679` has header-only output and supports no Substitute claim. Do not
  retry fixed-conflict observers before the requested Release/B+C/Rank-1--40
  checkpoint; measure the changed full trajectory first.
- Current Release validation is 75/76: all executable, proof/model, N1--N4,
  overlay, and source-conformance tests pass. Only the pre-existing document
  size gate fails (`PROGRESS.md` and `docs/implementation-notes.md`); do not
  mix the separately requested code/document cleanup into this checkpoint.
- Opportunistic inspection finds `4662` terminal: `drat-trim` independently
  verifies its 1,365,581,398-byte certificate in 2,999.189 seconds with exit 0;
  proof and derived artifacts are deleted and zero residue remains. No work
  waited on the checker.
- Checkpoint commit `02ab6f1` is local and unpushed. Submitted the identical
  Release SHA `0bb0d7b7...b4887` to full-width B28+C5 campaign `4680` (Slurm
  `1659167`, 66 shipped/candidate tasks) and homogeneous Rank-1--40 campaign
  `4681` (Slurm `1659168`, 40 candidate tasks). Gather provisional solver
  records to completion and aggregate immediately; never wait for proof
  checking. Final proof/model and zero-residue results remain later checks.
- The restart-reuse-randec-on homogeneous result is complete provisionally: all 40
  statuses match, total 9,412.958 seconds, but Rank 27 regresses to 483.655
  seconds versus the proof-verified pre-restart collection result at 344.756.
  Submitted isolated restart-reuse-randec-off Rank-27 projection `4682` (Slurm
  `1659276`) from detached worktree source differing only in
  `pick_for_restart_reuse`; consume its solver timing before extending that
  diagnostic projection to B28+C5 and Rank-1--40. Proof checking is nonblocking.
- `4682` proves restart-reuse-randec-off recovers Rank 27 at 347.443 seconds. Its
  complete provisional B28+C5 projection `4683` preserves all 66 statuses at
  1,377.769/2,437.703 seconds (0.565191x candidate/shipped), but homogeneous
  projection `4684` is rejected: it times out expected-SAT Rank 19 at 1,200
  seconds and solves only 39/40 (PAR-2 11,645.972). Restart-reuse-randec-on solves all
  40 in 9,412.958 seconds. Retain the general production repair; Rank-27's
  remaining gap is a downstream interaction to localize, not grounds to revert
  it. All proof/model checks remain nonblocking and must be inspected later.
- `4682` confirms the projection: expected Rank-27 UNSAT in 347.443 seconds /
  658,668 KiB, within 0.8% of pre-restart collection `4662` and far below
  restart-reuse-randec-on 483.655 seconds. Submitted the identical detached-worktree
  restart-reuse-randec-off arm to full-width B28+C5 `4683` (Slurm `1659277`) and
  homogeneous Rank-1--40 `4684` (Slurm `1659278`). These are feature-off
  diagnostic projections only; production remains `02ab6f1`. Consume solver
  records immediately and retain later proof/model checks without waiting.
- Production checkpoint gates are now fully final: B28+C5 `4680` is 66/66 valid
  and homogeneous `4681` is 40/40 valid, with every SAT model and UNSAT proof
  independently verified and zero residue.
- Fresh-pin boundary journal `4685` (job `1659383`, commit `ffae813`) is valid/
  clean: production Rank 27 ends UNSAT at 6,804,239 conflicts versus Kissat's
  4,120,691. Both solvers hold macro schedule lock-step through conflict ~4.01M
  (52 matched Probe/Eliminate invocations within ~4,400 conflicts), then
  Kissat's final Eliminate at 4,086,997 removes ~5,914 variables and it
  finishes 34k conflicts later, while Tactix's Eliminate at 4,082,465 removes
  ~500 and search continues 2.7M more conflicts. The earliest trajectory
  divergence is the CDCL interval from the exact conflict-110,362 Eliminate to
  the next Probe: Tactix enters at 120,313/37,518,169 versus Kissat
  120,310/37,521,538. Submitted paired per-conflict checkpoint zoom `4686`
  (job `1659384`) over exactly that window; both arms exit at Probe begin and
  emit no proof.
- `4686` completes 2/2 valid/clean in 104.421/113.692 seconds and moves the
  first divergence inside the 110,362 Eliminate itself. The Kissat arm's
  round journal shows round-1 Strengthen output exact in every counter
  (132,288 active / 541,260 irred / 21,908 red / 167,774 binary), then
  round-1 BVE end with one extra Tactix binary (153,805 versus 153,804 at
  exact active 114,081 and irred 576,041). Round two amplifies to irred
  522,424/522,431, and post-Eliminate CDCL decisions first split two conflicts
  later at 110,364 (interval 2,781 versus 2,783 decisions after an exact
  5,987-decision resume interval). Submitted paired one-shot BVE begin/end
  binary multiset campaign `4687` (job `1659387`, diagnostic overlay worktree
  `worktree-bve-binary-set-20260901`) to name the differing pair.
- `4687` completes 2/2 valid/clean: round-1 BVE INPUT binary multisets are
  byte-identical, so round-1 BVE owns the first divergence outright. Its
  output differs by six Tactix-only pairs (four on literal 424046) and five
  Kissat-only pairs (two on 35319): Kissat eliminates 424046 and keeps 35319,
  Tactix the reverse. `4688` (4/4 valid/clean) then proves both arms first try
  candidate 424046 at the exact same resolution tick 24,160,715, spend 19
  ticks, and decline identically; Kissat must therefore re-arm and eliminate
  424046 later within the same round, while Tactix instead eliminates 35319
  at tick 27,832,495 (Kissat declines it at 27,833,038). The owner is the
  within-round candidate re-arm path. Submitted full try-history campaign
  `4689` (job `1659393`) for both externals across the whole Eliminate.
- `4689` completes 4/4 valid/clean. Kissat re-tries 424046 once at tick
  27,512,167 and eliminates it; Tactix re-tries it at 27,511,963 and declines
  (then again at 29,659,099 and 30,267,159, declining both). Root-unit counts
  match (1,362) at the second try, but the 204-tick entry offset proves the
  first differing action lies strictly between exact tick 24,160,734 and
  27,511,963. Submitted unfiltered candidate-stream campaign `4690` (job
  `1659397`) to name the first differing (external, tick, applied) tuple.
- `4690` completes 2/2 valid/clean with 242,511/242,474 candidate begins. The
  streams are tuple-exact through try 139,721; the first divergence is a pure
  pop-order swap at tick 25,873,783: Tactix pops externals 488086 then 485686
  where Kissat pops 485686 then 488086, all four zero-tick declines. The
  physical schedule-heap topologies therefore diverged in some earlier update
  without affecting any pop until this tie. Added a standalone deduped
  selected-subject heap journal to both diagnostic solvers (no global trace)
  and submitted campaign `4691` (job `1659399`) tracking both subjects'
  score/position/occurrence rows across the whole Eliminate.
- `4691`'s Kissat arm segfaulted because the selected-subject observer cached
  internal indices across compaction; subjects are now stored as externals and
  resolved through the import table per call. Kissat-only retry `4692` (job
  `1659401`) is valid/clean and decisive: subject 485686's BVE relevance
  differs by exactly 1.0 from schedule initialization (Tactix 3,896,995
  versus Kissat 3,896,996) with identical occurrence counts, while 488086's
  score is bit-exact. The focused-mode queue stamp of variable 485686 already
  differs at Eliminate entry; the BVE heap tie at tick 25,873,783 is only the
  first place the one-unit stamp gap decides an order. Submitted paired
  full member dump `4693` (job `1659402`) on the exact-trajectory pins to
  learn whether the stamp divergence is one variable, a set, or a shifted
  range.
- `4693` completes 2/2 valid/clean and reveals a clean counter-offset window:
  all 132,288 schedule members, occurrence counts, and heap positions are
  exact, but 36,324 variables have Tactix stamp = Kissat stamp - 1. The
  shifted set is precisely the variables whose last bump carries Kissat stamp
  below ~7,897,06x; every more recent stamp is equal (33-stamp transition
  window). Thus the two VMTF stamp counters were offset by one for a long
  period and re-synchronized about 103k bumps before Eliminate 110,362 --
  approximately the conflict-107,95x restart-reuse window. Queue ORDER never
  differed, so search stayed exact; only BVE's use of raw stamps as relevance
  exposes the offset. Added a queue-stamp column to both standalone
  checkpoint journals and submitted per-conflict campaign `4694` (job
  `1659404`) from conflict 0 to locate both offset transitions exactly.
- `4694`'s Kissat arm and Tactix retry `4695` (ordinal 3; the preprocess
  Eliminate also emits `eliminate_begin`) are valid/clean with 101,517 common
  per-conflict checkpoints. The counter columns read Tactix = Kissat + 1
  constantly from the first compaction at conflict 1,002 through 110,362,
  which cannot be reconciled with the member stamps (old members Kissat =
  Tactix + 1, recent equal) by any single-event model; one side's counter
  read is a sampling-convention artifact. Added a symmetric one-shot ordered
  queue dump (position, external, stamp) and submitted campaign `4696` (job
  `1659407`) at Eliminate begin 110,362 to resolve membership, order, and
  per-variable stamps directly.
- `4696` completes 2/2 valid/clean and closes the structure: Tactix's VMTF
  queue holds one extra member, external 488590, at position 95,964 (stamp
  102,871); Kissat's queue is exactly the 132,308 active variables. Queue
  order is otherwise identical and precisely the 36,344 members behind the
  extra entry carry stamp = Kissat + 1 (the earlier 4693 sign read was the
  heap's score negation). BVE reads stamps as relevance, so the retained
  entry is the general owner of the 110,362 pivot swap. The repair is the
  missing deactivation/dequeue of 488590; submitted Kissat-only `4697` (job
  `1659409`) with a selected-external deactivate observer to name the phase
  and class (eliminated versus fixed) of the reference dequeue.
- `4697` closes the provenance: pinned Kissat deactivates external 488590 at
  conflict 0 / decision 0 with `fixed=1`, and the input formula's ONLY unit
  clause is exactly 488590. The unit is assigned during load, before
  DefaultPolicy attaches `H`; Kissat dequeues it at fix time, while Tactix's
  attach enqueued every identity and nothing ever consumed the pre-attach
  root trail. General repair: `heuristics_deactivate_inactive_vars` at
  `ScopedHeuristics` attach dequeues/heap-removes every already root-fixed or
  retired identity after enqueueing (preserving Kissat's consumed stamp-slot
  semantics). Regression `test_attach_deactivates_prefixed_input_unit` covers
  membership, heap, and the preserved stamp slot. Full Release is 75/76 with
  only known doc-size debt. Causal campaign `4698` (job `1659410`) validates
  the repaired BVE-110,362 binary multisets and Probe entry against the 4687
  Kissat authority.
- `4698` validates the repair causally: all four BVE begin/end binary
  multisets at conflict 110,362 are byte-exact to the 4687 Kissat authority
  (previously +1/-1/+7 pair diffs), and focused ASan/UBSan decide/restart/
  no-heuristics tests pass. Checkpoint rerun `4700` (job `1659412`; first
  submission `1659411` failed closed on a stale path, and the inherited
  validity contract flags the new 15-column rows, so its journal is read
  directly) shows the repaired pin exact to Kissat through conflict 118,697
  -- 8,335 conflicts past the old split -- with a new single-decision
  divergence at 118,698, inside the conflict-118,697 Reduce interval, that
  chaotically amplifies to -52,689 decisions by Probe 120,310/120,311. That
  Reduce-time mechanism is the next localization target after banking.
  Checked Release measurement `4699` (job `1659413`) is running.
- Checked Release `4699` provisionally returns expected Rank-27 UNSAT in
  468.777 seconds / 7,441,126 conflicts / 185,077,336 decisions on the
  repaired binary, versus production 483.655 seconds; its DRAT checker is
  running and nonblocking. The wall gain is modest because the single-decision
  118,698 split still amplifies chaotically; the repair is banked on causal
  fidelity (byte-exact BVE multisets, +8,335-conflict exact prefix), not on
  this one timing.
- Banked as commit `f3ac562` (Release binary
  `b33031547de2aa1445b6c01ec567acddf20d3f84ea002c715550990cd260bb79`,
  byte-reproduced by the fuzz pin build). Per the standing directive, the
  thorough fuzz campaign launched immediately after the commit: nightly
  200k+LRAT leg (job `1659414`), corpus+20k random-policy LRAT leg
  (`1659415`), and reuse-scenario stress leg 10k (`1659416`), all on
  `build/pin-fuzz-f3ac562`. Regression gates submitted from the identical
  SHA: homogeneous Rank-1--40 `4701` (job `1659417`) and frozen B28+C5
  paired gate `4702` (job `1659431`), both full-width and unthrottled.
- Next localization target after the gates: the single-decision divergence
  at conflict 118,698 inside the conflict-118,697 Reduce interval.
- Fuzz legs two and three are terminal PASS: corpus+20,000 random-policy
  cases with zero failures (17,670 verdicts; 11.7% five-second timeouts saved
  as non-terminating with no oracle run), and 10,000 reuse-scenario stress
  cases with zero failures and 24,450 stress-target applications. The nightly
  200k+LRAT leg continues.
- Randec probe `4703` (2/2 valid/clean) rules randec OUT for 118,698: both
  arms switched to stable at conflict 111,797/8, Kissat's randec state is
  inactive (remaining 0, sequence 22, constant RNG across eight mode-ungated
  pre rows), and Tactix's stable-gated observer is silent by design. The
  interval is stable-mode heap picks: first pick 145 matches, the
  4,226-decision common-prefix hashes differ, so the first differing pick is
  inside. Submitted 4621-style trajectory-only window `4704` (job `1659535`)
  over conflicts 118,696--118,699.
- Both regression gates for `f3ac562` pass provisionally with all checkers
  nonblocking: homogeneous Rank 1--40 `4701` has 40/40 expected statuses in
  9,596.978 seconds total (versus 9,412.958 at the prior checkpoint, +2.0%
  trajectory wobble; Rank 27 measures 496.458 here and 468.777 in standalone
  `4699`), and B28+C5 `4702` has all 66 expected statuses at
  1,332.203/2,439.690 seconds, candidate/shipped ratio 0.546054 against the
  frozen 1.10 limit. Final classification still requires every proof/model
  check and zero residue.
- `4704`'s window traces (both arms complete; the inherited endpoint contract
  merely flags the +1 overshoot and Kissat's SIGTERM convention) localize the
  118,697 interval split to one stable-mode pick: at decision 36,646,252
  (interval position 2,080, no restart or backtrack nearby) Kissat decides
  variable 176863 while repaired Tactix pops 148563 and never decides 176863
  in the window; the streams realign shifted by one. With identical
  assignments this is a scores-heap ranking difference for 176863 -- either a
  score value or a physical tie-topology divergence. Submitted paired
  post-decision heap snapshot `4705` (job `1659537`) at decision 36,646,251.
- `4705` (2/2 valid/clean) exonerates the heap entirely: all 101,679 members,
  positions, and scores are identical; the single difference is that variable
  176863 is already ASSIGNED in Tactix and unassigned in Kissat at the
  aligned post-decision snapshot. With identical decision prefixes, a
  differing propagation fixpoint means the clause databases first diverge in
  some learned clause between 110,362 and 118,697 (implication-graph /
  watch-order dependent) without moving any decision counter. Submitted
  learned-content comparison `4706` (job `1659539`) from conflict 0 through
  118,698 on the repaired pin.
- `4706` (2/2 valid/clean) exonerates learning as well: all 101,242 learned
  clauses through conflict 118,697 match in both order and set hashes, and
  the conflict-118,698 clause is identical too (only the known one-decision
  counter offset). With decisions, learned clauses, eliminate outputs, and
  heaps all exact, the remaining owner of the one-propagation split is the
  conflict-118,697 Reduce's retention decision (glue/used metadata driven).
  Submitted paired one-row formula fingerprint `4707` (job `1659541`) at
  reduce_end 118,697.
- `4707` (2/2 valid/clean) confirms it: at reduce_end 118,697 the irredundant
  count and hash (527,801 / c39606d4...) and binary hash are exact, but the
  redundant sets differ -- Tactix 18,623 versus Kissat 18,624 with different
  content hashes. Reduce retained different clauses from identical learned
  inputs, so the divergent input is per-clause retention metadata (glue/used/
  reason bits, which depend on reason selection and therefore physical watch
  order). Submitted late-open full metadata dump `4708` (job `1659543`) at
  reduce_begin 118,697 to name the clause and the field.
- `4699` is now final valid: the repaired Rank-27 UNSAT certificate verified
  independently (3,279.308 seconds) with artifacts deleted and zero residue.
- `4708` (2/2 valid/clean, 548k clauses per arm) shows the DBs already differ
  at reduce BEGIN: Tactix holds one extra redundant clause
  `(-145,175252,176747,176815,176849,176863)` -- containing exactly the
  variable whose propagation split the trajectory -- while Kissat holds two
  others (one shrunken=1), and 301 common redundant clauses differ only in
  metadata (mostly the known representation-only shrunken flag, plus at least
  one glue 5-versus-6). The clause-set divergence therefore arises between
  110,362 and 118,697 through a non-learning mechanism (OTFS/eager
  subsumption acting on differently-represented clauses), and the glue
  differences feed Reduce retention. Submitted eliminate_end-110,362 content
  dump `4709` (job `1659545`) to bound the creation window.
- `4709` (2/2 valid/clean) pins the mechanism: at eliminate_end 110,362 the
  clause SETS are fully identical (irredundant hash-exact, redundant
  membership 0/0), but 15 common redundant clauses have Tactix glue = size-1
  where Kissat keeps glue = size. Reference audit: kissat clamps glue to
  size-1 only in search-time OTFS (strengthen.c:42), Sweep (sweep.c:1007),
  and Vivify (vivify.c:116/755/846); its fused forward strengthen (forward.c)
  and generic root flush (collect.c) never touch glue. Tactix wrongly applied
  `kernel_shrunken_glue` in Strengthen's in-place rewrite and in
  `kernel_flush_root_clause`. Repaired both (root flush now takes a
  `clamp_glue` parameter so Vivify's schedule-time root simplification keeps
  its reference clamp); regression `test_root_flush_preserves_glue` plus the
  pre-existing Vivify pass-time-tier test cover both semantics. Release is
  75/76 (doc-size only) and ASan/UBSan focused suites pass. Submitted causal
  eliminate_end re-dump `4710` (job `1659547`) on the repaired trace pin.
- `4710` (2/2 valid/clean) closes the glue mechanism: the eliminate_end
  110,362 redundant state now matches pinned Kissat with ZERO membership and
  ZERO metadata differences (16,587 clauses, all glue/priority/searched/used/
  reason/shrunken fields equal). Submitted checked Release `4711` (job
  `1659549`) and full boundary journal `4712` (job `1659550`) to measure the
  stacked attach+glue repairs.
- The thorough post-commit fuzz campaign for `f3ac562` is terminal PASS on
  all three legs: nightly 200,000 generated cases with LRAT (job `1659414`,
  196,421 verdicts, 1.8% five-second timeouts, VariableCompaction reached 469
  times), corpus+20,000 random-policy LRAT cases (job `1659415`, zero
  failures), and 10,000 reuse-scenario stress cases with 24,450 target fires
  (job `1659416`) -- zero crash, sanitizer, invariant, model, proof, or
  differential failures anywhere. The user's fuzz-after-commit directive is
  discharged; the follow-on glue-preservation commit carries its own bounded
  fuzz leg.
- `4712` is valid/clean: the stacked repairs reach UNSAT in 5,793,285
  conflicts (from 6,804,239 pre-glue; Kissat 4,120,691, ratio 1.406 from
  1.65). Probe 120,310 entry is now byte-exact in conflicts/decisions and
  formula scalars, and conflict counters stay EQUAL through the 151,982
  Probe (decisions +0.23%); the first remaining differences are nine extra
  Tactix redundant clauses at the 120,310 Probe exit and the decision drift
  by 151,982. The next localization owner is inside that Probe or the
  ensuing CDCL window.
- The glue repair is banked as commit `14f9417` (Release binary
  `4b6d0707...626d9`, byte-reproduced by `build/pin-fuzz-14f9417`). Its
  bounded fuzz is terminal PASS on both legs: corpus+10,000 default-policy
  LRAT cases (job `1659551`, Strengthen fired in 218 cases) and 10,000
  random-policy LRAT cases (job `1659552`), zero failures each. Regression
  gates `4713` (Rank 1--40, job `1659553`) and `4714` (B28+C5, job
  `1659569`) run from the identical SHA; checked Release `4711`'s DRAT
  checker remains nonblocking.
- `4702`, the B28+C5 gate for `f3ac562`, is now FINAL: 66/66 valid rows with
  every model and proof independently checked and zero residue, at the
  0.546054 candidate/shipped ratio recorded provisionally.
- The `14f9417` gates split: B28+C5 `4714` preserves all 66 statuses at
  ratio 0.622292, and Rank 27 confirms 432.240 seconds in the homogeneous
  matrix, but Rank 19 (expected SAT; 227.238 seconds at `02ab6f1`, 251.483
  at `f3ac562`) times out at 1,200 seconds, so homogeneous `4713` is 39/40
  and the committed state FAILS the uniform gate. The glue semantics are
  reference-verified (zero metadata deviation at the 110,362 boundary), so
  per the faithful-port rule the repair is retained locally, nothing is
  pushed, and the Rank-19 blowup is the next coupled discrepancy to
  localize: the old size-1 clamp was masking it.
- `4701`, the homogeneous gate for `f3ac562`, is now FINAL as well: 40/40
  valid rows with every SAT model and UNSAT proof independently checked and
  zero residue. `f3ac562` is therefore a fully validated checkpoint (both
  gates final, thorough three-leg fuzz PASS); `14f9417` remains local-only
  until the Rank-19 interaction is repaired. Rank-19 paired boundary
  diagnostic `4715` (job `1659659`) is running.
- `4715` quantifies the Rank-19 blowup: pinned Kissat solves SAT in 302.673
  seconds at 701,597 conflicts, while the glue-repaired trace pin is still
  unsolved at 9,995,035 conflicts when the 2,700-second diagnostic cap
  kills it. The journals' first Probe at conflict 3,139 matches EXACTLY in
  all six scalars; the second triggers at 9,493 versus 9,570, so the first
  decision split lies in that early window (Rank 19 never had a converged
  prefix -- its previous 227/251-second solves rode a different chaotic
  path). Submitted per-conflict checkpoint pair `4716` (job `1659663`,
  90-second diagnostic kills; initial submission `1659661` failed closed on
  a working-directory drift and was cancelled) to find the first decision
  split from conflict 0.
- `4716` (2/2 valid) finds Rank 19's first decision split at conflict 4,197:
  the interval decision counts run [1,0,9,...] in Tactix versus [1,0,0,9,...]
  in Kissat -- a nine-decision burst lands one conflict earlier in the
  repaired pin, then the counters realign with diverged content. Submitted
  bounded window trace `4717` (job `1659665`) over conflicts 4,190--4,205.
- `4717` (2/2 clean) refines the split: after twenty identical decisions in
  the conflict-4,190 interval, Tactix's stable-mode pick at decision 103,133
  is -79,275 versus Kissat's -78,966 -- the same stable-heap signature as
  Rank 27's 176863 case. Kissat's last Reduce before the split is at
  conflict 3,415 (red 1,972 to 1,743). Submitted metadata dump `4718` (job
  `1659667`) at reduce_begin 3,415 on Rank 19.
- `4718` (2/2 valid) reaches the physical frontier: at reduce_begin 3,415 the
  redundant counts are equal, but one long clause differs in content, three
  clauses (sizes 240--264) have Tactix glue = Kissat + 1, and 227 differ only
  in the known representation-local shrunken flag. Under chronological
  backtracking a variable's assignment LEVEL depends on physical propagation
  order, so glue recomputes (kissat deduce.c/promote.h) can diverge while
  every scalar counter stays exact: the remaining class is
  implication-graph/watch-order parity -- exactly the staged
  representation-convergence program the plan defers to after inprocessing
  and CDCL. Submitted paired glue-transition window `4719` (job `1659669`)
  over conflicts 1--3,415 to name the first differing promote.
- `4719` is observer-invalid/clean: both arms flooded the two-million-row cap
  with `phase_save` rows at conflict 213; no claim. Both diagnostic solvers
  already carry a `GLUE_ONLY` filter retaining only `reason_glue` promote
  rows; unchanged-pin retry `4720` (job `1659671`) uses it over the same
  window.
- `4720`/`4721` are observer-invalid for their headline claims and those
  claims are WITHDRAWN: the kissat overlay's learned and reason_glue
  emitters silently dropped any payload over 4,096 bytes, so the "missing"
  Kissat learned row at conflict 1,161 and "missing" 240-literal promote at
  1,164 were dropped rows, not semantic events. Steering that survives: the
  4,721 window's 18,837 implication rows and all 515 resolve/reason-size
  rows at conflict 1,161 are IDENTICAL across arms, so propagation and the
  analysis walk match exactly there. The 4718 formula-dump glue diffs
  (per-literal fprintf, uncapped) remain valid evidence. Enlarged both
  overlay buffers to 65,536 bytes and submitted unchanged-question retry
  `4722` (job `1659675`).
- `4714`, the B28+C5 gate for `14f9417`, is FINAL: 66/66 valid rows with all
  checks and zero residue at ratio 0.622292. Only the Rank-19 homogeneous
  loss blocks the glue commit.
- `4722` (fixed buffers) restores row parity at 1,164 -- both arms promote
  the same clause 66-to-64, confirming the artifact -- and names the TRUE
  first promote divergence: at conflict 1,171 / decision 85,699 Kissat
  promotes ternary (-100265,-82860,95684) glue 3-to-2 while Tactix does not
  (705 versus 644 promotes by 3,415). Submitted full-detail window `4723`
  (job `1659677`) over conflicts 1,168--1,172.
- `4723` retires the promote-stream method for cross-solver comparison: the
  conflict-1,171 event is Kissat's OTFS-strengthen promote, Tactix performs
  the same strengthen through a path that emits no reason_glue row, and the
  ternary's state at reduce 3,415 is IDENTICAL in both arms (glue 2, all
  metadata). No semantic claim from 4720/4722/4723 promote-count deltas.
  Ground truth remains the 4718 formula dumps. Submitted bisecting dump
  `4724` (job `1659679`) at reduce_begin 2,001.
- `4724`'s dumps (the Kissat task wrote its complete 67 MB dump then timed
  out because this overlay lineage lacks the SET_STOP exit on the generic
  dump path; the dump itself is complete and the timeout claims nothing)
  show reduce_begin 2,001 FULLY converged: identical redundant membership,
  zero glue diffs, one inert shrunken-flag row. Rank 19's three glue diffs
  and one content diff are therefore created within conflicts
  2,001--3,415. Submitted full-payload learned-only window `4725` (job
  `1659681`) over that window to compare the three clauses' creation
  levels/glue directly.
- Cross-referencing 4722's corrected promote streams with the 4724 dumps
  localizes ALL THREE Rank-19 glue diffs to one instant: each clause is
  identical at reduce 2,001 (glues 233/184/215) and receives its decisive
  recompute at conflict 3,139 -- the first Probe -- where Tactix counts
  exactly one more distinct level than Kissat (233-to-42 versus 41,
  184-to-124 versus 123, 215-to-126 versus 125) through the vivify-path
  recompute. Submitted full-detail single-conflict window `4726` (job
  `1659683`) whose vivify-trail imply rows carry per-assignment levels.
- The chain is now fully closed to a single mechanism. `4726` stopped before
  the Probe (its interval lies past the boundary); corrected window `4727`
  plus a new paired selected-clause recompute-input observer (campaign
  `4728`, jobs `1659685`/`1659687`) prove: at the conflict-3,139 vivify
  recompute both arms sit on candidate ordinal 13 with identical tick
  deltas, and exactly ONE literal differs -- external 90754 is level 0
  (root) in Tactix but level 17 (assumption) in Kissat -- producing the
  42-versus-41 level count; candidate 13 is likewise size 467 versus 468
  because Tactix's copy already lost the root-false literal. The probe
  journals name the owner: Backbone-early at probe 3,139 fixes 244 variables
  in Tactix versus Kissat's 232 (125,926 to 125,682 versus 125,694); twelve
  variables including 90754 move from Kissat's later Backbone pass into
  Tactix's first, and the sets re-converge at the second Congruence end
  (both 125,224). NEXT MECHANISM: compare the Backbone round-1 stopping
  rule (probing order, per-round budget/cutoff) against kissat backbone.c;
  the 12-variable attribution difference is the whole remaining Rank-19
  causal chain.
- Paired per-probe effort journal `4729` (jobs `1659689`; new dormant
  `TACTIX_BACKBONE_TICKS_TRACE`/`KISSAT_BACKBONE_TICKS_TRACE` observers)
  sharpens it further: the effort budgets are EXACT (both 351,454) and tick
  deltas are equal through probe 83, so neither the budget anchor nor the
  cache-line tick model deviates. At probe ordinal 84 the candidate
  sequences diverge: Tactix probes external -18422 where Kissat probes
  18247, each arm value-filtering the other's candidate, then the streams
  realign with a four-tick offset. NEXT STEP: paired dump of the backbone
  computation's ordered candidate list with priority bits (Tactix
  `backbone_priority` versus kissat persistent `backbone0/backbone1`
  flags) at the conflict-3,139 computation entry; the divergence is in
  candidate order or priority-flag maintenance, not effort.
- Scheduled-list campaign `4730` (jobs `1659691`; the effort journals now
  also dump the ordered candidate list with priority bits) confirms
  persistent-flag divergence: the two lists first differ at entry 17,652
  where Tactix holds (-18422) PRIORITIZED and Kissat schedules it later
  unprioritized, with 1,004,038 versus 1,004,062 total entries and 476,601
  versus 476,595 prioritized. Those flags were written by the preprocessing
  backbone computation at conflict 0, whose input state is uniform, so the
  creating mechanism is inside that first computation. Submitted the same
  paired journals at conflict 0 as `4731` (job `1659693`).
- `4731` closes the Rank-19 chain to a single deferred mechanism. At conflict
  0 the backbone schedule is BYTE-IDENTICAL (252,042 entries, zero order or
  priority differences), the effort budget is identical (200,000), and both
  arms probe the SAME externals in the SAME order; only the cumulative tick
  counter diverges (+19 by probe 59, concentrated on one probe's scan). The
  owner is binary-propagator tick accounting: kissat prices its backbone
  scan with a GLOBAL `large_clauses_watched_after_binary_clauses` stop-early
  flag (backbone.c:210), while Tactix uses a PER-LIST
  `heads[lit].binaries_first()` (kernel.cpp propagate_literal_binary). On
  lists where the two disagree the scanned word count differs, so
  `1 + cache_lines(p - begin)` differs; accumulated over ~800 probes it
  truncates the first Backbone computation at a different tick, writing
  different persistent `backbone0/backbone1` flags, which reorders the next
  computation's schedule, moves twelve variable fixes between Backbone
  passes, and cascades (via one glue recompute and Reduce retention) into
  the chaotic SAT blowup.
- DISPOSITION: this is the staged arena/watch representation-convergence
  item the plan sequences last, not an inprocessing or CDCL defect. The
  glue-preservation repair `14f9417` is reference-verified correct (zero
  metadata deviation at the 110,362 and 3,415 boundaries) and must not be
  reverted per the faithful-port rule; it exposed a pre-existing latent
  tick-accounting divergence that the old size-1 clamp masked on Rank 19. It
  stays LOCAL (unpushed) until the global-versus-per-list backbone stop-early
  tick flag is converged. Production/origin remains `f3ac562` (Rank 27
  483.655 to 468.777 seconds, both gates final, three-leg fuzz PASS). NEXT
  SESSION: converge the backbone binary-propagation stop-early flag to
  kissat's global semantics under isolated measurement, then re-run the
  Rank-1--40 and B+C gates on `14f9417`.
- Final gate classification for `14f9417`: B28+C5 `4714` is 66/66 valid
  (ratio 0.622292); homogeneous `4713` is 39/40 -- the ONLY failure is Rank
  19's 1,200-second timeout, every other rank valid with checked model/proof
  and zero residue. This isolates the block to the single Rank-19
  representation interaction above.
- Pushed the fully validated line: origin/main now at `f3ac562` (13 commits
  published); `14f9417` remains the only local commit, held until the
  Rank-19 watch-order interaction is closed or the gate is otherwise
  satisfied.
- Rank-27 Release campaign `20260901-4490-tierd-rank27-forward-lifecycle-release`
  provisionally returns expected UNSAT in 423.068 s / 5,854,078 conflicts /
  200,851,977 decisions. This is 0.731569x the prior 578.302 s candidate, but
  still 1.627103x pinned Kissat time and 1.420654x its conflicts. The exact
  forward-step currency and fresh per-Eliminate-round allowance are retained;
  the gap is reduced, not closed, and coarse trajectory localization resumes.
  DRAT checking is pending and nonblocking.
- Consolidation fuzz campaign `4476` banked 6,000 generated default/Factor-
  stress cases cleanly but found five random-policy dense-image assertions.
  The general repair now keeps physical binaries, the large membership mirror,
  and BVE's optional mixed-order stream synchronized across deduplication,
  Sweep/Substitute bulk transactions, Congruence reconstruction, and in-place
  large shrink. Corpus 004/005/015/017/020 pass the per-rule debug oracle;
  focused tests and a whole-corpus replay are the immediate correctness gate.
- All ten focused sanitizer targets pass after the dense-image repair. The
  attempted Claude-owned 27-case replay did not launch: its first owner process
  failed DNS, and the required network retry was denied because sending the
  internal repository/build prompt externally lacks explicit authorization.
  Do not work around that decision or self-run the fuzz campaign; resume it
  only with renewed disclosure authorization. The user later granted that
  authorization directly, and the Claude-owned consolidation fuzz campaign
  `20260901-4631-compact-active-consolidation-fuzz` completed with a terminal
  PASS (details in the Rank-27 priority section and its manifest).
  Initial campaign `4492`/job `1658522` was canceled observer-invalid before
  interpretation because its Kissat allowlist used Tactix event names; no
  claim is made from it. Corrected 500k campaign `4493`/job `1658524` was then
  canceled after ten active minutes because global-trace overhead was too
  expensive for a coarse probe; partial rows are uninterpreted. Reduced
  campaign `4494`/job `1658526` was also canceled after six active minutes;
  the global observer is retired from this coarse step and no partial rows are
  interpreted. Replacement `20260901-4495-tierd-rank27-standalone-boundaries`,
  job `1658528`, runs both arms to their ordinary UNSAT verdict with only
  constant-time standalone macro journals. It records sparse outer
  rule/preprocess/probe summaries, produces no proof artifact, and will decide
  whether the next inferior split is later inprocessing or intervening CDCL
  before any detailed trace is opened.
- Standalone boundary campaign `4495`/job `1658528` is valid and clean; both
  diagnostic arms return expected UNSAT with zero stderr/residue and no proof
  artifact. Probe outputs at conflicts 32,087 and 48,432 are core-exact. Both
  enter the second Eliminate at conflict 60,618 with 11,546,720 decisions and
  exact active/irredundant/redundant/binary counts. They exit with Tactix/Kissat
  active 135,042/135,036, irredundant 589,267/589,274, and binaries
  170,317/170,316 (redundant 14,797/12,050). By the next Probe Tactix has spent
  149,013 extra decisions; by the following Probe the excess is 7,003,762.
  Thus the second Eliminate is the earliest demonstrated inferior owner. Add
  only its internal Strengthen/BVE boundaries next, then narrow the first
  mismatching subround/candidate lifecycle.
- Internal-boundary campaign `4496` is submitted as Slurm job `1658530`.
  Its standalone Kissat observer records only Strengthen begin/end and BVE
  begin/end across the two conflict-60,618 Eliminate rounds and exits after
  exactly eight rows. It reuses the complete Tactix trace from `4495`; no
  global trace, proof checker, duplicate candidate run, or unrelated job is
  involved.
- Job `1658530` completed valid/clean in 63.419 s with exactly the requested
  eight rows and zero stderr. Round-one Strengthen first differs by one
  irredundant clause (577,922/577,921), but round two enters BVE with exact
  counts after Strengthen heals it. The durable split is therefore round-two
  BVE: Tactix/Kissat finish at active 135,042/135,036, irredundant
  589,267/589,274, and binary 170,317/170,316. Compare that BVE's initial
  schedule fingerprint next; only then narrow its pop/action lifecycle.
- Added a symmetric zero-based schedule-ordinal selector to the standalone BVE
  observer; focused executable/static tests pass. Campaign `4497`, Slurm job
  `1658531`, fingerprints only ordinal 1 at conflict 60,618 in both arms and
  exits before its first candidate. This separates an inherited schedule split
  from a later action-lifecycle split without a global trace.
- Job `1658531` completed valid/clean. The second-round entry has exact
  active/irredundant/redundant/binary counts, but its schedule is already
  different before the first pop: Tactix/Kissat schedule 28,586/28,585, and
  their set, insertion, topology, and score fingerprints all differ. Thus BVE
  inherits a physical formula/retry image divergence; dump the compact member
  rows once to identify its width and first concrete variables.
- Campaign `4498`, Slurm job `1658533`, performs that one paired 28k-row member
  dump at the same second-schedule endpoint. It records only external member,
  heap position, two physical occurrence counts, and score bits, then exits;
  no candidate stream or proof checker is involved.
- Job `1658533` completed valid/clean and proves the schedule split is exactly
  one eligibility bit. All 28,585 common members have identical polarity
  occurrence counts and score bits. Tactix alone schedules external variable
  81,489 at position 22,379 with 15/10 occurrences; that one insertion perturbs
  19,864 physical heap positions. Inspect only 81,489's active/fixed/eliminated
  state at the same endpoint next.
- Campaign `4499`, job `1658535`, records only external 81,489's selected
  schedule-state row in each arm at ordinal 1. It will distinguish a differing
  eliminate/retry flag from an active/fixed/eliminated identity difference,
  then exit before BVE consumes the schedule.
- Job `1658535` completed valid/clean. Variable 81,489 is active, unassigned,
  nonfixed, noneliminated, and has identical 15/10 physical occurrences in
  both arms. Only Tactix's retry/eliminate eligibility bit is set. Trace that
  variable's first-round candidate begin/end state next; do not widen to other
  candidates or CDCL.
- Added a standalone selected-candidate begin/end observer with symmetric
  eliminate/retry state and immediate clean exit; focused/static tests pass.
  Campaign `4500`, job `1658537`, applies it only to conflict-60,618 variable
  81,489 and emits two rows per arm.
- Job `1658537` completed valid/clean. Both arms begin variable 81,489 eligible,
  decline it, and end with the bit cleared. Tactix spends 44 resolution ticks
  versus Kissat's 40, but the extra second-round eligibility is created only
  afterward. A Tactix-only one-row observer now arms at that end and reports
  the first later false-to-true mutation source.
- Campaign `4501`, job `1658539`, runs only that immutable Tactix endpoint and
  exits after the first post-decline false-to-true retry transition. The
  observer is covered by an executable regression and touches no solver path
  when its Trace env is absent.
- Job `1658539` completed valid/clean: the first post-decline rearm of 81,489
  is caused by deletion of an irredundant large clause. Binary-copy lifecycle,
  score updates, and search are exonerated. Extend the same one-row observer
  with the deleting BVE trigger and canonical external clause hash.
- Campaign `4502`, job `1658540`, performs that one-row extension on the same
  immutable endpoint. It remains Tactix-only, exits at the first mark, and
  launches neither a comparator rerun nor a proof checker.
- Job `1658540` is observer-invalid/clean: solver exit and stderr are clean and
  the first rearming object is a ternary (id 3,834,631, canonical hash
  `64ef2d3cdee7abef`), but trigger `0` showed that diagnostic ownership ended
  before the policy's post-candidate propagation/unit flush. Keep that bounded
  clause evidence; retain the candidate through this reference-owned tail and
  rerun once with external ternary literals.
- Corrected replacement `4503`, job `1658541`, retains the selected candidate
  as owner through its post-rule propagation/unit flush and prints the
  canonical external ternary. Its focused observer regression passes.
- Job `1658541` completed valid/clean. BVE candidate 416,622 owns the first
  rearm: its post-candidate flush deletes ternary
  `(-81489,-81367,81506)` (id 3,834,631, hash `64ef2d3cdee7abef`), setting
  81,489's retry bit. Compare candidate 416,622 begin/end in both arms before
  instrumenting the root flush itself.
- Campaign `4504`, job `1658542`, performs that paired two-row comparison and
  exits before the post-candidate flush. It reuses the already-tested
  standalone candidate observer and produces no proof.
- Job `1658542` completed valid/clean. Candidate 416,622 declines in both arms,
  clears eligibility in both, and spends an exact 49 resolution ticks. The
  Tactix counter enters six ticks ahead, so the transaction itself is not the
  owner. Add exactly one `postflush` row with cumulative root/unit and trail
  deltas to compare the ensuing propagation/deletion tail.
- Campaign `4505`, job `1658544`, adds only that third `postflush` row for
  candidate 416,622 in both arms. The generated Kissat hook and Tactix hook
  pass static/executable checks; no global trace or proof is opened.
- Campaign `4505` is observer-invalid/clean. Kissat emits an unchanged
  postflush row (units 643, trail/propagate 20); Tactix has unchanged roots
  643 and trail/propagate 643 at begin/end, then correctly skips the no-op
  tail. Owned task `1658544_0` was canceled rather than allowed to full-solve.
  The retry observer's retained last-candidate label was stale into the next
  Strengthen round: the selected ternary is the one-clause round-one
  Strengthen excess, deleted by round-two Strengthen when counts heal. Trace
  that exact ternary's first forward result symmetrically.
- Campaign `4506`, job `1658547`, uses the existing standalone exact-clause
  Strengthen observer on `(-81489,-81367,81506)` at conflict 60,618. Each arm
  exits at its first result; no BVE candidate stream, global trace, or proof is
  involved.
- Job `1658547` completed valid/clean and locates the causal action mismatch.
  Kissat discovers, schedules, checks, and subsumes ternary
  `(-81489,-81367,81506)`; Tactix checks it at candidate index 172,612 but
  returns no-op with no partner. This exactly explains the one-clause
  round-one Strengthen excess and its later 81,489 retry mark. Record only
  Kissat's winning partner next, then repair the general forward-watch image.
- Campaign `4507`, job `1658553`, is the bounded Kissat-only continuation: its standalone
  endpoint records the winning partner for that exact candidate and exits at
  the existing selected result. It launches no duplicate Tactix arm and no
  proof checker. Separately, the requested whole-codebase cleanup remains
  queued after Rank-27 convergence and its fuzz checkpoint, not interleaved
  with causal trajectory work.
- Job `1658553` completed valid/clean in 63.548 s with zero stderr or residue.
  Kissat subsumes through binary `(-81489,81506)`, reached from literal 81,506.
  Campaign `4508` therefore reads only the selected Tactix candidate's kernel
  copies, mirror copies, and round-local stream copies for that exact pair.
- Campaign `4508`, job `1658555`, is submitted as that one-row-pair,
  one-row-result Tactix endpoint. Focused Strengthen and event-trace tests pass;
  it launches no proof checker.
- Job `1658555` completed valid/clean in 55.458 s with zero stderr or residue.
  At the exact candidate Tactix has zero copies in both live watch mirrors and
  zero copies in the two-entry 81,506 local forward stream. The missing
  subsumer therefore predates mixed-stream construction. Trace this single
  binary's paired add/delete history next, ending at the same candidate.
- Campaign `4509`, job `1658556`, is the pair-filtered lifecycle comparison
  for `(-81489,81506)`. Both arms stop at the already-selected candidate; no
  global event log or proof task is opened.
- Job `1658556` completed cleanly (Tactix 56.137 s, Kissat 65.126 s) but is an
  observer-gap result: neither ordinary lifecycle hook emits a selected row.
  Source audit explains the Kissat side—forward large-to-binary conversion
  installs watches directly and bypasses its ordinary binary-add callback.
  Tactix's normal replacement path would log an add if it derived the pair.
  Add a single Kissat callback at that direct conversion to name its ternary
  source; do not widen the lifecycle stream.
- Campaign `4510`, job `1658558`, is submitted with exactly that one-site
  callback and the existing single-pair filter. It remains Kissat-only, stops
  at the selected candidate, and starts no proof task.
- Job `1658558` completed valid/clean in 65.930 s. Kissat creates the missing
  binary during this same Strengthen pass by reducing source ternary
  `(81506,-81489,-81502)`. Compare that exact earlier candidate in both arms
  and record Kissat's winning strengthener; this moves one causal edge earlier
  without widening the log.
- Campaign `4511`, job `1658559`, is submitted as that paired exact-source
  action endpoint. Both arms stop at the source result; no proof task or global
  event stream is started.
- Job `1658559` completed valid/clean. Both solvers strengthen the source with
  the same binary `(-81489,81502)` and remove `-81502`; the action logic is
  exact. Kissat's in-place source precedes the victim, but Tactix schedules its
  packed replacement 1,431 candidates after the victim because the equal-size
  tie used append-only raw Ref instead of the inherited logical arena position.
  Strengthen now stores that position in each candidate and uses it for the
  stable equal-size tie. An adversarial replacement fixture proves the source
  creates a binary early enough to subsume a later victim in the same pass.
  Validate the exact Rank-27 victim next before promoting to round boundaries.
- Campaign `4512`, job `1658561`, is submitted as that repaired Tactix-only
  exact-victim endpoint. It requires subsumption by `(-81489,81506)` and
  launches no proof task.
- Job `1658561` is observer-invalid/clean but causally successful: the solver
  reports action=subsumed with binary `(81506,-81489)` at index 172,613. Only
  the harness's textual pair-order assertion rejected the unordered reverse
  spelling. Do not duplicate the run; promote to the complete two-round
  Strengthen/BVE boundary and compare its scalar state to pinned Kissat.
- Campaign `4513`, job `1658563`, is submitted as that repaired Tactix-only
  eight-boundary endpoint, reusing the immutable Kissat `4496` reference and
  launching no proof task.
- Job `1658563` is observer-invalid/clean but causally exact. All eight rows
  were durable before the owned job was canceled: repaired Tactix exactly
  matches Kissat's formula scalars at first Strengthen end
  `146866/577921/14797/180741`, first BVE end
  `137314/602571/14797/171167`, second Strengthen end
  `137307/584364/14797/171656`, and second BVE end
  `135036/589274/14797/170316` (active/irred/red/binary). The stop harness used
  lowercase names while Tactix emits `Strengthen`/`BVE`; no solver issue is
  implicated. Check the second BVE schedule fingerprint next without rerunning
  Kissat.
- Campaign `4514`, job `1658564`, is submitted as that repaired Tactix-only
  schedule fingerprint, reusing immutable Kissat campaign `4497` and launching
  no proof task.
- Job `1658564` completed valid/clean/exact in 58.355 s. The repaired second
  BVE schedule has 28,585 members and exactly matches Kissat's external sum,
  set XOR/sum, insertion hash, topology hash, and score XOR/sum. This closes
  the durable inprocessing divergence through second BVE. Proceed to full
  Release tests and Rank-27 timing/correctness validation; proof checking is
  asynchronous and never a launch dependency.
- Release rebuild is clean. The full suite passes 75/76; the sole failure is
  the already-separated documentation-size cleanup (`PROGRESS.md` exceeds its
  350-line budget), while all 75 code, proof, N1--N4, static-overlay, and
  hygiene tests pass. Campaign `4515`, job `1658567`, is the full repaired
  Rank-27 Release solve. Its provisional result will be consumed immediately;
  the independent proof checker may continue afterward and will never block
  the next launch.
- `4515` provisionally returns expected UNSAT in 718.838 s / 13,099,173
  conflicts / 220,390,778 decisions with 679,328 KiB peak RSS. That is
  1.699107x time and 2.237615x conflicts versus the 423.068-second predecessor,
  and 2.764623x time and 3.178878x conflicts versus pinned Kissat. The proof
  checker continues nonblocking. Keep the verified Strengthen ordering repair:
  it closes the exact second-Eliminate state but exposes a much worse later
  trajectory. Campaign `4516`, job `1658570`, records only Kissat's
  inprocessing begin/end boundaries after the common conflict-60,618
  second-BVE state and will be compared with retained repaired Tactix `4513`.
- `4516` is observer-invalid/canceled cleanly. Its generic Kissat snapshot hook
  admitted high-frequency non-macro names ending in `_begin`/`_end`, growing a
  6.2 MB partial stream and slowing the solver; no row is interpreted. Existing
  valid `4495` already supplies Kissat's post-common control boundaries: its
  next Probe begins at conflict 68,537 / 20,221,397 decisions, while repaired
  Tactix `4513` begins at 68,539 / 23,014,173. Thus the next gap is already in
  intervening CDCL (only two conflicts but 2,792,776 extra decisions). Add a
  strict Tactix macro-only endpoint from Eliminate end to that Probe begin.
- The strict O(1) macro observer is covered by `test_event_trace`; focused
  `test_event_trace` and `test_strengthen` pass. Campaign `4517`, job
  `1658572`, runs only repaired Tactix from after conflict 60,618 to its first
  `probe_begin`, recording control macros and direct counters without any
  formula/variable scan, global trace, solver verdict, or proof task.
- `4517` completed valid/clean in 69.838 s with 1,391 rows. Both arms' first
  two post-common restarts are exact at conflict/decision 61,291/11,549,234 and
  62,516/11,968,831. The first control split is the stable-to-focused switch:
  Kissat switches at 62,625/11,972,953, Tactix at 62,627/11,972,954. Narrow
  only final learned-clause hashes across conflicts 62,517--62,627 next; do not
  trace the roughly four thousand decisions until that semantic stream splits.
- Campaign `4518`, job `1658574`, is that paired learned-only window. It uses
  immutable current Tactix and established Kissat observer pins, a predeclared
  62,516--62,627 output interval, a 500-row cap, and one solver per full-width
  array task. No decision/watch bodies or proof work are opened.
- `4518` completed valid/clean: 112 compact learned rows per arm, 18--20 KiB
  each, zero stderr. Final learned clauses are exact through conflict 62,625.
  At 62,626—after Kissat switched but while Tactix remains stable—the first
  clause mismatch appears (Tactix size 56, Kissat 57); 62,627 then diverges
  fully. Mode-switch timing is therefore causal. Compare only search ticks,
  stable limit, anchor, and count at pre/post-analysis boundaries next.
- Campaign `4519`, job `1658576`, is that paired 112-conflict mode-clock
  window. Decisions, random draws, and watch rows are explicitly excluded;
  only the symmetric pre/post-analysis clock plus compact context is retained
  under a 1,000-row cap. It launches no proof work.
- `4519` is observer-invalid/clean: both arms reached the selected stop with
  zero stderr, but retained trajectory context exhausted the 1,000-row cap
  before the final clock row. Do not interpret its 222 partial clock rows per
  arm. Rerun the identical 112-conflict scope once with a 5,000-row cap; do not
  widen the event families.
- Corrected campaign `4520`, job `1658578`, repeats exactly `4519` with only
  the record cap raised to 5,000. The harness dependency and both immutable
  pins are hashed; semantic scope and event families are unchanged.
- `4520` completed valid/clean with 223 clock rows per arm and zero stderr. At
  conflict 62,516 both arms have the exact stable-mode limit 376,814,504, but
  Tactix search ticks are 376,491,049 versus Kissat's 376,491,477. That exact
  -428 delta remains constant through the common learned-clause prefix ending
  at conflict 62,625. Kissat consequently crosses and switches before conflict
  62,626; Tactix crosses later and switches at 62,627, after the first learned
  clause split. The `>=` predicate and limit construction match. Localize the
  earlier missing tick charge with paired narrow clock windows around the first
  two mode switches; do not open a full conflict or decision log.
- Campaign `4521` prepares those paired current-pin windows at conflicts
  995--1,005 and 1,885--1,900. Four independent tasks reuse `4520`'s validated
  observer and immutable binaries, cap each trace at 2,000 rows, and stop at
  the selected endpoint without proof work. Submitted as job `1658580`.
- `4521` completed 4/4 valid/clean with zero stderr. Both clocks, limits,
  conflict/decision/propagation counters, and learned clauses are exact through
  the conflict-1,001 focused-to-stable switch, the conflict-1,892 stable-to-
  focused switch, and conflict 1,900. The 428-tick deficit is not startup or
  first-cycle behavior; bracket its first occurrence coarsely between conflict
  1,900 and 62,516 before opening any per-operation tick stream.
- Campaign `4522` prepares paired five-conflict clock windows centered at
  conflicts 10,000, 20,000, 40,000, and 60,000. It reuses the same observer
  and pins, starts no proof work, and will select only the first unequal
  interval for refinement. Submitted as job `1658584`.
- `4522`'s completed conflict-10,000 pair is tick-exact. At conflict 19,998,
  Tactix is 7 ticks behind (267,289,411 versus 267,289,418), with identical
  decisions and identical local pre/post deltas. The first discrepancy is now
  bracketed to conflicts 10,002--19,998; split at the conflict-15,347
  Eliminate boundary and immediately before conflict-19,378 Factor while the
  later coarse arms finish independently.
- `4522` completed 8/8 valid/clean with zero stderr. Tactix-minus-Kissat tick
  deltas are 0 at conflict 10,000, -7 at 19,998, -375 at 39,998, and -387 at
  59,998. Thus the discrepancy begins before conflict 20,000 and accumulates
  primarily between 20,000 and 40,000; all sampled local analysis deltas and
  decisions remain exact.
- Campaign `4523` prepares paired windows spanning conflicts 15,344--15,350
  and 19,373--19,380. These are the exact Eliminate exit and Factor-entry
  lifecycle brackets; the same immutable observer stops each arm without
  proof work. Submitted as job `1658592`.
- `4523` is observer-invalid/clean overall: both Eliminate arms stop after
  conflict 15,349 because the native counter skips the requested 15,350 row,
  while both Factor arms are valid. Its common durable prefix is decisive.
  Ticks are exact through post-analysis conflict 15,345; Tactix is 7 behind
  before analyzing conflict 15,346 and remains exactly 7 behind through
  conflict 19,380. Local analysis charges are exact. The first missing charge
  occurs across Eliminate's sparse-resume lifecycle.
- A trace-only search-watch-cost filter now suppresses all startup/inprocessing
  scans and all per-entry payloads, retaining only one O(1) aggregate cost row
  per ordinary search scan. The Tactix executable regression, overlay static
  test, and a fresh generated Kissat overlay build pass.
- Campaign `4524` prepares the paired cost decomposition only for the ordinary
  search propagation after conflict 15,345 and before conflict 15,346. It caps
  the aggregate rows at 10,000 and starts no proof work. Submitted as job
  `1658596`.
- `4524` completed 2/2 valid/clean. Its 2,038 ordinary main-loop scan rows and
  every aggregate component are exact: 102,220 total ticks (base 6,497;
  binary assignments 1,134; large assignments 775; large dereferences 47,679;
  relocations 46,135). The physical main-search watch scan is exonerated.
  Source comparison finds the missing 7 in Kissat's distinct Eliminate sparse-
  resume: after reconnect it resets to the simplifier-produced root suffix and
  invokes `kissat_search_propagate`, charging `search_ticks`. Tactix rebuilt
  watches but omitted that reset/replay. The general repair now retains the
  immutable old-root prefix, replays only the phase-produced suffix, and uses
  search tick currency; focused scheduling/event tests and overlay builds pass.
- Campaign `4525` prepares two changed-arm clock validations: the immediate
  conflicts 15,344--15,349 boundary and the downstream conflicts
  62,516--62,627 switch. Immutable Kissat `4523`/`4520` traces are reused; no
  comparator or proof work is duplicated. Submitted as job `1658598`.
- `4525` completed 2/2 valid/clean. The repair restores the exact missing
  seven ticks before conflict 15,346 and remains learned-clause exact through
  conflict 62,625. It also restores the stable-to-focused switch to the same
  conflict boundary, but Tactix still enters conflict 62,516 at 376,491,285
  ticks versus Kissat's 376,491,477: a residual -192. Thus sparse resume
  repairs 236/428 missing ticks and is retained, while the next coarse bracket
  targets the conflict-29,694 Eliminate episode where the prior deficit grew.
- Campaign `4526` prepares one changed-arm confirmation at conflicts
  19,996--20,002 and a paired conflicts-29,690--29,698 clock bracket around
  the later Eliminate. It retains only trajectory and mode-clock rows, caps
  each task at 2,000 records, and launches no proof work. Submitted as job
  `1658600`.
- `4526` completed 3/3 valid/clean. Repaired Tactix is tick-exact at conflict
  19,998 (267,289,418). By conflict 29,690 it is already 173 ticks behind;
  the conflict-29,694 Eliminate adds only five more missing ticks, yielding
  -178 before conflict 29,695. The dominant residue therefore first appears
  in the free-search interval 20,002--29,690, not that later Eliminate.
- Campaign `4527` prepares one paired four-conflict mode-clock midpoint at
  conflict 25,000 to bisect that free-search interval without opening a full
  conflict log. It launches no proof or model work. Submitted as job
  `1658603`.
- `4527` completed 2/2 valid/clean. At conflict 24,998 Tactix has 278,974,430
  search ticks versus Kissat's 278,974,575, a -145 delta. Thus 145/173 of the
  pre-Eliminate residue accumulates in conflicts 20,002--24,998. Retain coarse
  sampling at 21k/22k/23k/24k before opening operation-level logging.
- Campaign `4528` prepares those four paired four-conflict clock windows with
  1,000-row caps and no other detailed event family. It starts no proof or
  model work. Submitted as job `1658605`.
- `4528` completed 8/8 valid/clean. Tactix-minus-Kissat tick deltas at
  conflicts 20,998/21,998/22,998/23,998/24,998 are respectively
  -22/-68/-96/-117/-145. This is gradual search-work drift, not one missed
  macro transition. The mode-clock observer now reports cumulative ordinary
  propagation ticks and the residual conflict-analysis ticks at the same O(1)
  checkpoint. It is trace-only in Tactix; Kissat's overlay accumulates the
  native `kissat_search_propagate` deltas. Focused/static tests and a fresh
  overlay build pass.
- Campaign `4529` prepares one paired conflict-24,998 clock checkpoint using
  the cumulative propagation/analysis decomposition. It retains the same
  event frequency and starts no proof or model work. Submitted as job
  `1658613`.
- `4529` completed 2/2 valid/clean. At conflict 24,998 both arms have exactly
  412,459 cumulative conflict-analysis ticks. Ordinary propagation is
  278,561,971 for Tactix versus 278,562,116 for Kissat: the entire -145 total
  is physical search propagation. Conflict analysis is exonerated; narrow the
  first watch-scan/relocation cost split with sparse 20,250/20,500/20,750
  checkpoints before enabling per-scan decomposition.
- Campaign `4530` prepares paired component-bearing clock checkpoints at
  conflicts 20,250/20,500/20,750. Each arm retains only four conflicts and no
  per-scan events, proof, or model work. Submitted as job `1658615`.
- `4530` is observer-invalid/clean overall because both conflict-20,250 arms
  stop at a native conflict-counter jump before the requested endpoint. Their
  durable common prefix is exact through pre-analysis conflict 20,250. The
  propagation delta is -5 by conflict 20,498 and -12 by 20,748. The first
  physical cost split is therefore inside conflicts 20,250--20,498; a bounded
  paired clock stream over only that interval is now justified.
- Campaign `4531` prepares that paired conflicts-20,245--20,500 component
  clock stream. It caps retained trajectory/clock rows at 10,000 and keeps
  per-scan watches, decisions, random draws, proofs, and models disabled.
  Submitted as job `1658621`.
- `4531` completed 2/2 valid/clean with 366 clock rows per arm. Cumulative
  propagation and analysis are exact through post-analysis conflict 20,338
  (267,032,250 / 354,114). Before analyzing conflict 20,339, Tactix/Kissat
  propagation is 267,032,288/267,032,289 while analysis remains exact. Thus
  the single ordinary propagation episode producing conflict 20,339 costs
  38/39 ticks. Apply the validated aggregate watch-cost observer only there.
- Campaign `4532` prepares that single paired conflicts-20,338--20,339
  aggregate watch-cost episode. It emits one O(1) component row per search
  literal scan, caps at 5,000, and starts no proof or model work. Submitted as
  job `1658623`.
- `4532` completed 2/2 valid/clean with 14 aggregate scans per arm. All cost
  components match except the final propagated literal `-483432`: Tactix/
  Kissat enters with 11/13 words, exits with 9/9, dereferences 4/5 large
  clauses, and charges 9/10 ticks. Kissat therefore carries one additional
  two-word large watch into the scan and discards it there; Tactix lacks that
  pre-scan physical entry. Record only this literal's pre/post watch entries
  and clause pose next.
- Campaign `4533` prepares that selected `-483432` pre/post watch-list
  capture at conflict 20,338, including only clause pose and values. Both arms
  exit immediately after the selected post-scan summary; no solver verdict,
  proof, or model is produced. Submitted as job `1658625`.
- `4533` is observer-invalid/clean. Tactix reached the selected scan and
  retained six pre-scan and five post-scan entries, but its 838 rows included
  unrelated snapshots. Kissat filled the 1,000-row cap with those snapshots
  before the target and timed out. Neither arm writes stderr or leaves scratch
  residue. The target Tactix rows remain descriptive only; no paired lifecycle
  claim is taken. A tested strict watch-only observer now rejects snapshots
  before payload construction while preserving the established broader
  watch-only mode. Rebuild both immutable trace pins and rerun the pair.
- Campaign `4534` is that strict replacement, submitted full-width as job
  `1658627` with fresh immutable Tactix/Kissat trace pins. It retains at most
  100 selected watch rows, exits at the conflict-20,338 post-scan summary, and
  starts no verdict, proof, or model work.
- `4534` completed 2/2 valid/clean with 12/13 rows and zero stderr or residue.
  Both arms enter with the same six common entries and leave the same five.
  Kissat alone enters with irredundant-looking four-literal clause
  `(-396808,36755,37046,483432)`, pose
  `(483432,36755,-396808,37046)`, `searched=3`, then discards its two-word
  watch. This is exactly the extra dereference and tick from `4532`. Determine
  whether Tactix owns that live clause under another pose or retired/rewrote it
  before changing production lifecycle behavior.
- Campaign `4535`, job `1658629`, now tests that distinction directly. At
  post-analysis conflict 20,338 it dumps only live clauses touching the four
  target variables and stops before more search; no proof/model work begins.
- `4535` completed 2/2 valid/clean. The 48 selected live rows are semantically
  exact between arms, and neither contains the target clause. Thus Kissat's
  extra pre-scan entry is a stale watch to an already-garbage clause, not a
  live formula or pose difference. Determine whether Tactix already collected
  that garbage record or explicitly unwatched it, then locate the marking/
  collection boundary at coarse macro granularity.
- Campaign `4536`, job `1658631`, is the direct arena-record discriminator.
  A focused observer regression proves selected formula snapshots can include
  garbage records without altering default snapshots. Both arms stop at the
  same post-analysis conflict-20,338 state and start no certificate work.
- `4536` completed 2/2 valid/clean. Tactix has no selected garbage record;
  Kissat retains three selected irredundant garbage records, including the
  exact four-literal stale watch source. The coarse trace reveals a Reduce at
  conflict 20,332. Campaign `4537`, job `1658633`, captures selected live-plus-
  garbage arena state on both sides of that Reduce with four full-width tasks.
- `4537` is observer-invalid/clean: its harness mistakenly set solver-prefixed
  target-only variables instead of shared `SAT_EVENT_TRACE_TARGET_ONLY`, so
  startup snapshots marked all four traces truncated at five rows. Every
  terminal formula artifact exists and descriptively has the selected garbage
  clause in both begin arms, only Kissat in the end arms, but that paired claim
  is withheld until the corrected replacement.
- Corrected campaign `4538` uses the shared target-only contract and was
  submitted full-width as job `1658669` with the same immutable observer pins.
- `4538` completed 4/4 valid/clean with zero stderr or residue. The exact target
  garbage clause is present in both conflict-20,332 Reduce-begin snapshots. It
  survives Kissat's Reduce but is absent at Tactix Reduce-end. This proves that
  sparse collection is the deletion boundary. The general discrepancy is that
  Tactix packs all garbage out of its logical prefix and drops those watches,
  while Kissat leaves pre-watermark arena records and non-root-fixed large
  watches physically intact. Repair that lifecycle without retaining Tactix's
  append-only, implementation-only replacement sources.
- Implemented that general sparse-prefix repair. Focused collection tests pass
  for both genuine garbage retention and a live append-replacement shadowing
  its implementation-only source. Trace-enabled observer tests also pass.
  Campaign `4539`, job `1658673`, now validates the causal Reduce-end record
  and selected conflict-20,338 watch scan in two full-width tasks; neither task
  starts proof/model work.
- `4539` completed 2/2 valid/clean with zero stderr or residue. Tactix now
  retains the exact garbage record at Reduce-end. Six conflicts later its
  selected list has the same seven pre-scan and five post-scan entries as
  Kissat, in the same semantic order with exact pose, blocker, values, and
  `searched=3` target cursor. The causal missing watch and first search-tick
  discrepancy are closed. Advance the coarse mode-clock comparison before
  requesting any broader trace.
- Campaign `4540`, job `1658675`, is that coarse changed-arm comparison at the
  first repaired conflict, conflict 24,998, and the conflict-62,516 mode-switch
  checkpoint. It reuses immutable Kissat authorities, disables per-watch detail,
  and starts no proof/model work.
- `4540` completed 3/3 valid/clean. The clock is exact through conflict 20,341.
  At conflict 24,998 Tactix is four propagation ticks ahead with exact analysis
  currency. At conflict 62,516 the prior 192-tick deficit is only one tick;
  learned clauses, decisions, levels, and the stable-to-focused transition
  remain aligned, with both arms switching before conflict 62,626. Bank this
  near-exact trajectory repair with fresh Release Rank-27 timing, then fuzz.
- Release collection/analyze/scheduling regressions pass. Campaign `4541`, job
  `1658678`, runs the uniform Release Rank-27 candidate and publishes its solver
  result before starting `drat-trim`. Proof checking is nonblocking and will be
  inspected later; work proceeds immediately from the provisional timing.
- `4541` provisionally returns expected UNSAT in 820.854 s at 14,246,887
  conflicts and 221,715,762 decisions. That is 1.141914x the 718.838 s
  predecessor and 3.156973x pinned Kissat's 260.013 s; proof checking is still
  running and is not awaited. Thus the general repair closes the measured early
  clock discrepancy but exposes a later, worse trajectory. Bank it with fuzzing
  now, then resume coarse localization after the exact conflict-62,627 window.
- The project-mandated Claude-owned fuzz launch was rejected by the environment's
  external-disclosure safeguard because it would send private repository and
  diagnostic details to the Claude API. Do not work around that safeguard or
  self-run the campaign; explicit user authorization is required. Because the
  Rank-27 gap remains open, defer this fuzz checkpoint and continue coarse
  post-62,627 trajectory localization without touching proof job `1658678`.
- Campaign `4542`, job `1658679`, reuses the strict O(1) macro observer to stop
  current Tactix at the next Probe begin after the exact 62,625 switch window.
  It will be compared with immutable Kissat `4495`; no detailed search rows,
  proof, model, or solver verdict are produced.
- `4542` completed valid/clean in 68.586 s. Every major conflict/decision
  boundary is exact to Kissat through Probe entry: switches at 62,625 and
  64,977, reductions at 62,709 and 67,292, and Probe at conflict 68,537 /
  decision 20,221,397. The next inferior divergence can be in Probe itself or
  later; compare the Probe-end durable state before returning to CDCL detail.
- Campaign `4543`, job `1658680`, runs Probe with only the O(1) macro journal
  and performs one arena count at selected `probe_end`. Its Tactix active/live
  irredundant/redundant/binary tuple will be compared with immutable Kissat
  `132807/563942/12084/168182`; no verdict or certificate work begins.
- `4543` completed valid/clean in 71.991 s. Post-Probe live formula counts are
  exact at 563,942 irredundant, 12,084 redundant, and 168,182 binary clauses.
  Tactix alone retains one extra active identity: 132,808 versus Kissat's
  132,807. Probe is therefore the first demonstrated inferior owner after the
  repaired control-exact prefix. Identify the variable with a paired one-shot
  heap set, then locate its rule-local retirement boundary.
- Campaign `4544`, job `1658781`, captures both complete stable heaps at exact
  post-Probe conflict 68,537 / decision 20,221,397 and terminates immediately.
  The Kissat overlay now honors the existing symmetric heap-set stop contract;
  its static test and a fresh overlay build pass. No proof/model work begins.
- `4544` completed 2/2 valid/clean. Both heaps contain the same 119,264
  variables in exactly the same positions, assignments, and activity states.
  Only external variable 77,069 differs: at common position 1 its Tactix score
  is `1.4756718151936151e115` versus Kissat's `1.8189980438567545e115`. Thus
  the extra active-count identity is not a heap/topology owner; the one inferior
  search-visible Probe divergence is a missed score bump. Localize its first
  Probe rule-boundary split next.
- Rebuilt Release after the sparse-resume repair. All 75 code, proof, N1--N4,
  overlay, citation, and hygiene tests pass; the sole remaining suite failure
  is the already-separated `PROGRESS.md` size cleanup (1,496 versus 350
  lines). A newly exposed observer-comment hygiene finding was corrected and
  its focused Release test rerun successfully.
- The independent `4515` proof checker finished without being awaited:
  `drat-trim` verified the 3,229,209,356-byte Rank-27 UNSAT certificate in
  3,117.165 s with exit code 0. The proof and derived artifacts were deleted
  cleanly with zero residue; provisional timing interpretation is unchanged.
- Campaign `4545` is prepared with fresh immutable trace pins to journal only
  external variable 77,069's score at conflict-68,537 Probe and named rule
  boundaries. Both solvers compile and the focused executable/static observer
  tests pass. It exits at Probe end, starts no proof/model work, and separates
  an in-Probe score split from one inherited from the control-exact CDCL prefix.
  Submitted full-width as Slurm job `1658862`; unrelated bash job `1657786`
  remains untouched and no proof checker is part of this campaign.
- `4545` completed 2/2 valid/clean with 62 selected score rows per arm. External
  77,069 has exactly the same score, heap position, assignment, and activity at
  every paired Probe boundary: `1.8189980438567545e115` at both `probe_begin`
  and `probe_end`. Probe is exonerated for the earlier heap-set score split.
  Reproduce both endpoints in one current Tactix run and narrow only the
  post-`probe_end` handoff before returning to wider CDCL or active-state work.
- Submitted single-arm campaign `4546` as Slurm job `1658864`. The same current
  immutable Tactix process records the selected `probe_end` score and then the
  complete first heap endpoint at the unchanged conflict/decision counters.
  This is the smallest reproducer for a real handoff mutation versus the older
  heap observation being source/observer dependent; no proof work is opened.
- `4546` is observer-invalid/clean but resolves the ambiguity: the complete
  119,264-row heap fires before any `probe_begin`/`probe_end` boundary row at
  the same counters, then exits 0 with zero stderr. Thus `4544` sampled
  incomparable intermediate analysis callbacks and its single-score
  attribution is withdrawn. Explicit-boundary `4545` is authoritative and
  exact. Resume the genuine inferior inprocessing finding by identifying
  Tactix's one extra active identity at true `probe_end`.
- Focused active-set observer coverage passes in Tactix and the static Kissat
  overlay test; both fresh trace pins compile. Submitted paired campaign `4547`
  full-width as Slurm job `1658865`. It dumps only the true `probe_end` active
  external set and exits, with no global trace or proof/model work.
- `4547` is observer-invalid/clean overall. Kissat emits a valid 132,807-row
  active set. Tactix emits 133,671 non-retired rows against declared active
  132,808: the exact 863-row excess is its retained level-zero trail. The
  observer now applies the established non-retired-and-not-root-fixed active
  predicate, with a root-fixed regression. Rerun only Tactix and reuse the
  immutable valid Kissat set; no solver conclusion is taken from the bad dump.
- Submitted corrected Tactix-only active-set endpoint `4548` as Slurm job
  `1658867`, reusing immutable valid Kissat `4547`. It opens no global trace or
  proof work and will name the exact one-identity post-Probe difference.
- `4548` completed valid/clean in 72.936 s. Corrected Tactix has 132,808 unique
  active externals; immutable Kissat `4547` has 132,807. All 132,807 Kissat
  identities are common and Tactix alone retains extension variable 494,144.
  Trace only that variable's first present/active split across named Probe
  boundaries; Factor's fresh-variable lifecycle is the leading owner.
- Submitted paired selected-extension boundary campaign `4549` full-width as
  Slurm job `1658868`. It records only external 494,144's value, active bit,
  score, and heap position at true Probe/rule boundaries, then exits at
  `probe_end`; no global trace or proof/model work is opened.
- `4549` completed 2/2 valid/clean. Kissat creates external extension 494,144
  at the first `factor_applied`, then early Backbone in round two fixes it true
  and removes it from the heap. Tactix first creates the same external identity
  only at round-two `Factor_applied`, where it remains unassigned and active.
  Thus the upstream inferior owner is Factor fresh creation/order, not a missed
  Backbone retirement of an already-common extension. Compare coarse per-rule
  formula/active scalars next before tracing any Factor candidate stream.
- Submitted Tactix-only coarse rule-scalar campaign `4550` as Slurm job
  `1658870`, reusing immutable Kissat `4545`. It emits only the 36 inner
  begin/end rows for the two Probe rounds and exits at outer `probe_end`.
- `4550` completed valid/clean in 72.842 s with all 36 rows. The first scalar
  difference is one fewer Tactix redundant clause after Vivify, while active,
  irredundant, and binary inputs stay exact through first Factor entry at
  `131970/569522/170159`. First Factor then exits Tactix/Kissat at active
  132,387/132,394, irredundant 567,039/566,987, and binary 167,833/167,843;
  Tactix performed materially less factoring from the same consumed state.
  Compare the bounded fresh-event stream and locate its first differing grid;
  do not attribute this to the inherited redundant-only Vivify difference.
- Focused observer tests and fresh pins pass for a conflict-selected
  `factor_fresh` stream. Submitted paired campaign `4551` full-width as Slurm
  job `1658871`; it retains at most the roughly 424 first-round accepted grids
  and exits at the first Factor end, with no proof/model work.
- `4551` is observer-invalid/clean overall. Kissat reaches a valid first-Factor
  terminal with 424 fresh rows in 172.365 s. Tactix's fresh-only gate rejected
  its selected slot-boundary endpoint, continued into round two, and timed out
  at 300.021 s; its 839 partial rows are uninterpreted. The general slot
  endpoint now bypasses detail-mode rejection when explicitly matched, with an
  executable regression. Rerun only Tactix against immutable Kissat `4551`.
- Submitted repaired Tactix-only endpoint `4552` as Slurm job `1658873`, using
  a fresh pin and hash-verifying the `4551` harness. Immutable Kissat `4551`
  remains the 424-row authority; no comparator or proof work is duplicated.
- `4552` completed valid/clean in 97.572 s with 417 first-Factor fresh rows.
  The first 87 accepted grids exactly match Kissat through ordinal 5,223
  (fresh 493,813, seed 10,238, members `10238,-10133`). At ordinal 5,224,
  Tactix chooses seed 51,306 while Kissat chooses -51,180; the two grids then
  appear in reverse order at 5,225. Compare only the mutable schedule update
  and heap after exact ordinal 5,223; a full candidate stream is unnecessary.
- Submitted paired update/heap campaign `4553` full-width as Slurm job
  `1658874`. It selects only exact accepted Factor ordinal 5,223, records the
  update transaction and competing seeds 51,306/-51,180, dumps the resulting
  heap once, and exits at common fresh ordinal 5,224; no proof work begins.
- `4553` is observer-invalid/clean overall. Kissat reaches the selected
  transaction and heap in 178.965 s. Tactix's heap-digest helper ignored the
  ordinal selector, filled the 500-row cap with earlier digests, and timed out
  at 300.021 s; its partial artifacts are uninterpreted. The digest now honors
  `SAT_EVENT_TRACE_FACTOR_HEAP_ORDINAL`, with static coverage. Rerun only
  Tactix against immutable valid Kissat `4553` without widening the cap.
- Submitted corrected Tactix-only campaign `4554` full-width as Slurm job
  `1658876`. It hash-verifies and reuses the `4553` harness and immutable
  valid Kissat trace, while the fresh Tactix pin makes the Factor heap digest
  honor the selected ordinal. The run remains bounded at 300 seconds and
  proof-free because it terminates at the diagnostic endpoint.
- `4554` is valid/clean: 198.180 seconds, 46 selected update rows, complete
  151,837-row heap dump, zero stderr, and zero residue. Every post-grid update
  literal, ordinal, and score matches immutable Kissat `4553` exactly. The
  resulting heaps do not: Tactix has 151,837 members and pops `51306`, while
  Kissat has 150,755 and pops the equal-score `-51180` (both score 166). Set
  comparison finds 150,753 common literals, 1,084 Tactix-only literals, two
  Kissat-only literals, and zero score mismatches on the common set. Thus the
  ordinal-5224 split is not a scoring/update discrepancy; it is inherited
  schedule membership plus heap topology. Next compare the initial Factor
  schedule before any accepted grid, then trace the first inferior persistent
  factor-mark transition rather than logging the whole rule.
- Submitted paired initial-schedule campaign `4555` full-width as Slurm job
  `1658877`. Both immutable pins stop on one Factor heap record immediately
  after the common first-Factor schedule is built (ordinal zero), with a
  180-second solver cap and no proof/model artifact. This coarse boundary
  decides whether the 1,082-net member surplus predates all accepted grids.
- `4555` is observer-invalid/clean: both arms emitted the selected ordinal-zero
  heap at the earlier conflict-19,360 Factor call, where size 294,449 and both
  selected positions/scores are exact, then correctly refused to terminate
  before the conflict-68,537 exit floor and timed out at 180 seconds. This is
  neutral partial evidence only. The target Factor call begins with cumulative
  `factored=5136` (its first accepted grid is ordinal 5137); rerun that exact
  initial ordinal without changing the cap or interpreting the partial files.
- Submitted corrected paired campaign `4556` full-width as Slurm job
  `1658879`, selecting cumulative Factor ordinal 5136 at conflict 68,537. It
  retains the 180-second cap and one-record endpoint; no proof/model work is
  produced or awaited.
- `4556` is observer-invalid/clean. Cumulative ordinal 5136 also labels the
  preceding conflict-48,432 grid's post-update heap. Kissat later reached the
  intended conflict-68,537 initial row (size 151,269; `51306` position 259 and
  `-51180` position 521, both score 166), but the composite exit selector did
  not match fields separated inside the payload; Tactix spent the 180-second
  cap repeatedly externalizing selected literals before its target. Do not
  pair the partial rows. Repair the disabled observer by rejecting nonselected
  ordinals before its O(n) literal scan, use payload match `stage=initial`, and
  validate only the selected conflict's initial row.
- Submitted observer-fixed paired campaign `4557` full-width as Slurm job
  `1658881`. The fresh Tactix pin rejects nonselected Factor ordinals before
  externalizing literals; both arms now exit on the simple `stage=initial`
  payload match at conflict 68,537, and validation admits only that row. The
  180-second cap remains unchanged.
- `4557` is valid/clean: Tactix 98.627 seconds and Kissat 173.693 seconds,
  exactly one target initial row per arm, zero stderr, and zero residue. Before
  the conflict-68,537 Factor call accepts any grid, Tactix already schedules
  152,356 literals versus Kissat's 151,269, a net surplus of 1,087. Both
  competing literals have score 166; Tactix positions `51306/-51180` at
  1046/522 while Kissat positions them at 259/521. Since the post-ordinal-5223
  net surplus is 1,082, the primary membership/topology split is inherited
  from persistent pre-Factor eligibility, not created by this call's matched
  grid updates. Dump only this initial heap set next, then inspect selected
  differing literals' factor marks and degrees.
- Submitted exact initial-set campaign `4558` full-width as Slurm job
  `1658883`. It adds only a stage selector to the disabled heap-set observer,
  dumps the two complete target initial heaps once, and exits at the same
  conflict-68,537 boundary under the unchanged 180-second cap.
- `4558` is valid/clean: Tactix 99.018 seconds and Kissat 176.790 seconds,
  complete 152,356/151,269-row dumps, zero stderr, and zero residue. The exact
  initial sets contain 151,267 common literals, 1,089 Tactix-only literals,
  and two Kissat-only literals (`39220`, `84146`); all common scores match.
  The Tactix-only population includes high-degree witness `-213255` (score
  404), typical low-degree witness `-479842` (score 5), and the large score-two
  family. Add a target-conflict filter to the existing selected literal-state
  observer, then compare marks/active/value/degree for those witnesses and a
  Kissat-only control before tracing mark provenance.
- Submitted selected-state campaign `4559` full-width as Slurm job `1658885`
  (eight independent arm/witness tasks). Each task suppresses non-target
  conflicts and exits on one `factor_literal_state` row; the 180-second cap is
  unchanged and no proof/model artifact is produced.
- `4559` is 8/8 valid/clean. Every witness is active, unassigned, and has the
  same dense score in both solvers. Only the persistent mark differs:
  Tactix/Kissat is 1/0 for `-213255` (score 404), `-479842` (5), and `-230203`
  (2), while Kissat-only control `39220` (4) is 0/1. This proves the initial
  heap membership split is exactly a persistent Factor-mark lifecycle
  discrepancy, not activity, assignment, or watch-degree representation.
  Extend the selected-only observer to retain mark transitions outside the
  detail window, then compare set/clear sources for the high-degree witness
  and Kissat-only control through the target state.
- Submitted selected mark-lifecycle campaign `4560` full-width as Slurm job
  `1658893` (six arm/witness tasks). The selected-only mode now retains the
  chosen literal's mark transitions even outside a detail window and exits on
  its single target state; no general event stream or proof work is enabled.
- `4560` is 6/6 harness-valid/clean but paired-history-incomplete because the
  Kissat overlay recorded mark sets but not Factor-pop clears; that missing
  read-only hook is now added. Tactix's selected histories are complete and
  expose the concrete re-arms: high-degree `-213255` is cleared at the
  conflict-48,432 Factor pop then set by `root_flush` at conflict 68,537;
  score-two `-230203` is set there by `add_binary`; control `39220` is cleared
  at the first Factor and never re-armed. Source audit identifies the general
  mismatch: Kissat sparse collection shortens a large clause in place or
  converts it to a watched binary without `mark_added_literal`, while Tactix
  routed those physical rewrites through ordinary added-clause marking.
- Repaired the production mechanism generally: `kernel_replace_clause` and
  binary publication now carry an explicit `mark_added_material` semantic;
  root flush passes false for both large-to-large and large-to-binary rewrites.
  Ordinary semantic additions retain the default true behavior. Focused
  kernel, Factor, and event-trace tests pass, including new regressions that
  clear surviving Factor marks before each root-flush shape and assert they
  remain clear. No formula/benchmark/rank/size/density/order gate is involved.
- Submitted repaired Tactix initial-set closure campaign `4561` full-width as
  Slurm job `1658899`. It hash-verifies and reuses the immutable valid `4558`
  harness/reference, runs one fresh repair pin, and stops at the target initial
  Factor set under the unchanged 180-second cap.
- `4561` is valid/clean in 99.368 seconds with a complete 151,594-row repaired
  Tactix set, zero stderr, and zero residue. Against immutable Kissat `4558`,
  the difference contracts from 1,089 Tactix-only / two Kissat-only literals
  to 334/9; 151,260 members are common and all common scores match. This closes
  755 of the original 1,087 net excess candidates and removes the high-degree
  root-flush witness. Bank Release performance before localizing the smaller
  residual mark lifecycle.
- Submitted checked Rank-27 Release campaign `4562` as Slurm job `1658900`
  using one uniform event-trace-disabled Release pin. Consume its provisional
  UNSAT timing/result as soon as the solver finishes; independent DRAT checking
  continues asynchronously and must not block the investigation.
- `4562` provisionally returns expected UNSAT in 658.478 seconds with 9,560,680
  conflicts, 236,590,373 decisions, and 713,136 KiB peak RSS. Versus `4541`,
  time improves 19.781% and conflicts 32.893%; versus pinned Kissat it remains
  2.532481x time and 2.320164x conflicts. Retain the general repair and bank
  this large trajectory gain with the authorized bounded consolidation fuzz
  before continuing the 334/9 residual mark lifecycle. Independent DRAT
  checking is pending nonblocking and is neither polled nor awaited.

- Release SHA `5743b0a...03698`, homogeneous job `1657639`, provisionally solves
  40/40 in 10,290.269 s: 0.973186x preceding, 0.916900x accepted, and 1.340710x
  Kissat; geometric-mean ratios are 0.973948x, 0.930427x, and 1.330300x.
- SAT/UNSAT totals are 4,520.427/5,769.842 s. All 22 SAT models and all 18 UNSAT
  proofs are independently verified, with no invalidity or residue. Full table/revisit: campaign
  `3760` `comparison-live.md` and `2026-08-31-fixed-active-rank1-40-codebase-revisit.md`.
- Jobs `1657634`/`1657636` prove Rank 17's old 944,336 active count included
  616,255 retained roots; the repair changes only that field to 328,081 at
  first BVE entry. Rank 17 improves 449.150→314.130 s, Rank 19 452.403→166.486,
  Rank 31 628.742→461.093, and Rank 34 807.219→692.338.
- Rank 33 regresses 395.771→753.767 s; retain the general repair and localize
  its downstream trajectory. Rank 36 remains closed at 385.209 s versus
  Kissat's 368.510 s. Peak RSS remains Rank 10 at 16,987,860 KiB.

## Rank-33 current localization

- At BVE conflict 15,347 both solvers generate ternary
  `(-237373,-237365,-145)` from the same 2x8 transaction. Valid jobs `4140` and
  `4170` prove both forward passes retain it; invalid observer jobs are
  classified clean and make no claim.
- Job `4210` follows the same clause through conflict-17,312 relocation and to
  Probe entry with exact decisions, heap prefix, formula counts, pose, and
  metadata. Job `4220` proves every Probe subrule retains it and confines the
  score 2-versus-1 split to Factor's dense candidate construction.
- Source comparison found Tactix omitted Kissat's separate final survivor
  recheck against the last `factorcandrounds` count rebuild. The uniform recheck
  and a three-layer peel regression are implemented. Causal job `1657806`
  (`4240`) now matches `-237365` active=1, marked=1, value=0, degree=1 in both
  solvers. Job `4230` is invalid/clean and contributes no paired claim.
- Proceed to Release, Rank-33, and Tier-B+C validation. Do not restore a profile
  or add a formula/rank selector. Release passes 76/76; Rank-33 job `1657808`
  is valid and clean at 468.533 s, with its 2.137 GB DRAT independently verified.
  Fixed
  B28+C5 job `1657809` and homogeneous Rank-1--40 job `1657810` are running
  from the same immutable Release pin. B28+C5 has all 66 expected solver
  verdicts and checked artifacts at 2,010.962/2,444.835 s candidate/shipped
  (0.822535x PAR-2): all 66 rows are valid and clean, so the gate passes.
  Rank 1--40 has all 40 expected solver verdicts in
  10,848.684 s: 1.054266x preceding, 0.966657x accepted, and 1.413466x Kissat;
  all 22 SAT models and 18 UNSAT proofs verify, and all 40 rows are clean.

## Rank-25 current localization

- Rank 25 regresses 270.955→760.379 s with exact search/BVE work but BVE time 13,803→509,433 ms over the same 3,296,386 applications: an implementation-unit-cost regression.
- Matched campaign `20260831-4290-tierd-rank25-bve-unit-cost`, job `1657922`, is valid/clean: previous 263.337 s / 13,779 ms BVE versus current 755.590 s / 507,738 ms, with exact work and verified models.
- Streaming cleanup passes three waves at median 0.348100x current; Rank33 and B+C are checked/clean (B+C 0.996294x shipped), while homogeneous `1658000` at 10,266.505 s has 6 proofs continuing asynchronously.

## Rank-17 inferior inprocessing state

- Rank 17 is the first-priority known open inferior inprocessing-state
  divergence.  Historical current-lineage evidence shows Tactix entering the
  first Eliminate/BVE schedule with 1,910,024 irredundant clauses and 66,106
  physical binaries versus Kissat's 1,699,758 and 254,564.  The bound-0
  schedules have a 11,550-variable symmetric difference, and 72,684 common
  variables have different physical occurrence pairs.  The divergence is
  upstream of BVE's per-candidate lifecycle and remains unclosed.
- The latest homogeneous candidate still solves Rank 17 materially slower:
  job `1658170` takes 237.447 s, 181,396 conflicts, and 2,418,077 decisions
  versus pinned Kissat's 121.813 s, 186,237 conflicts, and 3,961,527
  decisions.  This is primarily an inprocessing/state-quality gap rather than
  excess CDCL conflicts.
- Compact current-pin reconfirmation
  `20260831-4404-tierd-rank17-current-bve-entry`, job `1658217`, records one
  first-schedule summary per arm and exits before the first BVE candidate.  It
  is valid/clean and reproduces every historical summary field and hash exactly;
  the split survives all intervening repairs. Boundary-only job `1658219` was
  canceled and is observer-invalid because opening the global trace remained
  expensive; neither arm reached first BVE. Its completed preprocessing
  endpoint is valid paired evidence: both arms have active/unassigned 695,090,
  3,719,727 irredundant, zero redundant, 272,143 binary clauses, and the same
  ranked top-32. Thus the inferior formula/schedule split first develops after
  preprocessing. The replacement is a standalone boundary journal paired with
  the existing pre-candidate BVE exit; it does not open the global trace.
- Replacement campaign
  `20260901-4406-tierd-rank17-standalone-boundaries`, job `1658222`, is now
  running on immutable rebuilt pins. It records only begin/end inprocessing
  state after conflict zero, emits no global event trace, and intentionally
  exits at the exact first-BVE pre-candidate schedule. Focused Trace tests pass;
  the two tasks are unthrottled and have no node constraint.
- Job `1658222` completed valid/clean in 44.070/32.587 seconds. At conflict
  3,654 the first Probe enters with exact semantic counts, and first
  Congruence ends exact. Early Substitute is the first count split: Tactix has
  198,235 binaries versus Kissat's 198,233 while active, irredundant, and
  redundant counts remain exact. The first clearly inferior transition is the
  second Backbone application: both enter with 524,620 active variables, then
  Kissat fixes 597 while Tactix fixes only 429, leaving Tactix 168 behind.
  Localize the two extra Substitute binaries first because they precede and may
  cause the Backbone deficit; do not yet attribute the deficit to Backbone.
- Campaign `20260901-4407-tierd-rank17-first-substitute-binaries`, job
  `1658225`, now dumps one sorted physical binary multiset per arm at the first
  conflict-3,654 Substitute end. It retains the standalone first-BVE exit and
  never opens the global trace. The expected 198,235/198,233 rows will expose
  exact pair multiplicity rather than infer it from aggregate counts.
- Job `1658225` is observer-invalid overall: both solver arms reached the exact
  endpoint and cleaned scratch, but conflict 3,654 contains multiple Probe
  rounds, so the selector dumped every Substitute end rather than exactly one.
  Its intended snapshot zero is retained only as steering: it suggests three
  Tactix-only pairs `(-1051309,525827)`, `(-525827,1051309)`, and
  `(1051309,1051651)` versus one Kissat-only pair `(525827,1051651)`. A tested
  one-shot selector must reproduce this four-row multiset difference before it
  becomes evidence.
- Corrected one-shot campaign
  `20260901-4408-tierd-rank17-first-substitute-binaries-once`, job `1658227`,
  is running on rebuilt immutable pins. The snapshot helper now stops after its
  first selected boundary in both solvers; focused Trace tests pass.
- Job `1658227` completed valid/clean in 44.159/32.520 seconds and reproduced
  exactly four differing rows. Tactix-only binaries are
  `(-1051309,525827)`, `(-525827,1051309)`, and `(1051309,1051651)`;
  Kissat-only is `(525827,1051651)`. The net +2 count and edge shape identify
  an incomplete equivalence/substitution collapse through external 1,051,309
  as the earliest semantic content frontier. Trace that variable's map and
  rewrite lifecycle from the exact post-Congruence input.
- Selected-map campaign `20260901-4409-tierd-rank17-first-substitute-map`, job
  `1658229`, is running. It records only the six signed map rows for variables
  525,827, 1,051,309, and 1,051,651 in the first conflict-3,654 Substitute
  invocation, while retaining the validated one-shot binary and BVE bounds.
- Job `1658229` completed valid/clean in 44.229/33.174 seconds. The maps are
  identical: ±1,051,309 maps to ±525,827, while 525,827 and 1,051,651 are self
  representatives. SCC orientation is excluded. The state before rewrite is
  not: Kissat already has 525,827 and 1,051,309 inactive with their positive
  literals fixed true; Tactix still has both active/unassigned and applies its
  failed units only after map emission. The owner is root-closure timing and
  the post-SCC failed-unit/live-substitution interaction.
- General repair: canonical Substitute now keeps the frozen SCC map distinct
  from the still-live retirement/lemma set. Fixed mid-round members remain map
  sources for physical binary and large-clause rewriting, exactly as Kissat's
  `substitute_binaries`/`substitute_clauses`, but receive no equivalence lemma,
  retirement, or reconstruction witness. A focused failed-root fixture and
  Debug/Trace/Release Substitute and proof tests pass. Causal Rank-17 campaign
  `20260901-4410-tierd-rank17-fixed-member-substitute-validation`, job
  `1658235`, requires the first binary multiset to become exact and retains all
  coarse boundaries through first BVE.
- Job `1658235` completed valid/clean in 44.008/33.225 seconds. The repaired
  first-Substitute binary multiset is exactly 198,233 rows in both arms with
  zero pair differences, causally closing the fixed-member rewrite mechanism.
  Later coarse state changes only by those two removed binaries, so this real
  general parity fix is not the large Rank-17 owner. The next clearly inferior
  transition remains second Backbone (429 versus 597 fixed variables); compare
  its complete binary input before attributing the deficit to Backbone itself.
- Prepared bounded campaign
  `20260901-4411-tierd-rank17-second-backbone-binaries` on freshly hashed
  observer pins. A tested ordinal selector captures only the complete binary
  multiset entering the second Backbone pass at conflict 3,654: Tactix's third
  matching Substitute end and Kissat's second `substitute_early_end`. Expected
  row counts are 258,941/258,938; global tracing, proofs, and models remain off.
  Priority remains inferior inprocessing, then Rank-27 CDCL, then bounded
  arena/watch representation convergence.
- Job `1658237` completed valid/clean in 43.819/32.714 seconds. The complete
  second-Backbone-input multisets differ in only 13 physical binary rows: eight
  Tactix-only and five Kissat-only, for the observed net +3. This small but real
  input split means the 429-versus-597 unit deficit is not yet attributable to
  Backbone. Campaign `4412` snapshots the immediately preceding second
  Congruence output to distinguish prior-state ownership from Substitute.
- Job `1658245` completed valid/clean in 43.878/32.797 seconds. At second
  Congruence end the full binary difference is nine rows: the same eight
  Tactix-only chain edges plus Kissat-only `(-452808,452826)`. The following
  Substitute adds four Kissat-only `906xxx` rows but does not create this older
  split. Because first Vivify begins from an exact binary multiset and is the
  earliest boundary to end at net +7, inspect its complete output next.
- Job `1658264` completed valid/clean in 44.008/32.559 seconds. First Vivify
  output differs in exactly the persistent nine rows: eight Tactix-only chain
  edges and Kissat-only `(-452808,452826)`. Its binary input is exact, so first
  Vivify owns the earliest remaining inferior binary-state divergence. The
  next observer traces only add/delete lifecycle events for these nine pairs.
- Job `1658266` completed valid/clean in 44.051/32.899 seconds. Tactix adds all
  eight chain edges during first Vivify; Kissat instead adds
  `(-452808,452826)` there. The first-output snapshot proves Kissat's six logged
  chain additions occur only in a later conflict-3,654 Vivify pass, with the
  final two no earlier than conflict 10,926. This is a first-round Vivify
  candidate/work-order split. Add selected source-clause annotations at root
  shortening and probing binary rewrites before changing production behavior.
- Job `1658269` completed valid/clean in 44.292/33.063 seconds. All eight
  Tactix chain edges are probing-produced binaries from four paired
  four-literal source groups, not schedule-time root shortening. Kissat later
  produces six from identical semantic source clauses; its first-dose
  `(-452808,452826)` instantiation likewise comes from the same three-literal
  source Tactix reaches only at conflict 10,926. Source semantics match; trace
  candidate ordinal and source position next to isolate schedule order/effort.
- Job `1658271` completed valid/clean in 44.376/32.923 seconds. Tactix reaches
  the four chain groups at first-dose irredundant ordinals 12,224, 12,756,
  13,302, and 13,862. Kissat reaches matching later groups in the subsequent
  same-conflict dose at ordinals 1,030, 1,576, and 2,136; the latter spacings
  exactly match Tactix. Kissat's glue-1 `452xxx` source is tier-1 ordinal 19,
  while Tactix defers it to conflict 10,926 ordinal 382. This points to band-
  level cumulative budget/frontier or priority carryover. Record one compact
  begin/end row per Vivify band next.
- Job `1658273` completed valid/clean in 44.272/33.267 seconds. First tier-1
  schedules are exact at 3,065 candidates, but Tactix tries 17/produces 16 and
  spends 674,910 ticks against ceiling 671,202; Kissat tries 20/produces 17 and
  spends 817,292 ticks from a 110,896,869 start against a 679,483 allowance.
  Tier 2 is exact; tier 3 schedules four but produces 4/2. The irredundant
  schedule is exact at 3,168,759, but Tactix enters with 115,049 more remaining
  cumulative budget and tries 14,184 versus 11,726, directly explaining the
  chain frontier. Compare only tier-1 candidates 0--20 next.
- Job `1658275` completed valid/clean in 44.394/34.189 seconds. First-outer
  tier-1 candidates 0--16 match exactly in semantic clause, physical pose,
  terminal outcome, and tick delta; both arms have spent exactly 674,910 ticks
  before ordinal 17. Tactix stops only because its allowance is 671,202, while
  Kissat's 679,483 allowance admits ordinals 17--19, including the useful
  `(-452808,452826,467362)` candidate, before stopping at ordinal 20. Candidate
  order and propagation are excluded through the common prefix. The total
  first-dose allowance is 2,237,340 versus Kissat's 2,264,943, a 27,603-tick
  reference deficit distributed proportionally across bands. Campaign `4419`
  records only Tactix's existing Vivify slot-limit calculation to identify the
  anchor/reference owner; it was submitted as job `1658278`. No proof checking
  is on this diagnostic path.
- Job `1658278` completed valid/clean in 44.450 seconds and identifies the
  exact general mechanism. At first search-time Vivify, Tactix has
  `now=22,649,450`, `anchor=276,045`, and `reference=22,373,405`; pinned Kissat
  keeps `last.ticks.probe=0` because initial preprocessing does not advance it.
  Those 276,045 ticks are input-unit closure already charged by the ordinary
  search propagator before solve entry, so Tactix's reset was incorrectly
  subtracting real reference work. The same initialization affected the first
  Eliminate dose. Both initial phase anchors now remain zero; only completed
  search-time phases advance them. A focused regression prices the exact
  nonzero pre-solve tick witness for both groups, and Trace/Release scheduler
  tests pass. Validate the repaired first Vivify candidate frontier and full
  binary output before proceeding to any later inferior inprocessing split.
- Paired causal campaign
  `20260901-4420-tierd-rank17-vivify-anchor-validation` is prepared on the
  repaired Trace pin. It compares candidate ordinals 0--20, compact band
  ledgers, the nine selected pair lifecycles, and the complete first-Vivify
  binary multiset, then exits before the first BVE candidate. Global tracing
  and certificates remain off. Submitted as job `1658279`.
- Job `1658279` completed valid/clean in 39.246/34.404 seconds. The repaired
  first tier-1 allowance is exactly 679,483 in both arms; normalized candidate
  rows through ordinal 20 and all later band frontiers match. Both complete
  first-Vivify binary multisets contain 197,611 rows and share data digest
  `ca0236d...a5ce07e`; semantic boundary counts are also exact. This causally
  closes the former nine-row first-Vivify divergence. The later conflict-3,654
  four-binary Substitute split is transient with an exact subsequent Backbone
  fixed count, while the conflict-10,926 pipeline often leaves Tactix fewer
  active variables; neither is yet demonstrated inferior. Measure the repaired
  full Rank-17 result before labeling another state difference as a priority
  regression.
- Prepared full checked Release Rank-17 campaign
  `20260901-4421-tierd-rank17-anchor-release`. It uses the uniform production
  configuration, validates the expected SAT model, and compares reporting-only
  with job `1658170` and pinned Kissat. Submitted as job `1658281`; no proof
  task exists for this instance.
- Job `1658281` is infrastructure-invalid and made no solver invocation: the
  inherited runner failed closed because the single-rank manifest omitted its
  required `input_sha256` map. The claim/result are retained, scratch is clean,
  and no timing claim is made. Use a fresh corrected campaign rather than
  overwriting this record.
- Corrected fresh campaign
  `20260901-4422-tierd-rank17-anchor-release-corrected` includes the pinned
  instance hash map and otherwise uses the identical Release binary, checker
  bundle, single-rank matrix, and uniform configuration. Submitted as job
  `1658282`.
- Job `1658282` is valid/clean: Rank 17 returns SAT in 256.908 seconds and
  `checkmodel` verifies all 6,040,974 clauses. This is 1.08196x the preceding
  candidate and 2.10899x pinned Kissat. Conflicts improve to 163,700 versus
  Kissat's 186,237 and decisions are close at 4,068,548 versus 3,961,527, but
  Tactix still performs 1,384,202,105 search propagations versus about
  830,971,844 non-probing Kissat propagations. The repaired exact Vivify state
  is retained; later coarse inprocessing splits are transient or more
  aggressive for Tactix, not demonstrated inferior. The remaining Rank-17 gap
  is mixed CDCL propagation work and implementation cost. Per the priority
  order, resume the frozen Rank-27 CDCL heap frontier before arena/watch work.
- Because the anchor repair changes search-time inprocessing budgets, do not
  assume the old conflict-19,381 Rank-27 frontier survives. Prepared compact
  learned-only campaign `20260901-4423-tierd-rank27-learned-prefix50k` on the
  current pin. It records at most one final learned row per conflict through
  50,000, with all detailed watch/implication logging off; only after locating
  the current first divergence should the heap window be reopened. Submitted
  as job `1658283`.
- Job `1658283` completed valid/untruncated in 67.888/74.198 seconds. Portable
  learned semantics, decisions, levels, and modes are exact through conflict
  19,382. At 19,383 only the recorded level differs (23,120/23,122) with exact
  decision count and clause hashes; first learned content differs at 19,387.
  The repaired pin reproduces the old frontier and decision counts exactly.
  Reopen the bounded heap-selection observer on fresh pins at semantic decision
  6,606,976; do not broaden the trace.
- Prepared current-pin campaign
  `20260901-4424-tierd-rank27-heap-selection6606976` from the previously frozen
  observer. It covers only conflicts 19,381--19,382 and heap operations around
  decision 6,606,976, with watch/implication logs disabled; both arms must emit
  a complete unique-variable heap snapshot and the selected pop path. Submitted
  as job `1658285`.
- Job `1658285` is observer-invalid but complete steering: both arms reach the
  selected decision with untruncated traces and valid unique-variable heap
  snapshots, but neither emits the pre-required `heap_pop` because the chosen
  root is already unassigned. No behavioral claim is taken from the invalid
  campaign. Steering shows 90,065 common heap variables with zero score
  differences, including candidates 284,274 and 291,261 both at 6.5, but 5,798
  common positions differ. Kissat retains 47 assigned lazy members absent from
  Tactix. The likely owner is earlier equal-score heap topology/lifecycle, not
  score arithmetic or the final pop. Compare a valid full heap snapshot at the
  first stable-mode learned boundary before tracing individual mutations.
- Prepared paired campaign `20260901-4425-tierd-rank27-first-stable-heap` to
  snapshot the complete heap at conflict 1,002 / decision 60,632, the first
  learned row in stable mode. It observes only conflicts 1,002--1,003 and does
  not require a pop event. Submitted as job `1658287`.
- Job `1658287` is observer-invalid: Kissat's search-only filter deliberately
  admits `sweep_schedule`, so startup Sweep filled the row cap before the
  selected window. Both 345,226-row heap snapshots were still written and
  their header-free bytes match (`c490fd...2d9882`), but this is steering only.
  Retry with trajectory-only filtering, which excludes startup Sweep.
- Corrected fresh campaign
  `20260901-4426-tierd-rank27-first-stable-heap-corrected` keeps the same
  immutable solver pins, conflict 1,002 / decision 60,632 snapshot, and
  low-volume boundary, but uses trajectory-only filtering so startup Sweep
  cannot consume the trace cap. Submitted as job `1658289`; no proof or model
  task is part of this diagnostic, and proof checking remains nonblocking.
- Job `1658289` reached conflict 1,003 cleanly in both arms in 3.171/3.462
  seconds but is observer-invalid. Trajectory-only correctly removed Sweep;
  however, Tactix returned before the separately selected one-shot heap dump,
  while Kissat emitted it and ended by the observer's intentional `SIGTERM`.
  The Tactix observer now attempts the selected heap snapshot before its cheap
  trajectory-only return, matching Kissat without hashing or emitting ordinary
  heap-score rows. The fresh runner must also accept only the selected Kissat
  endpoint signal in addition to normal completion.
- Fresh campaign `20260901-4427-tierd-rank27-first-stable-heap-valid` pins the
  observer-order correction (`fbcce3d6...9cdabd1`), retains the identical
  formula and boundary, and recognizes Kissat's intentional selected-endpoint
  `SIGTERM` only after the complete snapshot and untruncated trace conditions
  hold. Submitted as job `1658291`.
- Job `1658291` produced both complete 345,226-row snapshots and their
  header-free byte streams are exact (`c490fd...2d9882`), but the campaign is
  observer-invalid because its inherited harness also required a `decision`
  row. There is no new decision inside conflicts 1,002--1,003; decision 60,632
  is the state at which the snapshot hook fires. Preserve the exact result as
  steering and issue a fresh harness-only correction requiring the two actual
  conflict/analysis/learned transactions instead.
- Fresh harness-only campaign
  `20260901-4428-tierd-rank27-first-stable-heap-final` retains the same pins
  and observer, replacing the impossible decision-row condition with at least
  two complete conflict/analysis/learned transactions through conflict 1,003.
  Submitted as job `1658293`.
- Job `1658293` is valid/clean in 3.716/3.463 seconds. Both heap snapshots
  contain 345,226 unique valid rows and are byte-identical after their solver
  headers (`c490fd...2d9882`). Therefore Rank 27 enters its first stable-mode
  learned boundary with exact scores and exact physical heap topology. The
  eventual equal-score selection divergence is introduced later; next take
  the same single snapshot at exact conflict 3,070 / decision 4,322,527, after
  the first large inprocessing transition but while learned semantics remain
  exact.
- Prepared `20260901-4429-tierd-rank27-heap-after-first-inprocess` with the
  same validated one-shot observer at conflict 3,070 / decision 4,322,527.
  This is the first learned boundary after the large early inprocessing work;
  both learned rows and decision counts are exact there. Submitted as job
  `1658296`.
- Job `1658296` is valid/clean in 25.738/26.342 seconds. Both snapshots have
  428,815 unique valid rows and their header-free byte streams match exactly
  (`ced5358a...f72ba`). Thus neither the first large inprocessing transition
  nor the ensuing stable reconstruction introduces the eventual heap
  divergence. Continue coarse bisection at conflict 11,000 / decision
  5,827,539, still inside the exact learned trajectory.
- Prepared coarse midpoint campaign
  `20260901-4430-tierd-rank27-heap-mid-search` at conflict 11,000 / decision
  5,827,539 with the same one-shot snapshot and two-conflict trace. Submitted
  as job `1658298`.
- Job `1658298` emitted byte-identical 5,379-row heap snapshots
  (`9bd45cf2...a0f036`) but is observer-invalid: the inherited validity check
  required more than 100,000 rows even though inprocessing has legitimately
  reduced the heap. Preserve as steering and rerun fresh with the general
  requirement that the exact-boundary heap be nonempty, unique, and
  structurally valid.
- Fresh size-independent midpoint campaign
  `20260901-4431-tierd-rank27-heap-mid-search-valid` retains the exact pins,
  boundary, and trace, changing only the validity floor from 100,000 rows to a
  nonempty unique valid heap. Submitted as job `1658300`.
- Job `1658300` is valid/clean in 40.438/34.829 seconds. Both 5,379-row heaps
  are header-free byte-identical (`9bd45cf2...a0f036`). Rank 27's heap remains
  exact through conflict 11,000, so continue coarse bisection at conflict
  15,000 / decision 5,930,114 rather than opening detailed mutation logging.
- Prepared `20260901-4432-tierd-rank27-heap-conflict15000` with the validated
  size-independent one-shot observer at conflict 15,000 / decision 5,930,114.
  Submitted as job `1658302`.
- Job `1658302` is valid/clean in 43.932/36.926 seconds. Both 422,171-row
  heaps are header-free byte-identical (`3d8c8e89...15b99a9`). The topology
  remains exact through conflict 15,000. Continue the coarse bisection at
  conflict 17,200 / decision 6,129,595.
- Prepared `20260901-4433-tierd-rank27-heap-conflict17200` at conflict 17,200
  / decision 6,129,595 with the unchanged one-shot observer. Submitted as job
  `1658312`.
- Job `1658312` is valid/clean in 54.179/42.209 seconds. Both 151,348-row
  heaps are header-free byte-identical (`150d02ca...b584a3`). The topology
  remains exact through conflict 17,200. Continue coarse bisection at conflict
  18,300 / decision 6,426,315.
- Prepared `20260901-4434-tierd-rank27-heap-conflict18300` at conflict 18,300
  / decision 6,426,315 with the unchanged one-shot observer. Submitted as job
  `1658314`.
- Job `1658314` is valid/clean in 56.451/43.329 seconds. Both 109,134-row
  heaps are header-free byte-identical (`719eaf93...006373`). The topology
  remains exact through conflict 18,300. Continue coarse bisection at conflict
  18,800 / decision 6,484,636.
- Prepared `20260901-4435-tierd-rank27-heap-conflict18800` at conflict 18,800
  / decision 6,484,636 with the unchanged one-shot observer. Submitted as job
  `1658316`.
- Job `1658316` is valid/clean in 56.470/43.595 seconds. Both 130,418-row
  heaps are header-free byte-identical (`150b1c02...cc4e57`). The topology
  remains exact through conflict 18,800. Continue coarse bisection at the next
  convenient learned boundary, conflict 19,200 / decision 6,547,571.
- Prepared `20260901-4436-tierd-rank27-heap-conflict19200` at conflict 19,200
  / decision 6,547,571 with the unchanged one-shot observer. Submitted as job
  `1658318`.
- Job `1658318` is valid/clean in 56.720/43.748 seconds. Both 130,873-row
  heaps are header-free byte-identical (`59bbabcf...d8cb2e`). The topology
  remains exact through conflict 19,200. The next convenient exact learned
  boundary is conflict 19,370 / decision 6,583,073; use it before opening
  detailed heap mutations.
- Add exactly one bounded consolidation fuzz campaign after the CDCL mechanism
  repair and focused tests, before Rank-33/Tier-B+C/Rank-1--40 acceptance.
  Preserve the Claude-owned private-workspace boundary: do not inspect or
  replace its artifacts, and document the independent fuzz scope before
  launch. Fuzzing does not interrupt the current coarse-to-fine localization.
- Rank 27 gap closure is the near-term completion target, not merely closure of
  the current CDCL hypothesis. Follow any evidence-backed general mechanism,
  including inprocessing, propagation, or representation. When a substantial
  improvement is implemented, bank it with focused tests and the bounded fuzz
  campaign before continuing if a meaningful Rank-27 gap remains.
- Prepared `20260901-4437-tierd-rank27-heap-conflict19370` at conflict 19,370
  / decision 6,583,073 with the unchanged one-shot observer. Submitted as job
  `1658320`.
- Job `1658320` is valid/clean in 65.504/64.554 seconds and crosses the heap
  boundary. Both arms have 151,490 unique valid rows, but only 151,485 common
  variables; five are arm-only on each side, 102 common positions differ, and
  133 common assignment bits differ. All common scores and active bits are
  exact. Thus the first heap-topology/lazy-membership difference lies between
  exact conflict 19,200 and 19,370, not in score arithmetic. Continue the
  coarse bisection at conflict 19,291 / decision 6,563,294.
- Prepared `20260901-4438-tierd-rank27-heap-conflict19291` at the midpoint
  exact learned boundary, conflict 19,291 / decision 6,563,294. Submitted as
  job `1658322`.
- Job `1658322` is valid/clean in 57.072/43.794 seconds. Both 131,037-row
  heaps are header-free byte-identical (`99dbdadb...1e8820`). The first
  topology difference is after conflict 19,291. Continue at conflict 19,333 /
  decision 6,573,283.
- Prepared `20260901-4439-tierd-rank27-heap-conflict19333` at conflict 19,333
  / decision 6,573,283. Submitted as job `1658324`.
- Job `1658324` is valid/clean in 56.821/44.049 seconds. Both 132,300-row
  heaps are header-free byte-identical (`8851854f...c9e2b`). The first
  topology difference is after conflict 19,333. Continue at conflict 19,352 /
  decision 6,575,701.
- Prepared `20260901-4440-tierd-rank27-heap-conflict19352` at conflict 19,352
  / decision 6,575,701. Submitted as job `1658326`.
- Job `1658326` is valid/clean in 56.868/43.847 seconds. Both 131,344-row
  heaps are header-free byte-identical (`dbabca2a...0d3280`). The first
  topology difference is after conflict 19,352. Continue at conflict 19,361 /
  decision 6,582,961.
- Prepared `20260901-4441-tierd-rank27-heap-conflict19361` at conflict 19,361
  / decision 6,582,961. Submitted as job `1658328`.
- Job `1658328` is valid/clean in 64.396/62.089 seconds and is divergent.
  Both arms have 140,857 heap rows; 140,852 variables are common, five are
  arm-only on each side, only five common positions differ, zero common scores
  or active bits differ, and 128 assignment bits differ. The preceding learned
  checkpoints are 19,358 / decision 6,576,714 and then 19,361 / 6,582,961.
  Check 19,358 next; if exact, localize the owner to that single large
  decision/backtrack transition.
- Prepared `20260901-4442-tierd-rank27-heap-conflict19358` at conflict 19,358
  / decision 6,576,714. Because conflict numbering next jumps to 19,361, the
  selected endpoint may cross that boundary, but the one-shot snapshot itself
  is fixed at the exact pre-transition state. Submitted as job `1658330`.
- Job `1658330` emitted byte-identical 131,546-row heaps
  (`00ec96fb...982a9`) but is observer-invalid because conflicts 19,359--19,360
  are probing/root conflicts and yield one learned row, not the predeclared
  two. Both arms do record two conflict/analysis transactions. Preserve as
  steering and retry fresh through conflict 19,361 with the general contract
  requiring at least one learned row. The divergence is therefore introduced
  during the Probe-to-search transition after conflict 19,358.
- Fresh campaign
  `20260901-4443-tierd-rank27-heap-pre-probe-transition-valid` retains the
  exact conflict-19,358 snapshot, extends the endpoint through 19,361, and
  requires two conflict/analysis transactions plus at least one learned row.
  Submitted as job `1658332`.
- Job `1658332` is valid/clean in 92.140/76.971 seconds. The conflict-19,358
  heaps contain 131,546 unique valid rows and match byte-for-byte after headers
  (`00ec96fb...982a9`). All following 6,247 decision rows through conflict
  19,361 normalize exactly. Tactix's compact trace exposes the intervening
  Probe/Backbone work. Before tracing per-decision mutations, snapshot conflict
  19,360 / decision 6,576,714: the post-Probe, pre-new-decision state. If it
  differs, inprocessing owns the first topology transition; if exact, the
  owner is within the ensuing exact decision sequence.
- Prepared `20260901-4444-tierd-rank27-heap-post-probe-predecision` to snapshot
  conflict 19,360 / unchanged decision 6,576,714, after Probe/Backbone and
  before the 6,247-decision run. Submitted as job `1658334`.
- Job `1658334` is valid/clean in 92.189/76.121 seconds. Both post-Probe,
  pre-new-decision heaps have 132,357 unique valid rows and are header-free
  byte-identical (`b04d92bd...f88018`). Probe/Backbone does not introduce the
  topology discrepancy. It arises within decisions 6,576,715--6,582,961 even
  though all chosen literals match. Bisect at decision 6,579,837 while
  conflict remains 19,360, then trace only the first differing heap mutation.
- Prepared `20260901-4445-tierd-rank27-heap-decision6579837` at conflict
  19,360 / midpoint decision 6,579,837. Submitted as job `1658336`.
- Job `1658336` is valid/clean in 92.703/74.138 seconds and divergent. Both
  heaps have 150,576 rows; five variables are arm-only on each side and only
  five common positions differ, with exact common scores/active bits and 186
  assignment-bit differences. The first mutation lies in decisions
  6,576,715--6,579,837. Add a diagnostic-only stop immediately after the
  selected heap-set file is closed so decision bisection does not replay the
  unused suffix; apply identically to both observers before the next fresh pin.
- Both diagnostic observers now honor `SAT_EVENT_TRACE_HEAP_SET_STOP` only
  after closing a successfully selected heap-set file. Fresh pins are Tactix
  `602fbe3f...ab655d` and Kissat `5f7c981e...52717c`; this changes no solver
  policy or production path. Prepared stopped bisection campaign
  `20260901-4446-tierd-rank27-heap-decision6578275-stop` at conflict 19,360 /
  decision 6,578,275 and submitted it as job `1658338`.
- Job `1658338` is observer-invalid steering despite its harness result.
  Kissat increments `decisions` before selecting a variable, so its generic
  heap hook can match during lazy pops before the decision event/assignment;
  Tactix increments inside Decide and matches after assignment. Tactix emitted
  1,561 selected-window decision rows versus Kissat's 1,560, confirming the
  phase mismatch. Both observers now support a post-assignment-only heap-set
  stage called immediately after the portable decision event. Repeat the same
  decision counter on fresh pins before any further bisection.
- Both observers now have an explicit post-assignment heap-set stage invoked
  immediately after their portable decision event; generic lazy-pop hooks are
  suppressed when that stage is selected. Fresh pins are Tactix
  `f3c4c605...70bf7` and Kissat `318b1e65...2c386`. Prepared same-counter
  campaign `20260901-4447-tierd-rank27-heap-decision6578275-postdecision` and
  submitted it as job `1658340`.
- Job `1658340` is valid and confirms a real aligned post-decision difference.
  Both arms emit exactly 1,561 decision rows and both heaps have 156,155 rows;
  five variables are arm-only on each side, five common positions differ,
  common scores/active bits are exact, and 133 assignment bits differ. The
  owner is decisions 6,576,715--6,578,275. Continue aligned bisection at
  decision 6,577,494.
- Prepared aligned post-decision campaign
  `20260901-4448-tierd-rank27-heap-decision6577494-postdecision` at decision
  6,577,494. Submitted as job `1658342`.
- Job `1658342` is valid and divergent at aligned decision 6,577,494; both
  arms emit 780 decision rows and have 159,237 heap rows. The first difference
  is within decisions 6,576,715--6,577,494. Continue at decision 6,577,104.
- Prepared aligned campaign
  `20260901-4449-tierd-rank27-heap-decision6577104-postdecision` at decision
  6,577,104. Submitted as job `1658344`.
- Job `1658344` is valid and divergent at decision 6,577,104; both arms emit
  390 decision rows and have 160,500 heap rows. Continue at decision 6,576,909.
- Prepared aligned campaign
  `20260901-4450-tierd-rank27-heap-decision6576909-postdecision` at decision
  6,576,909. Submitted as job `1658346`.
- Job `1658346` is valid and divergent; both arms emit 195 decision rows and
  have 161,305 heap rows. Five zero-score active variables are arm-only on each
  side, only five common positions differ, and all common scores/active bits
  match. The aligned first-divergence bracket is decisions
  6,576,715--6,576,909. Continue at decision 6,576,811; mutation-level logging
  remains premature until the exact decision is known.
- Prepared aligned campaign
  `20260901-4451-tierd-rank27-heap-decision6576811-postdecision` at decision
  6,576,811 and submitted it unthrottled as job `1658348`. No proof or model
  checking is on this diagnostic path.
- Job `1658348` completed valid/clean and divergent with 97 decision rows and
  equal 161,694-row heaps. The same five zero-score variables differ per arm;
  common scores/active bits remain exact. The bracket is now
  6,576,715--6,576,811; continue at decision 6,576,762.
- Submitted aligned midpoint campaign `4452` as job `1658350` at decision
  6,576,762, again with full array width and no correctness-check dependency.
- Job `1658350` completed valid/clean and divergent with 48 decisions and
  equal 161,917-row heaps. The five-member zero-score split is unchanged; the
  bracket is now 6,576,715--6,576,762. Continue at decision 6,576,738.
- Submitted aligned midpoint campaign `4453` as job `1658352` at decision
  6,576,738; it remains a two-arm, unthrottled, proof-free diagnostic.
- Job `1658352` completed valid/clean and divergent with 24 decisions and
  162,039 heap rows per arm. The same discrete five-member split remains; the
  bracket is now 6,576,715--6,576,738. Continue at decision 6,576,726.
- Submitted aligned midpoint campaign `4454` as job `1658354` at decision
  6,576,726; no proof/model checker is present.
- Job `1658354` completed valid/clean and divergent with 12 decisions and
  162,078 heap rows per arm. The same five zero-score members differ and only
  two common assignment bits now differ. The bracket is
  6,576,715--6,576,726; continue at decision 6,576,720.
- Submitted aligned midpoint campaign `4455` as job `1658356` at decision
  6,576,720, with no proof/model checker.
- Job `1658356` completed valid/clean and divergent after only six decisions;
  both heaps have 162,107 rows and the same five-member zero-score split. The
  bracket is now decisions 6,576,715--6,576,720. Continue at 6,576,717.
- Submitted aligned midpoint campaign `4456` as job `1658358` at decision
  6,576,717; no proof/model checker is present.
- Job `1658358` completed valid/clean and divergent after three decisions;
  decision literals `145,-74283,-132505` are exact, but the unchanged
  five-member zero-score heap split is already present. Test the first decision
  6,576,715 directly; the exact pre-stage remains 6,576,714.
- Submitted exact first-decision campaign `4457` as job `1658360` at decision
  6,576,715, with no proof/model checker.
- Job `1658360` completed valid/clean. Immediately after the first exact
  decision literal `145`, both heaps have 162,130 rows but differ by the same
  five zero-score active variables per arm; common positions/scores/assignment/
  active bits are otherwise exact. No lazy heap pop occurs for this decision.
  Reinspection corrects the previous boundary label: campaign `4444`'s exact
  snapshot fired at an early generic conflict-19,360 hook, before four logged
  Backbone computations and their final heap repopulation, not after the whole
  inprocessing episode. Thus the first owner is the tail of inprocessing, not
  CDCL lazy removal. Trace only the ten differing variables through those
  Backbone/root-assignment/heap-insert events next.
- Submitted selected-variable lifecycle campaign `4458` as job `1658362`.
  It records only heap/root events involving the ten arm-specific variables,
  retains the first-decision complete snapshot, and has no proof/model checker.
- Job `1658362` is observer-invalid/clean. Both complete endpoint heaps repeat
  `4457`, but trajectory-only filtering runs after heap-vars-only and removed
  every intended selected lifecycle row. Retry fresh without trajectory-only;
  heap-vars-only itself bounds the stream to the ten variables.
- Submitted corrected selected-variable campaign `4459` as job `1658364` with
  heap-vars-only as the sole stream filter. No proof/model checker is present.
- Job `1658364` emitted the intended lifecycle but is observer-invalid/clean
  because the harness incorrectly required `analysis_end`, which
  heap-vars-only filters. Steering is exact: both arms identically insert all
  ten vars; Tactix then derives/removes `490736,490929,491050,492208,492508`,
  while Kissat derives/removes `490726,490928,491049,492209,492509`. Retry with
  the actual 10-insert/5-root/5-remove contract before promoting the evidence.
- Submitted harness-only validation campaign `4460` as job `1658366`, requiring
  exactly ten selected inserts, five selected roots, five selected removals,
  and the complete endpoint heap in each arm. No proof/model checker is present.
- Job `1658366` completed valid/clean and reproduces the lifecycle exactly.
  Identical scores, insertion poses, and cumulative insertion hashes exclude
  heap construction. Tactix fixes/removes `490736,490929,491050,492208,492508`;
  Kissat fixes/removes `490726,490928,491049,492209,492509`. The removals have
  matching positions and matching last members pairwise, so heap mechanics are
  a downstream reporter. Localize the exact preceding rule boundary and root
  derivation with compact high-level control logging before candidate tracing.
- Submitted high-level control campaign `4461` as job `1658368`. It retains
  macro rule boundaries and root events through the first decision, suppresses
  watches/implications, and has no proof/model checker.
- Job `1658368` is observer-invalid/clean. `CONTROL_ONLY` also admits Factor's
  high-volume `factor_*` detail stream, so both arms saturated the 200,000-row
  cap before the intended complete rule-boundary stream. Both endpoint heaps
  are complete and structurally valid; this is an observer-capacity failure,
  not trajectory evidence. Use a dedicated snapshot-only filter next.
- Submitted snapshot-only campaign `4462` as job `1658370`. The new diagnostic
  filter retains only macro snapshots (plus trace endpoints), explicitly keeps
  snapshots before the safety stop, and still closes on the complete heap after
  conflict 19,360 decision 6,576,715. No proof/model checker is present.
- Job `1658370` completed valid/clean: 884 Tactix and 1,016 Kissat boundary
  rows, no truncation, and complete 162,130-member endpoint heaps. The five
  selected roots arise in the second Congruence call, between dense-first and
  dense-second-entry. The preceding Factor boundary is the first clause-count
  split: identical inputs of 592,491 irredundant, 3,160 redundant, and 210,554
  binary clauses become Tactix 577,512/3,160/201,163 versus Kissat
  577,514/3,160/201,161. Thus Congruence and the heap are downstream. Compare
  exact pre/post-Factor formula and binary multisets next; equal input counts do
  not yet prove equal input content.
- Submitted exact-set campaign `4463` as job `1658372`: four unthrottled tasks
  capture Tactix/Kissat large-clause and binary multisets at the first Factor
  input and output boundary at conflict 19,360, then exit immediately. This is
  diagnostic-only and has no proof/model checker.
- The invalid/clean `4461` detail stream is used only for steering: its first
  differing `factor_fresh` row follows 1,935 exact transactions, after which
  Tactix selects seed `-203452` and Kissat `-96920`. Submitted independent
  validating campaign `4464` as job `1658376`; it records transaction 1,935's
  exact update stream and complete resulting Factor heap, then exits at the
  arm-specific transaction-1,936 endpoint. No proof/model checker is present.
- Job `1658372` (`4463`) is observer-invalid/clean. All four tasks hit the
  600-second observer timeout at conflicts 3,069--4,006 because keeping every
  global snapshot computed expensive ranking summaries long before the target.
  No exact set was emitted; every task deleted scratch. Retry target-only.
- Job `1658376` (`4464`) emitted both complete 292,026-member Factor heaps and
  both 26-row update streams, but is observer-invalid/clean because its harness
  rejected Kissat's intentional `SIGTERM` diagnostic endpoint. Validate those
  immutable artifacts with the established endpoint contract before promoting
  the steering result.
- Harness-only campaign `4465` validates the immutable `4464` artifacts under
  the intended endpoint contract with zero residue. All 26 transaction-1,935
  updates are exact. Both resulting heaps contain the same 292,026 unique
  literal/score pairs, but their physical roots differ: Tactix `-202372`
  score 66 versus Kissat `-96920` score 66. The topology divergence predates
  this exact update; locate its first transaction with compact heap digests.
- Submitted corrected exact-set campaign `4466` as job `1658378`. Its four
  tasks use target-only filtering, so every pre-target macro returns before
  ranking summaries; each selected Factor boundary still emits complete large
  and binary multisets before its explicit exit. No proof/model checker exists.
- Campaign `4466` is observer-invalid/clean and was manually cancelled after
  5m58s: its target-only variable was accidentally Tactix-prefixed instead of
  using the generic observer name, so the expensive pre-target summaries were
  not bypassed. It emitted no selected exact set and left zero scratch residue.
- Added a disabled-by-default portable topology digest over Factor's physical
  heap order, external literals, and integer scores; Trace builds, Kissat's
  diagnostic overlay builds, and focused event-trace/Factor tests pass.
  Submitted campaign `4467` as job `1658382` to emit one digest after every
  successful first Factor transaction and exit at that Factor boundary. No
  proof/model checker exists.
- Job `1658382` (`4467`) is observer-invalid/clean only because its harness
  required a Factor-end row suppressed by the ordinary pre-safety-stop snapshot
  rule. Both arms emitted 5,048 monotone, untruncated heap digests and reached
  the conflict-19,360 safety endpoint. Frozen steering shows exact topology
  hashes through transaction 1,819 and the first hash split after transaction
  1,820; validate that series harness-only before promoting it.
- Harness-only campaign `4468` validates both frozen 5,048-row series with
  zero residue. They partition identically into 5,041 transactions in the
  first Factor call and seven in the second. Physical topology hashes are exact
  through transaction 1,819 and first differ after transaction 1,820, while
  heap size 292,261, root `202583`, and root score 82 remain equal. Trace only
  transaction 1,820's updates and complete resulting heaps next.
- Submitted focused campaign `4469` as job `1658384`. It records only Factor
  transaction 1,820's update stream and complete resulting heap per arm, then
  exits at the common transaction-1,821 fresh endpoint. No proof/model checker
  exists.
- Job `1658384` (`4469`) completed valid/clean. Both arms emit 54 update rows
  and complete unique 292,261-member heaps. Update literal/score multisets are
  exact, but order differs: remainder literal `489855` is first in Tactix's
  quotient-column stream and last in Kissat's, in both factor-row passes. Heap
  member/score multisets remain exact while their first physical-position split
  is 146,129. Trace binary `(202606,489855)` to its earlier fresh-divider
  creation and dense-watch insertion lifecycle.
- The binary is a divider created at transaction 1,265
  (`fresh=489855`, factors `202608,202606,-202583`). This exposes a general
  Factor representation discrepancy: Tactix borrowed the initial binary watch
  prefix but reconstructed *all* later binaries ahead of its large-clause
  occurrence vector, whereas Kissat keeps one mixed dense stream and appends a
  new divider after every occurrence already connected. A bounded production
  repair now keeps the initial binaries borrowed, appends only during-Factor
  binaries into the same stream as during-Factor large quotients, and compacts
  those appended entries on deletion. It adds no workload gate or whole-formula
  duplicate image. A focused two-transaction regression passes and would put
  the appended binary's clause id before, rather than after, three existing
  large quotients under the old reconstruction. Rank-27 causal validation is
  next; this result is not yet a timing claim.
- Submitted causal campaign `4470` as job `1658386`. Its two pinned arms
  replay only transaction 1,820's 54 ordered heap updates and complete
  292,261-member post-update heap, then stop at the common transaction-1,821
  endpoint. It has no proof/model checker and must match ordered content, not
  merely multisets, before the mechanism repair is promoted.
- Job `1658386` (`4470`) completed valid/clean/exact. After normalizing only
  the arm label and Tactix's extra read-only `grid=` field, all 54 transaction-
  1,820 updates match in order and all 292,261 heap position/literal/score rows
  match. Transaction 1,821 now selects the same seed `202822`. This closes the
  repair at the first validated topology split; downstream Factor output,
  Congruence roots, and first search decision remain the next causal gates.
- Submitted downstream campaign `4471` as job `1658388`. It reuses the valid
  snapshot-only endpoint to compare the repaired Factor output, second
  Congruence lifecycle, and complete first post-Probe decision heap. No proof
  or model checker exists.
- Job `1658388` (`4471`) completed valid/clean and materially converged. The
  formerly split Factor output is now count-exact at 577,514 irredundant,
  3,160 redundant, and 201,161 binary clauses. The following Congruence creates
  the same 95 roots and reaches the same 162,361 active variables. The complete
  162,130-variable first post-Probe heap is exact in position, external
  variable, score, assignment, and activity. A smaller redundant-clause count
  difference remains (3,160/3,158 at that Congruence exit, then 2,669/2,668
  before the later Factor); its irredundant/binary output and final heap are
  exact. Measure Rank-27 Release before deciding whether that residue owns any
  remaining performance gap.
- Submitted checked Rank-27 Release campaign `4472` as job `1658390`. The
  solver writes a provisional UNSAT verdict, timing, memory, and statistics
  before its independent DRAT check begins. Continue immediately from that
  provisional result; do not wait for proof checking, but retain and inspect
  its final result later.
- Submitted compact learned-prefix campaign `4473` as job `1658391`. It
  compares only final learned records through conflict 100,000 to locate any
  remaining post-repair search divergence without a full-detail trace. No
  proof/model checker exists.
- Job `1658391` (`4473`) completed valid/clean. Learned records are exact
  through conflict 29,694, extending the prior exact prefix from 19,382 by
  9,812 conflicts. Conflict 29,695 retains identical learned content while
  Tactix has one extra decision and one extra recorded decision level; the
  first order-only learned difference is 29,703 and the first learned-set
  difference is 29,706. Follow the requested coarse-to-fine method: inspect
  high-level scheduling/inprocessing boundaries around 29,690 before detailed
  CDCL logging.
- Submitted the high-level conflict-29,680--29,710 macro window as campaign
  `4474`, job `1658393`. Its window-aware observer rejects pre-target ranking
  snapshots, retains scheduling/inprocessing boundaries near the split, and
  has no proof/model checker.
- Campaign `4474` is observer-invalid/clean but supplies bounded steering. Its
  filter suppresses Kissat's terminal analysis row, and the newly consolidated
  Tactix diagnostic terminator incorrectly fell through to `abort` after the
  cooperative SIGTERM handler returned; that observer-only fallthrough is now
  a clean exit. The immutable boundary rows show the next priority is still
  inprocessing: both arms enter Eliminate at conflict 29,694 with 162,119
  active variables, 575,408 irredundant clauses, 203,572 binaries, 7,368,531
  decisions, level 5,702, and exact top-32 order (apart from the known one red
  clause), but leave at Tactix/Kissat 146,733/147,104 active,
  582,005/586,222 irredundant, 5,578/5,680 redundant, and
  180,347/181,992 binary. This precedes the extra decision at conflict 29,695;
  localize the Eliminate/BVE mechanism before CDCL.
- Submitted coarse BVE-pop checkpoint campaign `4475` as job `1658396`. It
  records only each 4,096th cumulative pop/order hash in the conflict-29,694
  Eliminate phase, then stops at the next analysis boundary. This brackets the
  first BVE ordering split before any per-candidate logging; no proof/model
  checker exists.

## Rank-38 current localization

- Homogeneous Rank 38 is a stable search-work loss: candidate 790.264 s versus
  pinned Kissat 366.258 s, with 35,907,678/47,372,472 conflicts/decisions
  versus 18,430,710/24,657,248.  Its accepted/current times are essentially
  unchanged, so it is the next general search-trajectory owner rather than a
  volatile downstream rank.
- Fresh pinned trace binaries and the fixed Rank-38 input match exactly through
  conflict 50,000: all 458,818 common portable decisions, conflicts, analysis
  boundaries, and learned-clause stages agree.  Candidate job `1658059`
  (`20260831-4361-tierd-rank38-trajectory-prefix`) is valid and untruncated;
  Tactix's eight-row suffix is solely its stop landing at conflict 50,001.
- Predecessor job `1658057` is observer-invalid, not a solver divergence.  A
  Tactix-only generic 1,024-call post-Reorder detail budget suppressed compact
  `analysis_begin`/`analysis_end` rows after conflict 10,043 while Kissat kept
  them.  Trajectory mode now retains every portable analysis boundary, with a
  regression that exhausts the old budget.  The repair is diagnostic-only and
  N4 read-only.
- Learned-only campaign `20260831-4362-tierd-rank38-learned-prefix1m`, job
  `1658061`, first differed at conflict 136,869.  Narrow mode-clock traces
  localized the cause to one extra Tactix search tick at conflict 128,927:
  scan `+367` had identical 1,501-entry watch order, clause poses, `searched`
  cursors, and values, but Tactix charged 160 relocations while Kissat charged
  159.  Both removed 160 large entries; the remaining Kissat removal was the
  garbage 21-literal clause later identified as Tactix clause ID 147584.
- Kissat retires that clause at conflict 128,927 through
  `eagerly_subsume_last_learned`.  Tactix had inserted the exact clause into
  its recent-learned register at conflict 128,925, then cleared the register
  during Vivify's Tactix-only reclamation collection.  Pinned Kissat does not
  collect at Vivify exit, so its register survives.  The general repair adds
  an explicit collector mode that forwards live recent-learned refs (and
  invalidates garbage refs) only for implementation-only Vivify reclamation;
  genuine Kissat-equivalent collections retain their unconditional reset.
  This is rule/mechanism dependent only and adds no formula or workload gate.
- Focused Analyze/EventTrace tests pass.  After repair, clause ID 147584 is
  preserved and eagerly deleted at conflict 128,927; scan `+367` is exact at
  159 relocations and 429 ticks.  A fresh local learned-only run matches all
  996,949 pinned-Kissat final learned records exactly through conflict
  1,000,000.  Release 76/76 and Trace 75/75 pass.  Rank-33 campaign
  `20260831-4370-tierd-rank33-vivify-eager-register`, job `1658063`, returns
  expected UNSAT provisionally in 466.840 s (prior valid 467.925 s); its proof
  check continues nonblocking.  Full fixed B28+C5 campaign
  `20260831-4380-BC-vivify-eager-register`, job `1658064`, is submitted with
  shipped/candidate SHA pins `c18133ce...cab00` / `9b53e154...52ab6`.
  All 66 provisional verdicts are expected: candidate 2,007.209 s versus
  shipped 2,000.647 s (1.003280x), inside the 1.10 guard; final validation
  continues.  Uniform Rank-1--40 campaign
  `20260831-4390-tierd-ranks1-40-vivify-eager-register` first had an
  infrastructure-only job `1658130`: all tasks stopped before solver invocation
  because the fresh runner directories were absent; its logs are retained.
  After creating those empty directories, job `1658170` is the sole solver
  execution from the same candidate pin, with no comparator rerun or selector;
  proof checks may finish asynchronously.  Its repaired Rank 38 is expected
  UNSAT in 388.117 s / 18,197,625 conflicts, versus the preceding 790.264 s /
  35,907,678 and pinned Kissat's 366.258 s / 18,430,710: search work converges,
  leaving a roughly 6% implementation-cost gap on this witness.  The full
  solver phase is 40/40 expected in 9,758.178 s (SAT 4,473.787; UNSAT
  5,284.391): 0.950487x the preceding candidate, 0.869489x accepted Tactix,
  and 1.271385x pinned Kissat.  Model/proof checks and zero-residue
  classification continue asynchronously.

## Rank-27 current localization

- Rank 27 is the leading remaining stable search-work witness: current
  homogeneous job `1658170` takes 722.318 s with 12,637,423 conflicts and
  204,832,338 decisions, versus pinned Kissat's 260.013 s, 4,120,691
  conflicts, and 171,577,295 decisions.  Both historical authorities classify
  the instance UNSAT; certificate checks remain authoritative but never block
  trajectory work.
- Full portable logging through conflict 10,000 was stopped as an inefficient
  observer design.  Campaign `20260831-4400-tierd-rank27-trajectory-prefix10k`
  filled its two-million-row cap near conflict 2,000 and is
  observer-invalid/canceled.  Its apparent conflict-1,074 mismatch was a
  Kissat observer defect: a fixed 4,096-byte payload silently dropped one very
  large irredundant conflict while both solvers retained identical analysis and
  learned stages.  The diagnostic-only Kissat overlay now allocates that
  payload by clause size; this is not production evidence or a solver repair.
- Coarse-to-fine campaign
  `20260831-4401-tierd-rank27-learned-prefix100k`, job `1658213`, retains only
  final learned clauses through conflict 100,000 and is valid in both arms.
  Records are exact through conflict 19,382.  At conflict 19,383 only the
  reported decision level differs (23,120 versus 23,122) while cumulative
  decisions and clause hashes remain exact; the first learned-clause content
  difference is conflict 19,387.  Narrow campaign
  `20260831-4402-tierd-rank27-trajectory-window19375-19390`, job `1658215`,
  is valid and exact through decision 6,606,975.  At conflict 19,381 and
  decision 6,606,976 Tactix selects `-284274` while Kissat selects `-291261`;
  the same three variables appear within the next three decisions in different
  order, and no inprocessing event occurs in the window.  Heap campaign `4403`
  is frozen unsubmitted as the priority-two CDCL frontier.  Priority order is
  inferior inprocessing state, then CDCL, then bounded arena/watch physical
  representation convergence.

## Next attribution gate

- After the active validation and checkpoint commit, first classify the largest
  remaining gaps as search-work, matched-work runtime/RSS, or mixed.  Inventory
  implementation-only arena collections and their bytes/time/state effects as
  read-only evidence, not as a presupposed production rewrite.
- After Rank-27 closure and its single bounded consolidation fuzz campaign,
  perform a separate full-codebase revisit: inventory and remove superseded
  macro/env-gated diagnostic paths, collapse obsolete compatibility branches,
  simplify the accumulated documentation, and re-run the architectural and
  regression suites. Keep that cleanup distinct from causal performance fixes
  until the observers needed to validate this repair have been banked.
- Preserve independent immutable proof identities, N1--N4, and unconditional
  Kissat creation order.  Select physical representation convergence only if
  attribution shows it dominates; otherwise resume the leading general search
  mechanism.  Any stage needs focused differential tests and isolated evidence
  before cumulative B+C and homogeneous validation.
- Initial immutable-authority attribution favors search localization: the
  remaining aggregate gap is 2,082.942 s; Ranks 27/31/34 contribute 1,202.346 s
  with 3.067x/2.201x/1.757x conflicts, and adding Ranks 22/37/21 reaches
  1,867.295 s (89.65% of the gap) with material work divergence.  Keep bounded
  representation measurements on matched-work witnesses, but do not promote a
  broad arena refactor ahead of the leading stable search discrepancy.

## Tests and correctness

- Full trace SubSolver oracle passes 100,000 randomized formulas; the complete
  75/75 Trace and 76/76 Release suites pass. Configuration
  wiring has zero failures; campaign pins are hash-checked per task.
- Two observer campaigns were correctly disqualified after their SAT checker
  bundle omitted `modelcheck.py`. Tier-D preparation now pins that runtime
  module with `checkmodel` and fails closed; positive and negative regressions
  cover the recurrence.
- Proof checks are nonblocking for timing but remain authoritative and preserved.
- Fresh campaigns must be all-valid before scoring. Solver execution is
  Slurm-only, one solver per task, immutable pins, no node constraints, with
  model/proof validation and zero-residue cleanup.

## Open work

1. Historical Rank-33 prefix, heap, watch, and early BVE localization is closed
   and retained in its dated reports; the current Rank-33 resume state is the
   compact section above. The
   non-reference `bve_min_bound` selective admission cache is removed;
   `bve_retry` now mirrors Kissat's eliminate flag, including blanket re-arm
   only when the bound advances. The first changed-arm validation (job
   `1656889`) correctly showed that cache removal alone did not change this
   boundary, excluding it as the cause. Kissat clears still-set flags after an
   incomplete elimination round and does not advance the bound.
   Tactix now does the same, with a focused regression fixture. Focused BVE,
   slot-pose, event-trace, and overlay tests pass. The changed-arm successor,
   `20260830-2440-tierd-rank33-bve-incomplete-validation` (job `1656890`), is
   valid but also non-closing: its 84-record trace has the exact same
   `be4763db...` digest as job `1656889`, including the target heap trajectory.
   Both exact lifecycle repairs remain; the next bounded observer summarizes
   every BVE schedule without another expensive full lifecycle trace.
   Its focused tests and both instrumented builds pass. Two-arm campaign
   `20260830-2450-tierd-rank33-bve-schedule-sets` (job `1656891`) is valid in
   both arms. The first schedule is the exact same 429,743-variable external
   set in both solvers (all three fingerprints match at bound 0). This corrects
   the earlier selected-event interpretation and excludes retry-set admission
   as the first-round cause. The first two observed pops match; the next heap
   transition differs. Add ordered topology and score-multiset fingerprints to
   distinguish initial heap construction from dynamic update mechanics.
   Focused tests and both trace builds pass; campaign
   `20260830-2460-tierd-rank33-bve-topology` (job `1656893`) is valid in both
   arms. At the first round, insertion hash `eaf80835b69cd9e3`, topology hash
   `1ad753a321a3f75d`, both score fingerprints, and all set fingerprints match
   exactly. Thus compact order, initial scores, and heap construction are
   excluded. The owner is a dynamic elimination/update difference after the
   initial common pops. Locate its first pop-stream mismatch with cumulative
   block hashes, then open only that ordinal block. Focused tests and both
   trace builds pass; two-arm campaign
   `20260830-2470-tierd-rank33-bve-pop-blocks` (job `1656895`) is valid in both
   arms. Cumulative external-ID hashes are exact through pop 131,072
   (`5811a78766ecde2c`) and differ by pop 135,168, with equal heap size. Open
   only ordinals 131,073--135,168 to identify the first different root.
   Two-arm campaign `20260830-2480-tierd-rank33-bve-pop-window` (job `1656897`)
   is valid in both arms. Pops match through ordinal 132,360: both pop 91,836
   at score 3,999,990.5 and expose 91,843 as root. The next pop differs:
   Tactix promotes 91,837 at score 3,999,991 while Kissat pops 91,843 at
   3,999,990.5. Candidate 91,836's elimination/update path owns the first
   divergence. Two-arm campaign
   `20260830-2490-tierd-rank33-bve-candidate91836` (job `1656899`) reached the
   target in both arms, but its Kissat arm hit the 100,000-row observer cap
   before the stop boundary and is invalid overall. Its completed target
   prefix exposes the mechanism: Kissat marks three duplicate-preserving
   opposite-side binary watches as AND/OR gate members; Tactix marks only two
   because its sparse lookup retains one index per partner. This changes the
   duplicate resolvent and 91,837's score. Tactix now marks every matching
   binary copy, with a direct duplicate-gate regression test. Focused tests
   pass. Changed-arm campaign
   `20260830-2500-tierd-rank33-bve-duplicate-gate-validation` (job `1656901`)
   is valid and closes the boundary: gate counts are 1/3, all four resolvent
   hashes and their order match, 91,837 finishes at 3,999,989, and the next
   pops are 91,843 then 91,845. The implementation uses one linear side scan,
   adds no admission/effort/shape selector, and preserves N1--N4.
   Full Release validation passes 76/76. Uniform Rank 1--40 campaign
   `20260830-2510-tierd-ranks1-40-bve-duplicate-gate` is job `1656902`; it
   pins Release SHA `6ef2c6d5...da9bc7a2`, uses the unchanged 40-rank matrix,
   and reruns no comparator. Solver verdict/timing is provisional and
   nonblocking; proof checkers remain alive to finish independently.
   While that immutable pin runs, the complete pinned-`ands.c` follow-through
   found a complementary edge: one unrelated duplicate stays antecedent and
   later copies become gate members after Kissat consumes the literal mark.
   The sparse index now retains the first physical copy and reproduces both
   required- and unrelated-partner duplicate semantics in one scan. A focused
   content-sensitive BVE test passes. This second fix is deliberately absent
   from job `1656902` and awaits its own release/cumulative validation.
   The follow-through also found that pinned Kissat accepts a physical large
   AND base shortened by root assignments to pivot plus one input when the
   other side has at least two distinct binary partners. Tactix's equivalence
   detector cannot cover that physical-large case. AND detection now carries
   Kissat's distinct-partner precondition and accepts the shortened base; a
   focused resolvent test and the full 76/76 Release gate pass. The cumulative
   follow-up Release SHA is `0ea7b7ca...a529994` (diff `9287df9d...fcbc33c`).
   Job `1656902` is solver-complete: all 40 provisional verdicts are expected,
   total 10,524.426 s. Versus job `1656827`, total is 1.02909x and geomean
   1.06488x; versus accepted Tactix, total is 0.93776x; versus pinned Kissat,
   total is 1.37122x. Rank 33 improves 776.358→469.639 s and Rank 36 stays
   stable at 384.756 s; Rank 19 regresses 39.330→261.796 s through a much
   longer search trajectory but remains faster than Kissat's 739.902 s.
   Eleven proof checkers were still running at this snapshot and remain alive.
   Uniform cumulative follow-up campaign
   `20260830-2520-tierd-ranks1-40-bve-gate-complete` is job `1656942`. It
   pins Release SHA `0ea7b7ca...a529994`, uses the identical matrix, compares
   reporting-only to job `1656902`, and contains no selector or comparator
   rerun. Provisional timing remains independent of proof-check completion.
   Validation correction: jobs `1656902` and `1656942` omitted the adjacent
   `pins/modelcheck.py` module required by the pinned `checkmodel` wrapper.
   Their 22 SAT checks therefore fail infrastructurally; exact stdout/models
   were deleted with task scratch. Both campaigns are observer-only, are not
   acceptance candidates, and their records will not be overwritten. UNSAT
   proof checks remain valid and continue. The next cumulative campaign must
   pin `modelcheck.py` SHA `f70f0eef...1676cf` before submission and is the
   only candidate eligible for all-valid acceptance.
   Job `1656942` is solver-complete with 40/40 expected verdicts but remains
   observer-invalid for the SAT packaging issue. Total is 10,553.166 s:
   1.00273x job `1656902`, 0.94033x accepted Tactix, and 1.37496x Kissat;
   geomean is 0.98045x, 0.94173x, and 1.34647x respectively. Ranks 19, 31,
   32, and 35 improve materially; Rank 37 regresses 420.727→747.713 s and
   Rank 33 regresses 469.639→639.537 s, while Rank 36 stays at 384.237 s.
   Acceptance-eligible campaign
   `20260830-2530-tierd-ranks1-40-bve-physical-definition` is job `1656982`.
   It pins Release SHA `d304e8e0...a691f0e5` plus `drat-trim`, `checkmodel`,
   and `modelcheck.py`; the runner hash-checks all four before every task. It
   uses the identical uniform matrix, no selector, and no comparator rerun.
   Tracked Tier-D tooling now pins `checkmodel` and `modelcheck.py` as one
   checked bundle and fails closed when the runtime module is absent. The
   direct pinning regression and campaign-certificate-cleanup tests pass.
   Superseded observer jobs `1656902` and `1656942` retained 11/18 and 9/18
   verified UNSAT proofs respectively. Their remaining proof-only tasks were
   then canceled: missing SAT models already made acceptance impossible, and
   those tasks were holding seven replacement-campaign ranks pending. Job
   `1656982` was untouched; all of its remaining ranks started immediately.
   A subsequent `definition.c` audit found that Kissat removes only the pivot
   when exporting a physical large occurrence to Kitten; root-false literals
   remain subsolver variables. Tactix incorrectly reused resolution's stripped
   image and could prove a definition absent in Kissat. Definition import now
   reads the physical clause without allocating a duplicate image, and LRAT
   lifting starts from that same physical original. A fixture distinguishes a
   SAT physical environment from its artificial UNSAT stripped image; focused
   and full 76/76 Release tests pass. Release SHA is `d304e8e0...a691f0e5`.
2. Rank 34 causal report is
   `benchmarks/reports/2026-08-30-rank34-sat-divergence.md`; final revisit is
   `benchmarks/reports/2026-08-31-transitive-unit-rank1-40-codebase-revisit.md`.
   Jobs `1657550`/`1657552` localize the second conflict-7,705 TransitiveReduce
   pass and its only excess deletions. Job `1657554` proves Tactix deferred two
   failed-source units that Kissat commits immediately. The unconditional rule
   repair retains prior tombstones, emits native-LRAT BFS chains, and reaches
   root/H per source. Job `1657559` makes all four binary snapshots exact and
   closes the two units and end counts. Checked job `1657561` improves Rank 34
   1,014.550→804.709 s. Uniform job `1657564` solves 40/40 but regresses total
   9,305.953→10,573.791 s; retain fixes and localize downstream trajectory.
3. Required Claude-owned fuzz remains blocked by private-workspace access.
   Do not inspect or replace Claude fuzz artifacts. The user authorized the
   fuzz checkpoint, but the external-disclosure safeguard still requires a
   direct statement authorizing transmission of private repository,
   diagnostic, and performance context to the Claude API. The attempted owner
   launch after campaign `4562` was rejected before Claude or any fuzz job
   started. Do not work around that boundary; continue Rank-27 localization
   without waiting for unrelated proof checking and request the narrower
   authorization at handoff.
   Residual localization campaign `4563`, job `1658901`, compares only four
   selected Factor-mark histories at conflict 68,537 using the repaired Tactix
   pin and the now-complete Kissat Factor-pop clear observer; no proof or full
   search stream is involved.
   Campaign `4563` is 8/8 valid/clean after one unchanged singleton retry for a
   0.012-second endpoint miss. All four literals clear identically at the first
   Factor. Tactix alone re-arms `-479842` and `-230203` through `add_binary` at
   conflict 68,537; Kissat alone re-arms `39220` and `-234176` through
   `new_binary_clause` at conflicts 19,360 and 68,537. This remains an inferior
   inprocessing binary-publication discrepancy, not CDCL, score, or activity.
   Campaign `4564`, job `1658910`, retains only the exact binary pair touching
   each witness so the publication transactions can be compared directly.
   Its all-task criterion is invalid/clean because representation-only Tactix
   publications correctly have no physical `binary_add` event. The causal rows
   show `-479842` and `-230203` were rearmed only by
   `kernel_publish_binary_clause`; pinned Kissat substitutes those binary watch
   directions in place/delayed without `mark_added_literal`. The mirror and
   publication helpers now pass `mark_added_material=false`; ordinary semantic
   binary additions remain true. A direct regression covers both physical
   helper paths and the focused binary, Substitute, kernel, Factor, and trace
   suites pass. Closure campaign `4565`, job `1658918`, compares one fresh
   repaired initial Factor set to immutable Kissat `4558`.
   Campaign `4565` is valid/clean in 99.267 seconds. Tactix's 151,258 members
   are now a strict subset of Kissat's 151,269: 151,258 common, zero Tactix-
   only, 11 Kissat-only, and zero common score mismatches. The publication
   repair closes all 334 remaining excess candidates. Bank its uniform Rank-27
   Release effect next; then classify the 11 missing candidates only if the
   trajectory still needs them.
   Submitted checked Release campaign `4566` as job `1658919` with a uniform
   trace-off binary. Consume its provisional solver result immediately; its
   independent DRAT check is nonblocking and must not be polled or awaited.
   `4566` provisionally returns expected UNSAT in 650.047 seconds with
   9,968,492 conflicts, 217,432,735 decisions, and 710,448 KiB peak RSS. It is
   1.280% faster but 4.266% more conflicts than `4562`; versus pinned Kissat it
   remains 2.500056x time and 2.419131x conflicts. Retain the faithful repair,
   leave DRAT checking unpolled, and resume coarse post-Factor trajectory
   localization before spending detail on the 11 Kissat-only marks.
   Submitted coarse first-Factor accepted-grid campaign `4567` as job
   `1658920`, reusing the hash-verified `4551` harness and immutable 424-row
   Kissat stream. It records no candidate failures or CDCL detail.
   `4567` is valid/clean in 98.829 seconds. The accepted-grid common prefix
   extends from 87 to 102 rows; after common ordinal 5,238, Tactix/Kissat swap
   `51474`/`10054` at ordinal 5,239. Both seeds had exact initial positions and
   scores. Paired campaign `4568`, job `1658921`, records only ordinal 5,238's
   update transaction, the two heap nodes, and the resulting complete heap.
   `4568` is 2/2 valid/clean. All 42 selected update rows are exact; the heaps
   contain 150,735 common members, zero Tactix-only, the same 11 Kissat-only,
   and zero common score mismatches. Yet Tactix/Kissat positions for
   `10054,51474` are `35,8` / `17,35`; after common pops `-177040,-177074,
   -178160`, Tactix pops `51474` while Kissat first visits `-10238`. The missing
   11 candidates are therefore heap-topology relevant. Classify all 11 selected
   mark/addition histories next without general Factor logging.
   Submitted full-width selected lifecycle matrix `4569` as job `1658923` (22
   independent tasks). Each task retains one literal's mark transitions,
   optional exact touching binary additions, and target state only; no proof or
   unselected Factor/search event is enabled.
   `4569` is 22/22 valid/clean. Kissat re-arms ten literals through
   `new_binary_clause` at conflict 68,537 and `39220` through the same source at
   conflict 19,360. Seven of the ten late witnesses have the identical physical
   binary published by Tactix at the same boundary without a mark; the other
   three pairs were physically present earlier in Tactix and only republished
   by Kissat at 68,537. This is one clause-collapse/publication timing family.
   Trace the complete pair lifecycle and source large clause for shared pair
   `(-234176,-234173)` next.
   Submitted paired source-lifecycle campaign `4570` as job `1658945`. It uses
   the existing pair-filtered standalone ledger plus one selected endpoint;
   no global event detail or proof work is enabled.
   `4570` is 2/2 valid/clean and exact: inside Vivify at conflict 68,537,
   Tactix root-flushes source clause `(-234176,-234173,234177)` to the selected
   binary at search tick 478,703,963; Kissat calls `new_binary_clause` for the
   same pair one tick later and marks it. Source audit confirms Kissat marks
   only Vivify's size-two terminal, not larger root-shortened candidates.
   `kernel_flush_root_clause` now accepts a caller-semantic binary-terminal
   flag: sparse collection remains false and Vivify passes true; large-to-large
   rewrites remain unmarked in both cases. Focused kernel/Factor/trace tests
   pass. Closure campaign `4571`, job `1658947`, compares the repaired initial
   Factor set against immutable Kissat. `4571` is valid/clean in 98.918
   seconds. The set improves from 151,258 to 151,265 members and remains a
   strict subset of immutable Kissat's 151,269: zero Tactix-only and only
   `39220,396573,401857,84146` Kissat-only. Thus all seven same-boundary
   Vivify collapses close; the four residuals are the distinct pre-existing-
   binary/republication subcase. Submitted one-run, four-pair lifecycle
   campaign `4572` as job `1658948`; it retains only exact pair/source rows and
   one Factor endpoint, with no global trace or proof work. `4572` is
   valid/clean in 102.231 seconds. Tactix classifies `39220` as a Sweep physical
   tautology at conflict 19,360 and the other three at conflict 68,537, without
   a duplicate binary publication. Pinned Kissat has no known-equivalence
   shortcut: it still poses both Kitten queries, calls `new_binary_clause` for
   both implications, marks all endpoints, then substitutes away the physical
   bridges. Removed Tactix's detection- and emission-time shortcuts so the full
   uniform transaction occurs. A regression clears Factor marks before an
   already-stated equivalence and requires all four to be re-armed; the focused
   Sweep test including 2,000 oracle cases passes under ASan/UBSan (LSan
   disabled only because the sandbox is ptraced). Submitted target Factor-set
   closure campaign `4573` as job `1658949` against immutable Kissat `4558`.
   `4573` is valid/clean but byte-for-byte identical to `4571` (151,265 members,
   the same four missing): the known-equivalence hypothesis is falsified and
   that production/test edit was reverted. The Kissat overlay already records
   source clauses for Sweep large-to-binary collapses; use it on all four pairs
   next to distinguish coincident target binaries from equivalence bridges.
   Submitted that single pinned-Kissat four-pair source run as campaign `4574`,
   job `1658950`; no global event stream or proof work is enabled. `4574` is
   valid/clean in 183.763 seconds and emits zero large-collapse source rows.
   Instead each selected pair is added with one pre-existing copy, marked,
   classified as a dense duplicate, deleted, and then substituted away. These
   are duplicate equivalence bridges, not coincident large-clause targets.
   Added a mirrored exact-equivalence selector to both observers; focused trace
   and overlay-static tests plus `git diff --check` pass. Submitted early/late
   representative comparison `4575` as four-task job `1658951`.
   `4575` is 4/4 valid/clean and both selected representatives emit zero Sweep
   equivalence/commit rows in both solvers, so the pairs are not generated by
   Sweep's detector. The remaining `new_binary_clause` source is global
   Substitute's large-to-binary branch, which is followed by the observed
   dense deduplication. Added an exact source hook there; overlay static test
   passes. Submitted one-run four-pair source campaign `4576` as job `1658955`.
   `4576` is valid/clean in 202.254 seconds but also emits zero selected source
   rows, ruling out global Substitute's large-collapse branch. Added an exact-
   pair pre-constructor site row to all remaining semantic binary creators
   (Factor divider/quotient, Congruence, Sweep, Substitute, Vivify); the first
   generated overlay build exposed and then fixed a missing-brace diagnostic
   injection around Congruence's watched/unwatched branch. The rebuilt overlay
   and static test pass. Submitted call-site campaign `4577` as job `1658956`.
   `4577` is valid/clean in 200.990 seconds with zero invalid rows, stderr, or
   residue. Every residual pair is published by Congruence with one copy
   already live: `(-91462,39220)` at conflict 19,360 and
   `(-91474,84146)`, `(-485153,396573)`, `(-489930,401857)` at conflict
   68,537. Each publication creates a second watched copy, marks its endpoints,
   and is then removed by dense duplicate handling. Source parity isolates the
   general defect: Tactix's representative-union helper published only missing
   pairs, while pinned Kissat publishes both representative clauses on every
   union. Repair that lifecycle generally, including independent native-LRAT
   identities, then rerun the exact initial Factor set.
   Repaired the representative-union lifecycle generally: every union now
   publishes both fresh watched binaries even when an identical pair is live;
   native LRAT proves a duplicate from its pre-existing immutable identity.
   A new DRAT/LRAT regression requires endpoint rearming, fresh ids, and the
   subsequent first-copy dense deduplication. Congruence, kernel, and Factor
   pass under ASan/UBSan, and the trace-enabled Congruence/observer tests pass.
   Submitted exact initial-set closure campaign `4578` full-width as Slurm job
   `1658957`, using the immutable `4558` reference set and unchanged endpoint.
   `4578` is valid/clean/exact in 99.265 seconds: 151,269 members, zero
   Tactix-only or Kissat-only literals, zero position/score mismatches, and an
   identical complete ordered heap after normalizing the solver label. Stderr
   and scratch residue are zero. This fully closes the inherited first-Factor
   membership and topology discrepancy. Rerun the bounded accepted-grid stream
   next, then bank the resulting Rank-27 Release trajectory.
   Submitted bounded accepted-grid campaign `4579` full-width as Slurm job
   `1658958`, using the same repaired pin and immutable 424-row Kissat stream.
   It emits no candidate failures, general CDCL events, proof, or model.
   `4579` is valid/clean/bounded in 97.925 seconds. All 417 Tactix accepted
   grids match the first 417 Kissat rows exactly, extending the common prefix
   from 102 through ordinal 5,553. Tactix then ends while Kissat accepts seven
   more grids, beginning with seed `121992` and members `121992,-121992` at
   ordinal 5,554. Stderr and residue are zero. Bank Release performance now;
   concurrently use a selected post-5,553 heap/candidate observer to distinguish
   late heap exhaustion from a candidate-admission mismatch.
   Submitted selected-seed campaign `4581` full-width as Slurm job `1658959`.
   It retains only seed `121992`'s admission, remainder, candidate/source, and
   fresh rows at conflict 68,537 plus the Factor endpoint; unrelated seed rows
   are suppressed before payload construction. No proof/model work is enabled.
   `4581` reached the endpoint cleanly in 122.333 seconds but is criterion-
   invalid: seed `121992` is popped four times, not once, and unrelated heap
   rows exhausted the 20,000-row cap before any selected detail survived. The
   four admissions show the seed remains live; the first/fourth pops are armed,
   ruling out late heap exhaustion. Added and tested a seed-only diagnostic
   allowlist that rejects generic search/heap rows before emission. Submitted
   immutable retry `4582` full-width as Slurm job `1658961` to classify all
   armed passes without changing solver behavior; no proof/model work is
   enabled.
   `4582` is endpoint-clean and untruncated in 112.094 seconds, but contains no
   selected seed/detail row at conflict 68,537. Thus the four global admission
   rows in `4581` belong to other Factor calls: the target call ends before it
   reaches `121992` as an armed seed. Together with the exact initial heap and
   417 accepted/update transitions, this makes the coarse effort boundary the
   next discriminator. Compare only total Factor tick spend/allowance at the
   shared call before opening another candidate stream.
   Submitted paired coarse effort campaign `4583` full-width as Slurm job
   `1658962`. Tactix emits only its Factor slot reference/allowance and final
   spend; Kissat emits one selected Factor rule-journal spend row. Both stop at
   the first conflict-68,537 Factor endpoint with no proof/model work.
   The Tactix half reaches that endpoint in 99.625 seconds: reference
   126,301,766 search ticks, handed 6,315,088 ticks, spent 6,315,095 ticks,
   `CUT`, 417 grids. Its requested slot observer also emits documented `[slot]`
   inventory rows that the harness over-strictly labels unexpected; preserve
   the immutable run and use the intact measurement while Kissat completes.
   The Kissat arm is valid/clean in 208.411 seconds and spends 6,315,703 ticks
   for 424 grids—only 608 ticks (0.0096%) beyond Tactix's spend, yet seven more
   exact grids. This rules out a material dose gap and candidate rejection, but
   608 ticks could still be either a small reference/allowance difference or
   cumulative scan accounting. Align cumulative tick meters at the 417 exact
   fresh events to distinguish those explanations without logging every seed.
   Added a disabled-by-default cumulative meter to the existing fresh event in
   both observers; focused Tactix and overlay-static tests pass, and the fresh
   payload is unchanged unless explicitly requested. Submitted paired campaign
   `4584` full-width as Slurm job `1658964`, bounded to at most 1,000 rows per
   arm and the first conflict-68,537 Factor endpoint.
   `4584` is 2/2 valid/clean in 97.723/175.760 seconds. All 417 common grid
   payloads are exact and cumulative ticks remain exact through ordinal 5,159.
   At ordinal 5,160 (`145,-150280`) Tactix first gets 132 ticks ahead; the next
   grid raises the excess to 287, and it grows to 43,602 by the final common
   grid (6,312,119 vs 6,268,517). This proves an inferior Factor accounting
   mechanism, not an allowance difference. Split only cumulative first-factor,
   next-factor, matching, and pop counters at these same fresh checkpoints.
   Added symmetric disabled-by-default cumulative component counters to that
   bounded fresh observer: heap pops plus first-factor, next-factor-selection,
   and factorize-next matching ticks. Focused Factor/N1/event-trace tests and a
   freshly generated Kissat overlay build pass. Prepared immutable paired
   campaign `4585` and submitted it full-width as Slurm job `1658966`; it
   preserves the exact `4584` endpoint and row cap and emits no per-watch,
   candidate, CDCL, proof, or model stream.
   `4585` is 2/2 valid/clean in 98.775/176.809 seconds. Across all 417 common
   exact grids, heap-pop, first-factor, and next-factor-selection counters are
   bit-exact. The complete accounting excess is factorize-next matching: +132
   ticks first appears at ordinal 5,160, becomes +287 at 5,161, and reaches
   +43,602 at the final common grid—exactly the total-tick deficit. Narrow the
   selected `(145,-150280)` match traversal and its physical dense-watch order;
   CDCL, allowance, seed scheduling, and next-factor choice are ruled out at
   this boundary.
   Added a selected-pair standalone traversal observer after that coarse
   localization. For only `(145,-150280)`, it records each large remainder's
   normalized identity, least-degree scan literal, dense degree, charged large
   visits, and match outcome, then stops at ordinal 5,160. Both trace binaries
   build and focused tests pass; submitted immutable paired campaign `4586`
   full-width as Slurm job `1658969`.
   `4586` is harness-criterion-invalid but artifact-clean and causally exact:
   both solvers return 0 at the selected fresh row, but the parser looked for
   `event|payload` in a tab-separated file. All 2,291 common large traversal
   rows are exact. Tactix alone scans 15 rows, every one already containing
   chosen factor `-150280`; their 30 scan-charge ticks plus 102 visited-clause
   ticks equal the complete +132 deficit. Kissat `new_quotient(next)` marks the
   next factor before `factorize_next`; Tactix marked it afterward. Moved that
   uniform factor mark before matching and added a direct effort regression.
   Focused Factor, N1, event-trace, generated-overlay/static, and diff checks
   pass after the repair. Submitted paired closure campaign `4587` full-width
   as Slurm job `1658971`, using the same bounded 424-row component stream as
   `4585`; exact payload and every cumulative accounting component are
   required.
   `4587` is 2/2 valid/clean in 99.327/176.857 seconds and closes all seven
   missing grids: all 424 accepted payloads plus pop, first-factor, and match
   meters are exact. Only the final grid is 15 next-factor ticks below Kissat.
   Tactix had an extra budget precheck on the inner growth loop; Kissat enters
   `next_factor`, charges the first current quotient occurrence, then checks
   its limit. Removed that extra precheck; validate bit-exact terminal effort.
   Focused tests remain green. Submitted final paired component closure
   campaign `4588` full-width as Slurm job `1658973`; it requires exactness of
   all 424 payloads and every cumulative meter.
   `4588` is 2/2 valid/clean and bit-exact in 98.825/176.707 seconds: 424/424
   payloads and zero total/pop/first/next/match meter mismatches. Both terminate
   at 6,315,703 total ticks, 1,390 pops, 579,820 first ticks, 2,976,254 next
   ticks, and 2,759,629 match ticks. The Rank-27 first-Factor gap is closed.
   Current Release validation passes all 69 executable/proof/model tests and
   all six separately rooted source-conformance checks; the queued doc-size
   cleanup remains separate. Bank Rank-27 Release performance now.
   Submitted uniform checked Rank-27 Release campaign `4589` as Slurm job
   `1658975`. Consume its provisional solver record as soon as it appears;
   never wait for its independent DRAT checker.
   `4589` provisionally returns valid UNSAT in 636.741 seconds, 10,402,831
   conflicts, and 219,843,659 decisions. That is 1.498x time / 2.016x conflicts
   versus partial-Factor `4580`, and 2.449x time / 2.525x conflicts versus
   pinned Kissat. The general Factor lifecycle repair remains causally required
   and bit-exact, but this full trajectory is not promotable on performance.
   Resume coarse post-Factor boundary comparison from the newly exact state;
   its proof checker remains nonblocking.
   Submitted paired campaign `4590` as full-width Slurm job `1658976`, from
   conflict 68,537 through the next Probe
   entry. Tactix uses its O(1) outer macro journal and ordinal stop; pinned
   Kissat uses the tested constant-stat boundary journal and its immutable
   conflict-92,422 endpoint. The full-width two-task diagnostic emits no
   verdict, proof, or model and will establish the first coarse post-Factor
   control split before any detailed CDCL logging.
   `4590` is 2/2 valid/clean in 86.660/89.761 seconds. Probe begin/end and
   restart entries at conflicts 69,725 and 70,795 are exact in conflicts and
   decisions. The first coarse mismatch is plain CDCL before Reduce at
   conflict 71,983: Tactix has 23,097,363 decisions versus Kissat's 22,762,227
   (+335,136) while the conflict clock still agrees. Their next Probes are
   conflict 92,420/27,695,380 decisions and 92,422/26,752,166 respectively.
   Add an O(1) sparse conflict checkpoint over only 70,795--71,983, then narrow
   the earliest differing subwindow; do not reopen inprocessing yet.
   Added the symmetric disabled-by-default checkpoint to the existing
   standalone journals. It records only scalar counters every requested
   conflict interval and performs no clause, watch, heap, or variable scan.
   The focused Tactix event-trace tests, Kissat overlay-static test, both trace
   builds, and diff checks pass. Submitted full-width paired campaign `4591`
   as Slurm job `1658979`, with interval 32 over conflicts 70,795--71,983 and
   a Reduce-boundary stop.
   `4591` is 2/2 valid/clean in 77.442/83.452 seconds with exactly 38 rows per
   arm. Conflict/decision counters and propagation deltas are exact through
   conflict 70,987. At conflict 71,019 Tactix is two decisions and one
   propagation behind after subtracting the pre-existing constant propagation
   accounting offset. The first genuine CDCL split is therefore inside only
   conflicts 70,987--71,019. Resample that bracket at interval one before
   enabling semantic decision or analysis detail.
   Submitted paired campaign `4592` as full-width Slurm job `1658981` with the
   same immutable pins and O(1) journal, now sampling every conflict from
   70,987. It retains the known conflict-71,983 Reduce stop for a clean
   endpoint; causal comparison stops at the first differing row no later than
   conflict 71,019.
   `4592` is 2/2 valid/clean in 77.391/83.303 seconds. There are no missing
   checkpoint conflicts before the split. Conflict/decision state and
   propagation deltas are exact through conflict 71,011. The next checkpoint,
   71,012, has Tactix three decisions and four propagations behind Kissat.
   Capture only the decisions and learned transaction for the single
   71,011-to-71,012 CDCL transition; this is now narrow enough for semantic
   trajectory logging.
   Submitted paired campaign `4593` as full-width Slurm job `1658983` with the
   same trace pins. It enables only
   the existing portable trajectory stream for conflicts 71,011--71,012,
   suppresses watch and heap detail, and stops at the conflict-71,012 analysis
   end. The 50,000-row cap safely covers the roughly 5,115 intervening
   decisions without tracing earlier search.
   `4593` is harness-criterion-invalid but causally complete and artifact-clean:
   Tactix returned 0 at the diagnostic endpoint while the parser accepted only
   SIGTERM. Both traces reach conflict-71,012 analysis end, are untruncated,
   structurally valid, and have empty stderr. The first 2,112 decisions are
   exact. At decision 20,756,027 / level 6,864 Kissat alone chooses `-81001`;
   Tactix goes directly to the next common choice `161063`. Both later reach
   the identical ordered conflict clause and identical size-3 learned hashes,
   with Tactix three levels lower. This first split is locally favorable to
   Tactix but changes the later trajectory. Compare the complete heap and
   assignment state after common decision 20,756,026 before attributing the
   omission to propagation or heap topology.
   Submitted paired one-shot heap campaign `4594` as full-width Slurm job
   `1658986`, at conflict 71,011 after common decision 20,756,026. It records
   each stable-heap variable's exact position, score, assigned bit, and active
   bit, then exits immediately. This will show whether external 81,001 is
   already assigned in Tactix or whether the first choice difference is heap
   membership/topology.
   `4594` is 2/2 structurally valid and clean in 164.616/116.839 seconds, but
   its generic decision-counter selector captures adjacent native moments:
   Kissat increments the counter before selection and fires during lazy root
   popping; Tactix selects before `Decide` increments and fires after
   assignment. Common `-244219` is consequently assigned only in the Tactix
   row. Do not claim the observed 78 position differences or two Kissat-only
   assigned members. External 81,001 is active and unassigned in both adjacent
   states, excluding propagation as its omission owner. Static source audit
   confirms both stable selectors use the same lazy assigned-max pop. Trace
   only heap lifecycle events involving 81,001 and its swap partners through
   the first divergent choice.
   Submitted paired campaign `4595` as full-width Slurm job `1658988`, using
   the existing heap-vars-only observer for external variables 81,001,
   261,907, 80,894, 27,820, 33,950, 244,219, and 161,063. It retains only their
   insert/remove/lazy-pop/root lifecycle and the conflict-71,012 terminal row,
   with no full heap, formula, or watch scan.
   `4595` is invalid/clean and was cancelled after both arms saturated the
   10,000-row cap before conflict 19,000: high-score 81,001 participates in
   nearly every bubble move. Both result parsers report truncation, empty
   stderr, no verdict, and no residue. Take no target-window lifecycle claim.
   Replace per-move logging with the existing O(1) selected-variable
   score/position sink attached to sparse conflict checkpoints.
   Added a symmetric conflict-from filter to that existing score sink; focused
   Tactix event-trace tests, overlay-static checks, both trace builds, and diff
   checks pass. Prepared paired campaign `4596` to sample external 81,001's
   score, position, value, and active bit every 32 conflicts from 68,537 to the
   conflict-71,983 Reduce endpoint. Each row is O(1); no global trace opens.
   Submitted the two immutable arms full-width as Slurm array job `1658990`.
   This diagnostic produces no proof and has no checker dependency.
   `4596` is 2/2 valid/clean in 78.644/85.455 seconds with 108 scalar
   checkpoints per arm. External 81,001's score, value, active bit, decision
   counter, and heap position are exact through conflict 69,017. At checkpoint
   69,049 only its position differs, 121,147 in Tactix versus 121,148 in
   Kissat; score remains byte-identical at `5.8843976939486929e-300`, value is
   zero, active is one, and decisions remain exact. By conflict 70,969 its
   score has underflowed to zero and the physical positions differ widely,
   preceding the known choice split. This points to heap evolution/equal-score
   pose rather than score bumping or propagation. Narrow 69,017--69,049 to one
   conflict, then retain only selected-variable heap events in that conflict.
   Prepared paired campaign `4597` with the same immutable pins, one-conflict
   scalar checkpoints beginning at 69,017, and the unchanged clean Reduce
   endpoint. It remains O(1), trace-only, full-width, and proof-free.
   Submitted its two arms as Slurm array job `1658992`.
   `4597` is 2/2 valid/clean in 79.249/85.105 seconds. The last exact common
   checkpoint is conflict 69,036. At 69,037 both arms have exactly 20,712,235
   decisions and external 81,001 retains the identical score
   `5.8843976939486929e-300`, value zero, and active bit one, but its position
   is 60,573 in Tactix versus 30,286 in Kissat. The split is confined to the
   single 69,036-to-69,037 transition.
   Found and repaired a symmetric diagnostic defect before the lifecycle
   follow-up: heap-vars-only callbacks bypassed their configured conflict
   window, explaining `4595`'s early cap. Selected heap insert/remove/pop and
   root-assignment rows now honor the inclusive window in both Tactix and the
   Kissat overlay. Focused event-trace and overlay-static tests pass; this is
   disabled trace behavior only and does not alter production search.
   Prepared paired campaign `4598` with fresh immutable trace pins. It retains
   only external 81,001 heap lifecycle at conflicts 69,036--69,037, stops at
   the latter analysis end, and rejects any lifecycle row outside the window.
   Submitted both arms full-width as Slurm array job `1658994`.
   `4598` is 2/2 valid/clean in 110.793/110.546 seconds, with 35/36 selected
   heap rows and zero lifecycle rows outside the window. The first 14 heap
   payloads are exact (native decision counters differ by the already-known
   observer timing offset). At payload 15, while lazily moving 81,001 down
   from the former heap tail, Tactix selects child 444,366 at position 27,419
   with score `4.588641434069082e-299`; Kissat selects child 281,577 at the
   same position with score `1.9971289143098594e-299`. Both paths reconverge
   and initially leave 81,001 at position 109,682, then both move it to
   60,573. Kissat alone subsequently bubbles it from 60,573 to 30,286 while
   demoting held variable 449,881, exactly explaining the scalar split. Thus
   the causal predecessor is a latent neighboring heap-topology difference,
   not 81,001's score/update. Trace positions of 444,366, 281,577, and 449,881
   backward with sparse O(1) checkpoints.
   Prepared six-arm campaign `4599`: each solver independently samples one of
   those three neighbors every 32 conflicts from the exact post-Factor
   boundary. The array is homogeneous, full-width, and proof-free.
   Submitted all six independent arms as Slurm array job `1658996`.
   `4599` is 6/6 valid/clean. Tactix arms take 78.593--79.045 seconds and
   Kissat arms 85.408--85.607 seconds. The leading neighbors 444,366 and
   281,577 are exact at conflict 68,921 (positions 877 and 7,024 while
   assigned). At conflict 68,953 both are unassigned with unchanged exact
   scores and exact decision totals, but their positions are exchanged:
   Tactix 54,840/109,678 versus Kissat 109,678/54,840. Variable 449,881 stays
   exact until checkpoint 69,049, confirming it is downstream. Narrow the
   68,921--68,953 exchange for 444,366 and 281,577 to one conflict.
   Prepared four-arm campaign `4600` with one-conflict scalar checkpoints for
   those two variables from conflict 68,921. It reuses immutable trace pins,
   is full-width, and creates no proof.
   Submitted all four arms as Slurm array job `1659006`.
   `4600` is 4/4 valid/clean in 79.096--79.101 seconds for Tactix and
   85.210--85.360 seconds for Kissat. Both variables are assigned and absent
   from both heaps at conflicts 68,929 and 68,930. At 68,931 they are
   unassigned and reinserted in both, but land in the exchanged positions from
   `4599`; scores, values, activity, conflicts, and decisions are exact. Both
   backtrack implementations scan the retained trail suffix forward and push
   an absent stable variable immediately on unassignment. Therefore the next
   bounded trace should compare the two variables' assignment, lazy-pop, and
   backtrack-reinsertion order during conflicts 68,928--68,931; a different
   reinsertion order would be inherited from trail/propagation order rather
   than heap arithmetic.
   Prepared paired campaign `4601` to retain only 444,366/281,577 heap and
   assignment lifecycle during conflicts 68,928--68,931. It stops at the
   latter analysis end and rejects lifecycle leakage outside the window.
   Submitted both arms full-width as Slurm array job `1659010`.
   `4601` is 2/2 valid/clean in 111.445/110.941 seconds, with 25 selected
   lifecycle rows per arm and zero leakage. All 23 lazy-pop payloads are
   identical after ignoring the established native decision-counter offset.
   At the conflict-68,931 backtrack, Tactix reinserts 281,577 then 444,366;
   Kissat reinserts 444,366 then 281,577. The two general heap/backtrack
   mechanisms are otherwise the same, so the physical heap split is inherited
   directly from opposite trail order. Trace only these variables' implication
   provenance over 68,928--68,931; if neither is implied, inspect the matching
   selected decisions instead.
   Prepared paired imply-only campaign `4602` for exactly those two external
   variables and that four-conflict window. Zero rows is explicitly a valid
   decision classification; the trace stops at conflict 68,931 and is proof-
   free.
   Submitted both arms full-width as Slurm array job `1659012`.
   `4602` is 2/2 valid/clean in 115.651/116.704 seconds. Both selected
   implications occur in the same batch at conflict 68,928, decision
   20,699,396, level 2,295, with identical ternary reasons. Tactix emits
   281,577 from `(178705,281567,281577)` and then -444,366 from
   `(-444366,173503,178705)`; Kissat emits those same implications in the
   opposite order. This is the physical trail-order cause of the later heap
   split. Capture the watch list scanned by propagated literal -178,705 in
   that conflict to confirm the two reason clauses' physical order and inspect
   their watch lifecycle.
   Prepared paired one-scan campaign `4603`: at conflict 68,928 it retains
   only watch entries for the propagated literal -178,705, including clause
   pose and values, then stops at the next analysis endpoint. The harness
   requires both exact ternary reason sets and no out-of-window row.
   Submitted both arms full-width as Slurm array job `1659014`.
   `4603` is 2/2 valid/clean in 117.455/121.862 seconds. The physical target
   order exactly matches implication order: Tactix has the 281,577 reason
   before the -444,366 reason; Kissat has the reverse. Tactix's watched list
   also has nine entries versus Kissat's four: five additional live large
   entries remain, all skipped by satisfied blocking literals at this scan.
   This is a concrete inferior watch-lifecycle/physical-order divergence.
   Before tracing individual moves, compare a one-shot selected-list digest at
   the exact conflict-68,537 post-Factor `probe_end` boundary to decide whether
   the difference is inherited from inprocessing or created in later CDCL.
   Prepared paired campaign `4604` to emit exactly one `probe_end` row at
   conflict 68,537 with the selected 178,705 large-watch count and ordered
   hash, then exit. This is one bounded list scan and no proof.
   Submitted both arms full-width as Slurm array job `1659016`.
   `4604` is observer-invalid/clean: both global-trace arms timed out at the
   fixed 240-second solver cap with zero data rows, empty stderr, no verdict,
   and no residue. The target-only emitter still pays disproportionate
   pre-filter snapshot work. Do not enlarge the cap. Add a symmetric disabled
   standalone selected-watch sink to the existing macro boundary journal so
   only the chosen list is scanned once at `probe_end`.
   Added that symmetric disabled standalone sink and repaired the selected-heap
   observer so its detail conflict window is honored. Native behavioral and
   generated-overlay static tests pass. Prepared paired campaign `4605` with
   fresh immutable binaries: it scans only watched external 178,705 at the
   exact conflict-68,537 `probe_end`, exits immediately, produces no verdict
   or proof, and accepts an empty selected list as a meaningful observation.
   This distinguishes an inprocessing-owned physical watch divergence from a
   later CDCL watch-move divergence without repeating `4604`'s global work.
   Submitted both arms full-width as Slurm array job `1659018`.
   `4605` is 2/2 valid/clean in 79.447/89.917 seconds with zero residue.
   At the exact post-Factor boundary, Tactix has 29 large watches on external
   literal 178,705; Kissat has none. The two future ternary reasons are already
   first/second among Tactix's large entries. Thus the conflict-68,928 trail
   order is inherited from an earlier physical watch lifecycle and the
   inferior discrepancy is already visible at an inprocessing boundary.
   Sample the matching `probe_begin` before instrumenting internal phases: if
   it is exact, localize within this Probe; if not, walk back to the preceding
   macro interval.
   Prepared paired campaign `4606` on the same immutable pins to snapshot only
   the exact matching conflict-68,537 `probe_begin`, again with an immediate
   diagnostic exit, no verdict/proof, and zero-residue requirement.
   Submitted both arms full-width as Slurm array job `1659020`.
   `4606` is 2/2 valid/clean in 76.491/86.557 seconds with zero residue.
   Both solvers begin the exact Probe with the same three large watches, in
   identical physical order and clause pose. Therefore the 29-versus-0
   `probe_end` split is created inside this Probe. Add one bounded selected-list
   count per internal phase boundary, then zoom into the first differing phase.
   Added a symmetric, disabled phase-series sink that emits only total/large
   counts for the selected list at the exact chosen conflict. Native and
   generated-overlay tests/builds pass. Prepared paired campaign `4607` to
   capture the conflict-68,537 `probe_begin` through `probe_end` series; its
   observer performs about 38 tiny selected-list scans and no global snapshots.
   Submitted both arms full-width as Slurm array job `1659022`.
   `4607` is 2/2 valid/clean in 79.047/90.617 seconds, with 62 bounded rows
   per arm and zero residue. Probe begin through Backbone is count-exact.
   The dense-entry hooks straddle flush (Tactix after, Kissat before) and are
   not comparable. Post-reconnect resume ordinal 0 is exact at three large
   watches; ordinal 1 is the first comparable difference, Tactix 32 versus
   Kissat 31. The first Vivify ends 9 versus 10, and the second ends 29 versus
   zero. Capture the complete selected list at resume ordinal 1 to identify the
   one clause/lifecycle difference before following the later amplification.
   Added a symmetric ordinal selector to the one-shot list sink; focused tests
   and both generated trace builds pass. Prepared paired campaign `4608` to
   dump exactly Vivify post-reconnect ordinal 1 and then stop at `probe_end`,
   retaining complete canonical clause sets/physical poses but no proof.
   Submitted both arms full-width as Slurm array job `1659024`.
   `4608` is observer-invalid/clean: Kissat emitted 31 selected entries, while
   Tactix emitted none because the full one-shot sink was only routed through
   outer macro boundaries, not internal slots. Both arms reached the endpoint
   with empty stderr and zero residue. Take no paired causal claim. Route the
   existing disabled sink through Tactix slot boundaries and rerun fresh.
   Routed it through those slot boundaries; native behavioral and overlay
   static checks pass. Prepared fresh paired replacement `4609` with the same
   exact conflict/event/ordinal selector and clean endpoint contract.
   Submitted both arms full-width as Slurm array job `1659026`.
   `4609` is 2/2 valid/clean in 80.351/91.119 seconds with zero residue. All
   31 common entries are identical in canonical set, physical order, and
   clause pose. Tactix alone retains one final live entry for
   `(-178720,-145,173499,173503,178705)`. Determine at the same reconnect
   whether Kissat retains that clause elsewhere, marks it garbage, or never
   constructs that image; this is the first concrete Vivify lifecycle delta.
   Added a symmetric ordinal selector to the existing bounded formula-set
   snapshot; focused tests/builds pass. Prepared paired campaign `4610` to scan
   the arena once at that reconnect, retain only clauses containing variable
   145, and report both live and garbage records with values. It exits later at
   `probe_end` and creates no proof/model artifact.
   Submitted both arms full-width as Slurm array job `1659028`.
   `4610` is 2/2 valid/clean in 81.417/92.082 seconds with zero residue. The
   target five-literal clause exists live and redundant in both arenas with the
   exact same canonical set and every literal value zero. Therefore Kissat did
   not delete it: the one-entry watch difference is strictly a physical
   clause-pose/reconnect difference. Narrow to that hash's ordered pose and the
   selected occurrence counts that drive Vivify sorting.
   Added a selected canonical-hash/pose mode to the bounded formula snapshot
   and a standalone five-literal occurrence-count sink at Vivify schedules.
   Focused tests and generated builds pass. Prepared paired campaign `4611` to
   emit exactly one target clause row plus eight tiny count rows per solver,
   stopping at `probe_end` without a proof or verdict.
   Submitted both arms full-width as Slurm array job `1659030`.
   `4611` is 2/2 valid/clean in 81.402/94.971 seconds with zero residue.
   Selected occurrence counts are exact for schedule rows 0 and 1. At
   reconnect ordinal 1, Tactix poses the target in strict descending-count
   order `(-145,178705,173503,173499,-178720)`, while Kissat preserves
   `(-178720,173499,178705,173503,-145)`. Thus Tactix commits the precise sort
   to a clause Kissat did not admit to that redundant band. Capture the target
   glue/priority/pose before each of the eight sorts to locate the band
   classification discrepancy.
   Extended the bounded counts sink to append one selected live clause's
   size/redundancy/glue/priority/searched/pose before each sort. Focused tests
   and generated builds pass. Prepared paired campaign `4612`, retaining five
   counts plus this single target lookup at each of eight schedules and no
   proof/verdict.
   Submitted both arms full-width as Slurm array job `1659032`.
   `4612` is 2/2 valid/clean in 81.953/95.671 seconds with zero residue. The
   same live redundant target clause has glue 4 in Tactix and glue 5 in pinned
   Kissat at every one of the eight Vivify schedules. Its initial pose and all
   five selected occurrence counts are exact through the first two schedules.
   Consequently Tactix admits and sorts it in the tier-2 band, one band before
   Kissat admits it in tier 3; Kissat first raises its Vivify priority at
   schedule 2 while Tactix has already permuted the clause at schedule 1. This
   replaces the apparent reconnect-order symptom with a concrete pre-sort
   metadata cause. Determine whether the target already exists with different
   glue at `probe_begin`; if not, isolate the pre-Vivify rewrite that creates
   it, then trace the originating glue transition backward.
   Added a symmetric selected-hash metadata mode to the bounded formula-set
   snapshot. It reports glue, Vivify priority, searched/used counters, reason,
   shrink state, and pose from one arena scan. Release behavioral and overlay
   parity tests pass. Prepared campaign `4613` with four immutable arms at
   exact `probe_begin` and first `vivify_begin` boundaries; submitted it
   full-width as Slurm array job `1659038`. The unrelated user job `1657786`
   remains running and untouched.
   The first Kissat pin was harness-invalid because compile-time `--quiet`
   removed the established runtime `-q` option; tasks 1 and 3 exited in 0.007
   seconds before solving and produced no evidence. Retained those outputs,
   rebuilt the unchanged overlay with the standard `--statistics`
   configuration, and resubmitted only those two immutable arms as unthrottled
   array job `1659042`. The original Tactix arms continue unchanged.
   Both original Tactix arms then reached the 240-second cap on node017 before
   conflict 68,537, more than three times the established diagnostic time,
   with empty stderr and no formula/boundary evidence. Retained those
   endpoint-missing outputs and resubmitted only tasks 0 and 2 unchanged as
   unthrottled array job `1659044`.
   Corrected the diagnosis of those Tactix arms: their pin was the ordinary
   Release binary, which compiles EventTrace out, so it could neither emit nor
   exit at the requested boundary. Cancelled only job `1659044`; user job
   `1657786` remains untouched. The two corrected Kissat arms are valid/clean
   in 92.271/92.373 seconds: the target is absent at `probe_begin`, then live,
   redundant, in-place shrunken, glue 5, used 30, priority 0, and searched 2 at
   first `vivify_begin`. Do not claim a paired `4613` result; combine this
   one-sided bracket with pre-repair `4612`, which already records Tactix glue
   4 at the first Vivify schedule.
   Source comparison exposes the exact known deviation: canonical Kissat
   `substitute.c` preserves glue on an in-place large-clause shrink, while
   Tactix applied CaDiCaL's size-minus-one clamp and maps this clause's glue
   5 to 4. Repaired only canonical Substitute to preserve the old glue;
   Sweep-connected substitution retains its own Kissat-matching clamp. Added a
   six-to-five redundant-clause regression. Focused Substitute and Sweep tests
   pass; trace-enabled Release rebuild and post-repair boundary validation are
   in progress.
   Trace-enabled Release behavioral, Substitute, Sweep, and overlay-parity
   tests pass. Prepared paired campaign `4614` to capture the exact target at
   first Vivify entry from the repaired Tactix and pinned Kissat; submitted it
   full-width as Slurm array job `1659046` with no proof or verdict endpoint.
   `4614` is 2/2 valid/clean in 81.001/92.721 seconds with zero residue. At
   first Vivify entry the target is exact in both solvers for live/redundant
   state, size 5, glue 5, used 30, priority 0, searched 2, and pose
   `(-178720,173499,178705,173503,-145)`. The causal metadata discrepancy is
   closed. Tactix has `shrunken=0` after its compaction while Kissat retains
   `shrunken=1` in the original allocation; classify that as a remaining
   representation-only lifecycle difference, after trajectory validation.
   Full Release validation is 75/76; every solver/proof/model/N1/scheduling/
   API/conformance test passes, and the sole failure is the already-recorded
   documentation-size cleanup.
   Prepared checked uniform Rank-27 Release campaign `4615` for the repaired
   default binary, with provisional UNSAT publication before independent DRAT
   checking. Submitted as Slurm job `1659051`; consume provisional timing
   immediately and never wait for its checker.
   Prepared paired post-repair campaign `4616` to remeasure the target's eight
   Vivify schedules, five occurrence counts, glue/priority/searched state, and
   pose. Submitted full-width as Slurm array job `1659052` while Release runs.
   `4616` is 2/2 valid/clean in 81.552/95.026 seconds with zero residue. All
   five occurrence counts and target glue/searched/pose are exact across all
   eight schedules. At sequence 2 Kissat's observer sees priority 1 while
   Tactix's earlier observer site sees 0 immediately before setting it; by
   sequence 3 priority and sorted pose are exact and remain exact. Treat this
   as observer-site timing with no downstream divergence. The former one-band-
   early sort and extra-watch symptom are closed.
   Prepared paired O(1) per-conflict scan `4617` over the former 70,987--71,983
   CDCL window to determine whether the conflict-71,012 split is closed or
   moved. Submitted full-width as Slurm array job `1659054`; no formula/watch/
   heap scan, proof, or verdict is enabled.
   `4617` is 2/2 valid/clean in 76.642/86.561 seconds with 850 checkpoints per
   arm and zero residue. Every conflict-indexed decision counter and
   propagation delta is exact from 70,987 through the common conflict-71,983 /
   decision-22,762,227 endpoint. The former conflict-71,012 CDCL split is
   closed, not shifted. This proves the apparent CDCL/heap divergence was a
   downstream consequence of the inferior canonical-Substitute glue rewrite.
   Prepared paired coarse macro campaign `4618` from the conflict-68,537 Probe
   through the next Probe, with causal comparison resuming after the now-exact
   conflict-71,983 Reduce. Submitted full-width as Slurm array job `1659056`;
   no detailed search or state scan is enabled.
   `4618` is 2/2 valid/clean in 93.518/100.482 seconds with zero residue. Macro
   state remains exact through restart conflict 72,138 / decision 22,762,634.
   The first later coarse mismatch is plain CDCL before the next restart:
   Tactix reaches 75,058 / 24,789,556 while Kissat reaches 75,048 /
   24,803,202. At the next Probe Tactix is 381,712 decisions above Kissat,
   improved from the pre-repair 943,214 excess but not closed. Bracket only the
   72,138--75,058 search interval coarsely next.
   The checked Release campaign `4615` publishes an immediately actionable
   provisional UNSAT result in 553.315 seconds, 7,323,751 conflicts, and
   196,160,457 decisions. This regresses from the pre-repair `4580` result
   (425.067 seconds / 5,159,503 conflicts) and remains 2.13x pinned Kissat time.
   The local Substitute/Vivify correction is causally required but exposes or
   amplifies a later trajectory split, so it is not yet a performance closure.
   Its independent certificate checker is pending and is not being awaited.
   Prepared paired sparse campaign `4619` over the last exact restart at
   conflict 72,138 through the common Reduce at conflict 76,779, sampling only
   O(1) search counters every 32 conflicts. This is the coarse-first bracket
   for the first remaining plain-CDCL divergence; no semantic/full logging,
   verdict, proof, or model artifact is enabled.
   Submitted `4619` full-width as Slurm array job `1659058` with no throttle or
   node constraint. Continue causal work from its result; never idle on the
   unrelated `4615` proof checker.
   `4619` is 2/2 valid/clean in 80.694/88.762 seconds with 146 scalar rows per
   arm and zero residue. Counters are exact through conflict 72,395 / decision
   22,873,787. At conflict 72,426 Tactix has 44 extra decisions and three fewer
   propagation-counter increments. Prepared interval-one campaign `4620` over
   that 31-conflict bracket; it retains the shared conflict-76,779 Reduce only
   as a clean diagnostic stop and emits fewer than 4,400 scalar rows.
   Submitted `4620` full-width as Slurm array job `1659060`, with no throttle,
   node constraint, proof, verdict, or detailed semantic stream.
   `4620` is 2/2 valid/clean in 80.249/88.168 seconds with zero residue and
   fewer than 3,800 rows per arm. State is exact through conflict 72,397 /
   decision 22,879,620. At conflict 72,398 Tactix has made 46 fewer decisions,
   has one extra propagation increment, and has one fewer unassigned variable.
   Prepared bounded trajectory campaign `4621` for only this one conflict
   transition, suppressing watch and heap detail and capping output at 30,000
   rows.
   Submitted `4621` full-width as Slurm array job `1659062`; it has no proof,
   verdict, node constraint, throttle, or formula-state dump.
   `4621` is 2/2 valid/clean in 169.148/121.737 seconds with zero residue. The
   conflict-72,397 clause, learned clause, backjump, and first five decisions
   are exact. At decision 22,879,626 both arms choose external variable 170225
   and both saved phases are positive, but Tactix's target is negative while
   Kissat's is positive; the decisions are therefore `-170225` and `170225`.
   Both later derive the same conflict-72,398 and learned clause, 46 levels
   apart. Prepared phase-only campaign `4622` to locate the last selected-
   variable target snapshot write; it suppresses every clause/watch/heap/
   decision/formula stream and remains under a 30,000-row cap.
   Submitted `4622` full-width as Slurm array job `1659064`, with no throttle,
   node constraint, proof, or verdict.
   `4622` is harness-invalid/clean: both arms hit 300 seconds at only conflicts
   2,860/2,033 despite writing just 405/117 rows. Global event dispatch from
   startup, not output volume, caused prohibitive overhead; no lifecycle claim
   is taken. Prepared corrected `4623`, activating the identical phase-only
   observer only from the already exact conflict-68,537 post-Vivify boundary
   through the target pick.
   Submitted corrected `4623` full-width as Slurm array job `1659066`, with no
   throttle, node constraint, proof, or verdict.
   `4623` is again harness-invalid/clean: both arms time out at 300 seconds
   before the selected pick. Source inspection identifies the symmetric cause:
   `target_state` ignored the conflict detail window and serialized more than
   3,300 pre-window snapshot rows. Partial rows show both targets negative just
   after conflict 68,537 but are not used causally. Repaired both disabled-by-
   default observers so target state uses the existing detail-window predicate;
   trace-enabled Tactix and Kissat overlay parity tests pass. Prepared `4624`
   with freshly rebuilt immutable pins and the same conflict-68,537 window.
   Submitted `4624` full-width as Slurm array job `1659068`, with no throttle,
   node constraint, proof, or verdict.
   `4624` is also harness-invalid/clean: after the target-state window repair,
   both arms still hit 300 seconds at the same conflicts while emitting only
   42/18 selected phase rows. This isolates prohibitive per-assignment selected-
   phase filtering. Abandoned the global trace. Added opt-in target/saved and
   target/best high-water scalars to the existing standalone score journal;
   it performs one O(1) lookup only at sparse checkpoints. Both observer tests
   and builds pass. Prepared `4625` at interval 32 from conflict 68,537 to the
   common conflict-76,779 Reduce.
   Submitted `4625` full-width as Slurm array job `1659071`, with no throttle,
   node constraint, global trace, proof, or verdict.
   `4625` is partial-valid/clean: Kissat reaches the intended conflict-76,779
   endpoint in 90.466 seconds with 258 checkpoints and 334 phase rows; Tactix
   stops at the first Reduce at conflict 71,983 because the reused event-only
   selector omitted ordinal 1. The O(1) observer cost is normal. Prepared
   unchanged-pin retry `4626`, selecting Tactix's second Reduce only; no paired
   phase claim is taken from `4625`.
   Submitted `4626` full-width as Slurm array job `1659073`, with no throttle,
   node constraint, global trace, proof, or verdict.
   `4626` is 2/2 valid/clean in 83.401/90.917 seconds with 258 checkpoints,
   333/334 phase rows, and zero residue. Both targets are negative through
   conflict 72,377. By conflict 72,409 Kissat's target is positive while
   Tactix's remains negative through the endpoint. At conflict 68,537 the
   targets match, but Tactix's target high-water is already 664 assignments
   above Kissat's. Prepared interval-one `4627` over conflicts 72,377--72,409
   using the same O(1) journal and pins.
   Submitted `4627` full-width as Slurm array job `1659076`, with no throttle,
   node constraint, global trace, proof, or verdict.
   `4627` is 2/2 valid/clean in 83.357/90.372 seconds with zero residue. The
   target matches through conflict 72,396. At conflict 72,397 Kissat advances
   its high-water to 135,472 and saves variable 170225 positive; Tactix's stale
   135,700 high-water suppresses that save despite the matching positive saved
   phase. Source/evidence audit locates the stale value at Reduce compaction
   conflict 62,709: compact population/current assigned match, but Tactix
   recounts 663 extra root-fixed immutable phase entries before collapsing its
   import image. Repaired `heuristics_compact_random_order_fixed` to recompute
   target/best watermarks over the collapsed compact order. Focused Release and
   trace tests pass. Prepared `4628` to validate from the repair boundary
   through the former split with unchanged O(1) phase checkpoints.
   Submitted `4628` full-width as Slurm array job `1659078`, with no throttle,
   node constraint, global trace, proof, or verdict.
   `4628` is partial-valid/clean: Kissat reaches conflict 76,779 in 90.769
   seconds with 440 checkpoints and 1,751 phase rows; Tactix stops at conflict
   71,983 in 78.996 seconds because ordinal 2 selected its third observed
   Reduce rather than the required fourth. The first recount removes the 663
   immutable-root excess, but leaves Tactix target/best watermarks one above
   Kissat at conflict 62,709 (135,037/134,966 versus 135,036/134,965). Source
   audit shows Kissat's `compact_best_and_target_values` counts only active
   flags and therefore excludes its retained fixed representative. Refined the
   general recount to exclude retired and root-fixed variables; the regression
   now models three live plus one inactive fixed compact slots. Release and
   trace focused tests pass. Retry through the fourth Reduce with new pins; no
   former-split closure claim is taken from `4628`.
   Submitted unchanged-pin causal retry `4629` as full-width Slurm array job
   `1659080`, selecting Tactix's fourth Reduce (ordinal 3). It carries no
   throttle, node constraint, global trace, proof, model, or verdict.
   `4629` is 2/2 valid/clean in 83.651/90.161 seconds with 440 scalar
   checkpoints per arm and 1,747/1,751 phase rows. All 440 conflict/decision
   tuples and all 440 sampled target/saved/target-assigned/best-assigned tuples
   are exact; post-Factor propagation increments are exact after their
   representation-local constant offset. Both recount 135,036/134,965 at the
   conflict-62,709 repair boundary. Across the former split both move from
   target -1 at conflict 72,373 to target/saved +1 with watermarks
   135,701/135,701 at conflict 72,406, and both reach conflict 76,779 at
   decision 25,164,642. The variable-170225 phase/decision divergence is
   closed. Bank with the complete Release suite, then measure Rank 27.
   Full Release validation passes 75/76 tests: all 69 executable/proof/model
   tests and all six source-conformance checks pass; only the separately queued
   documentation-size gate fails. Submitted checked uniform Rank-27 Release
   campaign `4630` as Slurm job `1659082`. Consume its provisional solver
   result immediately and never wait for its independent certificate checker.
   `4630` provisionally returns expected UNSAT in 363.325 seconds, 4,215,088
   conflicts, and 163,165,809 decisions. Versus `4615`, this is 34.3% faster
   with 42.4% fewer conflicts; versus prior best repaired `4580`, it is 14.5%
   faster with 18.3% fewer conflicts. The remaining gap to pinned Kissat is
   only 2.29% in conflicts but 1.397x in time. This is a large, bankable
   trajectory convergence result; implementation/inprocessing cost is now the
   dominant visible Rank-27 gap. Do not wait for the running checker. Perform
   the authorized consolidation-fuzz workflow if its private-context boundary
   is satisfied, then continue cost attribution or the next residual trajectory
   split as evidence dictates.
   The required single Claude-owned consolidation fuzz is now the next gate.
   Its bounded candidate is Release SHA
   `03e4006c3a273407f3785cee75af0476f6bbe6cadeef821ec732b7dc53dcec76`;
   scope is the repaired compact active-phase lifecycle plus the retained
   Factor/dense-image consolidation corpus, with proof/model/status/invariant
   oracles, immutable snapshots, cleanup audit, and the §8.2 physical-core
   contract. The bounded generated leg is the standard `--quick` 10,000-seed
   campaign at 50 physical cores/50 workers (100 SMT CPUs, 400,000 MiB), plus
   the fixed retained 27-case dense-image replay and a targeted compact-phase
   reachability scenario. Acceptance requires zero crash/sanitizer/invariant,
   proof, model, I/O, or unexplained differential failures; every corpus case
   must terminate, generated coverage/nontermination and achieved concurrency
   must be reported, and successful cleanup must leave zero snapshot residue.
   Harness/oracles are unchanged, so their prior self-test remains applicable;
   Claude must rerun it if preparation changes either. The user granted the
   required disclosure authorization and the Claude-owned campaign
   `20260901-4631-compact-active-consolidation-fuzz` (Slurm jobs
   `1659083`/`1659084`/`1659085`) is a terminal PASS on the candidate Release
   SHA `03e4006c3a273407f3785cee75af0476f6bbe6cadeef821ec732b7dc53dcec76`,
   whose byte-exact reproduction from the submission tree was verified before
   launch. All three legs are COMPLETED 0:0 with zero crash, sanitizer,
   invariant, proof, model, I/O, or differential failures: the quick leg ran
   the 27-entry corpus plus 10,000 generated cases (9,708 verdicts; all eight
   required rules fired), the random-policy leg replayed all 27 fixed corpus
   entries cleanly including the five previously-crashing dense-image entries
   004/005/015/017/020, and the targeted compact-reach leg (3,000 cases,
   distinct seed base, 10 s budget) proved reduce-time VariableCompaction
   reachability in 12 cases (plus 22 in quick) with zero failures. All 5,271
   generated UNSAT verdicts were natively LRAT-checked and all 7,375 SAT
   models independently verified. Cleanup left zero snapshot residue, and the
   retained 4476 failed-job pin was removed after its failure class
   revalidated as fixed. Evidence: the campaign manifest under
   `benchmarks/runs/20260901-4631-compact-active-consolidation-fuzz/`.
   The fuzz gate is closed; continue Rank-27 residual cost
   attribution, then B+C and homogeneous Rank 1--40 validation.
   Post-fuzz coarse attribution shows the residual Rank-27 gap is no longer
   mainly search work. Current Tactix uses 163.166M decisions and 2.307B
   propagations versus Kissat's 171.577M/2.666B, yet spends 213.637 s in
   search/scaffolding versus Kissat's 168.64 s and 149.434 s inprocessing
   versus 88.77 s. Peak RSS is 733,632 versus 298,820 KiB. Prepared paired
   full-run source profile `4632`: its Tactix `-O3 -DNDEBUG -g` `.text` is
   byte-identical to validated Release, and the comparator is accepted
   current-source Kissat. Use flat coarse owners before adding new
   collection/watch instrumentation.
   Submitted `4632` full-width as Slurm array job `1659090`, with no throttle,
   node constraint, proof, model, global trace, or behavior-changing observer.
   Both tasks fail before staging because this cluster does not export
   `SLURM_TMPDIR`; no solver or profile ran and no artifact exists. Repaired
   only the harness scratch allocation to use `mktemp` under `/tmp` with an
   explicit file/rmdir trap. Retry unchanged pins and profile contract.
   Submitted unchanged retry as full-width Slurm array job `1659092`.
   Opportunistic inspection also finds `4615` complete: independent
   `drat-trim` verifies its 2,653,786,156-byte certificate in 3,167.491 seconds
   with exit 0; the proof and derived artifacts were deleted. No work waited
   on that checker. `4630` checking remains nonblocking.
   Release validation passes 69/69 executable/proof/model tests and all six
   source-conformance checks. The sole remaining failure is the already-queued
   documentation-size cleanup. Submitted checked uniform Rank-27 Release
   campaign `4580` as Slurm job `1658960`; consume its provisional solver result
   immediately and leave its independent DRAT checker entirely nonblocking.
   Its provisional result is valid UNSAT in 425.067 seconds and 5,159,503
   conflicts, versus repaired predecessor `4566` at 650.047 seconds and
   9,968,492 conflicts and pinned Kissat at 260.013 seconds / 4,120,691
   conflicts. This is a material 34.6% time and 48.2% conflict reduction, but
   1.63x Kissat time remains. The certificate checker is still pending and is
   not on the critical path.
   Later opportunistic inspection finds `4580` complete: independent
   `drat-trim` verifies the 2,047,235,412-byte certificate in 3,043.193 seconds
   with exit 0; all derived artifacts were deleted. No work waited on it.
4. Andrew report `2026-08-31-0140-transitive-unit-and-rank1-40-revisit`
   records the Rank-34 closure and homogeneous validation. Required gates pass;
   record remote publication only after independently verifying it.
5. Historical Rank-20 random-index and Rank-34 TransitiveReduce closures are
   retained; their dated reports own the full causal evidence and validation.
6. After Rank-27 convergence and its consolidation fuzz, perform a separate
   full-codebase revisit: inventory and remove obsolete macro/env-gated paths,
   simplify duplicated mechanisms and documentation, and restore the progress
   document size gate. The user reaffirmed this track on 2026-09-01. Do not mix
   this cleanup with causal trajectory repairs.

## Architectural contract

<!-- track-sched:begin -->
- Kissat creation order is the unconditional production default.
  `TACTIX_ARENA_ORDER=1` is an explicit CaDiCaL diagnostic arm only.
- Unsupported formula-dependent profiles stay removed. No formula, size,
  density, benchmark, identity, or rank gate may replace them.
- A sound general repair is retained through downstream regression while the
  discrepancy is localized. Do not mask it with a shape profile.
- Preserve N1 soundness, N2 proof accountability, N3 resource discipline, and
  N4 reference scheduling parity.
<!-- track-sched:end -->

## Immutable authorities and gates

- Accepted source watermark: `bad5cbf7548c6abf1181d36150206ac6f52c316f`.
- Accepted binary SHA: `a3f26ae0...f6859`.
- Fresh accepted/Kissat authority:
  `20260829-1930-tierd-ranks1-40-accepted-fresh`, 80/80 valid.
- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: matched Tier B/C Kissat coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26, and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; then T5 best effort.
# Current Rank-27 priority (2026-09-01)

- Exact input-state tracing localized the first inferior post-Factor divergence
  to Strengthen effort accounting at conflict 29,694. Tactix checks candidate
  309,201 and deletes it using binary `(-484045,-396530)`; Kissat discovers and
  schedules the same clause but reaches its `forward_steps` limit before
  checking it. Replace the old optional partner-visit/mid-candidate budget with
  Kissat's uniform forward-step currency and between-candidate cutoff, then
  validate Rank 27. Campaign `4482` is observer-invalid/clean for its original
  paired-result criterion but causally decisive steering.
- After Rank-27 convergence and the already-running bounded consolidation fuzz,
  revisit the whole codebase separately: remove obsolete macro/env-gated paths,
  consolidate mechanisms, and simplify/compact documentation.
