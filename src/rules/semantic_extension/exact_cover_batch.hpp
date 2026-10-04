#pragma once

#include "rules/registry.hpp"
#include "rules/semantic_extension/exact_cover_recognition.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace tactix::exact_cover_semantic {

enum class TheoryVariableKind : uint8_t { BlockOccupancy };
enum class ModelProjection : uint8_t { OmitFreshOutput };
enum class VariableLifetime : uint8_t { OrdinaryLiveClauses };

struct DuplicateKey {
  // Exact structural identity, rather than a collision-prone hash.
  std::vector<int32_t> disjunction;
  bool operator==(const DuplicateKey &) const = default;
};

struct SemanticIdentity {
  // Semantic identities are action-scoped.  The pair is collision-free as
  // long as the runtime action sequence is monotone; no caller-owned global
  // semantic counter is required.
  uint64_t action = 0;
  uint32_t slot = 0;
  bool operator==(const SemanticIdentity &) const = default;
};

struct PlannedDefinition {
  uint32_t block = 0;
  SemanticIdentity semantic;
  int32_t output = 0;
  TheoryVariableKind kind = TheoryVariableKind::BlockOccupancy;
  ModelProjection projection = ModelProjection::OmitFreshOutput;
  VariableLifetime lifetime = VariableLifetime::OrdinaryLiveClauses;
  std::vector<int32_t> inputs;
  // Concrete RAT clauses, pivot first: (output v -input_i), followed by
  // (-output v inputs...).
  std::vector<std::vector<int32_t>> formula;
  // Exact occurrences supporting this block's clique and the covers incident
  // to its items.  The plan's retained source resolves every occurrence.
  std::vector<uint64_t> source_premises;
  DuplicateKey duplicate_key;
  uint64_t proof_additions = 0;
  uint64_t literal_occurrences = 0;
  uint32_t extent_growth = 0;
  bool operator==(const PlannedDefinition &) const = default;
};

struct BatchPlanningRequest;
struct BatchPublicationRequest;
struct BatchPublicationResult;
enum class BatchPlanningStatus : uint8_t {
  Complete,
  Invalid,
  NoStructure,
  ResourceExhausted,
};

template <class Charge>
bool plan_batch(const std::vector<Premise> &, const BatchPlanningRequest &,
                Charge &&, class BatchPlan &, BatchPlanningStatus * = nullptr);

class BatchPlan {
public:
  BatchPlan() = default;
  uint32_t arity() const { return arity_; }
  uint32_t first_output_external() const { return first_output_external_; }
  const std::vector<Premise> &source() const { return source_; }
  const std::vector<PlannedDefinition> &definitions() const {
    return definitions_;
  }
  uint64_t proof_additions() const { return proof_additions_; }
  uint64_t literal_occurrences() const { return literal_occurrences_; }
  uint32_t extent_growth() const { return extent_growth_; }
  bool operator==(const BatchPlan &) const = default;

private:
  template <class Charge>
  friend bool plan_batch(const std::vector<Premise> &,
                         const BatchPlanningRequest &, Charge &&, BatchPlan &,
                         BatchPlanningStatus *);
  friend BatchPublicationResult publish_batch(
      Kernel &, const BatchPlan &, const BatchPublicationRequest &);

  uint32_t arity_ = 0;
  uint32_t first_output_external_ = 0;
  std::vector<Premise> source_;
  std::vector<PlannedDefinition> definitions_;
  uint64_t proof_additions_ = 0;
  uint64_t literal_occurrences_ = 0;
  uint32_t extent_growth_ = 0;
};

struct BatchPlanningRequest {
  uint32_t arity = 0;
  uint64_t maximum_items = 0;
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_premise_references = 0;
  uint32_t maximum_definitions = 0;
  uint64_t maximum_additions = 0;
  uint64_t maximum_literal_occurrences = 0;
  uint32_t maximum_extent_growth = 0;
  uint32_t first_output_external = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t action_id = 0;
};

// Pure, transactional planning.  Recognition runs exactly once over `source`;
// all definitions retain concrete formulas and provenance, so publishing the
// first definition cannot invalidate the remaining plan.  `out` changes only
// on complete success.
template <class Charge>
bool plan_batch(const std::vector<Premise> &source,
                const BatchPlanningRequest &request, Charge &&charge,
                BatchPlan &out, BatchPlanningStatus *status) {
  const auto fail = [status](BatchPlanningStatus value) {
    if (status) *status = value;
    return false;
  };
  if (request.arity < 3 || !request.maximum_items ||
      !request.maximum_source_clauses || !request.maximum_source_literals ||
      !request.maximum_premise_references || !request.maximum_definitions ||
      !request.maximum_additions || !request.maximum_literal_occurrences ||
      !request.maximum_extent_growth || !request.first_output_external ||
      !request.action_id)
    return fail(BatchPlanningStatus::Invalid);
  if (source.size() > request.maximum_source_clauses)
    return fail(BatchPlanningStatus::ResourceExhausted);

  uint64_t source_literals = 0;
  uint32_t maximum_source_variable = 0;
  std::set<int32_t> candidate_items;
  for (const Premise &premise : source) {
    if (premise.literals.size() >
        request.maximum_source_literals - source_literals)
      return fail(BatchPlanningStatus::ResourceExhausted);
    if (!charge(uint64_t{1} + premise.literals.size()))
      return fail(BatchPlanningStatus::ResourceExhausted);
    source_literals += premise.literals.size();
    for (int32_t literal : premise.literals) {
      if (!literal || literal == INT32_MIN)
        return fail(BatchPlanningStatus::Invalid);
      maximum_source_variable = std::max(maximum_source_variable,
                                         variable(literal));
    }
    if (premise.literals.size() == 2)
      for (int32_t literal : premise.literals)
        candidate_items.insert(-literal);
  }
  if (candidate_items.size() > request.maximum_items)
    return fail(BatchPlanningStatus::ResourceExhausted);
  if (request.first_output_external <= maximum_source_variable)
    return fail(BatchPlanningStatus::Invalid);

  Image image;
  if (!recognize(source, request.arity, request.maximum_items, charge, image))
    return fail(BatchPlanningStatus::NoStructure);

  // The retained image is a one-shot batch: publish every recognized block
  // before the fresh definitions make strict source recognition impossible.
  // Selective/repeated publication needs a persistent controller-owned session
  // with consumed keys, which this foundation deliberately does not claim.
  if (image.blocks.size() > UINT32_MAX)
    return fail(BatchPlanningStatus::ResourceExhausted);
  std::vector<uint32_t> selected(image.blocks.size());
  for (uint32_t block = 0; block < selected.size(); ++block)
    selected[block] = block;
  if (selected.empty()) return fail(BatchPlanningStatus::NoStructure);
  const uint64_t last_output = uint64_t{request.first_output_external} +
                               selected.size() - 1;
  if (selected.size() > request.maximum_definitions ||
      selected.size() > request.maximum_extent_growth ||
      last_output > INT32_MAX ||
      last_output > request.maximum_external_extent)
    return fail(BatchPlanningStatus::ResourceExhausted);
  std::set<uint32_t> unique_blocks;
  for (uint32_t block : selected)
    if (block >= image.blocks.size() || !unique_blocks.insert(block).second)
      return fail(BatchPlanningStatus::Invalid);

  std::map<int32_t, const Item *> items;
  for (const Item &item : image.items) items.emplace(item.literal, &item);
  BatchPlan plan;
  plan.arity_ = image.arity;
  plan.first_output_external_ = request.first_output_external;
  plan.source_ = source;
  uint64_t premise_references = 0;
  for (uint32_t slot = 0; slot < selected.size(); ++slot) {
    if (!charge(uint64_t{1} + request.arity))
      return fail(BatchPlanningStatus::ResourceExhausted);
    const uint32_t block_index = selected[slot];
    const Block &block = image.blocks[block_index];
    PlannedDefinition definition;
    definition.block = block_index;
    definition.semantic = {request.action_id, slot};
    definition.output = static_cast<int32_t>(request.first_output_external + slot);
    definition.inputs = block.items;
    definition.duplicate_key.disjunction = block.items;
    definition.extent_growth = 1;
    for (int32_t input : block.items)
      definition.formula.push_back({definition.output, -input});
    std::vector<int32_t> reverse{-definition.output};
    reverse.insert(reverse.end(), block.items.begin(), block.items.end());
    definition.formula.push_back(std::move(reverse));
    definition.proof_additions = definition.formula.size();
    for (const auto &clause : definition.formula)
      definition.literal_occurrences += clause.size();

    std::set<uint64_t> premises;
    for (uint32_t first = 0; first < image.arity; ++first)
      for (uint32_t second = first + 1; second < image.arity; ++second)
        premises.insert(block.exclusions[size_t(first) * image.arity + second]);
    for (int32_t input : block.items) {
      const auto found = items.find(input);
      if (found == items.end()) return fail(BatchPlanningStatus::Invalid);
      for (uint32_t cover : found->second->covers) {
        if (cover >= image.covers.size())
          return fail(BatchPlanningStatus::Invalid);
        premises.insert(image.covers[cover].premise);
      }
    }
    if (premises.size() >
        request.maximum_premise_references - premise_references)
      return fail(BatchPlanningStatus::ResourceExhausted);
    if (!charge(premises.size()))
      return fail(BatchPlanningStatus::ResourceExhausted);
    premise_references += premises.size();
    definition.source_premises.assign(premises.begin(), premises.end());
    if (definition.proof_additions >
            request.maximum_additions - plan.proof_additions_ ||
        definition.literal_occurrences >
            request.maximum_literal_occurrences - plan.literal_occurrences_)
      return fail(BatchPlanningStatus::ResourceExhausted);
    plan.proof_additions_ += definition.proof_additions;
    plan.literal_occurrences_ += definition.literal_occurrences;
    ++plan.extent_growth_;
    plan.definitions_.push_back(std::move(definition));
  }
  out = std::move(plan);
  if (status) *status = BatchPlanningStatus::Complete;
  return true;
}

struct BatchPublicationRequest {
  uint64_t action_id = 0;
  uint64_t maximum_journal_bytes = 0;
  FILE *records = nullptr;
};

enum class BatchPublicationStatus : uint8_t {
  Published,
  Invalid,
  Inapplicable,
  Stale,
  UnsupportedProof,
  ResourceExhausted,
  Interrupted,
};

struct BatchPublicationResult {
  BatchPublicationStatus status = BatchPublicationStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint32_t definitions = 0;
  uint64_t additions = 0;
  uint64_t literal_occurrences = 0;
  uint32_t extent_growth = 0;
  uint64_t journal_bytes = 0;
};

// Atomically publish a completed plan with respect to semantic decline,
// termination, and caller budgets.  Those checks precede the first mutation;
// commit has no interruption point and follows the kernel convention that
// allocation or record I/O failure is fatal rather than recoverable.
BatchPublicationResult publish_batch(Kernel &, const BatchPlan &,
                                     const BatchPublicationRequest &);

} // namespace tactix::exact_cover_semantic
