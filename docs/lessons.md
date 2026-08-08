# Standing lessons — measured, do not re-derive

This living working aid records current experimental lessons. It is not an authority and must never be
cited by lesson number. Put any finding used for a decision, together with its primary evidence, in the
owning report or design document. Rewrite or delete lessons when evidence changes; frozen reports,
designs, logs, and Git preserve history. `PROGRESS.md` owns status and the may-not-gate set.

- **L2 · Unbudgeted recurring BVE loses.** The measured recurring growth-slack arm regressed Tier B
  from 20.8 to 32.9 PAR-2; the smaller interval variants were inside noise. This bounds that undosed
  form, not during-search elimination in general.
- **L3 · `NoOp` probing can mutate search state.** Shared propagation relocates watches and rewrites
  `searched` hints even when a probing rule derives nothing. The defect no longer gates routing, but any
  policy that treats `NoOp` as state identity must account for these mutations.
- **L6 · Read performance deltas only from same-window A/B runs at equal concurrency.** Solo probes are
  still valid for reachability, bit identity, counters, and mechanism checks, but not effect size.
- **L7 · Repeated BVE/Factor damage is search-trajectory damage, not rule runtime.** Cadence is
  non-monotone and therefore not a reliable control; admission is the useful experimental axis.
- **L8 · A one-feature ablation does not isolate that feature.** Survivors reallocate work, so include an
  all-off control. Interacting families may inflate or reverse single-member effects; measure the family.
- **L10 · Use the reference solver's own control, with replication and counters.** Same-instance
  reference controls distinguish intrinsic damage from our scheduling, but wall time alone cannot
  separate mechanism from contention.
- **L11 · Price missing mechanisms in the reference with an all-off control.** In the measured whole-tier
  window, the roughly 14 PAR-2 missing-technique cost was almost entirely Sweep; do not generalize that
  window into a stable percentage of the solver gap or attribute separate value below its noise floor.
- **L12 · Decompose by instance before interpreting a tier aggregate.** Aggregates can hide offsetting
  effects, population changes, and configuration-specific chaos.
- **L13 · Cross-format proof checking is coverage, not redundancy.** DRAT can accept cases that LRAT hint
  checking rejects. Keep negative proof assertions, record checker failures/OOMs, and exercise both.
- **L14 · Compare the same instance and the same counter scope.** Read a reference counter's declaration
  and every increment site before using it; similarly named aggregates may cover different mechanisms.
- **L15 · Rule-family value can be non-separable.** Measure interacting rules jointly before judging a
  member. A clean structural separator is not by itself evidence for a static admission gate.
- **L16 · Reproducible per-instance effects can sum to an unstable or accidentally correct aggregate.**
  Replicate boundary decisions and inspect the decomposition predicted by the proposed mechanism.
- **L19 · Read reference option source, including macros and polarity.** Names, help text, and grep are
  insufficient. CaDiCaL's `factorthresh` is live through token pasting and `factordelay=0` is maximally
  delayed; parity on one guard does not imply whole-rule parity.
- **L20 · Replication-stable instances can be perturbation-fragile.** Before attributing a large
  single-instance regression, test an unrelated perturbation and distinguish knife-edge behavior from
  ordinary run-to-run noise. This is the basis of the may-not-gate set.
- **L21 · Compare a candidate fix with the shipped default, not only the broken arm it repairs.** Never
  estimate a tier verdict from a small hand-picked decomposition.
- **L22 · A stress knob must be observed firing on the generated size class.** Count at the source, pass
  the knob explicitly in the command, print its denominator, and ensure the corpus spans the range that
  can activate it. A zero from an unreachable or stale arm is not evidence.
- **L23 · Scope safety checks at the granularity required by the proof.** A rare per-item hazard becomes
  near-universal when it skips a whole round. Instrument the skip rate and prefer source counters over
  noisy PAR-2 attribution.
- **L24 · Compute a knob's formula-level blast radius before launching jobs.** Predicted-inert instances
  are useful contention controls, but membership must be rechecked as the live formula evolves; a moving
  control is evidence that the prediction was incomplete.
- **L25 · Do not gate a per-literal property with an aggregate condition.** Assert the invariant at the
  point of harm and require every regression test to have been observed red. Pass experimental knobs in
  the executed command so a silently missing setting cannot masquerade as a clean run.
- **L26 · Elimination timing is a large lever, but the deleted density gates earned no warrant.** New
  admission mechanisms begin unconditional and must beat never-run and per-instance-oracle controls
  under the current protocol before they can be shipped or cited.
- **L27 · A rule's value may live entirely in round one.** Arms that change both cadence and which rules
  execute are uncontrolled comparisons; do not infer a round-cap trend from them.
- **L28 · No reference schedule supports our BCE constants.** The rate, slot, round cap, and effort floor
  are local choices and must be labeled as such. `mod4block` nevertheless has a measured dependency on
  BCE deletions through the downstream X2 resolvent/conflict mechanism.
