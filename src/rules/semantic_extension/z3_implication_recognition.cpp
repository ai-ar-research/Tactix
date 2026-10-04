#include "rules/semantic_extension/z3_implication_recognition.hpp"

#include <algorithm>
#include <climits>
#include <map>
#include <set>

namespace tactix::z3_implication_semantic {
namespace {

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

struct Meter {
  const Limits &limits;
  Charge charge;
  void *context;
  uint64_t work = 0;
  Status status = Status::Recognized;

  bool take(uint64_t amount) {
    if (work > limits.maximum_work || amount > limits.maximum_work - work) {
      status = Status::LimitExceeded;
      return false;
    }
    if (charge && !charge(context, amount)) {
      status = Status::Interrupted;
      return false;
    }
    work += amount;
    return true;
  }
};

} // namespace

Status recognize(const std::vector<Premise> &source, const Limits &limits,
                 Charge charge, void *charge_context, Image &out) {
  if (!limits.maximum_clauses || !limits.maximum_literals ||
      !limits.maximum_work || !limits.maximum_derived_binaries ||
      !limits.maximum_components || !limits.maximum_inputs_per_component ||
      !limits.maximum_guards_per_component)
    return Status::LimitExceeded;
  if (source.empty()) return Status::NotRecognized;
  if (source.size() > limits.maximum_clauses)
    return Status::LimitExceeded;

  Meter meter{limits, charge, charge_context};
  std::map<std::vector<int32_t>, const Premise *> clauses;
  std::set<uint64_t> occurrences, native_proof_ids;
  uint64_t literal_count = 0;
  for (const Premise &premise : source) {
    if (!premise.occurrence || !premise.native_proof_id ||
        !occurrences.insert(premise.occurrence).second ||
        !native_proof_ids.insert(premise.native_proof_id).second ||
        premise.literals.empty())
      return Status::NotRecognized;
    if (literal_count > limits.maximum_literals ||
        premise.literals.size() > limits.maximum_literals - literal_count)
      return Status::LimitExceeded;
    literal_count += premise.literals.size();
    if (!meter.take(uint64_t{1} + premise.literals.size())) return meter.status;
    std::vector<int32_t> normalized = premise.literals;
    for (int32_t literal : normalized)
      if (!literal || literal == INT32_MIN) return Status::NotRecognized;
    std::sort(normalized.begin(), normalized.end());
    if (std::adjacent_find(normalized.begin(), normalized.end()) !=
        normalized.end())
      return Status::NotRecognized;
    for (size_t i = 1; i < normalized.size(); ++i)
      if (variable(normalized[i - 1]) == variable(normalized[i]))
        return Status::NotRecognized;
    if (!clauses.emplace(std::move(normalized), &premise).second)
      return Status::NotRecognized;
  }

  std::vector<Component> components;
  std::set<int32_t> roots;
  std::set<uint64_t> classified;

  // A backward OR clause has one negative output and at least two positive
  // inputs.  It is a gate only when every input -> output forward clause is
  // present.  Ambiguous duplicate definitions are rejected below.
  for (const auto &[normalized, premise] : clauses) {
    if (!meter.take(1)) return meter.status;
    if (normalized.size() < 3) continue;
    size_t negatives = 0;
    int32_t root = 0;
    std::vector<int32_t> inputs;
    for (int32_t literal : normalized) {
      if (literal < 0) {
        ++negatives;
        root = -literal;
      } else {
        inputs.push_back(literal);
      }
    }
    if (negatives != 1 || inputs.size() + 1 != normalized.size()) continue;
    if (inputs.size() > limits.maximum_inputs_per_component)
      return Status::LimitExceeded;
    std::vector<uint64_t> forwards;
    bool complete = true;
    for (int32_t input : inputs) {
      std::vector<int32_t> key = {-input, root};
      std::sort(key.begin(), key.end());
      const auto found = clauses.find(key);
      if (found == clauses.end()) {
        complete = false;
        break;
      }
      forwards.push_back(found->second->occurrence);
    }
    if (!complete) continue;
    if (components.size() >= limits.maximum_components)
      return Status::LimitExceeded;
    if (!roots.insert(root).second) return Status::NotRecognized;
    Component component;
    component.root = root;
    component.inputs = std::move(inputs);
    component.backward_premise = premise->occurrence;
    component.forward_premises = std::move(forwards);
    components.push_back(std::move(component));
  }
  if (components.empty()) return Status::NotRecognized;

  std::map<int32_t, size_t> root_index;
  for (size_t i = 0; i < components.size(); ++i) {
    root_index.emplace(components[i].root, i);
    if (!classified.insert(components[i].backward_premise).second)
      return Status::NotRecognized;
    for (uint64_t occurrence : components[i].forward_premises)
      if (!classified.insert(occurrence).second)
        return Status::NotRecognized;
    for (int32_t input : components[i].inputs)
      if (roots.count(input)) return Status::NotRecognized;
  }

  // Consume fixed-false inputs before classifying guards.  An input unit must
  // belong to exactly one gate; otherwise assigning it to a semantic component
  // would discard shared provenance.
  for (const auto &[normalized, premise] : clauses) {
    if (classified.count(premise->occurrence) || normalized.size() != 1 ||
        normalized[0] >= 0)
      continue;
    const int32_t input = -normalized[0];
    size_t owner = components.size();
    for (size_t i = 0; i < components.size(); ++i)
      if (std::find(components[i].inputs.begin(), components[i].inputs.end(),
                    input) != components[i].inputs.end()) {
        if (owner != components.size()) return Status::NotRecognized;
        owner = i;
      }
    if (owner == components.size()) return Status::NotRecognized;
    components[owner].fixed_false_inputs.push_back(
        {input, premise->occurrence});
    classified.insert(premise->occurrence);
  }

  // Every remaining clause must be a binary root guard owned by exactly one
  // component.  The guard consequence may have either polarity.
  for (const auto &[normalized, premise] : clauses) {
    if (classified.count(premise->occurrence)) continue;
    if (!meter.take(1)) return meter.status;
    if (normalized.size() != 2) return Status::NotRecognized;
    size_t owner = components.size();
    int32_t consequence = 0;
    for (size_t literal_index = 0; literal_index < 2; ++literal_index) {
      const int32_t literal = normalized[literal_index];
      if (literal >= 0) continue;
      const auto found = root_index.find(-literal);
      if (found == root_index.end()) continue;
      if (owner != components.size()) return Status::NotRecognized;
      owner = found->second;
      consequence = normalized[1 - literal_index];
    }
    if (owner == components.size() ||
        variable(consequence) == uint32_t(components[owner].root))
      return Status::NotRecognized;
    Component &component = components[owner];
    if (component.guards.size() >= limits.maximum_guards_per_component)
      return Status::LimitExceeded;
    component.guards.push_back({consequence, premise->occurrence});
    classified.insert(premise->occurrence);
  }
  if (classified.size() != source.size()) return Status::NotRecognized;

  uint64_t derived_count = 0;
  for (Component &component : components) {
    if (component.guards.empty()) return Status::NotRecognized;
    std::map<int32_t, uint64_t> fixed_false;
    for (const FixedFalseInput &fixed : component.fixed_false_inputs)
      if (!fixed_false.emplace(fixed.input, fixed.premise).second)
        return Status::NotRecognized;
    component.reduced_backward.push_back(-component.root);
    component.reduced_backward_rup_premises.push_back(
        component.backward_premise);
    for (size_t i = 0; i < component.inputs.size(); ++i) {
      const auto fixed = fixed_false.find(component.inputs[i]);
      if (fixed == fixed_false.end()) {
        component.active_inputs.push_back(component.inputs[i]);
        component.active_forward_premises.push_back(
            component.forward_premises[i]);
        component.reduced_backward.push_back(component.inputs[i]);
      } else {
        component.reduced_backward_rup_premises.push_back(fixed->second);
      }
    }
    if (component.active_inputs.size() < 2) return Status::NotRecognized;
    std::sort(component.reduced_backward.begin(),
              component.reduced_backward.end());
    if (component.active_inputs.size() >
        (limits.maximum_derived_binaries - derived_count) /
            component.guards.size())
      return Status::LimitExceeded;
    const uint64_t component_derived =
        uint64_t(component.active_inputs.size()) * component.guards.size();
    derived_count += component_derived;
    component.derived_binary_count = component_derived;
    std::set<uint32_t> input_variables;
    for (int32_t input : component.active_inputs)
      input_variables.insert(variable(input));
    for (const Guard &guard : component.guards) {
      if (!meter.take(1)) return meter.status;
      if (input_variables.count(variable(guard.consequence)))
        return Status::NotRecognized;
    }
    std::sort(component.guards.begin(), component.guards.end(),
              [](const Guard &a, const Guard &b) {
                if (a.consequence != b.consequence)
                  return a.consequence < b.consequence;
                return a.premise < b.premise;
              });
  }
  std::sort(components.begin(), components.end(),
            [](const Component &a, const Component &b) {
              return a.root < b.root;
            });

  Image image;
  image.components = std::move(components);
  image.classified_premises = source;
  std::sort(image.classified_premises.begin(), image.classified_premises.end(),
            [](const Premise &a, const Premise &b) {
              return a.occurrence < b.occurrence;
            });
  image.inspected_work = meter.work;
  out = std::move(image);
  return Status::Recognized;
}

bool consequence_at(const Component &component, uint32_t input_index,
                    uint32_t guard_index, BinaryConsequence &out) {
  if (input_index >= component.active_inputs.size() ||
      guard_index >= component.guards.size() ||
      input_index >= component.active_forward_premises.size())
    return false;
  const Guard &guard = component.guards[guard_index];
  out = {component.active_inputs[input_index], guard.consequence,
         component.active_forward_premises[input_index], guard.premise};
  return true;
}

} // namespace tactix::z3_implication_semantic
