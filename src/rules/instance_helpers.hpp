#pragma once

#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <vector>

namespace tactix {

// The quiescent-root guard shared by the root-level inprocessing rules. Each
// rule's own guard records why it needs no further conjunct.
inline RuleEligibility root_level_guard(const Kernel &kernel) {
  RuleEligibility result;
  return result.fact("consistent", !kernel.inconsistent)
      .fact("root-level", kernel.level == 0);
}

inline bool root_literal_open(const Kernel &kernel, Lit literal) {
  return kernel.values[literal] == 0;
}

// A wholly unassigned live irredundant clause is no root reason: a reason is
// unit, so all but one of its literals are assigned. This is the shared BCE/CCE
// deletion-candidate predicate at a drained root.
inline bool root_irredundant_clause_open(const Kernel &kernel,
                                         const Clause *clause) {
  if (clause->garbage() || clause->redundant()) return false;
  for (const Lit literal : *clause)
    if (!root_literal_open(kernel, literal)) return false;
  return true;
}

// Only root-satisfied clauses may be skipped by blockedness/CLA reasoning;
// live redundant clauses and clauses containing root-false literals remain
// relevant.
inline bool root_clause_relevant(const Kernel &kernel, const Clause *clause) {
  if (clause->garbage()) return false;
  for (const Lit literal : *clause)
    if (kernel.values[literal] > 0) return false;
  return true;
}

// One RUP lemma: LRAT names `id` and cites `chain` in checker-propagation
// order; every other format writes id 0 without hints. The id-0 spelling is
// load-bearing beyond LRAT, since DSR unit staging and the PBP derived-
// constraint tracker observe the id they are handed.
inline void emit_rup_lemma(Kernel &kernel, uint64_t id, const Lit *lits,
                           uint32_t size, const uint64_t *chain,
                           uint32_t nchain) {
  if (!kernel.proof)
    return;
  const bool lrat = kernel.proof->lrat();
  kernel.proof->add_rup(lrat ? id : 0, lits, size, lrat ? chain : nullptr,
                        lrat ? nchain : 0);
}

inline void emit_rup_lemma(Kernel &kernel, uint64_t id, const Lit *lits,
                           uint32_t size, const std::vector<uint64_t> &chain) {
  emit_rup_lemma(kernel, id, lits, size, chain.data(),
                 static_cast<uint32_t>(chain.size()));
}

inline void emit_rup_lemma(Kernel &kernel, uint64_t id,
                           const std::vector<Lit> &lits,
                           const std::vector<uint64_t> &chain) {
  emit_rup_lemma(kernel, id, lits.data(), static_cast<uint32_t>(lits.size()),
                 chain);
}

// Root refutation by a clause every literal of which is root-false (`clause`,
// or the binary {first, second} when it is null). LRAT cites each literal's
// root unit and closes with the clause itself; the other formats add the bare
// empty clause. Only the proof is written: the caller owns the state change.
inline void emit_falsified_refutation(Kernel &kernel, const Clause *clause,
                                      Lit first, Lit second) {
  if (!kernel.proof)
    return;
  if (!kernel.proof->lrat()) {
    kernel.proof->add_empty(0, nullptr, 0);
    return;
  }
  kernel.lrat_entries.clear();
  const auto add = [&](Lit lit) {
    assert(kernel.value(lit) < 0);
    const Var v = var_of(lit);
    kernel.lrat_entries.emplace_back(kernel.assigned[v].trail,
                                     kernel.unit_ids[v]);
  };
  if (clause)
    for (const Lit lit : *clause)
      add(lit);
  else {
    add(first);
    add(second);
  }
  kernel_lrat_chain(kernel, clause ? clause->id()
                                   : kernel_binary_id(kernel, first, second));
  kernel.proof->add_empty(++kernel.next_clause_id, kernel.lrat_chain.data(),
                          static_cast<uint32_t>(kernel.lrat_chain.size()));
}

// Abandon a decision-free probe (the assumptions of `kernel_assign_decision`):
// drop its conflict and return to the root without saving phases. Ending the
// probing scope gives search back the watch order and `searched` hints the
// probe churned; it must run before the caller mutates the arena, because the
// rollback log holds Refs.
inline void probe_unwind(Kernel &kernel, bool end_scope = true) {
  kernel.conflict_valid = false;
  kernel.conflict_ref = INVALID_REF;
  if (kernel.level > 0)
    kernel_backtrack(kernel, 0, /*update_phases=*/false);
  if (end_scope)
    kernel_probing_end(kernel);
}

template <class Value, class Project = std::identity>
inline uint32_t copy_instance_prefix(const std::vector<Value> &values,
                                     uint64_t *out, uint32_t cap,
                                     Project project = {}) {
  const uint32_t count =
      std::min<uint32_t>(cap, static_cast<uint32_t>(values.size()));
  for (uint32_t i = 0; i < count; ++i)
    out[i] = project(values[i]);
  return count;
}

inline uint32_t copy_literal_instance_prefix(const Kernel &kernel,
                                             const std::vector<Lit> &literals,
                                             uint64_t *out, uint32_t cap) {
  return copy_instance_prefix(literals, out, cap, [&](Lit literal) {
    return lit_instance(kernel, literal);
  });
}

inline uint32_t copy_positive_variable_instance_prefix(
    const Kernel &kernel, const std::vector<Var> &variables, uint64_t *out,
    uint32_t cap) {
  return copy_instance_prefix(variables, out, cap, [&](Var variable) {
    return lit_instance(kernel, make_lit(variable, false));
  });
}

inline Ref arena_clause_instance(const Kernel &kernel, uint64_t instance) {
  for (Ref ref = 0; ref < kernel.arena.size();) {
    if (ref == instance)
      return ref;
    if (ref > instance)
      break;
    ref += clause_actual_words(clause_at(kernel.arena, ref));
  }
  return INVALID_REF;
}

template <ApplyResult (*Canonical)(Kernel &),
          ApplyResult (*Selected)(Kernel &, Lit)>
inline ApplyResult apply_root_literal_instance(Kernel &kernel,
                                               uint64_t instance) {
  if (inprocess_reach_root(kernel))
    return ApplyResult::Unsat;
  if (instance == INSTANCE_CANONICAL)
    return Canonical(kernel);
  Lit literal;
  if (!lit_of_instance(kernel, instance, &literal))
    return ApplyResult::NoOp;
  return Selected(kernel, literal);
}

template <ApplyResult (*Canonical)(Kernel &),
          ApplyResult (*Selected)(Kernel &, Ref)>
inline ApplyResult apply_root_arena_instance(Kernel &kernel,
                                             uint64_t instance) {
  if (inprocess_reach_root(kernel))
    return ApplyResult::Unsat;
  if (instance == INSTANCE_CANONICAL)
    return Canonical(kernel);
  const Ref ref = arena_clause_instance(kernel, instance);
  return ref == INVALID_REF ? ApplyResult::NoOp : Selected(kernel, ref);
}

} // namespace tactix
