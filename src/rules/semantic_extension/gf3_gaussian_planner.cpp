#include "rules/semantic_extension/gf3_gaussian_planner.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <utility>

namespace tactix::gf3_gaussian {
namespace {

constexpr uint64_t node_bytes = 48;
constexpr uint64_t row_term_bytes = 8;
constexpr uint64_t provenance_term_bytes = 16;
constexpr uint64_t outcome_bytes = 16;

uint8_t add3(uint8_t a, uint8_t b) {
  return static_cast<uint8_t>((a + b) % 3);
}

uint8_t multiply3(uint8_t a, uint8_t b) {
  return static_cast<uint8_t>((a * b) % 3);
}

bool add_checked(uint64_t a, uint64_t b, uint64_t &result) {
  if (b > std::numeric_limits<uint64_t>::max() - a) return false;
  result = a + b;
  return true;
}

bool multiply_checked(uint64_t a, uint64_t b, uint64_t &result) {
  if (a && b > std::numeric_limits<uint64_t>::max() / a) return false;
  result = a * b;
  return true;
}

bool valid_row(const SourceRow &row) {
  if (!row.source_id || row.rhs >= 3) return false;
  uint32_t previous = 0;
  for (const Term &term : row.terms) {
    if (!term.variable || term.variable <= previous ||
        (term.coefficient != 1 && term.coefficient != 2))
      return false;
    previous = term.variable;
  }
  return true;
}

uint8_t normalization_factor(const SourceRow &row) {
  return row.terms.empty() || row.terms.front().coefficient == 1 ? 1 : 2;
}

bool canonical_less(const SourceRow &a, const SourceRow &b) {
  const uint8_t af = normalization_factor(a);
  const uint8_t bf = normalization_factor(b);
  const size_t common = std::min(a.terms.size(), b.terms.size());
  for (size_t i = 0; i < common; ++i) {
    if (a.terms[i].variable != b.terms[i].variable)
      return a.terms[i].variable < b.terms[i].variable;
    const uint8_t ac = multiply3(a.terms[i].coefficient, af);
    const uint8_t bc = multiply3(b.terms[i].coefficient, bf);
    if (ac != bc) return ac < bc;
  }
  if (a.terms.size() != b.terms.size())
    return a.terms.size() < b.terms.size();
  const uint8_t ar = multiply3(a.rhs, af);
  const uint8_t br = multiply3(b.rhs, bf);
  if (ar != br) return ar < br;
  return a.source_id < b.source_id;
}

struct WorkingRow {
  std::vector<Term> row;
  uint8_t rhs = 0;
  std::vector<ProvenanceTerm> provenance;
  uint32_t derivation = 0;
};

std::vector<Term> scaled(const std::vector<Term> &row, uint8_t factor) {
  std::vector<Term> result = row;
  for (Term &term : result)
    term.coefficient = multiply3(term.coefficient, factor);
  return result;
}

std::vector<ProvenanceTerm>
scaled(const std::vector<ProvenanceTerm> &provenance, uint8_t factor) {
  std::vector<ProvenanceTerm> result = provenance;
  for (ProvenanceTerm &term : result)
    term.coefficient = multiply3(term.coefficient, factor);
  return result;
}

std::vector<Term> added(const std::vector<Term> &a,
                        const std::vector<Term> &b) {
  std::vector<Term> result;
  result.reserve(a.size() + b.size());
  size_t i = 0, j = 0;
  while (i < a.size() || j < b.size()) {
    if (j == b.size() ||
        (i < a.size() && a[i].variable < b[j].variable)) {
      result.push_back(a[i++]);
    } else if (i == a.size() || b[j].variable < a[i].variable) {
      result.push_back(b[j++]);
    } else {
      const uint8_t coefficient =
          add3(a[i].coefficient, b[j].coefficient);
      if (coefficient) result.push_back({a[i].variable, coefficient});
      ++i;
      ++j;
    }
  }
  return result;
}

std::vector<ProvenanceTerm>
added(const std::vector<ProvenanceTerm> &a,
      const std::vector<ProvenanceTerm> &b) {
  std::vector<ProvenanceTerm> result;
  result.reserve(a.size() + b.size());
  size_t i = 0, j = 0;
  while (i < a.size() || j < b.size()) {
    if (j == b.size() ||
        (i < a.size() && a[i].source_id < b[j].source_id)) {
      result.push_back(a[i++]);
    } else if (i == a.size() || b[j].source_id < a[i].source_id) {
      result.push_back(b[j++]);
    } else {
      const uint8_t coefficient =
          add3(a[i].coefficient, b[j].coefficient);
      if (coefficient) result.push_back({a[i].source_id, coefficient});
      ++i;
      ++j;
    }
  }
  return result;
}

struct Builder {
  const Limits &limits;
  Charge &charge;
  Plan plan;
  Statistics consumed;
  LimitKind limit = LimitKind::None;
  bool interrupted = false;
  std::map<uint32_t, uint32_t> scale_by_two;

  Builder(const Limits &limits_, Charge &charge_)
      : limits(limits_), charge(charge_) {}

  bool consume_work(uint64_t amount) {
    uint64_t next = 0;
    if (!add_checked(consumed.work, amount, next) ||
        next > limits.maximum_work) {
      limit = LimitKind::Work;
      return false;
    }
    if (!charge(amount)) {
      interrupted = true;
      return false;
    }
    consumed.work = next;
    return true;
  }

  bool consume_retained_plan(uint64_t amount) {
    uint64_t next = 0;
    if (!add_checked(consumed.retained_plan_bytes, amount, next) ||
        next > limits.maximum_retained_plan_bytes) {
      limit = LimitKind::RetainedPlan;
      return false;
    }
    consumed.retained_plan_bytes = next;
    return true;
  }

  bool check_width(size_t width) {
    if (width <= limits.maximum_intermediate_width) return true;
    limit = LimitKind::IntermediateWidth;
    return false;
  }

  bool check_provenance(size_t size) {
    if (size <= limits.maximum_provenance_terms) return true;
    limit = LimitKind::Provenance;
    return false;
  }

  bool add_node(Derivation node, bool counts_fill, uint32_t &id) {
    if (plan.derivations.size() >= limits.maximum_derivations) {
      limit = LimitKind::Derivations;
      return false;
    }
    if (!check_width(node.row.size()) ||
        !check_provenance(node.provenance.size()))
      return false;
    if (counts_fill) {
      uint64_t next = 0;
      if (!add_checked(consumed.fill_entries, node.row.size(), next) ||
          next > limits.maximum_fill_entries) {
        limit = LimitKind::Fill;
        return false;
      }
      consumed.fill_entries = next;
    }
    uint64_t row_memory = 0, provenance_memory = 0, memory = node_bytes;
    if (!multiply_checked(node.row.size(), row_term_bytes, row_memory) ||
        !multiply_checked(node.provenance.size(), provenance_term_bytes,
                          provenance_memory) ||
        !add_checked(memory, row_memory, memory) ||
        !add_checked(memory, provenance_memory, memory) ||
        !consume_retained_plan(memory))
      return false;
    plan.derivations.push_back(std::move(node));
    id = static_cast<uint32_t>(plan.derivations.size());
    return true;
  }

  bool source(const SourceRow &input, WorkingRow &result) {
    if (!consume_work(1 + input.terms.size())) return false;
    Derivation node;
    node.kind = DerivationKind::Source;
    node.source_id = input.source_id;
    node.row = input.terms;
    node.rhs = input.rhs;
    node.provenance = {{input.source_id, 1}};
    uint32_t id = 0;
    if (!add_node(std::move(node), false, id)) return false;
    const Derivation &stored = plan.derivations[id - 1];
    result = {stored.row, stored.rhs, stored.provenance, id};
    if (!result.row.empty() && result.row.front().coefficient == 2)
      return scale(result, result);
    return true;
  }

  bool scale(const WorkingRow &input, WorkingRow &result) {
    const auto found = scale_by_two.find(input.derivation);
    if (found != scale_by_two.end()) {
      const Derivation &stored = plan.derivations[found->second - 1];
      result = {stored.row, stored.rhs, stored.provenance, found->second};
      return true;
    }
    if (!consume_work(1 + input.row.size() + input.provenance.size()))
      return false;
    Derivation node;
    node.kind = DerivationKind::Scale;
    node.first_parent = input.derivation;
    node.factor = 2;
    node.row = scaled(input.row, 2);
    node.rhs = multiply3(input.rhs, 2);
    node.provenance = scaled(input.provenance, 2);
    uint32_t id = 0;
    if (!add_node(std::move(node), true, id)) return false;
    scale_by_two.emplace(input.derivation, id);
    const Derivation &stored = plan.derivations[id - 1];
    result = {stored.row, stored.rhs, stored.provenance, id};
    return true;
  }

  bool add(const WorkingRow &a, const WorkingRow &b, WorkingRow &result) {
    if (!consume_work(1 + a.row.size() + b.row.size() +
                      a.provenance.size() + b.provenance.size()))
      return false;
    Derivation node;
    node.kind = DerivationKind::Add;
    node.first_parent = a.derivation;
    node.second_parent = b.derivation;
    node.row = added(a.row, b.row);
    node.rhs = add3(a.rhs, b.rhs);
    node.provenance = added(a.provenance, b.provenance);
    uint32_t id = 0;
    if (!add_node(std::move(node), true, id)) return false;
    const Derivation &stored = plan.derivations[id - 1];
    result = {stored.row, stored.rhs, stored.provenance, id};
    return true;
  }

  bool eliminate(WorkingRow &target, const WorkingRow &pivot,
                 uint8_t coefficient) {
    WorkingRow addend = pivot;
    // pivot coefficient is one.  1 + 2 = 0 and 2 + 1 = 0.
    if (coefficient == 1 && !scale(pivot, addend)) return false;
    WorkingRow sum;
    if (!add(target, addend, sum)) return false;
    target = std::move(sum);
    if (!target.row.empty() && target.row.front().coefficient == 2) {
      WorkingRow normalized;
      if (!scale(target, normalized)) return false;
      target = std::move(normalized);
    }
    return true;
  }

  bool add_outcome(RowOutcomeKind kind, uint32_t derivation) {
    if (!consume_retained_plan(outcome_bytes)) return false;
    plan.outcomes.push_back({kind, derivation});
    return true;
  }
};

Result decline(Builder &builder, PlanStatus status) {
  Result result;
  result.status = status;
  result.limit = builder.limit;
  result.consumed = builder.consumed;
  return result;
}

} // namespace

Result build_plan(const std::vector<SourceRow> &source, const Limits &limits,
                  Charge charge) {
  if (!charge) return {};
  Builder builder(limits, charge);
  if (source.empty()) {
    Result result;
    result.status = PlanStatus::NoStructure;
    return result;
  }
  if (source.size() > limits.maximum_derivations) {
    builder.limit = LimitKind::Derivations;
    return decline(builder, PlanStatus::LimitExceeded);
  }
  std::set<uint64_t> identities;
  for (const SourceRow &row : source) {
    if (!valid_row(row) || !identities.insert(row.source_id).second)
      return decline(builder, PlanStatus::InvalidInput);
    if (!builder.check_width(row.terms.size()))
      return decline(builder, PlanStatus::LimitExceeded);
  }

  std::vector<size_t> order(source.size());
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    return canonical_less(source[a], source[b]);
  });

  std::map<uint32_t, WorkingRow> basis;
  for (size_t index : order) {
    WorkingRow row;
    if (!builder.source(source[index], row))
      return decline(builder, builder.interrupted ? PlanStatus::Interrupted
                                                  : PlanStatus::LimitExceeded);
    while (!row.row.empty()) {
      const auto found = basis.find(row.row.front().variable);
      if (found == basis.end()) break;
      if (!builder.eliminate(row, found->second,
                             row.row.front().coefficient))
        return decline(
            builder, builder.interrupted ? PlanStatus::Interrupted
                                         : PlanStatus::LimitExceeded);
    }
    if (row.row.empty()) {
      if (!row.rhs) continue;
      if (!builder.add_outcome(RowOutcomeKind::ZeroRowConflict,
                               row.derivation))
        return decline(builder, PlanStatus::LimitExceeded);
      builder.plan.statistics = builder.consumed;
      return {PlanStatus::Complete, LimitKind::None, builder.consumed,
              std::move(builder.plan)};
    }
    basis.emplace(row.row.front().variable, std::move(row));
  }

  // Deterministic backward elimination produces reduced row-echelon form and
  // exposes every algebraic unit implied by the recognized system.
  for (auto lower = basis.rbegin(); lower != basis.rend(); ++lower) {
    const uint32_t pivot = lower->first;
    for (auto upper = basis.begin();
         upper != basis.end() && upper->first < pivot;
         ++upper) {
      const auto term = std::lower_bound(
          upper->second.row.begin(), upper->second.row.end(), pivot,
          [](const Term &candidate, uint32_t variable) {
            return candidate.variable < variable;
          });
      if (term == upper->second.row.end() || term->variable != pivot) continue;
      const uint8_t coefficient = term->coefficient;
      if (!builder.eliminate(upper->second, lower->second, coefficient))
        return decline(
            builder, builder.interrupted ? PlanStatus::Interrupted
                                         : PlanStatus::LimitExceeded);
    }
  }

  if (basis.empty()) return decline(builder, PlanStatus::NoStructure);
  for (const auto &[pivot, row] : basis) {
    (void)pivot;
    const RowOutcomeKind kind = row.row.size() == 1
                                    ? RowOutcomeKind::Unit
                                    : RowOutcomeKind::Derived;
    if (!builder.add_outcome(kind, row.derivation))
      return decline(builder, PlanStatus::LimitExceeded);
  }
  builder.plan.statistics = builder.consumed;
  return {PlanStatus::Complete, LimitKind::None, builder.consumed,
          std::move(builder.plan)};
}

} // namespace tactix::gf3_gaussian
