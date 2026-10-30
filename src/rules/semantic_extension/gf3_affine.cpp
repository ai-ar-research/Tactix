#include "rules/semantic_extension/gf3_affine.hpp"

#include "kernel/types.hpp"

#include <algorithm>
#include <climits>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace tactix::gf3_affine {
namespace {

uint32_t variable(int32_t literal) {
  return external_variable_of(literal);
}

bool literal_less(int32_t first, int32_t second) {
  return std::pair(variable(first), first) <
         std::pair(variable(second), second);
}

std::vector<int32_t> normalized(std::vector<int32_t> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

bool add_count(uint64_t amount, uint64_t &total) {
  if (amount > UINT64_MAX - total) return false;
  total += amount;
  return true;
}

bool multiply_count(uint64_t first, uint64_t second, uint64_t &result) {
  if (first && second > UINT64_MAX / first) return false;
  result = first * second;
  return true;
}

bool power3(uint32_t exponent, uint64_t &result) {
  result = 1;
  while (exponent--) {
    if (result > UINT64_MAX / 3) return false;
    result *= 3;
  }
  return true;
}

uint8_t inverse(uint8_t value) { return value == 1 ? 1 : 2; }

uint8_t tuple_sum(uint64_t tuple, const std::vector<uint8_t> &coefficients) {
  uint8_t sum = 0;
  for (uint8_t coefficient : coefficients) {
    sum = static_cast<uint8_t>((sum + coefficient * (tuple % 3)) % 3);
    tuple /= 3;
  }
  return sum;
}

bool valid_clause(const std::vector<int32_t> &clause) {
  if (clause.empty()) return false;
  std::set<uint32_t> variables;
  for (int32_t literal : clause)
    if (!literal || literal == INT32_MIN ||
        !variables.insert(variable(literal)).second)
      return false;
  return true;
}

struct VectorLess {
  bool operator()(const std::vector<int32_t> &first,
                  const std::vector<int32_t> &second) const {
    return first < second;
  }
};

struct EquationGroup {
  std::vector<uint32_t> domains;
  std::map<uint64_t, uint64_t> tuple_occurrences;
};

const Premise *find_occurrence(const Theory &theory, uint64_t occurrence) {
  const auto found = std::lower_bound(
      theory.source.begin(), theory.source.end(), occurrence,
      [](const Premise &premise, uint64_t id) {
        return premise.occurrence < id;
      });
  return found != theory.source.end() && found->occurrence == occurrence
             ? &*found
             : nullptr;
}

bool structurally_valid_equation(const Theory &theory,
                                 const Equation &equation) {
  if (equation.domains.size() < 2 ||
      equation.domains.size() != equation.coefficients.size() ||
      equation.coefficients.front() != 1 || equation.rhs >= 3)
    return false;
  uint64_t tuples = 0;
  if (!power3(static_cast<uint32_t>(equation.domains.size()), tuples) ||
      equation.forbidden_by_tuple.size() != tuples)
    return false;
  for (size_t i = 0; i < equation.domains.size(); ++i) {
    if (equation.domains[i] >= theory.domains.size() ||
        (i && equation.domains[i - 1] >= equation.domains[i]) ||
        (equation.coefficients[i] != 1 && equation.coefficients[i] != 2))
      return false;
  }
  for (uint64_t tuple = 0; tuple < tuples; ++tuple) {
    const bool forbidden = tuple_sum(tuple, equation.coefficients) != equation.rhs;
    if (forbidden != bool(equation.forbidden_by_tuple[tuple])) return false;
    if (forbidden && !find_occurrence(theory,
                                      equation.forbidden_by_tuple[tuple]))
      return false;
  }
  return true;
}

} // namespace

RecognitionResult recognize(const std::vector<Premise> &source,
                            const RecognitionBounds &bounds, Charge charge) {
  RecognitionResult result;
  auto decline = [&](RecognitionStatus status) {
    result.status = status;
    result.theory = {};
    return result;
  };
  auto spend = [&](uint64_t amount) {
    if (!charge || !charge(amount)) return false;
    return add_count(amount, result.consumed_work);
  };

  if (!bounds.maximum_source_clauses || !bounds.maximum_source_literals ||
      !bounds.maximum_domains || !bounds.maximum_equations ||
      bounds.maximum_equation_arity < 2 ||
      !bounds.maximum_tuples_per_equation || source.empty() || !charge)
    return decline(RecognitionStatus::InvalidRequest);
  if (source.size() > bounds.maximum_source_clauses)
    return decline(RecognitionStatus::BoundExceeded);

  std::map<uint64_t, Premise> by_occurrence;
  std::map<std::vector<int32_t>, uint64_t, VectorLess> by_clause;
  for (const Premise &premise : source) {
    if (!spend(uint64_t{1} + premise.literals.size()))
      return decline(RecognitionStatus::EffortExhausted);
    if (!premise.occurrence || !valid_clause(premise.literals) ||
        !by_occurrence.emplace(premise.occurrence, premise).second)
      return decline(RecognitionStatus::IncompleteTheory);
    const std::vector<int32_t> clause = normalized(premise.literals);
    if (!by_clause.emplace(clause, premise.occurrence).second)
      return decline(RecognitionStatus::IncompleteTheory);
    ++result.consumed_source_clauses;
    if (!add_count(premise.literals.size(), result.consumed_source_literals) ||
        result.consumed_source_literals > bounds.maximum_source_literals)
      return decline(RecognitionStatus::BoundExceeded);
    for (int32_t literal : premise.literals)
      result.theory.maximum_variable =
          std::max(result.theory.maximum_variable, variable(literal));
  }

  std::vector<Domain> candidates;
  for (const auto &[occurrence, premise] : by_occurrence) {
    if (!spend(1)) return decline(RecognitionStatus::EffortExhausted);
    if (premise.literals.size() != 3) continue;
    std::vector<int32_t> values = premise.literals;
    std::sort(values.begin(), values.end(), literal_less);
    bool exact_one = true;
    Domain domain;
    domain.values = values;
    domain.at_least_one = occurrence;
    domain.at_most_one.assign(9, 0);
    for (size_t first = 0; first < 3 && exact_one; ++first)
      for (size_t second = first + 1; second < 3; ++second) {
        const std::vector<int32_t> amo =
            normalized({-values[first], -values[second]});
        const auto found = by_clause.find(amo);
        if (found == by_clause.end()) {
          exact_one = false;
          break;
        }
        domain.at_most_one[first * 3 + second] = found->second;
        domain.at_most_one[second * 3 + first] = found->second;
      }
    if (exact_one) candidates.push_back(std::move(domain));
  }
  if (candidates.empty()) return decline(RecognitionStatus::IncompleteTheory);
  std::sort(candidates.begin(), candidates.end(),
            [](const Domain &first, const Domain &second) {
              return first.values < second.values;
            });
  if (candidates.size() > bounds.maximum_domains)
    return decline(RecognitionStatus::BoundExceeded);

  std::map<uint32_t, std::pair<uint32_t, uint8_t>> value_index;
  std::set<uint64_t> used;
  for (uint32_t index = 0; index < candidates.size(); ++index) {
    const Domain &domain = candidates[index];
    if (!used.insert(domain.at_least_one).second)
      return decline(RecognitionStatus::IncompleteTheory);
    for (uint8_t value = 0; value < 3; ++value) {
      if (!value_index.emplace(variable(domain.values[value]),
                               std::pair(index, value)).second)
        return decline(RecognitionStatus::IncompleteTheory);
      for (uint8_t other = value + 1; other < 3; ++other)
        if (!used.insert(domain.at_most_one[value * 3 + other]).second)
          return decline(RecognitionStatus::IncompleteTheory);
    }
  }

  std::map<std::vector<uint32_t>, EquationGroup> groups;
  for (const auto &[occurrence, premise] : by_occurrence) {
    if (used.count(occurrence)) continue;
    if (!spend(uint64_t{1} + premise.literals.size()))
      return decline(RecognitionStatus::EffortExhausted);
    if (premise.literals.size() < 2 ||
        premise.literals.size() > bounds.maximum_equation_arity)
      return decline(RecognitionStatus::IncompleteTheory);
    std::vector<std::tuple<uint32_t, uint8_t>> entries;
    for (int32_t literal : premise.literals) {
      const auto found = value_index.find(variable(literal));
      if (found == value_index.end())
        return decline(RecognitionStatus::IncompleteTheory);
      const Domain &domain = candidates[found->second.first];
      if (literal != -domain.values[found->second.second])
        return decline(RecognitionStatus::IncompleteTheory);
      entries.emplace_back(found->second.first, found->second.second);
    }
    std::sort(entries.begin(), entries.end());
    std::vector<uint32_t> support;
    uint64_t tuple = 0, place = 1;
    for (const auto &[domain, value] : entries) {
      if (!support.empty() && support.back() == domain)
        return decline(RecognitionStatus::IncompleteTheory);
      support.push_back(domain);
      tuple += place * value;
      if (place > UINT64_MAX / 3)
        return decline(RecognitionStatus::BoundExceeded);
      place *= 3;
    }
    EquationGroup &group = groups[support];
    group.domains = support;
    if (!group.tuple_occurrences.emplace(tuple, occurrence).second)
      return decline(RecognitionStatus::IncompleteTheory);
  }
  if (groups.empty()) return decline(RecognitionStatus::IncompleteTheory);
  if (groups.size() > bounds.maximum_equations)
    return decline(RecognitionStatus::BoundExceeded);

  std::vector<Equation> equations;
  for (const auto &[support, group] : groups) {
    const uint32_t arity = static_cast<uint32_t>(support.size());
    uint64_t tuples = 0;
    if (!power3(arity, tuples) ||
        tuples > bounds.maximum_tuples_per_equation)
      return decline(RecognitionStatus::BoundExceeded);
    if (group.tuple_occurrences.size() != 2 * (tuples / 3))
      return decline(RecognitionStatus::IncompleteTheory);
    bool matched = false;
    Equation equation;
    const uint64_t coefficient_sets = uint64_t{1} << (arity - 1);
    for (uint64_t bits = 0; bits < coefficient_sets && !matched; ++bits) {
      if (!spend(arity)) return decline(RecognitionStatus::EffortExhausted);
      std::vector<uint8_t> coefficients(arity, 1);
      for (uint32_t i = 1; i < arity; ++i)
        coefficients[i] = static_cast<uint8_t>(1 + ((bits >> (i - 1)) & 1));
      for (uint8_t rhs = 0; rhs < 3 && !matched; ++rhs) {
        bool exact = true;
        for (uint64_t tuple = 0; tuple < tuples; ++tuple) {
          if (!spend(1)) return decline(RecognitionStatus::EffortExhausted);
          const bool forbidden = tuple_sum(tuple, coefficients) != rhs;
          if (forbidden != bool(group.tuple_occurrences.count(tuple))) {
            exact = false;
            break;
          }
        }
        if (!exact) continue;
        equation.domains = support;
        equation.coefficients = std::move(coefficients);
        equation.rhs = rhs;
        equation.forbidden_by_tuple.assign(tuples, 0);
        for (const auto &[tuple, occurrence] : group.tuple_occurrences)
          equation.forbidden_by_tuple[tuple] = occurrence;
        matched = true;
      }
    }
    if (!matched) return decline(RecognitionStatus::IncompleteTheory);
    for (uint64_t occurrence : equation.forbidden_by_tuple)
      if (occurrence) used.insert(occurrence);
    equations.push_back(std::move(equation));
  }
  if (used.size() != by_occurrence.size())
    return decline(RecognitionStatus::IncompleteTheory);

  result.theory.source.reserve(by_occurrence.size());
  for (auto &[occurrence, premise] : by_occurrence) {
    (void)occurrence;
    result.theory.source.push_back(std::move(premise));
  }
  result.theory.domains = std::move(candidates);
  result.theory.equations = std::move(equations);
  result.status = RecognitionStatus::Recognized;
  return result;
}

PairEliminationPlan
plan_pair_elimination(const Theory &theory, uint32_t first_equation,
                      uint32_t second_equation,
                      const PairEliminationBounds &bounds, Charge charge) {
  PairEliminationPlan result;
  result.first_equation = first_equation;
  result.second_equation = second_equation;
  auto decline = [&](PlanStatus status) {
    result.status = status;
    result.pivot_domain = 0;
    result.consequence = {};
    result.additions.clear();
    result.projection = {};
    return result;
  };
  auto spend = [&](uint64_t amount) {
    if (!charge || !charge(amount)) return false;
    return add_count(amount, result.consumed_work);
  };

  if (!charge || !bounds.maximum_result_arity ||
      !bounds.maximum_result_tuples || !bounds.maximum_additions ||
      !bounds.maximum_addition_literals || first_equation == second_equation ||
      first_equation >= theory.equations.size() ||
      second_equation >= theory.equations.size())
    return decline(PlanStatus::InvalidRequest);
  const Equation &first = theory.equations[first_equation];
  const Equation &second = theory.equations[second_equation];
  if (!spend(theory.source.size()))
    return decline(PlanStatus::EffortExhausted);
  for (size_t i = 0; i < theory.source.size(); ++i)
    if (!theory.source[i].occurrence ||
        (i && theory.source[i - 1].occurrence >=
                  theory.source[i].occurrence))
      return decline(PlanStatus::IncompleteTheory);
  uint64_t first_validation = 0, second_validation = 0,
           validation_work = 2;
  uint64_t first_tuple_work = 0, second_tuple_work = 0;
  if (!multiply_count(2, first.domains.size(), first_tuple_work) ||
      !add_count(2, first_tuple_work) ||
      !multiply_count(2, second.domains.size(), second_tuple_work) ||
      !add_count(2, second_tuple_work) ||
      !multiply_count(first.forbidden_by_tuple.size(), first_tuple_work,
                      first_validation) ||
      !multiply_count(second.forbidden_by_tuple.size(), second_tuple_work,
                      second_validation) ||
      !add_count(first_validation, validation_work) ||
      !add_count(second_validation, validation_work) ||
      !add_count(first.domains.size() + second.domains.size(),
                 validation_work))
    return decline(PlanStatus::BoundExceeded);
  if (!spend(validation_work))
    return decline(PlanStatus::EffortExhausted);
  if (!structurally_valid_equation(theory, first) ||
      !structurally_valid_equation(theory, second))
    return decline(PlanStatus::IncompleteTheory);

  std::set<uint32_t> involved_domains(first.domains.begin(),
                                      first.domains.end());
  involved_domains.insert(second.domains.begin(), second.domains.end());
  for (uint32_t domain_index : involved_domains) {
    const Domain &domain = theory.domains[domain_index];
    if (!spend(13)) return decline(PlanStatus::EffortExhausted);
    if (domain.values.size() != 3 || domain.at_most_one.size() != 9)
      return decline(PlanStatus::IncompleteTheory);
    std::set<uint32_t> variables;
    for (int32_t literal : domain.values)
      if (!literal || literal == INT32_MIN ||
          !variables.insert(variable(literal)).second)
        return decline(PlanStatus::IncompleteTheory);
    const Premise *alo = find_occurrence(theory, domain.at_least_one);
    if (!alo || normalized(alo->literals) != normalized(domain.values))
      return decline(PlanStatus::IncompleteTheory);
    for (size_t left = 0; left < 3; ++left) {
      if (domain.at_most_one[left * 3 + left])
        return decline(PlanStatus::IncompleteTheory);
      for (size_t right = left + 1; right < 3; ++right) {
        const uint64_t forward = domain.at_most_one[left * 3 + right];
        const uint64_t reverse = domain.at_most_one[right * 3 + left];
        const Premise *amo = find_occurrence(theory, forward);
        if (!forward || forward != reverse || !amo ||
            normalized(amo->literals) !=
                normalized({-domain.values[left], -domain.values[right]}))
          return decline(PlanStatus::IncompleteTheory);
      }
    }
  }
  const auto exact_equation_premises = [&](const Equation &equation) {
    for (uint64_t tuple = 0; tuple < equation.forbidden_by_tuple.size();
         ++tuple) {
      const uint64_t occurrence = equation.forbidden_by_tuple[tuple];
      if (!occurrence) continue;
      const Premise *premise = find_occurrence(theory, occurrence);
      if (!premise) return false;
      uint64_t rest = tuple;
      std::vector<int32_t> expected;
      for (uint32_t domain : equation.domains) {
        const uint8_t value = static_cast<uint8_t>(rest % 3);
        rest /= 3;
        expected.push_back(-theory.domains[domain].values[value]);
      }
      if (normalized(premise->literals) != normalized(std::move(expected)))
        return false;
    }
    return true;
  };
  if (!exact_equation_premises(first) || !exact_equation_premises(second))
    return decline(PlanStatus::IncompleteTheory);

  std::vector<uint32_t> shared;
  std::set_intersection(first.domains.begin(), first.domains.end(),
                        second.domains.begin(), second.domains.end(),
                        std::back_inserter(shared));
  if (shared.size() != 1) return decline(PlanStatus::UnsupportedOverlap);
  const uint32_t pivot = shared.front();
  result.pivot_domain = pivot;
  const auto first_pivot = std::lower_bound(first.domains.begin(),
                                            first.domains.end(), pivot);
  const auto second_pivot = std::lower_bound(second.domains.begin(),
                                             second.domains.end(), pivot);
  const size_t first_pivot_index = first_pivot - first.domains.begin();
  const size_t second_pivot_index = second_pivot - second.domains.begin();
  const uint8_t first_multiplier = second.coefficients[second_pivot_index];
  const uint8_t second_multiplier =
      static_cast<uint8_t>(3 - first.coefficients[first_pivot_index]);

  std::map<uint32_t, uint8_t> combined;
  for (size_t i = 0; i < first.domains.size(); ++i)
    if (first.domains[i] != pivot)
      combined[first.domains[i]] =
          static_cast<uint8_t>((first_multiplier * first.coefficients[i]) % 3);
  for (size_t i = 0; i < second.domains.size(); ++i)
    if (second.domains[i] != pivot) {
      const uint8_t contribution =
          static_cast<uint8_t>((second_multiplier * second.coefficients[i]) % 3);
      combined[second.domains[i]] =
          static_cast<uint8_t>((combined[second.domains[i]] + contribution) % 3);
    }
  for (auto it = combined.begin(); it != combined.end();)
    if (!it->second)
      it = combined.erase(it);
    else
      ++it;
  if (combined.empty()) return decline(PlanStatus::NoConsequence);
  if (combined.size() > bounds.maximum_result_arity)
    return decline(PlanStatus::BoundExceeded);

  EquationMeaning consequence;
  uint8_t rhs = static_cast<uint8_t>(
      (first_multiplier * first.rhs + second_multiplier * second.rhs) % 3);
  const uint8_t normalization = inverse(combined.begin()->second);
  for (const auto &[domain, coefficient] : combined) {
    consequence.domains.push_back(domain);
    consequence.coefficients.push_back(
        static_cast<uint8_t>((normalization * coefficient) % 3));
  }
  consequence.rhs = static_cast<uint8_t>((normalization * rhs) % 3);
  for (const Equation &existing : theory.equations)
    if (existing.domains == consequence.domains &&
        existing.coefficients == consequence.coefficients &&
        existing.rhs == consequence.rhs)
      return decline(PlanStatus::NoConsequence);

  uint64_t tuples = 0;
  if (!power3(static_cast<uint32_t>(consequence.domains.size()), tuples) ||
      tuples > bounds.maximum_result_tuples)
    return decline(PlanStatus::BoundExceeded);
  const Domain &pivot_domain = theory.domains[pivot];
  const Premise *pivot_alo = find_occurrence(theory, pivot_domain.at_least_one);
  if (!pivot_alo) return decline(PlanStatus::IncompleteTheory);

  auto required_pivot = [&](const Equation &equation, size_t pivot_index,
                            const std::map<uint32_t, uint8_t> &values) {
    uint8_t sum = 0;
    for (size_t i = 0; i < equation.domains.size(); ++i)
      if (i != pivot_index)
        sum = static_cast<uint8_t>(
            (sum + equation.coefficients[i] * values.at(equation.domains[i])) %
            3);
    const uint8_t residual =
        static_cast<uint8_t>((equation.rhs + 3 - sum) % 3);
    return static_cast<uint8_t>(
        (inverse(equation.coefficients[pivot_index]) * residual) % 3);
  };
  auto parent_tuple = [](const Equation &equation, size_t pivot_index,
                         uint8_t pivot_value,
                         const std::map<uint32_t, uint8_t> &values) {
    uint64_t tuple = 0, place = 1;
    for (size_t i = 0; i < equation.domains.size(); ++i) {
      const uint8_t value =
          i == pivot_index ? pivot_value : values.at(equation.domains[i]);
      tuple += place * value;
      place *= 3;
    }
    return tuple;
  };
  auto reference = [&](uint64_t occurrence, SourceReference &out) {
    const Premise *premise = find_occurrence(theory, occurrence);
    if (!premise) return false;
    out = {occurrence, premise->native_proof_id};
    return true;
  };

  std::vector<Addition> additions;
  uint64_t addition_literals = 0;
  for (uint64_t tuple = 0; tuple < tuples; ++tuple) {
    if (!spend(uint64_t{1} + consequence.domains.size()))
      return decline(PlanStatus::EffortExhausted);
    ++result.consumed_result_tuples;
    if (tuple_sum(tuple, consequence.coefficients) == consequence.rhs) continue;
    uint64_t proof_work = 0;
    if (!add_count(first.domains.size(), proof_work) ||
        !add_count(second.domains.size(), proof_work) ||
        !add_count(first.domains.size(), proof_work) ||
        !add_count(second.domains.size(), proof_work) ||
        !add_count(5, proof_work))
      return decline(PlanStatus::BoundExceeded);
    if (!spend(proof_work)) return decline(PlanStatus::EffortExhausted);
    if (additions.size() >= bounds.maximum_additions ||
        consequence.domains.size() >
            bounds.maximum_addition_literals - addition_literals)
      return decline(PlanStatus::BoundExceeded);
    std::map<uint32_t, uint8_t> values;
    uint64_t rest = tuple;
    Addition addition;
    for (uint32_t domain : consequence.domains) {
      const uint8_t value = static_cast<uint8_t>(rest % 3);
      rest /= 3;
      values.emplace(domain, value);
      addition.clause.push_back(-theory.domains[domain].values[value]);
    }
    const uint8_t required_first =
        required_pivot(first, first_pivot_index, values);
    const uint8_t required_second =
        required_pivot(second, second_pivot_index, values);
    if (required_first == required_second)
      return decline(PlanStatus::IncompleteTheory);
    for (uint8_t value = 0; value < 3; ++value)
      if (value != required_first) {
        SourceReference source_reference;
        const uint64_t parent = first.forbidden_by_tuple[parent_tuple(
            first, first_pivot_index, value, values)];
        if (!parent || !reference(parent, source_reference))
          return decline(PlanStatus::IncompleteTheory);
        addition.antecedents.push_back(source_reference);
      }
    for (uint8_t value = 0; value < 3; ++value)
      if (value != required_second) {
        SourceReference source_reference;
        const uint64_t parent = second.forbidden_by_tuple[parent_tuple(
            second, second_pivot_index, value, values)];
        if (!parent || !reference(parent, source_reference))
          return decline(PlanStatus::IncompleteTheory);
        addition.antecedents.push_back(source_reference);
      }
    addition.antecedents.push_back(
        {pivot_alo->occurrence, pivot_alo->native_proof_id});
    addition_literals += addition.clause.size();
    result.consumed_addition_literals = addition_literals;
    additions.push_back(std::move(addition));
  }
  if (additions.empty()) return decline(PlanStatus::NoConsequence);

  result.status = PlanStatus::Planned;
  result.consequence = std::move(consequence);
  result.additions = std::move(additions);
  result.projection = {{}, true, false};
  return result;
}

} // namespace tactix::gf3_affine
