# Progress and resume guide

## Current stage

**Committed candidate: c2.4 = `3361d3b51`** (branch `candidate-c2.4`, Release `57bdd51c…`; user
decision 2026-10-09; [gates](benchmarks/reports/2026-10-09-candidate-c2.4-gates/README.md)): c2.3
plus API-gap Steps 1–4a and the [restart-only retained services](benchmarks/reports/2026-10-09-restart-only-services/README.md).
All 263 gate rows solved by both c2.4 and c2.3 search have identical conflicts and certificates;
WV100 and WV2–89 pass; the B28+C5 (1,011.2 s) and Rank1–50 (8,626.8 s) ceiling overruns were
accepted as environmental (same-CPU instructions within 0.2%); fuzz 0 failures in five legs
(graph-labeling slices unreached by broad legs); full Debug 358/358; abw-K-dwt SAT 21.3 s.

**Earlier candidate: c2.2 = `8ce068dbf`** (user decision 2026-10-09): c2.1 plus the startup
FastEliminate guard ([record](benchmarks/reports/2026-10-09-candidate-c2.2/README.md)); adds the
verified noL-11-10 closure and solves noL-11-0 within the gate; c2.1's snw_16_9_Encpre,
openstacks and VexRiscv closures were not rerun on c2.2. Its predecessor c2.1 =
`1aad09b07` (branch `candidate-c2.1`; Release `28995e13…`) met the session goal
(at least 10 verified fixed44 closures on one coherent candidate;
[plan §9.5](DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration)):

- full Debug 353/353; Claude-owned fuzz PASS, 0 product failures;
- B28+C5 accepted (964.3 s ≤ 999.9 s); Rank1–50, WV100 and WV2–89 pass under §8.4
  (5,000 s retention for noL-11-0, noL-11-8 and Circuit_multiplier24; accepted
  exception snw_16_9_CCSpre);
- **13 verified fixed44 closures**: ranks 147, 226, 263, 489, 497, 511, 541, 627, 630,
  899, 991, 1075, 1129;
- [qualification report](benchmarks/reports/2026-10-08-candidate-c2.1-qualification/README.md).

Main's source equals c2.3 (c2.2 plus a behaviour-identical cleanup). Development after c2.1 was
purged from the tree at the user's request and survives only in Git history (last
pre-purge main: `d3a6a6527`). That includes the c3/c4 Satsuma symmetry practice, the
witness-free SR rewrite (`163429b44`), the Kissat start-of-search repair (`5ea5c5df1`;
candidate c5a failed B28+C5, Rank1–50, WV100 and WV2–89 on seven SAT rows),
affine Gauss–Jordan (`f39f59246`) and the FastEliminate bound port (`d3a6a6527`).
Kept: c2.1's evidence, the plan rules its acceptance uses (§8.3 profiling, §8.4
5,000 s retention and the snw16 exception), and the baseline rerun tooling.

## Next action

0. **Candidate c2.3 = `555ecb4b0`** (branch `candidate-c2.3`, Release `ae4a26e4…`) is c2.2
   ([record](benchmarks/reports/2026-10-09-candidate-c2.2/README.md); full Debug 353/353) plus the
   cleanup: graph labeling on by default, trimmed statistics, DefaultPolicy structural Walk seeding
   and the X17 census tool removed. It is byte-identical to c2.2 on 9 identity rows
   (`benchmarks/probes/scratch/cleanup-20261009/`). Persistent affine search stays (plan §9.5
   Persistent XOR), reworked in c2.4 by user decision (natural full restarts only).
   Pending: lec_mult 64 GiB proof rechecks (jobs 1763319–1763321, 1763353;
   `benchmarks/runs/20261009-restart-rework-lec64-*`); record their verdicts in the restart-only
   report.
1. Full Tier D on c2.3 is running (`benchmarks/runs/20261009-tierd-c2.3`, job 1761609; cpu-q,
   4 CPUs, 32 GB, 5,000 s solve, 8 h check, `%125`, certificates on the home quota). When it
   finishes: `tools/bench/tierd_home_campaign.sh status`, then `recheck` the proof-check memouts
   at 64 GB.
2. Close the DefaultPolicy public-API gap following the
   [eight-step plan](benchmarks/reports/2026-10-09-defaultpolicy-api-gap-plan/README.md). Steps 0–1
   are committed and byte-identical to c2.3 on 12 identity rows, with full Debug 355/355
   ([record](benchmarks/reports/2026-10-09-defaultpolicy-api-gap-steps-0-1/README.md)): route
   ratchet `tests/test_default_action_map.cpp` (internal entries 24 → 14), report-only public
   replay, and macro actions through registry dispatch. Step 2 is committed
   ([record](benchmarks/reports/2026-10-09-defaultpolicy-api-gap-step2/README.md)): public and
   controller H publication go through the shared executor, with the additive
   `tactix_set_guidance_publication` (v1 default DISCARD). DefaultPolicy and the controller are
   byte-identical to c2.3, and full Debug passes 356/356. Step 3 is committed
   ([record](benchmarks/reports/2026-10-09-defaultpolicy-api-gap-step3/README.md)): additive
   `tactix_install_parameters`/`tactix_install_material_settings`, and DefaultPolicy's
   classification, tier-pose, BVE-bound and Factor-composite writes become typed installs
   (ratchet internal entries 14 → 12; DefaultPolicy and controller byte-identical to c2.3; full
   Debug 356/356). Step 4a is committed
   ([record](benchmarks/reports/2026-10-09-defaultpolicy-api-gap-step4/README.md)): additive
   `tactix_support` exposes every support transaction DefaultPolicy composes except the
   inprocessing phase bracket, the per-solve resets and execution contexts become recorded supports,
   and the public replay runs the structured-root online steps through `tactix_online_*` with H equal
   after all 198 of them (the default-profile streams replay to the end). DefaultPolicy and the
   controller are byte-identical to c2.3 (12 identity rows) and full Debug passes 357/357. Steps 1–4a
   are gated as part of c2.4. Next: Step 4b (public phase entry/exit, which unblocks the search profiles
   up to their first `ConflictAnalysis`), then Step 6.

## Live work

| Work | Jobs | Status |
|---|---|---|
| Tactix Tier D (c2.3) | 1761609 | running |
| Baseline reruns | (B) 1757933; (C) Satsuma 64 GB recheck 1763099 | running |
| lec_mult 64 GiB proof rechecks | 1763319–1763321, 1763353 | running |
| Quota governor | 1757996 (600 s; hold >750 GB, release <700 GB); watches all of the above | running |

## Verification authorities

| Scope | Disposition |
|---|---|
| Intermediate b26 | Release `b26efc0f…`; Debug 353/353; fails WV2–89; full44 2/44 verified (147, 401). Reference only |
| Prior cumulative fuzz | Claude 4d8f46d8 [terminal report](benchmarks/runs/20261007-owner-policy-cumulative-fuzz/FINAL_REPORT.md): 44,000 broad cases, zero failures, targeted reach incomplete. No current-source acceptance |
| Explicit long64-fixed checker | Claude 49c00a94 [ACCEPTED](benchmarks/runs/20261007-long64-fixed-independent-review/DISPOSITION.md) for bounded explicit-profile use; final Trestle verification is mandatory |
| Native proof transport | [Rejected r9](benchmarks/runs/20261007-native-transport-claude-oracle-owner/requalification-deferral-r9/DISPOSITION.md); use pure Python |

## Standing references and operations

- Keep the approved no-MAB baseline `d170b4a5` and [saved five-arm comparison](benchmarks/reports/2026-10-01-tierd-five-arm-terminal-results.md);
  do not rerun the baseline. Historical accepted gap coverage remains 4/44 on its
  [frozen checkpoint](benchmarks/reports/2026-10-07-0e-four-regressions-terminal/README.md),
  not on the current repaired candidate.
- The noL-free checkpoint's [fourth regression gate](benchmarks/reports/2026-10-07-nol-free-wv2-accepted/README.md)
  completes its earlier four-set acceptance; later behavior changes require fresh
  gates. Plan §8.4 owns the exact regression ceilings and bounded checker policy;
  formal full-verification targets remain distinct from regression censors.
- The latest recorded [Tier-B paired window](benchmarks/runs/20260926-163543-B-am466-split-paired/) predates the no-MAB baseline and is historical diagnostic evidence only.
- Follow plan §8.1 for storage and Slurm. Quota at the purge: 540 GB; earlier audited
  obsolete Debug cleanup reclaimed 44.57 GB allocated. Recheck before builds and
  clean disposable storage aggressively above 700 GB. Protect live pins and evidence.
- [Andrew's handoff report](report-to-andrew/20261007T224148Z-session-handoff/report.pdf)
  has adjacent TeX; the [documentation cleanup report](report-to-andrew/20261007T225802Z-documentation-cleanup/report.pdf) records this follow-up.
- Earlier experiment narratives remain in their primary reports and Git. Old
  handoffs and reports describe their own snapshots; they do not authorize
  restoring retired mechanisms or restarting superseded jobs.

## Recent changes

- Graph labeling's run dose follows its owner (LymphoSAT runs its antibandwidth solver to completion
  before CDCL): the first run is in the initial root window, and every run continues the trajectory
  within the serialized stage doses until it completes, finds a model or is interrupted; later runs
  keep the Eliminate conflict limit and fruitless/fruitful delay. The 50‰ walk share (abw-K-dwt
  4,827 s) is removed: abw solves in 21.3 s before search; 11 non-graph identity rows equal c2.3; full
  Debug 358/358. Candidate c2.4 = `3361d3b51` (`build/pin-restart-c4-rel`), gates not run.
- Both retained root services use Kissat's split: due at the Eliminate NLOG2N conflict limit with a
  delay that halves after a fruitful run and doubles after a fruitless one, and run within a
  `SET_EFFORT_LIMIT` share of search ticks (affine: sweep share; graph labeling: walk share). The
  tick timers and graph labeling's 1M/64M/4 constants are removed.
- Gauss–Jordan is scoped to persistent affine sessions; the default startup root closure keeps
  c2.3's forward-echelon basis (lightsout regression in the first commit). Restart-only services
  [report](benchmarks/reports/2026-10-09-restart-only-services/README.md): identical to c2.3 on 11 of
  12 identity rows and on 9 fixed44 closures; full Debug 358/358.
- Persistent affine search and graph labeling never force a restart: both run only in natural
  full-restart root windows; affine runs only after a root fact on its retained support (or another
  retained-input change). Affine basis is Gauss–Jordan on every sync path, the startup census runs
  after preprocessing, BVE skips the persistent support, and the per-propagation consumption
  observer is removed.
- Compact living docs: archive the SP-JUMP-1 closure record to
  [docs/log](docs/log/2026-10-09-doc-compaction-archive.md); reflow and compact other budgeted docs.
- Remove DefaultPolicy's off-by-default structural Walk seeding and the X17-only census tool.
- Per-run statistics print the persistent affine-search block and `c handoff` only when enabled or
  nonzero (typical run 227 -> 157 `c` lines; parsed lines unchanged).
- Graph labeling (Lucky 26/27 schedule) is on by default; plain `--auto-proof` equals c2.2's qualified
  arguments (identity check: 9 rows byte-identical).
- Restore the startup FastEliminate guard after root-fixing structured symmetry (c2.2).
