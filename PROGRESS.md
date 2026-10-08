# Progress and resume guide

## Current stage

Session goal (user): **at least 10 independently verified fixed44 closures on one coherent
candidate**, preserving N1–N4, with effective Claude-owned fuzz, full Debug and B28+C5,
Rank1–50, WV100 and WV2–89 acceptance. **Nothing is accepted yet.** Scope:
[DEVELOPMENT_PLAN.md §9.5](DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration).

- **c2 = `76e161b24`** (branch `candidate-c2`): fuzz PASS
  ([owner report](benchmarks/runs/20261007-firstopen-original-seed-fuzz-owner/FINAL_REPORT.md)),
  B28+C5 accepted, full Debug 352/353 (test assertion only, fixed `0b7d2bc55`). full44
  `benchmarks/runs/20261007-fixed44-c2`: 8 verified (147, 263, 497, 627, 630, 899, 991, 1075).
  Gate rows at 5,000 s on the gate file (§8.4 rule): noL-11-0 SAT 2,513 s and noL-11-8 SAT
  1,138 s (retained); snw_16_9_CCSpre UNKNOWN (faithful-port exception candidate: Satsuma emits
  the same single unit and Satsuma+Kissat is UNKNOWN). Rank401 lost on c2 (SAT 7,271 s).
  Long-check rerun (45,000 s): 569 and 654 exceed 32 GB in the checker; others running.
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
  ([report](benchmarks/reports/2026-10-08-pure-round-shuffle-probe/README.md)).
- **Affine search:** schedule on Kissat functions, no measured benefit; stays off
  ([report](benchmarks/reports/2026-10-08-affine-search-scheduling/README.md)).

## Next action

1. Finish the A–D port repair (clique colouring first divergence against Satsuma's own log),
   plus EDP3/Timetable_392/MVRoundRobin attribution; same-CPU pairs vs c2.
2. Freeze c4 = repaired main; full Debug, four gates (§8.4 5,000 s retention rule;
   faithful-port exceptions only with shuffle/noise evidence), full44 with the 45,000 s
   checker, then send the c4 release binding to the fuzz owner (`firstopen-fuzz-owner`,
   holding with c3 preparation reusable).
3. Pending user decisions: baseline rerun storage plan (60 GB proof cap, ~50 concurrent
   Satsuma, (B) memout rows only, under `tools/slurm/quota_governor.py`); keep startup
   FastEliminate never suppressed (recommended); final documentation run -c 16 vs -c 6;
   Rank569 fix option.
4. After qualification: push, then the full Tactix Tier D run (cpu-q, -c 4, 32 GB, 5,000 s,
   8 h checker, 125 cap, governor; rerun failed checks at 64 GB).

## Live work (2026-10-08; check terminal state, never infer from the queue)

| Work | Jobs / owner | Status |
|---|---|---|
| c2 long checks | 1756907, `benchmarks/runs/20261008-fixed44-c2-longcheck` | running (569, 654 OOM) |
| c3 full44 | 1757164, `benchmarks/runs/20261008-fixed44-c3` (45,000 s checker) | running; informational for A–D |
| A–D repair | sub-agent (clique colouring regression) | running |
| Fuzz owner | `firstopen-fuzz-owner`; c3 held, nothing launched | waiting for c4 binding |
| Baseline reruns | `tools/bench/baseline_rerun.py`, [prep report](benchmarks/reports/2026-10-08-tierd-baseline-rerun-fixed64/README.md) | not submitted (user storage decision) |

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
