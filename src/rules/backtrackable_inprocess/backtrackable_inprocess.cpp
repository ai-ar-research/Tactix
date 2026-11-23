#include "rules/backtrackable_inprocess/backtrackable_inprocess.hpp"
#include "kernel/query_projection.hpp"
#include "rules/instance_helpers.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"

#include <algorithm>
#include <array>

namespace tactix {

RuleEligibility backtrackable_subsume_guard(const Kernel &k) {
  return RuleEligibility{}
      .fact("live-search-trail", k.level && !k.inconsistent && !k.trail_tainted)
      .fact("sparse-watches", k.large_watches_connected && !k.dense)
      .fact("no-conflict-or-probe",
            !k.conflict_valid && !k.pending.valid && !k.probing.recording)
      .fact("propagation-fixpoint", kernel_propagation_fixpoint(k))
      .fact("stash-available", k.stashed_clause == INVALID_REF);
}

// Outside the victim, every literal of the witness must be false. The
// highest such assignment level is where the implication stops holding on
// backtrack. Shared false literals contribute no dependency.
template <bool Strengthen>
static uint32_t subsumption_level(const Kernel &k, const Clause &witness,
                                  const Clause &victim, Lit &pivot) {
  uint32_t level = 0;
  for (const Lit lit : witness) {
    if (std::find(victim.begin(), victim.end(), lit) != victim.end())
      continue;
    if (k.value(lit) < 0) {
      level = std::max(level, k.var_level(var_of(lit)));
      continue;
    }
    if constexpr (Strengthen) {
      if (pivot == INVALID_LIT && k.value(lit) == 0 &&
          std::find(victim.begin(), victim.end(), negate(lit)) != victim.end()) {
        pivot = negate(lit);
        continue;
      }
    }
    return 0;
  }
  return (Strengthen == (pivot != INVALID_LIT)) ? level : 0;
}

static bool has_open_watches(const Kernel &k, const Clause &c) {
  return k.value(c.lits[0]) == 0 && k.value(c.lits[1]) == 0 &&
         std::none_of(c.begin() + 2, c.end(),
                      [&](Lit lit) { return k.value(lit) > 0; });
}

constexpr uint32_t MAX_CLAUSE_SIZE = 12;

// Keep context literals in the deputy, so its full resolvent is globally RUP.
// A deputy only goes live with two currently open watches.
static Ref publish_resolvent(Kernel &k, std::span<Lit> resolvent,
                              uint32_t size_limit) {
  const auto size = static_cast<uint32_t>(resolvent.size());
  if (size < 3 || size > size_limit)
    return INVALID_REF;
  const auto first_false = std::partition(resolvent.begin(), resolvent.end(),
                                          [&](Lit l) { return !k.value(l); });
  if (first_false - resolvent.begin() < 2)
    return INVALID_REF;
  const uint64_t id = ++k.next_clause_id;
  emit_rup_lemma(k, id, resolvent.data(), size);
  return kernel_add_large_clause(k, resolvent.data(), size, true, size, id);
}

static Ref resolve_for_strengthening(Kernel &k, const Clause &witness,
                                     const Clause &victim, Lit pivot) {
  std::array<Lit, 2 * MAX_CLAUSE_SIZE> resolvent;
  auto end = std::copy_if(victim.begin(), victim.end(), resolvent.begin(),
                          [&](Lit lit) { return lit != pivot; });
  for (const Lit lit : witness)
    if (lit != negate(pivot) &&
        std::find(victim.begin(), victim.end(), lit) == victim.end())
      *end++ = lit;
  return publish_resolvent(k, std::span(resolvent.begin(), end), victim.size);
}

template <bool Strengthen>
static ApplyResult apply_backtrackable_sumption(Kernel &k) {
  assert(backtrackable_subsume_guard(k));
  // Work stays bounded without building an occurrence index during search.
  // Both clauses have two open watches and are unsatisfied, so the victim
  // cannot be a live reason.
  constexpr uint32_t max_records = 32;
  std::array<Ref, max_records> candidates;
  uint32_t count = 0;
  uint32_t records = 0;
  for (const auto [ref, c] : arena_records(k.arena)) {
    if (records++ == max_records)
      break;
    if (c->garbage() || c->size > MAX_CLAUSE_SIZE ||
        !has_open_watches(k, *c))
      continue;
    candidates[count++] = ref;
  }
  for (uint32_t i = 0; i < count; ++i) {
    const Clause &victim = *clause_at(k.arena, candidates[i]);
    if (victim.redundant())
      continue;
    for (uint32_t j = 0; j < count; ++j) {
      if (i == j)
        continue;
      const Clause &witness = *clause_at(k.arena, candidates[j]);
      Lit pivot = INVALID_LIT;
      const uint32_t level = subsumption_level<Strengthen>(k, witness, victim,
                                                          pivot);
      if (!level)
        continue;
      Ref deputy = candidates[j];
      if constexpr (Strengthen) {
        deputy = resolve_for_strengthening(k, witness, victim, pivot);
        if (deputy == INVALID_REF)
          continue;
      }
      kernel_stash_clause(k, candidates[i], level, deputy, Strengthen);
      return ApplyResult::Ok;
    }
  }
  return ApplyResult::NoOp;
}

ApplyResult backtrackable_subsume_apply(Kernel &k) {
  return apply_backtrackable_sumption<false>(k);
}

ApplyResult backtrackable_strengthen_apply(Kernel &k) {
  return apply_backtrackable_sumption<true>(k);
}

RuleEligibility backtrackable_eliminate_guard(const Kernel &k) {
  return backtrackable_subsume_guard(k)
      .fact("no-affine-matrix", !k.affine_matrix_search_session ||
                               !k.affine_matrix_search_session->enabled());
}

// This bounded case has exactly one parent of each sign and no other live
// occurrence. For correctness the scan includes learned clauses and binaries,
// including clauses already satisfied by the current trail.
static bool elimination_parents(const Kernel &k, Var v,
                                 std::array<Ref, 2> &parents) {
  const Lit positive = make_lit(v, false);
  if (k.watches.binary_size(positive) || k.watches.binary_size(negate(positive)))
    return false;
  parents.fill(INVALID_REF);
  for (const auto [ref, c] : arena_records(k.arena)) {
    if (c->garbage())
      continue;
    for (const Lit l : *c) {
      if (var_of(l) != v)
        continue;
      Ref &parent = parents[negated(l)];
      if (parent != INVALID_REF || c->redundant() ||
          c->size > MAX_CLAUSE_SIZE || !has_open_watches(k, *c))
        return false;
      parent = ref;
    }
  }
  return parents[0] != INVALID_REF && parents[1] != INVALID_REF;
}

static Ref resolve_elimination_parents(Kernel &k, Var v,
                                       const std::array<Ref, 2> &parents) {
  std::array<Lit, 2 * MAX_CLAUSE_SIZE> resolvent;
  auto end = resolvent.begin();
  uint32_t max_size = 0;
  for (const Ref ref : parents) {
    const Clause &parent = *clause_at(k.arena, ref);
    max_size = std::max(max_size, parent.size);
    for (const Lit l : parent) {
      if (var_of(l) == v || std::find(resolvent.begin(), end, l) != end)
        continue;
      if (std::find(resolvent.begin(), end, negate(l)) != end)
        return INVALID_REF;
      *end++ = l;
    }
  }
  return publish_resolvent(k, std::span(resolvent.begin(), end), max_size);
}

ApplyResult backtrackable_eliminate_apply(Kernel &k) {
  assert(backtrackable_eliminate_guard(k));
  // Until there is a search-level occurrence index, charge a fixed scan
  // allowance and use only pivots for which the whole population can be read.
  constexpr size_t max_arena_words = 2048;
  constexpr size_t max_pivots = 8;
  if (k.arena.size() > max_arena_words)
    return ApplyResult::NoOp;
  std::array<Var, max_pivots> seen;
  auto end = seen.begin();
  for (const auto [ref, c] : arena_records(k.arena)) {
    (void)ref;
    if (c->garbage() || c->redundant() || c->size > MAX_CLAUSE_SIZE ||
        !has_open_watches(k, *c))
      continue;
    for (const Lit l : *c) {
      if (end == seen.end())
        return ApplyResult::NoOp;
      const Var v = var_of(l);
      if (k.value(l) || kernel_var_retired(k, v) ||
          std::find(seen.begin(), end, v) != end)
        continue;
      *end++ = v;
      const Var external = var_of(kernel_external_lit(k, l));
      if (k.variable_retention.contains(external) ||
          (k.query_projection && k.query_projection->constrains(external)))
        continue;
      std::array<Ref, 2> parents;
      if (!elimination_parents(k, v, parents))
        continue;
      const Ref deputy = resolve_elimination_parents(k, v, parents);
      if (deputy == INVALID_REF)
        continue;
      kernel_stash_elimination(k, v, parents[0], parents[1], deputy);
      return ApplyResult::Ok;
    }
  }
  return ApplyResult::NoOp;
}

template <auto Guard>
static RuleEligibility incremental_guard(const Kernel &k) {
  return Guard(k).fact("incremental-query", k.query_projection != nullptr);
}

TACTIX_REGISTER_RULE(BacktrackableSubsume, RuleTier::Macro,
                     incremental_guard<backtrackable_subsume_guard>,
                     backtrackable_subsume_apply, nullptr, nullptr);
TACTIX_REGISTER_RULE(BacktrackableStrengthen, RuleTier::Macro,
                     incremental_guard<backtrackable_subsume_guard>,
                     backtrackable_strengthen_apply, nullptr, nullptr);
TACTIX_REGISTER_RULE(BacktrackableEliminate, RuleTier::Macro,
                     incremental_guard<backtrackable_eliminate_guard>,
                     backtrackable_eliminate_apply, nullptr, nullptr);

} // namespace tactix
