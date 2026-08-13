// Canonical pick transliterated from kissat/src/decide.c (MIT): VMTF
// last-enqueued-unassigned in focused mode, max-score-unassigned (EVSIDS
// heap, lazy pop) in stable mode; phase priority target (stable) -> saved.
#include "rules/decide/decide.hpp"
#include "policies/heuristics.hpp"

#include <algorithm>

namespace tactix {

// Pure precondition (docs/calculus.md §3 M2): no conflict, not refuted,
// some variable unassigned. No propagation-fixpoint conjunct (D-LEVEL-1), so a
// policy may take several decisions in a row — the N1 capability a learned
// policy reasons over. Off-fixpoint deciding is made safe by the
// `trail_tainted` kernel repair, not by a guard (kernel.hpp).
RuleEligibility decide_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("no-conflict", !k.conflict_valid)
      .fact("consistent", !k.inconsistent)
      .fact("has-unassigned", k.unassigned > 0);
}

ApplyResult decide_apply(Kernel &k, Lit lit, bool external) {
  if (var_of(lit) >= k.nvars || k.values[lit] != 0)
    return ApplyResult::NoOp;
  // D-LEVEL-1: a decision taken off a propagation fixpoint taints the trail —
  // a not-yet-run watch scan can rest a watch on an "ahead" assignment, so the
  // next backtrack must re-propagate the whole trail (kernel.hpp
  // `trail_tainted`). DefaultPolicy decides only at a fixpoint, so it never
  // sets this and stays bit-exact.
  if (k.propagate_head < k.trail.size())
    k.trail_tainted = true;
  k.stats.decisions++;
  if (k.first_extension_var != UINT32_MAX && var_of(lit) >= k.first_extension_var)
    k.stats.decisions_extension++; // task #11 diagnostic (see kernel.hpp)
  if (kernel_var_retired(k, var_of(lit)))
    k.stats.decisions_substituted++; // see Stats::decisions_substituted
  if (external)
    k.stats.api_decisions++;
  kernel_assign_decision(k, lit);
  return ApplyResult::Ok;
}

ApplyResult decide_apply_canonical(Kernel &k) {
  assert(k.unassigned > 0);
  if (!k.h) {
    // Unguided (no `H`, H5): lowest-index unassigned var, positive phase.
    // Sound and deterministic; only the order degrades from the ranked one.
    for (Var v = 0; v < k.nvars; v++)
      if (k.values[make_lit(v, false)] == 0)
        return decide_apply(k, make_lit(v, false), false);
    return ApplyResult::NoOp; // unreachable given unassigned > 0
  }
  const Var v = heuristics_next_decision_var(k);
  if (v == INVALID_VAR) // guard promised one; never index `H` with a sentinel
    return ApplyResult::NoOp;
  return decide_apply(k, heuristics_decide_lit(k, v), false);
}

// N1 completeness tail (`N1-RETIRED-DECIDE`). `H`'s ranked structures hold only
// the variables the POLICY still branches on: retirement dequeues a substituted
// or eliminated variable from the VMTF queue and the EVSIDS heap, and a lazy
// heap pop drops it too. Those variables can still be unassigned, the guard
// counts them in `unassigned`, and `apply_instance` decides them — so leaving
// them out of the enumeration would narrow the API surface to the default
// policy's own action space, which N1 forbids. Ranked order first, then
// whatever the ranked walk could not reach, in index order.
// Only runs when the ranked walk left room, so a capped caller pays nothing.
static uint32_t decide_candidate_tail(const Kernel &k, Lit *out, uint32_t cap,
                                      uint32_t n) {
  if (n >= cap)
    return n;
  std::vector<uint8_t> emitted(k.nvars, 0);
  for (uint32_t i = 0; i < n; i++)
    emitted[var_of(out[i])] = 1;
  for (Var v = 0; v < k.nvars && n < cap; v++)
    if (!emitted[v] && k.values[make_lit(v, false)] == 0)
      out[n++] = heuristics_decide_lit(k, v);
  return n;
}

uint32_t decide_candidates(const Kernel &k, Lit *out, uint32_t cap) {
  uint32_t n = 0;
  if (!k.h) {
    // Unguided: index order, positive phase (N1 — every unassigned literal is a
    // legal candidate; the ranking is what `H` would add).
    for (Var v = 0; v < k.nvars && n < cap; v++)
      if (k.values[make_lit(v, false)] == 0)
        out[n++] = make_lit(v, false);
    return n;
  }
  if (k.h->stable) {
    // rank unassigned vars by score, descending. Introspection path, not hot:
    // a sort + allocation is fine; the heap array is not fully ordered.
    std::vector<Var> scratch;
    scratch.reserve(k.unassigned);
    for (Var v = 0; v < k.nvars; v++)
      if (k.values[make_lit(v, false)] == 0)
        scratch.push_back(v);
    std::stable_sort(scratch.begin(), scratch.end(),
                     [&](Var a, Var b) {
                       return k.h->scores.score[a] > k.h->scores.score[b];
                     });
    for (const Var v : scratch) {
      if (n >= cap)
        break;
      out[n++] = heuristics_decide_lit(k, v);
    }
    return n;
  }
  // walk the VMTF queue from most-recent to least, unassigned vars only
  for (Var v = k.h->search_idx; v != UINT32_MAX && n < cap;
       v = k.h->links[v].prev)
    if (k.values[make_lit(v, false)] == 0)
      out[n++] = heuristics_decide_lit(k, v);
  return decide_candidate_tail(k, out, cap, n);
}

// instance surface: payload = the DIMACS literal in the shared lit_instance
// encoding (registry.hpp), enumerated in the ranked candidate order. BOTH
// polarities of each candidate variable are offered: the calculus makes M2's
// instance surface *any* unassigned literal (docs/calculus.md §3 M2), so the
// phase is a dimension of the instance and not the heuristic's to fix. The
// ranked phase is emitted first, so a caller that keeps only a prefix still
// sees the canonical pick ahead of its complement.
static uint32_t decide_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  const uint32_t vars = (cap + 1) / 2;
  std::vector<Lit> buf(vars);
  const uint32_t n = decide_candidates(k, buf.data(), vars);
  uint32_t m = 0;
  for (uint32_t i = 0; i < n && m < cap; i++) {
    out[m++] = lit_instance(k, buf[i]);
    if (m < cap)
      out[m++] = lit_instance(k, negate(buf[i]));
  }
  return m;
}

static ApplyResult decide_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return decide_apply_canonical(k);
  Lit l;
  if (!lit_of_instance(k, inst, &l))
    return ApplyResult::NoOp;
  return decide_apply(k, l, true);
}

TACTIX_REGISTER_RULE(Decide, RuleTier::Micro, decide_guard,
                     decide_apply_canonical, decide_instances,
                     decide_apply_instance);

} // namespace tactix
