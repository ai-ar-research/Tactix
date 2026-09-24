#include "rules/condition/condition.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace tactix {
namespace {

int8_t lit_value(Lit lit) { return negated(lit) ? -1 : 1; }

bool live_clause_is(const Clause *candidate, const std::vector<Lit> &sorted) {
  if (candidate->garbage() || candidate->size != sorted.size())
    return false;
  std::vector<Lit> lits(candidate->begin(), candidate->end());
  std::sort(lits.begin(), lits.end());
  return lits == sorted;
}

// Test the local implication ¬C ∧ D => T_W(D). A satisfying assignment of D
// before overwrite but not afterward is a counterexample. Under ¬T_W(D),
// every unaffected literal of D must be false; a witness-true literal makes
// the transformed clause true immediately. The remaining assignment is only
// units, so no SAT search or clause-shape restriction is involved.
bool preserved(const Lit *lits, uint32_t size,
               const std::vector<int8_t> &witness_values,
               std::vector<int8_t> &forced,
               std::vector<Var> &temporary) {
  for (uint32_t i = 0; i < size; ++i) {
    const Lit lit = lits[i];
    if (witness_values[var_of(lit)] == lit_value(lit))
      return true;
  }
  temporary.clear();
  bool consistent = true;
  for (uint32_t i = 0; i < size; ++i) {
    const Lit lit = lits[i];
    const Var v = var_of(lit);
    if (witness_values[v])
      continue;
    const int8_t want = -lit_value(lit);
    if (forced[v] && forced[v] != want) {
      consistent = false;
      break;
    }
    if (!forced[v]) {
      forced[v] = want;
      temporary.push_back(v);
    }
  }
  bool counterexample = false;
  if (consistent)
    for (uint32_t i = 0; i < size; ++i) {
      const Lit lit = lits[i];
      if (forced[var_of(lit)] != -lit_value(lit)) {
        counterexample = true;
        break;
      }
    }
  for (const Var v : temporary)
    forced[v] = 0;
  return !counterexample;
}

} // namespace

RuleEligibility condition_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0)
      .fact("no-conflict", !k.conflict_valid)
      .fact("no-pending", !k.pending.valid)
      .fact("propagated", kernel_propagation_fixpoint(k));
}

bool condition_prepare(Kernel &k, const ConditionInstance &instance,
                       ConditionPrepared &prepared) {
  if (!condition_guard(k) || (k.stop && *k.stop))
    return false;
  const auto &clause = instance.clause;
  const auto &witness = instance.witness;
  if (clause.size() < 2 || witness.empty() ||
      clause.size() > UINT32_MAX || witness.size() > UINT32_MAX)
    return false;

  std::vector<Lit> sorted = clause;
  std::sort(sorted.begin(), sorted.end());
  std::vector<int8_t> witness_values(k.nvars, 0);
  std::vector<Var> witness_external_vars;
  witness_external_vars.reserve(witness.size());
  bool satisfies_candidate = false;
  bool root_satisfies_candidate = false;
  for (size_t i = 0; i < sorted.size(); ++i) {
    const Lit lit = sorted[i];
    if (var_of(lit) >= k.nvars ||
        (i && var_of(sorted[i - 1]) == var_of(lit)))
      return false;
    root_satisfies_candidate |= k.values[lit] > 0;
  }
  for (const Lit external : witness) {
    const Var ev = var_of(external);
    if (ev >= k.external_to_internal.size())
      return false;
    witness_external_vars.push_back(ev);
    if (ev < k.fixed_external_values.size() &&
        k.fixed_external_values[ev] &&
        k.fixed_external_values[ev] != lit_value(external))
      return false;
    const Var v = k.external_to_internal[ev];
    if (v == INVALID_VAR)
      continue; // no live clause can mention this compacted identity
    const Lit lit = make_lit(v, negated(external));
    if (k.values[lit] < 0)
      return false;
    witness_values[v] = lit_value(lit);
    satisfies_candidate |= std::binary_search(sorted.begin(), sorted.end(), lit);
  }
  std::sort(witness_external_vars.begin(), witness_external_vars.end());
  if (std::adjacent_find(witness_external_vars.begin(),
                         witness_external_vars.end()) !=
      witness_external_vars.end())
    return false;
  if (!satisfies_candidate)
    return false;

  Ref target = INVALID_REF;
  if (sorted.size() == 2) {
    if (!kernel_binary_live(k, sorted[0], sorted[1]))
      return false;
  } else {
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *current = clause_at(k.arena, ref);
      if (live_clause_is(current, sorted)) {
        target = ref;
        break;
      }
      if (k.stop && *k.stop)
        return false;
      ref += clause_actual_words(current);
    }
    if (target == INVALID_REF)
      return false;
  }

  // A permanent root fact already satisfies C, so no model extending the
  // roots can trigger its cube repair. Deletion still retires a possible root
  // reason through the kernel primitive, publishing the DSR root unit first.
  // In this case ¬C has no root-consistent assignment and the local implication
  // scan is vacuous; forcing ¬C would overwrite a root value incorrectly.
  if (!root_satisfies_candidate) {
    std::vector<int8_t> forced(k.nvars, 0);
    for (Var v = 0; v < k.nvars; ++v)
      forced[v] = k.values[make_lit(v, false)];
    for (const Lit lit : sorted)
      forced[var_of(lit)] = -lit_value(lit);
    std::vector<Var> temporary;
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *current = clause_at(k.arena, ref);
      // Learned clauses are entailed by the irredundant formula (F |= R).
      // Overwriting a model that satisfies every irredundant clause, including
      // the restored candidate, therefore satisfies R too. Checking each R
      // clause locally would reject valid CaDiCaL-style conditional autarkies.
      if (!current->garbage() && !current->redundant() &&
          !preserved(current->lits, current->size, witness_values, forced,
                     temporary))
        return false;
      if (k.stop && *k.stop)
        return false;
      ref += clause_actual_words(current);
    }
    for (Lit a = 0; a < 2 * k.nvars; ++a) {
      for (const WatchEntry entry : k.watches.entries(a)) {
        if (!entry.binary() || entry.other() <= a)
          continue;
        const Lit pair[2] = {a, entry.other()};
        if (!preserved(pair, 2, witness_values, forced, temporary))
          return false;
        if (k.stop && *k.stop)
          return false;
      }
      if (k.stop && *k.stop)
        return false;
    }
  }
  if (k.stop && *k.stop)
    return false;

  // Reserve the proof-neutral reconstruction record before a deletion can
  // become visible. The tagged segment contains both full variable-length
  // vectors; no fixed cube-size or witness-in-clause assumption is made.
  const size_t words = clause.size() + witness.size() + 3;
  if (words < clause.size() ||
      words > k.reconstruction.max_size() - k.reconstruction.size())
    return false;
  k.reconstruction.reserve(k.reconstruction.size() + words);
  prepared.target = target;
  if (target == INVALID_REF)
    prepared.first = sorted[0], prepared.second = sorted[1];
  return true;
}

ApplyResult condition_commit(Kernel &k, const ConditionInstance &instance,
                             const ConditionPrepared &prepared) {
  const auto &clause = instance.clause;
  const auto &witness = instance.witness;
  if (prepared.target == INVALID_REF) {
    if (!kernel_delete_binary_all_copies(k, prepared.first, prepared.second))
      return ApplyResult::NoOp;
  } else {
    Clause *current = clause_at(k.arena, prepared.target);
    kernel_delete_clause(k, current);
  }
  kernel_push_cube_witness(k, clause.data(),
                           static_cast<uint32_t>(clause.size()),
                           witness.data(),
                           static_cast<uint32_t>(witness.size()));
  return ApplyResult::Ok;
}

bool condition_apply_explicit(Kernel &k, const ConditionInstance &instance) {
  ConditionPrepared prepared;
  return condition_prepare(k, instance, prepared) &&
         condition_commit(k, instance, prepared) == ApplyResult::Ok;
}

ApplyResult condition_apply(Kernel &) {
  // General cube discovery belongs to a policy producer. Explicit caller
  // tuples are never constrained by that producer's budget or ordering.
  return ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(Condition, RuleTier::Inprocess, condition_guard,
                     condition_apply);

} // namespace tactix
