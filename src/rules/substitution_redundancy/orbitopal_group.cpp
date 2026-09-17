#include "rules/substitution_redundancy/orbitopal_group.hpp"

#include <algorithm>
#include <limits>
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace tactix::orbitopal_group {
namespace {

struct WordGenerator {
  std::vector<size_t> image;
  std::vector<size_t> inverse;
  uint32_t word = 0;
};

std::string key(const std::vector<size_t> &image) {
  return std::string(reinterpret_cast<const char *>(image.data()),
                     image.size() * sizeof(size_t));
}

bool permutation(const std::vector<size_t> &image) {
  std::vector<uint8_t> seen(image.size(), 0);
  for (const size_t target : image) {
    if (target >= image.size() || seen[target])
      return false;
    seen[target] = 1;
  }
  return true;
}

bool transposition(const std::vector<size_t> &image, size_t &first,
                   size_t &second) {
  first = second = SIZE_MAX;
  for (size_t source = 0; source < image.size(); ++source) {
    if (image[source] == source)
      continue;
    if (first == SIZE_MAX)
      first = source;
    else if (second == SIZE_MAX)
      second = source;
    else
      return false;
  }
  return first != SIZE_MAX && second != SIZE_MAX && image[first] == second &&
         image[second] == first;
}

class Search {
public:
  Search(const std::vector<Generator> &generators, uint64_t maximum_states,
         uint64_t maximum_ticks, StopCallback stop, void *stop_context)
      : input_(generators), maximum_states_(maximum_states),
        maximum_ticks_(maximum_ticks), stop_(stop),
        stop_context_(stop_context) {}

  Result run() {
    if (input_.empty() || input_.size() > UINT32_MAX / 2)
      return finish(Status::Invalid);
    rows_ = input_.front().rows.size();
    columns_ = input_.front().columns.size();
    if (!rows_ || !columns_ || !maximum_states_)
      return finish(maximum_states_ ? Status::Invalid : Status::LimitReached);
    for (const Generator &generator : input_)
      if (generator.rows.size() != rows_ ||
          generator.columns.size() != columns_ ||
          !permutation(generator.rows) || !permutation(generator.columns))
        return finish(Status::Invalid);
    if (stopped())
      return finish(Status::Interrupted);

    ++states_used_;
    generator_words_.assign(2 * input_.size(), UINT32_MAX);

    // The order of a generator's column action kills that projection. Its
    // corresponding power can expose a useful row kernel element without
    // enumerating the (potentially factorial) column image group. Keep the
    // joint-action stabilizer chain for cases this does not settle.
    for (size_t generator = 0; generator < input_.size(); ++generator) {
      if (!charge(1 + rows_ + columns_))
        return finish(limit_status_);
      const auto &action = input_[generator];
      uint64_t order = 1;
      std::vector<uint8_t> seen(columns_, 0);
      bool representable = true;
      for (size_t start = 0; start < columns_; ++start) {
        if (seen[start])
          continue;
        uint64_t length = 0;
        for (size_t point = start; !seen[point]; point = action.columns[point]) {
          seen[point] = 1;
          ++length;
        }
        const uint64_t factor = length / std::gcd(order, length);
        if (order > std::numeric_limits<uint64_t>::max() / factor) {
          representable = false;
          break;
        }
        order *= factor;
      }
      if (!representable)
        continue;
      std::vector<size_t> powered(rows_);
      seen.assign(rows_, 0);
      for (size_t start = 0; start < rows_; ++start) {
        if (seen[start])
          continue;
        std::vector<size_t> cycle;
        for (size_t point = start; !seen[point]; point = action.rows[point]) {
          seen[point] = 1;
          cycle.push_back(point);
        }
        const size_t shift = order % cycle.size();
        for (size_t at = 0; at < cycle.size(); ++at)
          powered[cycle[at]] = cycle[(at + shift) % cycle.size()];
      }
      if (!transposition(powered, result_.first, result_.second))
        continue;
      if (!admit_state())
        return finish(Status::LimitReached);
      uint32_t factor = generator_word(static_cast<uint32_t>(2 * generator));
      uint32_t power = 0;
      for (uint64_t exponent = order; exponent; exponent >>= 1) {
        if (factor == UINT32_MAX ||
            ((exponent & 1u) && !append_word(power, factor)))
          return finish(limit_status_);
        if (exponent > 1 && !append_word(factor, factor))
          return finish(limit_status_);
      }
      selected_word_ = power;
      return finish(Status::Found);
    }

    if (rows_ > SIZE_MAX - columns_)
      return finish(Status::Invalid);
    const size_t degree = columns_ + rows_;
    // Fix columns pointwise before sifting row transpositions. This computes
    // the same projection kernel without enumerating the entire column group.
    std::vector<WordGenerator> combined;
    for (size_t ordinal = 0; ordinal < input_.size(); ++ordinal) {
      if (!charge(1 + degree))
        return finish(limit_status_);
      WordGenerator generator;
      generator.image.resize(degree);
      generator.inverse.resize(degree);
      for (size_t point = 0; point < columns_; ++point)
        generator.image[point] = input_[ordinal].columns[point];
      for (size_t point = 0; point < rows_; ++point)
        generator.image[columns_ + point] = columns_ + input_[ordinal].rows[point];
      for (size_t point = 0; point < degree; ++point)
        generator.inverse[generator.image[point]] = point;
      generator.word = generator_word(static_cast<uint32_t>(2 * ordinal));
      if (generator.word == UINT32_MAX)
        return finish(limit_status_);
      combined.push_back(std::move(generator));
    }
    const int found = stabilize(combined, degree, columns_);
    return finish(found < 0 ? limit_status_
                           : found ? Status::Found : Status::Absent);
  }

private:

  bool stopped() const { return stop_ && stop_(stop_context_); }

  bool charge(uint64_t amount) {
    if (stopped()) {
      limit_status_ = Status::Interrupted;
      return false;
    }
    if (ticks_ >= maximum_ticks_ || amount > maximum_ticks_ - ticks_) {
      ticks_ = maximum_ticks_;
      limit_status_ = Status::LimitReached;
      return false;
    }
    ticks_ += amount;
    return true;
  }

  bool admit_state() {
    if (states_used_ >= maximum_states_)
      return false;
    ++states_used_;
    return true;
  }

  uint32_t add_word(WordNode node) {
    if (!charge(1))
      return UINT32_MAX;
    if (words_.size() >= UINT32_MAX) {
      limit_status_ = Status::LimitReached;
      return UINT32_MAX;
    }
    const uint32_t result = static_cast<uint32_t>(words_.size());
    words_.push_back(node);
    word_inverses_.push_back(UINT32_MAX);
    return result;
  }

  uint32_t generator_word(uint32_t encoded) {
    if (!charge(1))
      return UINT32_MAX;
    uint32_t &result = generator_words_[encoded];
    if (result == UINT32_MAX) {
      result = add_word({WordKind::Generator, encoded, 0});
      const uint32_t inverse = generator_words_[encoded ^ 1u];
      if (result != UINT32_MAX && inverse != UINT32_MAX) {
        word_inverses_[result] = inverse;
        word_inverses_[inverse] = result;
      }
    }
    return result;
  }

  uint32_t inverse_word(uint32_t word) {
    if (!charge(1))
      return UINT32_MAX;
    if (word_inverses_[word] != UINT32_MAX)
      return word_inverses_[word];
    const WordNode node = words_[word];
    const uint32_t inverse = node.kind == WordKind::Generator
        ? generator_word(node.first ^ 1u)
        : add_word({WordKind::Inverse, word, 0});
    if (inverse != UINT32_MAX) {
      word_inverses_[word] = inverse;
      word_inverses_[inverse] = word;
    }
    return inverse;
  }

  bool append_word(uint32_t &left, uint32_t right) {
    if (!charge(1))
      return false;
    if (!left || !right) {
      left = left ? left : right;
      return true;
    }
    if (word_inverses_[left] == right) {
      left = 0;
      return true;
    }
    const uint64_t key = (static_cast<uint64_t>(left) << 32) | right;
    const auto found = word_compositions_.find(key);
    if (found != word_compositions_.end()) {
      left = found->second;
      return true;
    }
    const uint32_t result = add_word({WordKind::Compose, left, right});
    if (result == UINT32_MAX)
      return false;
    word_compositions_.emplace(key, result);
    left = result;
    return true;
  }

  bool export_word() {
    if (!charge(words_.size()))
      return false;
    std::vector<uint8_t> used(words_.size(), 0);
    std::vector<uint32_t> pending = {selected_word_};
    while (!pending.empty()) {
      if (!charge(1))
        return false;
      const uint32_t at = pending.back();
      pending.pop_back();
      if (used[at])
        continue;
      used[at] = 1;
      const auto &node = words_[at];
      if (node.kind == WordKind::Compose) {
        pending.push_back(node.first);
        pending.push_back(node.second);
      } else if (node.kind == WordKind::Inverse) {
        pending.push_back(node.first);
      }
    }
    GeneratorWord word;
    word.nodes.push_back({});
    std::vector<uint32_t> mapped(words_.size(), UINT32_MAX);
    mapped[0] = 0;
    for (size_t at = 1; at < words_.size(); ++at) {
      if (!charge(1))
        return false;
      if (!used[at])
        continue;
      WordNode node = words_[at];
      if (node.kind == WordKind::Compose) {
        node.first = mapped[node.first];
        node.second = mapped[node.second];
      } else if (node.kind == WordKind::Inverse) {
        node.first = mapped[node.first];
      }
      mapped[at] = static_cast<uint32_t>(word.nodes.size());
      word.nodes.push_back(node);
    }
    word.root = mapped[selected_word_];
    result_.word = std::move(word);
    return true;
  }

  int stabilize(const std::vector<WordGenerator> &input, size_t degree,
                size_t fixed_prefix) {
    struct OrbitState {
      std::vector<size_t> image;
      std::vector<size_t> inverse;
      uint32_t word = 0;
    };
    struct ChainLevel {
      std::vector<OrbitState> orbit;
      std::vector<size_t> representative;
    };
    std::vector<ChainLevel> chain;
    std::vector<WordGenerator> generators = input;
    for (size_t level = 0; level < degree; ++level) {
      for (const WordGenerator &generator : generators)
        if (transposition(generator.image, result_.first, result_.second) &&
            result_.first >= fixed_prefix) {
          result_.first -= fixed_prefix;
          result_.second -= fixed_prefix;
          selected_word_ = generator.word;
          return 1;
        }
      if (generators.empty())
        break;
      OrbitState identity;
      identity.image.resize(degree);
      identity.inverse.resize(degree);
      for (size_t row = 0; row < degree; ++row)
        identity.image[row] = identity.inverse[row] = row;
      std::vector<OrbitState> orbit = {std::move(identity)};
      std::vector<size_t> representative(degree, SIZE_MAX);
      representative[level] = 0;
      for (size_t at = 0; at < orbit.size(); ++at)
        for (const WordGenerator &generator : generators)
          for (uint32_t inverse = 0; inverse < 2; ++inverse) {
            if (!charge(1 + degree))
              return -1;
            const auto &step = inverse ? generator.inverse : generator.image;
            const size_t target = step[orbit[at].image[level]];
            if (representative[target] != SIZE_MAX)
              continue;
            if (!admit_state()) {
              limit_status_ = Status::LimitReached;
              return -1;
            }
            OrbitState next;
            next.image.resize(degree);
            next.inverse.resize(degree);
            for (size_t row = 0; row < degree; ++row)
              next.image[row] = step[orbit[at].image[row]];
            for (size_t row = 0; row < degree; ++row)
              next.inverse[next.image[row]] = row;
            next.word = orbit[at].word;
            const uint32_t step_word =
                inverse ? inverse_word(generator.word) : generator.word;
            if (step_word == UINT32_MAX || !append_word(next.word, step_word))
              return -1;
            representative[target] = orbit.size();
            orbit.push_back(std::move(next));
          }
      if (orbit.size() == 1) {
        // A fixed point needs no stored identity permutation or index table.
        chain.push_back({});
        continue;
      }

      std::vector<WordGenerator> next_generators;
      std::vector<std::vector<size_t>> pivots(degree);
      std::unordered_set<std::string> next_actions;
      std::vector<size_t> next_image(degree), next_inverse(degree);
      for (const OrbitState &source : orbit)
        for (const WordGenerator &generator : generators)
          for (uint32_t inverse = 0; inverse < 2; ++inverse) {
            if (!charge(1 + degree))
              return -1;
            const auto &step = inverse ? generator.inverse : generator.image;
            const size_t target = step[source.image[level]];
            const OrbitState &target_state = orbit[representative[target]];
            bool identity_action = true;
            for (size_t row = 0; row < degree; ++row) {
              next_image[row] =
                  target_state.inverse[step[source.image[row]]];
              identity_action &= next_image[row] == row;
            }
            const std::string row_key = key(next_image);
            if (identity_action || next_actions.count(row_key))
              continue;
            uint32_t next_word = source.word;
            const uint32_t step_word =
                inverse ? inverse_word(generator.word) : generator.word;
            const uint32_t target_inverse_word = inverse_word(target_state.word);
            if (step_word == UINT32_MAX || target_inverse_word == UINT32_MAX ||
                !append_word(next_word, step_word) ||
                !append_word(next_word, target_inverse_word))
              return -1;
            if (!admit_state()) {
              limit_status_ = Status::LimitReached;
              return -1;
            }
            next_actions.insert(row_key);
            // A triangular Sims filter removes redundant Schreier generators.
            // Every discarded residue is explicitly reduced to identity by
            // retained generators; retaining each new pivot preserves the
            // generated subgroup without enumerating its elements.
            for (size_t point = 0; point < degree; ++point) {
              if (next_image[point] == point)
                continue;
              if (!charge(1 + degree))
                return -1;
              if (pivots[point].empty())
                pivots[point].assign(degree, SIZE_MAX);
              size_t &pivot = pivots[point][next_image[point]];
              if (pivot == SIZE_MAX) {
                pivot = next_generators.size();
                for (size_t row = 0; row < degree; ++row)
                  next_inverse[next_image[row]] = row;
                next_generators.push_back(
                    {next_image, next_inverse, std::move(next_word)});
                break;
              }
              const auto &basis = next_generators[pivot];
              for (size_t &image : next_image)
                image = basis.inverse[image];
              const uint32_t inverse = inverse_word(basis.word);
              if (inverse == UINT32_MAX || !append_word(next_word, inverse))
                return -1;
            }
          }
      generators = std::move(next_generators);
      chain.push_back({std::move(orbit), std::move(representative)});
    }

    // A transposition need not occur in a stabilizer's generator list. Once
    // its first moved point is fixed, the remaining stabilizers lose it.
    // Sift every transposition through the completed chain instead: at each
    // level remove the transversal carrying that base point to its image.
    // Identity residue proves membership and the reversed transversal words
    // recover an exact word in the original generators, fixing every column.
    std::vector<size_t> residue(degree);
    for (size_t first = fixed_prefix; first < degree; ++first)
      for (size_t second = first + 1; second < degree; ++second) {
        if (!charge(1 + degree))
          return -1;
        if (!admit_state()) {
          limit_status_ = Status::LimitReached;
          return -1;
        }
        std::iota(residue.begin(), residue.end(), 0);
        std::swap(residue[first], residue[second]);
        std::vector<uint32_t> factors;
        bool member = true;
        for (size_t level = 0; level < chain.size(); ++level) {
          if (!charge(1))
            return -1;
          const auto &data = chain[level];
          if (data.orbit.empty()) {
            if (residue[level] != level) {
              member = false;
              break;
            }
            continue;
          }
          if (!charge(degree))
            return -1;
          const size_t representative = data.representative[residue[level]];
          if (representative == SIZE_MAX) {
            member = false;
            break;
          }
          const auto &transversal = data.orbit[representative];
          for (size_t &image : residue)
            image = transversal.inverse[image];
          factors.push_back(transversal.word);
        }
        for (size_t point = 0; point < degree && member; ++point)
          member = residue[point] == point;
        if (!member)
          continue;
        uint32_t word = 0;
        for (auto factor = factors.rbegin(); factor != factors.rend(); ++factor)
          if (!append_word(word, *factor))
            return -1;
        selected_word_ = word;
        result_.first = first - fixed_prefix;
        result_.second = second - fixed_prefix;
        return 1;
      }
    return 0;
  }

  Result finish(Status status) {
    if (status == Status::Found && !export_word())
      status = limit_status_;
    if (status != Status::Found)
      result_.first = result_.second = SIZE_MAX;
    result_.status = status;
    result_.ticks = ticks_;
    result_.states = states_used_;
    return std::move(result_);
  }

  const std::vector<Generator> &input_;
  uint64_t maximum_states_, maximum_ticks_;
  StopCallback stop_;
  void *stop_context_;
  size_t rows_ = 0, columns_ = 0;
  uint64_t ticks_ = 0, states_used_ = 0;
  Status limit_status_ = Status::LimitReached;
  Result result_;
  uint32_t selected_word_ = 0;
  std::vector<WordNode> words_ = {{}};
  std::vector<uint32_t> word_inverses_ = {0};
  std::vector<uint32_t> generator_words_;
  std::unordered_map<uint64_t, uint32_t> word_compositions_;

};

} // namespace

Result find_kernel_transposition(const std::vector<Generator> &generators,
                                 uint64_t maximum_states,
                                 uint64_t maximum_ticks, StopCallback stop,
                                 void *stop_context) {
  return Search(generators, maximum_states, maximum_ticks, stop,
                stop_context)
      .run();
}

} // namespace tactix::orbitopal_group
