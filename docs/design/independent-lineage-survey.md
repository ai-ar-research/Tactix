# Independent-lineage survey — Glucose & CryptoMiniSat vs. every landed feature

> **Frozen snapshot — do not edit.** A design/reference record from when this
> work was done; it is *not* kept in sync. Living truth: `docs/calculus.md` (what
> the rules are), `DEVELOPMENT_PLAN.md` (scope/roadmap), `PROGRESS.md` (status).

2026-07-15, per plan decision (9cecad5) (§2 takeaway: not a Kissat re-implementation; §5
survey obligation). Retroactive pass over every feature implemented so far
(PROGRESS.md phases 1–3), grounded in the cloned sources: `glucose/` (4.2.1)
and `cryptominisat/` (master). Verdicts: **keep** (our shape confirmed or
superior), **candidate** (a concrete alternative worth measuring, with its
target phase), **preset** (policy-layer variant, no rule/kernel change).

## Kernel & propagation

| Feature (ours) | Glucose | CryptoMiniSat | Verdict |
|---|---|---|---|
| Flat clause arena, 32-bit refs, header-packed | MiniSat `ClauseAllocator` (region-based, 32-bit, similar) | Same MiniSat-descended allocator + `cl-consolidate` compaction pass | **keep** — all three lineages converge; ours matches the most cache-disciplined (kissat) variant |
| Watches: blocking lits, binaries inlined in ONE list, flat watch arena | Separate `watchesBin` / `watches` per literal (two vectors) | Implicit binaries in watch lists (like us); per-lit vectors | **keep**. Note: Glucose's *separate* binary lists make binary-only scans free (used by its binResMinimize, below) — our single list pays a filter walk. Not worth a layout change; revisit only if a binary-scanning rule shows up in profiles |
| Propagation: `searched` resumption, delayed relocation, ticks | Plain MiniSat propagate (no blockers on binaries, no resumption) | MiniSat-shaped with implicit binaries | **keep** — kissat's is the measured state of the art here |

## Decide / branching (M2)

- **Ours:** VMTF (focused) + EVSIDS heap (stable), 2-mode switching (S4).
- **Glucose:** VSIDS only, one mode.
- **CMS:** `enum class branch {vsids, rand, vmtf}` — **rotates strategies on a
  growing conflict schedule** (`branch_strategy_change += 5000; *= 1.1`,
  searcher.cpp:2195), each paired with its own restart type (luby/geom/glue).
- **Verdict: keep now; preset (phase 4).** CMS is an existence proof that
  N-strategy rotation beats a fixed pair on diverse benchmarks — in our
  architecture that is literally a Policy preset over the existing
  instance-ranking modes (plus optional new rankings, e.g. LRB, as kernel
  H additions per §5.1). Feeds the phase-4 MAB scheduler (O6) directly.

## Analyze (M3)

- **Ours:** 1-UIP + recursive minimization + all-UIP shrinking + OTFS (kissat
  4.0.4 shape; winners ship it byte-identical).
- **Glucose:** recursive minimization, **plus `minimisationWithBinaryResolution`
  (Solver.cc:561)**: for learned clauses with LBD ≤ 6, scan the asserting
  literal's *binary* watch list once and self-subsume away clause literals
  binary-implied by the UIP. This is NOT subsumed by reason-walk minimization
  or by shrinking (the binary need not be anyone's reason), and neither
  kissat 4.0.4 nor CaDiCaL has it (checked minimize.c/shrink.c/minimize.cpp —
  their "binary" handling is reason-following only).
- **CMS:** recursive only.
- **Verdict: CANDIDATE (phase 3/4, measured).** Cheap (one binary watch scan
  per qualifying conflict), independent-lineage-unique, composes cleanly as an
  Analyze intensity step (post-v1-additive per analyze.hpp's atomic record).
  Risk: kissat deliberately dropped it — the survey argument cuts both ways;
  Tier B/C must judge.

## Backjump + chrono (M4)

- **CMS** has chrono with a decision-level-gap threshold
  (`diff_declev_for_chrono`, searcher.cpp:1804) — same shape as our
  policy-side `CHRONO_LEVELS` via `backjump_apply_to`. Glucose: none.
- **Verdict: keep** — second lineage confirms the gap-threshold form.

## Restart (S1)

- **Ours:** kissat package — bias-corrected fast/slow glue EMAs, margin 1.10
  (focused); reluctant doubling (stable); trail reuse.
- **Glucose:** bounded queues, not EMAs (last-50 LBD vs global mean, K=0.8),
  **plus blocking restarts** (Solver.cc:1498): when the trail is unusually
  deep (`trail.size() > R·trailQueue.avg`, R=1.4) the restart is *postponed*
  — "we may be close to a model, don't throw it away". Kissat has no direct
  equivalent (stable mode plays that role, bluntly).
- **CMS:** rotates restart *types* (luby/geom/glue) with the branching
  rotation. No trail reuse (ours/kissat's is strictly extra).
- **Verdict: keep now; blocking-restart is a pure-policy CANDIDATE.** It
  reads only trail size + a queue the policy can own — zero rule/kernel
  change (the decision (169aecd) dividend). Worth a Tier B/C ablation in phase 4; a
  natural MAB arm alongside the mode switcher.

## Reduce (S2)

- **Ours:** glue tiers (≤2 / ≤6) + 5-bit used recency + promotion on use +
  75% volume, arithmetic interval.
- **Glucose:** LBD-sort, delete half; `setCanBeDel(false)` freeze when LBD
  improves (≈ our promotion+used); **adaptive interval** (Solver.cc:1216):
  if the LBD at the deletion boundary is low (DB is healthy), the next
  reduction is *delayed* by `specialIncReduceDB` — the interval reacts to DB
  quality, not just a counter.
- **CMS:** 3 levels (lev0 keep-forever / lev1 / lev2) with a
  `must_touch_lev1_within` recency window (≈ our used-bits, second-lineage
  confirmation) — but lev2 victims are ranked by **usage counters
  (`SortRedClsUIP1`, `SortRedClsProps` — times used in UIP derivations /
  propagations) and optionally an ML predictor**, not by glue+size.
  *Correction (2026-07-15, found at experiment 4 implementation): the
  usage-count sorts are `STATS_NEEDED`/`FINAL_PREDICTOR`-gated — CMS's
  competition default rotates plain glue/activity sorts (reducedb.cpp
  `clean_type` switch). The experiment measured the usage-count variant
  anyway (it is the interesting delta), with the caveat recorded.*
- **Verdict: keep now; two CANDIDATES.** (a) Glucose's DB-quality-adaptive
  interval is pure ReductionPolicy state — cheap phase-4 experiment.
  (b) CMS's usage-count ranking is an alternative rule-side ranking (we
  already count `used`; extending to a wider counter and ranking by it is a
  small reduce.cpp variant) — measure on Tier C when it exists.

## SwitchMode (S4), Rephase (S3), phases

- **Glucose:** neither (plain phase saving).
- **CMS:** polarity-mode **rotation** `best → stable → best_inv → saved`
  (searcher.cpp:2352) — includes **inverted-best** (`best_inv`), a type
  kissat doesn't have (kissat's Inverted is all-negative; CMS inverts the
  *best snapshot*).
- **Verdict: keep; `best_inv` is a trivial CANDIDATE** — one more payload in
  Rephase's instance space (rule-local, additive) when Walk lands in phase 4
  and the rotation is revisited anyway.

## Lucky (S5), trail reuse, target phases

Kissat-specific; neither independent lineage has them. **Keep** (they are
strictly-extra probes/optimizations; CMS/Glucose absence is not evidence
against, since both lag kissat on exactly the instance classes these help).

## Native LRAT / proof backend

- **Glucose:** DRUP text only. **CMS:** FRAT (+ external tooling).
- **Verdict: keep** (CaDiCaL remains the only native-LRAT reference; our
  choice is ahead of both independent lineages — the competition combined
  solver+checker metric is the bet, plan §2).

## C1 SubsumeForward (this week)

- **Glucose:** preprocessing-only backward subsumption (SimpSolver.cc,
  MiniSat shape, same 1000 clause-size cap — second source for the constant).
- **CMS:** `occ-backw-sub-str` in the occ schedule **plus
  `subsumeimplicit.cpp`** — subsumption *through the watch lists*, the
  closest existing precedent for our binary-subsumers-from-watches and
  per-instance discovery path. Recorded in subsume_forward.hpp.
- **Verdict: keep** — our design is independently confirmed by the
  non-Kissat lineage.

## The inprocessing scheduler (upcoming, §5.3 phase-3 consumer)

- **CMS's `simplify_schedule_nonstartup` is a comma-separated technique
  string** (`"scc-vrepl,sub-impl,breakid,occ-backw-sub-str,occ-clean-implicit,
  occ-bve,occ-bva,occ-ternary-res,occ-xor,card-find,cl-consolidate,..."`,
  solverconf.cpp:279) dispatched at runtime, each token time-budgeted.
- **Verdict: adopt the IDEA for the registry-driven scheduler** — CMS proves
  a data-driven, reorderable inprocessing schedule works at competition
  level. Ours becomes: policy presets = sequences over registry rule names
  with tick budgets, which is also exactly the §8.4 ablation-as-config story
  and the N1 selling point. Design input for the C2-adjacent scheduler item.

## Beyond parity (noted, not scheduled)

- **CMS Gauss–Jordan/XOR** (`occ-xor`, gaussian.cpp): powerful on crypto
  classes; proof story (DRAT for XOR reasoning) is genuinely hard — CMS
  itself struggled there. Backlog-with-RFC territory (new proof primitive
  discussion per §5.3), only if Tier D shows an XOR-heavy gap.
- **CMS ML clause predictor** (`SortRedClsPred*`): evidence that learned
  ranking functions fit in the clause-DB policy slot — a natural future
  LLM/RL-policy demo target through our N1 surface, no kernel change.

## Summary of actionable deltas

1. **Glucose binResMinimize** — Analyze candidate, phase 3/4, measured.
2. **Glucose blocking restarts** — pure DefaultPolicy candidate / MAB arm.
3. **Glucose adaptive reduce interval** — pure ReductionPolicy candidate.
4. **CMS usage-count clause ranking** — Reduce ranking variant, measure on Tier C.
5. **CMS branch/restart/polarity rotation** — phase-4 preset shape (O6 input).
6. **CMS `best_inv` rephase type** — trivial additive payload, with S6 Walk.
7. **CMS schedule strings** — design input for the registry-driven scheduler.

None of these contradict a landed design; items 1–4 are the ones that could
make an implemented feature measurably better and each is small. Everything
already landed survives the survey with its shape confirmed by at least one
independent lineage or by measured evidence.
