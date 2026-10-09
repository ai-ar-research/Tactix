#include "rules/semantic_extension/affine_matrix.hpp"

#include <algorithm>
#include <climits>
#include <bit>
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
  bool *work_exhausted = nullptr;

  bool consume(uint64_t amount) {
    if (amount > limit - used) {
      if (work_exhausted) *work_exhausted = true;
      return false;
    }
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

enum class CombineStatus { Complete, BoundedOut, Exhausted, Interrupted };

CombineStatus combine(MatrixRow &row, const MatrixRow &other,
                      const MatrixLimits &limits, Work &work) {
  if (!work.consume(1 + row.variables.size() + other.variables.size() +
                    row.source_equations.size() +
                    other.source_equations.size()))
    return work.interrupted ? CombineStatus::Interrupted
                            : CombineStatus::Exhausted;
  std::vector<External> variables;
  std::vector<size_t> sources;
  xor_sorted(row.variables, other.variables, variables);
  xor_sorted(row.source_equations, other.source_equations, sources);
  if (variables.size() > limits.maximum_row_width ||
      sources.size() > limits.maximum_sources)
    return CombineStatus::BoundedOut;
  row.variables = std::move(variables);
  row.source_equations = std::move(sources);
  row.rhs ^= other.rhs;
  return CombineStatus::Complete;
}

MatrixStatus reduce(const std::vector<MatrixRow> &input,
                    const std::vector<MatrixAssignment> &trail,
                    const MatrixLimits &limits, Work &work,
                    std::vector<MatrixRow> &output, bool precharge = false, size_t *coefficient_rank = nullptr) {
  if (precharge && !work.consume(1 + uint64_t(trail.size()) *
      (2 * std::bit_width(uint64_t(limits.maximum_assignments)) + 2)))
    return work.interrupted ? MatrixStatus::Interrupted : MatrixStatus::Exhausted;
  std::map<External, bool> assignments;
  for (const MatrixAssignment assignment : trail) {
    if (assignment.variable <= 0 || assignment.variable == INT32_MIN ||
        !assignments.emplace(assignment.variable, assignment.value).second)
      return MatrixStatus::Invalid;
  }

  std::map<External, MatrixRow> basis;
  std::vector<MatrixRow> contradictions;
  std::set<External> admitted_variables;
  bool partial = false;
  for (const MatrixRow &seed : input) {
    if (precharge && !work.consume(1 + seed.variables.size() *
        (2 * std::bit_width(uint64_t(trail.size())) + 2) + seed.source_equations.size()))
      return work.interrupted ? MatrixStatus::Interrupted : MatrixStatus::Exhausted;
    MatrixRow row;
    row.rhs = seed.rhs;
    row.source_equations = seed.source_equations;
    for (External variable : seed.variables) {
      const auto found = assignments.find(variable);
      if (found == assignments.end())
        row.variables.push_back(variable);
      else
        row.rhs ^= found->second;
    }
    if (!precharge && !work.consume(1 + seed.variables.size())) {
      if (work.interrupted) return MatrixStatus::Interrupted;
      partial = true;
      break;
    }
    if (!bounded(row, limits)) {
      partial = true;
      continue;
    }

    while (!row.variables.empty()) {
      const auto found = basis.find(row.variables.front());
      if (found == basis.end()) break;
      const CombineStatus combined =
          combine(row, found->second, limits, work);
      if (combined == CombineStatus::Interrupted)
        return MatrixStatus::Interrupted;
      if (combined == CombineStatus::Exhausted) {
        partial = true;
        goto finished;
      }
      if (combined == CombineStatus::BoundedOut) {
        partial = true;
        row.variables.clear();
        row.source_equations.clear();
        break;
      }
    }
    if (row.source_equations.empty()) continue;
    if (row.variables.empty()) {
      if (row.rhs) {
        if (contradictions.size() + basis.size() == limits.maximum_rows) {
          partial = true;
          continue;
        }
        contradictions.push_back(std::move(row));
      }
      continue;
    }

    size_t additional_variables = 0;
    for (External variable : row.variables)
      additional_variables += !admitted_variables.contains(variable);
    if (additional_variables >
            limits.maximum_variables - admitted_variables.size() ||
        contradictions.size() + basis.size() == limits.maximum_rows) {
      partial = true;
      continue;
    }
    admitted_variables.insert(row.variables.begin(), row.variables.end());
    basis.emplace(row.variables.front(), std::move(row));
  }

finished:
  // Gauss-Jordan (CryptoMiniSat EGaussian::eliminate XORs a pivot row into
  // *all* rows with a 1 in its column): remove every other pivot from each
  // row, so a conditioned row is a singleton exactly when its pivot is
  // forced. Rows are visited by descending pivot; a row only holds pivots
  // larger than its own, and those rows are already reduced.
  if (!work.interrupted) {
    for (auto row = basis.rbegin(); row != basis.rend(); ++row) {
      // One pivot lookup per coordinate after the row's own pivot.
      if (row->second.variables.size() > 1 &&
          !work.consume(row->second.variables.size() - 1)) {
        if (work.interrupted) return MatrixStatus::Interrupted;
        partial = true;
        break;
      }
      size_t at = 1;
      while (at < row->second.variables.size()) {
        const auto pivot = basis.find(row->second.variables[at]);
        if (pivot == basis.end()) { ++at; continue; }
        const CombineStatus combined =
            combine(row->second, pivot->second, limits, work);
        if (combined == CombineStatus::Interrupted)
          return MatrixStatus::Interrupted;
        if (combined == CombineStatus::Exhausted) {
          partial = true;
          goto reduced;
        }
        if (combined == CombineStatus::BoundedOut) {
          // Kept sound but unreduced against this pivot: reduction stays
          // complete only within the row-width and provenance bounds.
          ++at;
          continue;
        }
        // The XOR removed this pivot and added only non-pivot or larger
        // reduced coordinates; rescan from the same position.
      }
    }
  }
reduced:
  if (coefficient_rank) *coefficient_rank = basis.size();
  output.clear();
  output.reserve(contradictions.size() + basis.size());
  for (MatrixRow &row : contradictions) output.push_back(std::move(row));
  for (auto &[pivot, row] : basis) {
    (void)pivot;
    output.push_back(std::move(row));
  }
  return partial ? MatrixStatus::Partial : MatrixStatus::Complete;
}

std::vector<size_t> source_union(const std::vector<MatrixRow> &rows) {
  std::set<size_t> sources;
  for (const MatrixRow &row : rows)
    sources.insert(row.source_equations.begin(), row.source_equations.end());
  return {sources.begin(), sources.end()};
}

std::vector<External> variable_union(const std::vector<MatrixRow> &rows) {
  std::set<External> variables;
  for (const MatrixRow &row : rows)
    variables.insert(row.variables.begin(), row.variables.end());
  return {variables.begin(), variables.end()};
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
  // Support variables with their parity in the source sum. A conflict whose
  // sources cancel every variable is the unconditional 0 = 1: the certificate
  // refutes it outright, so it consumed no assignment and its reason is the
  // empty clause even when support variables are assigned.
  std::map<External, bool> support;
  size_t residue = 0;
  for (size_t source : event.row.source_equations)
    for (External variable : sources[source].variables) {
      const auto [entry, inserted] = support.emplace(variable, true);
      if (!inserted) entry->second = !entry->second;
      if (entry->second) ++residue; else --residue;
    }
  if (event.kind == MatrixEventKind::Conflict && !residue) return;
  for (const MatrixAssignment assignment : trail)
    if (support.contains(assignment.variable))
      event.reason.push_back(false_literal(assignment));
  if (event.kind == MatrixEventKind::Implication)
    event.reason.push_back(event.implied_value ? event.implied_variable
                                               : -event.implied_variable);
}

bool same_equation(const Equation &first, const Equation &second) {
  return first.variables == second.variables && first.rhs == second.rhs &&
         first.premises == second.premises &&
         first.premise_clauses == second.premise_clauses;
}

bool same_event(const MatrixEvent &first, const MatrixEvent &second) {
  return first.kind == second.kind && first.row == second.row &&
         first.implied_variable == second.implied_variable &&
         first.implied_value == second.implied_value &&
         first.reason == second.reason;
}

bool valid_extension(const std::vector<MatrixAssignment> &prefix,
                     const std::vector<MatrixAssignment> &trail) {
  if (trail.size() < prefix.size() ||
      !std::equal(prefix.begin(), prefix.end(), trail.begin()))
    return false;
  std::set<External> variables;
  for (const MatrixAssignment assignment : prefix)
    variables.insert(assignment.variable);
  for (size_t i = prefix.size(); i < trail.size(); ++i) {
    const MatrixAssignment assignment = trail[i];
    if (assignment.variable <= 0 || assignment.variable == INT32_MIN)
      return false;
    if (!variables.insert(assignment.variable).second) return false;
  }
  return true;
}

} // namespace

MatrixStatus build_affine_matrix(
    const std::vector<Equation> &input, uint64_t owner_identity,
    const MatrixLimits &limits, const std::function<bool(uint64_t)> &charge,
    AffineMatrix &output, bool bounded_geometry) {
  if (!owner_identity || !charge || !limits.maximum_work ||
      !limits.maximum_rows || !limits.maximum_variables ||
      !limits.maximum_row_width || !limits.maximum_sources ||
      !limits.maximum_assignments)
    return MatrixStatus::Invalid;
  if (input.empty()) return MatrixStatus::NoStructure;

  uint64_t source_width_bound = 0;
  std::vector<MatrixRow> seeds;
  seeds.reserve(input.size());
  for (size_t source = 0; source < input.size(); ++source) {
    const Equation &equation = input[source];
    source_width_bound = std::max(source_width_bound,uint64_t(equation.variables.size()));
    if (!valid_equation(equation)) return MatrixStatus::Invalid;
    MatrixRow seed;
    seed.variables = equation.variables;
    seed.rhs = equation.rhs;
    seed.source_equations.push_back(source);
    seeds.push_back(std::move(seed));
  }

  Work work{0, limits.maximum_work, charge};
  std::vector<MatrixRow> rows;
  size_t coefficient_rank = 0;
  const MatrixStatus status = reduce(seeds, {}, limits, work, rows, false, &coefficient_rank);
  if (status == MatrixStatus::Interrupted || status == MatrixStatus::Invalid)
    return status;
  if (rows.empty())
    return status == MatrixStatus::Partial ? MatrixStatus::Exhausted
                                          : MatrixStatus::NoStructure;

  AffineMatrix candidate;
  candidate.source_width_bound_ = source_width_bound;
  candidate.owner_identity_ = owner_identity;
  candidate.work_ = work.used;
  candidate.complete_admission_ = status == MatrixStatus::Complete;
  candidate.limits_ = limits;
  candidate.sources_ = input;
  candidate.admitted_rows_ = rows;
  candidate.admitted_sources_ = source_union(rows);
  __uint128_t source_support=0;
  for(size_t id:candidate.admitted_sources_) source_support+=input[id].variables.size();
  candidate.source_support_bound_=source_support>UINT64_MAX?UINT64_MAX:uint64_t(source_support);
  candidate.admitted_variables_ = variable_union(rows);
  if (bounded_geometry) {
    // Subsequent row operations only condition/combine this admitted basis.
    // They cannot introduce a coordinate or source ordinal outside its union.
    // A source ordinal's numerical value is not its provenance cardinality.
    candidate.limits_.maximum_row_width = std::min<uint64_t>(limits.maximum_row_width,
        std::max<size_t>(1, candidate.admitted_variables_.size()));
    candidate.limits_.maximum_sources = std::min<uint64_t>(limits.maximum_sources,
        std::max<size_t>(1, candidate.admitted_sources_.size()));
  }
  candidate.basis_rank_ = coefficient_rank;
  candidate.rows_ = std::move(rows);
  candidate.incremental_ready_ = true;
  output = std::move(candidate);
  return status;
}

MatrixStatus AffineMatrix::project(const std::vector<External> &unavailable,
    const std::function<bool(uint64_t)> &charge) {
  if (!charge || !std::is_sorted(unavailable.begin(), unavailable.end()) ||
      std::adjacent_find(unavailable.begin(), unavailable.end()) != unavailable.end() ||
      std::any_of(unavailable.begin(), unavailable.end(), [](External v){return v <= 0;}))
    return MatrixStatus::Invalid;
  continuation_ = {};
  if (unavailable.empty()) return MatrixStatus::Complete;
  Work work{0, limits_.maximum_work, charge};
  for (const auto &row : admitted_rows_)
    if (!work.consume(1 + row.variables.size() + row.source_equations.size()))
      return work.interrupted ? MatrixStatus::Interrupted : MatrixStatus::Exhausted;
  auto rows = admitted_rows_;
  for (External variable : unavailable) {
    size_t pivot = rows.size();
    for (size_t i = 0; i < rows.size(); ++i) {
      if (!work.consume(1 + rows[i].variables.size()))
        return work.interrupted ? MatrixStatus::Interrupted : MatrixStatus::Exhausted;
      if (std::binary_search(rows[i].variables.begin(), rows[i].variables.end(), variable)) {
        pivot = i; break;
      }
    }
    if (pivot == rows.size()) continue;
    for (size_t i = 0; i < rows.size(); ++i) {
      if (!work.consume(1 + rows[i].variables.size()))
        return work.interrupted ? MatrixStatus::Interrupted : MatrixStatus::Exhausted;
      if (i == pivot || !std::binary_search(rows[i].variables.begin(), rows[i].variables.end(), variable)) continue;
      const auto status = combine(rows[i], rows[pivot], limits_, work);
      if (status != CombineStatus::Complete)
        return status == CombineStatus::Interrupted ? MatrixStatus::Interrupted : MatrixStatus::Exhausted;
    }
    rows.erase(rows.begin() + pivot);
  }
  std::vector<MatrixRow> reduced;
  size_t rank = 0;
  const auto status = reduce(rows, {}, limits_, work, reduced, false, &rank);
  if (status != MatrixStatus::Complete) return status;
  basis_rank_ = rank;
  admitted_rows_ = std::move(reduced);
  admitted_sources_ = source_union(admitted_rows_);
  __uint128_t source_support=0;
  for(size_t id:admitted_sources_) source_support+=sources_[id].variables.size();
  source_support_bound_=source_support>UINT64_MAX?UINT64_MAX:uint64_t(source_support);
  admitted_variables_ = variable_union(admitted_rows_);
  rows_ = admitted_rows_; trail_.clear(); incremental_ready_ = false;
  index_ = {}; index_ready_ = false; index_reservation_ = 0;
  predecessor_rows_.clear(); predecessor_trail_.clear(); predecessor_ready_ = false;
  work_ = work.used;
  return MatrixStatus::Complete;
}

MatrixStatus AffineMatrix::synchronize(
    const std::vector<MatrixAssignment> &trail,
    const std::function<bool(uint64_t)> &charge, MatrixEvent &event,
    bool sorted_set_extension, const MatrixIncrementalLimits *incremental_limits) {
  last_synchronization_work_exhausted_ = false;
  continuation_ = {};
  if (!owner_identity_ || !charge) return MatrixStatus::Invalid;
  if (trail.size() > limits_.maximum_assignments)
    return MatrixStatus::Exhausted;
  if (incremental_limits && !sorted_set_extension) return MatrixStatus::Invalid;
  if (incremental_limits && (!incremental_limits->maximum_index_bytes ||
      index_reservation_ > incremental_limits->maximum_index_bytes)) return MatrixStatus::Exhausted;

  Work work{0, limits_.maximum_work, charge, false,
            &last_synchronization_work_exhausted_};
  bool monotone_set = false;
  if (sorted_set_extension) {
    // Both validation/comparison and staging are reserved before inspecting
    // coordinates or allocating the private difference vector.
    if (!work.consume(1 + uint64_t(trail.size()) * 3 + trail_.size()))
      return work.interrupted ? MatrixStatus::Interrupted : MatrixStatus::Exhausted;
    External previous = 0;
    for (const auto assignment : trail) {
      if (assignment.variable <= previous) return MatrixStatus::Invalid;
      previous = assignment.variable;
    }
    monotone_set = true;
    size_t old = 0;
    for (const auto assignment : trail) {
      if (old < trail_.size() && trail_[old].variable < assignment.variable) {
        monotone_set = false; break;
      }
      if (old < trail_.size() && trail_[old].variable == assignment.variable) {
        if (trail_[old].value != assignment.value) { monotone_set = false; break; }
        ++old;
      }
    }
    if (old != trail_.size()) monotone_set = false;
  }

  // An unchanged exact support assignment cannot alter the reduced basis or
  // its deterministic event. Avoid re-reducing it at every Boolean fixpoint.
  if (incremental_ready_ && trail == trail_ && !incremental_limits) {
    MatrixEvent candidate = index_ready_ ? indexed_event() : find_event(rows_);
    make_reason(sources_, trail_, candidate);
    last_dirty_update_ = false; last_dirty_rows_ = 0;
    last_synchronization_incremental_ = true;
    last_synchronization_restored_predecessor_ = false;
    last_synchronization_prefix_ = trail_.size();
    last_synchronization_conditioned_ = 0;
    last_synchronization_kind_ = MatrixSynchronizationKind::Extension;
    if (sorted_set_extension)
      work_ = work.used > UINT64_MAX - work_ ? UINT64_MAX : work_ + work.used;
    event = std::move(candidate);
    return MatrixStatus::Complete;
  }

  // A complete predecessor is already conditioned on this exact snapshot.
  // Restore by move, preserving source ordinals and deterministic event order;
  // a same-length reassignment can never enter this path.
  const bool predecessor_candidate =
      predecessor_ready_ && trail.size() < trail_.size() &&
      trail.size() == predecessor_trail_.size();
  if (predecessor_candidate) {
    if (!work.consume(1 + trail.size()))
      return work.interrupted ? MatrixStatus::Interrupted
                              : MatrixStatus::Exhausted;
  }
  if (predecessor_candidate && trail == predecessor_trail_) {
    RowIndex restored_index;
    uint64_t restored_reservation = index_reservation_;
    if (incremental_limits) {
      const auto status = build_row_index(predecessor_rows_, [&](uint64_t n){ return work.consume(n); },
          incremental_limits->maximum_index_bytes, restored_index);
      if (status != MatrixStatus::Complete) return work.interrupted ? MatrixStatus::Interrupted : status;
      const __uint128_t peak = __uint128_t(restored_index.bytes) * 4;
      if (peak > UINT64_MAX) return MatrixStatus::Exhausted;
      restored_reservation = std::max(restored_reservation, uint64_t(peak));
      if (restored_reservation > incremental_limits->maximum_index_bytes) return MatrixStatus::Exhausted;
    }
    MatrixEvent candidate;
    if (incremental_limits) {
      size_t id=0;
      if (!restored_index.conflicts.empty()) {candidate.kind=MatrixEventKind::Conflict;id=*restored_index.conflicts.begin();}
      else if (!restored_index.singletons.empty()) {candidate.kind=MatrixEventKind::Implication;id=restored_index.singletons.begin()->second;}
      if (candidate.kind!=MatrixEventKind::None) {
        const auto &selected=predecessor_rows_[id];
        if (!work.consume(1+selected.variables.size()+selected.source_equations.size()))
          return work.interrupted ? MatrixStatus::Interrupted : MatrixStatus::Exhausted;
        __uint128_t support=0;
        for(size_t source:selected.source_equations) {
          if(!work.consume(1))return work.interrupted ? MatrixStatus::Interrupted : MatrixStatus::Exhausted;
          if(source>=sources_.size())return MatrixStatus::Invalid;
          support+=sources_[source].variables.size();
        }
        const __uint128_t reason_work=(support+trail.size()+1)*(2*std::bit_width(uint64_t(limits_.maximum_variables))+3);
        const __uint128_t reason_bytes=support*(sizeof(External)+8*sizeof(void*));
        if(reason_work>UINT64_MAX||reason_bytes+restored_index.bytes+index_.bytes>incremental_limits->maximum_index_bytes)
          return MatrixStatus::Exhausted;
        if(!work.consume(uint64_t(reason_work)))return work.interrupted ? MatrixStatus::Interrupted : MatrixStatus::Exhausted;
        candidate.row=selected;
        if(candidate.kind==MatrixEventKind::Implication) {candidate.implied_variable=selected.variables.front();candidate.implied_value=selected.rhs;}
        make_reason(sources_,trail,candidate);
      }
    } else {candidate=find_event(predecessor_rows_);make_reason(sources_,trail,candidate);}
    trail_ = std::move(predecessor_trail_);
    rows_ = std::move(predecessor_rows_);
    predecessor_ready_ = false;
    if (incremental_limits) {
      index_ = std::move(restored_index); index_ready_ = true; index_reservation_ = restored_reservation;
    } else { index_ = {}; index_ready_ = false; index_reservation_ = 0; }
    last_dirty_update_ = false; last_dirty_rows_ = 0;
    incremental_ready_ = true;
    last_synchronization_incremental_ = false;
    last_synchronization_restored_predecessor_ = true;
    last_synchronization_prefix_ = 0;
    last_synchronization_conditioned_ = 0;
    last_synchronization_kind_ = MatrixSynchronizationKind::Backtrack;
    if (work.used > std::numeric_limits<uint64_t>::max() - work_)
      work_ = std::numeric_limits<uint64_t>::max();
    else
      work_ += work.used;
    event = std::move(candidate);
    return MatrixStatus::Complete;
  }
  std::vector<MatrixRow> rows;
  MatrixSynchronizationKind kind = MatrixSynchronizationKind::Extension;
  if (!incremental_ready_)
    kind = MatrixSynchronizationKind::PriorPartial;
  else if (trail.size() < trail_.size())
    kind = MatrixSynchronizationKind::Backtrack;
  else if (!(sorted_set_extension ? monotone_set : valid_extension(trail_, trail)))
    kind = MatrixSynchronizationKind::Reassignment;
  const bool incremental = kind == MatrixSynchronizationKind::Extension;
  const std::vector<MatrixRow> &input = incremental ? rows_ : admitted_rows_;
  std::vector<MatrixAssignment> assignments;
  if (incremental && sorted_set_extension) {
    // The validation pass allocates nothing. Exactly one private assignment
    // workspace exists on every path: delta for extension, full snapshot for
    // rebuild. Its allocation is covered by the preceding 3*n+old reservation.
    assignments.reserve(trail.size());
    size_t old = 0;
    for (const auto assignment : trail) {
      if (old < trail_.size() && trail_[old].variable == assignment.variable) ++old;
      else assignments.push_back(assignment);
    }
  }
  else if (incremental)
    assignments.assign(trail.begin() + trail_.size(), trail.end());
  else
    assignments = trail;
  if (incremental_limits && !sorted_set_extension) return MatrixStatus::Invalid;
  if (incremental_limits && incremental && index_ready_) {
    uint64_t dirty_work = 0;
    const auto status = synchronize_dirty(assignments, trail, charge,
        limits_.maximum_work - work.used, incremental_limits->maximum_index_bytes, dirty_work, event);
    if (status == MatrixStatus::Complete)
      work_ = work.used > UINT64_MAX - work_ ? UINT64_MAX : work_ + work.used;
    return status;
  }
  const MatrixStatus status =
      reduce(input, assignments, limits_, work, rows, incremental_limits != nullptr);
  if (status != MatrixStatus::Complete && status != MatrixStatus::Partial)
    return status;
  // Persistent set updates preserve the previous complete basis on a bounded
  // partial reduction; no admitted source silently disappears from that basis.
  if (sorted_set_extension && status == MatrixStatus::Partial) return MatrixStatus::Exhausted;
  if (rows.empty() && status == MatrixStatus::Partial)
    return MatrixStatus::Exhausted;

  RowIndex candidate_index;
  uint64_t candidate_reservation = 0;
  if (incremental_limits) {
    const auto index_status = build_row_index(rows, [&](uint64_t n){ return work.consume(n); },
        incremental_limits->maximum_index_bytes, candidate_index);
    if (index_status != MatrixStatus::Complete)
      return work.interrupted ? MatrixStatus::Interrupted : index_status;
    const __uint128_t peak = __uint128_t(candidate_index.bytes) * 4;
    candidate_reservation = std::max(index_reservation_, peak > UINT64_MAX ? UINT64_MAX : uint64_t(peak));
    if (peak > UINT64_MAX || candidate_reservation > incremental_limits->maximum_index_bytes)
      return MatrixStatus::Exhausted;
  }
  MatrixEvent candidate;
  if (incremental_limits) {
    size_t id=0;
    if (!candidate_index.conflicts.empty()) {candidate.kind=MatrixEventKind::Conflict;id=*candidate_index.conflicts.begin();}
    else if (!candidate_index.singletons.empty()) {candidate.kind=MatrixEventKind::Implication;id=candidate_index.singletons.begin()->second;}
    if (candidate.kind!=MatrixEventKind::None) {
      const auto &selected=rows[id];
      if (!work.consume(1+selected.variables.size()+selected.source_equations.size()))
        return work.interrupted ? MatrixStatus::Interrupted : MatrixStatus::Exhausted;
      __uint128_t support=0;
      for(size_t source:selected.source_equations) {
        if(!work.consume(1))return work.interrupted ? MatrixStatus::Interrupted : MatrixStatus::Exhausted;
        if(source>=sources_.size())return MatrixStatus::Invalid;
        support+=sources_[source].variables.size();
      }
      const __uint128_t reason_work=(support+trail.size()+1)*(2*std::bit_width(uint64_t(limits_.maximum_variables))+3);
      const __uint128_t reason_bytes=support*(sizeof(External)+8*sizeof(void*));
      if(reason_work>UINT64_MAX||reason_bytes+candidate_index.bytes+index_.bytes>incremental_limits->maximum_index_bytes)
        return MatrixStatus::Exhausted;
      if(!work.consume(uint64_t(reason_work)))return work.interrupted ? MatrixStatus::Interrupted : MatrixStatus::Exhausted;
      candidate.row=selected;
      if(candidate.kind==MatrixEventKind::Implication) {candidate.implied_variable=selected.variables.front();candidate.implied_value=selected.rhs;}
      make_reason(sources_,trail,candidate);
    }
  } else {candidate=find_event(rows);make_reason(sources_,trail,candidate);}
  // Current and predecessor share the existing mutable row/trail population
  // allowances. Full matrices simply keep the established rebuild behavior.
  if (incremental && !incremental_limits && status == MatrixStatus::Complete &&
      rows_.size() <= limits_.maximum_rows - rows.size() &&
      trail_.size() <= limits_.maximum_assignments - trail.size()) {
    predecessor_trail_ = std::move(trail_);
    predecessor_rows_ = std::move(rows_);
    predecessor_ready_ = true;
  } else {
    predecessor_ready_ = false;
    predecessor_trail_.clear();
    predecessor_rows_.clear();
  }
  // Assignment-memory peak is at most four support-sized populations:
  // current trail capacity, retained predecessor capacity (clear may retain
  // it), the single private workspace above, and a replacement allocation for
  // trail_. This is the 4*support*sizeof(MatrixAssignment) admission reserve.
  // Moving the current trail into the predecessor releases the old predecessor
  // storage before allocating the destination; it cannot add a fifth buffer.
  trail_ = trail;
  rows_ = std::move(rows);
  if (incremental_limits) {
    index_ = std::move(candidate_index); index_ready_ = true; index_reservation_ = candidate_reservation;
  } else { index_ = {}; index_ready_ = false; index_reservation_ = 0; }
  last_dirty_update_ = false; last_dirty_rows_ = 0;
  incremental_ready_ = status == MatrixStatus::Complete;
  last_synchronization_incremental_ = incremental;
  last_synchronization_restored_predecessor_ = false;
  last_synchronization_prefix_ = incremental ? trail_.size() - assignments.size()
                                               : 0;
  last_synchronization_conditioned_ = assignments.size();
  last_synchronization_kind_ = kind;
  if (work.used > std::numeric_limits<uint64_t>::max() - work_)
    work_ = std::numeric_limits<uint64_t>::max();
  else
    work_ += work.used;
  event = std::move(candidate);
  return status;
}

MatrixStatus AffineMatrix::synchronize_sliced(
    const std::vector<MatrixAssignment> &trail,
    const std::function<bool(uint64_t)> &charge, MatrixEvent &event,
    const MatrixSliceLimits &slice) {
  slice_effort_ = {};
  slice_effort_.cumulative_work = continuation_.cumulative_work;
  struct Accounting {
    uint64_t &total; MatrixSliceEffort &effort; Continuation &state;
    ~Accounting() {
      total = effort.work > UINT64_MAX-total ? UINT64_MAX : total+effort.work;
      effort.cumulative_work = effort.work > UINT64_MAX-effort.cumulative_work ?
          UINT64_MAX : effort.cumulative_work+effort.work;
      if (state.active) state.cumulative_work = effort.cumulative_work;
    }
  } accounting{work_,slice_effort_,continuation_};
  if (!owner_identity_ || !charge || !slice.maximum_work || !slice.maximum_bytes ||
      slice.maximum_work > limits_.maximum_work)
    return MatrixStatus::Invalid;
  if (trail.size() > limits_.maximum_assignments) return MatrixStatus::Exhausted;
  const uint64_t support_bound=source_support_bound_;
  const uint64_t comparisons = 2 * std::bit_width(std::max({
      uint64_t(admitted_rows_.size()),uint64_t(admitted_variables_.size()),
      uint64_t(trail.size()),support_bound})) + 3;
  auto take = [&](uint64_t amount) {
    slice_effort_.minimum_slice_work = std::max(slice_effort_.minimum_slice_work, amount);
    if (amount > slice.maximum_work - slice_effort_.work) return false;
    if (!charge(amount)) return false;
    slice_effort_.work += amount;
    return true;
  };
  // Compare/copy the exact input only after its scan charge. These costs are
  // distinct from retained algebra; no completed row reduction is repeated.
  const __uint128_t wide_boundary=1+__uint128_t(trail.size())+continuation_.trail.size();
  if (wide_boundary>UINT64_MAX) return MatrixStatus::Exhausted;
  const uint64_t boundary_work=uint64_t(wide_boundary);
  if (boundary_work > slice.maximum_work) {
    slice_effort_.minimum_slice_work = boundary_work;
    return MatrixStatus::Exhausted;
  }
  if (!take(boundary_work)) { continuation_ = {}; return MatrixStatus::Interrupted; }
  const bool same = continuation_.active && continuation_.trail == trail;
  slice_effort_.resumed = same;
  if (same) slice_effort_.cumulative_work = continuation_.cumulative_work;
  slice_effort_.invalidated = continuation_.active && !same;
  if (!same) {
    slice_effort_.cumulative_work = 0;
    continuation_ = {};
    // Tree metadata, mutable row populations and scratch XOR/reason buffers.
    // This reservation excludes the already-owned immutable/current matrix.
    const uint64_t row_variables=std::min<uint64_t>(admitted_variables_.size(),limits_.maximum_row_width);
    const uint64_t row_sources=std::min<uint64_t>(admitted_sources_.size(),limits_.maximum_sources);
    const __uint128_t bytes = sizeof(Continuation) +
        __uint128_t(trail.size()) * (sizeof(MatrixAssignment) + sizeof(std::pair<const External,bool>) + 8*sizeof(void*)) +
        __uint128_t(admitted_rows_.size()) * (4*sizeof(MatrixRow) + 16*sizeof(void*)) +
        4*(__uint128_t(row_variables)*sizeof(External)+__uint128_t(row_sources)*sizeof(size_t)) +
        __uint128_t(support_bound) * (sizeof(External)+8*sizeof(void*)) +
        (__uint128_t(trail.size())+1)*sizeof(External);
    if (bytes > slice.maximum_bytes) return MatrixStatus::Exhausted;
    if (trail.size() > slice.maximum_work - slice_effort_.work) return MatrixStatus::Exhausted;
    if (!take(trail.size())) return MatrixStatus::Interrupted;
    continuation_.trail = trail;
    continuation_.bytes = continuation_.base_bytes = uint64_t(bytes);
    continuation_.active = true;
  } else if (continuation_.bytes > slice.maximum_bytes) return MatrixStatus::Exhausted;
  auto &state = continuation_;
  bool stopped = false;
  auto step = [&](__uint128_t requested) {
    if (requested + boundary_work > UINT64_MAX) {
      slice_effort_.minimum_slice_work = UINT64_MAX;return false;
    }
    const uint64_t amount=uint64_t(requested);
    slice_effort_.minimum_slice_work = std::max(slice_effort_.minimum_slice_work, amount + boundary_work);
    if (amount > slice.maximum_work - slice_effort_.work) return false;
    if (!charge(amount)) { stopped = true; return false; }
    slice_effort_.work += amount;
    ++slice_effort_.completed_steps;
    return true;
  };
  auto pending = [&]() {
    if (stopped) { continuation_ = {}; return MatrixStatus::Interrupted; }
    return slice_effort_.minimum_slice_work > slice.maximum_work ?
        MatrixStatus::Exhausted : MatrixStatus::Pending;
  };
  for (;;) {
    if (state.phase == 0) {
      if (state.assignment < state.trail.size()) {
        if (!step(1 + comparisons)) return pending();
        const auto value = state.trail[state.assignment];
        if (value.variable <= 0 || (state.assignment &&
            state.trail[state.assignment-1].variable>=value.variable) ||
            !state.assignments.emplace(value.variable,value.value).second) {
          continuation_ = {}; return MatrixStatus::Invalid;
        }
        ++state.assignment; continue;
      }
      state.phase = 1;
    }
    if (state.phase == 1) {
      if (state.seed == admitted_rows_.size()) { state.phase = 2; continue; }
      if (!state.row_loaded) {
        const auto &seed = admitted_rows_[state.seed];
        if (!step(1 + __uint128_t(seed.variables.size())*comparisons + seed.source_equations.size())) return pending();
        state.row.rhs = seed.rhs;
        state.row.source_equations = seed.source_equations;
        for (External variable : seed.variables) {
          auto found = state.assignments.find(variable);
          if (found == state.assignments.end()) state.row.variables.push_back(variable);
          else state.row.rhs ^= found->second;
        }
        state.row_loaded = true;
      }
        if (state.combine_pivot) {
        const auto &other=state.basis.at(state.combine_pivot);
        if (!step(1 + state.row.variables.size() + other.variables.size() +
                  state.row.source_equations.size() + other.source_equations.size())) return pending();
        std::vector<External> variables; std::vector<size_t> sources;
        xor_sorted(state.row.variables,other.variables,variables);
        xor_sorted(state.row.source_equations,other.source_equations,sources);
        if (variables.size()>limits_.maximum_row_width || sources.size()>limits_.maximum_sources) {
          continuation_={};return MatrixStatus::Exhausted;
        }
        state.row.variables=std::move(variables);state.row.source_equations=std::move(sources);
        state.row.rhs ^= other.rhs;state.combine_pivot=0;
        continue;
      }
      if (!step(1 + comparisons)) return pending();
      // Reserve finalized sparse row payload before its ownership transfer.
      const __uint128_t payload=4*(__uint128_t(state.row.variables.size())*sizeof(External)+
          __uint128_t(state.row.source_equations.size())*sizeof(size_t));
      const __uint128_t next_bytes=__uint128_t(state.base_bytes)+state.payload_bytes+payload;
      if (next_bytes>slice.maximum_bytes) return MatrixStatus::Exhausted;
      if (!state.row.variables.empty()) {
        auto pivot=state.basis.find(state.row.variables.front());
        if (pivot!=state.basis.end()) {state.combine_pivot=pivot->first;continue;}
        state.payload_bytes+=uint64_t(payload);
        state.bytes=uint64_t(next_bytes);
        state.basis.emplace(state.row.variables.front(),std::move(state.row));
      } else if (state.row.rhs) {
        state.payload_bytes+=uint64_t(payload);state.bytes=uint64_t(next_bytes);
        state.contradictions.push_back(std::move(state.row));
      }
      state.row = {}; state.row_loaded=false; ++state.seed;
      continue;
    }
    if (state.phase == 2) {
      if (!state.contradictions.empty()) {
        if (!step(1)) return pending();
        // Preserve contradiction insertion order, followed by ascending pivots.
        state.rows = std::move(state.contradictions);
      }
      if (!state.basis.empty()) {
        if (!step(1 + comparisons)) return pending();
        auto first=state.basis.begin();state.rows.push_back(std::move(first->second));state.basis.erase(first);
        continue;
      }
      state.phase=8;state.scan=state.rows.size();
    }
    if (state.phase==8) {
      // Gauss-Jordan back-substitution, as in reduce(): descending pivots,
      // one row XOR per step. Pivot rows follow the contradictions in
      // ascending pivot order.
      const auto pivot_rows=std::partition_point(state.rows.begin(),state.rows.end(),
          [](const MatrixRow &row){return row.variables.empty();});
      const size_t first_pivot=size_t(pivot_rows-state.rows.begin());
      if (state.scan==first_pivot) {state.phase=3;state.scan=0;continue;}
      auto &row=state.rows[state.scan-1];
      if (!step(1 + __uint128_t(row.variables.size())*comparisons)) return pending();
      const MatrixRow *other=nullptr;
      for (size_t at=1; at<row.variables.size() && !other; ++at) {
        const External variable=row.variables[at];
        const auto found=std::lower_bound(pivot_rows,state.rows.end(),variable,
            [](const MatrixRow &candidate,External value){return candidate.variables.front()<value;});
        if (found!=state.rows.end() && found->variables.front()==variable) other=&*found;
      }
      if (!other) {--state.scan;continue;}
      if (!step(1 + row.variables.size() + other->variables.size() +
                row.source_equations.size() + other->source_equations.size())) return pending();
      std::vector<External> variables; std::vector<size_t> sources;
      xor_sorted(row.variables,other->variables,variables);
      xor_sorted(row.source_equations,other->source_equations,sources);
      if (variables.size()>limits_.maximum_row_width || sources.size()>limits_.maximum_sources) {
        continuation_={};return MatrixStatus::Exhausted;
      }
      const uint64_t before=4*(uint64_t(row.variables.size())*sizeof(External)+
          uint64_t(row.source_equations.size())*sizeof(size_t));
      const uint64_t after=4*(uint64_t(variables.size())*sizeof(External)+
          uint64_t(sources.size())*sizeof(size_t));
      if (after>before) {
        const __uint128_t next_bytes=__uint128_t(state.bytes)+(after-before);
        if (next_bytes>slice.maximum_bytes) return MatrixStatus::Exhausted;
        state.bytes=uint64_t(next_bytes);state.payload_bytes+=after-before;
      }
      row.variables=std::move(variables);row.source_equations=std::move(sources);
      row.rhs^=other->rhs;
      continue;
    }
    if (state.phase==3 || state.phase==4) {
      if (state.scan<state.rows.size()) {
        const auto &row=state.rows[state.scan];
        if (!step(1 + row.variables.size()+row.source_equations.size())) return pending();
        const bool selected=state.phase==3 ? row.variables.empty()&&row.rhs : row.variables.size()==1;
        ++state.scan;
        if (!selected) continue;
        state.event.kind=state.phase==3?MatrixEventKind::Conflict:MatrixEventKind::Implication;
        state.event.row=row;
        if (state.phase==4) {state.event.implied_variable=row.variables.front();state.event.implied_value=row.rhs;}
        state.phase=5;continue;
      }
      if (state.phase==3) {state.phase=4;state.scan=0;continue;}
      state.phase=7;continue;
    }
    if (state.phase==5) {
      if (state.reason_source<state.event.row.source_equations.size()) {
        const size_t id=state.event.row.source_equations[state.reason_source];
        if (id>=sources_.size()) {continuation_={};return MatrixStatus::Invalid;}
        const auto &source=sources_[id];
        if (!step(1+__uint128_t(source.variables.size())*comparisons)) return pending();
        for (External variable : source.variables) {
          const auto [entry,inserted]=state.support.emplace(variable,true);
          if (!inserted) entry->second=!entry->second;
          if (entry->second) ++state.residue; else --state.residue;
        }
        ++state.reason_source;continue;
      }
      // An unconditional contradiction keeps the empty reason (make_reason).
      if (state.event.kind==MatrixEventKind::Conflict && !state.residue)
        state.reason_trail=state.trail.size();
      state.phase=6;
    }
    if (state.phase==6) {
      if (state.reason_trail<state.trail.size()) {
        if (!step(1+comparisons)) return pending();
        const auto value=state.trail[state.reason_trail++];
        if (state.support.contains(value.variable)) state.event.reason.push_back(false_literal(value));
        continue;
      }
      if (state.event.kind==MatrixEventKind::Implication) {
        if (!step(1)) return pending();
        state.event.reason.push_back(state.event.implied_value?state.event.implied_variable:-state.event.implied_variable);
      }
      state.phase=7;
    }
    if (state.phase==7) {
      if (!step(1)) return pending();
      trail_=std::move(state.trail);rows_=std::move(state.rows);event=std::move(state.event);
      incremental_ready_=true;index_={};index_ready_=false;index_reservation_=0;
      predecessor_ready_=false;predecessor_rows_.clear();predecessor_trail_.clear();
      last_dirty_update_=false;last_dirty_rows_=0;
      last_synchronization_incremental_=false;last_synchronization_restored_predecessor_=false;
      last_synchronization_prefix_=0;
      last_synchronization_conditioned_=trail_.size();
      last_synchronization_kind_=MatrixSynchronizationKind::PriorPartial;
      continuation_={};
      return MatrixStatus::Complete;
    }
  }
}

namespace {
// Conservative pinned GNU tree allocation envelope: three RB-tree links,
// colour/alignment padding and allocator bookkeeping fit within eight pointer
// widths beyond the stored value. This is an admission reserve, not sizeof
// an implementation-private node or an exact allocator-usage claim.
template<class T> constexpr uint64_t index_node_bytes() { return sizeof(T) + 8 * sizeof(void *); }
constexpr uint64_t occurrence_node = index_node_bytes<std::pair<const External,std::vector<size_t>>>();
constexpr uint64_t pivot_node = index_node_bytes<std::pair<const External,size_t>>();
constexpr uint64_t conflict_node = index_node_bytes<size_t>();
}

MatrixStatus AffineMatrix::build_row_index(const std::vector<MatrixRow> &rows,
    const std::function<bool(uint64_t)> &charge, uint64_t maximum, RowIndex &out) {
  RowIndex candidate;
  candidate.bytes = sizeof(RowIndex);
  std::map<External,size_t> counts;
  const uint64_t comparisons = 2 * std::bit_width(uint64_t(std::max(limits_.maximum_rows, limits_.maximum_variables))) + 3;
  for (size_t id=0;id<rows.size();++id) {
    if (!charge(1)) return MatrixStatus::Exhausted;
    const auto &row=rows[id];
    if (!charge(1 + row.variables.size() * comparisons)) return MatrixStatus::Exhausted;
    for (External variable:row.variables) {
      if (!counts.contains(variable)) {
        if (occurrence_node > maximum/4 - std::min(candidate.bytes,maximum/4)) return MatrixStatus::Exhausted;
        candidate.bytes+=occurrence_node;
      }
      ++counts[variable];
      if (sizeof(size_t)>maximum/4-std::min(candidate.bytes,maximum/4)) return MatrixStatus::Exhausted;
      candidate.bytes+=sizeof(size_t);
    }
    const uint64_t metadata=row.variables.empty() ? (row.rhs ? conflict_node : 0) :
        pivot_node + (row.variables.size()==1 ? pivot_node : 0);
    if (metadata>maximum/4-std::min(candidate.bytes,maximum/4)) return MatrixStatus::Exhausted;
    candidate.bytes+=metadata;
  }
  if (candidate.bytes>maximum/4) return MatrixStatus::Exhausted;
  for (const auto &[variable,count]:counts) {
    if (!charge(1+count)) return MatrixStatus::Exhausted;
    auto &list=candidate.occurrences[variable];list.reserve(count);
  }
  for (size_t id=0;id<rows.size();++id) {
    const auto &row=rows[id];
    if (!charge(1+row.variables.size()*comparisons)) return MatrixStatus::Exhausted;
    for (External variable:row.variables) candidate.occurrences.at(variable).push_back(id);
    if (row.variables.empty()) { if(row.rhs)candidate.conflicts.insert(id); }
    else {
      if (!candidate.pivots.emplace(row.variables.front(),id).second) return MatrixStatus::Invalid;
      if(row.variables.size()==1)candidate.singletons.emplace(row.variables.front(),id);
    }
  }
  out=std::move(candidate);return MatrixStatus::Complete;
}

MatrixEvent AffineMatrix::indexed_event() const {
  MatrixEvent event;
  size_t id=0;
  if(!index_.conflicts.empty()){event.kind=MatrixEventKind::Conflict;id=*index_.conflicts.begin();}
  else if(!index_.singletons.empty()){
    event.kind=MatrixEventKind::Implication;id=index_.singletons.begin()->second;
    event.implied_variable=index_.singletons.begin()->first;event.implied_value=rows_[id].rhs;
  }else return event;
  event.row=rows_[id];return event;
}

MatrixStatus AffineMatrix::synchronize_dirty(const std::vector<MatrixAssignment> &delta,
    const std::vector<MatrixAssignment> &trail,const std::function<bool(uint64_t)> &charge,
    uint64_t maximum_work,uint64_t maximum_bytes,uint64_t &used,MatrixEvent &event) {
  Work work{0,maximum_work,charge,false,&last_synchronization_work_exhausted_};
  auto declined=[&](){used=work.used;return work.interrupted ? MatrixStatus::Interrupted : MatrixStatus::Exhausted;};
  if(index_reservation_>maximum_bytes)return declined();
  uint64_t staging=0;
  auto memory=[&](uint64_t bytes){
    if(bytes>maximum_bytes-std::min(index_.bytes,maximum_bytes) ||
       staging>maximum_bytes-index_.bytes-bytes)return false;
    staging+=bytes;return true;
  };
  const uint64_t comparisons=2*std::bit_width(uint64_t(std::max({limits_.maximum_rows,
      limits_.maximum_variables,limits_.maximum_assignments})))+3;
  std::set<size_t> dirty;
  std::map<External,bool> values;
  for(const auto assignment:delta){
    if(!work.consume(1+comparisons)||!memory(pivot_node))return declined();
    values.emplace(assignment.variable,assignment.value);
    auto found=index_.occurrences.find(assignment.variable);
    if(found==index_.occurrences.end())continue;
    for(size_t id:found->second){
      if(!work.consume(1+comparisons))return declined();
      if(!dirty.contains(id)){
        if(!memory(conflict_node))return declined();
        dirty.insert(id);
      }
    }
  }
  // Optional old predecessors remain intact until success. Account their
  // retained row buffers separately before creating a private transaction.
  for(const auto &row:predecessor_rows_){
    if(!work.consume(1)||!memory(sizeof(MatrixRow)+row.variables.size()*sizeof(External)+
        row.source_equations.size()*sizeof(size_t)))return declined();
  }
  std::map<size_t,MatrixRow> replacement;
  RowIndex additions;
  const uint64_t row_reserve=index_node_bytes<std::pair<const size_t,MatrixRow>>() +
      6 * (uint64_t(limits_.maximum_row_width)*sizeof(External) +
           uint64_t(limits_.maximum_sources)*sizeof(size_t));
  std::map<External,size_t> staged_pivots;
  for(size_t id:dirty){
    const auto &old=rows_[id];
    if(!work.consume(1+old.variables.size()*comparisons+old.source_equations.size()) ||
       !memory(row_reserve))return declined();
    MatrixRow row;row.rhs=old.rhs;row.source_equations=old.source_equations;
    row.variables.reserve(old.variables.size());
    for(External variable:old.variables){
      auto assigned=values.find(variable);
      if(assigned==values.end())row.variables.push_back(variable);else row.rhs^=assigned->second;
    }
    while(!row.variables.empty()){
      if(!work.consume(1+comparisons))return declined();
      const External pivot=row.variables.front();
      const MatrixRow *other=nullptr;
      auto staged=staged_pivots.find(pivot);
      if(staged!=staged_pivots.end())other=&replacement.at(staged->second);
      else{
        auto retained=index_.pivots.find(pivot);
        if(retained!=index_.pivots.end()&&!dirty.contains(retained->second))other=&rows_[retained->second];
      }
      if(!other)break;
      const auto status=combine(row,*other,limits_,work);
      if(status!=CombineStatus::Complete)return declined();
    }
    if(!bounded(row,limits_))return declined();
    if(!work.consume(1+comparisons))return declined();
    if(!row.variables.empty()){
      if(!memory(pivot_node))return declined();
      staged_pivots.emplace(row.variables.front(),id);
    }
    replacement.emplace(id,std::move(row));
  }
  // Gauss-Jordan column elimination (CryptoMiniSat EGaussian::eliminate_col):
  // a row whose pivot was assigned takes a new pivot, which is removed from
  // every other row, by descending new pivot. Its other coordinates are
  // non-pivots, so the remaining rows keep their pivots.
  std::vector<std::pair<External,size_t>> new_pivots;
  for(const auto &[id,row]:replacement){
    if(!work.consume(1+comparisons))return declined();
    const auto &old=rows_[id];
    if(!row.variables.empty()&&(old.variables.empty()||old.variables.front()!=row.variables.front()))
      new_pivots.emplace_back(row.variables.front(),id);
  }
  std::sort(new_pivots.begin(),new_pivots.end(),std::greater<>());
  for(const auto &[pivot,pivot_id]:new_pivots){
    std::vector<size_t> candidates;
    auto retained=index_.occurrences.find(pivot);
    if(retained!=index_.occurrences.end()){
      if(!work.consume(1+retained->second.size()))return declined();
      candidates=retained->second;
    }
    for(const auto &[id,row]:replacement){(void)row;candidates.push_back(id);}
    if(!work.consume(1+candidates.size()*comparisons))return declined();
    std::sort(candidates.begin(),candidates.end());
    candidates.erase(std::unique(candidates.begin(),candidates.end()),candidates.end());
    for(size_t id:candidates){
      if(id==pivot_id)continue;
      auto staged=replacement.find(id);
      const MatrixRow &current=staged==replacement.end()?rows_[id]:staged->second;
      if(!work.consume(1+comparisons*std::bit_width(uint64_t(current.variables.size()))))return declined();
      if(!std::binary_search(current.variables.begin(),current.variables.end(),pivot))continue;
      if(staged==replacement.end()){
        if(!work.consume(1+current.variables.size()+current.source_equations.size())||
           !memory(row_reserve+conflict_node))return declined();
        staged=replacement.emplace(id,rows_[id]).first;
        dirty.insert(id);
      }
      const auto status=combine(staged->second,replacement.at(pivot_id),limits_,work);
      if(status!=CombineStatus::Complete)return declined();
    }
  }
  for(auto &[id,row]:replacement){
    // A zero equation is a tombstone in its stable slot; it cannot be an event.
    if(row.variables.empty()&&!row.rhs)row.source_equations.clear();
    if(!work.consume(1+comparisons))return declined();
    if(!row.variables.empty()){
      if(!memory(pivot_node))return declined();
      additions.pivots.emplace(row.variables.front(),id);
      if(row.variables.size()==1){if(!memory(pivot_node))return declined();additions.singletons.emplace(row.variables.front(),id);}
    }else if(row.rhs){if(!memory(conflict_node))return declined();additions.conflicts.insert(id);}
  }
  // Copy only occurrence lists touched by the private rows. The sorted IDs
  // remain stable; unrelated row bodies and occurrence lists are never copied.
  std::map<External,std::vector<size_t>> lists;
  auto list_for=[&](External variable)->std::vector<size_t>*{
    if(!work.consume(1+comparisons))return nullptr;
    auto staged=lists.find(variable);if(staged!=lists.end())return &staged->second;
    auto retained=index_.occurrences.find(variable);
    const size_t count=retained==index_.occurrences.end()?0:retained->second.size();
    // Up to one entry per dirty row may be newly inserted into this list.
    const uint64_t capacity=count+dirty.size();
    if(!work.consume(1+count)||!memory(occurrence_node+capacity*sizeof(size_t)))return nullptr;
    auto &list=lists[variable];list.reserve(capacity);
    if(retained!=index_.occurrences.end())list.assign(retained->second.begin(),retained->second.end());
    return &list;
  };
  uint64_t next_bytes=index_.bytes;
  for(const auto &[id,row]:replacement){
    const auto &old=rows_[id];
    for(External variable:old.variables){
      auto *list=list_for(variable);if(!list||!work.consume(1+list->size()+comparisons))return declined();
      auto at=std::lower_bound(list->begin(),list->end(),id);
      if(at==list->end()||*at!=id)return MatrixStatus::Invalid;
      list->erase(at);
    }
    for(External variable:row.variables){
      auto *list=list_for(variable);if(!list||!work.consume(1+list->size()+comparisons))return declined();
      list->insert(std::lower_bound(list->begin(),list->end(),id),id);
    }
    const auto metadata=[](const MatrixRow &r){return r.variables.empty()?(r.rhs?conflict_node:0):pivot_node+(r.variables.size()==1?pivot_node:0);};
    next_bytes-=metadata(old);next_bytes+=metadata(row);
  }
  for(const auto &[variable,list]:lists){
    if(!work.consume(1+comparisons))return declined();
    auto old=index_.occurrences.find(variable);
    if(old!=index_.occurrences.end())next_bytes-=occurrence_node+old->second.capacity()*sizeof(size_t);
    next_bytes+=occurrence_node+list.capacity()*sizeof(size_t);
  }
  const __uint128_t reservation=__uint128_t(next_bytes)*4;
  if(reservation>UINT64_MAX||reservation>maximum_bytes)return declined();
  const uint64_t next_reservation=std::max(index_reservation_,uint64_t(reservation));
  if(next_reservation>maximum_bytes)return declined();
  // Merge the two small event queues observationally before committing rows.
  auto conflict=index_.conflicts.begin();
  while(conflict!=index_.conflicts.end()){
    if(!work.consume(1+comparisons))return declined();
    if(!dirty.contains(*conflict))break;
    ++conflict;
  }
  auto singleton=index_.singletons.begin();
  while(singleton!=index_.singletons.end()){
    if(!work.consume(1+comparisons))return declined();
    if(!dirty.contains(singleton->second))break;
    ++singleton;
  }
  MatrixEvent candidate;const MatrixRow *selected=nullptr;
  size_t selected_id=0;
  if(conflict!=index_.conflicts.end()||!additions.conflicts.empty()){
    selected_id=conflict==index_.conflicts.end()?*additions.conflicts.begin():*conflict;
    if(!additions.conflicts.empty())selected_id=std::min(selected_id,*additions.conflicts.begin());
    candidate.kind=MatrixEventKind::Conflict;
  }else if(singleton!=index_.singletons.end()||!additions.singletons.empty()){
    const auto *entry=singleton==index_.singletons.end()?&*additions.singletons.begin():&*singleton;
    if(!additions.singletons.empty()&&additions.singletons.begin()->first<entry->first)entry=&*additions.singletons.begin();
    selected_id=entry->second;candidate.kind=MatrixEventKind::Implication;candidate.implied_variable=entry->first;
  }
  if(candidate.kind!=MatrixEventKind::None){
    auto changed=replacement.find(selected_id);
    selected=changed==replacement.end()?&rows_[selected_id]:&changed->second;
    if(!work.consume(1+selected->variables.size()+selected->source_equations.size()))return declined();
    uint64_t support=0;
    for(size_t source:selected->source_equations){
      if(!work.consume(1))return declined();
      if(source>=sources_.size())return MatrixStatus::Invalid;
      const uint64_t count=sources_[source].variables.size();
      if(count>UINT64_MAX-support)return declined();
      support+=count;
    }
    const uint64_t reason_comparisons=2*std::bit_width(uint64_t(limits_.maximum_variables))+3;
    const __uint128_t reason_work=(__uint128_t(support)+trail.size()+1)*reason_comparisons;
    const __uint128_t reason_bytes=__uint128_t(support)*index_node_bytes<External>();
    if(reason_work>UINT64_MAX||reason_bytes>UINT64_MAX||!work.consume(uint64_t(reason_work))||!memory(uint64_t(reason_bytes)))return declined();
    candidate.row=*selected;
    if(candidate.kind==MatrixEventKind::Implication)candidate.implied_value=selected->rhs;
    make_reason(sources_,trail,candidate);
  }
  if(!work.consume(1+trail.size()+dirty.size()*comparisons+lists.size()*comparisons))return declined();
  // All charged, checked, private preparation is complete. Map nodes transfer
  // without allocation; row/list moves cannot invalidate untouched references.
  for(size_t id:dirty){
    const auto &old=rows_[id];
    if(!old.variables.empty()){index_.pivots.erase(old.variables.front());if(old.variables.size()==1)index_.singletons.erase(old.variables.front());}
    else if(old.rhs)index_.conflicts.erase(id);
  }
  for(auto &[id,row]:replacement)rows_[id]=std::move(row);
  for(auto it=lists.begin();it!=lists.end();){
    auto old=index_.occurrences.find(it->first);
    if(old!=index_.occurrences.end()){old->second=std::move(it->second);it=lists.erase(it);}
    else{auto node=lists.extract(it++);index_.occurrences.insert(std::move(node));}
  }
  index_.pivots.merge(additions.pivots);index_.singletons.merge(additions.singletons);index_.conflicts.merge(additions.conflicts);
  index_.bytes=next_bytes;index_reservation_=next_reservation;
  // Dirty transactions do not copy the entire predecessor basis. Retraction
  // uses the bounded full rebuild; the old optional predecessor is released.
  predecessor_rows_.clear();predecessor_trail_.clear();predecessor_ready_=false;
  last_synchronization_prefix_=trail_.size();trail_=trail;
  incremental_ready_=true;last_synchronization_incremental_=true;
  last_synchronization_restored_predecessor_=false;last_synchronization_conditioned_=delta.size();
  last_synchronization_kind_=MatrixSynchronizationKind::Extension;
  last_dirty_update_=true;last_dirty_rows_=dirty.size();
  used=work.used;work_=used>UINT64_MAX-work_?UINT64_MAX:work_+used;
  event=std::move(candidate);return MatrixStatus::Complete;
}

bool AffineMatrix::authenticates(
    uint64_t owner_identity, const std::vector<Equation> &exact_sources,
    const MatrixEvent &event,
    const std::function<bool(uint64_t)> &charge) const {
  if (!charge || !charge(1) || !owner_identity_ ||
      owner_identity != owner_identity_ ||
      exact_sources.size() != sources_.size())
    return false;
  for (size_t i = 0; i < sources_.size(); ++i) {
    const Equation &source = sources_[i];
    if (!charge(3) || !charge(source.variables.size()) ||
        !charge(source.premises.size()) ||
        !charge(source.premise_clauses.size()))
      return false;
    for (const std::vector<External> &clause : source.premise_clauses)
      if (!charge(clause.size())) return false;
    if (!same_equation(exact_sources[i], sources_[i])) return false;
  }
  if (!charge(1) || !charge(trail_.size()) || !charge(rows_.size()))
    return false;
  for (const MatrixRow &row : rows_)
    if (!charge(row.variables.size() + row.source_equations.size()))
      return false;
  MatrixEvent expected = index_ready_ ? indexed_event() : find_event(rows_);
  if (!charge(event.reason.size()) || !charge(event.row.variables.size()) ||
      !charge(event.row.source_equations.size()))
    return false;
  for (size_t source : expected.row.source_equations)
    if (source >= sources_.size() ||
        !std::binary_search(admitted_sources_.begin(),
                            admitted_sources_.end(), source) ||
        !charge(sources_[source].variables.size()))
      return false;
  make_reason(sources_, trail_, expected);
  return expected.kind != MatrixEventKind::None && same_event(expected, event);
}

} // namespace tactix::affine_semantic_extension
