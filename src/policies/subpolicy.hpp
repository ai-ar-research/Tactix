// Sub-policy surface (plan §5/§5.6) — the uniform shape for a swappable scheduling
// heuristic. A `Policy` composes sub-policies and may swap ANY of them without
// touching a rule (presets, MAB arms, external/RL); the phase-6 learner treats each
// as an arm over which it learns. The uniform part is IDENTITY + swappability (and,
// later, a reward hook); the CHOICE signature is category-specific — branching
// returns a decision literal, a bound schedule returns a growth bound — because
// those decisions are genuinely different shapes. This umbrella un-defers Track H ·
// H6's sub-policy super-class now that a 2nd class of arm (the BVE bound ramp) exists.
//
// Existing scheduling that is NOT yet a SubPolicy (restart triggers, mode switching,
// rephase, the inprocessing-slot effort budgets) is reparented incrementally; this
// header is where each lands as it is formalized.
#pragma once

#include "kernel/kernel.hpp"

namespace tactix {

struct SubPolicy {
  virtual ~SubPolicy() = default;
  // a stable name for introspection and the §5.6 arm space (the learner enumerates
  // sub-policies by name, like rules by id).
  virtual const char *name() const = 0;
  // (phase-6 reward hook lands here when the learner does: observe(reward). Kept out
  // now — no consumer yet, §5.5.)
};

// Branching: choose the next decision literal. Any unassigned literal is a legal
// instance (N1); rules validate, choosing never bypasses the guard.
struct BranchingPolicy : SubPolicy {
  virtual Lit pick(Kernel &) = 0;
};

// Reduction: given the ranked worst-first deletion candidates (soundness-filtered by
// the rule), return how long a prefix to delete.
struct ReductionPolicy : SubPolicy {
  virtual size_t victims(const Kernel &, size_t candidates) = 0;
};

// BVE growth-slack schedule (kissat eliminate.c set_next_elimination_bound). A
// preprocessing BVE pass eliminates a variable iff its non-tautological resolvents
// ≤ occ⁺ + occ⁻ + bound; RAMPING the bound over successive passes does the cheapest
// (non-growing) eliminations first, so each is measured against a formula already
// simplified by the cheaper ones — the adaptive, per-instance version of a single
// fixed bound. (An earlier version of this comment justified the ramp by a "BVE
// U-shape" — too many eliminations tripling the conflicts. That curve was drawn
// through single draws on crypto1, an instance since found to be dose-chaotic, and
// it is withdrawn. The cheap-first argument above is kissat's own and stands
// without it.) `num_passes()` levels, `bound(p)` the
// growth slack at pass p (ascending). The policy drives `Kernel::bve_bound` from it.
struct BveBoundPolicy : SubPolicy {
  virtual uint32_t num_passes() const = 0;
  virtual uint32_t bound(uint32_t pass) const = 0;
};

// Factor grid-selection strategy (Track S · S2a). The BVA PROOF RULE is
// apply_factoring(x, F, R); WHICH grid to build is a STRATEGY, lifted out of
// the rule into this swappable/learnable sub-policy — the Factor analog of
// BveBoundPolicy. The rule (rules/factor/factor.cpp) keeps the MECHANISM (seed
// heap, co-occurrence counting, greedy grow, best-prefix, proof-bearing apply);
// this policy owns only the scoring/accept SCALARS the mechanism keys on.
//
// N1: reachability of every delta>=1 factoring is carried by SWAPPABILITY + the
// pure guard, exactly like k.bve_bound — NOT by a per-grid payload enumeration.
// A maximizing policy sets accept == (delta>=1) and reaches every positive-gain
// grid; the scores only REORDER which applicable grid is built first (a ranker,
// §5.6), never gate reachability. Design: docs/design/s2a-factor-matcher-design.md.
struct FactorMatchPolicy : SubPolicy {
  // Optional per-sweep lifecycle for a stateful score (kissat distinct_paths
  // caches across a sweep). Default no-op keeps a stateless watch-size arm trivial.
  virtual void begin_sweep(const Kernel &) {}
  virtual void end_sweep() {}

  // Seed priority for the max-heap (kissat watches_score / update_candidate),
  // re-consulted on the stale re-score and the post-apply re-push (kissat
  // update_factored). Pure scheduling: reorders the sweep, never restricts
  // reachability.
  virtual double seed_score(const Kernel &, Lit seed) = 0;

  // Secondary key among next-factor candidates tied on the max co-occurrence
  // count `cofactor` across the current remainder set (kissat
  // tied_next_factor_score). The count stays the primary key in the rule.
  // `seed` is the chain's seed factor — structural scores (kissat
  // distinct_paths) key on it; the default arm ignores it.
  virtual double next_factor_score(const Kernel &, Lit cand,
                                   uint32_t cofactor, Lit seed) = 0;

  // Accept threshold — the Factor analog of BveBoundPolicy::bound. Given the best
  // greedy grid (m×n, net gain delta = m·n − (m+n)), worth applying? MUST accept
  // every delta>=1 at its most permissive setting (N1 reachability floor — the
  // rule-side FACTOR_MIN_GAIN cost floor is applied before this is consulted).
  virtual bool accept(uint32_t m, uint32_t n, int delta) = 0;
};

} // namespace tactix
