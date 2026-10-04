#undef NDEBUG
#include "rules/semantic_extension/affine_image.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <map>

using namespace tactix::affine_semantic_extension;
using tactix::parity_certificate::SmallClause;

static bool satisfied(const std::vector<External> &clause, uint32_t mask) {
  for (External literal : clause) {
    const uint32_t variable = uint32_t(literal < 0 ? -literal : literal) - 1;
    const bool value = mask & (uint32_t{1} << variable);
    if (value == (literal > 0)) return true;
  }
  return false;
}

static std::vector<Premise> encoded(const std::vector<External> &variables,
                                    bool rhs, uint64_t &occurrence) {
  std::vector<Premise> result;
  for (auto clause : clausal_form(variables, rhs))
    result.push_back({occurrence++, std::move(clause)});
  return result;
}

static Limits limits() {
  return {1000, 100, 1000, 1000000, 8, 16, 16, 4};
}

static bool same(const SmallClause &a, const SmallClause &b) {
  if (a.size != b.size) return false;
  std::vector<External> left(a.lits.begin(), a.lits.begin() + a.size);
  std::vector<External> right(b.lits.begin(), b.lits.begin() + b.size);
  std::sort(left.begin(), left.end());
  std::sort(right.begin(), right.end());
  return left == right;
}

static bool rup(const std::vector<SmallClause> &database,
                const SmallClause &candidate) {
  std::map<External, int> assignment;
  for (uint32_t i = 0; i < candidate.size; ++i) {
    const External literal = candidate.lits[i];
    const External variable = literal < 0 ? -literal : literal;
    const int value = literal > 0 ? -1 : 1;
    if (assignment[variable] && assignment[variable] != value) return true;
    assignment[variable] = value;
  }
  bool changed = true;
  while (changed) {
    changed = false;
    for (const SmallClause &clause : database) {
      bool sat = false;
      uint32_t open = 0;
      External unit = 0;
      for (uint32_t i = 0; i < clause.size; ++i) {
        const External literal = clause.lits[i];
        const External variable = literal < 0 ? -literal : literal;
        const int value = assignment[variable];
        if (value && (value > 0) == (literal > 0)) { sat = true; break; }
        if (!value) { ++open; unit = literal; }
      }
      if (sat) continue;
      if (!open) return true;
      if (open == 1) {
        const External variable = unit < 0 ? -unit : unit;
        const int value = unit > 0 ? 1 : -1;
        if (assignment[variable] && assignment[variable] != value) return true;
        if (!assignment[variable]) { assignment[variable] = value; changed = true; }
      }
    }
  }
  return false;
}

static SmallClause small(const std::vector<External> &clause) {
  SmallClause result;
  assert(clause.size() <= result.lits.size());
  result.size = static_cast<uint8_t>(clause.size());
  std::copy(clause.begin(), clause.end(), result.lits.begin());
  return result;
}

static void verify_plan(const Image &image, size_t index,
                        const ProofPlan &plan) {
  const Consequence &consequence = image.consequences[index];
  std::vector<SmallClause> database;
  for (size_t source : consequence.sources)
    for (const auto &clause : image.equations[source].premise_clauses)
      database.push_back(small(clause));
  for (const ProofEvent &event : plan.events) {
    if (event.addition) {
      assert(rup(database, event.clause));
      database.push_back(event.clause);
    } else {
      const auto found = std::find_if(
          database.begin(), database.end(),
          [&](const auto &c) { return same(c, event.clause); });
      assert(found != database.end());
      database.erase(found);
    }
  }
  for (const auto &target : plan.targets) {
    const SmallClause clause = small(target);
    assert(std::find_if(database.begin(), database.end(), [&](const auto &c) {
             return same(c, clause);
           }) != database.end());
  }
}

int main() {
  // Exact recovery covers the retired constructor's default arities, and each
  // recovered row has exactly the CNF semantics it declares.
  for (uint32_t width = 1; width <= 8; ++width) for (bool rhs : {false, true}) {
    std::vector<External> variables;
    for (uint32_t i = 1; i <= width; ++i) variables.push_back(i);
    uint64_t occurrence = 1;
    auto input = encoded(variables, rhs, occurrence);
    Image image;
    assert(recognize(input, limits(), [](uint64_t) { return true; }, image) ==
           Status::Complete);
    assert(image.equations.size() == 1);
    assert(image.equations[0].variables == variables);
    assert(image.equations[0].rhs == rhs);
    assert(image.equations[0].premises.size() == (uint32_t{1} << (width - 1)));
    for (uint32_t mask = 0; mask < (uint32_t{1} << width); ++mask) {
      bool cnf = true;
      for (const auto &premise : input) cnf &= satisfied(premise.literals, mask);
      assert(cnf == ((std::popcount(mask) & 1u) == unsigned(rhs)));
    }
  }

  // Missing structure, interruption, and admission failure are transactional.
  uint64_t occurrence = 1;
  auto incomplete = encoded({1, 2, 3}, false, occurrence);
  incomplete.pop_back();
  Image sentinel;
  sentinel.clauses_seen = 777;
  assert(recognize(incomplete, limits(), [](uint64_t) { return true; },
                   sentinel) == Status::NoStructure);
  assert(sentinel.clauses_seen == 777);
  auto complete = encoded({1, 2, 3}, false, occurrence);
  assert(recognize(complete, limits(), [](uint64_t) { return false; }, sentinel) ==
         Status::Interrupted);
  assert(sentinel.clauses_seen == 777);
  auto tiny = limits(); tiny.maximum_patterns = 1;
  assert(recognize(complete, tiny, [](uint64_t) { return true; }, sentinel) ==
         Status::Exhausted);
  assert(sentinel.clauses_seen == 777);
  complete[1].occurrence = complete[0].occurrence;
  assert(recognize(complete, limits(), [](uint64_t) { return true; }, sentinel) ==
         Status::Invalid);
  assert(sentinel.clauses_seen == 777);

  // Two width-five facts cancel to the useful Boolean relation x5 xor x6 = 1.
  occurrence = 100;
  auto first = encoded({1, 2, 3, 4, 5}, false, occurrence);
  auto second = encoded({1, 2, 3, 4, 6}, true, occurrence);
  first.insert(first.end(), second.begin(), second.end());
  Image image;
  assert(recognize(first, limits(), [](uint64_t) { return true; }, image) ==
         Status::Complete);
  assert(image.equations.size() == 2 && image.consequences.size() == 1);
  const Consequence &consequence = image.consequences[0];
  assert((consequence.variables == std::vector<External>{5, 6}));
  assert(consequence.rhs && consequence.sources.size() == 2);
  assert(consequence.premises.size() == 32);
  assert(image.identity_model_projection);
  const auto targets = clausal_form(consequence.variables, consequence.rhs);
  for (uint32_t mask = 0; mask < 64; ++mask) {
    bool source = true, target = true;
    for (const Premise &premise : first)
      source &= satisfied(premise.literals, mask);
    for (const auto &clause : targets) target &= satisfied(clause, mask);
    assert(!source || target);
  }

  // The proof planner is a bounded dry run: too small leaves its caller value
  // intact. A complete plan is independently replayed and every addition is
  // checked as RUP from the exact recorded source clauses.
  ProofPlan plan;
  plan.additions = 999;
  assert(!build_proof_plan(image, 0, 1, plan));
  assert(plan.additions == 999);
  assert(build_proof_plan(image, 0, 100000, plan));
  assert(plan.additions && plan.premises == consequence.premises);
  assert(plan.targets == targets);
  verify_plan(image, 0, plan);

  // Opposite complete encodings become a certified Boolean contradiction.
  occurrence = 1000;
  auto even = encoded({1, 2, 3, 4, 5}, false, occurrence);
  auto odd = encoded({1, 2, 3, 4, 5}, true, occurrence);
  even.insert(even.end(), odd.begin(), odd.end());
  Image contradiction;
  assert(recognize(even, limits(), [](uint64_t) { return true; },
                   contradiction) == Status::Complete);
  assert(contradiction.equations.size() == 2);
  assert(contradiction.consequences.size() == 1);
  assert(contradiction.consequences[0].variables.empty());
  assert(contradiction.consequences[0].rhs);
  assert(build_proof_plan(contradiction, 0, 100000, plan));
  assert(plan.targets.size() == 1 && plan.targets[0].empty());
  verify_plan(contradiction, 0, plan);

  std::puts("affine semantic image: exact arity-1..8 recognition, certified "
            "cancellation, and transactional bounds PASS");
}
