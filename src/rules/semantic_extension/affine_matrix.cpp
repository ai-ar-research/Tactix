#include "rules/semantic_extension/affine_matrix.hpp"

#include <algorithm>
#include <climits>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace tactix::affine_semantic_extension {
namespace {

struct Work {
  uint64_t used = 0;
  uint64_t limit = 0;
  const std::function<bool(uint64_t)> &charge;
  bool interrupted = false;

  bool consume(uint64_t amount) {
    if (amount > limit - used) return false;
    if (!charge(amount)) {
      interrupted = true;
      return false;
    }
    used += amount;
    return true;
  }
};

template <class T>
void xor_sorted(const std::vector<T> &first, const std::vector<T> &second,
                std::vector<T> &output) {
  output.clear();
  output.reserve(first.size() + second.size());
  std::set_symmetric_difference(first.begin(), first.end(), second.begin(),
                                second.end(), std::back_inserter(output));
}

bool valid_equation(const Equation &equation) {
  if (!std::is_sorted(equation.variables.begin(), equation.variables.end()) ||
      std::adjacent_find(equation.variables.begin(),
                         equation.variables.end()) != equation.variables.end())
    return false;
  return std::all_of(equation.variables.begin(), equation.variables.end(),
                     [](External variable) {
                       return variable > 0 && variable != INT32_MIN;
                     });
}

bool bounded(const MatrixRow &row, const MatrixLimits &limits) {
  return row.variables.size() <= limits.maximum_row_width &&
         row.source_equations.size() <= limits.maximum_sources;
}

bool combine(MatrixRow &row, const MatrixRow &other, const MatrixLimits &limits,
             Work &work) {
  if (!work.consume(1 + row.variables.size() + other.variables.size() +
                    row.source_equations.size() +
                    other.source_equations.size()))
    return false;
  std::vector<External> variables;
  std::vector<size_t> sources;
  xor_sorted(row.variables, other.variables, variables);
  xor_sorted(row.source_equations, other.source_equations, sources);
  row.variables = std::move(variables);
  row.source_equations = std::move(sources);
  row.rhs ^= other.rhs;
  return bounded(row, limits);
}

MatrixStatus reduce(const std::vector<Equation> &input,
                    const std::vector<MatrixAssignment> &trail,
                    const MatrixLimits &limits, Work &work,
                    std::vector<MatrixRow> &output) {
  std::map<External, bool> assignments;
  for (const MatrixAssignment assignment : trail) {
    if (assignment.variable <= 0 || assignment.variable == INT32_MIN ||
        !assignments.emplace(assignment.variable, assignment.value).second)
      return MatrixStatus::Invalid;
  }

  std::map<External, MatrixRow> basis;
  std::vector<MatrixRow> contradictions;
  for (size_t source = 0; source < input.size(); ++source) {
    MatrixRow row;
    row.rhs = input[source].rhs;
    row.source_equations.push_back(source);
    for (External variable : input[source].variables) {
      const auto found = assignments.find(variable);
      if (found == assignments.end())
        row.variables.push_back(variable);
      else
        row.rhs ^= found->second;
    }
    if (!work.consume(1 + input[source].variables.size()))
      return work.interrupted ? MatrixStatus::Interrupted
                              : MatrixStatus::Exhausted;

    while (!row.variables.empty()) {
      const auto found = basis.find(row.variables.front());
      if (found == basis.end()) break;
      if (!combine(row, found->second, limits, work))
        return work.interrupted ? MatrixStatus::Interrupted
                                : MatrixStatus::Exhausted;
    }
    if (!bounded(row, limits)) return MatrixStatus::Exhausted;
    if (row.variables.empty()) {
      if (row.rhs) contradictions.push_back(std::move(row));
      continue;
    }

    // Eliminate the new pivot from every earlier row. Together with the
    // forward reduction above this keeps the stored basis in reduced form.
    const External pivot = row.variables.front();
    for (auto &[old_pivot, old_row] : basis) {
      (void)old_pivot;
      if (!std::binary_search(old_row.variables.begin(),
                              old_row.variables.end(), pivot))
        continue;
      if (!combine(old_row, row, limits, work))
        return work.interrupted ? MatrixStatus::Interrupted
                                : MatrixStatus::Exhausted;
    }
    basis.emplace(pivot, std::move(row));
  }

  output.clear();
  output.reserve(contradictions.size() + basis.size());
  for (MatrixRow &row : contradictions) output.push_back(std::move(row));
  for (auto &[pivot, row] : basis) {
    (void)pivot;
    output.push_back(std::move(row));
  }
  return MatrixStatus::Complete;
}

External false_literal(const MatrixAssignment assignment) {
  return assignment.value ? -assignment.variable : assignment.variable;
}

MatrixEvent find_event(const std::vector<MatrixRow> &rows) {
  MatrixEvent result;
  for (const MatrixRow &row : rows) {
    if (!row.variables.empty() || !row.rhs) continue;
    result.kind = MatrixEventKind::Conflict;
    result.row = row;
    return result;
  }
  for (const MatrixRow &row : rows) {
    if (row.variables.size() != 1) continue;
    result.kind = MatrixEventKind::Implication;
    result.row = row;
    result.implied_variable = row.variables.front();
    result.implied_value = row.rhs;
    return result;
  }
  return result;
}

void make_reason(const std::vector<Equation> &sources,
                 const std::vector<MatrixAssignment> &trail,
                 MatrixEvent &event) {
  if (event.kind == MatrixEventKind::None) return;
  std::set<External> support;
  for (size_t source : event.row.source_equations)
    for (External variable : sources[source].variables)
      support.insert(variable);
  for (const MatrixAssignment assignment : trail)
    if (support.contains(assignment.variable))
      event.reason.push_back(false_literal(assignment));
  if (event.kind == MatrixEventKind::Implication)
    event.reason.push_back(event.implied_value ? event.implied_variable
                                               : -event.implied_variable);
}

} // namespace

MatrixStatus build_affine_matrix(
    const std::vector<Equation> &input, uint64_t owner_identity,
    const MatrixLimits &limits, const std::function<bool(uint64_t)> &charge,
    AffineMatrix &output) {
  if (!owner_identity || !charge || !limits.maximum_work ||
      !limits.maximum_rows || !limits.maximum_variables ||
      !limits.maximum_row_width || !limits.maximum_sources ||
      !limits.maximum_assignments)
    return MatrixStatus::Invalid;
  if (input.empty()) return MatrixStatus::NoStructure;
  if (input.size() > limits.maximum_rows) return MatrixStatus::Exhausted;

  std::set<External> variables;
  for (const Equation &equation : input) {
    if (!valid_equation(equation)) return MatrixStatus::Invalid;
    if (equation.variables.size() > limits.maximum_row_width)
      return MatrixStatus::Exhausted;
    variables.insert(equation.variables.begin(), equation.variables.end());
  }
  if (variables.size() > limits.maximum_variables)
    return MatrixStatus::Exhausted;

  Work work{0, limits.maximum_work, charge};
  std::vector<MatrixRow> rows;
  const MatrixStatus status = reduce(input, {}, limits, work, rows);
  if (status != MatrixStatus::Complete) return status;
  if (rows.empty()) return MatrixStatus::NoStructure;

  AffineMatrix candidate;
  candidate.owner_identity_ = owner_identity;
  candidate.work_ = work.used;
  candidate.limits_ = limits;
  candidate.sources_ = input;
  candidate.rows_ = std::move(rows);
  output = std::move(candidate);
  return MatrixStatus::Complete;
}

MatrixStatus AffineMatrix::synchronize(
    const std::vector<MatrixAssignment> &trail,
    const std::function<bool(uint64_t)> &charge, MatrixEvent &event) {
  if (!owner_identity_ || !charge) return MatrixStatus::Invalid;
  if (trail.size() > limits_.maximum_assignments)
    return MatrixStatus::Exhausted;

  Work work{0, limits_.maximum_work, charge};
  std::vector<MatrixRow> rows;
  const MatrixStatus status = reduce(sources_, trail, limits_, work, rows);
  if (status != MatrixStatus::Complete) return status;

  MatrixEvent candidate = find_event(rows);
  make_reason(sources_, trail, candidate);
  trail_ = trail;
  rows_ = std::move(rows);
  if (work.used > std::numeric_limits<uint64_t>::max() - work_)
    work_ = std::numeric_limits<uint64_t>::max();
  else
    work_ += work.used;
  event = std::move(candidate);
  return MatrixStatus::Complete;
}

} // namespace tactix::affine_semantic_extension
