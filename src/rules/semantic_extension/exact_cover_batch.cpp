#include "rules/semantic_extension/exact_cover_batch.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <string>

namespace {

using tactix::Clause;
using tactix::INVALID_LIT;
using tactix::Kernel;
using tactix::Lit;
using tactix::Ref;
using tactix::exact_cover_semantic::Premise;

std::vector<int32_t> normalized_external(const Kernel &k, const Lit *begin,
                                         uint32_t size) {
  std::vector<int32_t> result;
  result.reserve(size);
  for (uint32_t i = 0; i < size; ++i)
    result.push_back(tactix::kernel_externalize(k, begin[i]));
  std::sort(result.begin(), result.end());
  return result;
}

bool retained_source_is_live(const Kernel &k,
                             const std::vector<Premise> &source) {
  std::map<std::vector<int32_t>, uint64_t> expected;
  for (const Premise &premise : source) {
    std::vector<int32_t> clause = premise.literals;
    std::sort(clause.begin(), clause.end());
    if (!expected.emplace(std::move(clause), premise.native_proof_id).second)
      return false;
  }

  std::map<std::vector<int32_t>, uint64_t> live;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = tactix::clause_at(k.arena, ref);
    ref += tactix::clause_actual_words(clause);
    if (clause->garbage()) continue;
    if (!live.emplace(normalized_external(k, clause->lits, clause->size),
                      clause->id()).second)
      return false;
  }
  const uint64_t literal_extent = uint64_t{2} * k.nvars;
  for (uint64_t raw = 0; raw < literal_extent; ++raw) {
    const Lit first = static_cast<Lit>(raw);
    for (const auto watch : k.watches.entries(first)) {
      if (!watch.binary() || first >= watch.other()) continue;
      const Lit pair[] = {first, watch.other()};
      const uint64_t proof_id = k.binary_ids.tracks_values()
                                    ? tactix::kernel_binary_id(k, first,
                                                               watch.other())
                                    : 0;
      if (!live.emplace(normalized_external(k, pair, 2), proof_id).second)
        return false;
    }
  }
  if (live.size() != expected.size()) return false;
  for (const auto &[clause, proof_id] : expected) {
    const auto found = live.find(clause);
    if (found == live.end() || (proof_id && found->second != proof_id))
      return false;
  }
  return true;
}

bool checked_add(uint64_t &total, uint64_t amount) {
  if (amount > UINT64_MAX - total) return false;
  total += amount;
  return true;
}

struct Journal {
  uint64_t limit = 0;
  std::string bytes;

  bool append(const std::string &text) {
    if (bytes.size() > limit || text.size() > limit - bytes.size())
      return false;
    bytes += text;
    return true;
  }

  bool integers(const std::vector<int32_t> &values) {
    for (size_t i = 0; i < values.size(); ++i)
      if (!append((i ? "," : "") + std::to_string(values[i])))
        return false;
    return true;
  }

  bool unsigned_integers(const std::vector<uint64_t> &values) {
    for (size_t i = 0; i < values.size(); ++i)
      if (!append((i ? "," : "") + std::to_string(values[i])))
        return false;
    return true;
  }
};

} // namespace

namespace tactix::exact_cover_semantic {

BatchPublicationResult publish_batch(Kernel &k, const BatchPlan &plan,
                                     const BatchPublicationRequest &request) {
  BatchPublicationResult result;
  const uint64_t extent = k.external_to_internal.size();
  if (!request.action_id || !request.maximum_journal_bytes ||
      !request.records || plan.arity_ < 3 || plan.source_.empty() ||
      plan.definitions_.empty() ||
      plan.first_output_external_ != extent + 1 ||
      plan.definitions_.size() != plan.extent_growth_) {
    result.status = BatchPublicationStatus::Invalid;
    return result;
  }
  if (k.proof && k.proof->pb()) {
    result.status = BatchPublicationStatus::UnsupportedProof;
    return result;
  }
  if (kernel_terminating(k)) {
    result.status = BatchPublicationStatus::Interrupted;
    return result;
  }
  if (k.inconsistent || k.level || k.conflict_valid || k.pending.valid ||
      !k.trail.empty() || k.dense || k.probing.recording ||
      !k.large_watches_connected || !kernel_propagation_fixpoint(k)) {
    result.status = BatchPublicationStatus::Inapplicable;
    return result;
  }
  if (!retained_source_is_live(k, plan.source_)) {
    result.status = BatchPublicationStatus::Stale;
    return result;
  }
  if (extent > INT32_MAX ||
      plan.definitions_.size() > UINT32_MAX - k.nvars ||
      plan.definitions_.size() > uint64_t(INT32_MAX) - extent) {
    result.status = BatchPublicationStatus::ResourceExhausted;
    return result;
  }

  uint64_t additions = 0, literal_occurrences = 0, arena_words = 0;
  std::set<std::vector<int32_t>> duplicate_keys;
  std::set<uint64_t> source_occurrences;
  std::set<uint64_t> source_proof_ids;
  std::set<std::pair<uint64_t, uint32_t>> semantic_ids;
  std::set<uint32_t> planned_blocks;
  for (const Premise &premise : plan.source_)
    if (!premise.occurrence ||
        !source_occurrences.insert(premise.occurrence).second ||
        (premise.native_proof_id &&
         !source_proof_ids.insert(premise.native_proof_id).second)) {
      result.status = BatchPublicationStatus::Invalid;
      return result;
    }
  for (uint32_t slot = 0; slot < plan.definitions_.size(); ++slot) {
    const PlannedDefinition &definition = plan.definitions_[slot];
    if (definition.semantic.action != request.action_id ||
        definition.semantic.slot != slot ||
        !semantic_ids.insert({definition.semantic.action,
                              definition.semantic.slot}).second ||
        !planned_blocks.insert(definition.block).second ||
        definition.output != int64_t(plan.first_output_external_) + slot ||
        definition.inputs.size() != plan.arity_ ||
        definition.formula.size() != plan.arity_ + 1 ||
        definition.proof_additions != definition.formula.size() ||
        definition.extent_growth != 1 ||
        definition.duplicate_key.disjunction != definition.inputs ||
        definition.projection != ModelProjection::OmitFreshOutput ||
        definition.kind != TheoryVariableKind::BlockOccupancy ||
        definition.lifetime != VariableLifetime::OrdinaryLiveClauses ||
        definition.source_premises.empty() ||
        !std::is_sorted(definition.source_premises.begin(),
                        definition.source_premises.end()) ||
        std::adjacent_find(definition.source_premises.begin(),
                           definition.source_premises.end()) !=
            definition.source_premises.end() ||
        !duplicate_keys.insert(definition.duplicate_key.disjunction).second) {
      result.status = BatchPublicationStatus::Invalid;
      return result;
    }
    for (uint64_t premise : definition.source_premises)
      if (!source_occurrences.count(premise)) {
        result.status = BatchPublicationStatus::Invalid;
        return result;
      }
    std::set<uint32_t> variables;
    uint64_t expected_literals = 0;
    for (uint32_t i = 0; i < plan.arity_; ++i) {
      const int32_t input = definition.inputs[i];
      if (!input || input == INT32_MIN ||
          !variables.insert(variable(input)).second ||
          definition.formula[i] !=
              std::vector<int32_t>({definition.output, -input})) {
        result.status = BatchPublicationStatus::Invalid;
        return result;
      }
      const Lit internal = kernel_internalize(k, input);
      if (internal == INVALID_LIT || kernel_var_retired(k, var_of(internal))) {
        result.status = BatchPublicationStatus::Stale;
        return result;
      }
      expected_literals += 2;
    }
    std::vector<int32_t> reverse{-definition.output};
    reverse.insert(reverse.end(), definition.inputs.begin(),
                   definition.inputs.end());
    if (definition.formula.back() != reverse) {
      result.status = BatchPublicationStatus::Invalid;
      return result;
    }
    expected_literals += reverse.size();
    if (definition.literal_occurrences != expected_literals) {
      result.status = BatchPublicationStatus::Invalid;
      return result;
    }
    if (!checked_add(additions, definition.proof_additions) ||
        !checked_add(literal_occurrences, definition.literal_occurrences) ||
        !checked_add(arena_words, reverse.size() + CLAUSE_HEADER_WORDS)) {
      result.status = BatchPublicationStatus::ResourceExhausted;
      return result;
    }
  }
  if (additions != plan.proof_additions_ ||
      literal_occurrences != plan.literal_occurrences_) {
    result.status = BatchPublicationStatus::Invalid;
    return result;
  }
  if (k.arena.size() >= INVALID_REF ||
      arena_words >= INVALID_REF - k.arena.size() ||
      k.next_clause_id > uint64_t(INT64_MAX) - additions) {
    result.status = BatchPublicationStatus::ResourceExhausted;
    return result;
  }

  Journal journal{request.maximum_journal_bytes, {}};
  if (!journal.append(
          "{\"kind\":\"exact-cover-batch\",\"action\":" +
          std::to_string(request.action_id) + ",\"arity\":" +
          std::to_string(plan.arity_) + ",\"definitions\":" +
          std::to_string(plan.definitions_.size()) + ",\"additions\":" +
          std::to_string(plan.proof_additions_) +
          ",\"literal_occurrences\":" +
          std::to_string(plan.literal_occurrences_) +
          ",\"extent_growth\":" + std::to_string(plan.extent_growth_) +
          ",\"recognition\":\"single-retained-source-image\"}\n")) {
    result.status = BatchPublicationStatus::ResourceExhausted;
    return result;
  }
  for (const Premise &premise : plan.source_) {
    if (!journal.append(
            "{\"kind\":\"exact-cover-batch-source\",\"action\":" +
            std::to_string(request.action_id) + ",\"occurrence\":" +
            std::to_string(premise.occurrence) +
            ",\"native_proof_id\":" +
            std::to_string(premise.native_proof_id) + ",\"clause\":[") ||
        !journal.integers(premise.literals) || !journal.append("]}\n")) {
      result.status = BatchPublicationStatus::ResourceExhausted;
      return result;
    }
  }
  uint64_t journal_id = k.next_clause_id;
  for (const PlannedDefinition &definition : plan.definitions_) {
    if (!journal.append(
            "{\"kind\":\"exact-cover-theory-variable\",\"action\":" +
            std::to_string(request.action_id) +
            ",\"semantic_action\":" +
            std::to_string(definition.semantic.action) +
            ",\"semantic_slot\":" +
            std::to_string(definition.semantic.slot) + ",\"block\":" +
            std::to_string(definition.block) + ",\"output\":" +
            std::to_string(definition.output) +
            ",\"role\":\"block-occupancy\",\"meaning\":\"output iff "
            "OR(inputs)\",\"projection\":\"omit-fresh-output\","
            "\"lifetime\":\"ordinary-live-clauses\",\"inputs\":[") ||
        !journal.integers(definition.inputs) ||
        !journal.append("],\"source_premises\":[") ||
        !journal.unsigned_integers(definition.source_premises) ||
        !journal.append("],\"duplicate_key\":[") ||
        !journal.integers(definition.duplicate_key.disjunction) ||
        !journal.append("],\"proof_additions\":" +
                        std::to_string(definition.proof_additions) +
                        ",\"literal_occurrences\":" +
                        std::to_string(definition.literal_occurrences) +
                        ",\"extent_growth\":" +
                        std::to_string(definition.extent_growth) + "}\n")) {
      result.status = BatchPublicationStatus::ResourceExhausted;
      return result;
    }
    for (uint32_t i = 0; i < plan.arity_; ++i) {
      ++journal_id;
      if (!journal.append(
              "{\"kind\":\"exact-cover-definition-rat\",\"action\":" +
              std::to_string(request.action_id) +
              ",\"semantic_action\":" +
              std::to_string(definition.semantic.action) +
              ",\"semantic_slot\":" +
              std::to_string(definition.semantic.slot) + ",\"id\":" +
              std::to_string(journal_id) + ",\"clause\":[" +
              std::to_string(definition.output) + "," +
              std::to_string(-definition.inputs[i]) + "]}\n")) {
        result.status = BatchPublicationStatus::ResourceExhausted;
        return result;
      }
    }
    ++journal_id;
    if (!journal.append(
            "{\"kind\":\"exact-cover-definition-rat\",\"action\":" +
            std::to_string(request.action_id) +
            ",\"semantic_action\":" +
            std::to_string(definition.semantic.action) +
            ",\"semantic_slot\":" +
            std::to_string(definition.semantic.slot) + ",\"id\":" +
            std::to_string(journal_id) + ",\"clause\":[") ||
        !journal.integers(definition.formula.back()) ||
        !journal.append("]}\n")) {
      result.status = BatchPublicationStatus::ResourceExhausted;
      return result;
    }
  }
  if (kernel_terminating(k)) {
    result.status = BatchPublicationStatus::Interrupted;
    return result;
  }
  if (!retained_source_is_live(k, plan.source_)) {
    result.status = BatchPublicationStatus::Stale;
    return result;
  }

  // Commit starts here.  Semantic validity, termination, caller budgets, and
  // kernel capacity/identity bounds have all been checked.  Allocation or
  // record I/O failure remains fatal under the kernel's commit convention.
  k.close_startup_certificate();
  const Var first = kernel_new_extension_vars(
      k, static_cast<uint32_t>(plan.definitions_.size()));

  for (uint32_t slot = 0; slot < plan.definitions_.size(); ++slot) {
    const PlannedDefinition &definition = plan.definitions_[slot];
    const Lit output = make_lit(first + slot, false);
    if (kernel_externalize(k, output) != definition.output) std::abort();
    std::vector<int64_t> hints(plan.arity_);
    for (uint32_t i = 0; i < plan.arity_; ++i) {
      const Lit input = kernel_internalize(k, definition.inputs[i]);
      const Lit clause[] = {output, negate(input)};
      const uint64_t id = ++k.next_clause_id;
      if (k.proof) k.proof->add_rat(id, clause, 2, nullptr, 0);
      kernel_add_binary_clause(k, clause[0], clause[1], id);
      hints[i] = -static_cast<int64_t>(id);
    }
    std::vector<Lit> reverse(plan.arity_ + 1);
    reverse[0] = negate(output);
    for (uint32_t i = 0; i < plan.arity_; ++i)
      reverse[i + 1] = kernel_internalize(k, definition.inputs[i]);
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_rat(id, reverse.data(), reverse.size(), hints.data(),
                       hints.size());
    kernel_add_large_clause(k, reverse.data(), reverse.size(), false, 0, id);
  }
  if (k.next_clause_id != journal_id) std::abort();
  if (std::fwrite(journal.bytes.data(), 1, journal.bytes.size(),
                  request.records) != journal.bytes.size() ||
      std::fflush(request.records) || std::ferror(request.records))
    std::abort();
  result.status = BatchPublicationStatus::Published;
  result.result = ApplyResult::Ok;
  result.definitions = plan.definitions_.size();
  result.additions = plan.proof_additions_;
  result.literal_occurrences = plan.literal_occurrences_;
  result.extent_growth = plan.extent_growth_;
  result.journal_bytes = journal.bytes.size();
  return result;
}

} // namespace tactix::exact_cover_semantic
