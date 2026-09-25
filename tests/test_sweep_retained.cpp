// Retained admission is scheduling advice, never a public Sweep precondition.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/sweep/sweep.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace tactix;

static void binary(Kernel &k, int a, int b) {
  kernel_add_binary_clause(k, internalize(a), internalize(b),
                           ++k.next_clause_id);
}

static SweepLimits bounded() {
  SweepLimits lim;
  lim.max_seeds = 1;
  lim.env_ticks = 1;
  return lim;
}

static void mark_retained(Kernel &k) {
  k.sweep_incomplete = true;
  k.sweep_marked.resize(k.external_to_internal.size(), 0);
  for (const Var external : k.sweep_tail)
    if (external < k.sweep_marked.size())
      k.sweep_marked[external] = 1;
  for (const Var external : k.sweep_front)
    if (external < k.sweep_marked.size())
      k.sweep_marked[external] = 1;
}

static void pure_seed(bool old) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  binary(k, 1, 6);
  binary(k, 2, 7);
  binary(k, -2, 8); // A fresh eligible seed must not restart this call.
  k.sweep_tail = {0};
  mark_retained(k);
  assert(sweep_apply_with_limits(k, bounded()) == ApplyResult::NoOp);
  assert(k.stats.sweep_swept == (old ? 1u : 0u));
  assert(k.stats.sweep_resumed == (old ? 1u : 0u));
  assert(k.stats.sweep_scheduled == 0);
  assert(k.sweep_live == 0 && k.sweep_slot.empty());
  const uint32_t completed = old ? 1u : 2u;
  assert(k.sweep_passes_completed == completed &&
         k.stats.sweep_completed == completed);
  std::vector<Var> seeds;
  sweep_seeds(k, seeds);
  assert(std::find(seeds.begin(), seeds.end(), 0) != seeds.end());
  const auto before = k.stats.sweep_swept;
  (void)sweep_try(k, 0);
  assert(k.stats.sweep_swept == before + 1); // N1 still exposes pure seeds.
}

static void completed_tail_admits_fresh(bool filtered) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  binary(k, 1, 6);
  if (!filtered)
    binary(k, -1, 7);
  binary(k, 2, 7);
  binary(k, -2, 8);
  // The saved representative was already swept in the preceding pass. Its
  // unmarked tail is not evidence that the pass remains incomplete.
  k.sweep_tail = {0};
  (void)sweep_apply_with_limits(k, bounded());
  assert(k.stats.sweep_scheduled == 1);
  assert(k.stats.sweep_swept == 1);
  if (filtered) {
    assert(k.stats.sweep_resumed == 0);
    assert(k.sweep_passes_completed == 1 && !k.sweep_incomplete);
  } else {
    assert(k.stats.sweep_resumed == 1);
    assert(k.sweep_live == 1 && sweep_next_scheduled(k) == 1);
    assert(k.sweep_passes_completed == 0 && k.sweep_incomplete);
    assert(k.sweep_marked[1] == 1);
  }
}

static void completed_pass_keeps_unmarked_representative() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  binary(k, 1, 2);
  binary(k, -1, 3);
  binary(k, -2, 4);
  binary(k, -3, -4);
  k.sweep_tail = {0};
  k.sweep_incomplete = true;
  k.sweep_marked.resize(k.external_to_internal.size(), 0);
  k.sweep_marked[0] = 1;
  SweepLimits lim;
  lim.max_seeds = 1;
  (void)sweep_apply_with_limits(k, lim);
  assert(k.stats.sweep_rescheduled > 0);
  assert(k.sweep_live > 0 && sweep_next_scheduled(k) == 0);
  assert(!k.sweep_incomplete && k.sweep_passes_completed == 1);
  assert(k.sweep_marked[0] == 0);
  const uint64_t scheduled = k.stats.sweep_scheduled;
  (void)sweep_apply_with_limits(k, lim);
  assert(k.stats.sweep_scheduled > scheduled);
}

static void front_tail_order() {
  Kernel k;
  kernel_reserve_vars(k, 16);
  binary(k, 1, 12); // pure front
  binary(k, -3, 13); // pure tail
  for (int seed : {2, 4, 5}) {
    binary(k, seed, 14);
    binary(k, -seed, 15);
  }
  k.sweep_tail = {INVALID_VAR, 2, 3, 4};
  k.sweep_tail_next = 1;
  k.sweep_front = {1, INVALID_VAR, 0};
  k.sweep_slot.resize(16, 0);
  k.sweep_slot[1] = (1u << 31) + 1;
  k.sweep_slot[0] = (1u << 31) + 3;
  k.sweep_live = 2;
  mark_retained(k);
  (void)sweep_apply_with_limits(k, bounded());
  assert(k.stats.sweep_swept == 1 && k.stats.sweep_resumed == 1);
  assert(k.sweep_front.empty());
  assert(k.sweep_tail[1] == INVALID_VAR);
  assert(k.sweep_slot[0] == 0 && k.sweep_slot[1] == 0 &&
         k.sweep_slot[2] == 0);
  assert(k.sweep_live == 2 && sweep_next_scheduled(k) == 3);
  assert(k.sweep_slot[3] == 3 && k.sweep_slot[4] == 4);
  (void)sweep_apply_with_limits(k, bounded());
  assert(k.sweep_live == 1 && sweep_next_scheduled(k) == 4);
  assert(k.sweep_slot[3] == 0 && k.sweep_slot[4] == 4);
}

static void occurrence_boundary() {
  for (bool negative : {false, true})
    for (uint32_t count : {1u, 2u, 3u})
      for (uint32_t completed : {0u, 1u}) {
        Kernel k;
        kernel_reserve_vars(k, 16);
        binary(k, negative ? 1 : -1, 12);
        for (uint32_t j = 0; j < count; j++)
          binary(k, negative ? -1 : 1, 8 + j);
        k.sweep_tail = {0};
        mark_retained(k);
        k.sweep_passes_completed = completed;
        auto lim = bounded();
        lim.max_env_clauses = 2;
        (void)sweep_apply_with_limits(k, lim);
        const uint32_t cap = 2u << completed;
        assert(k.stats.sweep_limit_clauses == cap);
        assert(k.stats.sweep_swept == (count <= cap ? 1u : 0u));
      }
}

static void invalid_and_duplicate_entries() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  binary(k, 1, 7);
  // All pure duplicates and unresolved external identities must drain once.
  k.sweep_tail = {0, INVALID_VAR, 0, 99};
  mark_retained(k);
  (void)sweep_apply_with_limits(k, bounded());
  assert(k.stats.sweep_swept == 0 && k.sweep_live == 0);
  assert(k.sweep_slot.empty() && k.sweep_passes_completed == 2);
}

static void entry_not_pop() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  binary(k, 1, 2); binary(k, -1, 3);
  binary(k, -2, 4); binary(k, -3, -4);
  OccIndex occ;
  kernel_dense_connect(k, occ);
  k.sweep_tail = {0};
  mark_retained(k);
  SweepLimits lim;
  lim.max_seeds = 2;
  lim.max_env_clauses = 4;
  (void)sweep_apply_with_limits(k, lim);
  // The first seed proves the cycle equivalent and moves its representative
  // to the front. Its new bridge occurrences exceed the entry cap, but this
  // same-call representative must not be rejected by a second admission test.
  assert(k.stats.sweep_rescheduled > 0 && k.stats.sweep_swept == 2);
  assert(!kernel_var_retired(k, 0));
  assert(k.watches.size(internalize(1)) > lim.max_env_clauses &&
         k.watches.size(internalize(-1)) > lim.max_env_clauses);
  kernel_dense_disconnect(k);
}

static void fixed_and_retired() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  k.unit_ids[0] = ++k.next_clause_id;
  kernel_assign_unit(k, internalize(1));
  k.var_status.resize(k.nvars, VarStatus::Active);
  k.var_status[1] = VarStatus::Eliminated;
  k.external_to_internal[2] = INVALID_VAR;
  k.sweep_tail = {0, 1, 2};
  mark_retained(k);
  (void)sweep_apply_with_limits(k, bounded());
  assert(k.stats.sweep_swept == 0 && k.sweep_live == 0);
  assert(k.sweep_slot.empty() && k.stats.sweep_completed == 2);
}

static void large_occurrences() {
  for (bool dense : {false, true})
    for (uint32_t count : {1u, 2u}) {
      Kernel k;
      kernel_reserve_vars(k, 12);
      for (bool neg : {false, true})
        for (uint32_t j = 0; j < count; j++) {
          const Lit seed = make_lit(0, neg);
          const Lit a = make_lit(2 + 2 * j, false);
          const Lit b = make_lit(3 + 2 * j, false);
          const Lit lits[] = {dense ? a : seed, b, dense ? seed : a};
          kernel_add_large_clause(k, lits, 3, false, 0, ++k.next_clause_id);
        }
      OccIndex occ;
      if (dense)
        kernel_dense_connect(k, occ);
      k.sweep_tail = {0};
      mark_retained(k);
      auto lim = bounded();
      // Existing fresh admission uses raw sparse watch words (two per large
      // watch), versus one occurrence in the connected dense representation.
      // Retained admission must use the same predicate, not invent another.
      lim.max_env_clauses = dense ? 1 : 2;
      (void)sweep_apply_with_limits(k, lim);
      assert(k.stats.sweep_swept == (count == 1 ? 1u : 0u));
      if (dense)
        kernel_dense_disconnect(k);
    }
}

int main(int argc, char **argv) {
  const bool old = argc == 2 && !std::strcmp(argv[1], "--old-counterexample");
  pure_seed(old);
  if (old) {
    std::puts("old counterexample: retained pure seed attempted; public API legal");
    return 0;
  }
  completed_tail_admits_fresh(false);
  completed_tail_admits_fresh(true);
  completed_pass_keeps_unmarked_representative();
  front_tail_order();
  occurrence_boundary();
  invalid_and_duplicate_entries();
  entry_not_pop();
  fixed_and_retired();
  large_occurrences();
  std::puts("retained entry eligibility: PASS");
}
