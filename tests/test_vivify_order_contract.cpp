// Actual scheduler contract at the ordinary/skip-sort population boundary.
// No alternative sorter or solver implementation is supplied by this test.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/vivify/vivify.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace tactix;
static uint64_t digest = UINT64_C(1469598103934665603);
static void hash(uint64_t value) {
  digest = (digest ^ value) * UINT64_C(1099511628211);
}

static std::vector<unsigned> models(const Kernel &k) {
  std::vector<unsigned> result;
  for (unsigned mask = 0; mask != 256; ++mask) {
    bool valid = true;
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *clause = clause_at(k.arena, ref);
      if (!clause->garbage()) {
        bool satisfied = false;
        for (Lit lit : *clause)
          satisfied |= bool(mask & (1u << var_of(lit))) != negated(lit);
        valid &= satisfied;
      }
      ref += clause_actual_words(clause);
    }
    if (valid) result.push_back(mask);
  }
  return result;
}

static std::vector<uint64_t> check_order(Kernel &k, unsigned irred,
                                        unsigned red) {
  std::array<uint32_t, 16> counts{};
  unsigned seen_irred = 0, seen_red = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) {
      (c->redundant() ? seen_red : seen_irred)++;
      for (Lit lit : *c) ++counts[lit];
    }
    ref += clause_actual_words(c);
  }
  assert(seen_irred == irred && seen_red == red);
  const Arena arena = k.arena;
  const auto values = k.values;
  const auto trail = static_cast<std::vector<Lit>>(k.trail);
  const uint64_t ticks = k.stats.ticks;
  std::vector<Ref> order, again;
  vivify_candidates(k, order);
  vivify_candidates(k, again);
  assert(order == again && order.size() == irred);
  assert(k.arena == arena && k.values == values);
  assert(static_cast<std::vector<Lit>>(k.trail) == trail && k.stats.ticks == ticks);
  auto refs = order;
  std::sort(refs.begin(), refs.end());
  assert(std::adjacent_find(refs.begin(), refs.end()) == refs.end());
  const auto maximum = [&](const Clause *c) {
    uint32_t count = 0;
    for (Lit lit : *c) count = std::max(count, counts[lit]);
    return count;
  };
  // This checks semantic ordering properties on the actual API output, not
  // a replacement implementation of its sort. Exactly floor(irred/10)>red
  // switches the native schedule to reverse logical order.
  for (size_t i = 1; i < order.size(); ++i) {
    const Clause *a = clause_at(k.arena, order[i - 1]);
    const Clause *b = clause_at(k.arena, order[i]);
    assert(!a->redundant() && !b->redundant());
    if (a->vivify() != b->vivify()) assert(a->vivify());
    else if (irred / 10 > red) {
      assert(kernel_clause_position(k, order[i - 1]) > kernel_clause_position(k, order[i]));
    } else if (maximum(a) != maximum(b)) assert(maximum(a) > maximum(b));
    else if (a->size != b->size) assert(a->size < b->size);
    else assert(kernel_clause_position(k, order[i - 1]) > kernel_clause_position(k, order[i]));
  }
  std::vector<uint64_t> ids;
  for (Ref ref : order) {
    assert(!clause_at(k.arena, ref)->redundant());
    ids.push_back(clause_at(k.arena, ref)->id());
  }
  const auto &rule = tactix_rules()[tactix_rule_id("Vivify")];
  assert(rule.guard(k));
  for (uint32_t cap : {0u, 1u, irred / 2, irred, irred + 1}) {
    std::vector<uint64_t> surface(irred + 2, UINT64_MAX);
    const uint32_t n = rule.instances(k, surface.data(), cap);
    assert(n == std::min(cap, irred));
    for (uint32_t i = 0; i < n; ++i) assert(surface[i] == order[i]);
    assert(surface[n] == UINT64_MAX);
  }
  assert(k.arena == arena && k.values == values && k.stats.ticks == ticks);
  return ids;
}

static void check(unsigned irred, unsigned red, unsigned priority,
                  bool reverse_import, bool replace) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  k.vivify_scope = 2;
  k.vivify_reuse = true;
  k.vivify_flush = false;
  for (unsigned i = 0; i < 8; ++i)
    kernel_record_import(k, reverse_import ? 7 - i : i);
  std::vector<Ref> originals;
  for (unsigned i = 0; i < irred + red; ++i) {
    std::vector<Lit> lits;
    const unsigned length = 3 + (i % 3);
    // Repeated count profiles and lengths force logical-position tie breaks.
    for (unsigned j = 0; j < length; ++j)
      lits.push_back(2 * ((i * 3 + j) % 8));
    if (reverse_import) std::reverse(lits.begin(), lits.end());
    const bool redundant = i >= irred;
    const Ref ref = kernel_add_large_clause(k, lits.data(), lits.size(),
        redundant, redundant ? 2 : 0, ++k.next_clause_id);
    clause_at(k.arena, ref)->set_vivify(priority == 1 || (priority == 2 && i % 3 == 0));
    originals.push_back(ref);
  }
  const auto original_models = models(k);
  assert(!original_models.empty());
  if (replace) {
    for (unsigned i = 0; i < irred; i += 4) {
      const Ref ref = originals[i];
      const Clause *c = clause_at(k.arena, ref);
      const bool prioritized = c->vivify();
      const std::vector<Lit> lits(c->begin(), c->end());
      const uint64_t position = kernel_clause_position(k, ref);
      const uint64_t id = ++k.next_clause_id;
      kernel_inherit_clause_position(k, id, ref);
      kernel_delete_clause(k, clause_at(k.arena, ref));
      const Ref replacement = kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
      clause_at(k.arena, replacement)->set_vivify(prioritized);
      assert(replacement > originals.back());
      assert(kernel_clause_position(k, replacement) == position);
    }
  }
  assert(models(k) == original_models);
  auto expected = check_order(k, irred, red);
  // Exercise canonical schedule construction and rearming, but no probing:
  // zero is a bounded exhausted ceiling, not the unbounded legacy spelling.
  k.work_budget_bounded = true;
  k.work_budget = 0;
  k.work_budget_offset = 1;
  assert(vivify_apply(k) == ApplyResult::NoOp);
  assert(k.stats.vivify_checked == 0 && k.stats.vivify_rounds == 1);
  assert(k.stats.vivify_reprioritized == (priority == 0 ? 1u : 0u));
  assert(check_order(k, irred, red) == expected);
  assert(models(k) == original_models && k.level == 0 && !k.conflict_valid);
  kernel_check_invariants(k);
  kernel_collect_garbage(k);
  assert(check_order(k, irred, red) == expected);
  assert(models(k) == original_models);
  kernel_check_invariants(k);
  hash(irred); hash(red); hash(priority); hash(reverse_import); hash(replace);
  for (uint64_t id : expected) hash(id);
}

int main() {
  unsigned cases = 0;
  for (unsigned red = 0; red != 4; ++red)
    for (unsigned irred : {10 * (red + 1) - 1, 10 * (red + 1)})
      for (unsigned priority = 0; priority != 3; ++priority)
        for (bool reverse : {false, true})
          for (bool replace : {false, true}) {
            check(irred, red, priority, reverse, replace);
            ++cases;
          }
  std::printf("vivify-order-contract cases=%u digest=%016llx\n", cases,
              static_cast<unsigned long long>(digest));
}
