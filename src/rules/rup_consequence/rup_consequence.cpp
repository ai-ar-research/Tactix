#include "rules/rup_consequence/rup_consequence.hpp"
#include "proof/proof.hpp"
#include "rules/structural_action_guard.hpp"
#include <algorithm>
#include <unordered_map>

namespace tactix {
namespace {
struct Work {
  const Kernel &kernel;
  uint64_t remaining;
  bool take(uint64_t amount = 1) {
    if (amount > remaining || kernel_terminating(kernel)) return false;
    remaining -= amount;
    return true;
  }
};

bool normalize(const Kernel &k, const std::vector<int32_t> &external,
               Work &work, std::vector<Lit> &literals) {
  if (!work.take(external.size())) return false;
  literals.reserve(external.size());
  for (int32_t x : external) {
    if (!x || x == INT32_MIN) return false;
    const Lit lit = kernel_internalize(k, x);
    if (lit == INVALID_LIT || kernel_var_retired(k, var_of(lit))) return false;
    literals.push_back(lit);
  }
  std::sort(literals.begin(), literals.end());
  literals.erase(std::unique(literals.begin(), literals.end()), literals.end());
  return true;
}

bool live_premise(const Kernel &k, const std::vector<Lit> &clause, Work &work) {
  if (!work.take() || clause.empty()) return false;
  if (clause.size() == 1) return k.value(clause[0]) > 0;
  if (clause.size() == 2) {
    // Inspect the shorter watch list with explicit work accounting.
    Lit a = clause[0], b = clause[1];
    if (k.watches.size(b) < k.watches.size(a)) std::swap(a, b);
    for (const auto watch : k.watches.entries(a)) {
      if (!work.take()) return false;
      if (watch.binary() && watch.other() == b) return true;
    }
    return false;
  }
  // A connected large clause has two watches among its literals. Try small
  // lists first, but inspect all if necessary: a low-degree literal need not
  // itself be a watch. This remains valid after propagation moves watches.
  auto probes = clause;
  std::sort(probes.begin(), probes.end(), [&](Lit a, Lit b) {
    return k.watches.size(a) < k.watches.size(b);
  });
  for (Lit literal : probes)
    for (const auto watch : k.watches.entries(literal)) {
      if (!work.take()) return false;
      if (watch.binary()) continue;
      const Clause *source = clause_at(k.arena, watch.ref());
      if (source->garbage() || source->size != clause.size()) continue;
      bool same = true;
      for (Lit wanted : clause) {
        bool found = false;
        for (Lit actual : *source) {
          if (!work.take()) return false;
          if (actual == wanted) { found = true; break; }
        }
        if (!found) { same = false; break; }
      }
      if (same) return true;
    }
  return false;
}
}

RuleEligibility rup_consequence_guard(const Kernel &k) {
  return root_action_guard(k, false).clausal_certificate()
      .fact("not-terminated", !kernel_terminating(k))
      .fact("proof-id-available", k.next_clause_id < uint64_t(INT64_MAX))
      .finish();
}

bool rup_consequence_prepare(const Kernel &k, const std::vector<int32_t> &clause,
    const std::vector<std::vector<int32_t>> &premises, uint64_t maximum_work,
    RupConsequencePlan &out) {
  if (!rup_consequence_guard(k) || !maximum_work) return false;
  Work work{k, maximum_work};
  RupConsequencePlan plan;
  if (!normalize(k, clause, work, plan.clause)) return false;
  for (size_t i = 0; i < plan.clause.size(); ++i) {
    if (k.value(plan.clause[i]) > 0 ||
        (i && plan.clause[i-1] == negate(plan.clause[i]))) {
      plan.already_true = true; out = std::move(plan); return true;
    }
  }
  if (k.arena.size() >= INVALID_REF || plan.clause.size() > UINT32_MAX ||
      uint64_t(CLAUSE_HEADER_WORDS) + plan.clause.size() >= INVALID_REF-k.arena.size())
    return false;
  std::vector<std::vector<Lit>> source;
  if (!work.take(premises.size())) return false;
  source.reserve(premises.size());
  for (const auto &external : premises) {
    source.emplace_back();
    if (!normalize(k, external, work, source.back()) ||
        !live_premise(k, source.back(), work)) return false;
  }
  std::unordered_map<Var, bool> values;
  for (Lit literal : plan.clause) values[var_of(literal)] = negated(literal);
  bool changed;
  do {
    changed = false;
    for (const auto &row : source) {
      if (!work.take(1 + row.size())) return false;
      bool satisfied = false;
      size_t unassigned = 0;
      Lit unit = INVALID_LIT;
      for (Lit literal : row) {
        const auto it = values.find(var_of(literal));
        if (it == values.end()) { ++unassigned; unit = literal; }
        else if (it->second != negated(literal)) { satisfied = true; break; }
      }
      if (satisfied) continue;
      if (!unassigned) { out = std::move(plan); return true; }
      if (unassigned == 1) { values[var_of(unit)] = !negated(unit); changed = true; }
    }
  } while (changed);
  return false;
}

ApplyResult rup_consequence_commit(Kernel &k, const RupConsequencePlan &plan) {
  if (plan.already_true) return ApplyResult::NoOp;
  k.close_startup_certificate();
  const auto &clause = plan.clause;
  const uint64_t id = ++k.next_clause_id;
  if (k.proof) k.proof->add_rup(id, clause.data(), clause.size());
  if (clause.empty()) { k.inconsistent = true; return ApplyResult::Unsat; }
  if (clause.size() == 1) {
    const Lit unit = clause[0];
    if (k.value(unit) < 0) {
      k.inconsistent = true; kernel_emit_empty(k); return ApplyResult::Unsat;
    }
    k.unit_ids[var_of(unit)] = id;
    kernel_assign_unit(k, unit);
  } else if (clause.size() == 2) {
    kernel_add_binary_clause(k, clause[0], clause[1], id);
  } else {
    kernel_add_large_clause(k, clause.data(), clause.size(), true, 0, id);
  }
  return ApplyResult::Ok;
}

TACTIX_REGISTER_OPT_IN_RULE(RupConsequence, RuleTier::Inprocess,
    rup_consequence_guard, [](Kernel &) { return ApplyResult::NoOp; });
} // namespace tactix
