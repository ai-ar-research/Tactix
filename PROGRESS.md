# Progress and resume guide

## Current stage

Active development (2026-10-07 session). The user's session goal is **at least 10
independently verified fixed44 closures on one coherent candidate** (plan §9.5
states 12), preserving N1–N4, with effective Claude-owned fuzz, full Debug and
B28+C5, Rank1–50, WV100 and WV2–89 acceptance. **Nothing is accepted yet.**
Scope and contracts belong to [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration).

Candidate **c2 = `76e161b24`** (branch `candidate-c2`) is c1.1 (`bfaf65491`) plus `524b7b48b`.
c1.1 holds the b26 source, the certified symmetry units `d576abaeb` and the affine empty-reason repair `9ed61994c`.
`524b7b48b` makes startup FastEliminate never suppressed after structured progress
([report](benchmarks/reports/2026-10-07-fastel-suppression-removal/README.md)); the user decided this
after c1 lost WV100 snw_16_9_CCSpre. c1 evidence is superseded
([cancellation](benchmarks/runs/20261007-superseded-cancel-c2/README.md)). c2 excludes the unvalidated
graph-labeling commit `8e953218f`.

**Regression attribution** ([report](benchmarks/reports/2026-10-07-wv289-nol118-regression/README.md)):
b26's losses come from the faithful Satsuma→Kissat residual seed (noL ×3, oski ×3) and the
FastEliminate port (Break_triple, Circuit). Graph labeling leaves those proofs byte-identical.
The user kept the faithful seed and accepted noL-11-8 as the plan §8.4 faithful-port retention exception.
Graph-labeling recognition still spends up to its unbacked 100M budget recognizing nothing; it is
an open efficiency item.

## Next action

1. Grade c2's full Debug, four gates (apply the §8.4 exception only to noL-11-8) and full44
   when terminal. Closures need verification within the 45,000 s checker window
   ([winner plan](docs/winner-integration-plan.md)); proof routes are unchanged.
2. Integrate Satsuma practice A–D (plan §9.5) with the validated graph-labeling recognition fix
   `8e953218f` as candidate c3. Attribute any regression as noise (shuffled copies) or mechanism.
3. Use the Satsuma-boundary first divergences (896, 754, 453, 1144) to target remaining owner gaps.
4. After A–D, prototype restart symmetry inprocessing under the plan's Kissat-derived
   effort/interval/delay functions and search-use bookkeeping.

## Live work (2026-10-07 night; check terminal state, never infer from the queue)

| Work | Jobs / owner | Status |
|---|---|---|
| c2 Release / full Debug | Release `bec9eede…` (1756229, done) / Debug 1756230; archive `4cdf01d9…` | Debug submitted |
| c2 regression gates | 1756232 B28, 1756233 Rank1–50, 1756234 WV100, 1756235 WV2–89; `benchmarks/runs/20261007-c2-regression-*` | submitted |
| c2 full44 | 1756297, `benchmarks/runs/20261007-fixed44-c2` (shared-floor scratch, long64-fixed) | submitted |
| Fuzz owner | Claude `385a3588`; binding `benchmarks/runs/20261007-candidate-c2-source/pins/release-binding.json` | c1 legs stopped (25k cases, 0 product failures); c2 relaunch pending |
| Sub-agents | proof-route cost (541/1129/291/1075, plus 82/489/1085); graph-labeling recognition budget (`8e953218f`, unvalidated); Satsuma-boundary parity (896, 754, 453, 1144) | running |
| Diagnostic pairs | 1756222 snw, 1756223 Rank899: c1 vs guard-removed variant | informational |

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
