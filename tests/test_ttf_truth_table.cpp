#undef NDEBUG
#include "rules/semantic_extension/ttf_truth_table.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <map>
#include <vector>

using namespace tactix::ttf_truth_table;

namespace {

struct Fixture {
  std::vector<Premise> source;
  std::vector<uint64_t> shared;
  uint64_t next = 1;

  uint64_t add(std::vector<int32_t> literals) {
    const uint64_t id = next++;
    source.push_back({id, std::move(literals), 10000 + id});
    return id;
  }

  std::vector<uint64_t> gate(int32_t root,
                             const std::vector<int32_t> &inputs) {
    std::vector<uint64_t> ids;
    for (int32_t input : inputs) ids.push_back(add({-input, root}));
    std::vector<int32_t> backward = inputs;
    backward.push_back(-root);
    ids.push_back(add(std::move(backward)));
    return ids;
  }
};

struct Built {
  Fixture fixture;
  std::vector<std::vector<int32_t>> stars;
};

Built build() {
  Built built;
  built.stars = {
      {1, 10, 11, 12, 13}, {2, 10, 11, 14, 15},
      {3, 12, 13, 14, 15}, {101, 110, 111, 112, 113},
      {102, 110, 111, 114, 115}, {103, 112, 113, 114, 115},
  };
  Fixture &f = built.fixture;
  for (uint32_t d = 0; d < 3; ++d) {
    const int32_t root = 500 + d;
    const auto definition = f.gate(root, {600 + int32_t(2 * d),
                                          601 + int32_t(2 * d)});
    f.add({root});
    if (!d) f.shared.push_back(definition.back());
  }
  for (uint32_t coordinate = 0; coordinate < 2; ++coordinate)
    for (uint32_t domain = 0; domain < 3; ++domain) {
      const uint32_t s = coordinate * 3 + domain;
      const int32_t forced = 500 + domain;
      const int32_t x = 700 + int32_t(2 * domain);
      const int32_t y = x + 1;
      const bool has_participation = domain < 2;
      int32_t participation = 0;
      if (has_participation) {
        participation = 800 + int32_t(s);
        f.gate(participation, built.stars[s]);
      }
      f.add({-built.stars[s][0]});
      const bool forced_nonzero = coordinate == 0 || domain != 1;
      const auto add_nonzero = [&](int32_t atom) {
        auto clause = built.stars[s];
        clause.push_back(-atom);
        f.add(std::move(clause));
      };
      const auto add_zero = [&](int32_t atom) {
        if (has_participation) {
          f.add({-participation, -atom});
        } else {
          for (int32_t variable : built.stars[s])
            f.add({-atom, -variable});
        }
      };
      if (forced_nonzero)
        add_nonzero(forced);
      else
        add_zero(forced);
      add_nonzero(x);
      if (coordinate == 1 && domain == 2)
        add_nonzero(y);
      else
        add_zero(y);
    }
  // A declared AMO-owned residual occurrence exercises complete multi-owner
  // census without pretending this truth-table layer owns its semantics.
  built.fixture.shared.push_back(built.fixture.add({-500, -900, -901}));
  std::reverse(built.fixture.source.begin(), built.fixture.source.end());
  for (Premise &premise : built.fixture.source)
    std::reverse(premise.literals.begin(), premise.literals.end());
  return built;
}

Limits limits() { return {1000, 20000, 100000, 1000, 20, 10, 20, 20}; }

bool rup(const std::vector<Premise> &source,
         const PlannedAddition &addition) {
  std::map<int32_t, bool> assignment;
  for (int32_t literal : addition.clause)
    assignment[literal < 0 ? -literal : literal] = literal < 0;
  for (;;) {
    bool changed = false;
    for (uint64_t occurrence : addition.rup_premises) {
      const auto found = std::find_if(
          source.begin(), source.end(), [occurrence](const Premise &premise) {
            return premise.occurrence == occurrence;
          });
      assert(found != source.end());
      uint32_t unassigned = 0;
      int32_t unit = 0;
      bool satisfied = false;
      for (int32_t literal : found->literals) {
        const int32_t variable = literal < 0 ? -literal : literal;
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
        const int32_t variable = unit < 0 ? -unit : unit;
        const auto [position, inserted] =
            assignment.emplace(variable, unit > 0);
        if (!inserted && position->second != (unit > 0)) return true;
        changed |= inserted;
      }
    }
    if (!changed) return false;
  }
}

void recognizes_exact_grammar_and_plans_odd_frontier() {
  const Built built = build();
  uint64_t charged = 0;
  auto charge = [](void *context, uint64_t amount) {
    *static_cast<uint64_t *>(context) += amount;
    return true;
  };
  Image image;
  assert(recognize(built.fixture.source, built.fixture.shared, limits(), charge,
                   &charged, image) == Status::Recognized);
  assert(image.stars.size() == 6 && image.domains.size() == 3);
  assert(image.coordinates.size() == 2 && image.root_census.size() == 7);
  assert(image.inspected_work == charged);
  bool saw_shared_definition = false, saw_shared_residual = false;
  for (const RootCensus &census : image.root_census)
    for (const CensusEntry &entry : census.occurrences) {
      saw_shared_definition |=
          census.root == 500 && entry.shared_with_amo &&
          entry.use == RootUse::Definition;
      saw_shared_residual |=
          census.root == 500 && entry.shared_with_amo &&
          entry.use == RootUse::SharedAmo;
    }
  assert(saw_shared_definition && saw_shared_residual);

  ParityFrontierPlan plan;
  assert(plan_parity_frontier(image, limits(), nullptr, nullptr, plan) ==
         Status::Recognized);
  assert(plan.additions.size() == 3);
  for (const PlannedAddition &addition : plan.additions) {
    assert(addition.clause.size() == 4);
    assert(addition.rup_premises.size() == 3);
    assert(rup(built.fixture.source, addition));
  }
  assert(plan.required_amo_leaf_sets.size() == 3);
}

void incomplete_grammar_and_undeclared_occurrences_decline() {
  Built built = build();
  Image sentinel;
  sentinel.inspected_work = 1234;
  const auto missing = std::find_if(
      built.fixture.source.begin(), built.fixture.source.end(),
      [](const Premise &p) {
        auto clause = p.literals;
        std::sort(clause.begin(), clause.end());
        return clause == std::vector<int32_t>({-705, -13});
      });
  assert(missing != built.fixture.source.end());
  built.fixture.source.erase(missing);
  assert(recognize(built.fixture.source, built.fixture.shared, limits(), nullptr,
                   nullptr, sentinel) == Status::NotRecognized);
  assert(sentinel.inspected_work == 1234 && sentinel.stars.empty());

  built = build();
  built.fixture.shared.pop_back();
  assert(recognize(built.fixture.source, built.fixture.shared, limits(), nullptr,
                   nullptr, sentinel) == Status::NotRecognized);
  assert(sentinel.inspected_work == 1234 && sentinel.stars.empty());

  built = build();
  built.fixture.source.front().native_proof_id =
      built.fixture.source.back().native_proof_id;
  assert(recognize(built.fixture.source, built.fixture.shared, limits(), nullptr,
                   nullptr, sentinel) == Status::NotRecognized);
  assert(sentinel.inspected_work == 1234 && sentinel.stars.empty());
}

void budgets_and_interruption_are_transactional() {
  const Built built = build();
  Image sentinel;
  sentinel.inspected_work = 1234;
  uint64_t remaining = 10;
  auto charge = [](void *context, uint64_t amount) {
    uint64_t &left = *static_cast<uint64_t *>(context);
    if (amount > left) return false;
    left -= amount;
    return true;
  };
  assert(recognize(built.fixture.source, built.fixture.shared, limits(), charge,
                   &remaining, sentinel) == Status::Interrupted);
  assert(sentinel.inspected_work == 1234 && sentinel.stars.empty());

  Image image;
  assert(recognize(built.fixture.source, built.fixture.shared, limits(), nullptr,
                   nullptr, image) == Status::Recognized);
  ParityFrontierPlan sentinel_plan;
  sentinel_plan.coordinate = 99;
  Limits small = limits();
  small.maximum_plan_additions = 2;
  assert(plan_parity_frontier(image, small, nullptr, nullptr, sentinel_plan) ==
         Status::LimitExceeded);
  assert(sentinel_plan.coordinate == 99 && sentinel_plan.additions.empty());
}

} // namespace

int main() {
  recognizes_exact_grammar_and_plans_odd_frontier();
  incomplete_grammar_and_undeclared_occurrences_decline();
  budgets_and_interruption_are_transactional();
}
