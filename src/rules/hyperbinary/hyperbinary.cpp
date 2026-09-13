// See hyperbinary.hpp for the rule's contract.
#include "rules/hyperbinary/hyperbinary.hpp"

#include "rules/congruence/congruence.hpp" // congruence_replay_rup
#include "rules/propagate/propagate.hpp"
#include "rules/support/failed_literal.hpp"

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

uint64_t hyperbinary_semantic_instance(Lit dominator,
                                       uint32_t literal_word) {
  assert(dominator != INVALID_LIT && literal_word != UINT32_MAX);
  return (static_cast<uint64_t>(dominator) << 32) | literal_word;
}

RuleEligibility hyperbinary_guard(const Kernel &k) {
  // quiescent root, the probing-family precondition (V1's argument)
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("no-conflict", !k.conflict_valid)
      .fact("root-level", k.level == 0);
}

// Same schedule as V1 Probe: a literal is worth assuming only if its negation
// carries a binary watch, i.e. it is a head of the binary implication graph.
// Without that, the closure is empty and no long clause can be resolved.
void hyperbinary_candidates(const Kernel &k, std::vector<Lit> &out) {
  failed_literal_candidates(k, out);
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
  // A conflict here means `p` is a failed literal, which is V1 Probe's
  // inference and its proof obligation. Note the polarity: unlike V1, this rule
  // wants the probe to SURVIVE, because the resolvents are read off a
  // consistent level-1 closure.
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
    // Install the dominator as the parent, precisely the reason cadical would
    // have installed by adding the resolvent during propagation (see
    // hyperbinary.hpp) — this is what keeps the tree a tree.
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

// Emit one banked resolvent at the clean root. The replay is both the RUP test
// and the chain source (see hyperbinary.hpp), so a resolvent that unit
// propagation cannot re-derive is declined rather than emitted unchecked.
bool emit(Kernel &k, const Resolvent &r, std::vector<uint64_t> &chain) {
  const Lit lits[2] = {negate(r.dom), r.forced};
  if (lits[0] == lits[1] || lits[0] == negate(lits[1]))
    return false; // degenerate or tautological: nothing to state
  if (k.value(lits[0]) != 0 || k.value(lits[1]) != 0)
    return false; // root-fixed since the probe; the binary is not news
  if (kernel_binary_live(k, lits[0], lits[1]))
    return false; // already present
  if (!congruence_replay_rup(k, lits, 2, chain, RupReplay::Decide)) {
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

ApplyResult emit_semantic(Kernel &k, const Resolvent &r,
                          std::vector<uint64_t> &chain) {
  if (negate(r.dom) != r.forced)
    return emit(k, r, chain) ? ApplyResult::Ok : ApplyResult::NoOp;

  // The binary resolvent (¬d ∨ l0) collapses to unit l0 when l0=¬d.
  // Canonical probing retains its historical division of work and still uses
  // `emit`; this unit path belongs only to the complete semantic surface.
  const Lit unit = r.forced;
  const int8_t value = k.value(unit);
  if (value > 0)
    return ApplyResult::NoOp;
  if (!congruence_replay_rup(k, &unit, 1, chain, RupReplay::Decide)) {
    ++k.stats.hbr_declined;
    return ApplyResult::NoOp;
  }
  const bool lrat = k.proof && k.proof->lrat();
  const uint64_t id = ++k.next_clause_id;
  if (k.proof)
    k.proof->add_rup(lrat ? id : 0, &unit, 1,
                     lrat ? chain.data() : nullptr,
                     lrat ? static_cast<uint32_t>(chain.size()) : 0);
  ++k.stats.hbr_units;
  if (value < 0) {
    kernel_emit_empty_from_pair(k, id, k.unit_ids[var_of(unit)]);
    return ApplyResult::Unsat;
  }
  k.unit_ids[var_of(unit)] = id;
  kernel_assign_unit(k, unit);
  if (r.subsumes) {
    Clause *reason = clause_at(k.arena, r.reason);
    if (!reason->garbage()) {
      kernel_delete_clause(k, reason);
      ++k.stats.hbr_subsumed;
    }
  }
  return ApplyResult::Ok;
}

} // namespace

ApplyResult hyperbinary_try(Kernel &k, Lit p) {
  assert(hyperbinary_guard(k));
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (var_of(p) >= k.nvars || k.value(p) != 0)
    return ApplyResult::NoOp;
  std::vector<Lit> parent(k.nvars, INVALID_LIT);
  std::vector<Resolvent> found;
  k.stats.hbr_probes++;
  if (!collect(k, p, parent, found))
    return ApplyResult::NoOp; // a failed literal is V1 Probe's inference
  std::vector<uint64_t> chain;
  bool any = false;
  // `k.work_budget` in this rule's own currency (hbr_ticks); bounded zero is
  // distinguished from the legacy zero-as-unlimited handoff.
  // One root's closure can carry thousands of resolvents and every one costs a
  // RUP replay, so this is the only place the effort ledger can bind: the
  // policy polls between INSTANCES, and one instance is this whole loop. The
  // amount is the policy's (see run_dosed_entry); the rule only obeys it.
  // Emitting fewer resolvents is always sound — the rule derives fewer
  // consequences — and it never narrows `instances()`.
  const uint64_t tick_start = k.stats.hbr_ticks;
  for (const Resolvent &r : found) {
    if (kernel_work_budget_exceeded(k, k.stats.hbr_ticks - tick_start))
      break;
    if (kernel_rule_interrupt(k, k.stats.hbr_interrupts))
      break;
    any |= emit(k, r, chain);
  }
  return any ? ApplyResult::Ok : ApplyResult::NoOp;
}

ApplyResult hyperbinary_apply(Kernel &k) {
  assert(hyperbinary_guard(k));
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
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
    // A prior probe can retain newly discovered root facts when its
    // assumptions are unwound.  Complete their closure before opening the
    // next independent probing scope.
    if (inprocess_reach_root(k))
      return ApplyResult::Unsat;
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
  return failed_literal_instances(k, out, cap);
}

static ApplyResult hyperbinary_apply_instance(Kernel &k, uint64_t inst) {
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return hyperbinary_apply(k);
  Lit p;
  if (!lit_of_instance(k, inst, &p))
    return ApplyResult::NoOp;
  return hyperbinary_try(k, p);
}

static uint32_t hyperbinary_semantic_instances(Kernel &k, uint64_t *out,
                                               uint32_t cap) {
  uint32_t count = 0;
  std::vector<uint8_t> reached(2 * k.nvars, 0);
  std::vector<Lit> queue;
  for (Lit dom = 0; dom < 2 * k.nvars; ++dom) {
    // Complete enumeration can be quadratic in the variable count. The
    // caller publishes its external stop through Kernel while this legacy
    // callback is active; return the partial prefix immediately and let the
    // typed enumerator report Interrupted rather than mistaking it for a
    // complete frontier.
    if (k.stop && *k.stop)
      return count;
    if (k.value(dom) != 0)
      continue;
    std::fill(reached.begin(), reached.end(), 0);
    queue.clear();
    reached[dom] = 1;
    queue.push_back(dom);
    for (size_t head = 0; head < queue.size(); ++head)
      for (const WatchEntry watch : k.watches.entries(negate(queue[head]))) {
        if (!watch.binary() || reached[watch.other()])
          continue;
        reached[watch.other()] = 1;
        queue.push_back(watch.other());
      }

    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *clause = clause_at(k.arena, ref);
      const uint32_t words = clause_actual_words(clause);
      if (!clause->garbage())
        for (uint32_t pos = 0; pos < clause->size; ++pos) {
          const Lit forced = clause->lits[pos];
          // forced==dom is tautological. forced==¬dom is the legal unit case.
          const bool unit = forced == negate(dom);
          if (forced == dom ||
              (unit ? k.value(forced) > 0 : k.value(forced) != 0) ||
              (!unit && kernel_binary_live(k, negate(dom), forced)))
            continue;
          bool legal = true;
          for (uint32_t other = 0; other < clause->size; ++other)
            if (other != pos && !reached[negate(clause->lits[other])]) {
              legal = false;
              break;
            }
          if (!legal)
            continue;
          if (count == cap)
            return count;
          const uint32_t word = ref + CLAUSE_HEADER_WORDS + pos;
          out[count++] = hyperbinary_semantic_instance(dom, word);
        }
      ref += words;
    }
  }
  return count;
}

static ApplyResult hyperbinary_apply_semantic_instance(Kernel &k,
                                                       uint64_t inst) {
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return hyperbinary_apply(k);
  const Lit dom = static_cast<Lit>(inst >> 32);
  const uint32_t literal_word = static_cast<uint32_t>(inst);
  if (dom == INVALID_LIT || var_of(dom) >= k.nvars ||
      literal_word >= k.arena.size() || k.value(dom) != 0)
    return ApplyResult::NoOp;

  Ref reason = INVALID_REF;
  uint32_t forced_pos = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    const uint64_t first = static_cast<uint64_t>(ref) + CLAUSE_HEADER_WORDS;
    const uint64_t end = first + clause->size;
    if (literal_word >= first && literal_word < end) {
      if (clause->garbage())
        return ApplyResult::NoOp;
      reason = ref;
      forced_pos = static_cast<uint32_t>(literal_word - first);
      break;
    }
    ref += words;
  }
  if (reason == INVALID_REF)
    return ApplyResult::NoOp;

  std::vector<uint8_t> reached(2 * k.nvars, 0);
  std::vector<Lit> queue;
  reached[dom] = 1;
  queue.push_back(dom);
  for (size_t head = 0; head < queue.size(); ++head) {
    const auto entries = k.watches.entries(negate(queue[head]));
    ++k.stats.hbr_ticks;
    for (const WatchEntry watch : entries) {
      if (!watch.binary() || reached[watch.other()])
        continue;
      reached[watch.other()] = 1;
      queue.push_back(watch.other());
    }
  }

  const Clause *clause = clause_at(k.arena, reason);
  const Lit forced = clause->lits[forced_pos];
  const bool unit = forced == negate(dom);
  if (forced == dom ||
      (unit ? k.value(forced) > 0 : k.value(forced) != 0))
    return ApplyResult::NoOp;
  for (uint32_t pos = 0; pos < clause->size; ++pos)
    if (pos != forced_pos && !reached[negate(clause->lits[pos])])
      return ApplyResult::NoOp;
  bool subsumes = false;
  for (const Lit literal : *clause)
    subsumes |= literal == negate(dom);
  const Resolvent resolvent{dom, forced, reason, subsumes};
  std::vector<uint64_t> chain;
  return emit_semantic(k, resolvent, chain);
}

TACTIX_REGISTER_RULE(HyperBinaryRes, RuleTier::Inprocess, hyperbinary_guard,
                     hyperbinary_apply, hyperbinary_instances,
                     hyperbinary_apply_instance,
                     hyperbinary_semantic_instances,
                     hyperbinary_apply_semantic_instance);

} // namespace tactix
