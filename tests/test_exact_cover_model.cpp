#include "unit_test.hpp"
#include "dimacs_text.hpp"
#include "frontend/parse.hpp"
#include "rules/lucky/opportunities.hpp"
#include "rules/lucky/model.hpp"
#include "strategy/rule_parameters.hpp"

#include <sstream>

// An sgen-shaped formula: `blocks` at-most-one blocks of five consecutive
// variables, then 2*blocks positive 5-clauses covering every variable twice.
// Clause (b, j) takes item (t + j) mod 5 of block b + t, so the shape admits
// an exact cover.
static std::string sgen_like(int blocks) {
  std::ostringstream out;
  const int n = 5 * blocks;
  std::vector<std::vector<int>> covering;
  for (int b = 0; b < blocks; ++b)
    for (int j = 0; j < 2; ++j) {
      std::vector<int> clause;
      for (int t = 0; t < 5; ++t) {
        const int block = (b + t) % blocks;
        clause.push_back(5 * block + (t + j) % 5 + 1);
      }
      covering.push_back(clause);
    }
  out << "p cnf " << n << ' ' << (10 * blocks + 2 * blocks) << '\n';
  for (int b = 0; b < blocks; ++b)
    for (int i = 0; i < 5; ++i)
      for (int k = i + 1; k < 5; ++k)
        out << -(5 * b + i + 1) << ' ' << -(5 * b + k + 1) << " 0\n";
  for (const auto &clause : covering) {
    for (const int literal : clause)
      out << literal << ' ';
    out << "0\n";
  }
  return out.str();
}

int main() {
  {
    Kernel kernel = parse_dimacs_text(sgen_like(7));
    assert(exact_cover_model_guard(kernel));
    const ApplyResult result = exact_cover_model_apply(kernel);
    assert(kernel.stats.exact_cover_model_calls == 1);
    assert(kernel.stats.exact_cover_model_recognized == 1);
    assert(kernel.stats.exact_cover_model_blocks == 7);
    assert(result == ApplyResult::Sat);
    assert(kernel.stats.exact_cover_model_sat == 1);
    assert(kernel.stats.exact_cover_model_nodes > 0);
    assert(kernel.trail.size() == kernel.nvars);
  }
  {
    // A block of size four is not the owner's shape.
    Kernel kernel = parse_dimacs_text("p cnf 4 7\n-1 -2 0\n-1 -3 0\n-2 -3 0\n-1 -4 0\n"
                          "-2 -4 0\n-3 -4 0\n1 2 3 4 0\n");
    assert(exact_cover_model_apply(kernel) == ApplyResult::NoOp);
    assert(!kernel.stats.exact_cover_model_recognized);
  }
  {
    // Exhausting the deterministic budget without restarts reports a budget
    // stop and leaves the kernel untouched.
    Kernel kernel = parse_dimacs_text(sgen_like(7));
    kernel.exact_cover_model_parameters.maximum_nodes = 1;
    assert(exact_cover_model_apply(kernel) == ApplyResult::NoOp);
    assert(kernel.stats.exact_cover_model_recognized == 1);
    assert(kernel.stats.exact_cover_model_budget_exhausted == 1);
    assert(kernel.trail.empty());
    // A randomized restart with a real budget then finds the cover.
    kernel.exact_cover_model_parameters.restarts = 1;
    assert(exact_cover_model_apply(kernel) == ApplyResult::Sat);
    assert(kernel.stats.exact_cover_model_restarts == 1);
  }
  std::printf("test_exact_cover_model OK\n");
  return 0;
}
