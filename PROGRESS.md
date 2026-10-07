# Progress and resume guide

## Current stage

Active development (2026-10-07 session). The user's session goal is **at least 10
independently verified fixed44 closures on one coherent candidate** (plan §9.5
states 12), preserving N1–N4, with effective Claude-owned fuzz, full Debug and
B28+C5, Rank1–50, WV100 and WV2–89 acceptance. **Nothing is accepted yet.**
Scope and contracts belong to [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration).

Leading candidate **c1 = `e509d1d08`**: b26 source plus certified symmetry-unit
admission `d576abaeb` (r4 focused gate [passes](benchmarks/reports/2026-10-07-symmetry-unit-r4-focused-pass/README.md))
and the affine empty-reason repair `9ed61994c` ([report](benchmarks/reports/2026-10-07-affine-rootmaint-sweep-repair/README.md)).
The fixed64 patch hash is [corrected](benchmarks/reports/2026-10-07-fixed64-provenance-hash-correction/README.md).
c1 may still change if regression attribution finds a defective specialized pass.

**Regression attribution** ([report](benchmarks/reports/2026-10-07-wv289-nol118-regression/README.md)):
b26's losses come from the faithful Satsuma→Kissat residual seed (noL ×3, oski ×3) and the
FastEliminate port (Break_triple, Circuit). Graph labeling leaves those proofs byte-identical.
The user kept the faithful seed and accepted noL-11-8 as the plan §8.4 faithful-port retention exception.
Graph-labeling recognition still spends up to its unbacked 100M budget recognizing nothing; it is
an open efficiency item.

## Next action

1. Grade c1's four gates and full44 when terminal. Apply the §8.4 exception only to noL-11-8.
2. Qualify the final candidate: Release/full Debug pins, the fuzz owner's final binding,
   and all four gates. Do not rerun the d170 baseline or b26 campaigns.
3. Launch full44 on the final candidate only after the scratch-admission repair.
   b26 full44 measured 7/44 rows; 37 failed instantly on `scratch_node_busy`.
4. Proof-route diagnosis for fast UNSAT rows 541/1129/291/1075, which solve in under 250 s
   but never verify within 14,400 s, then fixed64 rechecks of 627/1004/654/569 on regeneration.

## Live work (2026-10-07 evening; check terminal state, never infer from the queue)

| Work | Jobs / owner | Status |
|---|---|---|
| c1 Release / full Debug | Release `c85bf484…` (1755757, done) / Debug 1755758; source `95d87cfe…` | Debug running |
| c1 regression gates | 1755859 B28, 1755860 Rank1–50, 1755861 WV100, 1755862 WV2–89; `benchmarks/runs/20261007-c1-regression-*` | submitted |
| c1 full44 | 1755927, `benchmarks/runs/20261007-fixed44-c1` (shared-floor scratch, long64-fixed) | submitted |
| Fuzz owner | Claude `385a3588` (resumed); staging binding `benchmarks/runs/20261007-candidate-d576-source/pins/staging-binding.json` | harness preparation and uncounted calibration; counted legs await final binding |
| Rank899 certified-unit gate | sub-agent; `benchmarks/runs/20261007-rank899-d576-*` | boundary checks, then 300 s causal pair |
| Proof-route cost | sub-agent; `benchmarks/runs/20261007-proofroute-*` | stage timings |

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
- **Rank899 (`mp1-Nb7T42`):** the [handoff](benchmarks/reports/2026-10-07-rank899-unit-admission-handoff/README.md)
  requires certified `+40310`, root growth, existing FastEl suppression and
  residual/activation-order checks before a bounded300s causal pair. The matched
  residual trace agrees for12,723 decisions and50 conflicts; it establishes
  neither preprocessing equivalence nor a runtime improvement. Do not revive MAB.
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
