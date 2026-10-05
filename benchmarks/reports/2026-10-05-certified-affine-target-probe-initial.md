# Initial certified-affine target probe

## Scope and authority

This report records the first target-scale diagnostic run of public X9
`AffineConsequence`.  It is diagnostic evidence, not a recovery claim.  The
three completed rows use immutable source `d7e48970d`, Release binary SHA-256
`b670b8dee45c827f532102e48b44dc45964be3d4c2d65998d2dcca552759002d`, DSR,
the opt-in `gf2-affine` producer, and the frozen one-action affine profile.  The
saved `d170b4a5` comparison was not rerun.

The anonymous runtime validations passed before submission as jobs `1750229`
and `1750230`.  Target jobs are `1750437` for WV2 rank 132 and `1750438` for
WV100 ranks 7 and 51.  Their manifests, input hashes, commands, result records,
and retained telemetry bindings live in the corresponding campaign directories
under `benchmarks/runs/20261005-certified-affine-d7e48970d-gf2-*`.

## Terminal results

All three rows reached their solver deadline with `UNKNOWN`; no checker ran and
no target is closed.

| Row | Expected | Limit | Producer calls | Producer work | Candidates / offers | X9 requests |
|---|---:|---:|---:|---:|---:|---:|
| WV2 rank 132, `tseitin_d3_n158` | UNSAT | 600 s | 397 | 4,789,262 | 0 / 0 | 0 |
| WV100 rank 7, `two-trees-511v` | SAT | 1,200 s | 281 | 1,940,937 | 0 / 0 | 0 |
| WV100 rank 51, `mdp-32-11-sat` | SAT | 1,200 s | 8 | 170,560 | 0 / 0 | 0 |

The supplemental audit fails closed on every row because the feedback sidecar
exceeded its 64 MiB retention cap.  Structural provenance contains only the
session binding because no X9 transaction committed.  Therefore the retained
solver statistics establish calls and zero offers, but the runs do not have a
complete action-by-action telemetry chain.

## Diagnosis

Three independent defects confound the intended experiment.

1. The old controller sampled all 20 named search configurations.  The first
   two rows made hundreds of search requests, so this did not measure affine
   reasoning layered over one controlled search trajectory.
2. The frozen affine limits truncate useful natural bases.  WV2 rank 132 has
   158 exact width-three equations, but reduction reaches width 68 and 157
   sources, beyond the configured 32/32 image and source limits.  It has 130
   selectable offers: one contradiction, one direct row, and 128 extended
   rows.  WV100 rank 7
   has 433 width-three equations and reaches width 62 and 160 sources, hitting
   the same four width/source caps; 326 extended rows are selectable.  WV100
   rank 51 is blocked earlier by 2,452
   capturable groups against `maximum_groups=1000`; after that cap is raised,
   its natural basis fits the other present limits and exposes 500 selectable
   rows, including 15 binary and two direct width-three consequences.
3. Feedback writes a full cumulative observation before and after every
   controller action.  One observation repeats hundreds of scalar and fixed
   array fields; the all-zero native-rule object alone is about 22.5 KiB.
   WV2-132 records about 2.24 million actions, WV100-7 about 4.83 million, and
   WV100-51 about 4.34 million.  The duplicated native snapshots alone imply
   raw lower bounds of 94, 203, and 183 GiB before other fields.  The post-run
   64 MiB cap consequently discards each stream wholesale.  This is a telemetry
   representation fault, not evidence that affine work itself generated that
   volume.  A lossless design therefore needs exact observation references and
   changed-value patches plus streaming compression; 64 MiB cannot be promised
   while retaining every search-service event.

The current source already fixes another issue visible here: a completed empty
or no-structure census of an immutable affine owner now remains terminal across
search and restart epochs.  Interrupted and resource-exhausted states retain
their explicit reconfiguration rules.  This prevents the hundreds of repeated
zero-yield scans observed in the old binary.

## Corrected probe requirements

The next target probe must use a fresh immutable source pin and must pass its
runtime validator before submission.  It will:

- allow exactly one named search configuration through the learner-owned
  search allowlist;
- include `AffineConsequence`, explicit `Propagate`, and
  `SemanticVariableAdvice`, with nonzero role-aware advice parameters in the
  typed, replay-fingerprinted affine profile;
- use the common exact-census correction `maximum_groups=2500`, image
  width/source limits 68/160, and basis width/source limits 68/160; keep every
  other proof, journal, extent, and work bound unchanged and finite;
- retain a complete, versioned telemetry chain without silently deleting it;
- require an independently checked model or proof and fail closed on missing,
  truncated, or inconsistent telemetry.

Rank 263 is a justified later probe after raising `maximum_groups` from 1,000
past its 1,899 capturable groups; its natural basis exposes 1,328 selectable
rows.  The resulting four-row corrected profile is
`affine-consequence2(12000,30000,1000000,2500,10000,10000000,8,68,160,4,1000000,1000000,10000000,10000000,68,160,10000,10000,3,100000,10000,1000000)`.
Fixed rank 569 has 10,158 groups and 10,230
patterns but only 24 independent one-source width-three equations, yielding no
selectable affine consequences; increasing its caps would add census cost
without testing X9 publication.  It should not be repeated as an affine target
unless new evidence changes that conclusion.
