# Progress and resume guide

## Current stage

The accepted shared execution/feedback preparation remains `375b621b`; its
[acceptance report](benchmarks/reports/2026-10-03-shared-policy-preparation-accepted.md)
owns the controller audit, replay exception, full Debug, selected regression
gates, fuzz, and Andrew handoff. Reward design and training remain deferred.
Current work is the unaccepted organic winner-integration stage in
[plan §9.5](DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration) and the
[detailed subplan](docs/winner-integration-plan.md). N1--N4, learner-selectable
shared actions, deterministic replay, pure observation, after-action raw
feedback, both-policy fuzz, and independent proof/model checking remain active.

The primary objective is still all 44 misses from the saved four-winner
portfolio, first closing the fixed 36-instance gap. Use the saved `d170b4a5`
comparison; do not rerun it. LymphoSAT-owned WV100/WV2 rows are recovery
obligations, other rows are preservation controls, and B28+C5 plus Rank1--50
are mandatory gates. DSR is the default; add PB support only after a
rule-specific derivation and measured clausal proof/check cost establish need.

## Current evidence and disposition

| Scope | Authority | Disposition |
|---|---|---|
| Restored LymphoSAT SAT constructors | [terminal regression](benchmarks/reports/2026-10-04-restored-lymphosat-sat-regression-terminal.md) | Rejected `156afef6b`: B28+C5 passed, Rank aggregate failed, and seven WV/WV2 solves were lost; no wrong verdict or checker rejection |
| Affine and guidance focus | [terminal focused report](benchmarks/reports/2026-10-04-affine-guidance-recovery.md) | Six unique direct MDP owners recovered with checked models; `cabp`, Break, and Timetable trajectories not recovered; guidance never offered or consumed |
| Current scheduler repair | `f6d7453f6` | Lucky/8 restored as structured-root stage 0; combined checkpoint suite passes 10/10 in normal and ASan/UBSan builds; no benchmark result yet |
| Partial-model guidance | `a49191bfa` | Exact immutable candidates can feed Walk and CDCL separately with raw telemetry; mechanics tested, campaign reach/effect still absent |
| Certified theory actions v5 | [technical report](benchmarks/reports/2026-10-04-certified-theory-actions-v5.md) | Public GF(3)/DirectCSP consequences and random offerers implemented but unaccepted; ordinary fuzz had incomplete verdict coverage and no effective v5 reach |
| Certified UNSAT verticals | `86d142984`, `048442c40`, `e4d76b336`, `cd956aea` | GF(3) now has a disabled-by-default public authenticated complete Gaussian unit/conflict batch with typed/C v6 replay, opt-in random reach and raw feedback; Tseitin has private bounded autonomous exact-XOR discovery through live X7; Direct PHP is public and terminal. Focused GF(3) Debug is 7/7 and ASan/UBSan is 2/2; target, fuzz and gate acceptance remain open |
| Naive target row-cover proof | [checker-censor report](benchmarks/reports/2026-10-04-row-cover-target-checker-censor.md) | Rejected: 6,783,269 additions and 187,733,915 bytes reached the 1,800-second checker cap |

The corrected guidance fuzz jobs are terminal. Default job `1748417` found zero
failures in 51 corpus cases and 10,000 generated cases, with 9,718 terminal
verdicts and 282 timeouts. Random job `1748418` found zero failures with 8,464
terminal verdicts and 1,536 timeouts. `LocalSearchGuidance` fired zero times in
both, so these are compatibility/correctness evidence for terminal cases, not
full oracle coverage or effective guidance fuzz acceptance. No active campaign
is part of this checkpoint. **SLURM: NOTHING IN FLIGHT for this workstream**;
the only user job, interactive allocation `1747502`, is unrelated.

## Next action

The first two certified UNSAT verticals reach the public controller. Direct PHP
is append-only ID 36; GF(3) Gaussian is ID 37 and exposes one exact complete
batch so simultaneous consequences remain reachable before root units close
its assignment-free boundary. Both provide typed replay, random reach, raw
feedback and DSR/DRAT/no-proof publication while DefaultPolicy keeps them
disabled. Next, give Tseitin a typed connected-component payload; no private
frontend may choose hidden opportunity policy. Every added
constraint or definition must retain Boolean form, theory meaning, exact
premises, action/parameters, variable mapping, proof reference,
reconstruction/projection obligation, deletion state, and exact
effort/yield/proof cost. Repeat only from a freshly authenticated live plan.

Before another full gate, and only when benchmark execution is authorized, run
the `cabp`, Break and Timetable trajectory controls from `f6d7453f6`. Require
zero ULC binaries for `cabp` and Break, and five SR applications with zero ULC
binaries for Timetable. If restored, run the owner subset and then B28+C5,
Rank1--50, WV100 and WV2--89 with the 1,800-second proof-check cap and baseline
plus `max(30%, 20s)` completed-proof slowdown rule.

## Established authorities and open gates

- Accepted solver baseline: no-MAB `d170b4a5`; the [AM551 milestone](benchmarks/reports/2026-09-28-am551-no-mab-hybrid-milestone-accepted.md) and [AM552 WV evidence](benchmarks/reports/2026-09-29-am552-wv100-terminal-tierd-progress.md) own stabilization.
- The [five-arm Tier-D report](benchmarks/reports/2026-10-01-tierd-five-arm-terminal-results.md) owns 986 checked Tactix solves, the 36 checker failures/146 checker timeouts, and the saved portfolio comparison.
- The newest retained Tier-B window, [AM466 split paired](benchmarks/runs/20260926-163543-B-am466-split-paired/), predates the no-MAB checkpoint and is historical diagnostic evidence only.
- Accepted regression references: Rank1--50 50/50 and B28+C5 33/33, with aggregate solver ceilings 8,521.52175 and 999.87615 seconds; WV100 is 99 checked plus one checker timeout, WV2--89 is 79 checked plus ten solver timeouts.
- Full acceptance still requires current-source full Debug; B28+C5, Rank1--50, WV100, and WV2--89; both-policy fuzz with effective new-action reach; all 44 independently checked targets; and a current N1--N4 audit.
- Formal T4-K/T4-VBS targets and universal N1--N4 verification remain unmet. The broader objective is to substantially exceed LymphoSAT, Satsuma+Kissat, Kissat-sc2024, and AE-Kissat-MAB combined.

## Recent changes

- `cd956aea` exposed the authenticated complete GF(3) Gaussian closure as public X14 with C v6, replay, random reach and raw feedback.
- `86d142984`/`048442c40`/`e4d76b336` completed private GF(3), autonomous Tseitin discovery and public Direct PHP verticals.
- `5c23c1f4c` aligned lifecycle tests with the affine stage; the combined 17-test Debug matrix passes.
- `f6d7453f6` restored affine Lucky/8 to the accepted structured-root stage-0 lifecycle and pinned its execution signature.
- `666f8bad0` corrected current guidance reachability parsing and added its regression test.
