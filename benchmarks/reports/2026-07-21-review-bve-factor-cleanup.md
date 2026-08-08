# Codebase review — post BVE/Factor bimodal churn (2026-07-21)

Focused `revisit-codebase` pass over this session's BVE/Factor work. Applied the
behavior-preserving subset inline (`simplify:` commit `9798642`); the structural
items below are routed to their owners.

## Applied inline (commit 9798642, behavior-preserving)

Removed dead diagnostic scaffolding from the bimodal investigation (129 lines out,
6 in; default byte-identical: mp1 1,482,563, clique 3.49 s, units 29/29, fuzz
default DRAT + random-policy LRAT 400/0):
- `bve.cpp`: `g_bve_aborts` global counter struct + increments (global mutable
  state in a rule — a smell), the two `TACTIX_BVE_TRACE` traces, and
  `TACTIX_BVE_SUBSUME` + `bve_resolvent_subsumed` (~60 lines — the net-negative
  subsumption experiment, preserved at commit `b8c1d3f`); vestigial `<cstdio>`.
- `factor.cpp`: `TACTIX_DUMP_CNF` + `TACTIX_GRID_TRACE` + the `init_R`/`traj_max_n`
  chain-trajectory instrumentation (27×27 investigation, concluded).
- `default_policy.cpp`: `TACTIX_INPROC_ROUNDS/BUDGET` (failed deep-cascade
  experiment) — reverted to the `INPROCESS_PHASE_ROUNDS/BUDGET` constants.

Kept (documented A/B knobs for the effort-budget work + load-bearing):
`TACTIX_NO_BVE`, `TACTIX_NO_FACTOR`, `TACTIX_BVE_FIRST`, `TACTIX_BVE_BOUND`,
`TACTIX_FACTOR_DENSITY`, `TACTIX_FACTOR_FORCE`.

## Routed findings

### 1. Rule-vs-policy leakage — Factor density=8 (axis 4, highest priority) — MEDIUM
`factor.cpp:189` — `density = 8` is a WHEN/WHICH scheduling threshold baked into
the rule (which instances to factor). Per §5 decision (7c4e85a) this belongs to the Policy;
it lives in the rule as a `NoOp` selectivity heuristic (N1-pure — every factoring
stays API-applicable). **Owner: the effort-budgeted scheduler (plan decision (06d3a8b))**,
which is where the selectivity moves to policy and where `density=8` (an overfit
magic number, shipped as the BVE-last stopgap) is superseded. Already documented
as a deviation in `implementation-notes.md`. No separate action — folds into decision (06d3a8b).

### 2. Redundancy — the binary-watch-scan idiom duplicated ×7 (axis 1) — LOW-MEDIUM
The pattern `ws = watches.arena.data() + heads[l].offset; for i: Watch w{ws[i]};
if (w.binary()) {…other()…; i+=1;} else i+=2;` recurs in **7 files**:
`bve.cpp` (`bve_binary_copies`), `factor.cpp` (`binary_partners`, `dump_*`),
`subsume_forward.cpp`, `strengthen.cpp`, `congruence.cpp`, `substitute.cpp`,
`kernel.cpp`. Rule of three well exceeded. A `kernel_for_each_binary_watch(k, l,
fn)` (or an iterator) would remove the hand-rolled `i += 1 : i += 2` stride from
every call site — reducing the drift risk in code that reads the watch layout.
**Caveat:** it is hot-path and layout-coupled, so extraction must be strictly
behavior-preserving (inline/`__attribute__((always_inline))`) and verified against
the Tier B reference — treat as a bounded **§5.5 hygiene** pass of its own, not a
revamp, and not mixed with feature work. Effort: ~half a day. Not blocking.

### 3. Coverage gap — Factor grid-generator fuzz (axis 5, task #38) — LOW
Factor's greedy grid detection (`factor.cpp` growth loop) is exercised by the
`TACTIX_FACTOR_FORCE` fuzz leg + `micro_proofs` cases, but the *generator* isn't
targeted to produce dense binary-grid structure, so wide/degenerate grids are
under-sampled. **Owner: fuzz** — add a grid-structured generator (m×n binary grids
+ perturbations) to `generators.py` so Factor's matching + the add-RAT proof path
see adversarial grids at small scale. Not blocking; folds into the next fuzz pass.

## Verdict
The hot rules are materially cleaner (bve.cpp −110 lines). No new soundness or
N1–N3 concern. The one architectural debt that matters (density in the rule) has
an owner and a removal path (decision (06d3a8b)). Items 2–3 are low-priority hygiene/coverage,
recorded for a later pass or their phase gate.
