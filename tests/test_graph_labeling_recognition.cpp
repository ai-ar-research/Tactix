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

// An n*n indexed rectangle (one column cover per column, path edges in the
// last column; n = 0 omits it) followed by unrelated material. `filler`
// ternary clauses use only externals from 64 on, outside every region below
// eight vertices; the `in_region` ternary clauses mix three columns of the
// rectangle, so they are region clauses that never index a column.
static void padded_rectangle(Kernel &k, uint32_t n, uint32_t filler_variables,
                             uint32_t filler, uint32_t in_region) {
  constexpr Var base = 64;
  assert(n * n <= base);
  kernel_reserve_vars(k, base + filler_variables);
  for (uint32_t column = 0; column < n; ++column) {
    std::vector<Lit> cover;
    for (uint32_t row = 0; row < n; ++row)
      cover.push_back(make_lit(row * n + column, false));
    kernel_add_large_clause(k, cover.data(), cover.size(), false, 0);
  }
  for (uint32_t row = 0; row + 1 < n; ++row)
    kernel_add_binary_clause(k, make_lit(row * n + n - 1, true),
                             make_lit((row + 1) * n + n - 1, true));
  for (uint32_t i = 0; i < filler; ++i) {
    const Lit lits[3] = {make_lit(base + i % filler_variables, i & 1),
                         make_lit(base + (i + 1) % filler_variables, false),
                         make_lit(base + (i + 2) % filler_variables, i & 2)};
    kernel_add_large_clause(k, lits, 3, false, 0);
  }
  // Distinct rows in columns 0, 1 and 2..n-1: at most 8*8*8*6 clauses for n=8.
  for (uint32_t i = 0; i < in_region; ++i) {
    const Lit lits[3] = {make_lit((i % n) * n, false),
                         make_lit(((i / n) % n) * n + 1, false),
                         make_lit(((i / (n * n)) % n) * n + 2 + (i / (n * n * n)) % (n - 2),
                                  false)};
    kernel_add_large_clause(k, lits, 3, false, 0);
  }
}

static uint64_t recognition_work(Kernel &k, uint32_t maximum_vertices,
                                 bool &recognized, graph_labeling::Shape &shape) {
  uint64_t work = 0;
  recognized = graph_labeling::recognize(
      k, maximum_vertices, 1 << 20,
      [&](uint64_t amount) { work += amount; return true; }, shape);
  return work;
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
  {
    // Recognition reads only the primary region: material above it changes
    // neither the result nor the charged work, however large it is.
    graph_labeling::Shape reference;
    bool recognized = false;
    Kernel small;
    padded_rectangle(small, 8, 0, 0, 0);
    const uint64_t base = recognition_work(small, 8, recognized, reference);
    assert(recognized && reference.vertices == 8 && reference.separation == 8);
    assert(reference.edges.size() == 7);
    for (uint32_t filler : {1000u, 20000u}) {
      Kernel k;
      padded_rectangle(k, 8, 5000, filler, 0);
      graph_labeling::Shape shape;
      assert(recognition_work(k, 8, recognized, shape) == base);
      assert(recognized && shape.primary == reference.primary &&
             shape.edges == reference.edges &&
             shape.separation == reference.separation);
    }
  }
  {
    // Region clauses that index no column cost their watches and literals,
    // not the dimension: no per-clause O(n) row reset is charged.
    graph_labeling::Shape shape;
    bool recognized = false;
    Kernel few, many;
    padded_rectangle(few, 8, 0, 0, 1000);
    padded_rectangle(many, 8, 0, 0, 2000);
    const uint64_t a = recognition_work(few, 8, recognized, shape);
    assert(recognized && shape.vertices == 8);
    const uint64_t b = recognition_work(many, 8, recognized, shape);
    assert(recognized && shape.vertices == 8);
    // Two watch entries in each of the two clause passes plus at most three
    // literal reads: seven units per clause, below the eight-row reset.
    assert(b > a && b - a <= 7 * 1000);
  }
  {
    // A large formula without an indexed column is rejected after reading
    // only the region below min(maximum_vertices, isqrt(variables))^2.
    graph_labeling::Shape shape;
    bool recognized = true;
    Kernel small, large;
    padded_rectangle(small, 0, 4000, 1000, 0);
    padded_rectangle(large, 0, 4000, 50000, 0);
    const uint64_t a = recognition_work(small, 8, recognized, shape);
    assert(!recognized && !shape.vertices);
    const uint64_t b = recognition_work(large, 8, recognized, shape);
    // One unit per region variable; the 50,000 outside clauses are unread.
    assert(!recognized && !shape.vertices && a == 64 && b == 64);
  }
}
