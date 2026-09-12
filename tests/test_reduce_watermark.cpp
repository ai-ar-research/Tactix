// Historical collection boundaries survive promotion, not genuine collection.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace tactix;

static Ref add(Kernel &k, unsigned tag, bool red, bool ids) {
  const Lit lits[] = {internalize(1), internalize(tag), internalize(20),
                      internalize(21), internalize(22)};
  return kernel_add_large_clause(k, lits, 5, red, 9,
                                 ids ? ++k.next_clause_id : 0);
}
static Ref find(Kernel &k, unsigned tag) {
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && std::find(c->begin(), c->end(), internalize(tag)) != c->end())
      return ref;
    ref += clause_actual_words(c);
  }
  assert(false); return INVALID_REF;
}
static void cursor(Kernel &k) {
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) c->searched = 4; // valid index in a five-literal clause
    ref += clause_actual_words(c);
  }
}
static unsigned checked, old_failures;
static void scenario(unsigned mode, unsigned packing, bool root, bool alias,
                     bool noop, bool ids, bool compact, bool parent) {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 26);
  add(k, 2, false, ids); // genuinely irredundant prefix
  Ref origin = add(k, 3, mode != 2, ids);
  add(k, 4, false, ids); // irredundant tail after the historical boundary
  add(k, 5, true, ids);  // selected victim
  if (mode != 2) {
    clause_at(k.arena, origin)->set_redundant(false);
    k.live_irredundant_large++;
  }
  if (alias) {
    // The production replacement protocol publishes position before allocation.
    // With id zero there is deliberately no alias; the replacement stays last.
    if (ids) kernel_inherit_clause_position(k, k.next_clause_id + 1, origin);
    kernel_delete_clause(k, clause_at(k.arena, origin));
    origin = add(k, 3, false, ids);
  }
  if (mode == 1) kernel_delete_clause(k, clause_at(k.arena, origin));
  if (packing) kernel_collect_garbage(k, 0, false, packing == 2);
  if (root) kernel_assign_unit(k, internalize(25));
  else kernel_assign_decision(k, internalize(26));
  assert(propagate_apply(k) == ApplyResult::Ok);
  if (noop) assert(reduce_apply_refs(k, nullptr, 0) == ApplyResult::NoOp);
  cursor(k);
  const auto &rule = tactix_rules()[tactix_rule_id("Reduce")];
  const auto image = k.arena;
  uint64_t surface[8];
  assert(rule.guard(k));
  assert(rule.instances(k, surface, 0) == 0);
  const auto count = rule.instances(k, surface, 8);
  assert(count == 2 && std::find(surface, surface + count, 1) != surface + count);
  assert(k.arena == image); // public enumeration cannot advance the watermark
  const Ref victim = find(k, 5);
  assert(reduce_apply_refs(k, &victim, 1, compact) == ApplyResult::Ok);
  assert(k.stats.deleted == 1);
  const bool historical = mode != 2 && packing != 1 && !noop;
  const unsigned expected = compact || historical ? 2 : 4;
  const auto target = clause_at(k.arena, find(k, 4))->searched;
  const auto prefix = clause_at(k.arena, find(k, 2))->searched;
  assert(prefix == (compact ? 2u : 4u));
  if (target != expected) ++old_failures;
  assert(target == (parent && !compact ? 4u : expected));
  if (mode != 1) {
    const bool physical_tail = alias && !ids;
    const auto promoted = clause_at(k.arena, find(k, 3))->searched;
    assert(promoted == (compact || physical_tail || (!parent && historical) ? 2u : 4u));
  }
  kernel_check_invariants(k);
  // Genuine collection has now consumed the old cut. A later learned clause
  // must not rewatch the former promoted tail a second time.
  add(k, 6, true, ids);
  cursor(k);
  const Ref next = find(k, 6);
  assert(reduce_apply_refs(k, &next, 1) == ApplyResult::Ok);
  assert(clause_at(k.arena, find(k, 4))->searched == 4);
  kernel_check_invariants(k);
  ++checked;
}
int main(int argc, char **argv) {
  const bool parent = argc == 2 && !std::strcmp(argv[1], "--expect-parent");
  assert(argc == 1 || parent);
  for (unsigned mode = 0; mode < 3; ++mode)
    for (unsigned packing = 0; packing < 3; ++packing)
      for (bool root : {false, true}) for (bool alias : {false, true})
        for (bool noop : {false, true}) for (bool ids : {false, true})
          for (bool compact : {false, true})
            scenario(mode, packing, root, alias, noop, ids, compact, parent);
  assert(checked == 288 && old_failures == (parent ? 32u : 0u));
  std::printf("watermark cases=%u historical-boundary failures=%u expected-parent=%d PASS\n",
              checked, old_failures, parent);
}
