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

// Every root-open literal in index order: the BCE/CCE canonical schedule.
inline void root_open_literals(const Kernel &kernel, std::vector<Lit> &out) {
  out.clear();
  for (Lit l = 0; l < 2 * kernel.nvars; l++)
    if (root_literal_open(kernel, l))
      out.push_back(l);
}

inline bool clause_contains(const Clause *clause, Lit target) {
  for (const Lit literal : *clause)
    if (literal == target)
      return true;
  return false;
}

// A wholly unassigned live irredundant clause is no root reason (the BCE/CCE
// candidate predicate).
inline bool root_irredundant_clause_open(const Kernel &kernel,
                                         const Clause *clause) {
  if (clause->garbage() || clause->redundant()) return false;
  for (const Lit literal : *clause)
    if (!root_literal_open(kernel, literal)) return false;
  return true;
}

// Only root-satisfied clauses may be skipped by blockedness/CLA reasoning.
inline bool root_clause_relevant(const Kernel &kernel, const Clause *clause) {
  if (clause->garbage()) return false;
  for (const Lit literal : *clause)
    if (kernel.values[literal] > 0) return false;
  return true;
}

// One RUP lemma; streaming PBP tracks `id`, DSR writes id 0 (needed for unit
// staging).
inline void emit_rup_lemma(Kernel &kernel, uint64_t id, const Lit *lits,
                           uint32_t size) {
  if (!kernel.proof)
    return;
  kernel.proof->add_rup(id, lits, size);
}

inline void emit_rup_lemma(Kernel &kernel, uint64_t id,
                           const std::vector<Lit> &lits) {
  emit_rup_lemma(kernel, id, lits.data(), static_cast<uint32_t>(lits.size()));
}

// Proof-only RUP lemma deleted after use; `id` is the deletion id (the clause
// id for streaming PBP, else 0).
struct TransientLemma {
  uint64_t id;
  std::vector<Lit> lits;
};

// Delete the transient lemmas, newest first, and forget them.
inline void delete_transients(Kernel &kernel,
                              std::vector<TransientLemma> &transients) {
  if (kernel.proof)
    for (size_t i = transients.size(); i-- > 0;)
      kernel.proof->del(kernel.next_clause_id, transients[i].id,
                        transients[i].lits.data(),
                        static_cast<uint32_t>(transients[i].lits.size()));
  transients.clear();
}

// Hot-loop work scalar published to its Stats counter on scope exit (Kissat's
// local `steps`), early exits included.
struct PublishedCounter {
  uint64_t &published;
  uint64_t value;
  explicit PublishedCounter(uint64_t &counter)
      : published(counter), value(counter) {}
  ~PublishedCounter() { published = value; }
};

// Abandon a decision-free probe: drop its conflict, return to root without
// saving phases and end the probing scope. Runs before arena mutation (the
// rollback log holds Refs).
inline void probe_unwind(Kernel &kernel, bool end_scope = true) {
  kernel.conflict_valid = false;
  kernel.conflict_ref = INVALID_REF;
  if (kernel.level > 0)
    kernel_backtrack(kernel, 0, /*update_phases=*/false);
  if (end_scope)
    kernel_probing_end(kernel);
}

// Retire `variable` with `status`: leaves the active pool, journals
// `journal_status`, records a retired event. The caller pushes the witness.
inline void retire_variable(Kernel &kernel, Var variable, VarStatus status,
                            const char *journal_status) {
  if (kernel.var_status.size() < kernel.nvars)
    kernel.var_status.resize(kernel.nvars, VarStatus::Active);
  if (kernel.var_status[variable] == VarStatus::Active &&
      kernel.values[make_lit(variable, false)] == 0) {
    assert(kernel.unassigned_active > 0); // it leaves the active pool
    kernel.unassigned_active--;
  }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_variable_status(kernel, variable, journal_status);
#else
  (void)journal_status;
#endif
  kernel_affine_note_namespace_change(kernel);
  kernel.var_status[variable] = status;
  kernel.retired_events.push_back(variable);
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

// v1 instance surface from a collected canonical list: literals as
// `lit_instance`, clauses as Refs.
template <void (*Collect)(const Kernel &, std::vector<Lit> &)>
inline uint32_t literal_list_instances(Kernel &kernel, uint64_t *out,
                                       uint32_t cap) {
  std::vector<Lit> literals;
  Collect(kernel, literals);
  return copy_literal_instance_prefix(kernel, literals, out, cap);
}

template <void (*Collect)(const Kernel &, std::vector<Ref> &)>
inline uint32_t arena_list_instances(Kernel &kernel, uint64_t *out,
                                     uint32_t cap) {
  std::vector<Ref> refs;
  Collect(kernel, refs);
  return copy_instance_prefix(refs, out, cap);
}

inline Ref arena_clause_instance(const Kernel &kernel, uint64_t instance) {
  for (const auto [ref, clause] : arena_records(kernel.arena)) {
    if (ref == instance)
      return ref;
    if (ref > instance)
      break;
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
