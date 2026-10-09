# Progress and resume guide

## Current stage

**Committed candidate: c2.1 = `1aad09b07`** (user decision 2026-10-09). Branch
`candidate-c2.1`; Release `28995e13…`. It meets the session goal (at least 10 verified
fixed44 closures on one coherent candidate; [plan §9.5](DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration)):

- full Debug 353/353; Claude-owned fuzz PASS, 0 product failures;
- B28+C5 accepted (964.3 s ≤ 999.9 s); Rank1–50, WV100 and WV2–89 pass under §8.4
  (5,000 s retention for noL-11-0, noL-11-8 and Circuit_multiplier24; accepted
  exception snw_16_9_CCSpre);
- **13 verified fixed44 closures**: ranks 147, 226, 263, 489, 497, 511, 541, 627, 630,
  899, 991, 1075, 1129;
- [qualification report](benchmarks/reports/2026-10-08-candidate-c2.1-qualification/README.md).

Main's source, tests and rule documentation equal c2.1. Development after c2.1 was
purged from the tree at the user's request and survives only in Git history (last
pre-purge main: `d3a6a6527`). That includes the c3/c4 Satsuma symmetry practice, the
witness-free SR rewrite (`163429b44`), the Kissat start-of-search repair (`5ea5c5df1`;
candidate c5a failed B28+C5, Rank1–50, WV100 and WV2–89 on seven SAT rows),
affine Gauss–Jordan (`f39f59246`) and the FastEliminate bound port (`d3a6a6527`).
Kept: c2.1's evidence, the plan rules its acceptance uses (§8.3 profiling, §8.4
5,000 s retention and the snw16 exception), and the baseline rerun tooling.

## Next action

0. **Candidate c2.2 = c2.1 + the startup FastEliminate guard** (skip it when structured
   symmetry breaking fixes a root; user decision 2026-10-09, accepted deviation in the
   implementation notes). Validate: Release + full Debug, the four gates, and noL-11-10 /
   mp1-Nb7T42 from the fixed44 set (c1, which had the guard, solved them in 42.7 s / 55 s).

1. Baseline reruns (A) 1757931 and (B) 1757933 run under governor 1757996 (600 s; hold
   >750 GB, release <700 GB); (C) follows (A).
2. Then the full Tactix Tier D run on c2.1: cpu-q, 4 CPUs, 32 GB, 5,000 s solve, 8 h
   check, at most 125 concurrent, under the governor; rerun failed checks at 64 GB.

## Live work

| Work | Jobs | Status |
|---|---|---|
| Baseline reruns | (A) 1757931, (B) 1757933; governor 1757996 | running |

## Verification authorities

| Scope | Disposition |
|---|---|
| Intermediate b26 | Release `b26efc0f…`; Debug 353/353; fails WV2–89; full44 2/44 verified (147, 401). Reference only |
| Prior cumulative fuzz | Claude 4d8f46d8 [terminal report](benchmarks/runs/20261007-owner-policy-cumulative-fuzz/FINAL_REPORT.md): 44,000 broad cases, zero failures, targeted reach incomplete. No current-source acceptance |
| Explicit long64-fixed checker | Claude 49c00a94 [ACCEPTED](benchmarks/runs/20261007-long64-fixed-independent-review/DISPOSITION.md) for bounded explicit-profile use; final Trestle verification is mandatory |
| Native proof transport | [Rejected r9](benchmarks/runs/20261007-native-transport-claude-oracle-owner/requalification-deferral-r9/DISPOSITION.md); use pure Python |

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
- Follow plan §8.1 for storage and Slurm. Quota at the purge: 540 GB; earlier audited
  obsolete Debug cleanup reclaimed44.57GB allocated. Recheck before builds and
  clean disposable storage aggressively above700GB. Protect live pins and evidence.
- [Andrew's handoff report](report-to-andrew/20261007T224148Z-session-handoff/report.pdf)
  has adjacent TeX; the [documentation cleanup report](report-to-andrew/20261007T225802Z-documentation-cleanup/report.pdf) records this follow-up.
- Earlier experiment narratives remain in their primary reports and Git. Old
  handoffs and reports describe their own snapshots; they do not authorize
  restoring retired mechanisms or restarting superseded jobs.

## Recent changes

- Restore the startup FastEliminate guard after root-fixing structured symmetry (c2.2).
- Purge: main's code returns to c2.1; post-c2.1 development removed from the tree.
- `0268acb59`: qualify c2.1 (13 verified fixed44 closures, all gates and fuzz).
- `4a22518c8`: accept WV100 snw_16_9_CCSpre as a faithful-port retention exception.
- `7f1dbc40a`: retain gate rows that solve within the 5,000 s envelope.
- `b175fb6e5`: require profiling both arms of every slowdown.
