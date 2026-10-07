# Progress and resume guide

## Current stage

Development is paused at the user's requested handoff. The active milestone is
**at least 12 independently verified closures from the fixed 44-instance gap on
one coherent candidate**, with effective Claude-owned fuzz, full Debug and all
four regression gates. It is not achieved; formal T4-K/T4-VBS and universal
N1–N4 verification remain pending. Scope and contracts belong to
[DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration).

The latest solver checkpoint is `d576abaeb` (certified symmetry-unit admission).
It removes singleton-unit suppression from FixAndCut and large-domain generator
fixing, including checked binaries that become unit at root. Its r4 focused Debug, ASan/UBSan and DSR/Trestle gate passes (job 1755545);
full Debug, fuzz and regression gates remain pending. No new Release exists.
See the [repair checkpoint](benchmarks/reports/2026-10-07-symmetry-singleton-unit-admission/README.md).

The [session handoff](benchmarks/reports/2026-10-07-session-wrap-up-handoff/README.md)
owns exact source hashes, launch commands, owner sessions, job snapshots,
reproducers and preserved evidence. Its snapshot is not a live queue. Check
submitted results when resuming; do not wait for long jobs or restart superseded
campaigns. Current documentation cleanup changes no solver behavior.

## Next action

1. Inspect submitted results and the existing Claude owner reports listed below.
   Record terminal states and failures without inferring acceptance from queue
   disappearance. Keep working independently while useful jobs remain active.
2. Done: the r4 focused gate [passes](benchmarks/reports/2026-10-07-symmetry-unit-r4-focused-pass/README.md)
   (job 1755545). Debug and ASan/UBSan pass, every-lemma DSR/Trestle verifies, and the invalid control is rejected.
3. Reproduce and repair the deterministic [affine root-maintenance API failure](benchmarks/runs/20261007-firstopen-original-seed-fuzz-owner/handoff/am-rel-23/README.md):
   a well-formed persistent `AffineMatrixInitialize` request returns
   `InvalidRequest` after root Sweep. It also reproduces on the older noL-free
   source. No wrong model, proof or verdict was observed; the API failure remains
   a required repair, not an accepted decline.
4. Done: the fixed64 patch hash is corrected and file-bound; frozen manifests
   are read through the [supplemental correction](benchmarks/reports/2026-10-07-fixed64-provenance-hash-correction/README.md).
5. Freeze fresh clean Release/full Debug pins from the coherent repaired source.
   Supply its exact binding to the existing Claude fuzz owner, require effective
   Default/random-controller reach, and run B28+C5, Rank1–50, WV100 and WV2–89.
   New target measurements do not waive pending correctness or regression gates.

## Candidate and verification authorities

| Scope | Current authority and disposition |
|---|---|
| Unit-admission repair | `d576abaeb`; prepared r4 archive `1249dc7f…`; focused validation pending. Dependent r3 Release1755536/fullDebug1755537 were canceled before start and produced no pins. [Binding and cancellation](benchmarks/reports/2026-10-07-symmetry-units-clean-build-handoff/README.md) |
| Intermediate b26 | Release `b26efc0fb133787a797820d40c759ae552e2cf4eaabde4bee990f27947890bcf`; superseded as final candidate by the unit repair. [Clean Debug1755126](benchmarks/reports/2026-10-07-b26ef-clean-debug-terminal/README.md) passes353/353, zero skips. This does not qualify changed source |
| Current fuzz owner | Claude `385a3588-c5ba-4eef-b07a-a5a237bd1182`; [handoff](benchmarks/runs/20261007-firstopen-original-seed-fuzz-owner/HANDOFF.md) records the affine failure, untested harness repairs and unmet coverage. b26 is SUPERSEDED/INCOMPLETE; broad1755222/23/24 never started. Resume the same owner with a new binding |
| Prior cumulative fuzz | Claude4d8f46d8 [terminal report](benchmarks/runs/20261007-owner-policy-cumulative-fuzz/FINAL_REPORT.md):44,000 seeded broad cases, zero reported failures, required targeted reach incomplete. No current-source acceptance |
| Explicit long64-fixed checker | Claude49c00a94 [ACCEPTED](benchmarks/runs/20261007-long64-fixed-independent-review/DISPOSITION.md) for bounded explicit-profile use;587 corpus/mutant cases agree with canonical32. Preserve the disclosed OOM/unchecked case and untested large-capacity limits. Metadata correction remains open; final Trestle verification is mandatory |
| Native proof transport | [Independent r9 rejection](benchmarks/runs/20261007-native-transport-claude-oracle-owner/requalification-deferral-r9/DISPOSITION.md): interrupted handler restoration can swallow Ctrl-C. Omit the helper; use pure Python. No further speculative signal patch is planned |

## Submitted reference work to inspect

These jobs belong to **intermediate b26**, not the unit-admission candidate.
The [frozen result snapshot](benchmarks/reports/2026-10-07-session-wrap-up-handoff/regression-snapshot.json)
records partial counts, not final acceptance. Use the owning graders and the
standing censor/timeout rules after reading terminal records.

| Campaign | Submitted job(s) | Next-session handling |
|---|---|---|
| B28+C5 / Rank1–50 | 1755134 / 1755135 | Inspect terminal solver/proof results and exact aggregate/row bounds |
| WV100 / WV2–89 | 1755136 / 1755160 | Inspect terminal checking, timeouts and retained/recovery rows |
| Full44 reference | 1755331; busy-only retries1755422/1755462 | At handoff, seven tasks remained active and ranks147/401 were independently checked. Thirty-five pending retry02 tasks were canceled. No further b26 retries; retain all incomplete rows |

The clean-build handoff owns the exact seven task IDs, source bindings and
cancellation evidence. Unrelated allocation1751450 is outside this workstream.
No solver, build, fuzz or benchmark job is launched by this documentation pass.

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
