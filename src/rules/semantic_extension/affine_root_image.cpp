#include "rules/semantic_extension/affine_root_image.hpp"
#include <algorithm>
#include <bit>
#include <set>
#include <map>

namespace tactix::affine_semantic_extension {
namespace {
struct Meter {
  const RootImageLimits &limits;
  const std::function<bool(uint64_t)> &callback;
  uint64_t used = 0;
  RootImageStatus failure = RootImageStatus::ResourceExhausted;
  bool take(uint64_t amount) {
    if (amount > limits.maximum_work - used) return false;
    if (!callback(amount)) { failure = RootImageStatus::Interrupted; return false; }
    used += amount;
    return true;
  }
};
bool valid_literal(External literal) { return literal && literal != INT32_MIN; }
External variable(External literal) { return literal < 0 ? -literal : literal; }
bool valid_source_identity(const AffineLiveSource &source) {
  switch (source.kind) {
  case AffineSourceKind::Large:
  case AffineSourceKind::CheckerPremise:
    return source.clause_id != 0;
  case AffineSourceKind::Binary:
    return source.clause.size() == 2 &&
        (source.clause_id != 0 || !source.proof_id_exact);
  case AffineSourceKind::RootUnit:
    return source.clause.size() == 1 &&
        (source.proof_id_exact ? source.clause_id != 0 : source.clause_id == 0);
  }
  return false;
}

// Independent reverse unit propagation over the complete supplied source view.
// No emitted residual or caller-supplied root fact is inserted into its oracle.
bool rup(const std::vector<AffineLiveSource> &sources,
         const std::vector<External> &variables,
         const std::vector<External> &candidate, std::vector<int8_t> &values,
         Meter &meter, bool &proved) {
  proved = false;
  if (!meter.take(1 + variables.size())) return false;
  std::fill(values.begin(), values.end(), 0);
  const uint64_t lookup = 1 + std::bit_width(variables.size());
  const auto slot = [&](External literal) {
    return size_t(std::lower_bound(variables.begin(), variables.end(), variable(literal)) - variables.begin());
  };
  for (External literal : candidate) {
    if (!meter.take(lookup)) return false;
    const size_t at = slot(literal);
    if (at == variables.size() || variables[at] != variable(literal)) return true;
    const int8_t value = literal > 0 ? -1 : 1;
    if (values[at] && values[at] != value) { proved = true; return true; }
    values[at] = value;
  }
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &source : sources) {
      if (!meter.take(1)) return false;
      bool satisfied = false;
      size_t open = 0;
      External last = 0;
      for (External literal : source.clause) {
        if (!meter.take(lookup)) return false;
        const int8_t value = values[slot(literal)];
        if (value == (literal > 0 ? 1 : -1)) { satisfied = true; break; }
        if (!value) { ++open; last = literal; }
      }
      if (satisfied) continue;
      if (!open) { proved = true; return true; }
      if (open == 1) {
        if (!meter.take(lookup)) return false;
        values[slot(last)] = last > 0 ? 1 : -1;
        changed = true;
      }
    }
  }
  return true;
}
} // namespace
RootImageStatus prepare_affine_root_image(
    const std::vector<AffineLiveSource> &sources,
    const std::vector<External> &roots, const RootImageLimits &limits,
    const std::function<bool(uint64_t)> &charge, RootImagePlan &output,
    const RootScopeCensus *previous, RootScopeCensus *completed, RootScopeEffort *effort) {
  if (!charge || !limits.maximum_sources || !limits.maximum_literals ||
      !limits.maximum_units || !limits.maximum_work || !limits.maximum_bytes ||
      !limits.maximum_proof_additions)
    return RootImageStatus::Invalid;
  if (sources.size() > limits.maximum_sources || roots.size() > limits.maximum_units)
    return RootImageStatus::ResourceExhausted;
  Meter meter{limits, charge};
  uint64_t literals = 0, unit_sources = 0;
  for (const auto &source : sources) {
    if (!meter.take(1)) return meter.failure;
    if (source.clause.size() > limits.maximum_literals - literals)
      return RootImageStatus::ResourceExhausted;
    literals += source.clause.size();
    unit_sources += source.clause.size() == 1;
  }
  // Simultaneous linear populations: retained physical/residual bodies,
  // normalized premise copies and grouping directories, sparse variable
  // directory plus reusable propagation workspace. Tree-node overhead is
  // conservatively four pointers beyond its stored value. Vector-growth
  // allowance is two payload copies; it is independent of configured row caps.
  const __uint128_t source_metadata = __uint128_t(sources.size()) *
      (sizeof(RootResidual) + sizeof(AffineLiveSource) + 2 * sizeof(Premise) +
       2 * sizeof(Equation) + 2 * (sizeof(uint64_t) + 4 * sizeof(void *)) +
       sizeof(std::pair<const uint64_t, uint64_t>) + 4 * sizeof(void *) +
       sizeof(Premise) + sizeof(std::vector<uint64_t>) + 4 * sizeof(void *) +
       sizeof(std::pair<const std::vector<External>, std::vector<Premise>>) +
       4 * sizeof(void *));
  const __uint128_t literal_storage = __uint128_t(literals) *
      (12 * sizeof(External) + sizeof(int8_t) + 4 * sizeof(void *));
  const __uint128_t root_storage = __uint128_t(roots.size()) *
      (3 * sizeof(External) + 4 * sizeof(void *));
  // A single exact scope can authenticate at most two arity-eight equations.
  // Its transient recognizer's pattern/source payload is linear in that
  // group's actual clauses; the linear allowance above covers its witnesses.
  const __uint128_t unit_directory = __uint128_t(unit_sources) *
      (sizeof(External) + 4 * sizeof(void *));
  __uint128_t census_bytes = 0, previous_scope_literals = 0;
  if (previous) {
    for (const auto &record : *previous) {
      if (!meter.take(1 + record.scope.size())) return meter.failure;
      previous_scope_literals += record.scope.size();
      census_bytes += sizeof(RootScopeRecord) + record.scope.size() * sizeof(External);
      for (const auto &body : record.normalized_bodies) {
        if (!meter.take(1 + body.size())) return meter.failure;
        census_bytes += sizeof(std::vector<External>) + body.size() * sizeof(External);
      }
      if (census_bytes > limits.maximum_bytes) return RootImageStatus::ResourceExhausted;
    }
  }
  // Previous plus staged census and canonical temporary bodies coexist.
  // One scope key and an index-vector entry per possible census record.  The
  // fourfold factor covers map nodes, vector growth, allocator metadata and
  // the staged key payload while preserving the existing exact byte ceiling.
  const __uint128_t scope_index_entries = previous || completed ?
      __uint128_t(sources.size()) + (previous ? previous->size() : 0) : 0;
  const __uint128_t scope_index_bytes = 4 * scope_index_entries *
          (sizeof(std::pair<const std::vector<External>, std::vector<size_t>>) +
           4 * sizeof(void *) + sizeof(size_t)) +
      (previous || completed ?
       4 * (__uint128_t(literals) + previous_scope_literals) * sizeof(External) : 0);
  const __uint128_t bytes = source_metadata + literal_storage + root_storage + unit_directory +
      scope_index_bytes +
      2 * census_bytes + (previous || completed ?
        __uint128_t(sources.size()) * (2 * sizeof(RootScopeRecord) +
            2 * sizeof(std::vector<External>)) +
        __uint128_t(literals) * 4 * sizeof(External) : 0);
  if (bytes > limits.maximum_bytes) return RootImageStatus::ResourceExhausted;
  if (!meter.take(literals + sources.size())) return meter.failure;
  std::set<External> variable_set;
  std::set<uint64_t> occurrences;
  std::set<External> existing_units;
  for (const auto &source : sources) {
    if (!meter.take(2 * (1 + std::bit_width(occurrences.size())))) return meter.failure;
    if (!source.occurrence || !valid_source_identity(source) ||
        !occurrences.insert(source.occurrence).second) return RootImageStatus::Invalid;
    if (source.clause.size() == 1) {
      if (!valid_literal(source.clause.front())) return RootImageStatus::Invalid;
      if (!meter.take(2 * (1 + std::bit_width(existing_units.size())))) return meter.failure;
      existing_units.insert(source.clause.front());
    }
    for (External literal : source.clause) {
      if (!meter.take(1)) return meter.failure;
      if (!valid_literal(literal)) return RootImageStatus::Invalid;
      if (!meter.take(2 * (1 + std::bit_width(variable_set.size())))) return meter.failure;
      variable_set.insert(variable(literal));
    }
  }
  if (!meter.take(1 + variable_set.size())) return meter.failure;
  std::vector<External> variables(variable_set.begin(), variable_set.end());
  std::vector<int8_t> workspace(variables.size());
  RootImagePlan candidate;
  candidate.accounted_peak_bytes = uint64_t(bytes);
  candidate.reserved_bytes = uint64_t(bytes);
  std::set<External> facts;
  for (External literal : roots) {
    if (!valid_literal(literal)) return RootImageStatus::Invalid;
    if (!meter.take(4 * (1 + std::bit_width(facts.size())))) return meter.failure;
    if (facts.contains(-literal) || !facts.insert(literal).second)
      return RootImageStatus::Invalid;
    if (candidate.rup_checks == limits.maximum_proof_additions)
      return RootImageStatus::ResourceExhausted;
    if (!meter.take(2 * (1 + std::bit_width(existing_units.size())))) return meter.failure;
    bool proved = existing_units.contains(literal);
    if (!proved && !rup(sources, variables, {literal}, workspace, meter, proved))
      return meter.failure;
    ++candidate.rup_checks;
    if (!proved) return RootImageStatus::UncertifiedRoot;
    candidate.certified_root_facts.push_back(literal);
  }
  std::map<std::vector<External>, std::vector<Premise>> groups;
  uint64_t normalized_count = 0;
  for (const auto &source : sources) {
    if(!meter.take(1))return meter.failure;
    // Oracle support is authenticated above and participates in RUP only.
    // It never nominates an equation or a completed discovery population.
    if(!source.recognition_candidate)continue;
    RootResidual residual;
    if (!meter.take(2 + 2 * source.clause.size())) return meter.failure;
    residual.physical_source = source;
    bool satisfied = false;
    for (External literal : source.clause) {
      if (!meter.take(4 * (1 + std::bit_width(facts.size())))) return meter.failure;
      if (facts.contains(literal)) { satisfied = true; break; }
      if (facts.contains(-literal)) residual.removed_root_facts.push_back(-literal);
      else residual.clause.push_back(literal);
    }
    if (satisfied || residual.clause.empty() || residual.clause.size() > limits.image.maximum_arity)
      continue;
    residual.occurrence = candidate.residuals.size() + 1;
    if (!meter.take(2 + residual.clause.size())) return meter.failure;
    if (++normalized_count > limits.image.maximum_clauses ||
        normalized_count > limits.image.maximum_patterns)
      return RootImageStatus::ResourceExhausted;
    std::vector<External> scope;
    for (External literal : residual.clause) scope.push_back(variable(literal));
    if (!meter.take(scope.size() * scope.size() + scope.size())) return meter.failure;
    std::sort(scope.begin(), scope.end());
    if (std::adjacent_find(scope.begin(), scope.end()) != scope.end()) continue;
    if (!meter.take(2 * (1 + std::bit_width(groups.size())) * (1 + scope.size())))
      return meter.failure;
    groups[scope].push_back({residual.occurrence, residual.clause});
    if (groups.size() > limits.image.maximum_groups)
      return RootImageStatus::ResourceExhausted;
    candidate.residuals.push_back(std::move(residual));
  }
  // Global elimination is intentionally deferred. Reusing the existing
  // recognizer on one exact scope at a time avoids quadratic cross-scope
  // consequence provenance while retaining every authenticated equation.
  RootScopeEffort measured;
  RootScopeCensus next_census;
  if (previous) {
    if (!meter.take(uint64_t(census_bytes))) return meter.failure;
    next_census = *previous;
  }
  // Keep the durable census in its stable vector representation, but index
  // records by scope for this planning transaction.  Comparing every new
  // scope with the whole retained census made first discovery quadratic even
  // when every scope was unique (the common mixed-CNF case).
  std::map<std::vector<External>, std::vector<size_t>> census_by_scope;
  const auto scope_index_work = [](size_t width, size_t entries) {
    const __uint128_t amount = __uint128_t(1 + width) *
        (1 + std::bit_width(entries));
    return amount > UINT64_MAX ? UINT64_MAX : uint64_t(amount);
  };
  for (size_t i = 0; i < next_census.size(); ++i) {
    const auto &scope = next_census[i].scope;
    if (!meter.take(scope_index_work(scope.size(), census_by_scope.size())))
      return meter.failure;
    census_by_scope[scope].push_back(i);
  }
  for (const auto &[scope, premises] : groups) {
    RootScopeRecord record;
    if (previous || completed) {
      if (!meter.take(1 + scope.size())) return meter.failure;
      record.scope = scope;
      uint64_t payload = 0;
      for (const auto &premise : premises) payload += 1 + premise.literals.size();
      const __uint128_t canonical_work =
          __uint128_t(payload) * (2 + premises.size());
      if (canonical_work > UINT64_MAX) return RootImageStatus::ResourceExhausted;
      if (!meter.take(uint64_t(canonical_work))) return meter.failure;
      for (const auto &premise : premises) {
        auto body = premise.literals;
        std::sort(body.begin(), body.end());
        record.normalized_bodies.push_back(std::move(body));
      }
      std::sort(record.normalized_bodies.begin(), record.normalized_bodies.end());
      // Exact completed populations win over an earlier unresolved record of
      // the same scope; a newly admitted subset must never be re-recognized.
      if (!meter.take(scope_index_work(scope.size(), census_by_scope.size())))
        return meter.failure;
      auto indexed = census_by_scope.find(scope);
      size_t found_index = next_census.size(), unresolved_index = next_census.size();
      if (indexed != census_by_scope.end()) {
        for (size_t index : indexed->second) {
          const auto &old = next_census[index];
          __uint128_t compare_work = 1 + old.normalized_bodies.size() +
                                     record.normalized_bodies.size();
          for (const auto &body : old.normalized_bodies)
            compare_work += 1 + body.size();
          for (const auto &body : record.normalized_bodies)
            compare_work += 1 + body.size();
          if (compare_work > UINT64_MAX || !meter.take(uint64_t(compare_work)))
            return meter.failure;
          if (old.normalized_bodies == record.normalized_bodies) {
            found_index = index;
            break;
          }
          if (unresolved_index == next_census.size() && !old.admitted)
            unresolved_index = index;
        }
      }
      if (found_index == next_census.size()) found_index = unresolved_index;
      if (found_index != next_census.size()) {
        auto &found = next_census[found_index];
        if (!meter.take(payload + 1)) return meter.failure;
        if (found.normalized_bodies == record.normalized_bodies) {
          ++measured.skipped_groups;
          continue;
        }
        if (found.admitted) {
          const size_t index = next_census.size();
          next_census.push_back(std::move(record));
          census_by_scope[scope].push_back(index);
        } else {
          found = std::move(record);
        }
      } else {
        const size_t index = next_census.size();
        next_census.push_back(std::move(record));
        census_by_scope[scope].push_back(index);
      }
    }
    Image local;
    ++measured.recognized_groups;
    const uint64_t recognition_start = meter.used;
    const Status status = recognize(premises, limits.image,
        [&](uint64_t amount) { return meter.take(amount); }, local);
    measured.recognition_work += meter.used - recognition_start;
    if (status == Status::NoStructure) continue;
    if (status == Status::Invalid) return RootImageStatus::Invalid;
    if (status != Status::Complete) return meter.failure;
    if (local.patterns_seen > limits.image.maximum_patterns - candidate.image.patterns_seen)
      return RootImageStatus::ResourceExhausted;
    candidate.image.patterns_seen += local.patterns_seen;
    candidate.image.groups_seen += local.groups_seen;
    candidate.image.clauses_seen += local.clauses_seen;
    if (local.elimination_work > limits.image.maximum_elimination_work - candidate.image.elimination_work)
      return RootImageStatus::ResourceExhausted;
    candidate.image.elimination_work += local.elimination_work;
    for (auto &equation : local.equations) {
      if (!meter.take(1 + equation.variables.size() + equation.premises.size()))
        return meter.failure;
      candidate.image.equations.push_back(std::move(equation));
    }
  }
  if (candidate.image.equations.empty()) {
    if (completed) *completed = std::move(next_census);
    if (effort) *effort = measured;
    return RootImageStatus::NoStructure;
  }
  std::set<uint64_t> selected;
  for (const auto &equation : candidate.image.equations)
    for (uint64_t occurrence : equation.premises) {
      if (!meter.take(2 * (1 + std::bit_width(selected.size())))) return meter.failure;
      selected.insert(occurrence);
    }
  size_t kept = 0;
  for (size_t i = 0; i < candidate.residuals.size(); ++i) {
    auto &residual = candidate.residuals[i];
    if (!meter.take(2 * (1 + std::bit_width(selected.size())))) return meter.failure;
    if (!selected.contains(residual.occurrence)) continue;
    if (!residual.removed_root_facts.empty()) {
      if (candidate.rup_checks == limits.maximum_proof_additions)
        return RootImageStatus::ResourceExhausted;
      // A physical source plus existing exact unit premises is itself a
      // complete local RUP witness: negating the residual falsifies its other
      // literals, and the units falsify every removed literal. This avoids
      // repeated whole-view propagation for already checker-visible facts.
      bool proved = true;
      for (External fact : residual.removed_root_facts) {
        if (!meter.take(2 * (1 + std::bit_width(existing_units.size())))) return meter.failure;
        proved &= existing_units.contains(fact);
      }
      if (!proved && !rup(sources, variables, residual.clause, workspace, meter, proved))
        return meter.failure;
      ++candidate.rup_checks;
      if (!proved) return RootImageStatus::UncertifiedRoot;
    }
    if (kept != i) candidate.residuals[kept] = std::move(residual);
    ++kept;
  }
  candidate.residuals.resize(kept);
  std::map<uint64_t, uint64_t> remapped;
  for (size_t i = 0; i < candidate.residuals.size(); ++i) {
    if (!meter.take(2 * (1 + std::bit_width(remapped.size())))) return meter.failure;
    auto &residual = candidate.residuals[i];
    remapped.emplace(residual.occurrence, i + 1);
    residual.occurrence = i + 1;
  }
  for (auto &equation : candidate.image.equations)
    for (auto &occurrence : equation.premises) {
      if (!meter.take(2 * (1 + std::bit_width(remapped.size())))) return meter.failure;
      const auto found = remapped.find(occurrence);
      if (found == remapped.end()) return RootImageStatus::Invalid;
      occurrence = found->second;
    }


  if (!meter.take(literals + sources.size())) return meter.failure;
  candidate.certification_sources = sources;
  candidate.work = meter.used;
  output = std::move(candidate);
  if (completed) *completed = std::move(next_census);
  if (effort) *effort = measured;
  return RootImageStatus::Prepared;
}
} // namespace tactix::affine_semantic_extension
