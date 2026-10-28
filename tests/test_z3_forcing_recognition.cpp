#undef NDEBUG
#include "rules/semantic_extension/z3_forcing_recognition.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <map>
#include <vector>

using namespace tactix::z3_forcing_semantic;

namespace {

struct Fixture {
  std::vector<Premise> source;
  uint64_t next = 1;

  void add(std::vector<int32_t> literals) {
    source.push_back({next, std::move(literals), 9000 + next});
    ++next;
  }

  void add_asserted_or(int32_t root, int32_t left, int32_t right) {
    add({root});
    add({-left, root});
    add({-right, root});
    add({left, right, -root});
  }
};

Limits limits() { return {100, 500, 10000, 20}; }

std::vector<Premise> mixed_source() {
  Fixture fixture;
  fixture.add_asserted_or(700, 51, 3);
  fixture.add({200, 17});
  fixture.add_asserted_or(42, 91, 12);
  std::reverse(fixture.source.begin(), fixture.source.end());
  for (Premise &premise : fixture.source)
    std::reverse(premise.literals.begin(), premise.literals.end());
  return fixture.source;
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
        const int32_t variable = static_cast<int32_t>(
            literal < 0 ? -int64_t(literal) : literal);
        const auto value = assignment.find(variable);
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
        const int32_t variable = static_cast<int32_t>(
            unit < 0 ? -int64_t(unit) : unit);
        const bool value = unit > 0;
        const auto [position, inserted] = assignment.emplace(variable, value);
        if (!inserted && position->second != value) return true;
        changed |= inserted;
      }
    }
    if (!changed) return false;
  }
}

void recognizes_complete_mixed_family_and_certificates() {
  const auto source = mixed_source();
  uint64_t charged = 0;
  auto charge = [](void *context, uint64_t amount) {
    *static_cast<uint64_t *>(context) += amount;
    return true;
  };
  Image image;
  assert(recognize(source, limits(), charge, &charged, image) ==
         Status::Recognized);
  assert(image.choices.size() == 3);
  assert(image.classified_premises.size() == source.size());
  assert(image.classified_premises.front().native_proof_id == 9001);
  assert(image.inspected_work == charged);

  uint32_t direct = 0, asserted = 0;
  for (const Choice &choice : image.choices) {
    assert(choice.consequence ==
           std::vector<int32_t>({choice.left, choice.right}));
    if (choice.encoding == Encoding::DirectClause) {
      ++direct;
      assert(choice.direct_premise && !choice.root);
      assert(choice.rup_premises.empty());
    } else {
      ++asserted;
      assert(choice.root && choice.root_premise && choice.backward_premise);
      assert(choice.left_forward_premise && choice.right_forward_premise);
      assert(rup(source, choice.rup_premises, choice.consequence));
    }
  }
  assert(direct == 1 && asserted == 2);
}

void incomplete_or_extra_sources_decline_transactionally() {
  Image sentinel;
  sentinel.inspected_work = 1234;
  auto source = mixed_source();
  source.erase(std::find_if(source.begin(), source.end(), [](const Premise &p) {
    std::vector<int32_t> clause = p.literals;
    std::sort(clause.begin(), clause.end());
    return clause == std::vector<int32_t>({-51, 700});
  }));
  assert(recognize(source, limits(), nullptr, nullptr, sentinel) ==
         Status::NotRecognized);
  assert(sentinel.inspected_work == 1234 && sentinel.choices.empty());

  source = mixed_source();
  source.push_back({101, {-3, -17}, 9101});
  assert(recognize(source, limits(), nullptr, nullptr, sentinel) ==
         Status::NotRecognized);
  assert(sentinel.inspected_work == 1234 && sentinel.choices.empty());
}

void ambiguity_aliases_and_duplicate_provenance_decline() {
  Image sentinel;
  auto source = mixed_source();
  source.push_back({101, {8, 9, -700}, 9101});
  assert(recognize(source, limits(), nullptr, nullptr, sentinel) ==
         Status::NotRecognized);

  source = mixed_source();
  source.front().occurrence = source.back().occurrence;
  assert(recognize(source, limits(), nullptr, nullptr, sentinel) ==
         Status::NotRecognized);

  source = mixed_source();
  source.front().native_proof_id = source.back().native_proof_id;
  assert(recognize(source, limits(), nullptr, nullptr, sentinel) ==
         Status::NotRecognized);

  Fixture alias;
  alias.add_asserted_or(40, 2, 3);
  alias.add({3, 8});
  assert(recognize(alias.source, limits(), nullptr, nullptr, sentinel) ==
         Status::NotRecognized);

  Fixture root_alias;
  root_alias.add_asserted_or(40, 2, 3);
  root_alias.add_asserted_or(2, 8, 9);
  assert(recognize(root_alias.source, limits(), nullptr, nullptr, sentinel) ==
         Status::NotRecognized);
}

void interruption_and_limits_are_transactional() {
  const auto source = mixed_source();
  Image sentinel;
  sentinel.inspected_work = 1234;
  uint64_t left = 5;
  auto charge = [](void *context, uint64_t amount) {
    uint64_t &remaining = *static_cast<uint64_t *>(context);
    if (amount > remaining) return false;
    remaining -= amount;
    return true;
  };
  assert(recognize(source, limits(), charge, &left, sentinel) ==
         Status::Interrupted);
  assert(sentinel.inspected_work == 1234 && sentinel.choices.empty());

  Limits small = limits();
  small.maximum_choices = 2;
  assert(recognize(source, small, nullptr, nullptr, sentinel) ==
         Status::LimitExceeded);
  assert(sentinel.inspected_work == 1234 && sentinel.choices.empty());

  small = limits();
  small.maximum_literals = 3;
  assert(recognize(source, small, nullptr, nullptr, sentinel) ==
         Status::LimitExceeded);
  assert(sentinel.inspected_work == 1234 && sentinel.choices.empty());
}

} // namespace

int main() {
  recognizes_complete_mixed_family_and_certificates();
  incomplete_or_extra_sources_decline_transactionally();
  ambiguity_aliases_and_duplicate_provenance_decline();
  interruption_and_limits_are_transactional();
}
