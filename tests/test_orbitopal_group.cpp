#undef NDEBUG
#include "rules/substitution_redundancy/orbitopal_group.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <deque>
#include <random>
#include <numeric>
#include <set>
#include <string>
#include <vector>

using namespace tactix::orbitopal_group;

static std::vector<size_t> inverse(const std::vector<size_t> &image) {
  std::vector<size_t> result(image.size());
  for (size_t source = 0; source < image.size(); ++source)
    result[image[source]] = source;
  return result;
}

static std::vector<size_t> lift(const std::vector<Generator> &generators,
                                const GeneratorWord &word, bool rows) {
  const size_t size = rows ? generators.front().rows.size()
                           : generators.front().columns.size();
  std::vector<std::vector<size_t>> values(word.nodes.size());
  for (size_t at = 0; at < word.nodes.size(); ++at) {
    const auto &node = word.nodes[at];
    auto &image = values[at];
    image.resize(size);
    if (node.kind == WordKind::Identity)
      std::iota(image.begin(), image.end(), 0);
    else if (node.kind == WordKind::Generator) {
      const auto &forward = rows ? generators[node.first / 2].rows
                                 : generators[node.first / 2].columns;
      image = node.first & 1u ? inverse(forward) : forward;
    } else if (node.kind == WordKind::Inverse) {
      assert(node.first < at);
      image = inverse(values[node.first]);
    } else {
      assert(node.kind == WordKind::Compose && node.first < at && node.second < at);
      for (size_t point = 0; point < size; ++point)
        image[point] = values[node.second][values[node.first][point]];
    }
  }
  return values[word.root];
}

static bool stopped(void *opaque) noexcept {
  return *static_cast<bool *>(opaque);
}

static std::vector<size_t> compose(const std::vector<size_t> &first,
                                   const std::vector<size_t> &second) {
  std::vector<size_t> result(first.size());
  for (size_t point = 0; point < first.size(); ++point)
    result[point] = second[first[point]];
  return result;
}

static std::string key(const std::vector<size_t> &rows,
                       const std::vector<size_t> &columns) {
  std::string result(reinterpret_cast<const char *>(rows.data()),
                     rows.size() * sizeof(size_t));
  result.append(reinterpret_cast<const char *>(columns.data()),
                columns.size() * sizeof(size_t));
  return result;
}

static bool brute_has_kernel_transposition(
    const std::vector<Generator> &generators) {
  Generator identity;
  identity.rows.resize(generators.front().rows.size());
  identity.columns.resize(generators.front().columns.size());
  for (size_t i = 0; i < identity.rows.size(); ++i)
    identity.rows[i] = i;
  for (size_t i = 0; i < identity.columns.size(); ++i)
    identity.columns[i] = i;
  std::deque<Generator> queue = {identity};
  std::set<std::string> seen = {key(identity.rows, identity.columns)};
  while (!queue.empty()) {
    const Generator current = queue.front();
    queue.pop_front();
    if (current.columns == identity.columns) {
      size_t moved = 0;
      for (size_t row = 0; row < current.rows.size(); ++row)
        moved += current.rows[row] != row;
      if (moved == 2)
        return true;
    }
    for (const Generator &generator : generators) {
      Generator next{compose(current.rows, generator.rows),
                     compose(current.columns, generator.columns)};
      if (seen.insert(key(next.rows, next.columns)).second)
        queue.push_back(std::move(next));
    }
  }
  return false;
}

static void word_evaluation_controls() {
  const std::vector<uint32_t> cycle = {1, 2, 0}, backward = {2, 0, 1};
  const std::vector<uint32_t> swap = {1, 0, 2};
  const std::vector<std::span<const uint32_t>> generators = {
      cycle, backward, swap, swap};
  GeneratorWord word{{{}, {WordKind::Generator, 0, 0},
                     {WordKind::Compose, 1, 1}, {WordKind::Inverse, 2, 0},
                     {WordKind::Compose, 2, 3}, {WordKind::Generator, 2, 0},
                     {WordKind::Compose, 4, 5}}, 6};
  const auto evaluated = evaluate_word(word, 3, generators, 10'000);
  assert(evaluated.status == Status::Found && evaluated.image == swap);
  auto limited = evaluate_word(word, 3, generators, evaluated.ticks - 1);
  assert(limited.status == Status::LimitReached && limited.image.empty());
  struct Polls { size_t count = 0, limit = SIZE_MAX; } polls;
  const auto stop = [](void *opaque) noexcept {
    auto &p = *static_cast<Polls *>(opaque);
    return ++p.count >= p.limit;
  };
  assert(evaluate_word(word, 3, generators, 10'000, stop, &polls).status == Status::Found);
  const size_t count = polls.count;
  for (size_t at : {size_t{1}, count / 2, count}) {
    polls = {0, at};
    const auto result = evaluate_word(word, 3, generators, 10'000, stop, &polls);
    assert(result.status == Status::Interrupted && result.image.empty());
  }
  // These two generators do not commute: order must mean first, then second.
  word = {{{}, {WordKind::Generator, 0, 0}, {WordKind::Generator, 2, 0},
           {WordKind::Compose, 1, 2}}, 3};
  assert(evaluate_word(word, 3, generators, 10'000).image ==
         std::vector<uint32_t>({0, 2, 1}));
  std::swap(word.nodes[3].first, word.nodes[3].second);
  assert(evaluate_word(word, 3, generators, 10'000).image ==
         std::vector<uint32_t>({2, 1, 0}));
  for (unsigned variant = 0; variant < 5; ++variant) {
    auto broken = word;
    if (variant == 0) broken.nodes[3].first = 3;
    if (variant == 1) broken.nodes[1].first = 4;
    if (variant == 2) broken.nodes[3].kind = static_cast<WordKind>(255);
    if (variant == 3) broken.root = 4;
    if (variant == 4) broken.nodes[3] = {WordKind::Inverse, 3, 0};
    const auto result = evaluate_word(broken, 3, generators, 10'000);
    assert(result.status == Status::Invalid && result.image.empty());
  }
  const std::vector<uint32_t> nonbijective = {0, 0, 2};
  auto invalid_generators = generators;
  invalid_generators[0] = nonbijective;
  assert(evaluate_word(word, 3, invalid_generators, 10'000).status == Status::Invalid);
  // Sixty squarings represent 2^60 applications without expanding them.
  word = {{{}, {WordKind::Generator, 0, 0}}, 1};
  for (unsigned square = 0; square < 60; ++square) {
    word.nodes.push_back({WordKind::Compose, word.root, word.root});
    ++word.root;
  }
  assert(evaluate_word(word, 3, generators, 10'000).image == cycle);
}

int main() {
  word_evaluation_controls();
  // A stabilizer generator list can omit a transposition that exists in the
  // group. The cyclic example needs a cube; the eight-point example needs
  // three different commuting generators and cannot be fixed by powers alone.
  const std::vector<std::vector<Generator>> hidden = {
      {{{1, 0, 3, 4, 2}, {0}}},
      {{{1, 0, 3, 2, 5, 4, 6, 7}, {0}},
       {{0, 1, 3, 2, 4, 5, 7, 6}, {0}},
       {{0, 1, 2, 3, 5, 4, 7, 6}, {0}}},
  };
  for (const auto &sample : hidden) {
    assert(brute_has_kernel_transposition(sample));
    const auto result = find_kernel_transposition(sample, 10'000, 1'000'000);
    assert(result.status == Status::Found && !result.word.empty());
    auto expected = sample.front().rows;
    std::iota(expected.begin(), expected.end(), 0);
    std::swap(expected[0], expected[1]);
    assert(lift(sample, result.word, true) == expected);
    assert(lift(sample, result.word, false) == std::vector<size_t>{0});
    assert(find_kernel_transposition(sample, result.states - 1, 1'000'000)
               .status == Status::LimitReached);
    const auto limited = find_kernel_transposition(sample, 10'000, result.ticks - 1);
    assert(limited.status == Status::LimitReached && limited.word.empty());
    struct Polls { size_t count = 0; size_t limit = SIZE_MAX; } polls;
    const auto callback = [](void *opaque) noexcept {
      auto &polls = *static_cast<Polls *>(opaque);
      return ++polls.count >= polls.limit;
    };
    assert(find_kernel_transposition(sample, 10'000, 1'000'000, callback, &polls)
               .status == Status::Found);
    const size_t completed_polls = polls.count;
    for (const size_t stop_at : {size_t{1}, completed_polls / 2, completed_polls}) {
      polls = {0, stop_at};
      const auto interrupted = find_kernel_transposition(
          sample, 10'000, 1'000'000, callback, &polls);
      assert(interrupted.status == Status::Interrupted && interrupted.word.empty());
    }
  }

  // A large symmetric column projection must not hide the row transposition
  // exposed by an odd-order column cycle. Full projection BFS exceeds this
  // bound; a certified power cancels the columns using a short exact word.
  for (const size_t columns : {size_t{9}, size_t{17}}) {
    Generator identity;
    identity.rows.resize(15);
    identity.columns.resize(columns);
    std::iota(identity.rows.begin(), identity.rows.end(), 0);
    std::iota(identity.columns.begin(), identity.columns.end(), 0);
    std::vector<Generator> product;
    for (size_t column = 0; column + 1 < columns; ++column) {
      auto adjacent = identity;
      std::swap(adjacent.columns[column], adjacent.columns[column + 1]);
      product.push_back(std::move(adjacent));
    }
    auto mixed = identity;
    std::swap(mixed.rows[3], mixed.rows[11]);
    std::rotate(mixed.columns.begin(), mixed.columns.begin() + 1,
                mixed.columns.end());
    product.push_back(std::move(mixed));
    const Result cancelled = find_kernel_transposition(product, 200, 100'000);
    assert(cancelled.status == Status::Found);
    assert(lift(product, cancelled.word, false) == identity.columns);
    auto expected = identity.rows;
    std::swap(expected[3], expected[11]);
    assert(lift(product, cancelled.word, true) == expected);
    assert(cancelled.states <= 2 && cancelled.word.size() < columns + 2);
  }

  // A coupled 15-row/9-column action has a large column projection but a
  // compact point stabilizer chain. Exhaustive projection traversal exceeds
  // this state budget. No supplied generator power exposes a pure swap.
  const std::vector<Generator> coupled_large = {
      {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 13},
       {0, 1, 2, 3, 4, 5, 6, 8, 7}},
      {{0, 1, 2, 10, 4, 5, 6, 7, 8, 9, 3, 11, 12, 14, 13},
       {0, 1, 5, 3, 4, 2, 6, 8, 7}},
      {{1, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
       {1, 0, 2, 3, 4, 5, 6, 7, 8}},
      {{1, 0, 5, 10, 8, 2, 7, 6, 4, 11, 3, 9, 12, 14, 13},
       {1, 0, 5, 6, 4, 2, 3, 8, 7}},
      {{14, 7, 2, 3, 5, 8, 6, 1, 10, 9, 13, 11, 0, 4, 12},
       {7, 4, 1, 2, 3, 6, 0, 5, 8}},
      {{14, 13, 11, 7, 8, 2, 12, 1, 3, 6, 5, 9, 4, 10, 0},
       {4, 6, 2, 8, 7, 1, 0, 3, 5}}
  };
  const auto large = find_kernel_transposition(coupled_large, 10'000, 100'000'000);
  assert(large.status == Status::Found && !large.word.empty());
  auto identity_rows = coupled_large.front().rows;
  auto identity_columns = coupled_large.front().columns;
  std::iota(identity_rows.begin(), identity_rows.end(), 0);
  std::iota(identity_columns.begin(), identity_columns.end(), 0);
  std::swap(identity_rows[large.first], identity_rows[large.second]);
  assert(lift(coupled_large, large.word, true) == identity_rows);
  assert(lift(coupled_large, large.word, false) == identity_columns);
  assert(find_kernel_transposition(coupled_large, large.states - 1, 100'000'000)
             .status == Status::LimitReached);
  const auto word_limited =
      find_kernel_transposition(coupled_large, 10'000, large.ticks - 1);
  assert(word_limited.status == Status::LimitReached && word_limited.word.empty());

  // A large odd column order has a logarithmic-size power certificate.
  Generator high_order{{1, 0}, {}};
  for (size_t length : {3, 5, 7, 11, 13, 17, 19, 23}) {
    const size_t begin = high_order.columns.size();
    for (size_t point = 0; point < length; ++point)
      high_order.columns.push_back(begin + (point + 1) % length);
  }
  const std::vector<Generator> power_group = {high_order};
  const auto compact_power = find_kernel_transposition(power_group, 10, 10'000);
  assert(compact_power.status == Status::Found && compact_power.word.size() < 100);
  assert(lift(power_group, compact_power.word, true) == std::vector<size_t>({1, 0}));
  auto expected_columns = high_order.columns;
  std::iota(expected_columns.begin(), expected_columns.end(), 0);
  assert(lift(power_group, compact_power.word, false) == expected_columns);

  // Every supplied generator flips the column pair, so no input generator is
  // in the column kernel. Their collisions yield a 4-cycle and a 3-cycle on
  // rows; only products of those kernel generators expose a transposition.
  const std::vector<Generator> generators = {
      {{0, 1, 2, 3}, {1, 0}},
      {{1, 2, 3, 0}, {1, 0}},
      {{1, 2, 0, 3}, {1, 0}},
  };
  const Result found =
      find_kernel_transposition(generators, 10'000, 1'000'000);
  assert(found.status == Status::Found && !found.word.empty());
  const std::vector<size_t> row_image = lift(generators, found.word, true);
  const std::vector<size_t> column_image = lift(generators, found.word, false);
  assert(column_image == std::vector<size_t>({0, 1}));
  size_t moved = 0;
  for (size_t row = 0; row < row_image.size(); ++row)
    moved += row_image[row] != row;
  assert(moved == 2 && row_image[found.first] == found.second &&
         row_image[found.second] == found.first);

  const Result replay =
      find_kernel_transposition(generators, 10'000, 1'000'000);
  assert(replay.status == Status::Found && replay.word == found.word &&
         replay.first == found.first && replay.second == found.second &&
         replay.ticks == found.ticks && replay.states == found.states);

  assert(find_kernel_transposition(generators, 0, 1'000'000).status ==
         Status::LimitReached);
  assert(find_kernel_transposition(generators, 10'000, 0).status ==
         Status::LimitReached);
  assert(find_kernel_transposition(generators, 1, 1'000'000).status ==
         Status::LimitReached);
  bool stop = true;
  assert(find_kernel_transposition(generators, 10'000, 1'000'000, stopped,
                                   &stop)
             .status == Status::Interrupted);

  const std::vector<Generator> faithful = {
      {{1, 2, 0}, {1, 2, 0}}, {{1, 0, 2}, {1, 0, 2}}};
  assert(find_kernel_transposition(faithful, 10'000, 1'000'000).status ==
         Status::Absent);

  // Differentially compare the bounded Schreier implementation against full
  // closure on small random product actions. This includes bases whose useful
  // kernel elements are products rather than supplied generators.
  std::mt19937 random(0x71c715u);
  for (unsigned trial = 0; trial < 300; ++trial) {
    const size_t rows = 2 + random() % 5;
    const size_t columns = 1 + random() % 3;
    const size_t count = 1 + random() % 4;
    std::vector<Generator> sample(count);
    for (Generator &generator : sample) {
      generator.rows.resize(rows);
      generator.columns.resize(columns);
      for (size_t i = 0; i < rows; ++i)
        generator.rows[i] = i;
      for (size_t i = 0; i < columns; ++i)
        generator.columns[i] = i;
      std::shuffle(generator.rows.begin(), generator.rows.end(), random);
      std::shuffle(generator.columns.begin(), generator.columns.end(), random);
    }
    const bool expected = brute_has_kernel_transposition(sample);
    const Result actual =
        find_kernel_transposition(sample, 1'000'000, 100'000'000);
    assert((actual.status == Status::Found) == expected);
    assert(actual.status == Status::Found || actual.status == Status::Absent);
    if (actual.status == Status::Found) {
      const auto actual_rows = lift(sample, actual.word, true);
      const auto actual_columns = lift(sample, actual.word, false);
      size_t moved = 0;
      for (size_t row = 0; row < rows; ++row)
        moved += actual_rows[row] != row;
      for (size_t column = 0; column < columns; ++column)
        assert(actual_columns[column] == column);
      assert(moved == 2 && actual_rows[actual.first] == actual.second &&
             actual_rows[actual.second] == actual.first);
    }
  }

  const std::vector<Generator> invalid = {{{0, 0}, {0}}};
  assert(find_kernel_transposition(invalid, 10, 100).status == Status::Invalid);
  std::puts("Orbitopal product-kernel traversal PASS");
}
