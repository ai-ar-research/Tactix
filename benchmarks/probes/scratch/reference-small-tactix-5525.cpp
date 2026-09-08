// Same fixed operations as the native fixture, on actual pinned SubSolver code.
#include "engine/subsolver.hpp"
#include <cinttypes>
#include <cstdio>
#include <vector>
using namespace tactix;

int main() {
  SubSolver embedded;
  embedded.start_kitten_session();
  for (uint32_t n : {2u, 3u, 17u, 270u}) {
    for (uint32_t repetition = 0; repetition < 3; repetition++) {
      embedded.reset(n, SubSearchMode::Kitten);
      std::vector<SubLit> literals;
      for (uint32_t i = 0; i < n; i++)
        literals.push_back(sub_lit(i, false));
      embedded.add_clause(literals);
      embedded.randomize_phases();
      if (embedded.solve({}, 0, SubTickMode::Kitten) != SubResult::Sat)
        return 2;
      const uint64_t initial = embedded.kitten_ticks();
      uint32_t flips = 0;
      for (uint32_t i = 0; i < n; i++)
        flips += embedded.flip_model_literal(sub_lit(i, false));
      uint64_t model = 1469598103934665603ull;
      for (uint32_t i = 0; i < n; i++)
        model = (model ^ (embedded.model()[i] > 0 ? 1 : 2)) * 1099511628211ull;
      std::printf("%u %u %" PRIu64 " %" PRIu64 " %u %" PRIu64 " %" PRIu64 "\n",
                  n, repetition, initial, embedded.kitten_ticks(), flips,
                  embedded.kitten_generator(), model);
    }
  }
}
