# AM395: current-source post-finalization wrong-SAT diagnostic

Date: 2026-09-25. Source commit `83ee5124` has solver source identical to
AM393 `c58c1f68`; the current Release `tactix` SHA-256 is
`10454cd605af39bdb7e773a3dd0b103ec91497f053af1a070e4b1d8f7cd7da90`.
This is diagnosis, not an API repair or N1/N2 acceptance.

The exact AM380 public-C-API two-clause driver (source SHA-256
`ac440015f693823477bf15a35e0c9c5d2004e97adf24f1a0f994e4c43395c9a7`)
was relinked against current Release objects and run on hwulab as job `1720247`
(2 CPUs, 8 GiB, terminal `COMPLETED 0`). Its original-CNF model check gives:

| Between `(3)` and `(1 ∨ 2)` | Solve/model | Independent check |
|---|---|---|
| no query | SAT, `1 -2 3` | satisfies both clauses |
| `tactix_decide_candidates(8)` | SAT, `-1 -2 3` | falsifies `(1 ∨ 2)` |
| `tactix_status` | SAT, `-1 -2 3` | falsifies `(1 ∨ 2)` |
| `tactix_rules_applicable` | SAT, `-1 -2 3` | falsifies `(1 ∨ 2)` |
| `tactix_rule_instances` | SAT, `-1 -2 3` | falsifies `(1 ∨ 2)` |

Exact driver and solver pins, wrapper, SLURM log, and per-variant output are
retained in [AM395 run](../runs/20260925-am395-current-postfinal/README.md).
The job's exit zero means its diagnostic wrapper completed, **not** that the
four affected solver variants passed.

## Causal source boundary

The query calls invoke `input_complete` (`src/api/api.cpp`), which calls
`finish_input` once. `finish_input` retires every unassigned variable absent
from the surviving input formula, decrementing `unassigned_active` and marking
it `Eliminated` (`src/frontend/parse.cpp`). With only `(3)` ingested, variables
1 and 2 are outside the active population. A later `tactix_add(1),
tactix_add(2), tactix_add(0)` still runs the binary fast importer: it sets
`input_activated` and adds a live binary watch, but does not restore the
retired `var_status`/`unassigned_active` population. `input_complete` is
idempotent and does not redo that lifecycle. Thus `has_sat_assignment` can
report completion with the new clause's variables unassigned, and
`tactix_solve` publishes an external model violating that clause. This is a
general lifecycle mismatch, not a branch-score or clause-specific issue.

The public v1 `tactix_add` returns `void`, the header calls it IPASIR-style,
and plan §5.1 defers incremental solving as O7. Rejecting post-finalization
adds requires a fail-closed API error/poison contract; supporting them requires
reactivation and proof-ID/reconstruction lifecycle design. Neither contract
was silently selected in this diagnostic. The user has been asked to choose;
until then, this remains a confirmed current N2 soundness defect. H2 native
DSR rejection, Claude-owned fuzz, the full Debug retry, and all retained
performance gates remain separately open. No Tier-D comparison was submitted.
