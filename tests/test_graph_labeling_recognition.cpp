#undef NDEBUG
#include "rules/lucky/graph_labeling_recognition.hpp"
#include "policies/heuristics.hpp"

#include <cassert>
#include <vector>

using namespace tactix;

// Indexed column covers with a sign-renamed Boolean encoding. The graph
// exclusions begin in column two (one-based separation three), as in the
// reference's interval encoding. This is only a recognition fixture, not a
// full Boolean encoding of graph labeling.
static void rectangle(Kernel &k, bool signed_encoding, bool malformed = false,
                      bool shortened = false) {
  constexpr uint32_t n = 4;
  kernel_reserve_vars(k, n * n + 2);
  const auto primary = [&](Var v) {
    return make_lit(v, signed_encoding && (v % 3 == 0));
  };
  if (shortened) kernel_assign_unit(k, negate(primary(3)));
  for (uint32_t column = 0; column < n; ++column) {
    std::vector<Lit> cover;
    for (uint32_t row = 0; row < n; ++row)
      if (k.value(primary(row * n + column)) >= 0)
        cover.push_back(primary(row * n + column));
    if (malformed && column == 1) cover[0] = primary(2);
    std::reverse(cover.begin(), cover.end());
    kernel_add_large_clause(k, cover.data(), cover.size(), false, 0);
  }
  for (uint32_t column = 2; column < n; ++column) {
    kernel_add_binary_clause(k, negate(primary(column)),
                             negate(primary(n + column)));
    kernel_add_binary_clause(k, negate(primary(n + column)),
                             negate(primary(2 * n + column)));
  }
}

int main() {
  for (bool signs : {false, true}) {
    Kernel k;
    rectangle(k, signs);
    const auto trail = k.trail;
    uint64_t work = 0;
    graph_labeling::Shape shape;
    const auto count = [&](uint64_t amount) { work += amount; return true; };
    assert(graph_labeling::recognize(k, 4, 1024, count, shape));
    assert(shape.vertices == 4 && shape.separation == 3);
    assert((shape.edges == std::vector<std::pair<uint32_t, uint32_t>>{
                               {0, 1}, {1, 2}}));
    assert((shape.adjacency[1] == std::vector<uint32_t>{0, 2}));
    for (Var v = 0; v < 16; ++v)
      assert(shape.primary[v] == make_lit(v, signs && v % 3 == 0));
    const uint64_t complete_work = work;
    for (uint64_t limit = 0; limit < complete_work; ++limit) {
      uint64_t used = 0;
      const auto bounded = [&](uint64_t amount) {
        if (amount > limit - used) return false;
        used += amount;
        return true;
      };
      assert(!graph_labeling::recognize(k, 4, 1024, bounded, shape));
      assert(!shape.vertices && shape.primary.empty() && shape.edges.empty());
      assert(used <= limit && k.trail == trail);
    }
    assert(!graph_labeling::recognize(k, 3, 1024, count, shape));
    assert(!graph_labeling::recognize(k, 4, 1, count, shape));
    assert(k.trail == trail);
  }
  {
    for (bool signs : {false, true}) {
      Kernel k;
      rectangle(k, signs, false, true);
      graph_labeling::Shape shape;
      assert(graph_labeling::recognize(k, 4, 1024,
                                       [](uint64_t) { return true; }, shape));
      assert(shape.vertices == 4 && shape.primary[3] == make_lit(3, signs));
      assert(k.value(shape.primary[3]) < 0 && k.trail.size() == 1);
    }
  }
  {
    Kernel k;
    rectangle(k, false, true);
    graph_labeling::Shape shape;
    assert(!graph_labeling::recognize(k, 4, 1024,
                                      [](uint64_t) { return true; }, shape));
  }
  {
    Kernel k;
    ScopedHeuristics h(k);
    rectangle(k, true);
    k.var_status.assign(k.nvars, VarStatus::Active);
    k.var_status[16] = VarStatus::Eliminated;
    heuristics_deactivate_var(k, 16);
    --k.unassigned_active;
    assert(kernel_compact_variables(k));
    graph_labeling::Shape shape;
    const auto charge = [](uint64_t) { return true; };
    assert(graph_labeling::recognize(k, 4, 1024, charge, shape));
    k.var_status.resize(k.nvars, VarStatus::Active);
    const Var internal = k.external_to_internal[2];
    assert(internal < k.nvars);
    k.var_status[internal] = VarStatus::Eliminated;
    assert(!graph_labeling::recognize(k, 4, 1024, charge, shape));
  }
}
