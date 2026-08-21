# Tier-D rank 27: separate Rephase trail clearing from Restart accounting

## Outcome

After the reference-wide Congruence repair, Tactix and Kissat agree through
the opening formula, 741,305 search decisions, and conflict 2027 analysis.
The first new portable divergence was not Vivify: Kissat performed a focused
Restart at conflict 2027, while Tactix deferred it to conflict 2028.

Both solvers had identical glue averages, restart admission values, deadlines,
and conflict histories. Tactix alone counted the earlier Rephase root-trail
clear as a canonical Restart. That one extra event crossed Kissat's shipped
logarithmic restart-interval boundary. The smallest source-backed repair splits
the shared sound backtrack mechanic from Restart lifecycle accounting and uses
the accounting-neutral mechanic from Rephase.

The repair is unit-, trace-, and regression-clean. It restores the exact
conflict-2027 Restart transaction and agrees through conflict 2028. It does
**not** close rank 27: the checked 1,200-second witness remains UNKNOWN. The
next investigation starts after conflict 2028 rather than repeating Vivify,
Congruence arity, or Restart-accounting work.

## Frozen witness and accepted predecessor

- Immutable rank: 27, expected UNSAT.
- Input: `tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz`.
- Compressed SHA-256:
  `5435b1b271b049ed9291ab1b9b297afb33b8d2d3c50ff1b48bfaed4ddf7bb4e9`.
- Frozen Kissat time: 278.187 s. The frozen performance reference was not
  rerun.
- Accepted predecessor binary SHA-256:
  `b6c991e099f4fa9cc635bacb20d7aa44ff3fdc23d0af78974b12998cecd6fb6f`.
- Repaired candidate binary SHA-256:
  `8f3208b12baf0b569e384aca65a8faba39fe4b98f043a6940ed1992b6322ec47`.
- Source base: `713cc26d449ca17699ff23e2d14e4acb8561a924`; source-diff SHA-256:
  `4c334c02e2890924fa2a657550eb97506ea0463901ff8c6de87f2dc4bfd3ab70`.

No input identity, rank, hash, conflict, literal, deadline, or local tuning
constant enters production behavior.

## Localization

Fresh trace job `1637041` begins from the Congruence-corrected binary. All
462,390 captured Tactix decisions match the retained Kissat trace. Jobs
`1637042` and `1637043` extended the compact trajectory through conflict 2011;
the apparent tail difference in the first was only the diagnostic stop. A
pinned-source diagnostic Kissat extension in job `1637053` confirmed exact
decisions and learned hashes through decision 495,135 and conflict 2011.

Paired job `1637054` found the first real search split at decision ordinal
741,306. Both solvers reached conflict 2027 with the same conflict clause,
learned binary hash `be68020008e7838b`, and backtrack target 22,853. Tactix
then chose literal 117122 at level 22,854; Kissat restarted and chose literal
117137 at level 1.

Lifecycle job `1637056` retained the surrounding transactions. Everything
through conflict 2026 was exact. At conflict 2027 Kissat emitted
`restart_begin`/`restart_end`; Tactix did not, then Tactix restarted at conflict
2028. Both had performed the same single Rephase at conflict 1001.

Restart-state job `1637058` compared the two focused glue EMAs, margin,
admission result, next deadline, and restart count. Every value except the count
matched. After the common Rephase, Tactix's counter was one larger. At the
relevant boundary Kissat had 90 canonical restarts while Tactix had 91.

The source explains the exact transition. Pinned Kissat commit
`8af8e56f174b778aef3aa45af9f739b2a5f492c2` implements:

- `kissat_rephase`: call `kissat_backtrack_propagate_and_flush_trail`, then
  increment `rephased`; it does not increment `statistics.restarts`.
- `kissat_restart`: increment `statistics.restarts`, backtrack/reuse the trail,
  and update the focused restart limit.
- focused delta: `restartint + log10(restarts + 9) - 1`, with shipped
  `restartint = 1`.

Thus count 90 gives `log10(99) - 1 < 1`, which truncates to an added zero and
keeps a one-conflict interval. Count 91 gives `log10(100) - 1 = 1`, growing the
interval to two. The old Tactix Rephase path called `restart_apply_to(k, 0)`,
so its neutral trail clear crossed that source-defined boundary early.

## Repair

`src/rules/restart/restart.cpp` now separates two responsibilities:

- `restart_backtrack_to` clamps the requested level and performs the sound
  backtrack without changing Restart or trail-reuse statistics;
- `restart_apply_to` retains canonical Restart accounting, then delegates the
  shared mechanic.

`DefaultPolicy` uses `restart_backtrack_to(k, 0)` only for Rephase's root clear.
Canonical scheduled Restarts still use `restart_apply_to`, with unchanged trail
reuse, phase updates, deadline updates, and event order. The Rephase rule remains
an H-only operation; its documentation now states the accounting boundary.

`tests/test_rephase.cpp` pins the first mathematical interval transition. It
starts with 90 canonical restarts at conflict 2000, proves that a Rephase clear
leaves the next deadline at 2001, then performs one canonical Restart and
requires the count 91 deadline at 2002. These test values derive from the
shipped reference formula and are absent from solver policy.

## Validation

The clean Release build passes 69/69 tests, including proof checks and the new
Rephase/Restart boundary regression. `git diff --check` passes. No unit or
campaign leg exposed a crash or soundness defect, so the development plan's
Claude repair-delegation trigger did not fire.

Repaired trace job `1637060` exactly matches Kissat at the former frontier:

- conflict 2026 decision and canonical Restart transaction;
- conflict 2027 decision 741,305, analysis, learned hash, Restart begin/end,
  and next root decision 117137;
- conflict 2028 decision 764,173 and learned hash `beabd2000ac68c29`.

The required matched guard is
`benchmarks/runs/20260821-0255-BC-rephase-restart-accounting/` (job `1637061`):

| arm | Tier B | Tier C | checked total | aggregate time |
|---|---:|---:|---:|---:|
| accepted predecessor | 28/28 | 5/5 | 33/33 | 2201.821 s |
| repaired candidate | 28/28 | 5/5 | 33/33 | 1731.598 s |

The candidate/predecessor ratio is 0.786439. Each arm returns 17 SAT and 16
UNSAT results. Across both arms, all 34 SAT models and 32 UNSAT proofs verify.
Candidate peak RSS is 3,294,292 KiB. The aggregate reports no invalid,
malformed, missing, cleanup-bad, or regressed task and no certificate residue.

Only after that gate passed, immutable witness job `1637131` ran under the
pre-Phase-4.5 16 GiB/1200 s contract. It returned UNKNOWN at 1200.004 s. The
runner recorded a 133,169,152-byte partial certificate, deleted it and all
derived scratch, and published zero residue. Scheduler peak RSS was 481,552
KiB. Evidence:
`benchmarks/runs/20260821-0310-tierd-rank27-rephase-restart-accounting/`.

## Vivify conclusion, limitation, and next action

The user's Vivify concern was correct at the policy level: the formula-range
disable was an unjustified non-reference gate and has already been removed.
The ensuing rank-27 comparison did audit the whole common trajectory rather
than assume Vivify was still the owner. In this instance, the first remaining
portable split after the Congruence repair occurred later in search and was
owned by Rephase/Restart accounting. Adding a Vivify-specific arm would have
missed the causal producer.

This increment closes that accounting divergence, not rank 27. The next action
is to extend the paired semantic/lifecycle trace only beyond the now-exact
conflict-2028 transaction, locate the first later common-state divergence, and
repair it only if a general source-backed change remains. Any new candidate
again owes full Tier B plus the predeclared Tier C guard before another witness.
