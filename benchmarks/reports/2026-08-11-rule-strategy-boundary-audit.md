# Rule/strategy boundary audit after P4-CVG and Tier B closure

**Date:** 2026-08-11

**Decision:** ship the behavior-preserving cleanup. The post-P4 rule and
strategy changes introduced three ownership defects, all now repaired. Full
Tier A/B execution counters remain identical, proof/model validation is clean,
and aggregate runtime remains inside the preregistered 2% ceiling.

## Scope and boundary

The audit revisited `src/`, the public registry/API surfaces, focused tests,
configuration checks, and the implementation ledger. The intended boundary is:

- rules own pure guards, complete legal instances, formula/proof effects,
  rule-local deterministic effort currencies, and canonical round mechanics;
- policies own admission, ordering, limits, delay, repetition choice, and the
  amount of rule-owned currency a dose may consume;
- the kernel provides neutral state and explicitly posed channels, without
  importing a policy implementation; and
- telemetry may inform a policy only through a declared rule metric. One rule
  must not mutate another rule's scheduling currency.

Existing neutral channels remain deliberate: `work_budget`, `vivify_scope`,
BVE bounds, Factor's selection interface, arena ordering, and heuristic event
queues allow any policy to pose a legal rule application without putting a
DefaultPolicy type in a rule. Sweep's retained seed tail remains rule-owned
canonical-round continuation state so registry/API applications keep their
existing behavior; the decision whether to retain it is now caller-owned.

## Findings and repairs

### 1. Dead Factor admission wire

`Kernel::factor_admit` was described as policy advice, read inside
`factor_apply`, and assigned only the value `true`. The writer check therefore
reported a policy connection even though no policy could decline through it.
This was fake wiring and left an N1-hostile admission branch in the proof rule.

The field, rule-side branch, redundant `TACTIX_FACTOR_FORCE` override, policy
writes, and stale configuration classification are removed. DefaultPolicy's
large-formula/material decisions already occur before dispatch, and direct
registry/API Factor applications remain unrestricted.

### 2. Sweep strategy option in Kernel

Sweep already accepts caller-owned `SweepLimits`, but its resumption choice was
separately stored as `Kernel::sweep_resume`. `SweepLimits::resume` now carries
that decision. DefaultPolicy poses its typed configuration there; standalone
and diagnostic applications retain the same default and environment overlay.
The retained tail/cursor/pass count is unchanged, preserving canonical rule
continuation and every existing counter.

### 3. Substitute charged Sweep's meter

`substitute_apply_sweep_equivalences` incremented
`Stats::sweep_ticks_repr`, making Substitute know that its caller was Sweep and
how that caller's governor was priced. The exact same pre-rewrite arena-word
charge now occurs at Sweep's representation-bridge call. The shared
`sweep_effort` accessor is the single definition of the rule's scheduled
currency; DefaultPolicy assigns an allowance without reconstructing its
private counter formula.

The conformance check now covers these data flows in addition to policy-header
imports and heuristic calls. It rejects policy switch fields
`factor_admit`/`sweep_resume` in Kernel, a non-Sweep write to
`sweep_ticks_repr`, a rule-side Factor admission gate, or a policy-side
reconstruction of Sweep's private currency.

No calculus precondition, legal instance, proof delta, formula mutation,
candidate order, budget threshold, or shipped default changed.

## Execution identity and runtime

Baseline binary: `build/pin-rule-policy-before/tactix`, SHA-256
`da51ecc190b58e066c9a905738f21019edfb001714df046f202bbfe3e2f575c1`.

Candidate binary: `build/pin-rule-policy-after/tactix`, SHA-256
`76cd0e4225662a451ed93fb17687e461419caf55d269973fd027de49b668c602`.

Both use `cpu-dense-hwulab-q`, 1 CPU/task and 8 GiB/CPU in the same windows.
Every SAT model and UNSAT DRAT certificate in both arms was independently
checked.

| Gate | Candidate | Baseline | Identity result | Runtime ratio |
|---|---:|---:|---|---:|
| Tier A, jobs `1609914/1609915` | 200/200, PAR-2 0.56 | 200/200, PAR-2 0.56 | all per-instance verdicts, reach maps, proof sizes, and model-check messages equal | 112.284/111.664 = **1.0056** |
| Tier B, jobs `1610318/1610319` | 28/28, PAR-2 27.87 | 28/28, PAR-2 27.67 | all per-instance verdicts, reach maps, proof sizes, and model-check messages equal | 780.303/774.651 = **1.0073** |

Tier A uses list SHA-256
`7c0e15d5b4563f8860a346d8dcae7a3239b77460ed7d473ed4e9ed185e9b7a5f`;
Tier B uses
`5130866032ae706e48f17f0db431bb66e08536f69640b5a62cb90fd3e9c6bcef`.
The measured increases, 0.56% and 0.73%, are below the 2% rejection ceiling.

The byte-level witness is job `1610390` under
`benchmarks/runs/20260811-rule-policy-audit-identity/`. Baseline and candidate
produce the identical 65.6 MB `FmlaEquivChain` DRAT file, SHA-256
`7559135370e204a035dea0dbc582907efc497c34219d09166e2edc36738e011e`.
After removing wall-clock lines, the complete logs are byte-identical. Runtime
is 49.87s versus 49.80s. The first submission (`1610385`) failed before solver
execution because this partition did not define `SLURM_TMPDIR`; the corrected
job used task-unique `/tmp` paths.

The complete Release suite passes 69/69 under job `1610381`. An earlier
submission (`1610376`) could not create CTest's `LastTest.log` while the Tier B
runner intentionally held the candidate pin read-only; it was rerun unchanged
after the build lock released.

## Disposition

The cleanup is accepted because behavior evidence is exact at the available
counter/log/proof levels and runtime remains below the declared ceiling. Tier C
was not needed for a non-scheduling refactor after full A/B identity; no
competitive or broad scheduling claim is made. Corpus cases 004/005 and the
held sparse-GC arm were not exercised.

Reopen on any rule module importing a policy implementation, heuristic call
from a macro formula rule, policy admission inside Factor, caller-specific
effort mutation inside Substitute, Kernel mirror of an existing caller-owned
Sweep option, proof/model mismatch, reach-map mismatch, or repeatable runtime
increase above 2% on the same allocation shape.
