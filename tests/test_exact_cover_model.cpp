#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/lucky/exact_cover_model.hpp"
#include "rules/lucky/model.hpp"
#include "strategy/rule_parameters.hpp"

#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;

static Kernel parse(const std::string &text) {
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  return kernel;
}

// A signed sgen-shaped formula: `blocks` at-most-one blocks of size 5 on
// consecutive variables, then 2*blocks positive 5-clauses in which every
// variable appears exactly twice. `planted` puts one item per block into a
// covering pattern that is an exact cover; a second pattern adds the two
// disjoint columns.
static std::string sgen_like(int blocks, bool satisfiable) {
  std::ostringstream out;
  const int n = 5 * blocks;
  std::vector<std::vector<int>> covering;
  // Pattern A: clause i takes the i-th item of every block? That would need
  // block count == 5. Instead: clause (b, j) for j in 0..1 holds items of
  // blocks b, b+1, b+2, b+3, b+4 (mod blocks) at positions chosen so that each
  // item lies in exactly two clauses and the diagonal items form a cover.
  // Items are numbered 5*b + p, p in 0..4.
  for (int b = 0; b < blocks; ++b)
    for (int j = 0; j < 2; ++j) {
      std::vector<int> clause;
      for (int t = 0; t < 5; ++t) {
        const int block = (b + t) % blocks;
        const int position = satisfiable ? (j == 0 ? t : (t + 1) % 5)
                                         : (j == 0 ? t : (t + 2) % 5);
        clause.push_back(5 * block + position + 1);
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
    // Every variable must appear in exactly two covering clauses; with the
    // patterns above each item (b, p) is in clause (b - p, 0) and clause
    // (b - p + 1, 1) for the satisfiable shape, so the shape is exact.
    Kernel kernel = parse(sgen_like(7, true));
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
    Kernel kernel = parse("p cnf 4 7\n-1 -2 0\n-1 -3 0\n-2 -3 0\n-1 -4 0\n"
                          "-2 -4 0\n-3 -4 0\n1 2 3 4 0\n");
    assert(exact_cover_model_apply(kernel) == ApplyResult::NoOp);
    assert(!kernel.stats.exact_cover_model_recognized);
  }
  {
    // Exhausting the deterministic budget without restarts reports a budget
    // stop and leaves the kernel untouched.
    Kernel kernel = parse(sgen_like(7, true));
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
