# Progress and resume guide

## Current stage

Session goal (user): **at least 10 independently verified fixed44 closures on one coherent
candidate**, preserving N1–N4, with effective Claude-owned fuzz, full Debug and B28+C5,
Rank1–50, WV100 and WV2–89 acceptance. **Nothing is accepted yet.** Scope:
[DEVELOPMENT_PLAN.md §9.5](DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration).

- **c2 = `76e161b24`** (branch `candidate-c2`): fuzz PASS
  ([owner report](benchmarks/runs/20261007-firstopen-original-seed-fuzz-owner/FINAL_REPORT.md)),
  B28+C5 accepted, full Debug 352/353 (test assertion only, fixed `0b7d2bc55`). **11 verified
  closures:** full44 `benchmarks/runs/20261007-fixed44-c2` (147, 263, 497, 627, 630, 899, 991,
  1075) plus the 45,000 s long-check rerun `benchmarks/runs/20261008-fixed44-c2-longcheck`
  (511 sokoban-p16, 541 crafted_n10_d6_c4_num9, 1129 crafted_n10_d6_c4_num8; drat-trim→CakeLPR).
  569 and 654 exceed 32 GB in the checker. Gate rows at 5,000 s on the gate file (§8.4 rule):
  noL-11-0 and noL-11-8 retained; snw_16_9_CCSpre UNKNOWN (faithful-port exception candidate:
  Satsuma emits the same single unit and Satsuma+Kissat is UNKNOWN). Rank401 lost (SAT 7,271 s).
- **Rank569** ([report](benchmarks/reports/2026-10-08-rank569-proof-memory/README.md)): one
  witnessed SR line routes the stream to backward dsr-trim, which needs ~42 GB even for
  Kissat's proof; verified at 128 GB. Fix options (solver witness-free expansion, or a
  split-width checker) await the user.
- **c3 = `b5ae7373f`** (c2 + graph-labeling region fix + Satsuma A–D + one-round pure literals
  `fa6859f1d` + affine run schedule (affine off) + symmetry-use bookkeeping off `8525f7032`):
  full Debug 355/355, zero skips; **all four gates fail**. A–D (`8a5b00658`) is a port defect on
  clique colouring: clqcl_50_6_5 759 → 203 SR applications, 3 s → timeout (Satsuma+Kissat 0.17 s;
  bisection `benchmarks/runs/20261008-bisect-clqcl*`). Also WV100 proof-cost regressions
  (MVRoundRobin, cliquecolouring), B28+C5 ceiling (EDP3 startup +6.9 s with no output,
  Timetable_392 AMO completion). Pure-round shuffle probe: luck, not mechanism
  ([report](benchmarks/reports/2026-10-08-pure-round-shuffle-probe/README.md)). c3 full44 adds
  SAT closures Timetable_C_495 (2,144 s) and lockchart (3,541 s); its 627 proof passes dsr-trim
  (31.3 GB) but Trestle exceeds 32 GB.
- **c4 = `11a26fb64`** (c3 + [Satsuma limit classification repair](benchmarks/reports/2026-10-08-satsuma-limit-classification/README.md)
  `6a2e08215`): renewable Tactix windows no longer count as Satsuma's `reached_limits`; DejaVu
  split limit, shallow-group skip and unused-variable colouring ported. Same-CPU pairs restore
  c2 on clqcl/MVRoundRobin/EDP3/Timetable_392 and keep bphp (2.4 s) and Rank754 (SAT 3,736 s).
  Release `b125ba45…`, archive `8360abd9…`,
  [binding](benchmarks/runs/20261007-candidate-c4-source/pins/release-binding.json).
  Open owner gaps: Johnson detector, one refinement per pass, per-pass split counter.
- **Affine search:** schedule on Kissat functions, neutral, no measured benefit
  ([report](benchmarks/reports/2026-10-08-affine-search-scheduling/README.md)). User
  2026-10-08: fix the two c4 fuzz findings, fix admission (xor_op census ResourceExhausted at
  zero work; lec_mult basis decays to rank 0), then turn it **on** in c5 (implemented rules ship on).

## Next action

1. Score c4: full Debug 1757469, gates 1757470–73 (§8.4 5,000 s retention rule; faithful-port
   exceptions only with shuffle/noise evidence and user consent), full44 1757529 (45,000 s
   checker). If c4 qualifies with ≥10 closures it is the session candidate; c2 (10 closures)
   needs the snw_16_9_CCSpre exception and a Debug rerun with `0b7d2bc55`.
2. Fuzz owner `firstopen-fuzz-owner` runs the c4 binding (sent 2026-10-08).
3. User decisions 2026-10-08: startup FastEliminate is never suppressed; the final
   documentation run uses -c 6; Rank569 gets solver-side witness-free expansion of
   permutation-witness SR lines (after c4 qualifies). Baseline reruns: 125 concurrent under
   the governor (no proof-size cap); submitted, see Live work.
4. After qualification: push, then the full Tactix Tier D run (cpu-q, -c 4, 32 GB, 5,000 s,
   8 h checker, 125 cap, governor; rerun failed checks at 64 GB).

## Live work (2026-10-08; check terminal state, never infer from the queue)

| Work | Jobs / owner | Status |
|---|---|---|
| c2 long checks | 1756907, `benchmarks/runs/20261008-fixed44-c2-longcheck` | 511, 541 verified; 569, 654 OOM; rest running |
| c3 full44 | 1757164, `benchmarks/runs/20261008-fixed44-c3` (45,000 s checker) | running; informational |
| c4 Debug / gates | 1757469; 1757470–73 (`benchmarks/runs/20261008-c4-regression-*`) | running |
| c4 full44 | 1757529, `benchmarks/runs/20261008-fixed44-c4` (45,000 s checker) | running |
| Fuzz owner | `firstopen-fuzz-owner`, FINAL_REPORT.md c4 section | c4 **FAIL**: no soundness failure; 2 deterministic affine findings also in c2 (am-refresh-after-elim N1 contract; afB-consequence-not-derived unclassified) |
| Repair agent | B28+C5 startup cost `2b1544cbb` (veer 43→24 s; mp1 = Satsuma's exact clauses, noise; battleship cut-point gap open) | now on the two affine findings, then battleship |
| c4 retention 5,000 s | 1757795–800 (`benchmarks/runs/20261008-r5000-c4-*`) | running |
| Baseline reruns | (A) 1757931 `benchmarks/baselines/sc24-26-satsuma-fixed64-32g-8h` %100; (B) 1757933 `sc24-26-recheck-64g-8h` %25; governor 1757996 (600 s, hold >750 G, release <700 G) | running; (C) after (A) |

## Verification authorities

| Scope | Disposition |
|---|---|
| Intermediate b26 | Release `b26efc0f…`; Debug 353/353; fails WV2–89; full44 2/44 verified (147, 401). Reference only |
| Prior cumulative fuzz | Claude 4d8f46d8 [terminal report](benchmarks/runs/20261007-owner-policy-cumulative-fuzz/FINAL_REPORT.md): 44,000 broad cases, zero failures, targeted reach incomplete. No current-source acceptance |
| Explicit long64-fixed checker | Claude 49c00a94 [ACCEPTED](benchmarks/runs/20261007-long64-fixed-independent-review/DISPOSITION.md) for bounded explicit-profile use; final Trestle verification is mandatory |
| Native proof transport | [Rejected r9](benchmarks/runs/20261007-native-transport-claude-oracle-owner/requalification-deferral-r9/DISPOSITION.md); use pure Python |

## Target investigations and decisions

- **Rank401:** FirstOpen/original-seed repair `0a15ecaf1` has [matched evidence](benchmarks/reports/2026-10-07-firstopen-original-seed-repair/README.md);
  exact b26 checks SAT42.273s. This is target evidence, not cumulative acceptance
  of the new unit candidate. Keep X17 and noL-specific local search retired;
  investigate ordinary Boolean search against actual Satsuma+Kissat order.
- **Rank899 (`mp1-Nb7T42`):** the [certified-unit gate](benchmarks/reports/2026-10-07-rank899-certified-unit-gate/README.md)
  passes. The large-domain producer publishes `+40310`, the proof prefix verifies, the boundary equals
  the Satsuma residual, and startup FastEliminate is suppressed. In a same-CPU pair, d576 is SAT in 55.921 s with a checked model; b26 is UNKNOWN at 300 s.
  This is a causal single-row result. Closure requires c1's full44 row.
- **Rank569:** [affine audit](benchmarks/reports/2026-10-07-rank569-affine-admission-lifecycle-audit/README.md)
  confirms24 recognized, disjoint XOR rows. The [conditional-clause experiment](benchmarks/reports/2026-10-07-rank569-conditional-clause-causal60/README.md)
  is retired for no measured benefit. The3627.980s/4.116GB run remains unverified
  and misses the compact/sub3359.975s target; its deleted proof prevents direct
  confirmation of the checker-failure attribution. Raising matrix budgets alone
  is unsupported. Persistent XOR remains restart-root-only with retained provenance.
- **Rank991 and other owners:** graph construction/Window remains opt-in;
  structural seeding stays off. Apply the [generality gate](DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration)
  to current and proposed LymphoSAT techniques. The [Lucky10 correction](benchmarks/reports/2026-10-07-lucky10-real-input-followup.md)
  preserves21 real recognized rows,11 checked SATs and10 search-free parity declines;
  its admission/cost review remains open. Cheap or unused theory consequences
  do not establish useful integration.

## Standing references and operations

- Keep the approved no-MAB baseline `d170b4a5` and [saved five-arm comparison](benchmarks/reports/2026-10-01-tierd-five-arm-terminal-results.md);
  do not rerun the baseline. Historical accepted gap coverage remains4/44 on its
  [frozen checkpoint](benchmarks/reports/2026-10-07-0e-four-regressions-terminal/README.md),
  not on the current repaired candidate.
- The noL-free checkpoint's [fourth regression gate](benchmarks/reports/2026-10-07-nol-free-wv2-accepted/README.md)
  completes its earlier four-set acceptance; later behavior changes require fresh
  gates. Plan §8.4 owns the exact regression ceilings and bounded checker policy;
  formal full-verification targets remain distinct from regression censors.
- The latest recorded [Tier-B paired window](benchmarks/runs/20260926-163543-B-am466-split-paired/) predates the no-MAB baseline and is historical diagnostic evidence only.
- Follow plan §8.1 for storage and Slurm. Last handoff quota:508GB; earlier audited
  obsolete Debug cleanup reclaimed44.57GB allocated. Recheck before builds and
  clean disposable storage aggressively above700GB. Protect live pins and evidence.
- [Andrew's handoff report](report-to-andrew/20261007T224148Z-session-handoff/report.pdf)
  has adjacent TeX; the [documentation cleanup report](report-to-andrew/20261007T225802Z-documentation-cleanup/report.pdf) records this follow-up. Commits are local; prior automatic
  approval review rejected pushing beyond the previously authorized range.
- Earlier experiment narratives remain in their primary reports and Git. Old
  handoffs and reports describe their own snapshots; they do not authorize
  restoring retired mechanisms or restarting superseded jobs.

## Recent changes

- `d576abaeb`: remove policy suppression of certified symmetry units; validation pending.
- `06b63069b`: record intermediate b26 full Debug353/353, zero skips.
- `5c6e653f7`: record Rank899's exact selected search-prefix match with Kissat.
- `4c17dee25`: add explicit long64-fixed checker profile; independent review accepts it.
- `f4b596b9b`: make generality an integration gate; `e3eb95c84` corrects Lucky10 reach evidence.
