#include "rules/semantic_extension/gf3_gaussian_bridge.hpp"

#include <algorithm>
#include <climits>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace tactix::gf3_gaussian_bridge {
namespace {

constexpr uint64_t gaussian_node_bytes = 48;
constexpr uint64_t gaussian_row_term_bytes = 8;
constexpr uint64_t gaussian_provenance_term_bytes = 16;
constexpr uint64_t gaussian_outcome_bytes = 16;

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

uint8_t add3(uint8_t first, uint8_t second) {
  return static_cast<uint8_t>((first + second) % 3);
}

uint8_t multiply3(uint8_t first, uint8_t second) {
  return static_cast<uint8_t>((first * second) % 3);
}

bool add_checked(uint64_t first, uint64_t second, uint64_t &result) {
  if (second > std::numeric_limits<uint64_t>::max() - first) return false;
  result = first + second;
  return true;
}

bool multiply_checked(uint64_t first, uint64_t second, uint64_t &result) {
  if (first && second > std::numeric_limits<uint64_t>::max() / first)
    return false;
  result = first * second;
  return true;
}

bool power3(uint32_t exponent, uint64_t &result) {
  result = 1;
  while (exponent--) {
    if (result > std::numeric_limits<uint64_t>::max() / 3) return false;
    result *= 3;
  }
  return true;
}

std::vector<int32_t> sorted(std::vector<int32_t> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

bool valid_clause(const std::vector<int32_t> &clause) {
  if (clause.empty()) return false;
  std::set<uint32_t> variables;
  for (const int32_t literal : clause)
    if (!literal || literal == INT32_MIN ||
        !variables.insert(variable(literal)).second)
      return false;
  return true;
}

struct Context {
  const Limits &limits;
  Charge &charge;
  Statistics stats;
  LimitKind limit = LimitKind::None;
  bool interrupted = false;

  Context(const Limits &limits_, Charge &charge_)
      : limits(limits_), charge(charge_) {}

  bool spend(uint64_t amount) {
    uint64_t next = 0;
    if (!add_checked(stats.work, amount, next) ||
        next > limits.maximum_work) {
      limit = LimitKind::Work;
      return false;
    }
    if (!charge(amount)) {
      interrupted = true;
      return false;
    }
    stats.work = next;
    return true;
  }

  bool add_count(uint64_t amount, uint64_t maximum, uint64_t &counter,
                 LimitKind exhausted) {
    uint64_t next = 0;
    if (!add_checked(counter, amount, next) || next > maximum) {
      limit = exhausted;
      return false;
    }
    counter = next;
    return true;
  }
};

struct AuthenticatedTheory {
  std::vector<gf3_gaussian::SourceRow> rows;
  std::vector<SourceAuthentication> authentication;
};

gf3_affine::SourceReference reference(const gf3_affine::Premise &premise) {
  return {premise.occurrence, premise.native_proof_id};
}

bool same_clause(const gf3_affine::Premise &premise,
                 std::vector<int32_t> expected) {
  return sorted(premise.literals) == sorted(std::move(expected));
}

bool authenticate(const gf3_affine::Theory &theory, Context &context,
                  AuthenticatedTheory &output) {
  if (theory.source.size() > context.limits.maximum_source_clauses) {
    context.limit = LimitKind::SourceClauses;
    return false;
  }
  if (theory.domains.size() > context.limits.maximum_domains) {
    context.limit = LimitKind::Domains;
    return false;
  }
  if (theory.equations.empty() ||
      theory.equations.size() > context.limits.maximum_equations) {
    context.limit = theory.equations.empty() ? LimitKind::None
                                             : LimitKind::Equations;
    return false;
  }

  std::map<uint64_t, const gf3_affine::Premise *> premises;
  uint32_t maximum_variable = 0;
  for (const gf3_affine::Premise &premise : theory.source) {
    if (!context.spend(1 + premise.literals.size())) return false;
    if (!premise.occurrence || !valid_clause(premise.literals) ||
        !premises.emplace(premise.occurrence, &premise).second)
      return false;
    if (!context.add_count(premise.literals.size(),
                           context.limits.maximum_source_literals,
                           context.stats.source_literals,
                           LimitKind::SourceLiterals))
      return false;
    ++context.stats.source_clauses;
    for (const int32_t literal : premise.literals)
      maximum_variable = std::max(maximum_variable, variable(literal));
  }
  if (maximum_variable != theory.maximum_variable) return false;

  std::vector<DomainAuthentication> domains(theory.domains.size());
  std::set<uint32_t> selection_variables;
  std::set<uint64_t> domain_occurrences;
  std::set<uint64_t> used_occurrences;
  for (uint32_t index = 0; index < theory.domains.size(); ++index) {
    if (!context.spend(8)) return false;
    const gf3_affine::Domain &domain = theory.domains[index];
    if (domain.values.size() != 3 || domain.at_most_one.size() != 9)
      return false;
    for (const int32_t literal : domain.values)
      if (!literal || literal == INT32_MIN ||
          !selection_variables.insert(variable(literal)).second)
        return false;
    const auto alo = premises.find(domain.at_least_one);
    if (alo == premises.end() ||
        !same_clause(*alo->second, domain.values) ||
        !domain_occurrences.insert(domain.at_least_one).second)
      return false;
    domains[index].domain = index;
    domains[index].at_least_one = reference(*alo->second);
    size_t pair = 0;
    for (uint8_t first = 0; first < 3; ++first) {
      if (domain.at_most_one[first * 3 + first]) return false;
      for (uint8_t second = first + 1; second < 3; ++second) {
        const uint64_t occurrence =
            domain.at_most_one[first * 3 + second];
        if (!occurrence ||
            occurrence != domain.at_most_one[second * 3 + first])
          return false;
        const auto amo = premises.find(occurrence);
        if (amo == premises.end() ||
            !same_clause(*amo->second,
                         {-domain.values[first], -domain.values[second]}) ||
            !domain_occurrences.insert(occurrence).second)
          return false;
        domains[index].at_most_one[pair++] = reference(*amo->second);
      }
    }
  }
  used_occurrences = domain_occurrences;

  output.rows.reserve(theory.equations.size());
  output.authentication.reserve(theory.equations.size());
  for (uint32_t index = 0; index < theory.equations.size(); ++index) {
    const gf3_affine::Equation &equation = theory.equations[index];
    if (!context.spend(1 + equation.domains.size())) return false;
    if (equation.domains.size() < 2 ||
        equation.domains.size() != equation.coefficients.size() ||
        equation.coefficients.front() != 1 || equation.rhs >= 3)
      return false;
    uint64_t tuple_count = 0;
    if (!power3(static_cast<uint32_t>(equation.domains.size()),
                tuple_count) ||
        equation.forbidden_by_tuple.size() != tuple_count)
      return false;

    gf3_gaussian::SourceRow row;
    row.source_id = uint64_t{index} + 1;
    row.rhs = equation.rhs;
    SourceAuthentication authentication;
    authentication.source_id = row.source_id;
    authentication.equation_index = index;
    uint64_t references = 0;
    for (size_t term = 0; term < equation.domains.size(); ++term) {
      const uint32_t domain = equation.domains[term];
      const uint8_t coefficient = equation.coefficients[term];
      if (domain >= theory.domains.size() ||
          (term && equation.domains[term - 1] >= domain) ||
          (coefficient != 1 && coefficient != 2))
        return false;
      row.terms.push_back({domain + 1, coefficient});
      authentication.domains.push_back(domains[domain]);
      references += 4;
    }
    if (!context.add_count(row.terms.size(), context.limits.maximum_terms,
                           context.stats.terms, LimitKind::Terms))
      return false;

    for (uint64_t tuple = 0; tuple < tuple_count; ++tuple) {
      if (!context.spend(1 + equation.domains.size())) return false;
      uint64_t rest = tuple;
      uint8_t sum = 0;
      std::vector<int32_t> clause;
      clause.reserve(equation.domains.size());
      for (size_t term = 0; term < equation.domains.size(); ++term) {
        const uint8_t value = static_cast<uint8_t>(rest % 3);
        rest /= 3;
        sum = add3(sum, multiply3(equation.coefficients[term], value));
        clause.push_back(
            -theory.domains[equation.domains[term]].values[value]);
      }
      const uint64_t occurrence = equation.forbidden_by_tuple[tuple];
      const bool forbidden = sum != equation.rhs;
      if (forbidden != bool(occurrence)) return false;
      if (!forbidden) continue;
      const auto found = premises.find(occurrence);
      if (found == premises.end() || !same_clause(*found->second, clause) ||
          domain_occurrences.count(occurrence) ||
          !used_occurrences.insert(occurrence).second)
        return false;
      authentication.forbidden_tuples.push_back(
          {tuple, reference(*found->second)});
      ++references;
    }
    if (references > context.limits.maximum_references_per_equation) {
      context.limit = LimitKind::ReferencesPerEquation;
      return false;
    }
    if (!context.add_count(references,
                           context.limits.maximum_authentication_references,
                           context.stats.authentication_references,
                           LimitKind::AuthenticationReferences))
      return false;
    output.rows.push_back(std::move(row));
    output.authentication.push_back(std::move(authentication));
  }
  return used_occurrences.size() == premises.size();
}

Result failure(const Context &context, Status invalid) {
  Result result;
  result.status = context.interrupted
                      ? Status::Interrupted
                      : context.limit != LimitKind::None
                            ? Status::LimitExceeded
                            : invalid;
  result.limit = context.limit;
  result.consumed = context.stats;
  return result;
}

bool same_source_row(const gf3_gaussian::SourceRow &first,
                     const gf3_gaussian::SourceRow &second) {
  return first.source_id == second.source_id && first.rhs == second.rhs &&
         first.terms == second.terms;
}

using Row = std::map<uint32_t, uint8_t>;
using Provenance = std::map<uint64_t, uint8_t>;

Row row_map(const std::vector<gf3_gaussian::Term> &terms, bool &valid) {
  Row result;
  uint32_t previous = 0;
  for (const gf3_gaussian::Term &term : terms) {
    if (!term.variable || term.variable <= previous ||
        (term.coefficient != 1 && term.coefficient != 2)) {
      valid = false;
      return {};
    }
    result.emplace(term.variable, term.coefficient);
    previous = term.variable;
  }
  return result;
}

Provenance provenance_map(
    const std::vector<gf3_gaussian::ProvenanceTerm> &terms, bool &valid) {
  Provenance result;
  uint64_t previous = 0;
  for (const gf3_gaussian::ProvenanceTerm &term : terms) {
    if (!term.source_id || term.source_id <= previous ||
        (term.coefficient != 1 && term.coefficient != 2)) {
      valid = false;
      return {};
    }
    result.emplace(term.source_id, term.coefficient);
    previous = term.source_id;
  }
  return result;
}

template <class Key>
std::map<Key, uint8_t> scale(const std::map<Key, uint8_t> &input,
                             uint8_t factor) {
  std::map<Key, uint8_t> result;
  for (const auto &[key, coefficient] : input)
    result.emplace(key, multiply3(coefficient, factor));
  return result;
}

template <class Key>
std::map<Key, uint8_t> add(const std::map<Key, uint8_t> &first,
                           const std::map<Key, uint8_t> &second) {
  std::map<Key, uint8_t> result = first;
  for (const auto &[key, coefficient] : second) {
    const uint8_t sum = add3(result[key], coefficient);
    if (sum)
      result[key] = sum;
    else
      result.erase(key);
  }
  return result;
}

struct Replayed {
  Row row;
  uint8_t rhs = 0;
  Provenance provenance;
};

bool equivalent_sources(const AuthenticatedTheory &expected,
                        const Plan &plan) {
  if (expected.rows.size() != plan.sources.size() ||
      expected.authentication != plan.authentication)
    return false;
  for (size_t i = 0; i < expected.rows.size(); ++i)
    if (!same_source_row(expected.rows[i], plan.sources[i])) return false;
  return true;
}

bool replay_derivations(const AuthenticatedTheory &expected, const Plan &plan,
                        Context &context, std::vector<Replayed> &replayed) {
  const auto &limits = context.limits.gaussian;
  if (plan.gaussian.derivations.empty() ||
      plan.gaussian.derivations.size() > limits.maximum_derivations ||
      plan.gaussian.outcomes.empty() ||
      plan.gaussian.outcomes.size() > limits.maximum_derivations) {
    context.limit = plan.gaussian.derivations.size() >
                                limits.maximum_derivations ||
                            plan.gaussian.outcomes.size() >
                                limits.maximum_derivations
                        ? LimitKind::Gaussian
                        : LimitKind::None;
    return false;
  }
  std::map<uint64_t, const gf3_gaussian::SourceRow *> sources;
  for (const auto &row : expected.rows) sources.emplace(row.source_id, &row);
  replayed.resize(1);
  for (size_t index = 0; index < plan.gaussian.derivations.size(); ++index) {
    const gf3_gaussian::Derivation &node =
        plan.gaussian.derivations[index];
    if (!context.spend(1 + node.row.size() + node.provenance.size()))
      return false;
    if (node.row.size() > limits.maximum_intermediate_width ||
        node.provenance.size() > limits.maximum_provenance_terms) {
      context.limit = LimitKind::Gaussian;
      return false;
    }
    bool valid = node.rhs < 3;
    const Row stored_row = row_map(node.row, valid);
    const Provenance stored_provenance =
        provenance_map(node.provenance, valid);
    if (!valid) return false;
    Replayed current;
    if (node.kind == gf3_gaussian::DerivationKind::Source) {
      const auto found = sources.find(node.source_id);
      if (!node.source_id || node.first_parent || node.second_parent ||
          node.factor != 1 || found == sources.end())
        return false;
      bool source_valid = true;
      current.row = row_map(found->second->terms, source_valid);
      current.rhs = found->second->rhs;
      current.provenance = {{node.source_id, 1}};
    } else if (node.kind == gf3_gaussian::DerivationKind::Scale) {
      if (node.source_id || !node.first_parent ||
          node.first_parent > index || node.second_parent || node.factor != 2)
        return false;
      const Replayed &parent = replayed[node.first_parent];
      current.row = scale(parent.row, 2);
      current.rhs = multiply3(parent.rhs, 2);
      current.provenance = scale(parent.provenance, 2);
    } else if (node.kind == gf3_gaussian::DerivationKind::Add) {
      if (node.source_id || !node.first_parent || node.first_parent > index ||
          !node.second_parent || node.second_parent > index || node.factor != 1)
        return false;
      const Replayed &first = replayed[node.first_parent];
      const Replayed &second = replayed[node.second_parent];
      current.row = add(first.row, second.row);
      current.rhs = add3(first.rhs, second.rhs);
      current.provenance = add(first.provenance, second.provenance);
    } else {
      return false;
    }
    if (current.row != stored_row || current.rhs != node.rhs ||
        current.provenance != stored_provenance)
      return false;

    Row from_sources;
    uint8_t source_rhs = 0;
    for (const auto &[source_id, coefficient] : current.provenance) {
      const auto found = sources.find(source_id);
      if (found == sources.end()) return false;
      bool source_valid = true;
      const Row source_row = row_map(found->second->terms, source_valid);
      from_sources = add(from_sources, scale(source_row, coefficient));
      source_rhs = add3(
          source_rhs, multiply3(found->second->rhs, coefficient));
    }
    if (from_sources != current.row || source_rhs != current.rhs) return false;
    replayed.push_back(std::move(current));
  }
  return true;
}

bool validate_gaussian_accounting(const Plan &plan, Context &context) {
  gf3_gaussian::Statistics computed;
  for (const gf3_gaussian::Derivation &node : plan.gaussian.derivations) {
    uint64_t work = 0;
    if (node.kind == gf3_gaussian::DerivationKind::Source) {
      if (!add_checked(1, node.row.size(), work)) return false;
    } else if (node.kind == gf3_gaussian::DerivationKind::Scale) {
      const auto &parent =
          plan.gaussian.derivations[node.first_parent - 1];
      if (!add_checked(1, parent.row.size(), work) ||
          !add_checked(work, parent.provenance.size(), work))
        return false;
    } else {
      const auto &first =
          plan.gaussian.derivations[node.first_parent - 1];
      const auto &second =
          plan.gaussian.derivations[node.second_parent - 1];
      if (!add_checked(1, first.row.size(), work) ||
          !add_checked(work, second.row.size(), work) ||
          !add_checked(work, first.provenance.size(), work) ||
          !add_checked(work, second.provenance.size(), work))
        return false;
    }
    if (!add_checked(computed.work, work, computed.work)) return false;

    if (node.kind != gf3_gaussian::DerivationKind::Source &&
        !add_checked(computed.fill_entries, node.row.size(),
                     computed.fill_entries))
      return false;

    uint64_t row_bytes = 0;
    uint64_t provenance_bytes = 0;
    uint64_t retained = gaussian_node_bytes;
    if (!multiply_checked(node.row.size(), gaussian_row_term_bytes,
                          row_bytes) ||
        !multiply_checked(node.provenance.size(),
                          gaussian_provenance_term_bytes,
                          provenance_bytes) ||
        !add_checked(retained, row_bytes, retained) ||
        !add_checked(retained, provenance_bytes, retained) ||
        !add_checked(computed.retained_plan_bytes, retained,
                     computed.retained_plan_bytes))
      return false;
  }
  uint64_t outcome_bytes = 0;
  if (!multiply_checked(plan.gaussian.outcomes.size(),
                        gaussian_outcome_bytes, outcome_bytes) ||
      !add_checked(computed.retained_plan_bytes, outcome_bytes,
                   computed.retained_plan_bytes))
    return false;

  if (computed.work > context.limits.gaussian.maximum_work ||
      computed.fill_entries >
          context.limits.gaussian.maximum_fill_entries ||
      computed.retained_plan_bytes >
          context.limits.gaussian.maximum_retained_plan_bytes) {
    context.limit = LimitKind::Gaussian;
    return false;
  }
  const auto &claimed = plan.gaussian.statistics;
  return claimed.work == computed.work &&
         claimed.fill_entries == computed.fill_entries &&
         claimed.retained_plan_bytes == computed.retained_plan_bytes;
}

uint32_t coefficient_rank(std::vector<Row> rows, Context &context) {
  std::map<uint32_t, Row> basis;
  for (Row row : rows) {
    while (!row.empty()) {
      if (!context.spend(1 + row.size())) return UINT32_MAX;
      const uint32_t pivot = row.begin()->first;
      const auto found = basis.find(pivot);
      if (found == basis.end()) {
        if (row.begin()->second == 2) row = scale(row, 2);
        basis.emplace(pivot, std::move(row));
        break;
      }
      Row addend = found->second;
      if (row.begin()->second == 1) addend = scale(addend, 2);
      row = add(row, addend);
    }
  }
  return static_cast<uint32_t>(basis.size());
}

bool validate_outcomes(const AuthenticatedTheory &expected, const Plan &plan,
                       const std::vector<Replayed> &replayed,
                       Context &context) {
  bool conflict = false;
  std::set<uint32_t> pivots;
  std::vector<Row> outcome_rows;
  for (const gf3_gaussian::RowOutcome &outcome : plan.gaussian.outcomes) {
    if (!context.spend(1)) return false;
    if (!outcome.derivation || outcome.derivation >= replayed.size())
      return false;
    const Replayed &row = replayed[outcome.derivation];
    if (outcome.kind == gf3_gaussian::RowOutcomeKind::ZeroRowConflict) {
      if (!row.row.empty() || !row.rhs || conflict) return false;
      conflict = true;
      continue;
    }
    if (conflict || row.row.empty() || row.row.begin()->second != 1)
      return false;
    if (outcome.kind == gf3_gaussian::RowOutcomeKind::Unit) {
      if (row.row.size() != 1) return false;
    } else if (outcome.kind == gf3_gaussian::RowOutcomeKind::Derived) {
      if (row.row.size() < 2) return false;
    } else {
      return false;
    }
    if (!pivots.insert(row.row.begin()->first).second) return false;
    outcome_rows.push_back(row.row);
  }
  if (conflict) return plan.gaussian.outcomes.size() == 1;

  for (const Row &row : outcome_rows)
    for (const uint32_t pivot : pivots)
      if (pivot != row.begin()->first && row.count(pivot)) return false;
  std::vector<Row> source_rows;
  std::vector<Row> augmented_rows;
  source_rows.reserve(expected.rows.size());
  augmented_rows.reserve(expected.rows.size());
  for (const auto &source : expected.rows) {
    bool valid = true;
    Row row = row_map(source.terms, valid);
    source_rows.push_back(row);
    if (source.rhs) row.emplace(0, source.rhs);
    augmented_rows.push_back(std::move(row));
  }
  const uint32_t source_rank = coefficient_rank(source_rows, context);
  if (source_rank == UINT32_MAX) return false;
  const uint32_t augmented_rank = coefficient_rank(augmented_rows, context);
  if (augmented_rank == UINT32_MAX || augmented_rank != source_rank)
    return false;
  const uint32_t output_rank = coefficient_rank(outcome_rows, context);
  return output_rank != UINT32_MAX && source_rank == output_rank &&
         output_rank == outcome_rows.size();
}

} // namespace

Result build(const gf3_affine::Theory &theory, const Limits &limits,
             Charge charge) {
  if (!charge) return {};
  Context context{limits, charge};
  AuthenticatedTheory authenticated;
  if (!authenticate(theory, context, authenticated))
    return failure(context, Status::InvalidTheory);

  gf3_gaussian::Result gaussian = gf3_gaussian::build_plan(
      authenticated.rows, limits.gaussian,
      [&](uint64_t amount) { return context.spend(amount); });
  if (gaussian.status != gf3_gaussian::PlanStatus::Complete) {
    Result result = failure(context, Status::InvalidTheory);
    if (!context.interrupted && context.limit == LimitKind::None &&
        gaussian.status == gf3_gaussian::PlanStatus::LimitExceeded) {
      result.status = Status::LimitExceeded;
      result.limit = LimitKind::Gaussian;
    }
    result.gaussian_limit = gaussian.limit;
    return result;
  }

  Result result;
  result.status = Status::Complete;
  result.consumed = context.stats;
  result.plan.sources = std::move(authenticated.rows);
  result.plan.authentication = std::move(authenticated.authentication);
  result.plan.gaussian = std::move(gaussian.plan);
  return result;
}

Result validate(const gf3_affine::Theory &theory, const Plan &plan,
                const Limits &limits, Charge charge) {
  if (!charge) return {};
  Context context{limits, charge};
  AuthenticatedTheory expected;
  if (!authenticate(theory, context, expected))
    return failure(context, Status::InvalidTheory);
  if (!equivalent_sources(expected, plan))
    return failure(context, Status::InvalidPlan);
  std::vector<Replayed> replayed;
  if (!replay_derivations(expected, plan, context, replayed) ||
      !validate_gaussian_accounting(plan, context) ||
      !validate_outcomes(expected, plan, replayed, context))
    return failure(context, Status::InvalidPlan);

  Result result;
  result.status = Status::Complete;
  result.consumed = context.stats;
  result.plan = plan;
  return result;
}

} // namespace tactix::gf3_gaussian_bridge
