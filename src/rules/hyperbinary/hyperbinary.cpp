// See hyperbinary.hpp for the reference/deviation record.
#include "rules/hyperbinary/hyperbinary.hpp"

#include "rules/congruence/congruence.hpp" // congruence_replay_rup
#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

namespace {

// A resolvent found on one probe, banked until the trail is unwound: the
// formula may not be mutated while probing bookkeeping is live.
struct Resolvent {
  Lit dom;      // the dominator; the emitted clause is (¬dom ∨ forced)
  Lit forced;   // the literal the long clause forced
  Ref reason;   // the long clause resolved against
  bool subsumes; // ¬dom ∈ reason, so the resolvent subsumes it
};

// Walk up the binary implication tree until the two literals meet. Both are
// true at level 1 and `parent` is a tree over exactly those literals, so the
// walk terminates at the probe root in the worst case. cadical
// `probe.cpp:149`; the `trail` comparison is what makes it O(depth) rather
// than O(depth²) — always step the DEEPER of the two.
Lit dominator(const Kernel &k, const std::vector<Lit> &parent, Lit a, Lit b) {
  Lit l = a, m = b;
  while (l != m) {
    if (k.assigned[var_of(l)].trail > k.assigned[var_of(m)].trail)
      std::swap(l, m);
    const Lit p = parent[var_of(m)];
    if (p == INVALID_LIT)
      return l; // m is the root: it dominates everything below it
    m = p;
  }
  return l;
}

} // namespace

bool hyperbinary_guard(const Kernel &k) {
  // quiescent root, the probing-family precondition (V1's argument)
  return !k.inconsistent && !k.conflict_valid && k.level == 0;
}

// Same schedule as V1 Probe: a literal is worth assuming only if its negation
// carries a binary watch, i.e. it is a head of the binary implication graph.
// Without that, the closure is empty and no long clause can be resolved.
void hyperbinary_candidates(const Kernel &k, std::vector<Lit> &out) {
  out.clear();
  for (Var v = 0; v < k.nvars; v++) {
    if (k.value(make_lit(v, false)) != 0)
      continue; // root-fixed
    for (int neg = 0; neg <= 1; neg++) {
      const Lit p = make_lit(v, neg != 0);
      for (const WatchEntry w : k.watches.entries(negate(p)))
        if (w.binary()) {
          out.push_back(p);
          break;
        }
    }
  }
}

namespace {

void unwind(Kernel &k) {
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  if (k.level > 0)
    kernel_backtrack(k, 0, /*update_phases=*/false);
  // Give search back the watch order and `searched` hints this probe churned.
  // MUST run before the caller mutates the formula (kernel_probing_begin).
  kernel_probing_end(k);
}

// Close `p` under propagation, then read the resolvents off the finished
// trail. Returns false if the probe conflicted — that is V1 Probe's failed
// literal, and it belongs to V1: emitting a unit here would duplicate another
// rule's inference and its proof obligation.
bool collect(Kernel &k, Lit p, std::vector<Lit> &parent,
             std::vector<Resolvent> &out) {
  kernel_probing_begin(k);
  k.level++;
  k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
  const uint32_t base = static_cast<uint32_t>(k.trail.size());
  kernel_assign(k, p, k.level, DECISION_REASON, false);
  // A CONFLICT here means `p` is a failed literal, which is V1 Probe's
  // inference and its proof obligation — not ours. Note the polarity: unlike
  // V1, this rule wants the probe to SURVIVE, because the resolvents are read
  // off a consistent level-1 closure.
  if (kernel_propagate_probing(k)) {
    unwind(k);
    return false;
  }

  // The trail from `base` is the level-1 closure, in assignment order, so a
  // literal's antecedents always precede it and `parent` is complete by the
  // time it is read.
  for (uint32_t i = base; i < k.trail.size(); i++) {
    const Lit l = k.trail[i];
    const Assigned &a = k.assigned[var_of(l)];
    if (a.reason == DECISION_REASON) {
      parent[var_of(l)] = INVALID_LIT; // the probe root
      continue;
    }
    if (a.binary) {
      // reason holds the OTHER literal of (l ∨ other); the literal that forced
      // l is therefore ¬other, and it is true.
      parent[var_of(l)] = negate(static_cast<Lit>(a.reason));
      continue;
    }
    if (a.reason == UNIT_REASON) {
      parent[var_of(l)] = INVALID_LIT; // root unit: nothing above it
      continue;
    }

    // Forced by a LONG clause: this is the resolution opportunity.
    const Ref ref = static_cast<Ref>(a.reason);
    const Clause *c = clause_at(k.arena, ref);
    Lit dom = INVALID_LIT;
    uint32_t non_root = 0;
    for (uint32_t j = 0; j < c->size; j++) {
      const Lit o = c->lits[j];
      if (o == l)
        continue;
      const Lit t = negate(o); // false in the clause ⇒ its negation is true
      if (k.assigned[var_of(t)].level == 0)
        continue; // root-fixed: cadical's case (A), contributes no dominator
      non_root++;
      dom = (dom == INVALID_LIT) ? t : dominator(k, parent, dom, t);
    }
    if (non_root == 0 || dom == INVALID_LIT) {
      // Case (A): every other literal is root-false, so the clause is already
      // binary in effect. cadical declines the resolvent here too; garbage
      // collection shrinks the clause eventually.
      parent[var_of(l)] = INVALID_LIT;
      continue;
    }
    // Install the dominator as the parent, which is precisely the reason
    // cadical would have installed by adding the resolvent during propagation
    // (hyperbinary.hpp deviation 1) — this is what keeps the tree a tree.
    parent[var_of(l)] = dom;
    if (dom == p)
      continue; // (¬p ∨ l) is the trivial resolvent: no dominance was gained
    bool subsumes = false;
    for (uint32_t j = 0; j < c->size; j++)
      if (c->lits[j] == negate(dom)) {
        subsumes = true;
        break;
      }
    out.push_back(Resolvent{dom, l, ref, subsumes});
  }
  unwind(k);
  return true;
}

// Emit one banked resolvent at the clean root. The replay is both the RUP TEST
// and the chain source (hyperbinary.hpp deviation 2), so a resolvent unit
// propagation cannot re-derive is DECLINED rather than emitted unchecked.
bool emit(Kernel &k, const Resolvent &r, std::vector<uint64_t> &chain) {
  const Lit lits[2] = {negate(r.dom), r.forced};
  if (lits[0] == lits[1] || lits[0] == negate(lits[1]))
    return false; // degenerate or tautological: nothing to state
  if (k.value(lits[0]) != 0 || k.value(lits[1]) != 0)
    return false; // root-fixed since the probe; the binary is not news
  if (kernel_binary_live(k, lits[0], lits[1]))
    return false; // already present
  if (!congruence_replay_rup(k, lits, 2, chain)) {
    k.stats.hbr_declined++;
    return false;
  }
  const bool lrat = k.proof && k.proof->lrat();
  const uint64_t id = ++k.next_clause_id;
  if (k.proof)
    k.proof->add_rup(lrat ? id : 0, lits, 2, lrat ? chain.data() : nullptr,
                     lrat ? static_cast<uint32_t>(chain.size()) : 0);
  kernel_add_binary_clause(k, lits[0], lits[1], id);
  k.stats.hbr_binaries++;

  if (r.subsumes) {
    // The resolvent subsumes its own reason, so the reason may go. Safe here
    // and only here: the probe is unwound, so no clause is a live reason.
    Clause *c = clause_at(k.arena, r.reason);
    if (!(c->flags & Clause::GARBAGE)) {
      kernel_delete_clause(k, c);
      k.stats.hbr_subsumed++;
    }
  }
  return true;
}

} // namespace

ApplyResult hyperbinary_try(Kernel &k, Lit p) {
  assert(hyperbinary_guard(k));
  if (var_of(p) >= k.nvars || k.value(p) != 0)
    return ApplyResult::NoOp;
  std::vector<Lit> parent(k.nvars, INVALID_LIT);
  std::vector<Resolvent> found;
  k.stats.hbr_probes++;
  if (!collect(k, p, parent, found))
    return ApplyResult::NoOp; // a failed literal is V1 Probe's inference
  std::vector<uint64_t> chain;
  bool any = false;
  for (const Resolvent &r : found)
    any |= emit(k, r, chain);
  return any ? ApplyResult::Ok : ApplyResult::NoOp;
}

ApplyResult hyperbinary_apply(Kernel &k) {
  assert(hyperbinary_guard(k));
  // D-LEVEL guard purification: complete the root closure if applied off a
  // fixpoint (no-op otherwise). A root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.hbr_rounds++;
  std::vector<Lit> candidates;
  hyperbinary_candidates(k, candidates);
  std::vector<Lit> parent(k.nvars, INVALID_LIT);
  std::vector<Resolvent> found;
  std::vector<uint64_t> chain;
  bool any = false;
  for (const Lit p : candidates) {
    if (k.value(p) != 0)
      continue; // fixed since the round began
    if (kernel_rule_interrupt(k, k.stats.hbr_interrupts))
      break;
    found.clear();
    k.stats.hbr_probes++;
    if (!collect(k, p, parent, found))
      continue;
    for (const Resolvent &r : found)
      any |= emit(k, r, chain);
  }
  return any ? ApplyResult::Ok : ApplyResult::NoOp;
}

// instance surface: payload = the probe root, the shared lit_instance encoding
// (as V1 Probe and M2 Decide); INSTANCE_CANONICAL = the full round.
// COMPLETE over roots — the enumeration is the same schedule the round walks,
// and `hyperbinary_try` accepts any unfixed literal, not only a scheduled one.
static uint32_t hyperbinary_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Lit> cands;
  hyperbinary_candidates(k, cands);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(cands.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(cands[i]);
  return n;
}

static ApplyResult hyperbinary_apply_instance(Kernel &k, uint64_t inst) {
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return hyperbinary_apply(k);
  Lit p;
  if (!lit_of_instance(inst, &p))
    return ApplyResult::NoOp;
  return hyperbinary_try(k, p);
}

TACTIX_REGISTER_RULE(HyperBinaryRes, RuleTier::Inprocess, hyperbinary_guard,
                     hyperbinary_apply, hyperbinary_instances,
                     hyperbinary_apply_instance);

} // namespace tactix
