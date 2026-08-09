// Sub-policy surface (plan §5, §5.6): the uniform shape of a swappable
// scheduling heuristic. A Policy composes sub-policies and may swap any of them
// without touching a rule; the phase-6 learner treats each as an arm. Uniform
// across them is identity plus swappability, never the choice signature.
// Scheduling not yet a SubPolicy (restart triggers, mode switching, rephase,
// inprocessing-slot effort budgets) is reparented here as it is formalized.
#pragma once

#include "kernel/kernel.hpp"
#include "rules/factor/factor.hpp"

namespace tactix {

struct SubPolicy {
  virtual ~SubPolicy() = default;
  // A stable name for introspection and the §5.6 arm space: the learner
  // enumerates sub-policies by name, as it does rules by id.
  virtual const char *name() const = 0;
  // The phase-6 reward hook lands here with the learner; kept out until there
  // is a consumer (§5.5).
};

// Branching: choose the next decision literal. Any unassigned literal is a
// legal instance (N1); rules validate, choosing never bypasses the guard.
struct BranchingPolicy : SubPolicy {
  virtual Lit pick(Kernel &) = 0;
};

// Candidate order: in what order to try the instances a rule enumerated for one
// occasion. Under a cutting effort budget the order is the schedule, since the
// budget stops the walk part-way. N1: `rank` is a permutation, never a filter —
// dropping a candidate narrows what this policy can reach, invisibly, since the
// rule would still accept the payload. The resumption cursor
// (DefaultPolicy::SlotEntry::cursor) is the same kind of object: it moves where
// the walk starts, never which candidates exist.
struct CandidateOrderPolicy : SubPolicy {
  // Permute insts[0..n) in place. `rule` is the registry id, so one policy
  // object can serve every entry and the learner can hold one arm per rule.
  virtual void rank(const Kernel &k, uint32_t rule, uint64_t *insts,
                    uint32_t n) = 0;
};

// Reduction: given the ranked worst-first deletion candidates, already
// soundness-filtered by the rule, return how long a prefix to delete.
struct ReductionPolicy : SubPolicy {
  virtual size_t victims(const Kernel &, size_t candidates) = 0;
};

// BVE growth-slack schedule (kissat eliminate.c set_next_elimination_bound). A
// pass eliminates a variable iff its non-tautological resolvents are
// <= occ+ + occ- + bound; ramping the bound does the cheapest eliminations
// first, so each later one is measured against an already-simplified formula.
// `bound(p)` is the ascending slack at pass p, and drives `Kernel::bve_bound`.
struct BveBoundPolicy : SubPolicy {
  virtual uint32_t num_passes() const = 0;
  virtual uint32_t bound(uint32_t pass) const = 0;
};

// Factor grid-selection strategy, the Factor analog of BveBoundPolicy. The BVA
// proof rule is `apply_factoring(x, F, R)`; which grid to build is strategy,
// left to this sub-policy, which owns only the scalars the rule's mechanism
// keys on. N1: reachability of every delta >= 1 factoring rests on swappability
// plus the pure guard, as with `k.bve_bound`, not on a per-grid enumeration — a
// maximizing policy sets accept to delta >= 1 and reaches every positive-gain
// grid. Design: docs/design/s2a-factor-matcher-design.md.
struct FactorMatchPolicy : SubPolicy, FactorSelection {
  // The coupled Factor bound (BVE's growth allowance) for this phase. An
  // injected matcher customizes SCORING, not the growth invariant — kissat's
  // bound test is unconditional (factor.c:939) — so the policy hands it the
  // snapshot; ignoring it is an explicit choice, not an accident.
  void set_bound(uint32_t) override {}
  // Optional per-sweep lifecycle for a stateful score (kissat distinct_paths
  // caches across a sweep). The no-op default keeps a stateless arm trivial.
  void begin_sweep(const Kernel &) override {}
  void end_sweep() override {}

  // Seed priority for the max-heap (kissat watches_score / update_candidate),
  // re-consulted on the stale re-score and the post-apply re-push.
  virtual double seed_score(const Kernel &, Lit seed) override = 0;

  // Secondary key among next-factor candidates tied on the max co-occurrence
  // count `cofactor` (kissat tied_next_factor_score), which stays the primary
  // key in the rule. `seed` is the chain's seed factor, ignored by the default
  // arm and keyed on by structural scores.
  virtual double next_factor_score(const Kernel &, Lit cand,
                                   uint32_t cofactor, Lit seed) override = 0;

  // Accept threshold for the best greedy grid (m by n, net gain
  // delta = m*n - (m+n)). At its most permissive setting it must accept every
  // delta >= 1 — the N1 reachability floor. The rule-side FACTOR_MIN_GAIN cost
  // floor is applied before this is consulted.
  virtual bool accept(uint32_t m, uint32_t n, int delta) override = 0;
};

} // namespace tactix
