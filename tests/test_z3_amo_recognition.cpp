#undef NDEBUG
#include "rules/semantic_extension/z3_amo_recognition.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

using namespace tactix::z3_amo_semantic;

namespace {

struct Fixture {
  std::vector<Premise> source;
  uint64_t next = 1;

  void add(std::vector<int32_t> literals) {
    source.push_back({next, std::move(literals), 5000 + next});
    ++next;
  }
};

void add_or(Fixture &f, int32_t output,
            const std::vector<int32_t> &inputs) {
  for (int32_t input : inputs) f.add({-input, output});
  std::vector<int32_t> backward = inputs;
  backward.push_back(-output);
  f.add(std::move(backward));
}

void add_clique(Fixture &f, int32_t root,
                const std::vector<int32_t> &atoms) {
  for (uint32_t first = 0; first < atoms.size(); ++first)
    for (uint32_t second = first + 1; second < atoms.size(); ++second)
      f.add({-root, -atoms[first], -atoms[second]});
}

std::vector<Premise> z3_nine() {
  Fixture f;
  constexpr int32_t root = 900;
  const std::vector<int32_t> first = {50, 2, 80, 13};
  const std::vector<int32_t> second = {5, 99, 17, 300};
  constexpr int32_t first_or = 700, second_or = 42, leftover = 71;
  f.add({root});
  add_clique(f, root, first);
  add_clique(f, root, second);
  add_or(f, first_or, first);
  add_or(f, second_or, second);
  add_clique(f, root, {first_or, second_or, leftover});

  // DIMACS normalization and auxiliary allocation order are irrelevant.
  std::reverse(f.source.begin(), f.source.end());
  for (Premise &premise : f.source)
    std::reverse(premise.literals.begin(), premise.literals.end());
  return f.source;
}

std::vector<Premise> nested_twelve() {
  Fixture f;
  constexpr int32_t root = 901;
  const std::vector<int32_t> first = {1, 3, 8, 21};
  const std::vector<int32_t> second = {2, 13, 34, 55};
  const std::vector<int32_t> third = {5, 89, 144, 233};
  constexpr int32_t first_or = 610, second_or = 377, third_or = 987;
  constexpr int32_t first_eight = 1597;
  f.add({root});
  add_clique(f, root, first);
  add_clique(f, root, second);
  add_clique(f, root, third);
  add_or(f, first_or, first);
  add_or(f, second_or, second);
  add_or(f, third_or, third);
  add_clique(f, root, {first_or, second_or});
  add_or(f, first_eight, {first_or, second_or});
  add_clique(f, root, {first_eight, third_or});
  return f.source;
}

Limits limits() { return {1000, 10000, 100000, 100, 100}; }

void recognizes_complete_totalizer_component() {
  const auto source = z3_nine();
  uint64_t charged = 0;
  auto charge = [](void *context, uint64_t amount) {
    *static_cast<uint64_t *>(context) += amount;
    return true;
  };
  Image image;
  assert(recognize(source, limits(), charge, &charged, image) ==
         Status::Recognized);
  assert(image.root == 900 && image.root_premise == 1);
  assert(image.leaves.size() == 9 && image.gates.size() == 2);
  assert(image.exclusions.size() == 15);
  assert(image.pairs.size() == 81);
  assert(image.classified_premises.size() == source.size());
  assert(image.classified_premises.front().native_proof_id == 5001);
  assert(image.inspected_work == charged);
  for (uint32_t first = 0; first < 9; ++first)
    for (uint32_t second = first + 1; second < 9; ++second) {
      const PairWitness &witness = image.pairs[first * 9 + second];
      assert(witness.first == first && witness.second == second);
      assert(witness.exclusion_premise);
    }
}

void recognizes_nested_totalizer_groups() {
  Image image;
  assert(recognize(nested_twelve(), limits(), nullptr, nullptr, image) ==
         Status::Recognized);
  assert(image.root == 901 && image.leaves.size() == 12);
  assert(image.gates.size() == 4 && image.pairs.size() == 144);
  for (uint32_t first = 0; first < 12; ++first)
    for (uint32_t second = first + 1; second < 12; ++second)
      assert(image.pairs[first * 12 + second].exclusion_premise);
}

void malformed_components_decline_without_output() {
  auto charge = [](void *, uint64_t) { return true; };
  Image sentinel;
  sentinel.root = 1234;

  auto source = z3_nine();
  source.erase(std::find_if(source.begin(), source.end(), [](const Premise &p) {
    std::vector<int32_t> literals = p.literals;
    std::sort(literals.begin(), literals.end());
    return literals == std::vector<int32_t>({-50, 700});
  }));
  assert(recognize(source, limits(), charge, nullptr, sentinel) ==
         Status::NotRecognized);
  assert(sentinel.root == 1234 && sentinel.leaves.empty());

  // Remove one top exclusion: all cross-group leaf pairs are then missing.
  source = z3_nine();
  source.erase(std::find_if(source.begin(), source.end(), [](const Premise &p) {
    std::vector<int32_t> literals = p.literals;
    std::sort(literals.begin(), literals.end());
    return literals == std::vector<int32_t>({-900, -700, -42});
  }));
  assert(recognize(source, limits(), charge, nullptr, sentinel) ==
         Status::NotRecognized);
  assert(sentinel.root == 1234 && sentinel.leaves.empty());

  // An unowned residual clause prevents a closed-component claim.
  source = z3_nine();
  source.push_back({9999, {-2, 71}, 88});
  assert(recognize(source, limits(), charge, nullptr, sentinel) ==
         Status::NotRecognized);
  assert(sentinel.root == 1234 && sentinel.leaves.empty());
}

void duplicate_coverage_cycle_and_provenance_decline() {
  auto charge = [](void *, uint64_t) { return true; };
  Image sentinel;
  sentinel.root = 1234;

  auto source = z3_nine();
  // This exclusion duplicates all pairs already covered between leaf 2 and
  // the second four-leaf OR atom.
  source.push_back({9999, {-900, -2, -42}, 88});
  assert(recognize(source, limits(), charge, nullptr, sentinel) ==
         Status::NotRecognized);

  source = z3_nine();
  source.back().occurrence = source.front().occurrence;
  assert(recognize(source, limits(), charge, nullptr, sentinel) ==
         Status::NotRecognized);

  // Turn the first OR into a self-dependent gate while keeping its local
  // Tseitin clauses syntactically complete.
  source = z3_nine();
  for (Premise &premise : source)
    for (int32_t &literal : premise.literals)
      if (literal == 50 || literal == -50) literal = literal > 0 ? 700 : -700;
  assert(recognize(source, limits(), charge, nullptr, sentinel) ==
         Status::NotRecognized);
  assert(sentinel.root == 1234 && sentinel.leaves.empty());
}

void interruption_and_limits_are_transactional() {
  const auto source = z3_nine();
  Image sentinel;
  sentinel.root = 1234;
  uint64_t left = 15;
  auto charge = [](void *context, uint64_t amount) {
    uint64_t &left = *static_cast<uint64_t *>(context);
    if (amount > left) return false;
    left -= amount;
    return true;
  };
  assert(recognize(source, limits(), charge, &left, sentinel) ==
         Status::Interrupted);
  assert(sentinel.root == 1234 && sentinel.leaves.empty());

  Limits small = limits();
  small.maximum_gates = 1;
  assert(recognize(source, small, nullptr, nullptr, sentinel) ==
         Status::LimitExceeded);
  assert(sentinel.root == 1234 && sentinel.leaves.empty());
}

} // namespace

int main() {
  recognizes_complete_totalizer_component();
  recognizes_nested_totalizer_groups();
  malformed_components_decline_without_output();
  duplicate_coverage_cycle_and_provenance_decline();
  interruption_and_limits_are_transactional();
}
