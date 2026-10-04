#include "rules/semantic_extension/balanced_signature_recognition.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

using namespace tactix::balanced_signature_semantic;

namespace {

struct Fixture {
  std::vector<Premise> clauses;
  uint64_t next = 1;

  void add(std::vector<int32_t> literals) {
    clauses.push_back({next, std::move(literals), 10000 + next});
    ++next;
  }
};

std::vector<std::vector<int8_t>> signatures(uint32_t width) {
  uint32_t count = 1;
  for (uint32_t i = 0; i < width; ++i) count *= 3;
  std::vector<std::vector<int8_t>> result;
  for (uint32_t code = 0; code < count; ++code) {
    uint32_t cursor = code;
    std::vector<int8_t> value(width);
    bool nonzero = false;
    for (uint32_t w = 0; w < width; ++w) {
      value[w] = static_cast<int8_t>(cursor % 3) - 1;
      cursor /= 3;
      nonzero |= value[w] != 0;
    }
    if (nonzero) result.push_back(std::move(value));
  }
  return result;
}

std::vector<Premise> encoding() {
  Fixture f;
  std::vector<std::vector<int32_t>> selectors = {
      {101, 7, 80, 19, 250, 42, 63, 12},
      {700, 31, 88, 5, 222, 17, 95, 55}};
  for (auto &domain : selectors) std::sort(domain.begin(), domain.end());
  const std::vector<std::vector<std::pair<int32_t, int32_t>>> states = {
      {{4, 900}, {50, 600}}, {{2, 333}, {13, 1000}}};
  const auto values = signatures(2);
  assert(values.size() == selectors[0].size());

  for (const auto &domain : selectors) f.add(domain);
  for (uint32_t ball = 0; ball < selectors.size(); ++ball) {
    for (uint32_t a = 0; a < selectors[ball].size(); ++a)
      for (uint32_t b = a + 1; b < selectors[ball].size(); ++b)
        f.add({-selectors[ball][a], -selectors[ball][b]});
    for (auto [negative, positive] : states[ball])
      f.add({-negative, -positive});
    for (uint32_t s = 0; s < selectors[ball].size(); ++s)
      for (uint32_t w = 0; w < 2; ++w) {
        f.add({-selectors[ball][s],
               values[s][w] == -1 ? states[ball][w].first
                                   : -states[ball][w].first});
        f.add({-selectors[ball][s],
               values[s][w] == 1 ? states[ball][w].second
                                  : -states[ball][w].second});
      }
  }
  for (uint32_t a = 0; a < values.size(); ++a)
    for (uint32_t b = 0; b < values.size(); ++b) {
      bool balanced = true;
      for (uint32_t w = 0; w < 2; ++w)
        balanced &= values[a][w] + values[b][w] == 0;
      if (!balanced) f.add({-selectors[0][a], -selectors[1][b]});
    }
  // Recognition is intentionally independent of clause and literal order.
  std::reverse(f.clauses.begin(), f.clauses.end());
  for (Premise &premise : f.clauses)
    std::reverse(premise.literals.begin(), premise.literals.end());
  return f.clauses;
}

Limits limits() {
  return {1000, 10000, 100000, 1000, 1000, 4, 4};
}

void exact_image_and_provenance() {
  const auto source = encoding();
  Image image;
  uint64_t charged = 0;
  auto charge = [](void *context, uint64_t amount) {
    *static_cast<uint64_t *>(context) += amount;
    return true;
  };
  assert(recognize(source, limits(), charge, &charged, image) ==
         Status::Recognized);
  assert(image.balls == 2 && image.width == 2);
  assert(image.domains.size() == 2 && image.forbidden.size() == 56);
  assert(image.classified_occurrences.size() == source.size());
  assert(image.classified_premises.size() == source.size());
  assert(image.classified_premises.front().native_proof_id == 10001);
  assert(image.inspected_work == charged);
  for (const Ball &ball : image.domains) {
    assert(ball.states.size() == 2 && ball.signatures.size() == 8);
    for (const Signature &signature : ball.signatures)
      assert(signature.value.size() == 2 &&
             signature.definition_premises.size() == 4);
  }
}

void near_misses_are_transactional() {
  auto source = encoding();
  Image sentinel;
  sentinel.balls = 99;
  auto charge = [](void *, uint64_t) { return true; };

  source.pop_back();
  assert(recognize(source, limits(), charge, nullptr, sentinel) ==
         Status::NotRecognized);
  assert(sentinel.balls == 99 && sentinel.domains.empty());

  source = encoding();
  source.push_back({999999, {-7, -5, -17}, 77});
  assert(recognize(source, limits(), charge, nullptr, sentinel) ==
         Status::NotRecognized);
  assert(sentinel.balls == 99 && sentinel.domains.empty());

  source = encoding();
  source.back().occurrence = source.front().occurrence;
  assert(recognize(source, limits(), charge, nullptr, sentinel) ==
         Status::NotRecognized);
  assert(sentinel.balls == 99 && sentinel.domains.empty());
}

void interruption_and_limits_are_transactional() {
  const auto source = encoding();
  Image sentinel;
  sentinel.width = 77;
  uint64_t left = 12;
  auto charge = [](void *context, uint64_t amount) {
    uint64_t &left = *static_cast<uint64_t *>(context);
    if (amount > left) return false;
    left -= amount;
    return true;
  };
  assert(recognize(source, limits(), charge, &left, sentinel) ==
         Status::Interrupted);
  assert(sentinel.width == 77 && sentinel.domains.empty());

  Limits small = limits();
  small.maximum_clauses = 10;
  assert(recognize(source, small, nullptr, nullptr, sentinel) ==
         Status::LimitExceeded);
  assert(sentinel.width == 77 && sentinel.domains.empty());
}

} // namespace

int main() {
  exact_image_and_provenance();
  near_misses_are_transactional();
  interruption_and_limits_are_transactional();
}
