#pragma once

#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace tactix {

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

template <class Value>
inline uint32_t copy_instance_prefix(const std::vector<Value> &values,
                                     uint64_t *out, uint32_t cap) {
  const uint32_t count =
      std::min<uint32_t>(cap, static_cast<uint32_t>(values.size()));
  for (uint32_t i = 0; i < count; ++i)
    out[i] = values[i];
  return count;
}

inline uint32_t copy_literal_instance_prefix(const Kernel &kernel,
                                             const std::vector<Lit> &literals,
                                             uint64_t *out, uint32_t cap) {
  const uint32_t count =
      std::min<uint32_t>(cap, static_cast<uint32_t>(literals.size()));
  for (uint32_t i = 0; i < count; ++i)
    out[i] = lit_instance(kernel, literals[i]);
  return count;
}

inline uint32_t copy_positive_variable_instance_prefix(
    const Kernel &kernel, const std::vector<Var> &variables, uint64_t *out,
    uint32_t cap) {
  const uint32_t count =
      std::min<uint32_t>(cap, static_cast<uint32_t>(variables.size()));
  for (uint32_t i = 0; i < count; ++i)
    out[i] = lit_instance(kernel, make_lit(variables[i], false));
  return count;
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
