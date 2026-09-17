#undef NDEBUG
#include "rules/substitution_redundancy/clause_multiset.hpp"
#include "rules/substitution_redundancy/literal_permutation.hpp"

#include <array>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <numeric>
#include <random>
#include <set>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

using tactix::Lit;
using tactix::SymmetryClauseMultiset;
using Formula = std::vector<std::vector<Lit>>;

// Independent ordered-multiset oracle: materialize both complete multisets,
// without using source counts, hash keys, or the index under test.
template <bool AsSet = false, class Charge>
static bool reference(const Formula &clauses, const std::vector<Lit> &mapping,
                      Charge &&charge) {
  using Population = std::conditional_t<AsSet, std::set<std::vector<Lit>>,
                                      std::multiset<std::vector<Lit>>>;
  Population before, after;
  for (const auto &clause : clauses) {
    if (!charge(clause.size()))
      return false;
    auto source = clause;
    std::vector<Lit> image;
    for (Lit literal : clause)
      image.push_back(mapping[literal]);
    std::sort(source.begin(), source.end());
    std::sort(image.begin(), image.end());
    before.insert(std::move(source));
    after.insert(std::move(image));
  }
  return before == after;
}

struct ConstantHash {
  size_t operator()(const std::string &) const { return 0; }
};
struct ConstantViewHash {
  size_t operator()(std::string_view) const { return 0; }
};
struct StatefulStringHash {
  inline static size_t next_seed = 0;
  size_t seed = ++next_seed;
  size_t operator()(const std::string &key) const {
    return std::hash<std::string>{}(key) ^ seed;
  }
};

struct ConstantBinaryHash {
  size_t operator()(uint64_t) const { return 0; }
};

struct CountedBinaryHash {
  inline static size_t calls = 0;
  size_t operator()(uint64_t) const { ++calls; return 0; }
};

static void binary_representation_oracles() {
  const auto unlimited = [](uint64_t) { return true; };
  tactix::BasicSymmetryClauseMultiset<ConstantViewHash, CountedBinaryHash> index;
  // Relabel the same formulas into compact and sparse domains. All 6!
  // bijections are checked against ordered sets and multisets. This also
  // forces dense -> sparse -> dense rebuilds, unequal duplicate populations,
  // absent images, and the packed-zero/repeated-literal pair.
  for (const Lit offset : {0u, 1024u, 0u}) {
    size_t lookup_calls = 0;
    for (unsigned variant = 0; variant < 4; ++variant) {
      Formula formula;
      for (Lit a = 0; a < 6; ++a)
        for (Lit b = a; b < 6; ++b)
          formula.push_back({offset + a, offset + b});
      if (variant == 1) formula.push_back({offset, offset});
      if (variant == 2) formula.erase(formula.begin() + 2);
      if (variant == 3) {
        const auto original = formula;
        formula.insert(formula.end(), original.begin(), original.end());
        formula.push_back({offset + 1, offset});
      }
      index.build(formula);
      auto retained = index;
      index.build({});
      index = std::move(retained);
      std::set<std::vector<Lit>> distinct;
      for (auto clause : formula) {
        std::sort(clause.begin(), clause.end());
        distinct.insert(std::move(clause));
      }
      assert(index.distinct_clause_count() == distinct.size());
      std::array<Lit, 6> permutation{0, 1, 2, 3, 4, 5};
      do {
        std::vector<Lit> mapping(offset + 6);
        std::iota(mapping.begin(), mapping.end(), Lit{0});
        for (Lit i = 0; i < 6; ++i) mapping[offset + i] = offset + permutation[i];
        assert(index.preserves(mapping, unlimited) == reference(formula, mapping, unlimited));
        CountedBinaryHash::calls = 0;
        assert(index.preserves_set_support(mapping, unlimited) == reference<true>(formula, mapping, unlimited));
        lookup_calls += CountedBinaryHash::calls;
      } while (std::next_permutation(permutation.begin(), permutation.end()));
    }
    // Instrument only the supplied hasher, independently witnessing that set
    // membership uses the bitmap for compact domains and collision-safe table
    // lookup for sparse domains; neither path can pass vacuously.
    assert(offset ? lookup_calls > 0 : lookup_calls == 0);
  }
}

static tactix::SparseLiteralPermutation compact(const std::vector<Lit> &dense) {
  tactix::SparseLiteralPermutation result;
  result.domain = dense.size();
  for (Lit p = 0; p < dense.size(); ++p)
    if (dense[p] != p) result.moved.emplace_back(p, dense[p]);
  return result;
}

static void sparse_permutation_controls() {
  const auto unlimited = [](uint64_t) { return true; };
  // Independent exhaustive total-function oracle includes non-injective and
  // unsigned permutations, then checks every possible retired-variable set.
  for (unsigned encoded = 0; encoded < 256; ++encoded) {
    std::vector<Lit> dense(4);
    unsigned rest = encoded;
    for (auto &target : dense) { target = rest % 4; rest /= 4; }
    for (unsigned retired = 0; retired < 4; ++retired) {
      auto sorted = dense;
      std::sort(sorted.begin(), sorted.end());
      bool valid = sorted == std::vector<Lit>({0, 1, 2, 3});
      for (Lit p = 0; p < 4; ++p)
        valid &= dense[p ^ 1] == (dense[p] ^ 1) &&
                 (!(retired & (1u << (p / 2))) || dense[p] == p);
      auto sparse = compact(dense);
      assert(sparse.validate([&](tactix::Var v) { return retired & (1u << v); }, unlimited) == valid);
      if (!valid) continue;
      std::vector<std::pair<Lit, Lit>> expected, actual;
      for (Lit p = 0; p < 4; p += 2)
        if (dense[p] != p) expected.emplace_back(p, dense[p]);
      sparse.positive_moves([&](Lit p, Lit target) { actual.emplace_back(p, target); });
      assert(actual == expected); // Ordered DSR substitution tail, not map equality.
    }
  }
  const auto active = [](tactix::Var) { return false; };
  const auto valid = compact({2, 3, 0, 1});
  for (unsigned malformed = 0; malformed < 7; ++malformed) {
    auto bad = valid;
    switch (malformed) {
    case 0: bad.moved[0].first = 4; break;
    case 1: bad.moved[0].second = 4; break;
    case 2: bad.moved[0].second = 0; break;
    case 3: std::swap(bad.moved[0], bad.moved[1]); break;
    case 4: bad.moved[1].first = bad.moved[0].first; break;
    case 5: bad.moved.pop_back(); break;
    case 6: bad.domain = 3; break;
    }
    assert(!bad.validate(active, unlimited));
  }
  for (unsigned stop = 0; stop <= valid.moved.size(); ++stop) {
    unsigned calls = 0;
    assert(!valid.validate(active, [&](uint64_t) { return calls++ < stop; }));
    assert(valid.validate(active, unlimited));
  }
}

// The lookup table must own stable key bytes after mapped scratch, original
// snapshots and copied/rebuilt siblings have all changed or been destroyed.
static void pooled_key_lifetimes() {
  const auto unlimited = [](uint64_t) { return true; };
  std::vector<Lit> mapping = {2, 3, 0, 1, 4, 5, 6, 7};
  Formula formula = {{}, {0}, {2}, {0, 4, 6}, {2, 4, 6},
      {0, 4, 4, 5, 6, 7}, {2, 4, 4, 5, 6, 7}};
  SymmetryClauseMultiset retained;
  {
    SymmetryClauseMultiset original;
    original.build(formula);
    retained = original;
    original.build({{0, 4, 4, 5, 6, 7}});
    assert(!original.preserves_set_support(mapping, unlimited));
    assert(retained.preserves(mapping, unlimited));
  }
  auto moved = std::move(retained);
  assert(moved.preserves(mapping, unlimited));
  auto sibling = moved;
  moved.build({});
  assert(sibling.preserves(mapping, unlimited));
  sibling.build(formula);
  assert(sibling.preserves(mapping, unlimited));
  // Force arena growth with distinct long clauses, then overwrite input and
  // probe buffers. Values include zero bytes; length remains part of equality.
  Formula many;
  for (unsigned n = 0; n < 2048; ++n) {
    std::vector<Lit> clause = {0, 2, 4, 6};
    for (unsigned bit = 0; bit < 11; ++bit)
      if (n & (1u << bit)) clause.insert(clause.end(), bit + 1, Lit{4});
    many.push_back(std::move(clause));
  }
  sibling.build(many);
  many.clear();
  auto copied = sibling;
  sibling.build(formula);
  assert(copied.preserves(mapping, unlimited));
  tactix::BasicSymmetryClauseMultiset<ConstantHash, ConstantBinaryHash> collisions;
  collisions.build(formula);
  tactix::BasicSymmetryClauseMultiset<StatefulStringHash> stateful;
  stateful.build(formula);
  auto stateful_copy = stateful;
  stateful.build({{0}});
  assert(stateful_copy.preserves(mapping, unlimited));
  auto collision_copy = collisions;
  collisions.build({{0, 4, 4, 5, 6, 7}});
  assert(collision_copy.preserves(mapping, unlimited));
  assert(!collisions.preserves_set_support(mapping, unlimited));
}

static void collision_population_rebuilds() {
  const auto unlimited = [](uint64_t) { return true; };
  tactix::BasicSymmetryClauseMultiset<ConstantViewHash, ConstantBinaryHash> index;
  // Reuse the same tables across large, tiny and empty snapshots. Every
  // lookup collides; varied lengths include empty keys and packed binary zero.
  for (const unsigned width : {129u, 1u, 17u, 0u, 257u, 2u}) {
    Formula formula = {{}, {0}, {2}, {0, 0}, {2, 2}};
    for (unsigned i = 0; i < width; ++i) {
      const Lit tail = 6 + 2 * i;
      formula.push_back({0, 4, tail});
      formula.push_back({2, 4, tail});
      formula.push_back({0, tail});
      formula.push_back({2, tail});
      if (i % 3 == 0) formula.push_back({tail, 4, 0});
    }
    std::vector<Lit> mapping(8 + 2 * width);
    std::iota(mapping.begin(), mapping.end(), Lit{0});
    std::swap(mapping[0], mapping[2]);
    std::swap(mapping[1], mapping[3]);
    index.build(formula);
    std::set<std::vector<Lit>> distinct;
    for (auto clause : formula) {
      std::sort(clause.begin(), clause.end());
      distinct.insert(std::move(clause));
    }
    assert(index.distinct_clause_count() == distinct.size());
    assert(index.preserves(mapping, unlimited) == reference(formula, mapping, unlimited));
    assert(index.preserves_set_support(mapping, unlimited));
    auto copy = index;
    index.build({});
    assert(copy.preserves_set_support(mapping, unlimited));
    auto moved = std::move(copy);
    copy.build(formula); // Rebuilding a moved-from owner must restore its table.
    assert(copy.preserves_set_support(mapping, unlimited));
    index = moved;
    moved.build({});
    assert(index.distinct_clause_count() == distinct.size());
    assert(index.preserves_set_support(mapping, unlimited));
    formula.erase(formula.begin() + 2); // Remove only unit {2}.
    index.build(formula);
    assert(!reference<true>(formula, mapping, unlimited));
    assert(!index.preserves_set_support(mapping, unlimited));
    tactix::SymmetryClauseVisitWorkspace workspace;
    const auto sparse = compact(mapping);
    assert(!index.preserves_set_moved(sparse, sparse.moved, workspace, unlimited));
  }
}

static void sparse_generator_accounting_matches_dense() {
  const std::vector<Formula> formulas = {
      {}, {{}}, {{0, 2}, {1, 3}, {4, 5}}, {{0}, {0, 2}},
      {{0, 2}, {0, 2}, {1, 3}}, {{0, 0, 2}, {2, 0}, {1, 3, 5}},
      {{0, 2, 4}, {0, 3, 4}, {1, 2, 5}, {1, 3, 5}}};
  size_t compared = 0, accepts = 0, rejects = 0, budget_stops = 0;
  for (const auto &formula : formulas) {
    SymmetryClauseMultiset index;
    index.build(formula);
    tactix::SymmetryClauseVisitWorkspace workspace;
    std::array<Lit, 3> vars{0, 1, 2};
    do {
      for (unsigned phases = 0; phases < 8; ++phases) {
        std::vector<Lit> image(6);
        for (Lit source = 0; source < 6; ++source)
          image[source] = (2 * vars[source / 2]) ^ (source & 1u) ^
              ((phases >> (source / 2)) & 1u);
        const auto sparse = compact(image);
        assert(sparse.validate([](auto) { return false; }, [](auto) { return true; }));
        uint64_t full_work = 0;
        const bool oracle = reference<true>(formula, image, [](auto) { return true; });
        assert(index.preserves_set_support(image, [&](uint64_t n) {
          full_work += n; return true;
        }) == oracle);
        for (uint64_t budget = 0; budget <= full_work + 2; ++budget) {
          uint64_t old_work = 0, new_work = 0;
          bool old_stopped = false, new_stopped = false;
          const auto charge = [&](uint64_t &work, bool &stopped, uint64_t amount) {
            if (work >= budget || amount > budget - work) {
              work = budget; stopped = true; return false;
            }
            work += amount; return true;
          };
          tactix::SymmetryClauseCheckStats old_stats, new_stats;
          const bool old_valid = index.preserves_set_support(image,
              [&](uint64_t n) { return charge(old_work, old_stopped, n); }, &old_stats);
          // Force generation rollover during reuse as well as ordinary cache reuse.
          if (budget == 1) workspace.generation = UINT32_MAX;
          const bool new_valid = index.preserves_set_moved_reference(sparse, sparse.moved,
              workspace, [&](uint64_t n) { return charge(new_work, new_stopped, n); }, &new_stats);
          assert(old_valid == new_valid && old_work == new_work && old_stopped == new_stopped);
          assert(old_stats.fixed_clauses == new_stats.fixed_clauses &&
                 old_stats.mapped_clauses == new_stats.mapped_clauses);
          if (!workspace.literal_image.empty())
            for (Lit point = 0; point < workspace.literal_image.size(); ++point)
              assert(workspace.literal_image[point] == point);
          ++compared; accepts += new_valid;
          rejects += !new_valid && !new_stopped;
          budget_stops += new_stopped;
        }
      }
    } while (std::next_permutation(vars.begin(), vars.end()));
  }
  assert(accepts && rejects && budget_stops);
  std::printf("generator accounting oracle: %zu comparisons, %zu accepts, %zu semantic rejects, %zu budget stops PASS\n",
              compared, accepts, rejects, budget_stops);
}

int main() {
  binary_representation_oracles();
  sparse_generator_accounting_matches_dense();
  pooled_key_lifetimes();
  collision_population_rebuilds();
  sparse_permutation_controls();
  tactix::SymmetryClauseVisitWorkspace sparse_workspace, collision_workspace, view_collision_workspace;
  std::vector<Formula> formulas = {
      {}, {{}}, {{}, {}}, {{0}}, {{0}, {2}}, {{0}, {0}, {2}},
      {{0, 2}, {2, 0}, {1, 3}}, {{0, 0}, {2, 2}},
      {{0, 1}, {2, 3}}, {{0, 2}, {0, 2}, {2, 4}, {4, 2}},
      {{0, 2, 4}, {4, 2, 0}, {1}, {3}, {5}},
  };
  std::mt19937 random(3917);
  for (unsigned i = 0; i < 32; ++i) {
    Formula formula;
    for (unsigned j = 0; j < 12; ++j) {
      std::vector<Lit> clause(random() % 6);
      for (auto &literal : clause)
        literal = random() % 8;
      formula.push_back(clause);
      if (random() % 3 == 0) {
        std::reverse(clause.begin(), clause.end());
        formula.push_back(std::move(clause));
      }
    }
    formulas.push_back(std::move(formula));
  }

  uint64_t comparisons = 0;
  for (const auto &formula : formulas) {
    SymmetryClauseMultiset index;
    index.build(formula);
    tactix::BasicSymmetryClauseMultiset<ConstantHash, ConstantBinaryHash> collisions;
    collisions.build(formula);
    tactix::BasicSymmetryClauseMultiset<ConstantViewHash, ConstantBinaryHash> view_collisions;
    view_collisions.build(formula);
    std::array<Lit, 4> variables = {0, 1, 2, 3};
    do {
      for (unsigned phases = 0; phases < 16; ++phases) {
        std::vector<Lit> mapping(8);
        for (unsigned v = 0; v < variables.size(); ++v) {
          mapping[2 * v] = 2 * variables[v] + ((phases >> v) & 1);
          mapping[2 * v + 1] = mapping[2 * v] ^ 1;
        }
        std::vector<uint64_t> charged, expected;
        tactix::SymmetryClauseCheckStats stats;
        const bool actual = index.preserves(mapping, [&](uint64_t n) {
          charged.push_back(n);
          return true;
        }, &stats);
        const bool oracle = reference(formula, mapping, [&](uint64_t n) {
          expected.push_back(n);
          return true;
        });
        assert(actual == oracle && charged == expected);
        tactix::SymmetryClauseCheckStats support_stats;
        assert(index.preserves_support(mapping, [](uint64_t) { return true; },
                                       &support_stats) == oracle);
        assert(support_stats.fixed_clauses == stats.fixed_clauses &&
               support_stats.mapped_clauses == stats.mapped_clauses);
        assert(collisions.preserves_support(mapping, [](uint64_t) { return true; }) == oracle);
        assert(collisions.preserves(mapping, [](uint64_t) { return true; }) == oracle);
        assert(view_collisions.preserves(mapping, [](uint64_t) { return true; }) == oracle);
        assert(view_collisions.preserves_support(mapping, [](uint64_t) { return true; }) == oracle);
        const bool set_oracle = reference<true>(formula, mapping, [](uint64_t) { return true; });
        std::vector<uint64_t> set_work, multiset_work;
        assert(index.preserves_set_support(mapping, [&](uint64_t n) { set_work.push_back(n); return true; }) == set_oracle);
        assert(index.preserves_support(mapping, [&](uint64_t n) { multiset_work.push_back(n); return true; }) == oracle);
        assert(set_work == multiset_work);
        assert(collisions.preserves_set_support(mapping, [](uint64_t) { return true; }) == set_oracle);
        assert(view_collisions.preserves_set_support(mapping, [](uint64_t) { return true; }) == set_oracle);
        const auto sparse_mapping = compact(mapping);
        assert(sparse_mapping.validate([](tactix::Var) { return false; }, [](uint64_t) { return true; }));
        tactix::SymmetryClauseCheckStats sparse_stats;
        assert(index.preserves_set_moved(sparse_mapping, sparse_mapping.moved,
            sparse_workspace, [](uint64_t) { return true; }, &sparse_stats) == set_oracle);
        assert(collisions.preserves_set_moved(sparse_mapping, sparse_mapping.moved,
            collision_workspace, [](uint64_t) { return true; }) == set_oracle);
        assert(view_collisions.preserves_set_moved(sparse_mapping, sparse_mapping.moved,
            view_collision_workspace, [](uint64_t) { return true; }) == set_oracle);
        assert(sparse_stats.fixed_clauses == stats.fixed_clauses &&
               sparse_stats.mapped_clauses == stats.mapped_clauses);
        assert(charged.size() == formula.size());
        uint64_t fixed = 0;
        for (const auto &clause : formula)
          fixed += std::all_of(clause.begin(), clause.end(),
                              [&](Lit literal) { return mapping[literal] == literal; });
        assert(stats.fixed_clauses == fixed);
        assert(stats.mapped_clauses + fixed == formula.size());
        ++comparisons;
      }
    } while (std::next_permutation(variables.begin(), variables.end()));
  }

  // A small moved support must not rescan thousands of fixed long clauses.
  Formula sparse = {{0}, {2}};
  for (unsigned copy = 0; copy < 1000; ++copy)
    sparse.push_back({4, 5, 6, 7, 8, 9, 10, 11});
  std::vector<Lit> sparse_map(12);
  std::iota(sparse_map.begin(), sparse_map.end(), 0);
  std::swap(sparse_map[0], sparse_map[2]);
  std::swap(sparse_map[1], sparse_map[3]);
  SymmetryClauseMultiset sparse_index;
  sparse_index.build(sparse);
  uint64_t full_ticks = 0, indexed_ticks = 0;
  assert(sparse_index.preserves(sparse_map, [&](uint64_t n) { full_ticks += n; return true; }));
  assert(sparse_index.preserves_support(sparse_map, [&](uint64_t n) { indexed_ticks += n; return true; }));
  assert(indexed_ticks < full_ticks / 2);
  uint64_t calls = 0;
  assert(sparse_index.preserves_support(sparse_map, [&](uint64_t) { ++calls; return true; }));
  for (const uint64_t stop_at : {uint64_t{0}, calls / 2, calls - 1}) {
    uint64_t at = 0;
    assert(!sparse_index.preserves_support(sparse_map, [&](uint64_t) { return at++ < stop_at; }));
    at = 0;
    assert(!sparse_index.preserves_set_support(sparse_map, [&](uint64_t) { return at++ < stop_at; }));
  }
  uint64_t charged_support = 0;
  assert(!sparse_index.preserves_support(sparse_map, [&](uint64_t n) {
    charged_support += n;
    return charged_support < indexed_ticks;
  }));
  auto compact_map = compact(sparse_map);
  compact_map.domain = 2000000; // Ambient identities do not add validation work.
  tactix::SymmetryClauseVisitWorkspace visits;
  uint64_t first_work = 0, repeated_work = 0, visit_calls = 0;
  assert(sparse_index.preserves_set_moved(compact_map, compact_map.moved, visits,
      [&](uint64_t n) { first_work += n; return true; }));
  assert(sparse_index.preserves_set_moved(compact_map, compact_map.moved, visits,
      [&](uint64_t n) { repeated_work += n; ++visit_calls; return true; }));
  assert(first_work - repeated_work == sparse.size() + compact_map.domain);
  assert(repeated_work == 16); // Sources, incidences, literals and support install/restore.
  for (uint64_t stop_at = 0; stop_at < visit_calls; ++stop_at) {
    uint64_t call = 0;
    assert(!sparse_index.preserves_set_moved(compact_map, compact_map.moved, visits,
        [&](uint64_t) { return call++ < stop_at; }));
    for (Lit p = 0; p < visits.literal_image.size(); ++p)
      assert(visits.literal_image[p] == p);
    assert(sparse_index.preserves_set_moved(compact_map, compact_map.moved, visits,
        [](uint64_t) { return true; }));
  }
  visits.generation = UINT32_MAX;
  assert(!sparse_index.preserves_set_moved(compact_map, compact_map.moved, visits,
      [](uint64_t n) { return n == 0; }));
  assert(visits.generation == UINT32_MAX);
  uint64_t overflow_work = 0;
  assert(sparse_index.preserves_set_moved(compact_map, compact_map.moved, visits,
      [&](uint64_t n) { overflow_work += n; return true; }));
  assert(overflow_work == first_work - compact_map.domain && visits.generation == 1);
  auto sparse_copy = sparse_index;
  sparse.push_back({0});
  assert(sparse_copy.preserves_support(sparse_map, [](uint64_t) { return true; }));
  sparse_copy.build(sparse);
  assert(!sparse_copy.preserves_support(sparse_map, [](uint64_t) { return true; }));

  const Formula unequal = {{0}, {0}, {2}, {}, {0, 0, 2}};
  const std::vector<Lit> swap = {2, 3, 0, 1};
  SymmetryClauseMultiset index;
  index.build(unequal);
  for (size_t stop_at = 0; stop_at <= unequal.size(); ++stop_at) {
    std::vector<uint64_t> charged, expected;
    const bool actual = index.preserves(swap, [&](uint64_t n) {
      charged.push_back(n);
      return charged.size() <= stop_at;
    });
    const bool oracle = reference(unequal, swap, [&](uint64_t n) {
      expected.push_back(n);
      return expected.size() <= stop_at;
    });
    assert(!actual && actual == oracle && charged == expected);
  }
  // Membership alone would wrongly accept a swap of these unequal unit counts.
  const Formula counts_only = {{0}, {0}, {2}};
  index.build(counts_only);
  assert(!index.preserves(swap, [](uint64_t) { return true; }));
  assert(index.preserves_set_support(swap, [](uint64_t) { return true; }));
  index.build({{0}, {0}}); // Missing image is not a duplicate-count difference.
  assert(!index.preserves_set_support(swap, [](uint64_t) { return true; }));
  const std::vector<Lit> binary_swap = {2, 3, 0, 1, 4, 5};
  index.build({{0, 4}, {0, 4}, {2, 4}});
  assert(index.preserves_set_support(binary_swap, [](uint64_t) { return true; }));
  assert(!index.preserves_support(binary_swap, [](uint64_t) { return true; }));
  index.build({{4, 5}, {4, 5}, {0, 4}});
  assert(!index.preserves_set_support(binary_swap, [](uint64_t) { return true; }));
  const Formula equal = {{0}, {0}, {2}, {2}, {}};
  SymmetryClauseMultiset copied;
  {
    SymmetryClauseMultiset original;
    original.build(equal);
    copied = original;
  }
  assert(copied.preserves(swap, [](uint64_t) { return true; }));
  // The index owns its snapshot; later mutation of the input cannot stale it.
  Formula mutable_input = equal;
  copied.build(mutable_input);
  mutable_input.push_back({0});
  assert(copied.preserves(swap, [](uint64_t) { return true; }));
  // Rebuilding discards old counts and ordinal metadata.
  copied.build(counts_only);
  assert(!copied.preserves(swap, [](uint64_t) { return true; }));
  // Invalid maps cannot satisfy the helper's bijection precondition. The
  // production caller checks it before entry; assertion builds reject misuse.
  for (const std::vector<Lit> &invalid :
       {std::vector<Lit>{0, 1, 0, 1}, std::vector<Lit>{0, 1, 2, 4}}) {
    const pid_t child = fork();
    assert(child >= 0);
    if (!child) {
      const rlimit no_core{0, 0};
      assert(setrlimit(RLIMIT_CORE, &no_core) == 0);
      close(STDERR_FILENO);
      copied.preserves(invalid, [](uint64_t) { return true; });
      _exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
  }
  std::printf("Clause multiset: %llu signed-permutation oracle comparisons PASS\n",
              static_cast<unsigned long long>(comparisons));
}
