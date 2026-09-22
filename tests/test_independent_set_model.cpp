#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/lucky/independent_set_model.hpp"
#include "strategy/rule_parameters.hpp"

#include <algorithm>
#include <cassert>
#include <initializer_list>
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

// The owner's encoding: N graph variables, then a layered exact-count
// automaton with min(i, K) + 1 states after reading vertex i, requiring at
// least K selected vertices. States are one-hot per layer, transitions read
// the vertex, and the last layer forbids counts below K.
static std::string independent_set(int n, int k,
                                   const std::vector<std::pair<int, int>> &edges) {
  std::vector<std::vector<int>> layer(n + 1);
  int next = n + 1;
  for (int i = 1; i <= n; ++i) {
    const int states = std::min(i, k) + 1;
    for (int s = 0; s < states; ++s)
      layer[i].push_back(next++);
  }
  std::ostringstream body;
  int clauses = 0;
  const auto emit = [&](std::initializer_list<int> literals) {
    for (const int literal : literals)
      body << literal << ' ';
    body << "0\n";
    ++clauses;
  };
  for (const auto &[a, b] : edges)
    emit({-a, -b});
  // layer 1: state = value of vertex 1
  emit({1, layer[1][0]});
  emit({-1, layer[1][1]});
  for (int i = 2; i <= n; ++i) {
    const int previous = static_cast<int>(layer[i - 1].size());
    for (int s = 0; s < previous; ++s) {
      const int stay = std::min(s, k);
      const int up = std::min(s + 1, k);
      emit({-layer[i - 1][s], i, layer[i][stay]});
      emit({-layer[i - 1][s], -i, layer[i][up]});
    }
    // one-hot: at most one state per layer
    for (size_t s = 0; s < layer[i].size(); ++s)
      for (size_t t = s + 1; t < layer[i].size(); ++t)
        emit({-layer[i][s], -layer[i][t]});
  }
  // Target: the last layer holds a state, and any state below k implies
  // state k, which its one-hot pair forbids. No unit clause is used, so the
  // parser keeps the (k + 1)-wide clause the owner's detection reads.
  for (int s = 0; s < k; ++s)
    emit({-layer[n][s], layer[n][k]});
  std::ostringstream wide;
  for (int s = 0; s <= k; ++s)
    wide << layer[n][s] << ' ';
  wide << "0\n";
  ++clauses;
  std::ostringstream out;
  out << "p cnf " << (next - 1) << ' ' << clauses << '\n' << body.str()
      << wide.str();
  return out.str();
}

int main() {
  {
    // A 6-cycle plus one chord has an independent set of size 3.
    Kernel kernel = parse(independent_set(
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
    Kernel kernel = parse(independent_set(3, 2, {{1, 2}, {2, 3}, {3, 1}}));
    assert(independent_set_model_apply(kernel) == ApplyResult::NoOp);
    assert(kernel.stats.independent_set_model_recognized == 1);
    assert(kernel.trail.empty());
  }
  {
    // Not the shape: no clause wider than two.
    Kernel kernel = parse("p cnf 3 3\n-1 -2 0\n-2 -3 0\n1 3 0\n");
    assert(independent_set_model_apply(kernel) == ApplyResult::NoOp);
    assert(!kernel.stats.independent_set_model_recognized);
  }
  std::printf("test_independent_set_model OK\n");
  return 0;
}
