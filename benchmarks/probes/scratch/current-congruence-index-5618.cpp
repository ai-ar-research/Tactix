// Fixed no-gate applications record the current satisfied-clause lifecycle.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace tactix;
int main() {
  for (unsigned width = 3; width <= 7; width++)
    for (unsigned position = 0; position < width; position++)
      for (int value = -1; value <= 1; value++) {
        Kernel k;
        kernel_reserve_vars(k, width);
        std::vector<Lit> lits;
        for (unsigned v = 1; v <= width; v++) lits.push_back(internalize(v));
        const Ref ref = kernel_add_large_clause(k, lits.data(), width, false, 0, ++k.next_clause_id);
        if (value) {
          const Lit unit = internalize(value * static_cast<int>(position + 1));
          k.unit_ids[var_of(unit)] = ++k.next_clause_id;
          kernel_assign_unit(k, unit);
          assert(!kernel_propagate(k));
        }
        std::vector<CongGate> gates;
        congruence_extract_gates(k, gates, true);
        assert(gates.empty() && !clause_at(k.arena, ref)->garbage());
        assert(congruence_guard(k));
        assert(congruence_apply(k) == ApplyResult::NoOp);
        assert(!k.inconsistent);
        std::printf("%u %u %d %u\n", width, position, value,
                    static_cast<unsigned>(clause_at(k.arena, ref)->garbage()));
      }
}
