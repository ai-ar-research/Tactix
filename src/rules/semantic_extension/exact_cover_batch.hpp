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

struct PlannedDefinition {
  uint32_t block = 0;
  uint32_t semantic_slot = 0;
  uint64_t semantic_id = 0;
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

struct BatchPlan {
  uint32_t arity = 0;
  uint32_t first_output_external = 0;
  std::vector<Premise> source;
  std::vector<PlannedDefinition> definitions;
  uint64_t proof_additions = 0;
  uint64_t literal_occurrences = 0;
  uint32_t extent_growth = 0;
  bool operator==(const BatchPlan &) const = default;
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
  uint64_t first_semantic_id = 0;
};

// Pure, transactional planning.  Recognition runs exactly once over `source`;
// all definitions retain concrete formulas and provenance, so publishing the
// first definition cannot invalidate the remaining plan.  `out` changes only
// on complete success.
template <class Charge>
bool plan_batch(const std::vector<Premise> &source,
                const BatchPlanningRequest &request, Charge &&charge,
                BatchPlan &out) {
  if (request.arity < 3 || !request.maximum_items ||
      !request.maximum_source_clauses || !request.maximum_source_literals ||
      !request.maximum_premise_references || !request.maximum_definitions ||
      !request.maximum_additions || !request.maximum_literal_occurrences ||
      !request.maximum_extent_growth || !request.first_output_external ||
      !request.first_semantic_id ||
      source.size() > request.maximum_source_clauses)
    return false;

  uint64_t source_literals = 0;
  uint32_t maximum_source_variable = 0;
  for (const Premise &premise : source) {
    if (!charge(uint64_t{1} + premise.literals.size()) ||
        premise.literals.size() >
            request.maximum_source_literals - source_literals)
      return false;
    source_literals += premise.literals.size();
    for (int32_t literal : premise.literals) {
      if (!literal || literal == INT32_MIN) return false;
      maximum_source_variable = std::max(maximum_source_variable,
                                         variable(literal));
    }
  }
  if (request.first_output_external <= maximum_source_variable) return false;

  Image image;
  if (!recognize(source, request.arity, request.maximum_items, charge, image))
    return false;

  // The retained image is a one-shot batch: publish every recognized block
  // before the fresh definitions make strict source recognition impossible.
  // Selective/repeated publication needs a persistent controller-owned session
  // with consumed keys, which this foundation deliberately does not claim.
  std::vector<uint32_t> selected(image.blocks.size());
  for (uint32_t block = 0; block < selected.size(); ++block)
    selected[block] = block;
  if (selected.empty()) return false;
  const uint64_t last_output = uint64_t{request.first_output_external} +
                               selected.size() - 1;
  if (selected.size() > request.maximum_definitions ||
      selected.size() > request.maximum_extent_growth ||
      last_output > INT32_MAX ||
      last_output > request.maximum_external_extent ||
      selected.size() - 1 > UINT64_MAX - request.first_semantic_id)
    return false;
  std::set<uint32_t> unique_blocks;
  for (uint32_t block : selected)
    if (block >= image.blocks.size() || !unique_blocks.insert(block).second)
      return false;

  std::map<int32_t, const Item *> items;
  for (const Item &item : image.items) items.emplace(item.literal, &item);
  BatchPlan plan;
  plan.arity = image.arity;
  plan.first_output_external = request.first_output_external;
  plan.source = source;
  uint64_t premise_references = 0;
  for (uint32_t slot = 0; slot < selected.size(); ++slot) {
    if (!charge(uint64_t{1} + request.arity)) return false;
    const uint32_t block_index = selected[slot];
    const Block &block = image.blocks[block_index];
    PlannedDefinition definition;
    definition.block = block_index;
    definition.semantic_slot = slot;
    definition.semantic_id = request.first_semantic_id + slot;
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
      if (found == items.end()) return false;
      for (uint32_t cover : found->second->covers) {
        if (cover >= image.covers.size()) return false;
        premises.insert(image.covers[cover].premise);
      }
    }
    if (!charge(premises.size()) ||
        premises.size() > request.maximum_premise_references - premise_references)
      return false;
    premise_references += premises.size();
    definition.source_premises.assign(premises.begin(), premises.end());
    if (definition.proof_additions >
            request.maximum_additions - plan.proof_additions ||
        definition.literal_occurrences >
            request.maximum_literal_occurrences - plan.literal_occurrences)
      return false;
    plan.proof_additions += definition.proof_additions;
    plan.literal_occurrences += definition.literal_occurrences;
    ++plan.extent_growth;
    plan.definitions.push_back(std::move(definition));
  }
  out = std::move(plan);
  return true;
}

struct BatchPublicationRequest {
  uint64_t action_id = 0;
  FILE *records = nullptr;
};

struct BatchPublicationResult {
  ApplyResult result = ApplyResult::NoOp;
  uint32_t definitions = 0;
  uint64_t additions = 0;
  uint64_t literal_occurrences = 0;
  uint32_t extent_growth = 0;
};

// Atomically publish a completed plan with respect to semantic decline,
// termination, and caller budgets.  Those checks precede the first mutation;
// commit has no interruption point and follows the kernel convention that
// allocation or record I/O failure is fatal rather than recoverable.
BatchPublicationResult publish_batch(Kernel &, const BatchPlan &,
                                     const BatchPublicationRequest &);

} // namespace tactix::exact_cover_semantic
