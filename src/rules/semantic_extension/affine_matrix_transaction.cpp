#include "rules/semantic_extension/affine_matrix_transaction.hpp"

#include "kernel/clause.hpp"
#include "rules/lucky/parity_certificate.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <map>
#include <set>

namespace tactix::affine_semantic_extension {
namespace {

using SmallClause = parity_certificate::SmallClause;

std::vector<External> key(std::vector<External> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

bool same_equation(const Equation &first, const Equation &second) {
  return first.variables == second.variables && first.rhs == second.rhs &&
         first.premises == second.premises &&
         first.premise_clauses == second.premise_clauses;
}

bool same_equations(const std::vector<Equation> &first,
                    const std::vector<Equation> &second) {
  if (first.size() != second.size()) return false;
  for (size_t i = 0; i < first.size(); ++i)
    if (!same_equation(first[i], second[i])) return false;
  return true;
}

struct Work {
  uint64_t used = 0;
  uint64_t limit = 0;
  const std::function<bool(uint64_t)> &charge;
  bool interrupted = false;
  bool exhausted = false;

  bool consume(uint64_t amount) {
    if (used > limit || amount > limit - used) {
      exhausted = true;
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

struct PlanningSink {
  std::vector<MatrixProofEvent> &events;
  uint64_t &next_id;
  uint64_t maximum_additions;
  uint64_t maximum_deletions;
  uint64_t additions = 0;
  uint64_t deletions = 0;
  std::map<std::vector<External>, std::vector<uint64_t>> active;
  bool exhausted = false;
  bool invalid = false;

  PlanningSink(std::vector<MatrixProofEvent> &events_, uint64_t &next_id_,
               uint64_t maximum_additions_, uint64_t maximum_deletions_)
      : events(events_), next_id(next_id_),
        maximum_additions(maximum_additions_),
        maximum_deletions(maximum_deletions_) {}

  bool add_external(const std::vector<External> &clause, bool rat) {
    if (additions == maximum_additions || next_id == UINT64_MAX) {
      exhausted = true;
      return false;
    }
    MatrixProofEvent event;
    event.kind = rat ? MatrixProofEvent::Kind::Rat
                     : MatrixProofEvent::Kind::Rup;
    event.clause = clause;
    event.id = ++next_id;
    events.push_back(event);
    active[key(clause)].push_back(event.id);
    ++additions;
    return true;
  }

  bool erase_external(const std::vector<External> &clause) {
    if (deletions == maximum_deletions) {
      exhausted = true;
      return false;
    }
    auto found = active.find(key(clause));
    if (found == active.end() || found->second.empty()) {
      invalid = true;
      return false;
    }
    MatrixProofEvent event;
    event.kind = MatrixProofEvent::Kind::Delete;
    event.clause = clause;
    event.id = next_id;
    event.deleted_id = found->second.back();
    found->second.pop_back();
    events.push_back(std::move(event));
    ++deletions;
    return true;
  }

  bool mark_install(const std::vector<External> &clause) {
    const auto found = active.find(key(clause));
    if (found == active.end() || found->second.size() != 1) return false;
    const uint64_t id = found->second.back();
    for (auto event = events.rbegin(); event != events.rend(); ++event)
      if (event->kind != MatrixProofEvent::Kind::Delete && event->id == id) {
        if (event->install) return false;
        event->install = true;
        return true;
      }
    return false;
  }

  bool only_active(const std::vector<External> &clause) const {
    const auto wanted = key(clause);
    size_t populated = 0;
    for (const auto &[candidate, ids] : active) {
      if (ids.empty()) continue;
      ++populated;
      if (candidate != wanted || ids.size() != 1) return false;
    }
    return populated == 1;
  }

  size_t active_size() const {
    size_t result = 0;
    for (const auto &[clause, ids] : active) {
      (void)clause;
      result += ids.size();
    }
    return result;
  }

  std::vector<uint64_t> active_support_ids() const {
    std::vector<uint64_t> result;
    for (const MatrixProofEvent &event : events) {
      if (event.kind == MatrixProofEvent::Kind::Delete) continue;
      const auto found = active.find(key(event.clause));
      if (found == active.end() ||
          std::find(found->second.begin(), found->second.end(), event.id) ==
              found->second.end())
        continue;
      result.push_back(event.id);
    }
    return result;
  }
};

struct DenseSink {
  PlanningSink &sink;
  const std::vector<External> &external_variables;
  uint32_t starting_extent;
  uint32_t fresh;
  uint32_t maximum_helpers;

  External external_literal(External literal) const {
    if (!literal || literal == INT32_MIN) return 0;
    const uint32_t variable = static_cast<uint32_t>(std::abs(literal));
    int64_t external = 0;
    if (variable <= external_variables.size())
      external = external_variables[variable - 1];
    else {
      const uint64_t offset = variable - external_variables.size();
      if (offset > maximum_helpers) return 0;
      external = uint64_t{starting_extent} + offset;
    }
    if (external <= 0 || external > INT32_MAX) return 0;
    return literal < 0 ? -static_cast<External>(external)
                       : static_cast<External>(external);
  }

  bool map(const SmallClause &input, std::vector<External> &output) const {
    output.clear();
    output.reserve(input.size);
    for (uint32_t i = 0; i < input.size; ++i) {
      const External literal = external_literal(input.lits[i]);
      if (!literal) return false;
      output.push_back(literal);
    }
    return true;
  }

  bool map(const std::vector<External> &input,
           std::vector<External> &output) const {
    output.clear();
    output.reserve(input.size());
    for (External item : input) {
      const External literal = external_literal(item);
      if (!literal) return false;
      output.push_back(literal);
    }
    return true;
  }

  bool add(const SmallClause &clause, bool rat) {
    std::vector<External> external;
    if (!map(clause, external)) {
      sink.invalid = true;
      return false;
    }
    return sink.add_external(external, rat);
  }

  bool add(const std::vector<External> &clause, bool rat) {
    std::vector<External> external;
    if (!map(clause, external)) {
      sink.invalid = true;
      return false;
    }
    return sink.add_external(external, rat);
  }

  bool erase(const SmallClause &clause) {
    std::vector<External> external;
    if (!map(clause, external)) {
      sink.invalid = true;
      return false;
    }
    return sink.erase_external(external);
  }

  bool new_variable(External &variable) {
    const uint64_t used = uint64_t{fresh} - external_variables.size();
    if (used == maximum_helpers || fresh == INT32_MAX) {
      sink.exhausted = true;
      return false;
    }
    variable = static_cast<External>(++fresh);
    return true;
  }

  uint32_t helpers() const {
    return fresh - static_cast<uint32_t>(external_variables.size());
  }
};

MatrixTransactionStatus failure(const Work &work, const PlanningSink &sink) {
  if (work.interrupted) return MatrixTransactionStatus::Interrupted;
  if (work.exhausted) return MatrixTransactionStatus::ResourceExhausted;
  if (sink.exhausted) return MatrixTransactionStatus::ResourceExhausted;
  if (sink.invalid) return MatrixTransactionStatus::Invalid;
  return MatrixTransactionStatus::UnsupportedProof;
}

std::vector<std::vector<External>> canonical_clauses(
    std::vector<std::vector<External>> clauses) {
  for (std::vector<External> &clause : clauses)
    std::sort(clause.begin(), clause.end());
  std::sort(clauses.begin(), clauses.end());
  return clauses;
}

bool authenticate_premises(const Equation &equation,
                           std::set<uint64_t> &occurrences, Work &work) {
  const uint64_t expected_clauses =
      uint64_t{1} << (equation.variables.size() - 1);
  if (!work.consume(3) || !work.consume(equation.premises.size()) ||
      !work.consume(equation.premise_clauses.size()) ||
      !work.consume(expected_clauses) ||
      !work.consume(expected_clauses * equation.variables.size()))
    return false;
  for (const std::vector<External> &clause : equation.premise_clauses)
    if (!work.consume(1) || !work.consume(clause.size())) return false;
  const auto expected = clausal_form(equation.variables, equation.rhs);
  if (equation.premises.size() != expected.size() ||
      equation.premise_clauses.size() != expected.size() ||
      canonical_clauses(equation.premise_clauses) !=
          canonical_clauses(expected))
    return false;
  for (uint64_t occurrence : equation.premises)
    if (!work.consume(1) || !occurrence ||
        !occurrences.insert(occurrence).second)
      return false;
  return true;
}

} // namespace

const MatrixProofCacheEntry *AffineMatrixProofCache::find(
    uint64_t owner_identity, uint64_t proof_epoch,
    const std::vector<size_t> &source_equations,
    const std::vector<Equation> &source_identity) const {
  for (const MatrixProofCacheEntry &entry : entries_)
    if (entry.owner_identity == owner_identity &&
        entry.proof_epoch == proof_epoch &&
        entry.source_equations == source_equations &&
        same_equations(entry.source_identity, source_identity))
      return &entry;
  return nullptr;
}

bool AffineMatrixProofCache::commit(MatrixProofCacheEntry &&entry,
                                    uint64_t expected_generation) {
  if (generation_ != expected_generation || generation_ == UINT64_MAX ||
      !entry.owner_identity || entry.support_ids.empty() ||
      find(entry.owner_identity, entry.proof_epoch, entry.source_equations,
           entry.source_identity))
    return false;
  if (!entries_.empty() &&
      entries_.front().proof_epoch != entry.proof_epoch)
    entries_.clear();
  entries_.push_back(std::move(entry));
  ++generation_;
  return true;
}

MatrixTransactionStatus prepare_affine_matrix_transaction(
    const AffineMatrix &matrix, uint64_t expected_owner_identity,
    const std::vector<Equation> &exact_sources, const MatrixEvent &event,
    uint32_t starting_external_extent, uint64_t starting_clause_id,
    const MatrixTransactionLimits &limits,
    const std::function<bool(uint64_t)> &charge,
    AffineMatrixTransaction &output,
    const AffineMatrixProofCache *proof_cache, uint64_t proof_epoch) {
  if (!expected_owner_identity || !charge || !limits.maximum_work ||
      !limits.maximum_proof_additions || !limits.maximum_proof_deletions ||
      !limits.maximum_helper_extent ||
      starting_external_extent > static_cast<uint32_t>(INT32_MAX))
    return MatrixTransactionStatus::Invalid;
  if (matrix.owner_identity() != expected_owner_identity)
    return MatrixTransactionStatus::Stale;

  Work work{0, limits.maximum_work, charge};
  if (!matrix.authenticates(expected_owner_identity, exact_sources, event,
                            [&](uint64_t amount) {
                              return work.consume(amount);
                            }))
    return work.interrupted ? MatrixTransactionStatus::Interrupted
         : work.exhausted ? MatrixTransactionStatus::ResourceExhausted
                          : MatrixTransactionStatus::Stale;
  if (!work.consume(1) || !work.consume(exact_sources.size()) ||
      !work.consume(event.reason.size()) ||
      !work.consume(event.row.source_equations.size()))
    return work.interrupted ? MatrixTransactionStatus::Interrupted
                            : MatrixTransactionStatus::ResourceExhausted;

  std::vector<External> variables;
  std::vector<parity_certificate::Equation> equations;
  std::set<uint64_t> occurrences;
  for (size_t source : event.row.source_equations) {
    if (source >= exact_sources.size()) return MatrixTransactionStatus::Stale;
    const Equation &input = exact_sources[source];
    if (input.variables.empty() ||
        input.variables.size() > parity_certificate::Equation{}.variables.size())
      return MatrixTransactionStatus::UnsupportedProof;
    if (!authenticate_premises(input, occurrences, work))
      return work.interrupted ? MatrixTransactionStatus::Interrupted
           : work.exhausted ? MatrixTransactionStatus::ResourceExhausted
                            : MatrixTransactionStatus::UnsupportedProof;
    variables.insert(variables.end(), input.variables.begin(),
                     input.variables.end());
  }
  std::sort(variables.begin(), variables.end());
  variables.erase(std::unique(variables.begin(), variables.end()),
                  variables.end());
  if (variables.empty() || variables.size() > INT32_MAX ||
      variables.back() > static_cast<External>(starting_external_extent))
    return MatrixTransactionStatus::Invalid;

  equations.reserve(event.row.source_equations.size());
  for (size_t source : event.row.source_equations) {
    const Equation &input = exact_sources[source];
    parity_certificate::Equation dense;
    dense.size = static_cast<uint8_t>(input.variables.size());
    dense.rhs = input.rhs;
    for (size_t i = 0; i < input.variables.size(); ++i) {
      const auto found =
          std::lower_bound(variables.begin(), variables.end(),
                           input.variables[i]);
      if (found == variables.end() || *found != input.variables[i])
        return MatrixTransactionStatus::Invalid;
      dense.variables[i] = static_cast<External>(found - variables.begin() + 1);
    }
    equations.push_back(dense);
  }

  std::vector<External> dense_reason;
  dense_reason.reserve(event.reason.size());
  for (External literal : event.reason) {
    if (!literal || literal == INT32_MIN)
      return MatrixTransactionStatus::Invalid;
    const External variable = literal < 0 ? -literal : literal;
    const auto found = std::lower_bound(variables.begin(), variables.end(),
                                        variable);
    if (found == variables.end() || *found != variable)
      return MatrixTransactionStatus::Stale;
    External dense = static_cast<External>(found - variables.begin() + 1);
    dense_reason.push_back(literal < 0 ? -dense : dense);
  }

  AffineMatrixTransaction candidate;
  candidate.owner_identity_ = expected_owner_identity;
  candidate.source_equations_ = event.row.source_equations;
  candidate.source_identity_.reserve(event.row.source_equations.size());
  for (size_t source : event.row.source_equations) {
    const Equation &identity = exact_sources[source];
    if (!work.consume(3) || !work.consume(identity.variables.size()) ||
        !work.consume(identity.premises.size()) ||
        !work.consume(identity.premise_clauses.size()))
      return work.interrupted ? MatrixTransactionStatus::Interrupted
                              : MatrixTransactionStatus::ResourceExhausted;
    for (const std::vector<External> &clause : identity.premise_clauses)
      if (!work.consume(1) || !work.consume(clause.size()))
        return work.interrupted ? MatrixTransactionStatus::Interrupted
                                : MatrixTransactionStatus::ResourceExhausted;
    candidate.source_identity_.push_back(exact_sources[source]);
  }
  const uint64_t arena_words = event.reason.size() > 2
                                   ? event.reason.size() + CLAUSE_HEADER_WORDS
                                   : 0;
  if (arena_words > limits.maximum_arena_words)
    return MatrixTransactionStatus::ResourceExhausted;
  candidate.arena_words_ = arena_words;
  candidate.reason_ = event.reason;
  if (proof_cache) {
    candidate.cache_generation_ = proof_cache->generation();
    const MatrixProofCacheEntry *cached = proof_cache->find(
        expected_owner_identity, proof_epoch, candidate.source_equations_,
        candidate.source_identity_);
    if (cached) {
      if (cached->external_extent > starting_external_extent ||
          cached->support_ids.empty())
        return MatrixTransactionStatus::Stale;
      uint64_t next_id = starting_clause_id;
      PlanningSink planning{candidate.events_, next_id,
                            limits.maximum_proof_additions,
                            limits.maximum_proof_deletions};
      if (!planning.add_external(event.reason, false))
        return failure(work, planning);
      if (!planning.mark_install(event.reason) ||
          !planning.only_active(event.reason))
        return MatrixTransactionStatus::Invalid;
      candidate.ready_ = true;
      candidate.additions_ = planning.additions;
      candidate.deletions_ = planning.deletions;
      candidate.helper_extent_ = 0;
      candidate.work_ = work.used;
      candidate.cache_hit_ = true;
      candidate.cache_entry_ = *cached;
      output = std::move(candidate);
      return MatrixTransactionStatus::Prepared;
    }
    if (proof_cache->generation() == UINT64_MAX)
      return MatrixTransactionStatus::ResourceExhausted;
  }
  uint64_t next_id = starting_clause_id;
  PlanningSink planning{candidate.events_, next_id,
                        limits.maximum_proof_additions,
                        limits.maximum_proof_deletions};
  DenseSink sink{planning, variables, starting_external_extent,
                 static_cast<uint32_t>(variables.size()),
                 limits.maximum_helper_extent};
  parity_certificate::Certificate<DenseSink> certificate(
      sink, static_cast<uint32_t>(variables.size()), UINT32_MAX,
      [&](uint64_t amount) { return work.consume(amount); });
  if (!certificate.accumulate(equations))
    return failure(work, planning);
  std::vector<uint64_t> support_ids;
  if (proof_cache) {
    support_ids = planning.active_support_ids();
    if (support_ids.empty() || support_ids.size() != planning.active_size())
      return MatrixTransactionStatus::Invalid;
  }
  if (!certificate.export_reason(dense_reason)) return failure(work, planning);
  if (!planning.mark_install(event.reason))
    return MatrixTransactionStatus::Invalid;
  if (proof_cache) {
    if (planning.active_size() != support_ids.size() + 1)
      return MatrixTransactionStatus::Invalid;
    const uint64_t cache_extent =
        uint64_t{starting_external_extent} + sink.helpers();
    if (cache_extent > static_cast<uint32_t>(INT32_MAX))
      return MatrixTransactionStatus::ResourceExhausted;
    candidate.cache_insert_ = true;
    candidate.cache_entry_.owner_identity = expected_owner_identity;
    candidate.cache_entry_.proof_epoch = proof_epoch;
    candidate.cache_entry_.external_extent =
        static_cast<uint32_t>(cache_extent);
    candidate.cache_entry_.source_equations = candidate.source_equations_;
    candidate.cache_entry_.source_identity = candidate.source_identity_;
    candidate.cache_entry_.support_ids = std::move(support_ids);
  } else {
    if (!certificate.retire_helpers()) return failure(work, planning);
    if (!planning.only_active(event.reason))
      return MatrixTransactionStatus::Invalid;
  }
  candidate.ready_ = true;
  candidate.additions_ = planning.additions;
  candidate.deletions_ = planning.deletions;
  candidate.helper_extent_ = sink.helpers();
  candidate.work_ = work.used;
  output = std::move(candidate);
  return MatrixTransactionStatus::Prepared;
}

} // namespace tactix::affine_semantic_extension
