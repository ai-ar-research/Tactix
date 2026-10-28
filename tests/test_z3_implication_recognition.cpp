#undef NDEBUG
#include "rules/semantic_extension/z3_implication_recognition.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <map>
#include <vector>

using namespace tactix::z3_implication_semantic;

namespace {

struct Fixture {
  std::vector<Premise> source;
  uint64_t next = 1;

  void add(std::vector<int32_t> literals) {
    source.push_back({next, std::move(literals), 7000 + next});
    ++next;
  }

  void add_component(int32_t root, const std::vector<int32_t> &inputs,
                     const std::vector<int32_t> &consequences) {
    for (int32_t input : inputs) add({-input, root});
    std::vector<int32_t> backward = inputs;
    backward.push_back(-root);
    add(std::move(backward));
    for (int32_t consequence : consequences) add({-root, consequence});
  }
};

Limits limits() { return {100, 1000, 100000, 1000, 10, 20, 20}; }

std::vector<Premise> source() {
  Fixture fixture;
  fixture.add_component(500, {3, 40, 9}, {-70, 81});
  fixture.add({-40});
  fixture.add_component(200, {7, 19}, {-91});
  std::reverse(fixture.source.begin(), fixture.source.end());
  for (Premise &premise : fixture.source)
    std::reverse(premise.literals.begin(), premise.literals.end());
  return fixture.source;
}

bool binary_rup(const std::vector<Premise> &premises,
                const BinaryConsequence &consequence) {
  const auto find = [&](uint64_t occurrence) -> const Premise & {
    const auto found = std::find_if(
        premises.begin(), premises.end(), [occurrence](const Premise &premise) {
          return premise.occurrence == occurrence;
        });
    assert(found != premises.end());
    return *found;
  };
  const Premise &forward = find(consequence.forward_premise);
  const Premise &guard = find(consequence.guard_premise);
  const int32_t root = *std::find_if(
      forward.literals.begin(), forward.literals.end(),
      [](int32_t literal) { return literal > 0; });
  const std::vector<int32_t> expected_forward = {-consequence.trigger, root};
  const std::vector<int32_t> expected_guard = {-root, consequence.consequence};
  auto normalized_forward = forward.literals;
  auto normalized_guard = guard.literals;
  auto normalized_expected_forward = expected_forward;
  auto normalized_expected_guard = expected_guard;
  std::sort(normalized_forward.begin(), normalized_forward.end());
  std::sort(normalized_guard.begin(), normalized_guard.end());
  std::sort(normalized_expected_forward.begin(), normalized_expected_forward.end());
  std::sort(normalized_expected_guard.begin(), normalized_expected_guard.end());
  return normalized_forward == normalized_expected_forward &&
         normalized_guard == normalized_expected_guard;
}

bool rup(const std::vector<Premise> &source,
         const std::vector<uint64_t> &premises,
         const std::vector<int32_t> &consequence) {
  std::map<int32_t, bool> assignment;
  for (int32_t literal : consequence)
    assignment[static_cast<int32_t>(literal < 0 ? -int64_t(literal)
                                                 : literal)] = literal < 0;
  for (;;) {
    bool changed = false;
    for (uint64_t occurrence : premises) {
      const auto found = std::find_if(
          source.begin(), source.end(), [occurrence](const Premise &premise) {
            return premise.occurrence == occurrence;
          });
      assert(found != source.end());
      uint32_t unassigned = 0;
      int32_t unit = 0;
      bool satisfied = false;
      for (int32_t literal : found->literals) {
        const int32_t var = static_cast<int32_t>(
            literal < 0 ? -int64_t(literal) : literal);
        const auto value = assignment.find(var);
        if (value == assignment.end()) {
          ++unassigned;
          unit = literal;
        } else if (value->second == (literal > 0)) {
          satisfied = true;
          break;
        }
      }
      if (satisfied) continue;
      if (!unassigned) return true;
      if (unassigned == 1) {
        const int32_t var = static_cast<int32_t>(
            unit < 0 ? -int64_t(unit) : unit);
        const bool value = unit > 0;
        const auto [position, inserted] = assignment.emplace(var, value);
        if (!inserted && position->second != value) return true;
        changed |= inserted;
      }
    }
    if (!changed) return false;
  }
}

void recognizes_closed_components_and_binary_witnesses() {
  const auto clauses = source();
  uint64_t charged = 0;
  auto charge = [](void *context, uint64_t amount) {
    *static_cast<uint64_t *>(context) += amount;
    return true;
  };
  Image image;
  assert(recognize(clauses, limits(), charge, &charged, image) ==
         Status::Recognized);
  assert(image.components.size() == 2);
  assert(image.components[0].root == 200);
  assert(image.components[0].derived_binary_count == 2);
  assert(image.components[1].root == 500);
  assert(image.components[1].fixed_false_inputs.size() == 1);
  assert(image.components[1].fixed_false_inputs[0].input == 40);
  assert(image.components[1].active_inputs == std::vector<int32_t>({3, 9}));
  assert(image.components[1].reduced_backward ==
         std::vector<int32_t>({-500, 3, 9}));
  assert(image.components[1].reduced_backward_rup_premises.size() == 2);
  assert(rup(clauses, image.components[1].reduced_backward_rup_premises,
             image.components[1].reduced_backward));
  assert(image.components[1].derived_binary_count == 4);
  assert(image.classified_premises.size() == clauses.size());
  assert(image.classified_premises.front().native_proof_id == 7001);
  assert(image.inspected_work == charged);
  for (const Component &component : image.components)
    for (uint32_t input = 0; input < component.active_inputs.size(); ++input)
      for (uint32_t guard = 0; guard < component.guards.size(); ++guard) {
        BinaryConsequence consequence;
        assert(consequence_at(component, input, guard, consequence));
        assert(binary_rup(clauses, consequence));
      }
  BinaryConsequence invalid;
  assert(!consequence_at(image.components[0], 99, 0, invalid));
}

void incomplete_ambiguous_or_residual_sources_decline() {
  Image sentinel;
  sentinel.inspected_work = 1234;
  auto clauses = source();
  clauses.erase(std::find_if(clauses.begin(), clauses.end(), [](const Premise &p) {
    auto literals = p.literals;
    std::sort(literals.begin(), literals.end());
    return literals == std::vector<int32_t>({-40, 500});
  }));
  assert(recognize(clauses, limits(), nullptr, nullptr, sentinel) ==
         Status::NotRecognized);
  assert(sentinel.inspected_work == 1234 && sentinel.components.empty());

  clauses = source();
  clauses.push_back({100, {-500, -200}, 7100});
  assert(recognize(clauses, limits(), nullptr, nullptr, sentinel) ==
         Status::NotRecognized);

  clauses = source();
  clauses.push_back({100, {1, 2}, 7100});
  assert(recognize(clauses, limits(), nullptr, nullptr, sentinel) ==
         Status::NotRecognized);
}

void aliases_duplicate_provenance_and_limits_decline() {
  Image sentinel;
  auto clauses = source();
  clauses.front().occurrence = clauses.back().occurrence;
  assert(recognize(clauses, limits(), nullptr, nullptr, sentinel) ==
         Status::NotRecognized);

  clauses = source();
  clauses.front().native_proof_id = clauses.back().native_proof_id;
  assert(recognize(clauses, limits(), nullptr, nullptr, sentinel) ==
         Status::NotRecognized);

  Fixture alias;
  alias.add_component(20, {3, 4}, {9});
  alias.add_component(3, {7, 8}, {10});
  assert(recognize(alias.source, limits(), nullptr, nullptr, sentinel) ==
         Status::NotRecognized);

  Limits small = limits();
  small.maximum_derived_binaries = 5;
  assert(recognize(source(), small, nullptr, nullptr, sentinel) ==
         Status::LimitExceeded);
}

void interruption_is_transactional() {
  Image sentinel;
  sentinel.inspected_work = 1234;
  uint64_t remaining = 8;
  auto charge = [](void *context, uint64_t amount) {
    uint64_t &left = *static_cast<uint64_t *>(context);
    if (amount > left) return false;
    left -= amount;
    return true;
  };
  assert(recognize(source(), limits(), charge, &remaining, sentinel) ==
         Status::Interrupted);
  assert(sentinel.inspected_work == 1234 && sentinel.components.empty());
}

} // namespace

int main() {
  recognizes_closed_components_and_binary_witnesses();
  incomplete_ambiguous_or_residual_sources_decline();
  aliases_duplicate_provenance_and_limits_decline();
  interruption_is_transactional();
}
