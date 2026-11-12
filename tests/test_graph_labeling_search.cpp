// Graph-labeling internals with real failure modes not observable through
// the public actions: recognition, private model completion, the scored
// local moves, seed/Window search against brute-force permutation oracles,
// and the resumable Window cursor, each under every work cutoff.
#undef NDEBUG
#include "policies/heuristics.hpp"
#include "rules/lucky/graph_labeling_completion.hpp"
#include "rules/lucky/graph_labeling_recognition.hpp"
#include "rules/lucky/graph_labeling_repair.hpp"
#include "rules/lucky/graph_labeling_search.hpp"
#include "rules/lucky/graph_labeling_window_cursor.hpp"

#include <cassert>
#include <numeric>
#include <vector>

using namespace tactix;
using namespace tactix::graph_labeling;

// A work callback admitting at most `limit` units in total, counted in `used`.
static auto cutoff(uint64_t limit, uint64_t &used) {
  used = 0;
  return [limit, &used](uint64_t amount) {
    if (amount > limit - used) return false;
    used += amount;
    return true;
  };
}

// The four-vertex graph whose edge set is the bit mask over pairs a < b.
static Shape graph(uint32_t mask, uint32_t separation) {
  Shape s;
  s.vertices = 4;
  s.separation = separation;
  s.adjacency.resize(4);
  uint32_t bit = 0;
  for (uint32_t a = 0; a < 4; ++a)
    for (uint32_t b = a + 1; b < 4; ++b, ++bit)
      if (mask & (1u << bit)) {
        s.edges.emplace_back(a, b);
        s.adjacency[a].push_back(b);
        s.adjacency[b].push_back(a);
      }
  return s;
}

// Independent scorer: a permutation, and the (violations, squared deficit) pair.
static Score oracle(const Shape &shape, const Candidate &candidate) {
  auto sorted = candidate.labels;
  std::sort(sorted.begin(), sorted.end());
  for (uint32_t i = 0; i < shape.vertices; ++i) assert(sorted[i] == i);
  Score result{0, 0};
  for (uint32_t v = 0; v < shape.vertices; ++v)
    for (uint32_t u : shape.adjacency[v]) {
      if (u <= v) continue;
      const int64_t distance = std::abs(int64_t(candidate.labels[v]) -
                                        int64_t(candidate.labels[u]));
      if (distance >= shape.separation) continue;
      ++result.first;
      const uint64_t deficit = shape.separation - distance;
      result.second += deficit * deficit;
    }
  return result;
}

static void repair_and_anneal() {
  auto always = [](uint64_t) { return true; };
  for (uint32_t mask = 0; mask < 64; ++mask) {
    Shape s = graph(mask, 2);
    Candidate c;
    c.labels = {0, 1, 2, 3};
    do {
      c.score = oracle(s, c);
      for (uint32_t a = 0; a < 4; ++a)
        for (uint32_t b = a; b < 4; ++b) {
          ScoreDelta delta;
          assert(swap_delta(s, c, a, b, always, delta));
          Candidate swapped = c;
          std::swap(swapped.labels[a], swapped.labels[b]);
          assert(offset_score(c.score, delta) == oracle(s, swapped));
          auto best = c, current = c;
          std::vector<uint32_t> at(4);
          for (uint32_t v = 0; v < 4; ++v) at[c.labels[v]] = v;
          assert(install_swap(current, best, at, a, b, delta, always));
          assert(current.labels == swapped.labels && current.score == oracle(s, current));
          assert(best.score <= c.score && best.score == oracle(s, best));
          for (uint32_t v = 0; v < 4; ++v) assert(at[current.labels[v]] == v);
        }
    } while (std::next_permutation(c.labels.begin(), c.labels.end()));
    c.score = oracle(s, c);
    Candidate repaired = c, annealed = c;
    uint64_t rounds = 0, attempts = 0;
    assert(repair(s, 3, 20, 123, 456, {}, {}, always, repaired, rounds, attempts));
    assert(repaired.score <= c.score && repaired.score == oracle(s, repaired));
    assert(rounds <= 3 && attempts <= 60);
    assert(anneal(s, 50, 456, {}, always, annealed, attempts));
    assert(annealed.score <= c.score && annealed.score == oracle(s, annealed));
    assert(attempts <= 50);
  }
  // Every work cutoff of an impossible clique retains a valid, correctly
  // scored best candidate, including cutoffs inside delta evaluation/export.
  Shape s = graph(63, 2);
  Candidate initial;
  initial.labels = {0, 1, 2, 3};
  initial.score = oracle(s, initial);
  for (bool use_anneal : {false, true}) {
    uint64_t work = 0, rounds = 0, attempts = 0;
    const auto count = [&](uint64_t amount) { work += amount; return true; };
    Candidate complete = initial;
    if (use_anneal) assert(anneal(s, 5, 456, {}, count, complete, attempts));
    else assert(repair(s, 1, 3, 123, 456, {}, {}, count, complete, rounds, attempts));
    for (uint64_t limit = 0; limit < work; ++limit) {
      uint64_t used;
      const auto bounded = cutoff(limit, used);
      Candidate partial = initial;
      const bool finished = use_anneal
          ? anneal(s, 5, 456, {}, bounded, partial, attempts)
          : repair(s, 1, 3, 123, 456, {}, {}, bounded, partial, rounds, attempts);
      assert(!finished && used <= limit);
      assert(partial.score <= initial.score && partial.score == oracle(s, partial));
    }
  }
}

static void seeds_and_window_search() {
  {
    // Final repair can undo the seed's reflection convention. Recover it
    // without changing any pairwise distance or partially mutating on a cut.
    Candidate original;
    original.labels = {3, 0, 2, 1};
    original.score = {7, 19};
    for (uint64_t limit = 0; limit <= 4; ++limit) {
      Candidate candidate = original;
      uint64_t used;
      const auto charge = cutoff(limit, used);
      assert(normalize_reflection(candidate, charge) == (limit == 4));
      assert(candidate.score == original.score);
      if (limit < 4) {
        assert(candidate.labels == original.labels && used == 0);
        continue;
      }
      assert((candidate.labels == std::vector<uint32_t>{0, 3, 1, 2}));
      for (size_t a = 0; a < 4; ++a)
        for (size_t b = 0; b < 4; ++b)
          assert(std::abs(int(candidate.labels[a]) - int(candidate.labels[b])) ==
                 std::abs(int(original.labels[a]) - int(original.labels[b])));
      assert(normalize_reflection(candidate, charge));
      assert(used == 4); // Already normalized: no additional work or mutation.
    }
  }
  // Exhaust every simple four-vertex graph, including disconnected graphs and
  // an impossible separation. Check complete permutations and objective values
  // independently of the constructor's edge scorer, plus deterministic replay.
  for (uint32_t mask = 0; mask < 64; ++mask) {
    const Shape shape = graph(mask, 3);
    Candidate candidate;
    uint64_t work = 0;
    const auto charge = [&](uint64_t amount) { work += amount; return true; };
    uint32_t completed = 0;
    assert(order_seeds(shape, charge, candidate, completed));
    assert(completed == 24 && candidate.score == oracle(shape, candidate));
    Candidate replay;
    assert(order_seeds(shape, charge, replay, completed));
    assert(replay.labels == candidate.labels && replay.score == candidate.score);
    // With all starts/choices allowed this finite search must agree with a
    // brute-force permutation oracle, even when the seed family misses a model.
    Candidate brute;
    brute.labels = {0, 1, 2, 3};
    bool exists = false;
    do {
      exists |= oracle(shape, brute).first == 0;
    } while (std::next_permutation(brute.labels.begin(), brute.labels.end()));
    Candidate searched = candidate;
    uint64_t nodes = 0;
    const WindowLimits limits{1000, 4, 4, 123};
    const auto unbounded = [](uint64_t) { return true; };
    const auto status = window_search(shape, limits, unbounded, searched, nodes);
    assert((status == WindowStatus::Found) == exists);
    assert(status != WindowStatus::Limited && nodes <= limits.nodes);
    if (exists) {
      assert(searched.score.first == 0);
      assert(searched.score == oracle(shape, searched));
    } else assert(searched.labels == candidate.labels);
    if (!mask) {
      uint64_t dfs_work = 0;
      const auto dfs_count = [&](uint64_t amount) { dfs_work += amount; return true; };
      assert(window_search(shape, limits, dfs_count, searched, nodes) == WindowStatus::Found);
      for (uint64_t limit = 0; limit < dfs_work; ++limit) {
        uint64_t used;
        const auto bounded = cutoff(limit, used);
        searched = candidate;
        assert(window_search(shape, limits, bounded, searched, nodes) == WindowStatus::Limited);
        assert(searched.labels == candidate.labels && searched.score == candidate.score);
      }
    }
    if (mask == 0) assert(candidate.score.first == 0);
    if (mask != 63) continue;
    // Every possible early stop retains an evaluated permutation, never an
    // interrupted sort or a half-built assignment.
    const uint64_t full_work = work / 2;
    for (uint64_t limit = 0; limit < full_work; ++limit) {
      uint64_t used;
      const auto bounded = cutoff(limit, used);
      Candidate partial;
      assert(!order_seeds(shape, bounded, partial, completed));
      assert(used <= limit && completed < 24);
      if (completed) assert(partial.score == oracle(shape, partial));
      else assert(partial.labels.empty());
    }
  }
}

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

static void recognition_cases() {
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
      uint64_t used;
      const auto bounded = cutoff(limit, used);
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

static void chain(Kernel &k) {
  kernel_reserve_vars(k, 6);
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  kernel_add_binary_clause(k, internalize(-4), internalize(5));
  const Lit large[] = {internalize(2), internalize(-5), internalize(6)};
  kernel_add_large_clause(k, large, 3, false, 0);
}

static void completion_cases() {
  Kernel k;
  ScopedHeuristics h(k);
  chain(k);
  const std::vector<Lit> trail(k.trail.begin(), k.trail.end());
  const auto saved = k.h->saved, best = k.h->best, target = k.h->target;
  const auto arena = k.arena;
  std::vector<uint8_t> model;
  const std::vector<int8_t> partial{1, -1, 0, 0, 0, 0};
  uint64_t total_work = 0;
  const auto count = [&](uint64_t amount) { total_work += amount; return true; };
  assert(complete_model(k, partial, 4096, count, model) == CompletionStatus::Model);
  assert((model == std::vector<uint8_t>{1, 0, 1, 1, 1, 1}));
  assert(k.trail == trail && k.arena == arena);
  assert(k.h->saved == saved && k.h->best == best && k.h->target == target);
  for (uint64_t limit = 0; limit < total_work; ++limit) {
    uint64_t used;
    const auto bounded = cutoff(limit, used);
    assert(complete_model(k, partial, 4096, bounded, model) == CompletionStatus::Limited);
    assert(model.empty() && used <= limit && k.trail == trail);
  }
  const auto always = [](uint64_t) { return true; };
  assert(complete_model(k, partial, 1, always, model) == CompletionStatus::Limited);
  assert(model.empty());
  // A graph-level candidate contradicting any additional live constraint is
  // rejected without a SAT publication, including retained redundant clauses.
  const Lit contradiction[] = {internalize(-3), internalize(-4), internalize(-5)};
  kernel_add_large_clause(k, contradiction, 3, true, 0);
  assert(complete_model(k, partial, 4096, always, model) == CompletionStatus::NoModel);
  assert(model.empty() && k.trail == trail);
  {
    Kernel root;
    kernel_reserve_vars(root, 2);
    kernel_assign_unit(root, internalize(-1));
    assert(complete_model(root, {1, 0}, 4096, always, model) == CompletionStatus::NoModel);
    assert(model.empty() && root.trail.size() == 1);
    assert(complete_model(root, {0, 0}, 4096, always, model) == CompletionStatus::Model);
    assert((model == std::vector<uint8_t>{0, 0}));
  }
  {
    Kernel retired;
    kernel_reserve_vars(retired, 2);
    kernel_add_binary_clause(retired, internalize(1), internalize(2));
    retired.var_status.assign(2, VarStatus::Active);
    retired.var_status[0] = VarStatus::Eliminated;
    assert(complete_model(retired, {1, -1}, 4096, always, model) == CompletionStatus::NoModel);
    assert(model.empty() && retired.trail.empty());
  }
}

static Shape complete_shape(uint32_t n) {
  Shape shape;
  shape.vertices = n;
  shape.separation = 1;
  shape.adjacency.resize(n);
  for (uint32_t a = 0; a < n; ++a)
    for (uint32_t b = a + 1; b < n; ++b) {
      shape.edges.emplace_back(a, b);
      shape.adjacency[a].push_back(b);
      shape.adjacency[b].push_back(a);
    }
  return shape;
}

static WindowCursorStatus finish(WindowCursor &cursor, const Shape &shape,
                                 uint64_t slice, Candidate &candidate) {
  for (uint64_t calls = 0; calls < 100000; ++calls) {
    const auto result = cursor.step(shape, slice, candidate);
    if (result.status != WindowCursorStatus::Pending) return result.status;
  }
  assert(false && "cursor did not terminate");
  return WindowCursorStatus::NoCandidate;
}

static void window_cursor_cases() {
  const Shape shape = complete_shape(7);
  const WindowLimits limits{100000, 4, 4, 0x51a7e};
  Candidate reference;
  uint64_t reference_work = 0, reference_nodes = 0;
  const auto reference_status = window_search(
      shape, limits,
      [&](uint64_t amount) {
        reference_work += amount;
        return true;
      },
      reference, reference_nodes);
  assert(reference_status == WindowStatus::Found);
  assert(!reference.score.first);

  const uint64_t required = WindowCursor::required_workspace(shape);
  assert(required);
  {
    WindowCursor rejected;
    assert(rejected.initialize(shape, limits, reference_work, required - 1) ==
           WindowCursorAdmission::WorkspaceExhausted);
    assert(!rejected.ready());
    assert(!rejected.work() && !rejected.nodes());
  }

  // Slicing replays the one-shot search: the same work, nodes and candidate.
  Candidate sliced;
  WindowCursor small;
  assert(small.initialize(shape, limits, reference_work, required) ==
         WindowCursorAdmission::Ready);
  assert(finish(small, shape, 31, sliced) == WindowCursorStatus::Found);
  assert(small.work() == reference_work && small.nodes() == reference_nodes);
  assert(sliced.labels == reference.labels && sliced.score == reference.score);

  Candidate prepaid_candidate;
  WindowCursor prepaid;
  const uint64_t setup = WindowCursor::initialization_work(shape);
  assert(setup < reference_work);
  assert(prepaid.initialize_after_charge(shape, limits, reference_work, required) ==
         WindowCursorAdmission::Ready);
  assert(prepaid.work() == setup);
  const auto prepaid_result =
      prepaid.step(shape, reference_work - setup, prepaid_candidate);
  assert(prepaid_result.status == WindowCursorStatus::Found);
  assert(prepaid_result.work == reference_work - setup);
  assert(prepaid.work() == reference_work && prepaid.nodes() == reference_nodes);
  assert(prepaid_candidate.labels == reference.labels);

  // Interruption differs from either per-slice or total-cap exhaustion and
  // leaves the exact next charged mutation available for resumption.
  WindowCursor interrupted;
  Candidate interrupted_candidate;
  assert(interrupted.initialize(shape, limits, reference_work, required) ==
         WindowCursorAdmission::Ready);
  uint64_t polls = 0;
  const auto stopped = interrupted.step(
      shape, reference_work, interrupted_candidate,
      [&] { return polls++ == 20; });
  assert(stopped.status == WindowCursorStatus::Interrupted);
  assert(finish(interrupted, shape, 43, interrupted_candidate) ==
         WindowCursorStatus::Found);
  assert(interrupted.work() == reference_work && interrupted.nodes() == reference_nodes);
  assert(interrupted_candidate.labels == reference.labels);

  WindowCursor capped;
  Candidate capped_candidate;
  const uint64_t capped_allowance = reference_work - 1;
  uint64_t reference_capped_work = 0, reference_capped_nodes = 0;
  Candidate reference_capped_candidate;
  const auto reference_capped = window_search(
      shape, limits,
      [&](uint64_t amount) {
        if (amount > capped_allowance - reference_capped_work) return false;
        reference_capped_work += amount;
        return true;
      },
      reference_capped_candidate, reference_capped_nodes);
  assert(reference_capped == WindowStatus::Limited);
  assert(capped.initialize(shape, limits, capped_allowance, required) ==
         WindowCursorAdmission::Ready);
  assert(finish(capped, shape, 29, capped_candidate) ==
         WindowCursorStatus::TotalWorkExhausted);
  assert(capped.work() == reference_capped_work &&
         capped.nodes() == reference_capped_nodes);
  assert(capped_candidate.labels == reference_capped_candidate.labels);
  assert(capped_candidate.score == reference_capped_candidate.score);
  assert(capped.terminal());
}

int main() {
  recognition_cases();
  completion_cases();
  repair_and_anneal();
  seeds_and_window_search();
  window_cursor_cases();
}
