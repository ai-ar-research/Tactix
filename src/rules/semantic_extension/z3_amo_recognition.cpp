#include "rules/semantic_extension/z3_amo_recognition.hpp"

#include "kernel/types.hpp"

#include <algorithm>
#include <climits>
#include <functional>
#include <limits>
#include <map>
#include <set>

namespace tactix::z3_amo_semantic {
namespace {

uint32_t variable(int32_t literal) {
  return external_variable_of(literal);
}

struct Meter {
  const Limits &limits;
  Charge charge;
  void *context;
  uint64_t work = 0;
  Status status = Status::Recognized;

  bool take(uint64_t amount) {
    if (amount > limits.maximum_work - work) {
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
      !limits.maximum_work || !limits.maximum_leaves ||
      !limits.maximum_gates)
    return Status::LimitExceeded;
  if (source.empty()) return Status::NotRecognized;
  if (source.size() > limits.maximum_clauses)
    return Status::LimitExceeded;

  Meter meter{limits, charge, charge_context};
  std::map<std::vector<int32_t>, const Premise *> clauses;
  std::set<uint64_t> occurrences;
  uint64_t literal_count = 0;
  for (const Premise &premise : source) {
    if (!premise.occurrence || premise.literals.empty() ||
        !occurrences.insert(premise.occurrence).second)
      return Status::NotRecognized;
    if (premise.literals.size() > limits.maximum_literals - literal_count)
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

  const Premise *root_premise = nullptr;
  int32_t root = 0;
  for (const auto &[normalized, premise] : clauses) {
    if (normalized.size() != 1) continue;
    if (normalized[0] <= 0 || root_premise) return Status::NotRecognized;
    root = normalized[0];
    root_premise = premise;
  }
  if (!root_premise) return Status::NotRecognized;

  std::map<int32_t, OrGate> gates;
  for (const auto &[normalized, premise] : clauses) {
    if (!meter.take(1)) return meter.status;
    if (normalized.size() < 3) continue;
    size_t negatives = 0;
    int32_t output = 0;
    std::vector<int32_t> inputs;
    for (int32_t literal : normalized) {
      if (literal < 0) {
        ++negatives;
        output = -literal;
      } else {
        inputs.push_back(literal);
      }
    }
    if (negatives != 1 || inputs.size() + 1 != normalized.size()) continue;
    if (output == root || inputs.size() < 2 ||
        gates.size() >= limits.maximum_gates)
      return output == root || inputs.size() < 2 ? Status::NotRecognized
                                                 : Status::LimitExceeded;
    OrGate gate;
    gate.output = output;
    gate.inputs = inputs;
    gate.backward_premise = premise->occurrence;
    if (!gates.emplace(output, std::move(gate)).second)
      return Status::NotRecognized;
  }

  std::set<uint64_t> classified{root_premise->occurrence};
  for (auto &[output, gate] : gates) {
    if (!meter.take(uint64_t{1} + gate.inputs.size())) return meter.status;
    classified.insert(gate.backward_premise);
    for (int32_t input : gate.inputs) {
      if (input == output || input == root) return Status::NotRecognized;
      std::vector<int32_t> key = {-input, output};
      std::sort(key.begin(), key.end());
      const auto found = clauses.find(key);
      if (found == clauses.end()) return Status::NotRecognized;
      gate.forward_premises.push_back(found->second->occurrence);
      if (!classified.insert(found->second->occurrence).second)
        return Status::NotRecognized;
    }
  }

  std::vector<Exclusion> exclusions;
  for (const auto &[normalized, premise] : clauses) {
    if (classified.count(premise->occurrence)) continue;
    if (!meter.take(uint64_t{1} + normalized.size())) return meter.status;
    if (normalized.size() != 3 || normalized[0] >= 0 ||
        normalized[1] >= 0 || normalized[2] >= 0)
      return Status::NotRecognized;
    std::vector<int32_t> atoms;
    bool saw_root = false;
    for (int32_t literal : normalized) {
      if (-literal == root) {
        if (saw_root) return Status::NotRecognized;
        saw_root = true;
      } else {
        atoms.push_back(-literal);
      }
    }
    if (!saw_root || atoms.size() != 2 || atoms[0] == atoms[1])
      return Status::NotRecognized;
    exclusions.push_back({atoms[0], atoms[1], premise->occurrence});
    classified.insert(premise->occurrence);
  }
  if (exclusions.empty() || classified.size() != source.size())
    return Status::NotRecognized;

  std::map<int32_t, uint8_t> state;
  std::map<int32_t, std::set<int32_t>> expansions;
  std::set<int32_t> used_gates;
  std::function<bool(int32_t)> expand = [&](int32_t atom) {
    if (!meter.take(1)) return false;
    const auto gate = gates.find(atom);
    if (gate == gates.end()) {
      expansions[atom].insert(atom);
      return true;
    }
    if (state[atom] == 1) return false;
    if (state[atom] == 2) return true;
    state[atom] = 1;
    used_gates.insert(atom);
    std::set<int32_t> leaves;
    for (int32_t input : gate->second.inputs) {
      if (!expand(input)) return false;
      const auto &part = expansions[input];
      const size_t before = leaves.size();
      leaves.insert(part.begin(), part.end());
      if (leaves.size() != before + part.size()) return false;
    }
    if (leaves.size() < 2) return false;
    expansions[atom] = std::move(leaves);
    state[atom] = 2;
    return true;
  };

  for (const Exclusion &exclusion : exclusions)
    if (!expand(exclusion.first) || !expand(exclusion.second))
      return meter.status == Status::Recognized ? Status::NotRecognized
                                                : meter.status;
  if (used_gates.size() != gates.size()) return Status::NotRecognized;

  std::set<int32_t> leaf_set;
  for (const Exclusion &exclusion : exclusions) {
    const auto &first = expansions[exclusion.first];
    const auto &second = expansions[exclusion.second];
    leaf_set.insert(first.begin(), first.end());
    leaf_set.insert(second.begin(), second.end());
  }
  if (leaf_set.size() < 3) return Status::NotRecognized;
  if (leaf_set.size() > limits.maximum_leaves) return Status::LimitExceeded;
  if (leaf_set.count(root)) return Status::NotRecognized;
  for (const auto &[output, gate] : gates) {
    (void)gate;
    if (leaf_set.count(output)) return Status::NotRecognized;
  }

  std::vector<int32_t> leaves(leaf_set.begin(), leaf_set.end());
  std::map<int32_t, uint32_t> leaf_index;
  for (uint32_t i = 0; i < leaves.size(); ++i) leaf_index[leaves[i]] = i;
  if (leaves.size() > std::numeric_limits<size_t>::max() / leaves.size())
    return Status::LimitExceeded;
  std::vector<PairWitness> pairs(leaves.size() * leaves.size());
  for (const Exclusion &exclusion : exclusions) {
    const auto &first = expansions[exclusion.first];
    const auto &second = expansions[exclusion.second];
    for (int32_t a_literal : first)
      for (int32_t b_literal : second) {
        if (!meter.take(1)) return meter.status;
        uint32_t a = leaf_index.at(a_literal), b = leaf_index.at(b_literal);
        if (a == b) return Status::NotRecognized;
        if (a > b) std::swap(a, b);
        PairWitness &slot = pairs[size_t(a) * leaves.size() + b];
        if (slot.exclusion_premise) return Status::NotRecognized;
        slot = {a, b, exclusion.premise, exclusion.first, exclusion.second};
        pairs[size_t(b) * leaves.size() + a] = slot;
      }
  }
  for (uint32_t a = 0; a < leaves.size(); ++a)
    for (uint32_t b = a + 1; b < leaves.size(); ++b)
      if (!pairs[size_t(a) * leaves.size() + b].exclusion_premise)
        return Status::NotRecognized;

  Image image;
  image.root = root;
  image.root_premise = root_premise->occurrence;
  image.leaves = std::move(leaves);
  for (auto &[output, gate] : gates) {
    (void)output;
    image.gates.push_back(std::move(gate));
  }
  image.exclusions = std::move(exclusions);
  image.pairs = std::move(pairs);
  image.classified_premises = source;
  std::sort(image.classified_premises.begin(), image.classified_premises.end(),
            [](const Premise &a, const Premise &b) {
              return a.occurrence < b.occurrence;
            });
  image.inspected_work = meter.work;
  out = std::move(image);
  return Status::Recognized;
}

} // namespace tactix::z3_amo_semantic
