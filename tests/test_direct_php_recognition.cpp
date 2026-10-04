#undef NDEBUG
#include "proof/direct_php_recognition.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <set>
#include <vector>

using namespace tactix::direct_php_recognition;
using DirectMatrix = tactix::direct_php_certificate::Matrix;

namespace {

struct Fixture {
  std::vector<Premise> source;
  uint64_t occurrence = 1;
  uint64_t proof = 10001;

  void add(std::vector<int32_t> literals) {
    source.push_back({occurrence++, std::move(literals), proof++});
  }
};

Limits limits() {
  return {10000, 100000, 10000000, 1000, 100000, 1000000, 32};
}

Fixture php(unsigned holes, int32_t first_variable = 1) {
  Fixture fixture;
  std::vector<std::vector<int32_t>> x(
      holes + 1, std::vector<int32_t>(holes));
  for (auto &row : x)
    for (int32_t &variable : row)
      variable = first_variable++;
  for (const auto &row : x)
    fixture.add(row);
  for (unsigned hole = 0; hole < holes; ++hole)
    for (unsigned first = 0; first <= holes; ++first)
      for (unsigned second = first + 1; second <= holes; ++second)
        fixture.add({-x[first][hole], -x[second][hole]});
  return fixture;
}

void renumber(std::vector<Premise> &source, int32_t maximum, unsigned mode) {
  for (Premise &premise : source)
    for (int32_t &literal : premise.literals) {
      const int32_t sign = literal < 0 ? -1 : 1;
      const int32_t old = literal < 0 ? -literal : literal;
      const int32_t image = mode == 0 ? old : maximum + 1 - old;
      literal = sign * (image * 3 + 7);
    }
}

void permute_fixture(Fixture &fixture, unsigned rotation, bool reverse_literals) {
  if (!fixture.source.empty())
    std::rotate(fixture.source.begin(),
                fixture.source.begin() + rotation % fixture.source.size(),
                fixture.source.end());
  if (rotation & 1)
    std::reverse(fixture.source.begin(), fixture.source.end());
  if (reverse_literals)
    for (Premise &premise : fixture.source)
      std::reverse(premise.literals.begin(), premise.literals.end());
}

void check_image(const Image &image, unsigned holes) {
  assert(image.variables.size() == holes + 1);
  assert(image.alos.size() == holes + 1);
  assert(image.amos.size() ==
         uint64_t(holes) * holes * (holes + 1) / 2);
  assert(image.census.alo_clauses == holes + 1);
  assert(image.census.amo_clauses == image.amos.size());
  assert(image.census.premises.size() ==
         image.census.alo_clauses + image.census.amo_clauses);
  assert(image.census.literals ==
         uint64_t(holes + 1) * holes + uint64_t{2} * image.amos.size());
  std::set<uint64_t> occurrences, proofs;
  for (const Premise &premise : image.census.premises) {
    assert(occurrences.insert(premise.occurrence).second);
    assert(proofs.insert(premise.native_proof_id).second);
  }
  for (const AmoPremise &amo : image.amos) {
    assert(amo.hole < holes);
    assert(amo.first_pigeon < amo.second_pigeon);
    assert(image.variables[amo.first_pigeon][amo.hole] ==
           amo.first_variable);
    assert(image.variables[amo.second_pigeon][amo.hole] ==
           amo.second_variable);
  }
}

void exhaustive_small_isomorphisms() {
  for (unsigned holes = 1; holes <= 3; ++holes) {
    std::vector<unsigned> rows(holes + 1), columns(holes);
    std::iota(rows.begin(), rows.end(), 0);
    do {
      std::iota(columns.begin(), columns.end(), 0);
      do {
        for (unsigned naming = 0; naming < 2; ++naming) {
          Fixture base = php(holes);
          std::vector<Premise> alos(base.source.begin(),
                                    base.source.begin() + holes + 1);
          std::vector<Premise> amos(base.source.begin() + holes + 1,
                                    base.source.end());
          Fixture transformed;
          transformed.occurrence = 5000;
          transformed.proof = 15000;
          for (unsigned row : rows) {
            std::vector<int32_t> clause;
            for (unsigned column : columns)
              clause.push_back(alos[row].literals[column]);
            transformed.add(std::move(clause));
          }
          // Clause reconstruction after row/column permutations is easiest
          // from the permuted ALO matrix itself.
          std::vector<std::vector<int32_t>> x;
          for (unsigned row = 0; row <= holes; ++row)
            x.push_back(transformed.source[row].literals);
          for (unsigned hole = 0; hole < holes; ++hole)
            for (unsigned first = 0; first <= holes; ++first)
              for (unsigned second = first + 1; second <= holes; ++second)
                transformed.add({-x[first][hole], -x[second][hole]});
          (void)amos;
          renumber(transformed.source, holes * (holes + 1), naming);
          permute_fixture(transformed, holes + naming, naming != 0);
          Image image;
          assert(recognize(transformed.source, limits(), nullptr, nullptr,
                           image) == Status::Recognized);
          check_image(image, holes);
        }
      } while (std::next_permutation(columns.begin(), columns.end()));
    } while (std::next_permutation(rows.begin(), rows.end()));
  }
}

void noise_and_exact_census() {
  Fixture fixture = php(4, 20);
  const size_t core_size = fixture.source.size();
  fixture.add({900});
  fixture.add({901, 902, 903});
  fixture.add({-904, 905});
  // Extra same-pigeon AMOs and unrelated cross clauses are legal snapshot
  // noise and must not contaminate the authenticated premise census.
  fixture.add({-20, -21});
  fixture.add({-20, -999});
  permute_fixture(fixture, 7, true);
  Image image;
  assert(recognize(fixture.source, limits(), nullptr, nullptr, image) ==
         Status::Recognized);
  check_image(image, 4);
  assert(image.census.premises.size() == core_size);
}

void ambiguity_and_near_misses() {
  Image sentinel;
  sentinel.variables = {{77}};

  Fixture two = php(2, 1);
  Fixture other = php(2, 100);
  for (Premise premise : other.source) {
    premise.occurrence += 1000;
    premise.native_proof_id += 1000;
    two.source.push_back(std::move(premise));
  }
  assert(recognize(two.source, limits(), nullptr, nullptr, sentinel) ==
         Status::Ambiguous);
  assert(sentinel.variables == DirectMatrix{{77}});

  Fixture mapping = php(2);
  // Complete all cross-row edges, producing multiple column decompositions.
  const std::vector<std::vector<int32_t>> rows = {{1, 2}, {3, 4}, {5, 6}};
  std::set<std::pair<int32_t, int32_t>> present;
  for (const Premise &premise : mapping.source)
    if (premise.literals.size() == 2 && premise.literals[0] < 0 &&
        premise.literals[1] < 0) {
      int32_t a = -premise.literals[0], b = -premise.literals[1];
      if (a > b) std::swap(a, b);
      present.insert({a, b});
    }
  for (unsigned a = 0; a < rows.size(); ++a)
    for (unsigned b = a + 1; b < rows.size(); ++b)
      for (int32_t x : rows[a])
        for (int32_t y : rows[b]) {
          auto pair = std::minmax(x, y);
          if (!present.count(pair))
            mapping.add({-x, -y});
        }
  assert(recognize(mapping.source, limits(), nullptr, nullptr, sentinel) ==
         Status::Ambiguous);

  Fixture missing = php(3);
  missing.source.pop_back();
  assert(recognize(missing.source, limits(), nullptr, nullptr, sentinel) ==
         Status::NotRecognized);
  assert(sentinel.variables == DirectMatrix{{77}});
}

void malformed_inputs() {
  Image sentinel;
  sentinel.variables = {{88}};
  auto expect = [&](Fixture fixture) {
    assert(recognize(fixture.source, limits(), nullptr, nullptr, sentinel) ==
           Status::Malformed);
    assert(sentinel.variables == DirectMatrix{{88}});
  };
  Fixture duplicate = php(2);
  Premise copy = duplicate.source.front();
  copy.occurrence = 999;
  copy.native_proof_id = 1999;
  duplicate.source.push_back(copy);
  expect(std::move(duplicate));

  Fixture occurrence = php(2);
  occurrence.source.back().occurrence = occurrence.source.front().occurrence;
  expect(std::move(occurrence));
  Fixture proof = php(2);
  proof.source.back().native_proof_id = proof.source.front().native_proof_id;
  expect(std::move(proof));
  Fixture zero_id = php(2);
  zero_id.source.front().native_proof_id = 0;
  expect(std::move(zero_id));
  Fixture repeated = php(2);
  repeated.source.front().literals = {1, 1};
  expect(std::move(repeated));
  Fixture tautology = php(2);
  tautology.source.front().literals = {1, -1};
  expect(std::move(tautology));
  Fixture zero_literal = php(2);
  zero_literal.source.front().literals = {0, 2};
  expect(std::move(zero_literal));
  Fixture empty = php(2);
  empty.source.front().literals.clear();
  expect(std::move(empty));
}

void limits_and_interruption_are_transactional() {
  const Fixture fixture = php(4);
  Image sentinel;
  sentinel.variables = {{99}};
  auto limited = [&](auto change) {
    Limits small = limits();
    change(small);
    assert(recognize(fixture.source, small, nullptr, nullptr, sentinel) ==
           Status::LimitExceeded);
    assert(sentinel.variables == DirectMatrix{{99}});
  };
  limited([&](Limits &l) { l.maximum_clauses = fixture.source.size() - 1; });
  limited([&](Limits &l) { l.maximum_literals = 3; });
  limited([&](Limits &l) { l.maximum_work = 10; });
  limited([&](Limits &l) { l.maximum_row_candidates = 3; });
  limited([&](Limits &l) { l.maximum_row_combinations = 0; });
  limited([&](Limits &l) { l.maximum_mapping_trials = 1; });
  limited([&](Limits &l) { l.maximum_holes = 0; });

  uint64_t remaining = 25;
  auto charge = [](void *context, uint64_t amount) {
    uint64_t &left = *static_cast<uint64_t *>(context);
    if (amount > left)
      return false;
    left -= amount;
    return true;
  };
  assert(recognize(fixture.source, limits(), charge, &remaining, sentinel) ==
         Status::Interrupted);
  assert(sentinel.variables == DirectMatrix{{99}});
}

void target_scale_pipeline_contract() {
  const Fixture fixture = php(22);
  Image image;
  assert(recognize(fixture.source, limits(), nullptr, nullptr, image) ==
         Status::Recognized);
  check_image(image, 22);
  assert(image.census.premises.size() == 5589);
  tactix::direct_php_certificate::Accounting accounting;
  assert(tactix::direct_php_certificate::preflight(
             image.variables, 507, tactix::direct_php_certificate::Limits{},
             accounting) == tactix::direct_php_certificate::Status::Ok);
  assert(accounting.additions == 24565 &&
         accounting.fresh_variables == 4907);
}

} // namespace

int main() {
  exhaustive_small_isomorphisms();
  noise_and_exact_census();
  ambiguity_and_near_misses();
  malformed_inputs();
  limits_and_interruption_are_transactional();
  target_scale_pipeline_contract();
  std::puts("test_direct_php_recognition: passed");
}
