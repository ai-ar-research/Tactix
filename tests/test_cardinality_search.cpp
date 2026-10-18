#undef NDEBUG
#include "rules/semantic_extension/cardinality_search.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

using namespace tactix::cardinality_search;

namespace {

Constraint constraint(std::vector<int32_t> literals, uint32_t lower,
                      uint32_t upper) {
  Constraint result;
  result.literals = std::move(literals);
  result.lower = lower;
  result.upper = upper;
  const uint32_t size = result.literals.size();
  for (uint32_t mask = 1; mask < (uint32_t{1} << size); ++mask) {
    const uint32_t width = __builtin_popcount(mask);
    if ((lower && width == size - lower + 1) ||
        (upper < size && width == upper + 1)) {
      Clause clause;
      for (uint32_t at = 0; at < size; ++at)
        if (mask & (1u << at))
          clause.push_back(width == size - lower + 1 && lower
                               ? result.literals[at]
                               : -result.literals[at]);
      // Exact cardinality can have equal lower/upper encoding widths. Add
      // both polarities when the two source clause families coincide in width.
      result.premises.push_back(clause);
      if (lower && upper < size && size - lower + 1 == upper + 1) {
        for (int32_t &literal : clause) literal = -literal;
        result.premises.push_back(clause);
      }
    }
  }
  return result;
}

struct Sink {
  bool stopped = false;
  int32_t fresh = 7;
  uint64_t work = 0, nodes = 0, additions = 0;
  std::vector<std::pair<Clause, bool>> proof;
  bool charge_work(uint64_t amount) {
    if (work > 1000000 || amount > 1000000 - work) return stopped = true, false;
    work += amount;
    return true;
  }
  bool new_variable(int32_t &variable) {
    if (nodes == 10000 || fresh == INT32_MAX) return stopped = true, false;
    ++nodes;
    variable = ++fresh;
    return true;
  }
  bool add(const Clause &clause, bool rat) {
    if (additions == 100000) return stopped = true, false;
    ++additions;
    proof.emplace_back(clause, rat);
    return true;
  }
  bool record_definition(int32_t, int32_t, int64_t, int64_t) {
    return true;
  }
};

void write_case(const std::string &prefix,
                const std::vector<Constraint> &constraints,
                const Sink &sink, bool checker_closure) {
  std::vector<Clause> input;
  for (const auto &constraint : constraints)
    input.insert(input.end(), constraint.premises.begin(),
                 constraint.premises.end());
  if (checker_closure)
    input.insert(input.end(), {{6, 7}, {6, -7}, {-6, 7}, {-6, -7}});
  FILE *cnf = std::fopen((prefix + ".cnf").c_str(), "w");
  assert(cnf);
  std::fprintf(cnf, "p cnf 7 %zu\n", input.size());
  for (const Clause &clause : input) {
    for (int32_t literal : clause) std::fprintf(cnf, "%d ", literal);
    std::fputs("0\n", cnf);
  }
  std::fclose(cnf);
  FILE *proof = std::fopen((prefix + ".dsr").c_str(), "w");
  assert(proof);
  for (const auto &[clause, rat] : sink.proof) {
    (void)rat;
    for (int32_t literal : clause) std::fprintf(proof, "%d ", literal);
    std::fputs("0\n", proof);
  }
  if (checker_closure) {
    std::fputs("6 0\n-6 0\n0\n", proof);
  }
  std::fclose(proof);
}

uint32_t next_random(uint32_t &state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

bool cnf_satisfied(const std::vector<Constraint> &constraints,
                   uint32_t assignment) {
  for (const Constraint &constraint : constraints)
    for (const Clause &clause : constraint.premises) {
      bool satisfied = false;
      for (int32_t literal : clause)
        satisfied |= (((assignment >> (std::abs(literal) - 1)) & 1u) ==
                      (literal > 0));
      if (!satisfied) return false;
    }
  return true;
}

bool rup(const std::vector<Clause> &database, const Clause &addition) {
  std::array<int8_t, 6> values{};
  for (int32_t literal : addition) {
    const int32_t variable = std::abs(literal);
    assert(variable >= 1 && variable <= 5);
    const int8_t value = literal > 0 ? -1 : 1;
    if (values[variable] && values[variable] != value) return true;
    values[variable] = value;
  }
  bool changed = true;
  while (changed) {
    changed = false;
    for (const Clause &clause : database) {
      bool satisfied = false;
      int32_t unknown = 0;
      unsigned remaining = 0;
      for (int32_t literal : clause) {
        const int8_t value = values[std::abs(literal)];
        if (value == (literal > 0 ? 1 : -1)) {
          satisfied = true;
          break;
        }
        if (!value) {
          unknown = literal;
          ++remaining;
        }
      }
      if (satisfied) continue;
      if (!remaining) return true;
      if (remaining == 1) {
        const int8_t value = unknown > 0 ? 1 : -1;
        if (values[std::abs(unknown)] &&
            values[std::abs(unknown)] != value)
          return true;
        if (!values[std::abs(unknown)]) {
          values[std::abs(unknown)] = value;
          changed = true;
        }
      }
    }
  }
  return false;
}

void validate_rup_plan(const std::vector<Constraint> &constraints,
                       const Clause &reason) {
  const ResolutionPlan plan =
      plan_resolution(constraints, reason, ResolutionLimits{});
  assert(plan.status == (reason.empty() ? Status::Conflict
                                        : Status::Implication));
  assert(!plan.additions.empty());
  std::vector<Clause> database;
  for (const auto &constraint : constraints)
    database.insert(database.end(), constraint.premises.begin(),
                    constraint.premises.end());
  for (const Clause &addition : plan.additions) {
    assert(rup(database, addition));
    database.push_back(addition);
  }
  Clause target = reason;
  std::sort(target.begin(), target.end());
  assert(plan.additions.back() == target);
}

void differential_fuzz() {
  uint32_t random = 0x52a41d97u;
  for (unsigned trial = 0; trial < 1000; ++trial) {
    std::vector<Constraint> constraints;
    std::set<int32_t> support;
    const unsigned count = 1 + next_random(random) % 3;
    for (unsigned c = 0; c < count; ++c) {
      std::vector<int32_t> literals;
      const unsigned width = 2 + next_random(random) % 3;
      while (literals.size() < width) {
        const int32_t variable = 1 + next_random(random) % 5;
        if (support.count(100 * c + variable)) continue;
        support.insert(100 * c + variable);
        literals.push_back((next_random(random) & 1u) ? variable : -variable);
      }
      const uint32_t lower = next_random(random) % (width + 1);
      uint32_t upper = lower + next_random(random) % (width - lower + 1);
      if (!lower && upper == width) upper = width - 1;
      constraints.push_back(constraint(literals, lower, upper));
    }
    std::vector<int32_t> trail;
    for (int32_t variable = 1; variable <= 5; ++variable)
      if (next_random(random) % 3 == 0)
        trail.push_back((next_random(random) & 1u) ? variable : -variable);
    const Result result = inspect(constraints, trail, Limits{});
    assert(result.status == Status::NoEvent ||
           result.status == Status::Conflict ||
           result.status == Status::Implication);
    unsigned compatible = 0;
    std::set<int32_t> actual_support;
    for (const Constraint &constraint : constraints)
      for (int32_t literal : constraint.literals)
        actual_support.insert(std::abs(literal));
    int32_t smallest_forced = 0;
    for (uint32_t mask = 0; mask < 32; ++mask) {
      if (!cnf_satisfied(constraints, mask)) continue;
      bool matches = true;
      for (int32_t literal : trail)
        matches &= (((mask >> (std::abs(literal) - 1)) & 1u) ==
                    (literal > 0));
      if (matches) ++compatible;
      bool reason_satisfied = false;
      for (int32_t literal : result.reason)
        reason_satisfied |= (((mask >> (std::abs(literal) - 1)) & 1u) ==
                             (literal > 0));
      if (result.status != Status::NoEvent) assert(reason_satisfied);
    }
    if (!compatible) assert(result.status == Status::Conflict);
    else if (result.status == Status::Conflict) assert(false);
    if (compatible) {
      for (int32_t variable : actual_support) {
        if (std::find(trail.begin(), trail.end(), variable) != trail.end() ||
            std::find(trail.begin(), trail.end(), -variable) != trail.end())
          continue;
        bool positive = false, negative = false;
        for (uint32_t mask = 0; mask < 32; ++mask) {
          if (!cnf_satisfied(constraints, mask)) continue;
          bool matches = true;
          for (int32_t literal : trail)
            matches &= (((mask >> (std::abs(literal) - 1)) & 1u) ==
                        (literal > 0));
          if (!matches) continue;
          positive |= (mask >> (variable - 1)) & 1u;
          negative |= !((mask >> (variable - 1)) & 1u);
        }
        if (positive != negative) {
          smallest_forced = variable;
          break;
        }
      }
      assert((result.status == Status::Implication) ==
             (smallest_forced != 0));
    }
    if (result.status == Status::Implication) {
      assert(!result.reason.empty());
      const int32_t implied = result.reason.back();
      assert(std::find(trail.begin(), trail.end(), implied) == trail.end());
      assert(std::find(trail.begin(), trail.end(), -implied) == trail.end());
      assert(std::abs(implied) == smallest_forced);
    }
    if (result.status != Status::NoEvent)
      validate_rup_plan(constraints, result.reason);
  }
}

} // namespace

int main(int argc, char **argv) {
  const std::vector<Constraint> overlap{
      constraint({1, 2, 3, 4}, 2, 4),
      constraint({2, 3, 4, 5}, 0, 2)};
  for (const Constraint &source : overlap) {
    const ConstructResult built = construct(
        source.literals, source.lower, source.upper, Limits{});
    assert(built.status == ConstructStatus::Constructed);
    auto expected = source.premises;
    auto actual = built.constraint.premises;
    for (Clause &clause : expected) std::sort(clause.begin(), clause.end());
    std::sort(expected.begin(), expected.end());
    std::sort(actual.begin(), actual.end());
    assert(actual == expected);
  }
  assert(construct({1, 1}, 1, 1, Limits{}).status ==
         ConstructStatus::Invalid);
  const Result implied = inspect(overlap, {5, 7}, Limits{});
  assert(implied.status == Status::Implication);
  assert((implied.reason == Clause{-5, 1}));
  Sink implication_proof;
  assert(certify(overlap, implied.reason, implication_proof));
  assert(!implication_proof.stopped && implication_proof.additions > 0);
  const ResolutionPlan implication_resolution =
      plan_resolution(overlap, implied.reason, ResolutionLimits{});
  assert(implication_resolution.status == Status::Implication &&
         !implication_resolution.additions.empty() &&
         implication_resolution.additions.back() == implied.reason);

  const std::vector<Constraint> odd{
      constraint({1, 2}, 1, 1), constraint({2, 3}, 1, 1),
      constraint({1, 3}, 1, 1)};
  const Result conflict = inspect(odd, {}, Limits{});
  assert(conflict.status == Status::Conflict && conflict.reason.empty());
  Sink conflict_proof;
  assert(certify(odd, conflict.reason, conflict_proof));
  assert(!conflict_proof.stopped && conflict_proof.additions > 0);
  const ResolutionPlan conflict_resolution =
      plan_resolution(odd, conflict.reason, ResolutionLimits{});
  assert(conflict_resolution.status == Status::Conflict &&
         !conflict_resolution.additions.empty() &&
         conflict_resolution.additions.back().empty());

  auto missing = overlap;
  missing[0].premises.pop_back();
  assert(inspect(missing, {5}, Limits{}).status == Status::Invalid);
  Limits tight;
  tight.maximum_support_variables = 4;
  assert(inspect(overlap, {5}, tight).status == Status::ResourceExhausted);
  assert(inspect(overlap, {5, -5}, Limits{}).status == Status::Invalid);
  const Result base_meter = inspect(overlap, {5}, Limits{});
  assert(base_meter.status == Status::Implication);
  std::vector<int32_t> irrelevant_trail{5};
  for (int32_t variable = 100; variable < 110; ++variable)
    irrelevant_trail.push_back(variable);
  Limits trail_meter;
  trail_meter.maximum_work = base_meter.work + 9;
  assert(inspect(overlap, irrelevant_trail, trail_meter).status ==
         Status::ResourceExhausted);
  differential_fuzz();

  if (argc == 2) {
    write_case(std::string(argv[1]) + "/implication", overlap,
               implication_proof, true);
    write_case(std::string(argv[1]) + "/conflict", odd,
               conflict_proof, false);
    Sink implication_rup;
    for (const Clause &clause : implication_resolution.additions)
      assert(implication_rup.add(clause, false));
    write_case(std::string(argv[1]) + "/implication-resolution", overlap,
               implication_rup, true);
    Sink conflict_rup;
    for (const Clause &clause : conflict_resolution.additions)
      assert(conflict_rup.add(clause, false));
    write_case(std::string(argv[1]) + "/conflict-resolution", odd,
               conflict_rup, false);
  } else assert(argc == 1);
  std::puts("test_cardinality_search: passed");
}
