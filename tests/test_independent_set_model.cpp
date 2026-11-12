#undef NDEBUG
#include "independent_set_test_support.hpp"
#include "dimacs_text.hpp"
#include "frontend/parse.hpp"
#include "rules/lucky/opportunities.hpp"
#include "strategy/rule_parameters.hpp"

#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;

static std::string independent_set(
    uint32_t n, uint32_t k, std::vector<std::pair<uint32_t, uint32_t>> edges) {
  const auto encoding = independent_set_encoding(n, k, std::move(edges));
  return dimacs_text(encoding.variables, encoding.clauses);
}

// Six arguments in a directed bipartite attack graph.  The nine negative
// binaries are the undirected conflict shadow; each positive clause has one
// unique pivot adjacent to its two attackers.  The final six clauses are
// non-tautological binary resolvents of a cover and a conflict clause.
static std::string stable_extension(bool corrupt_resolvent = false) {
  std::ostringstream out;
  out << "p cnf 6 21\n"
      << "-1 -4 0\n-1 -5 0\n-1 -6 0\n"
      << "-2 -4 0\n-2 -5 0\n-2 -6 0\n"
      << "-3 -4 0\n-3 -5 0\n-3 -6 0\n"
      << "1 4 5 0\n2 5 6 0\n3 6 4 0\n"
      << "4 1 2 0\n5 2 3 0\n6 3 1 0\n"
      << "-3 1 2 0\n-1 2 3 0\n-2 3 1 0\n"
      << "-4 5 6 0\n-5 6 4 0\n-6 4 "
      << (corrupt_resolvent ? 2 : 5) << " 0\n";
  return out.str();
}

int main() {
  {
    // A 6-cycle plus one chord has an independent set of size 3.
    Kernel kernel = parse_dimacs_text(independent_set(
        6, 3, {{1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 1}, {1, 4}}));
    assert(independent_set_model_guard(kernel));
    const ApplyResult result = independent_set_model_apply(kernel);
    assert(kernel.stats.independent_set_model_calls == 1);
    assert(kernel.stats.independent_set_model_recognized == 1);
    assert(kernel.stats.independent_set_model_vertices == 6 &&
           kernel.stats.independent_set_model_target == 3);
    assert(result == ApplyResult::Sat);
    assert(kernel.stats.independent_set_model_sat == 1);
    assert(kernel.trail.size() == kernel.nvars);
  }
  {
    // A triangle has no independent set of size 2: the search fails and the
    // kernel is untouched.
    Kernel kernel = parse_dimacs_text(independent_set(3, 2, {{1, 2}, {2, 3}, {3, 1}}));
    assert(independent_set_model_apply(kernel) == ApplyResult::NoOp);
    assert(kernel.stats.independent_set_model_recognized == 1);
    assert(kernel.trail.empty());
  }
  {
    // Not the shape: no clause wider than two.
    Kernel kernel = parse_dimacs_text("p cnf 3 3\n-1 -2 0\n-2 -3 0\n1 3 0\n");
    assert(independent_set_model_apply(kernel) == ApplyResult::NoOp);
    assert(!kernel.stats.independent_set_model_recognized);
  }
  {
    Kernel kernel = parse_dimacs_text(stable_extension());
    const uint64_t before = kernel.live_irredundant_large;
    assert(independent_set_model_apply(kernel) == ApplyResult::Sat);
    assert(kernel.stats.independent_set_model_recognized == 1);
    assert(kernel.stats.independent_set_stable_cores == 1);
    assert(kernel.stats.independent_set_stable_cover_clauses == 6);
    assert(kernel.stats.independent_set_stable_resolvents == 6);
    assert(kernel.live_irredundant_large == before);
    assert(kernel.trail.size() == kernel.nvars);
  }
  {
    // A single unauthenticated guarded clause makes the whole recognition
    // transactional: no subset is deleted.
    Kernel kernel = parse_dimacs_text(stable_extension(true));
    const uint64_t before = kernel.live_irredundant_large;
    assert(independent_set_model_apply(kernel) == ApplyResult::NoOp);
    assert(!kernel.stats.independent_set_model_recognized);
    assert(!kernel.stats.independent_set_stable_resolvents);
    assert(kernel.live_irredundant_large == before);
  }
  std::printf("test_independent_set_model OK\n");
  return 0;
}
