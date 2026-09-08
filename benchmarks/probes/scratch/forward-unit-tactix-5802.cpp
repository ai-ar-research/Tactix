#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/strengthen/strengthen.hpp"
#include "forward-unit-cases-5802.h"
#include <cassert>
#include <cstdio>
using namespace tactix;
int main() {
  for (unsigned test = 0; test < 6; ++test) {
    Kernel k;
    kernel_reserve_vars(k, 16);
    std::vector<Lit> lits;
    for (unsigned i = 0; i < lengths[test]; ++i) {
      const int lit = cases[test][i];
      if (lit) { lits.push_back(internalize(lit)); continue; }
      const uint64_t id = ++k.next_clause_id;
      if (lits.size() == 1) {
        k.unit_ids[var_of(lits[0])] = id;
        kernel_assign_unit(k, lits[0]);
      } else if (lits.size() == 2)
        kernel_add_binary_clause(k, lits[0], lits[1], id);
      else {
        assert(lits.size() > 2);
        kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
      }
      lits.clear();
    }
    assert(lits.empty() && propagate_apply(k) == ApplyResult::Ok);
    assert(k.value(internalize(3)) == 0);
    const size_t entry = k.trail.size();
    kernel_flush_large_watches(k);
    assert(strengthen_apply(k) == ApplyResult::Ok);
    printf("CASE %u complete=%u steps=%llu subsumed=%llu strengthened=%llu trail=",
           test, k.strengthen_complete, (unsigned long long)k.stats.strengthen_visits,
           (unsigned long long)k.stats.strengthen_subsumed,
           (unsigned long long)k.stats.strengthened);
    for (size_t i = entry; i < k.trail.size(); ++i)
      printf("%d,", externalize(k.trail[i]));
    printf(" values=");
    for (unsigned v = 1; v <= 16; ++v) printf("%d,", k.value(internalize(v)));
    puts("");
    assert(!k.inconsistent && k.value(internalize(3)) == 1);
  }
}
