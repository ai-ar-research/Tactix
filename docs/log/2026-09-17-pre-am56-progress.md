# Progress before the AM56 cleanup checkpoint

Historical PROGRESS body preserved during the user-requested 17 September
repository cleanup. It records intermediate AM7--AM54 status and qualifications;
[PROGRESS](../../PROGRESS.md) owns the current state. The body below is verbatim.

# Progress and resume guide

**Resumed, 17 September 2026 UTC.** Active goal has two strictly ordered phases.
**Phase 1:** recover independently checked Rank50/50 and B28+C5 33/33, with
aggregate runtimes below R16's8,115.735s and952.263s respectively; repair Rank1/6
overhead through general mechanisms and preserve existing WV61 coverage.
Rank6 has an additional user-required case gate: no slower than frozen Satsuma+Kissat
203.321s total under comparable conditions (hwu,c4,32GiB). Both aggregate gates remain.
After matching that Rank6 target, examine whether its remaining work is unavoidable
and pursue much larger preprocessing savings through exact reuse, cheaper algorithms
and useful-work admission. This deeper audit follows recovery, as requested.
Within Phase 1, prioritize unsolved Rank cases first, then Rank1/6 overhead.
Use cpu-q for diagnostics, profiling and coarse solve/timeout experiments;
use matched regression allocations for close comparisons and final aggregates.
**Phase 2 begins only after Phase 1 passes:** independently checked WV100 100/100
within1,200s and32GiB per case by integrating each remaining row's owning-baseline
techniques into Tactix, preserving Phase 1 gates. No portfolio or benchmark dispatch.
AM11 solver coverage is49/50 (Rank26 timeout), B28+C5 33/33 and WV61 61/61.
Rank aggregate including timeout is15,451.691s; B28+C5 is1,317.498s. Both
performance gates remain open. Late-root scheduling recovers Rank26 in a
checked1,090.825s diagnostic, but loses WV75/77 in the300s screen. AM13 changes
exact binary membership storage to target Rank6 validation cost. See next work.

[DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) owns the goal and gates: independently
checked **WV100 100/100**, with the fixed Satsuma57 intermediate milestone.
Preserve N1–N4, native DSR → forward dsr-trim → LSR → Trestle, original-CNF SAT
model checks, general mechanisms and typed strategy/rule separation. No
benchmark dispatch, sequential portfolio, reference rerun or new Tier-D campaign.

Do not begin further WV100 coverage expansion before the complete Phase 1
performance and independent-validation gates pass. **Aggregate runtime must improve on both sets against the start of
WV100 work; individual search-trajectory slowdowns are acceptable:** pinned R16 (`298c3467`) full native-DSR gates, B28+C5
33/952.263s and Rank50/8,115.735s. Later AM9/SR98 results are diagnostic
comparisons, not a relaxed acceptance baseline. Exact authorities, full vectors
and AM10 comparison: `build/wv100-lymphosat-mechanism-review/am11-start-baseline/baseline-and-am10.json`.

## Authoritative checkpoint

Work only in `/home/hwu/SAT-solving`, directly in the root checkout. The inherited
tree remains dirty. **No commit, promotion, push, source reset or deletion.**

- Current unaccepted candidate: **AM54 native row feedback and coloring ownership transfer**, atop AM51 first-anchor reuse;343files; default stage scheduling unchanged; benchmark/fuzz acceptance pending.
- Authority: `build/20260917-064549-am54-native-row-feedback-authority/manifest.json`.
- Snapshot: `build/source-20260917-064549-am54-native-row-feedback`.
- Matching release A/B main SHA256:
  `cf98d18a8e3f9c2dfed63d325700b55fb2256e8447770e3648a8ac09c62f4e50`.
- AM13 control: `build/20260917-010841-am13-binary-membership-authority/manifest.json`.
- AM11 rollback/control: `build/20260917-002902-am11-pure-closure-authority/manifest.json`.
- Mutable development builds: `build/sr27-fix-and-cut-20260915-0223-{a,b}`.
  Every submitted campaign owns immutable pins; never rebuild them.
- Prior measured candidate AM7:
  `build/20260916-104945-am7-sparse-group-authority/manifest.json` (337 files).
- Accepted foundations: online/N1 refactor `a08f9f6a`, native DSR `20223134`,
  direct SR `298c3467`. Later work remains unaccepted pending full gates.

AM10 retains an occurrence image across a generated PureOnly BVE pass, attaching
it only during each atomic application and selected propagation followup.
Kernel/version changes invalidate it; copied executors rebuild before mutation;
completion/reconfiguration releases it. Existing caller-owned dense mode stays
borrowed. Live redundant large clauses retain sparse BVE's delete-only cleanup.
Affine Lucky and other rules retain sparse public boundaries. The broad diagnostic
prototype disabled affine Lucky; that lifetime was not carried into production.

AM9 adds optional `progress_followups` to the shared typed sequence controller.
Non-followup progress admits a followup; completion consumes credit. Skips do
not mutate state or spend budget; interruption preserves admitted work;
reconfiguration starts a new credit scope while optionally retaining spent
budgets. Actual cleanup progress remains visible to scoped fixpoints and retry
invalidation. DefaultPolicy marks its structured cleanup steps. Rule guards,
explicit applications and the CDCL loop are unchanged. Contracts live in
[implementation notes](docs/implementation-notes.md).

AM10 validation: **159/159 Release passed, 180.33s**; **3/3 focused
ASan/UBSan/leak tests passed, 22.42s**; fixed SAT original-CNF model and UNSAT
native DSR → forward dsr-trim → LSR → Trestle checks passed. Matching A/B builds.
Evidence: `build/wv100-lymphosat-mechanism-review/am10-bve-cache/`.
AM9's 158/158 Release and two sanitizer checks remain in the sibling
`am9-progress-followups/` validation files; the initial sandbox LeakSanitizer
restriction was resolved by rerunning outside that sandbox with leak checks on.

## Rank 17 diagnosis and preservation evidence

[The diagnosis](benchmarks/reports/2026-09-16-rank17-startup-order.md) records
job1697698: base/no-pure/no-affine take158.586/158.564/159.255s, each2,897,870
conflicts; no-compaction/late-root take9.084/9.232s, each169,679 conflicts.
All five models check, and all have zero SR applications. Reduce98 performs
root simplification before early Lucky using24 already-present assignments.
Both paths later have matching formula/root/heuristic fingerprints, but
literal/watch ordering differs. Fingerprints are diagnostic, not equality proofs.

Conditional prototype1697707 restores169,679 conflicts with a checked model
in9.885s. Its WV61 screen1697708 solves61/61 with **all AM7 certificate hashes
unchanged**. The simpler no-compaction control1697769 solves44/61:17 timeouts,
so removing cleanup destroys useful SR behavior. Counts are solver coverage;
independent checking remains required. Prototype metrics and review decisions:
`build/wv100-lymphosat-mechanism-review/am9-progress-followups/prototype-summary.json`
and `am8c-conditional-prototype/review-decisions.md` in its parent.

Claude's bounded design review is terminal under
`build/rank17-cleanup-design-review/`. Adopted its preservation/negative controls;
rejected incorrect claims that late-root never runs or that real cleanup
progress should be hidden from fixpoints. Codex implemented/debugged the change.
Failed diagnostic1697659 is preserved: its tracer indexed lazily absent Active
statuses. Corrected1697698 fixed that diagnostic-only read, first reproducing
it under gdb on four clauses. No production tracer or benchmark selector was added.

## Validation and remaining blockers

| Job | AM10 scope | Resource contract |
|---|---|---|
| 1697987 | Full B28+C5,33 cases | hwu,c12,32GiB,1,200s solver |
| 1697988 | Full Rank1–50 | hwu,c12,32GiB,1,200s solver |
| 1697989 | Satsuma57 + affine gains51/63/64/65 | cpu-q,c8,32GiB,300s solver |

Run prefix: `benchmarks/runs/20260916-201748-am10-bve-cache-`.
`am10-bve-cache/experiments.json` records exact paths and job identities;
`gate-audit.json` records the latest observation. B28+C5 has33 solves/1,374.449s
with all16 UNSAT hashes unchanged; WV61 has61 solves/220.469s with all57 UNSAT
hashes unchanged from AM9. B28rank17 keeps169,679 conflicts (9.085s, checked
model). Rank25 is recovered in293.081s with a checked model versus AM9 timeout;
Rank9/21/26 and the full Rank vector remain pending. Independent proof checking
is also pending. All arrays are full width without node constraints and
own immutable pins. AM9 proof jobs1697830
(B28rank33) and1697861 (Rank22/47) were still checking at AM10 submission;
leave them running. Do not duplicate these campaigns or wait on proof checking
to do independent work.

Resume audit: B28+C5 **33 solves/1,398.532s**; Rank **46/19,051.114s**, with
timeouts9/21/25/26 unchanged. B28 rank17 retains169,679 conflicts and a checked
model in9.132s. WV61 has **61 solves/58 checked**, with all61 certificate hashes
matching both AM7 and the conditional prototype. WV60/61/82 passed dsr-trim but
Trestle exhausted the3,600s pipeline budget; Rank40 again exited137 near32GiB.
Complete vectors, pin audit and historical-reference caveats:
`build/wv100-lymphosat-mechanism-review/am9-resume-20260916/`.

Retained AM7 snapshot at12:00UTC: B28+C5 **33 solves/1,552.486s** versus matched
SR98 **33/1,088.553s**; Rank **46/19,697.773s**, timeouts9/21/25/26, versus SR98
**48/11,430.015s**. AM7 WV61 has61 solves/58 checked;60/61/82 passed dsr-trim but
Trestle exhausted the3,600s pipeline budget. Full-regression acceptance is open.
AM5's disjoint61+39 development screens total79 WV100 solves at300s, not official
1,200-second checked coverage. AM9 has not rerun the39-case complement.

## Next work

**Latest checkpoint:** Root AM54 adds generic, explicitly metered completed-row feedback; the SAT consumer stops once no legal new column can survive, accounting for every seed column, polarity and repeated variables. Backend remains encoding-neutral; legacy no-feedback overloads preserve exact output/status/ticks. Unique coloring ownership removes both AM51 copies.343files/A+B match;161Release tests pass175.39s;39366prefix-oracle comparisons and every-work-prefix tests pass, including ASan/UBSan/leaks;232320generic old/new observations pass in Release and sanitizer;8exact caller matrices and4rootproof/model micros pass. Actual-default paired job1699118 has16solves/10checked; Rank6 models verify753.612→697.056s onnode001, identical22872conflicts/certificate, still far above203.321s target. All8certificate pairs match; B22 observes105.510→65.855s on different nodes, coarse only. No diagnostic shared allowance/early Congruence/BCE imported. AM54 independent owner3376732 is active at build/am54-claude-native-row-feedback/. AM51 owner is TERMINAL:0solver findings,2auxiliary bounded-gdb harness findings dispositioned in its report;2.730million comparisons,2.647million oracle checks,2400checked solver outcomes plus180sanitized; retain callback/OOM/scale/order/typed-driver gaps. Next finish AM54 benchmark/fuzz disposition and integrate explicit non-refilling SR discovery allowance with charged admitted-prefix finalization; then typed early useful-inference admission and AMO-provenance BCE. AM49/52 remaining-regression results and rejected C33 hypothesis remain in the prior frozen report. Both full aggregate gates remain open; preserve all proof jobs. [Evidence/next work](benchmarks/reports/2026-09-17-native-row-feedback.md). AM13 paired Rank6 diagnostic1698296 is terminal: AM11
1,194.001s versus AM13 937.765s, both independently checked SAT, identical native
certificate hash. Exact transversal validation575.732s versus317.388s;
peak RSS4,348,688 versus3,664,312KiB. Same-window cpu-q c12 comparison is21.46%
faster, but remains far above the new203.321s Rank6 acceptance target.
AM14 is diagnostic-only atop AM13, binary643e36dc56eb60d6b31944a4f5d63d9ffe0d42be22f51b9f96222b5a6f6118d1.
Job1698310 is terminal: early Congruence checks Rank6 in916.643s but Rank26
still times out; disabling AMO completion checks Rank6 in929.770s.
Early Congruence proves Rank1 in0.718s (checked), before SR. Disabling AMO
completion solves Rank26 in316.694s (checked), but loses WV75 at300s in1698311;
therefore do not remove AMO completion globally. Early Congruence preserves
WV75/77 in9.480/11.234s with independent native proofs checked. Broader remaining59
WV preservation1698321, `20260917-013251-am14-congruence-wv59`, now has59/59
solver outcomes; checking is ongoing. This plus the two checked companion cases
preserves61/61 solver coverage, with51 independently checked at01:44UTC;
combine with1698311 tasks0/2, which were excluded only to avoid duplicate checks.
No production schedule change yet. Scripts/pins: `benchmarks/probes/scratch/am14-startup-isolation/`.
The first preservation submit hit a stale guard and submitted no job; its partial
unsubmitted directory is retained. The retry verified a fresh live clear guard.
Rank26's LymphoSAT0.066s baseline used its `heule-nol` embedded coloring, per
retained module attribution and source. Do not import the witness or its matcher;
only general structural coloring/repair methods are eligible for integration.

**Factor scheduling is rejected for adoption.** AM15/16 lose WV75/77; AM17/18
restore those two cases but still take291/251s on Rank6. AM19 completes one
Factor pass after a quiet pure/AMO prefix: checked Rank6 186.544s on cpu-q,
then156.000/158.118/158.778s on comparable hwu,c4,32GiB (1698401/03/04).
However full WV61 screen1698405 solves only29/61 at300s. The individual Rank6
target is attainable, but that schedule is not a general fix. Paired bounded
control spread exceeds2%; do not claim an accepted close timing comparison.
Full vectors: `benchmarks/probes/scratch/am18-prefix-factor/current-audit.json`.

**AM21 diagnostic shared SR budget also fails preservation.** Original SR
path plus early Congruence, one existing20b native discovery allowance across
invocations, and group cost debited before exact validation. No Factor reorder.
Job1698469 checks Rank6 SAT210.918s, using20b ticks with0 applied SR; six-case
WV screen1698470 checks all six. WV75/77 use1.874/2.086b. Comparable hwu,c4,32GiB
job1698476 checks Rank6 in194.127/194.778/194.628s: all below frozen203.321s,
median194.628, full range0.3345%; no reference rerun or same-window relative claim.
Full WV61 job1698479 solves60/61: WV88 times out300.006s after exhausting20b
with0 applied SR. Thus the absolute case improvement is not an accepted general fix.
Submission followed an assertion repair and fresh live guard; retain failed scaffolds, no resubmit.
All AM21 artifacts live under `benchmarks/probes/scratch/am21-shared-sr-budget/`.
Its independent `build/am21-base-am13` reproduces AM13's main hash; mutable
root A/B now contain AM54 and must not supply old diagnostic objects.
User's latest direction: add feedback WITHIN a preprocessing pass, extending
productive work and stopping stalls. Current FixAndCut plans the whole chain
before exporting candidates, so an outer progress test misses latent useful work.
AM22 completes admitted prefixes: WV88 now independently checks103.156s/537SR; disjoint1698543+1698580 solve61/61 with own-proof checking ongoing. Full1698551 B28+C5 33/1721.759s and1698552 Rank47/16738.278s (timeouts12/26/36) FAIL gates; no schedule adopted. Investigate incremental certified-prefix publication and useful-work feedback,
plus exact snapshot reuse and wordwise binary checks; do not merely raise the cap. Rank6 hwu1698581 checks188.666/191.737/192.538s, all below203.321 but2.0194% range exceeds2% threshold. Rank12 proof checks1698709 are complete (AM24 early6.875s/noearly17.689s); AM25 restores Rank36. Prioritize Rank26 (AM25 and all3 AM26 arms timeout), AM27/28 are terminal with all4 arms timing out; inspect AMO completion/search, then reduce repeated Rank6 preprocessing. AM25 WV7 checks7/7. AM30 preserves certificates/search with lower unit cost; next profile target is128one-column row completions costing61.325s. AM31 BCE recovers Rank26; narrower AM35 preserves B7. AM36 full gates are now active; do not duplicate them.
Trace1698540 checks Rank6 SAT212.933s; cap stops after258 validated generators, before group planning.
Rank26 times out1200.005s; AM22 preprocessing .084628s, so search is the problem. Nested timings overlap; defaults unchanged. [Evidence](benchmarks/reports/2026-09-17-preprocessing-progress-feedback.md).

AM20 implements typed `before_progress` admission only: own-progress batches
and interruption preserve admission; later rounds retain history; reconfiguration
resets scope while optionally retaining budgets. Defaults have no annotation.
159/159 Release pass232.50s, focused3 pass1.49s, ASan/UBSan/leak1 passes0.12s,
four A/B model/native-proof microchecks pass. AM20 Claude is TERMINAL:306 independent outcomes,0 defects; retain its declared reach/ABI-harness/allocation gaps. AM24 is now TERMINAL:16200 oracle extractions/294 checked solver outcomes,0 defects; actual1698721 used cpu-q (old handoff plan stale). AM29 is TERMINAL:478checked outcomes and one pre-existing Congruence assertion, reproduced on AM24 and fixed by AM34; retain four stale-ASan-recipe comparisons, four ingestion-decline controls and local seeded-preparation qualifications. AM30 is TERMINAL:6000checked outcomes,236474Release comparisons,6250ASan comparisons; no solver defect, three mis-parameterized oracle comparisons void (one harness finding/exit1). Mirrored binary watches are required; asynchronous mid-scan stop remains untested. Reports: `build/am29-claude-watch-order/terminal-report.md` and
`build/am30-claude-orbitopal-marks/terminal-report.md`. AM34 is TERMINAL:2853cases/696checked outcomes,0candidate failures; old-source controls show368assertions and304model-set changes, fixed by root closure. Preserve its local seeded-preparation, inactive budget/interrupt, root-reason and handed-file/isomorph gaps. AM43 campaign1699034 is TERMINAL0findings; its terminal report was read in full. AM51 owner is TERMINAL0solver/2harness findings at `build/am51-claude-first-refinement-reuse/`; AM54 owner3376732 is active at `build/am54-claude-native-row-feedback/`. Read only handoff/terminal/explicit reproducer. AM20 report: `build/am20-claude-before-progress/terminal-report.md`.

AM13 changes only the exact clause index and its tests;
no production schedule or tick change. Dense binary domains use a triangular
bitmap plus duplicate-excess counts only when no larger than the ordinary
hash table. All159 Release tests pass176.11s, focused ASan/UBSan/leak passes27.29s,
and four production/diagnostic model/native-proof microchecks pass. Claude's AM13
terminal report finds no defect:139,873 independent predicate observations,
167 checked native proofs and213 checked models. The targeted addendum is terminal:
both reach gaps closed,684 shipped flat lookups,124 checked outcomes (34 proofs,
90 models),0 solver defects. Its report records a two-job deviation and repaired
harness gates: `build/am13-claude-fuzz-reach-addendum/terminal-report.md`.
Read only handoff/terminal and an explicitly handed reproducer. AM11 is terminal:377 records,0 failures,
including repaired seeds; see `build/am11-claude-fuzz/terminal-report.md` for gaps.
AM13 paired Rank1/6 cpu-q diagnostic1698296 is terminal at
`benchmarks/runs/20260917-011111-am13-binary-membership-pair`. Rank1 checks in
6.926/6.977s with the same proof hash; Rank6 evidence is above.
Rank26 five-arm diagnostic1698277 is terminal: late-root checked SAT1,090.825s,
other four timeout. **Do not adopt global late-root:** cpu-q screen1698301
times out on both WV75/77 at300s. Selected Rank screen1698300 is terminal,
with four checked solves and timeouts on Rank6/9/21/25;
run prefix `20260917-011112-am12-late-root-screen-`. Continue diagnosing a
general scheduling change that preserves useful symmetry discovery. Solver remains general;
only harness chooses inputs/configuration. No production late-root change yet.
Full Rank6 trace1698294 is terminal: inclusive exact transversal validation585.218s,
group planning193.803s, DejaVu detection13.759s. Nested phase totals overlap.
Its perf record header did not finalize on timeout: no usable CPU percentages.
A live gdb sample landed in Tactix binary hash probing. Details, source-pin audit,
current gate vectors and diagnostic preparation are in
`benchmarks/probes/scratch/am13-binary-membership/`. Its audit asserts old AM13;
refresh current root/gates through `am29-watch-order/audit.py` instead.
AM11 full checking continues; latest audit has32/33 B28,45/50 Rank and58/61 WV
checked at the earlier audit. New AM54 report/checkpoint owns current vectors; no aggregate gate passes. Keep all running proof jobs and pins intact.

The following AM11 launch context is retained; the checkpoint above supersedes
its live-process and pending-diagnostic statements.

**Current execution:** AM11 full gates1698142/1698143/1698144 use run prefix
`benchmarks/runs/20260917-002902-am11-pure-closure-`; do not duplicate them.
All159 Release tests pass182.30s; A/B hashes match. AM11 fuzz ownerPID3176502
is live, preparing under `build/am11-claude-fuzz/`; read only its handoff,
terminal report and an explicitly handed reproducer. Do not restart it.
Prioritize unresolved Rank cases: paired300s experiment1698134 recovers
Rank9/21 in213.309/228.438s and cuts Rank25 from295.949 to106.890s, all checked;
AM11 full gates independently repeat those solves. Rank26 remains unresolved.
Diagnostic1698277 tests AM11 base/eager-cleanup/no-pure/no-affine/late-root on
Rank26 with immutable pins and full1200s limits; ten fixed proof/model checks
pass. Trace1698278 isolates Rank1/6 preprocessing components at180s; two fixed
oracles pass. Both were queued at launch; inspect job state and results.
Trace1698278 was canceled while wholly pending and replaced by cpu-q job1698291
(`benchmarks/runs/20260917-004141-am11-sr-cost-cpuq`); it is terminal. Rank1
shows expensive discovery before the decisive Congruence pass. Rank6's180s
trace spends25.024/55.448s in zero-application passes and another56.189s in
zero-matrix structured discovery inside graph SR; its graph has20,832 vertices
and11,126,592 edges. The remaining cost needs full attribution: cpu-q profile
1698294, `benchmarks/runs/20260917-004639-am12-rank6-profile-cpuq`, runs the same
pin with inclusive phase counters and99Hz CPU sampling,1200s solver limit.
Rank26 diagnostic1698277 had already started during migration and remains on
hwu; no running task was canceled. The unsubmitted partial cpu-q Rank26 clone
is retained; no inherited artifacts were deleted.
After unresolved Rank recovery, use those traces to localize overhead before
implementing general effectiveness-based effort control. Shared WorkUnit lacks
SR discovery; refinement/group have separate ticks, main DejaVu search has
separate search/support caps. Tick accounting must cover expensive discovery,
not only applied rules. No adaptive production scheduler is implemented yet.
Diagnostic pin audit and legacy AM10 template-field correction:
`am11-pure-closure/diagnostic-pin-audit.json` in the same review directory.
Latest exact current-state audit: `am11-pure-closure/current-audit.json` under
`build/wv100-lymphosat-mechanism-review/`.

Earlier diagnostic context (superseded execution instructions):

1. Collect paired diagnostic1698134, eight300s tasks on Rank9/21/25/26,
   `benchmarks/probes/scratch/am11-pure-closure/experiment.json`. Both arms have
   the Lucky validation repair; the experimental arm repeats each pure-BVE
   step to its scoped fixpoint before SR. The runtime trace1698122 showed25
   peeling rounds in60s, with repeated roughly0.5s no-op SR scans. Cache reuse
   was confirmed; no redundant-clause fallback appeared. This is a typed
   scheduling experiment, not a production scheduling edit or full gate.
2. Finish validation of `src/rules/lucky/model.hpp` and
   `tests/test_affine_model.cpp`: candidates must satisfy live clauses using
   values publication retains, excluding unassigned retired literals. Claude's
   AM10 report handed seeds36573/62491; both now pass ASan/UBSan/leak and
   invariants using the handed control driver (renamed local no-op cache to
   avoid the new type-name collision). A legal3-variable dense-BVE/redundant
   duplicate/affine composition reproduces the old assertion independently.
   Three focused Release and three sanitizer model tests pass; full Release
   validation is running (`build/am11-lucky-release-all.log`). Evidence:
   `benchmarks/probes/scratch/am11-lucky-boundary/`. Four diagnostic SAT/model
   and UNSAT/DSR oracles pass on the paired binaries. Next candidate needs a
   fresh Claude-owned fuzz checkpoint after its source is finalized.
3. AM10 Claude job1698132 is terminal:363 records, one failed Release record
   with two invariant-aborting chunks (six of eight completed). No model/proof
   rejection or sanitizer error; it reproduces on AM9. Read only its owner
   report and explicitly handed reproducer. Original AM9 harness scoring
   failures are closed by this checkpoint; report coverage gaps stay explicit.
   AM10 full Rank has47 solves/13,980.984s solved-only, three1,200s timeouts;
   its PAR-2 sum is21,180.984s versus R16's8,115.735s. B28+C5 is1.44335x
   R16's runtime. Individual regressions are allowed, aggregate improvements
   on BOTH sets and full coverage are required. Remaining AM10 B28rank33 and
   Rank22/47 proof jobs continue; WV60/61/82 again hit the checker budget.
4. Broader adaptive SR effort is a separate strategy question: existing feedback
   repeats on progress and suppresses unchanged exhausted retries. Consider
   useful producer output per unit work with per-call budgets kept explicit;
   do not silently reinterpret internal limits as a shared total budget.
5. Continue remaining WV100 mechanisms after understanding full regression costs.
   Restart-boundary SR remains lower priority; no new Tier-D campaign.

## Proof/fuzz debt and session hygiene

AM6 Claude job1697640 ended with669 records and three harness-control failures;
84 typed-API UNSAT outcomes lacked certificates. It is not a universal N2 pass.
Owner report: `build/am6-claude-fuzz/terminal-report.md`; qualifications:
`build/wv100-lymphosat-mechanism-review/am6-owner-disposition.md`.
Read Claude handoff/terminal/review reports and explicitly handed bounded
reproducers only; never raw campaign JSONL. Codex retains debugging.

Retained gaps: Rank40 checker exit137 near32GiB; SR122 fresh-random/identity
coverage and500,000-variable nontermination; flat-index11 unchecked cases;
SR66 natural-random Extension and wire-base2 replay reach. The WV80/83/86
parity-proof pilot verifies but Trestle scales poorly; no production XOR
refutation rule is accepted. Evidence: `build/xor-refutation-design/checker-scaling-attribution.md`.

Leave existing solver/proof jobs unchanged. Old allocation1683143 ended
OUT_OF_MEMORY at07:18:27EDT; it was not cancelled by us. Allocation1697679 is
node007. Storage guard2967585 checks every600s, threshold700GiB, **pending
scheduling only**; last observed usage126GiB. Never pause running jobs.
Verify its process/shared lock before restarting after allocation expiry;
`build/storage-guard.state` and `build/storage-guard-resume.json` own its state.
All campaigns use actual anonymous O_TMPFILE scratch, including crash/timeout
cleanup. No benchmark artifacts were deleted during this checkpoint.

Earlier history is in [the structured-SR log](docs/log/2026-09-16-structured-sr-progress.md).
AM7 implementation evidence remains in its frozen sparse-group report. AM9's
[Andrew handoff](report-to-andrew/20260916-1610-rank17-followups/report.pdf)
records the diagnosis, implementation, tests and pending acceptance gates.
The [resume report](report-to-andrew/20260916-1718-am9-resume-startup-cost/report.pdf)
records the full vectors and paired index intervention before AM10. AM10 source
and validation evidence are recorded above; full performance/fuzz gates remain open.
AM10's [technical report](benchmarks/reports/2026-09-16-am10-bve-occurrence-cache.md) and
[Andrew report](report-to-andrew/20260916-202935-am10-bve-cache/report.pdf) record the checked Rank25 recovery.

<!-- track-sched:begin -->
Kissat creation order remains the production default; `TACTIX_ARENA_ORDER=1`
is a CaDiCaL diagnostic arm. Retain sound general repairs through downstream
regressions.
<!-- track-sched:end -->
