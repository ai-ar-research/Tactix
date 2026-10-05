#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/lucky/nol_hypergraph_model.hpp"
#include "strategy/native_feedback.hpp"

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

static std::string hypergraph(
    int vertices, const std::vector<std::array<int, 3>> &edges,
    bool malformed = false, int active_vertices = -1) {
  if (active_vertices < 0) active_vertices = vertices;
  std::ostringstream body;
  uint64_t clauses = 0;
  for (int vertex = 0; vertex < active_vertices; ++vertex) {
    body << 3 * vertex + 1 << ' ' << 3 * vertex + 2 << ' '
         << 3 * vertex + 3 << " 0\n";
    ++clauses;
  }
  for (const auto &edge : edges)
    for (int color = 0; color < 3; ++color) {
      body << -(3 * edge[0] + color + 1) << ' '
           << -(3 * edge[1] + color + 1) << ' '
           << -(3 * edge[2] + color + 1) << " 0\n";
      ++clauses;
    }
  if (malformed) {
    body << "1 -5 9 0\n";
    ++clauses;
  }
  std::ostringstream formula;
  formula << "p cnf " << 3 * vertices << ' ' << clauses << '\n'
          << body.str();
  return formula.str();
}

int main() {
  {
    // A path of hyperedges is greedily 3-colorable. The published assignment
    // is total and independently checked against every live clause.
    Kernel kernel = parse(hypergraph(5, {{{0, 1, 2}}, {{1, 2, 3}},
                                          {{2, 3, 4}}}));
    assert(nol_hypergraph_model_guard(kernel));
    assert(nol_hypergraph_model_apply(kernel) == ApplyResult::Sat);
    assert(kernel.stats.nol_hypergraph_model_calls == 1);
    assert(kernel.stats.nol_hypergraph_model_recognized == 1);
    assert(kernel.stats.nol_hypergraph_model_vertices == 5);
    assert(kernel.stats.nol_hypergraph_model_edges == 3);
    assert(kernel.stats.nol_hypergraph_model_sat == 1);
    assert(kernel.trail.size() == kernel.nvars);
  }
  {
    // A mixed-sign ternary is outside the exact noL language. Recognition is
    // observational: it neither assigns nor publishes guidance.
    Kernel kernel = parse(hypergraph(3, {{{0, 1, 2}}}, true));
    const uint64_t offered = kernel.stats.local_search_hints_offered;
    assert(nol_hypergraph_model_apply(kernel) == ApplyResult::NoOp);
    assert(!kernel.stats.nol_hypergraph_model_recognized);
    assert(kernel.trail.empty());
    assert(kernel.stats.local_search_hints_offered == offered);
  }
  {
    // One same-color clause is not a hypergraph edge: all three colors must
    // occur for the detector to attach the monochromatic-edge objective.
    Kernel kernel = parse(
        "p cnf 9 4\n"
        "1 2 3 0\n4 5 6 0\n7 8 9 0\n-1 -4 -7 0\n");
    assert(nol_hypergraph_model_apply(kernel) == ApplyResult::NoOp);
    assert(!kernel.stats.nol_hypergraph_model_recognized);
    assert(!kernel.stats.local_search_hints_offered);
  }
  {
    // A complete seven-vertex 3-uniform hypergraph has no 3-coloring. A small
    // generic rule-work allowance stops the search at an atomic boundary and
    // exposes the best complete coloring only as phase guidance.
    std::vector<std::array<int, 3>> edges;
    for (int a = 0; a < 7; ++a)
      for (int b = a + 1; b < 7; ++b)
        for (int c = b + 1; c < 7; ++c)
          edges.push_back({a, b, c});
    // The eighth variable triple is intentionally inactive. It must remain
    // unknown in a partial hint instead of receiving invented false phases.
    Kernel kernel = parse(hypergraph(8, edges, false, 7));
    kernel.work_budget_bounded = true;
    kernel.work_budget = 5000;
    assert(nol_hypergraph_model_apply(kernel) == ApplyResult::Ok);
    assert(kernel.stats.nol_hypergraph_model_recognized == 1);
    assert(kernel.stats.nol_hypergraph_model_budget_exhausted == 1);
    assert(kernel.trail.empty());
    const auto &hint = kernel.local_search_hints[
        size_t(strategy::LocalSearchKind::HypergraphColoring)];
    assert(hint.available() && hint.objective > 0 && hint.objective_exact);
    assert(hint.known_phases == 21);
    assert(hint.external_phases[21] == 0);
    assert(hint.external_phases[22] == 0);
    assert(hint.external_phases[23] == 0);
    assert(hint.effort.amount <= kernel.work_budget);
  }
  {
    // Stop polling occurs before any publication. An interrupted constructor
    // can retain private best state, but must not leak a model or hint.
    Kernel kernel = parse(hypergraph(4, {{{0, 1, 2}}, {{1, 2, 3}}}));
    volatile sig_atomic_t stop = 1;
    kernel.stop = &stop;
    assert(nol_hypergraph_model_apply(kernel) == ApplyResult::NoOp);
    assert(kernel.stats.nol_hypergraph_model_interrupted == 1);
    assert(kernel.trail.empty() && !kernel.stats.local_search_hints_offered);
  }
  {
    // Stable public instance 25 participates in ordinary registry/replay
    // dispatch and reaches the same validated constructor.
    Kernel kernel = parse(hypergraph(4, {{{0, 1, 2}}, {{1, 2, 3}}}));
    const RuleVTable &lucky = tactix_rules()[tactix_rule_id("Lucky")];
    uint64_t instances[LUCKY_INSTANCE_COUNT];
    assert(lucky.instances(kernel, instances, LUCKY_INSTANCE_COUNT) ==
           LUCKY_ACTIVE_INSTANCE_COUNT);
    assert(instances[LUCKY_NOL_HYPERGRAPH] == LUCKY_NOL_HYPERGRAPH);
    kernel.work_budget_bounded = true;
    kernel.work_budget = 1234;
    const auto feedback = execution::native_rule_feedback(
        kernel, "Lucky", LUCKY_NOL_HYPERGRAPH, "noL hypergraph coloring");
    assert(feedback.meter && feedback.meter(kernel) == 0);
    assert(feedback.description.effort_currency == "hypergraph-work");
    assert(feedback.description.effort_allowance.amount() == 1234);
    kernel.work_budget_bounded = false;
    kernel.work_budget = 0;
    assert(lucky.apply_instance(kernel, LUCKY_NOL_HYPERGRAPH) ==
           ApplyResult::Sat);
  }
  {
    // If recognition plus the reserved terminal publication cannot fit, the
    // bounded action declines without overshooting or publishing a hint.
    Kernel kernel = parse(hypergraph(5, {{{0, 1, 2}}, {{1, 2, 3}},
                                          {{2, 3, 4}}}));
    kernel.work_budget_bounded = true;
    kernel.work_budget = 100;
    assert(nol_hypergraph_model_apply(kernel) == ApplyResult::NoOp);
    assert(kernel.stats.nol_hypergraph_model_work <= kernel.work_budget);
    assert(!kernel.stats.local_search_hints_offered);
  }
  std::puts("test_nol_hypergraph_model OK");
  return 0;
}
