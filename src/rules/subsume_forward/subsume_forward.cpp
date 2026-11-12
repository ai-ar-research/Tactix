// See subsume_forward.hpp for the rule's contract.
#include "rules/subsume_forward/subsume_forward.hpp"

#include "rules/instance_helpers.hpp"
#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

RuleEligibility subsume_forward_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct: a pure F-rewrite is sound
  // regardless of a recorded conflict (D-SPLIT-3, `docs/dev/calculus.md` §7).
  return root_level_guard(k);
}

// Does live irredundant `d` subsume the marked candidate (every literal marked
// or root-false)? A root-satisfied d never subsumes (cadical subsume_check).
static bool subsumes_marked(const Kernel &k, const std::vector<uint8_t> &marks,
                            const Clause *d) {
  for (const Lit l : *d) {
    if (marks[l])
      continue;
    const int8_t v = k.value(l);
    if (v > 0)
      return false; // satisfied subsumer: unusable
    if (v == 0)
      return false; // free literal outside the candidate
    // root-falsified: absorbed by the units
  }
  return true;
}

// Victim: live, not root-satisfied. Redundant victims are licensed by Forget;
// the redundancy check sits on the subsumer (subsume_forward.hpp).
static bool is_victim(const Kernel &k, const Clause *c) {
  return root_clause_relevant(k, c);
}

// Round scope: the redundant tier band belongs to C2 Strengthen, not this rule
// (ledger row D-C1C2-VICTIM).
static bool is_round_candidate(const Kernel &k, const Clause *c) {
  return !c->redundant() &&
         c->size <= k.subsume_forward_parameters.clause_size_limit &&
         is_victim(k, c);
}

void subsume_forward_candidates(const Kernel &k, std::vector<Ref> &out,
                                bool complete) {
  out.clear();
  // cache the sort key instead of re-dereferencing clause headers in the
  // comparator (reduce_candidates' Victim pattern)
  struct Cand {
    Ref ref;
    uint32_t size;
  };
  std::vector<Cand> cands;
  for (const auto [ref, c] : arena_records(k.arena)) {
    if (complete ? is_victim(k, c) : is_round_candidate(k, c))
      cands.push_back({ref, c->size});
  }
  std::sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) {
    if (a.size != b.size)
      return a.size < b.size;
    return a.ref < b.ref; // total order for determinism
  });
  out.reserve(cands.size());
  for (const Cand &c : cands)
    out.push_back(c.ref);
}

// Policy, not precondition (N1): only the canonical round uses the repetition
// filter and the redundancy scope (tests/test_subsume.cpp
// ::test_touch_filter_is_policy_only). Mark the candidate's free literals;
// there may be fewer than two under N1 histories, which stays sound.
static uint32_t mark_candidate(const Kernel &k, std::vector<uint8_t> &marks,
                               const Clause *c, uint8_t on) {
  uint32_t free_lits = 0;
  for (const Lit l : *c) {
    assert(k.value(l) <= 0);
    if (k.value(l) == 0) {
      marks[l] = on;
      free_lits++;
    }
  }
  return free_lits;
}

static bool watch_population_within(const Kernel &k, Lit lit, bool binary,
                                    uint32_t limit) {
  if (binary)
    return k.watches.binary_size(lit) <= limit;
  uint32_t count = 0;
  // Large entries still require the complete entry-aware walk: unlike the
  // exact binary population, lazy garbage remains part of this physical cap.
  for (const WatchEntry w : k.watches.entries(lit))
    if (!w.binary() && ++count > limit)
      return false;
  return true;
}

// Binary subsumers (l, o) with both marked, from the free literals' watch
// lists; oversized lists are skipped (kissat forward_literal limit).
static bool subsumed_by_binary(Kernel &k,
                               const std::vector<uint8_t> &marks,
                               const Clause *c, uint64_t &visits,
                               uint64_t visit_start, bool bounded,
                               uint64_t visit_budget,
                               bool &budget_exhausted) {
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    if (!watch_population_within(
            k, l, true,
            k.subsume_forward_parameters.binary_occurrence_limit))
      continue;
    const bool stop_at_large = k.watches.heads[l].binaries_first();
    for (const WatchEntry w : k.watches.entries(l)) {
      if (!w.binary()) {
        if (stop_at_large)
          break;
        continue;
      }
      visits++;
      if (bounded && visits - visit_start >= visit_budget) {
        budget_exhausted = true;
        return false;
      }
      if (marks[w.other()])
        return true;
    }
  }
  return false;
}

static void delete_subsumed(Kernel &k, Clause *c) {
  kernel_delete_clause(k, c);
  k.stats.forward_subsumed++;
}

// Canonical instance: one round. Candidates ascending by size are checked
// against binaries and earlier connected candidates, then connected at their
// least-occurring free literal. Connected clauses are never deleted and nothing
// allocates, so refs stay valid.
ApplyResult subsume_forward_apply(Kernel &k) {
  assert(subsume_forward_guard(k));
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.forward_subsumptions++; // rounds (kissat forward.c stat name)
  // Snapshot the epoch first so this round's own material is new to the next
  // (kissat forward.c:589-645).
  kernel_touch_sync_fixed(k);
  const bool filter = k.touch_filter;
  const uint64_t snapshot = k.touch_epoch;
  const uint64_t wm = k.subsume_touch_mark;
  std::vector<Ref> candidates;
  subsume_forward_candidates(k, candidates);
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  std::vector<std::vector<Ref>> occs(2 * k.nvars);
  // Kissat `connect_subsuming` selects from the complete dense population
  // (forward.c:467-507); build it for direct canonical calls.
  std::vector<uint32_t> fallback_population;
  if (!k.dense) {
    fallback_population.assign(2 * k.nvars, 0);
    for (const auto [ref, c] : arena_records(k.arena)) {
      if (!c->garbage() && !c->redundant())
        for (const Lit l : *c)
          fallback_population[l]++;
    }
  }
  const auto watch_population = [&](Lit l) -> size_t {
    const size_t large =
        k.dense ? k.dense->large_size(l) : fallback_population[l];
    return k.watches.binary_size(l) + large;
  };
  uint64_t deleted = 0;
  // `k.work_budget` polled in partner visits (Kissat forward_literal `steps`);
  // survivors stay candidates. 0 = unbounded (ledger D-STRENGTHEN-BUDGET).
  PublishedCounter visits(k.stats.forward_visits);
  const uint64_t check_start = visits.value;
  const bool visit_bounded = k.midround_budget;
  const uint64_t visit_budget = k.work_budget;
  size_t stopped_at = candidates.size(); // index of the first unreached one
  // §6 termination polling at the budget's cut point, one poll per candidate
  // and no tick cadence; an interrupted round is an incomplete round.
  bool interrupted = false;
  for (size_t ci = 0; ci < candidates.size(); ci++) {
    const Ref ref = candidates[ci];
    if (visit_bounded && visits.value - check_start >= visit_budget) {
      k.stats.budget_stops_c1++;
      stopped_at = ci;
      break;
    }
    if (kernel_rule_interrupt(k, k.stats.subsume_interrupted)) {
      stopped_at = ci;
      interrupted = true;
      break;
    }
    Clause *c = clause_at(k.arena, ref);
    // Unchecked candidates are still connected so the index stays complete
    // (kissat drops them, forward.c:488-492).
    const bool check = !filter || kernel_clause_new_material(k, c, wm);
    bool subsumed = false;
    bool budget_exhausted = false;
    if (check) {
      k.stats.forward_checked++;
      mark_candidate(k, marks, c, 1);
      subsumed = subsumed_by_binary(k, marks, c, visits.value, check_start,
                                    visit_bounded, visit_budget,
                                    budget_exhausted);
      if (!subsumed) {
        for (const Lit l : *c) {
          if (budget_exhausted)
            break;
          if (k.value(l) != 0)
            continue;
          const std::vector<Ref> &os = occs[l];
          if (os.size() >
              k.subsume_forward_parameters.large_occurrence_limit)
            continue;
          for (const Ref dref : os) {
            visits.value++;
            if (visit_bounded &&
                visits.value - check_start >= visit_budget) {
              budget_exhausted = true;
              break;
            }
            const Clause *d = clause_at(k.arena, dref);
            assert(d->size <= c->size); // connected earlier => not larger
            if (subsumes_marked(k, marks, d)) {
              subsumed = true;
              break;
            }
          }
          if (subsumed)
            break;
        }
      }
      mark_candidate(k, marks, c, 0);
      if (budget_exhausted) {
        k.stats.budget_stops_c1++;
        stopped_at = ci;
        break;
      }
    } else {
      k.stats.forward_mark_skipped++;
    }
    if (subsumed) {
      delete_subsumed(k, c);
      deleted++;
      continue;
    }
    // Connect at the least-occurring free literal unless saturated (Kissat
    // occlim).
    Lit min_lit = INVALID_LIT;
    size_t min_occs = 0;
    for (const Lit l : *c) {
      if (k.value(l) != 0)
        continue;
      const size_t n = watch_population(l);
      // Kissat updates the minimum on equality, choosing the last tied literal
      // in the clause's physical order.
      if (min_lit == INVALID_LIT || n <= min_occs) {
        min_lit = l;
        min_occs = n;
      }
    }
    assert(min_lit != INVALID_LIT);
    if (min_occs <= k.subsume_forward_parameters.large_occurrence_limit)
      occs[min_lit].push_back(ref);
  }
  // Resume: a cut round re-arms unreached candidates with new material, then
  // advances the watermark (kissat forward.c:589-645). An interrupted round
  // does neither (same condition in strengthen.cpp).
  for (size_t ci = interrupted ? candidates.size()
                               : (filter ? stopped_at : candidates.size());
       ci < candidates.size(); ci++) {
    const Clause *c = clause_at(k.arena, candidates[ci]);
    if (c->garbage() || !kernel_clause_new_material(k, c, wm))
      continue;
    for (const Lit l : *c)
      kernel_touch_var(k, var_of(l));
    k.stats.forward_rearmed++;
  }
  // Advance to the round-entry `snapshot`, not the current epoch: what this
  // round created — the re-arms above included — is new material next round.
  if (filter && !interrupted) {
    k.subsume_touch_mark = snapshot;
    k.stats.forward_mark_advances++;
  }
  // Kissat's `completed` (forward.c:664-670): reached the end of the list.
  k.subsume_complete = stopped_at == candidates.size();
  if (!deleted)
    return ApplyResult::NoOp;
  // Direct callers still need the packed-arena collection boundary.
  kernel_collect_garbage(k);
  return ApplyResult::Ok;
}

// Instance surface: subsume exactly one candidate via its free literals' watch
// lists. The victim may be redundant (N1); the subsumer may not.
template <bool Collect>
static ApplyResult subsume_forward_try_instance(Kernel &k, Ref candidate) {
  assert(subsume_forward_guard(k));
  // Check termination once at entry; an explicit request has no strategy caps.
  if (kernel_rule_interrupt(k, k.stats.subsume_interrupted))
    return ApplyResult::NoOp;
  Clause *c = clause_at(k.arena, candidate);
  if (!is_victim(k, c))
    return ApplyResult::NoOp;
  k.stats.forward_checked++;
  std::vector<uint8_t> marks(2 * static_cast<size_t>(k.nvars), 0);
  mark_candidate(k, marks, c, 1);
  // one fused scan per free literal's watch list: binary entries subsume when
  // the other literal is marked, large entries via subsumes_marked
  bool subsumed = false;
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    for (const WatchEntry w : k.watches.entries(l)) {
      if (subsumed)
        break;
      k.stats.forward_visits++;
      if (w.binary()) {
        subsumed = marks[w.other()] != 0;
        continue;
      }
      const Ref dref = w.ref();
      if (dref == candidate)
        continue;
      const Clause *d = clause_at(k.arena, dref);
      if (d->garbage() || d->redundant() || d->size > c->size)
        continue;
      subsumed = subsumes_marked(k, marks, d);
    }
    if (subsumed)
      break;
  }
  mark_candidate(k, marks, c, 0);
  if (!subsumed)
    return ApplyResult::NoOp;
  delete_subsumed(k, c);
  if constexpr (Collect)
    kernel_collect_garbage(k);
  return ApplyResult::Ok;
}

ApplyResult subsume_forward_try(Kernel &k, Ref candidate) {
  return subsume_forward_try_instance<true>(k, candidate);
}

ApplyResult subsume_forward_try_fastel(Kernel &k, Ref candidate,
                                       uint32_t occurrence_limit,
                                       std::vector<uint8_t> &marks) {
  assert(subsume_forward_guard(k));
  assert(k.dense && k.dense->track_order);
  if (kernel_rule_interrupt(k, k.stats.subsume_interrupted))
    return ApplyResult::NoOp;
  Clause *c = clause_at(k.arena, candidate);
  if (c->garbage() || c->redundant())
    return ApplyResult::NoOp;
  k.stats.forward_checked++;
  if (marks.size() < 2 * static_cast<size_t>(k.nvars))
    marks.resize(2 * static_cast<size_t>(k.nvars), 0);

  // fastel.c marks only literals that set a new occurrence maximum and skips
  // the last holder; marking the whole candidate would remove extra clauses.
  Lit max_occurring = INVALID_LIT;
  size_t max_occurrence = 0;
  for (const Lit other : *c) {
    const Var variable = var_of(other);
    if (kernel_var_retired(k, variable) || k.value(other))
      continue;
    const size_t occurrence = k.dense->ordered.size(other);
    if (occurrence <= max_occurrence)
      continue;
    max_occurrence = occurrence;
    max_occurring = other;
    marks[other] = 1;
  }

  bool subsumed = false;
  for (const Lit other : *c) {
    if (other == max_occurring)
      continue;
    const Var variable = var_of(other);
    if (kernel_var_retired(k, variable) || k.value(other))
      continue;
    const auto occurrences = k.dense->ordered[other];
    if (occurrences.size() > occurrence_limit)
      continue;
    for (const OccIndex::Entry entry : occurrences) {
      k.stats.forward_visits++;
      if (entry.binary) {
        if (marks[entry.payload]) {
          subsumed = true;
          break;
        }
        continue;
      }
      const Ref dref = entry.payload;
      if (dref == candidate)
        continue;
      const Clause *d = clause_at(k.arena, dref);
      if (d->garbage() || d->redundant() || d->size > c->size)
        continue;
      subsumed = true;
      for (const Lit other2 : *d) {
        if (k.value(other2) < 0)
          continue;
        if (!marks[other2]) {
          subsumed = false;
          break;
        }
      }
      if (subsumed)
        break;
    }
    if (subsumed)
      break;
  }
  for (const Lit other : *c)
    marks[other] = 0;
  if (!subsumed)
    return ApplyResult::NoOp;
  delete_subsumed(k, c);
  return ApplyResult::Ok;
}

// Instance surface: payload = candidate arena Ref, size ascending, complete
// over victims including redundant ones.
static uint32_t subsume_forward_instances(Kernel &k, uint64_t *out,
                                          uint32_t cap) {
  std::vector<Ref> candidates;
  subsume_forward_candidates(k, candidates, /*complete=*/true);
  return copy_instance_prefix(candidates, out, cap);
}

bool subsume_forward_compact_fastel_prefix(Kernel &k, Lit lit, size_t prefix) {
  assert(k.dense && k.dense->track_order);
  const auto live = [&](OccIndex::Entry entry) {
    if (entry.binary) {
      const Lit other = entry.payload;
      return k.value(other) > 0 || !kernel_var_retired(k, var_of(other));
    }
    const Clause *clause = clause_at(k.arena, entry.payload);
    return !clause->garbage() && clause_contains(clause, lit);
  };
  size_t at = 0;
  bool dead = false;
  for (const OccIndex::Entry entry : k.dense->ordered[lit]) {
    if (at++ == prefix) break;
    dead |= !live(entry);
  }
  if (!dead) return false;
  size_t visited = 0;
  k.dense->ordered.compact(lit, [&](OccIndex::Entry entry) {
    return visited++ >= prefix || live(entry);
  });
  return true;
}

// A fastel instance inside an ordered occurrence session; anything else, or a
// payload that is not a live irredundant clause start, declines.
static ApplyResult subsume_forward_apply_fastel(Kernel &k, uint64_t inst) {
  const Ref candidate = static_cast<Ref>(inst);
  const uint64_t limit = (inst & ~SUBSUME_FORWARD_FASTEL_INSTANCE) >>
                         SUBSUME_FORWARD_FASTEL_LIMIT_SHIFT;
  if (!k.dense || !k.dense->track_order || k.level || k.conflict_valid ||
      k.inconsistent)
    return ApplyResult::NoOp;
  // Validate an arbitrary payload as a clause start using one occurrence list,
  // reading only in-bounds words.
  const uint64_t arena_words = k.arena.size();
  if (uint64_t{candidate} + CLAUSE_HEADER_WORDS > arena_words)
    return ApplyResult::NoOp;
  const Clause *c = clause_at(static_cast<const Arena &>(k.arena), candidate);
  if (uint64_t{candidate} + CLAUSE_HEADER_WORDS + c->size > arena_words ||
      c->size == 0 || c->garbage() || c->redundant())
    return ApplyResult::NoOp;
  // Every live large clause of the ordered Eliminate session occurs in the
  // list of each of its literals, and the lists hold only clause starts.
  Lit rarest = INVALID_LIT;
  for (uint32_t i = 0; i < c->size; ++i) {
    const Lit lit = c->lits[i];
    if (lit >= 2 * static_cast<uint64_t>(k.nvars))
      return ApplyResult::NoOp;
    if (rarest == INVALID_LIT ||
        k.dense->ordered.large_size(lit) < k.dense->ordered.large_size(rarest))
      rarest = lit;
  }
  bool listed = false;
  for (const Ref ref : k.dense->ordered.large_refs(rarest))
    if (ref == candidate) {
      listed = true;
      break;
    }
  if (!listed)
    return ApplyResult::NoOp;
  // Zero in, zero out: any scratch is equivalent to the caller's.
  thread_local std::vector<uint8_t> marks;
  return subsume_forward_try_fastel(k, candidate, static_cast<uint32_t>(limit), marks);
}

template <bool Collect>
static ApplyResult subsume_forward_apply_instance_impl(Kernel &k, uint64_t inst) {
  if (inst != INSTANCE_CANONICAL && (inst & SUBSUME_FORWARD_FASTEL_INSTANCE))
    return subsume_forward_apply_fastel(k, inst);
  return apply_root_arena_instance<subsume_forward_apply,
                                   subsume_forward_try_instance<Collect>>(k,
                                                                         inst);
}

static ApplyResult subsume_forward_apply_instance(Kernel &k, uint64_t inst) {
  return subsume_forward_apply_instance_impl<true>(k, inst);
}

// Semantic batches keep arena offsets stable; collection is a separate
// boundary. Legacy calls collect immediately.
static ApplyResult subsume_forward_apply_semantic_instance(Kernel &k, uint64_t inst) {
  return subsume_forward_apply_instance_impl<false>(k, inst);
}

TACTIX_REGISTER_RULE(SubsumeForward, RuleTier::Inprocess, subsume_forward_guard,
                     subsume_forward_apply, subsume_forward_instances,
                     subsume_forward_apply_instance, nullptr,
                     subsume_forward_apply_semantic_instance);

} // namespace tactix
