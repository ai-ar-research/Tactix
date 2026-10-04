#undef NDEBUG
#include "proof/direct_php_certificate.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace tactix::direct_php_certificate;

namespace {

Matrix make_matrix(unsigned pigeons, unsigned holes, int32_t first = 1) {
  Matrix result(pigeons, std::vector<int32_t>(holes));
  for (auto &row : result)
    for (int32_t &variable : row)
      variable = first++;
  return result;
}

std::vector<Clause> premises(const Matrix &x) {
  std::vector<Clause> result;
  const unsigned pigeons = x.size(), holes = x[0].size();
  for (const auto &row : x)
    result.push_back(row);
  for (unsigned hole = 0; hole < holes; ++hole)
    for (unsigned first = 0; first < pigeons; ++first)
      for (unsigned second = first + 1; second < pigeons; ++second)
        result.push_back({-x[first][hole], -x[second][hole]});
  return result;
}

bool unit_conflict(const std::vector<Clause> &formula,
                   const std::vector<int32_t> &assumptions) {
  std::unordered_map<int32_t, bool> values;
  auto assign = [&](int32_t literal) {
    const int32_t variable = literal < 0 ? -literal : literal;
    const bool value = literal > 0;
    const auto [position, fresh] = values.emplace(variable, value);
    return fresh || position->second == value;
  };
  for (int32_t literal : assumptions)
    if (!assign(literal))
      return true;
  bool changed = true;
  while (changed) {
    changed = false;
    for (const Clause &clause : formula) {
      bool satisfied = false;
      int32_t open = 0;
      unsigned open_count = 0;
      for (int32_t literal : clause) {
        const int32_t variable = literal < 0 ? -literal : literal;
        const auto found = values.find(variable);
        if (found == values.end()) {
          open = literal;
          ++open_count;
        } else if (found->second == (literal > 0)) {
          satisfied = true;
          break;
        }
      }
      if (satisfied)
        continue;
      if (!open_count)
        return true;
      if (open_count == 1) {
        if (!assign(open))
          return true;
        changed = true;
      }
    }
  }
  return false;
}

bool rup(const std::vector<Clause> &formula, const Clause &clause) {
  std::vector<int32_t> assumptions;
  assumptions.reserve(clause.size());
  for (int32_t literal : clause)
    assumptions.push_back(-literal);
  return unit_conflict(formula, assumptions);
}

Clause resolvent(const Clause &added, const Clause &opposite, int32_t pivot,
                 bool &tautology) {
  std::set<int32_t> result;
  tautology = false;
  auto insert = [&](int32_t literal) {
    if (result.count(-literal)) {
      tautology = true;
      return;
    }
    result.insert(literal);
  };
  for (int32_t literal : added)
    if (literal != pivot)
      insert(literal);
  for (int32_t literal : opposite)
    if (literal != -pivot)
      insert(literal);
  return Clause(result.begin(), result.end());
}

bool rat(const std::vector<Clause> &formula, const Clause &clause) {
  assert(!clause.empty());
  const int32_t pivot = clause.front();
  for (const Clause &candidate : formula) {
    if (std::find(candidate.begin(), candidate.end(), -pivot) ==
        candidate.end())
      continue;
    bool tautology = false;
    const Clause r = resolvent(clause, candidate, pivot, tautology);
    if (!tautology && !rup(formula, r))
      return false;
  }
  return true;
}

void erase_one(std::vector<Clause> &formula, const Clause &clause) {
  const auto found = std::find(formula.begin(), formula.end(), clause);
  assert(found != formula.end());
  formula.erase(found);
}

void check_certificate(const Matrix &x, const Plan &plan) {
  std::vector<Clause> formula = premises(x);
  uint64_t additions = 0, deletions = 0, add_literals = 0,
           delete_literals = 0;
  for (const Event &event : plan.events) {
    if (event.kind == EventKind::Delete) {
      erase_one(formula, event.clause);
      ++deletions;
      delete_literals += event.clause.size();
      continue;
    }
    if (event.kind == EventKind::AddRup)
      assert(rup(formula, event.clause));
    else
      assert(rat(formula, event.clause));
    formula.push_back(event.clause);
    ++additions;
    add_literals += event.clause.size();
  }
  assert(!plan.events.empty());
  assert(plan.events.back().kind == EventKind::AddRup);
  assert(plan.events.back().role == EventRole::Conclusion);
  assert(plan.events.back().clause.empty());
  assert(additions == plan.accounting.additions);
  assert(deletions == plan.accounting.deletions);
  assert(add_literals == plan.accounting.addition_literals);
  assert(delete_literals == plan.accounting.deletion_literals);
}

uint64_t paper_additions(unsigned holes) {
  if (holes == 1)
    return 1;
  uint64_t total = 1;
  for (uint64_t k = holes - 1; k >= 1; --k) {
    const uint64_t groups = k == 1 ? 1 : (7 * k) / 2 - 4;
    total += k * (4 * k + 2) + k * groups + k + 1;
    if (k == 1)
      break;
  }
  return total;
}

void exhaustive_tiny_certificates() {
  // Every generated RUP/RAT event is checked against the exact formula state,
  // including deletion replay.  Extra pigeons exercise core selection.
  for (unsigned holes = 1; holes <= 5; ++holes) {
    for (unsigned extra = 0; extra <= 2; ++extra) {
      Matrix x = make_matrix(holes + 1 + extra, holes, 11);
      if ((holes + extra) & 1)
        std::reverse(x.begin(), x.end());
      int32_t maximum = 0;
      for (const auto &row : x)
        maximum = std::max(maximum, *std::max_element(row.begin(), row.end()));
      Accounting counted;
      assert(preflight(x, maximum + 7, Limits{}, counted) == Status::Ok);
      Plan plan_result;
      assert(plan(x, maximum + 7, plan_result) == Status::Ok);
      assert(plan_result.accounting == counted);
      assert(plan_result.accounting.additions == paper_additions(holes));
      assert(plan_result.core_pigeons == holes + 1);
      assert(plan_result.input_pigeons == holes + 1 + extra);
      check_certificate(x, plan_result);
    }
  }
}

void exact_accounting_and_limits() {
  const Matrix x = make_matrix(23, 22);
  const int32_t first_fresh = 507;
  Accounting accounting;
  assert(preflight(x, first_fresh, Limits{}, accounting) == Status::Ok);
  assert(accounting.additions == 24565);
  assert(accounting.deletions == 30121);
  assert(accounting.fresh_variables == 4907);
  assert(accounting.rup_additions + accounting.rat_additions ==
         accounting.additions);
  assert(accounting.events == accounting.additions + accounting.deletions);
  assert(accounting.next_fresh ==
         uint64_t(first_fresh) + accounting.fresh_variables);
  Plan full;
  assert(plan(x, first_fresh, full) == Status::Ok);
  assert(full.accounting == accounting);
  assert(full.events.size() == accounting.events);

  Limits limits;
  limits.maximum_additions = accounting.additions - 1;
  Accounting untouched{17};
  assert(preflight(x, first_fresh, limits, untouched) == Status::LimitExceeded);
  assert(untouched.additions == 17);
  limits = Limits{};
  limits.maximum_deletions = accounting.deletions - 1;
  assert(preflight(x, first_fresh, limits, untouched) == Status::LimitExceeded);
  limits = Limits{};
  limits.maximum_literals =
      accounting.addition_literals + accounting.deletion_literals - 1;
  assert(preflight(x, first_fresh, limits, untouched) == Status::LimitExceeded);
  limits = Limits{};
  limits.maximum_fresh_variables = accounting.fresh_variables - 1;
  assert(preflight(x, first_fresh, limits, untouched) == Status::LimitExceeded);
  limits = Limits{};
  limits.maximum_events = accounting.events - 1;
  assert(preflight(x, first_fresh, limits, untouched) == Status::LimitExceeded);
}

void negative_normalization() {
  Accounting accounting;
  assert(preflight({}, 1, Limits{}, accounting) == Status::NotOverfull);
  assert(preflight(Matrix(1), 1, Limits{}, accounting) == Status::NoHoles);
  assert(preflight(make_matrix(2, 2), 5, Limits{}, accounting) ==
         Status::NotOverfull);

  Matrix ragged{{1, 2}, {3}, {4, 5}};
  assert(preflight(ragged, 6, Limits{}, accounting) == Status::RaggedMatrix);
  Matrix zero{{1, 2}, {3, 0}, {4, 5}};
  assert(preflight(zero, 6, Limits{}, accounting) == Status::InvalidVariable);
  Matrix negative{{1, 2}, {3, -4}, {5, 6}};
  assert(preflight(negative, 7, Limits{}, accounting) ==
         Status::InvalidVariable);
  Matrix repeated{{1, 2}, {3, 1}, {4, 5}};
  assert(preflight(repeated, 6, Limits{}, accounting) ==
         Status::RepeatedVariable);
  const Matrix valid = make_matrix(4, 3);
  assert(preflight(valid, 12, Limits{}, accounting) ==
         Status::FreshNamespaceCollision);
  assert(preflight(valid, 1, Limits{}, accounting) ==
         Status::FreshNamespaceCollision);

  Matrix near_limit = make_matrix(3, 2, std::numeric_limits<int32_t>::max() - 6);
  assert(preflight(near_limit, std::numeric_limits<int32_t>::max(), Limits{},
                   accounting) == Status::ArithmeticOverflow);
}

void atomic_output() {
  const Matrix x = make_matrix(5, 4);
  Plan output;
  output.holes = 91;
  output.events.push_back(Event{});
  Limits limits;
  limits.maximum_additions = 1;
  assert(plan(x, 21, limits, output) == Status::LimitExceeded);
  assert(output.holes == 91 && output.events.size() == 1);
}

} // namespace

int main() {
  exhaustive_tiny_certificates();
  exact_accounting_and_limits();
  negative_normalization();
  atomic_output();
  std::puts("test_direct_php_certificate: passed");
}
