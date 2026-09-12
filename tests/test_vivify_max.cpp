// Fixed actual-scheduler comparisons: maximum-count ranking and fallback scopes.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/vivify/vivify.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace tactix;

static uint64_t digest = UINT64_C(1469598103934665603);
static void hash(uint64_t value) {
  digest = (digest ^ value) * UINT64_C(1099511628211);
}

static void check(unsigned population, unsigned scope, bool reuse, bool flush,
                  unsigned priority) {
  Kernel k;
  kernel_reserve_vars(k, 80);
  k.vivify_scope = scope;
  k.vivify_reuse = reuse;
  k.vivify_flush = flush;
  k.tier1 = 2;
  k.tier2 = 6;
  std::vector<Ref> refs;
  // Exact ties, mixed lengths, import-order fallback and both sides of the
  // existing 10:1 irredundant no-sort condition. No solver or random corpus.
  for (unsigned i = 0; i < 84; ++i) {
    std::vector<Lit> lits;
    const unsigned size = 3 + (i % 21);
    for (unsigned j = 0; j < size; ++j)
      lits.push_back(2 * ((i * 7 + j * 3) % 79) + ((i + j) & 1));
    const bool red = population == 0 ? i >= 42 : i >= 82;
    const Ref ref = kernel_add_large_clause(k, lits.data(), size, red,
                                            red ? 1 + i % 9 : 0);
    clause_at(k.arena, ref)->set_vivify(priority == 1 ||
                                      (priority == 2 && i % 3 == 0));
    refs.push_back(ref);
  }
  const Arena original_arena = k.arena;
  const auto original_values = k.values;
  std::vector<Ref> order, again;
  vivify_candidates(k, order);
  vivify_candidates(k, again);
  assert(order == again);
  assert(k.arena == original_arena && k.values == original_values);
  std::vector<Ref> sorted = order;
  std::sort(sorted.begin(), sorted.end());
  assert(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
  if (scope == 2) {
    assert(order.size() == (population ? 82 : 42));
    if (reuse && !flush && !population) {
      std::vector<uint32_t> counts(2 * k.nvars);
      for (const Ref ref : refs)
        for (const Lit lit : *clause_at(k.arena, ref)) ++counts[lit];
      const auto maximum = [&](const Clause *c) {
        uint32_t result = 0;
        for (const Lit lit : *c) result = std::max(result, counts[lit]);
        return result;
      };
      for (size_t i = 1; i < order.size(); ++i) {
        const Clause *a = clause_at(k.arena, order[i - 1]);
        const Clause *b = clause_at(k.arena, order[i]);
        if (a->vivify() != b->vivify()) assert(a->vivify());
        else if (maximum(a) != maximum(b)) assert(maximum(a) > maximum(b));
        else if (a->size != b->size) assert(a->size < b->size);
        else assert(kernel_clause_position(k, order[i - 1]) >
                    kernel_clause_position(k, order[i]));
      }
    }
  }
  hash(population); hash(scope); hash(reuse); hash(flush); hash(priority);
  hash(order.size());
  for (const Ref ref : order) hash(ref);
  for (const uint32_t word : k.arena) hash(word);
}

int main() {
  unsigned cases = 0;
  for (unsigned population = 0; population != 2; ++population)
    for (unsigned scope = 0; scope != 6; ++scope)
      for (bool reuse : {false, true})
        for (bool flush : {false, true})
          for (unsigned priority = 0; priority != 3; ++priority) {
            check(population, scope, reuse, flush, priority);
            ++cases;
          }
  std::printf("vivify-max cases=%u order-and-pose=%016llx\n", cases,
              static_cast<unsigned long long>(digest));
}
