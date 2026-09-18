#include "rules/lucky/simon_model.hpp"

#include "rules/lucky/model.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace tactix {
namespace {

struct Simon {
  uint32_t rounds = 0;
  std::array<uint32_t, 32> second_to_primary{};
  std::vector<uint8_t> key_half;
  uint32_t initial_left = 0, initial_right = 0;
  uint32_t target_left = 0, target_right = 0;
};

uint32_t rotate_left(uint32_t value, uint32_t shift) {
  return (value << shift) | (value >> (32 - shift));
}

uint32_t simon_function(uint32_t value) {
  return (rotate_left(value, 1) & rotate_left(value, 8)) ^
         rotate_left(value, 2);
}

bool map_external(const Kernel &k, uint64_t external, Var &internal) {
  if (!external || external > k.external_to_internal.size())
    return false;
  internal = k.external_to_internal[external - 1];
  return internal != INVALID_VAR && internal < k.nvars;
}

bool external_value(const Kernel &k, uint64_t external, uint8_t &value) {
  Var internal = INVALID_VAR;
  if (!map_external(k, external, internal))
    return false;
  const int current = k.value(make_lit(internal, false));
  if (!current)
    return false;
  value = current > 0;
  return true;
}

bool fixed_word(const Kernel &k, uint64_t first, uint32_t &word) {
  word = 0;
  for (uint32_t bit = 0; bit < 32; ++bit) {
    uint8_t value = 0;
    if (!external_value(k, first + bit, value))
      return false;
    word |= uint32_t{value} << bit;
  }
  return true;
}

bool cheap_shape(const Kernel &k,
                 const strategy::SimonModelParameters &parameters,
                 Simon &simon) {
  const uint64_t variables = k.external_to_internal.size();
  if (variables != k.nvars || variables < 288 || (variables - 128) % 160)
    return false;
  const uint64_t rounds = (variables - 128) / 160;
  if (!rounds || rounds > parameters.maximum_rounds || rounds > UINT32_MAX)
    return false;
  const uint64_t clauses = k.input_unit_clause_count +
                           k.input_nonunit_clause_count;
  if (clauses != 544 * rounds + 192 ||
      k.input_unit_clause_count != 128 ||
      k.input_binary_clause_count != 128 * rounds + 64 ||
      k.input_ternary_clause_count != 416 * rounds ||
      k.input_nonunit_clause_count !=
          k.input_binary_clause_count + k.input_ternary_clause_count)
    return false;
  simon.rounds = static_cast<uint32_t>(rounds);
  simon.second_to_primary.fill(UINT32_MAX);
  simon.key_half.assign(simon.rounds, UINT8_MAX);
  return true;
}

bool recover_permutation(Kernel &k, Simon &simon) {
  std::array<uint8_t, 32> seen{};
  uint64_t visited = 0;
  for (Lit first = 0; first < 2 * k.nvars; ++first)
    for (const WatchEntry watch : k.watches.entries(first)) {
      if (!watch.binary() || first > watch.other())
        continue;
      const int a = kernel_externalize(k, first);
      const int b = kernel_externalize(k, watch.other());
      const auto consider = [&](int high_lit, int low_lit) {
        const uint32_t high = static_cast<uint32_t>(
            high_lit < 0 ? -int64_t(high_lit) : int64_t(high_lit));
        const uint32_t low = static_cast<uint32_t>(
            low_lit < 0 ? -int64_t(low_lit) : int64_t(low_lit));
        if (high < 33 || high > 64 || !low || low > 32 ||
            (high_lit > 0) == (low_lit > 0))
          return true;
        const uint32_t slot = high - 33, primary = low - 1;
        if (simon.second_to_primary[slot] != UINT32_MAX &&
            simon.second_to_primary[slot] != primary)
          return false;
        simon.second_to_primary[slot] = primary;
        return true;
      };
      if (!consider(a, b) || !consider(b, a))
        return false;
      if (!(++visited & ((uint64_t{1} << 18) - 1)) &&
          kernel_rule_interrupt(k, k.stats.simon_model_interrupted))
        return false;
    }
  for (uint32_t slot = 0; slot < 32; ++slot) {
    const uint32_t primary = simon.second_to_primary[slot];
    if (primary >= 32 || seen[primary])
      return false;
    seen[primary] = 1;
  }
  return true;
}

bool recover_round_keys(Kernel &k, Simon &simon) {
  uint64_t visited = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage() && clause->size == 3) {
      std::array<uint32_t, 3> external{};
      for (uint32_t i = 0; i < 3; ++i) {
        const int literal = kernel_externalize(k, clause->lits[i]);
        external[i] = static_cast<uint32_t>(
            literal < 0 ? -int64_t(literal) : int64_t(literal));
      }
      for (uint32_t round = 0; round < simon.rounds; ++round) {
        const uint32_t base = 160 * round;
        bool output = false, previous = false;
        uint32_t key = 0;
        for (const uint32_t variable : external) {
          output |= variable == base + 225;
          previous |= variable == base + 193;
          if (variable >= 1 && variable <= 64)
            key = variable;
        }
        if (!output || !previous || !key)
          continue;
        const uint8_t half = key <= 32 ? 0 : 1;
        if (simon.key_half[round] != UINT8_MAX &&
            simon.key_half[round] != half)
          return false;
        simon.key_half[round] = half;
      }
    }
    ref += words;
    if (!(++visited & ((uint64_t{1} << 18) - 1)) &&
        kernel_rule_interrupt(k, k.stats.simon_model_interrupted))
      return false;
  }
  return std::none_of(simon.key_half.begin(), simon.key_half.end(),
                      [](uint8_t half) { return half == UINT8_MAX; });
}

uint32_t permute_key(uint32_t key, const Simon &simon) {
  uint32_t result = 0;
  for (uint32_t bit = 0; bit < 32; ++bit)
    result |= ((key >> simon.second_to_primary[bit]) & 1) << bit;
  return result;
}

bool matches(uint32_t key, const Simon &simon) {
  const uint32_t second = permute_key(key, simon);
  uint32_t left = simon.initial_left, right = simon.initial_right;
  for (uint32_t round = 0; round < simon.rounds; ++round) {
    const uint32_t round_key = simon.key_half[round] ? second : key;
    const uint32_t next = simon_function(left) ^ right ^ round_key;
    right = left;
    left = next;
  }
  return left == simon.target_left && right == simon.target_right;
}

enum class SearchResult { Exhausted, Found, Interrupted };

SearchResult find_key(Kernel &k, const Simon &simon, uint64_t maximum,
                      uint32_t &result) {
  const uint64_t limit = std::min<uint64_t>(maximum, uint64_t{1} << 32);
  uint64_t candidates = 0;
  for (uint64_t candidate = 0; candidate < limit; ++candidate) {
    if (!(candidate & ((uint64_t{1} << 20) - 1))) {
      k.stats.simon_model_candidates += candidates;
      candidates = 0;
      if (kernel_rule_interrupt(k, k.stats.simon_model_interrupted))
        return SearchResult::Interrupted;
    }
    ++candidates;
    if (matches(static_cast<uint32_t>(candidate), simon)) {
      k.stats.simon_model_candidates += candidates;
      result = static_cast<uint32_t>(candidate);
      return SearchResult::Found;
    }
  }
  k.stats.simon_model_candidates += candidates;
  return SearchResult::Exhausted;
}

bool set_external(const Kernel &k, std::vector<uint8_t> &assignment,
                  uint64_t external, bool value) {
  Var internal = INVALID_VAR;
  if (!map_external(k, external, internal))
    return false;
  assignment[internal] = value;
  return true;
}

bool set_word(const Kernel &k, std::vector<uint8_t> &assignment,
              uint64_t first, uint32_t word) {
  for (uint32_t bit = 0; bit < 32; ++bit)
    if (!set_external(k, assignment, first + bit, (word >> bit) & 1))
      return false;
  return true;
}

bool build_assignment(Kernel &k, const Simon &simon, uint32_t key,
                      std::vector<uint8_t> &assignment) {
  assignment.assign(k.nvars, 0);
  const uint32_t second = permute_key(key, simon);
  if (!set_word(k, assignment, 1, key) ||
      !set_word(k, assignment, 33, second) ||
      !set_word(k, assignment, 65, simon.initial_left) ||
      !set_word(k, assignment, 97, simon.initial_right))
    return false;
  uint32_t left = simon.initial_left, right = simon.initial_right;
  for (uint32_t round = 0; round < simon.rounds; ++round) {
    const uint64_t base = uint64_t{160} * round;
    const uint32_t round_key = simon.key_half[round] ? second : key;
    uint32_t and_word = 0, first_xor = 0, second_xor = 0, output = 0;
    for (uint32_t bit = 0; bit < 32; ++bit) {
      const bool a = (left >> ((bit + 31) & 31)) & 1;
      const bool b = (left >> ((bit + 24) & 31)) & 1;
      const bool conjunction = a && b;
      const bool x1 = ((left >> ((bit + 30) & 31)) & 1) ^ conjunction;
      const bool x2 = ((right >> bit) & 1) ^ x1;
      const bool out = x2 ^ ((round_key >> bit) & 1);
      and_word |= uint32_t{conjunction} << bit;
      first_xor |= uint32_t{x1} << bit;
      second_xor |= uint32_t{x2} << bit;
      output |= uint32_t{out} << bit;
    }
    if (!set_word(k, assignment, base + 129, and_word) ||
        !set_word(k, assignment, base + 161, first_xor) ||
        !set_word(k, assignment, base + 193, second_xor) ||
        !set_word(k, assignment, base + 225, output) ||
        !set_word(k, assignment, base + 257, left))
      return false;
    right = left;
    left = output;
  }
  return left == simon.target_left && right == simon.target_right;
}

} // namespace

RuleEligibility simon_model_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("no-conflict", !k.conflict_valid)
      .fact("no-pending", !k.pending.valid)
      .fact("root-level", k.level == 0)
      .fact("propagated", k.propagate_head == k.trail.size())
      .fact("sparse-mode", !k.dense)
      .fact("no-probe-transaction", !k.probing.recording)
      .fact("large-watches-connected", k.large_watches_connected);
}

ApplyResult simon_model_apply(Kernel &k) {
  assert(simon_model_guard(k));
  ++k.stats.simon_model_calls;
  const strategy::SimonModelParameters parameters = k.simon_model_parameters;
  if (!parameters.valid())
    return ApplyResult::NoOp;
  Simon simon;
  if (!cheap_shape(k, parameters, simon) ||
      !recover_permutation(k, simon) || !recover_round_keys(k, simon) ||
      !fixed_word(k, 65, simon.initial_left) ||
      !fixed_word(k, 97, simon.initial_right) ||
      !fixed_word(k, k.external_to_internal.size() - 63,
                  simon.target_left) ||
      !fixed_word(k, k.external_to_internal.size() - 31,
                  simon.target_right))
    return ApplyResult::NoOp;
  ++k.stats.simon_model_recognized;
  k.stats.simon_model_rounds = simon.rounds;
  uint32_t key = 0;
  if (find_key(k, simon, parameters.maximum_candidates, key) !=
      SearchResult::Found)
    return ApplyResult::NoOp;
  std::vector<uint8_t> assignment;
  if (!build_assignment(k, simon, key, assignment) ||
      !lucky_model_valid(k, assignment))
    return ApplyResult::NoOp;
  lucky_publish_model(k, assignment);
  ++k.stats.simon_model_sat;
  return ApplyResult::Sat;
}

} // namespace tactix
