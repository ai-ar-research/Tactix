#include "rules/lucky/cerny_model.hpp"

#include "rules/lucky/model.hpp"
#include "rules/lucky/root_guard.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace tactix {
namespace {

struct Automaton {
  uint32_t states = 0;
  uint32_t length = 0;
  uint64_t state_variables = 0;
  std::vector<std::array<uint32_t, 2>> transition;
};

bool cheap_shape(const Kernel &k, Automaton &automaton) {
  const uint64_t length = k.input_positive_binary_count;
  const uint64_t variables = k.external_to_internal.size();
  if (!length || variables % length)
    return false;
  const uint64_t quotient = variables / length;
  if (quotient <= 3 || quotient - 2 > UINT32_MAX || length > UINT32_MAX)
    return false;
  const uint64_t states = quotient - 2;
  if (length > 1 && states > UINT64_MAX / 2 / (length - 1))
    return false;
  const uint64_t minimum_ternaries = 2 * states * (length - 1);
  if (k.input_ternary_clause_count < minimum_ternaries)
    return false;
  automaton.states = static_cast<uint32_t>(states);
  automaton.length = static_cast<uint32_t>(length);
  automaton.state_variables = states * length;
  automaton.transition.assign(automaton.states,
                              std::array<uint32_t, 2>{UINT32_MAX, UINT32_MAX});
  return automaton.state_variables + 2 * length == variables;
}

bool recover_transitions(Kernel &k, Automaton &automaton) {
  uint64_t visited = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage() && clause->size == 3) {
      uint32_t source_variable = 0, destination_variable = 0;
      uint32_t selector_variable = 0;
      bool ambiguous = false;
      for (const Lit literal : *clause) {
        const int external = kernel_externalize(k, literal);
        const uint32_t variable = static_cast<uint32_t>(
            external < 0 ? -int64_t(external) : int64_t(external));
        if (external < 0 && variable <= automaton.state_variables) {
          ambiguous |= source_variable != 0;
          source_variable = variable;
        } else if (external > 0 && variable <= automaton.state_variables) {
          ambiguous |= destination_variable != 0;
          destination_variable = variable;
        } else if (external < 0 && variable > automaton.state_variables) {
          ambiguous |= selector_variable != 0;
          selector_variable = variable;
        }
      }
      if (!ambiguous && source_variable && destination_variable &&
          selector_variable) {
        const uint32_t source_layer =
            (source_variable - 1) / automaton.states;
        const uint32_t destination_layer =
            (destination_variable - 1) / automaton.states;
        const uint32_t source_state =
            (source_variable - 1) % automaton.states;
        const uint32_t destination_state =
            (destination_variable - 1) % automaton.states;
        const uint64_t selector_index =
            selector_variable - automaton.state_variables - 1;
        const uint32_t selector_layer =
            static_cast<uint32_t>(selector_index / 2);
        const uint32_t letter = static_cast<uint32_t>(selector_index % 2);
        if (source_layer + 1 != destination_layer ||
            selector_layer != destination_layer ||
            destination_layer >= automaton.length)
          return false;
        uint32_t &known = automaton.transition[source_state][letter];
        if (known == UINT32_MAX)
          known = destination_state;
        else if (known != destination_state)
          return false;
      }
    }
    ref += words;
    if (!(++visited & ((uint64_t{1} << 18) - 1)) &&
        kernel_rule_interrupt(k, k.stats.cerny_model_interrupted))
      return false;
  }
  for (const auto &row : automaton.transition)
    if (row[0] == UINT32_MAX || row[1] == UINT32_MAX)
      return false;
  return true;
}

bool validate_initial_image(Kernel &k, const Automaton &automaton) {
  std::vector<std::array<uint8_t, 2>> present(
      automaton.states, std::array<uint8_t, 2>{0, 0});
  uint64_t visited = 0;
  for (Lit literal = 0; literal < 2 * k.nvars; ++literal)
    for (const WatchEntry watch : k.watches.entries(literal)) {
      if (!watch.binary() || literal > watch.other())
        continue;
      const int first = kernel_externalize(k, literal);
      const int second = kernel_externalize(k, watch.other());
      for (const auto pair : {std::array<int, 2>{first, second},
                              std::array<int, 2>{second, first}}) {
        if (pair[0] <= 0 ||
            pair[0] > static_cast<int>(automaton.states) || pair[1] >= 0)
          continue;
        const uint32_t selector = static_cast<uint32_t>(-int64_t(pair[1]));
        if (selector == automaton.state_variables + 1)
          present[pair[0] - 1][0] = 1;
        else if (selector == automaton.state_variables + 2)
          present[pair[0] - 1][1] = 1;
      }
      if (!(++visited & ((uint64_t{1} << 18) - 1)) &&
          kernel_rule_interrupt(k, k.stats.cerny_model_interrupted))
        return false;
    }
  for (uint32_t state = 0; state < automaton.states; ++state)
    for (uint32_t letter = 0; letter < 2; ++letter)
      if (!present[automaton.transition[state][letter]][letter])
        return false;
  return true;
}

bool cycle_order(const Automaton &automaton, uint32_t letter,
                 std::vector<uint32_t> &order) {
  std::vector<uint32_t> indegree(automaton.states, 0);
  for (uint32_t state = 0; state < automaton.states; ++state) {
    const uint32_t destination = automaton.transition[state][letter];
    if (destination >= automaton.states)
      return false;
    ++indegree[destination];
  }
  if (!std::all_of(indegree.begin(), indegree.end(),
                   [](uint32_t value) { return value == 1; }))
    return false;
  order.clear();
  std::vector<uint8_t> seen(automaton.states, 0);
  uint32_t current = 0;
  for (uint32_t i = 0; i < automaton.states; ++i) {
    if (seen[current])
      return false;
    seen[current] = 1;
    order.push_back(current);
    current = automaton.transition[current][letter];
  }
  return current == 0;
}

void step_subset(const Automaton &automaton, uint32_t letter,
                 const std::vector<uint8_t> &current,
                 std::vector<uint8_t> &next) {
  std::fill(next.begin(), next.end(), 0);
  for (uint32_t state = 0; state < automaton.states; ++state)
    if (current[state])
      next[automaton.transition[state][letter]] = 1;
}

bool construct_word(const Automaton &automaton, std::vector<uint32_t> &word) {
  for (uint32_t cycle = 0; cycle < 2; ++cycle) {
    const uint32_t compression = 1 - cycle;
    std::vector<uint32_t> order;
    if (!cycle_order(automaton, cycle, order))
      continue;
    uint32_t changed = UINT32_MAX;
    bool matches = true;
    for (uint32_t i = 0; i < automaton.states; ++i) {
      const uint32_t state = order[i];
      const uint32_t destination = automaton.transition[state][compression];
      if (destination == state)
        continue;
      if (destination == order[(i + 1) % automaton.states] &&
          changed == UINT32_MAX) {
        changed = state;
        continue;
      }
      matches = false;
      break;
    }
    if (!matches || changed == UINT32_MAX)
      continue;
    const uint64_t required =
        uint64_t{automaton.states - 1} * (automaton.states - 1);
    if (required > automaton.length)
      return false;
    word.clear();
    word.reserve(automaton.length);
    for (uint32_t repetition = 0; repetition + 2 < automaton.states;
         ++repetition) {
      word.push_back(compression);
      for (uint32_t i = 0; i + 1 < automaton.states; ++i)
        word.push_back(cycle);
    }
    word.push_back(compression);
    while (word.size() < automaton.length)
      word.push_back(cycle);
    return true;
  }
  return false;
}

bool set_external(const Kernel &k, std::vector<uint8_t> &assignment,
                  uint64_t dimacs, bool value) {
  if (!dimacs || dimacs > k.external_to_internal.size())
    return false;
  const Var internal = k.external_to_internal[dimacs - 1];
  if (internal == INVALID_VAR || internal >= assignment.size())
    return false;
  assignment[internal] = value;
  return true;
}

bool build_assignment(Kernel &k, const Automaton &automaton,
                      const std::vector<uint32_t> &word,
                      std::vector<uint8_t> &assignment) {
  assignment.assign(k.nvars, 0);
  std::vector<uint8_t> current(automaton.states, 1),
      next(automaton.states, 0);
  for (uint32_t time = 0; time < automaton.length; ++time) {
    const uint32_t letter = word[time];
    const uint64_t selector =
        automaton.state_variables + uint64_t{2} * time + 1;
    if (!set_external(k, assignment, selector, letter == 0) ||
        !set_external(k, assignment, selector + 1, letter == 1))
      return false;
    step_subset(automaton, letter, current, next);
    current.swap(next);
    for (uint32_t state = 0; state < automaton.states; ++state)
      if (!set_external(k, assignment,
                        uint64_t{time} * automaton.states + state + 1,
                        current[state]))
        return false;
    if (!(time & ((uint32_t{1} << 12) - 1)) &&
        kernel_rule_interrupt(k, k.stats.cerny_model_interrupted))
      return false;
  }
  return std::count(current.begin(), current.end(), uint8_t{1}) == 1;
}

} // namespace

RuleEligibility cerny_model_guard(const Kernel &k) {
  return lucky_root_eligibility(k);
}

ApplyResult cerny_model_apply(Kernel &k) {
  assert(cerny_model_guard(k));
  ++k.stats.cerny_model_calls;
  Automaton automaton;
  if (!cheap_shape(k, automaton) || !recover_transitions(k, automaton) ||
      !validate_initial_image(k, automaton))
    return ApplyResult::NoOp;
  ++k.stats.cerny_model_recognized;
  k.stats.cerny_model_states = automaton.states;
  k.stats.cerny_model_length = automaton.length;
  std::vector<uint32_t> word;
  if (!construct_word(automaton, word))
    return ApplyResult::NoOp;
  std::vector<uint8_t> assignment;
  if (!build_assignment(k, automaton, word, assignment) ||
      !lucky_model_valid(k, assignment))
    return ApplyResult::NoOp;
  lucky_publish_model(k, assignment);
  ++k.stats.cerny_model_sat;
  return ApplyResult::Sat;
}

} // namespace tactix
