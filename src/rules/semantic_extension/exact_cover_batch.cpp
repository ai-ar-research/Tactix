#include "rules/semantic_extension/exact_cover_batch.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cstdlib>
#include <set>

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
  std::set<std::vector<int32_t>> live_large;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = tactix::clause_at(k.arena, ref);
    ref += tactix::clause_actual_words(clause);
    if (clause->garbage()) continue;
    live_large.insert(normalized_external(k, clause->lits, clause->size));
  }
  for (const Premise &premise : source) {
    std::vector<int32_t> wanted = premise.literals;
    std::sort(wanted.begin(), wanted.end());
    if (wanted.size() == 2) {
      const Lit first = tactix::kernel_internalize(k, wanted[0]);
      const Lit second = tactix::kernel_internalize(k, wanted[1]);
      if (first == INVALID_LIT || second == INVALID_LIT ||
          !tactix::kernel_binary_live(k, first, second))
        return false;
    } else if (!live_large.count(wanted)) {
      return false;
    }
  }
  return true;
}

} // namespace

namespace tactix::exact_cover_semantic {

BatchPublicationResult publish_batch(Kernel &k, const BatchPlan &plan,
                                     const BatchPublicationRequest &request) {
  BatchPublicationResult result;
  const uint64_t extent = k.external_to_internal.size();
  if (!request.action_id || !request.records || plan.arity < 3 ||
      plan.source.empty() || plan.definitions.empty() ||
      plan.first_output_external != extent + 1 ||
      plan.definitions.size() != plan.extent_growth ||
      plan.definitions.size() > UINT32_MAX - k.nvars ||
      extent > INT32_MAX ||
      plan.definitions.size() > uint64_t(INT32_MAX) - extent ||
      k.inconsistent || k.level || k.conflict_valid || k.pending.valid ||
      !k.trail.empty() || k.dense || k.probing.recording ||
      !k.large_watches_connected || !kernel_propagation_fixpoint(k) ||
      (k.proof && k.proof->pb()) || kernel_terminating(k) ||
      !retained_source_is_live(k, plan.source))
    return result;

  uint64_t additions = 0, literal_occurrences = 0, arena_words = 0;
  std::set<std::vector<int32_t>> duplicate_keys;
  std::set<uint64_t> source_occurrences, semantic_ids;
  std::set<uint32_t> planned_blocks;
  for (const Premise &premise : plan.source)
    if (!premise.occurrence ||
        !source_occurrences.insert(premise.occurrence).second)
      return result;
  for (uint32_t slot = 0; slot < plan.definitions.size(); ++slot) {
    const PlannedDefinition &definition = plan.definitions[slot];
    if (definition.semantic_slot != slot || !definition.semantic_id ||
        !semantic_ids.insert(definition.semantic_id).second ||
        !planned_blocks.insert(definition.block).second ||
        definition.output != int64_t(plan.first_output_external) + slot ||
        definition.inputs.size() != plan.arity ||
        definition.formula.size() != plan.arity + 1 ||
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
        !duplicate_keys.insert(definition.duplicate_key.disjunction).second)
      return result;
    for (uint64_t premise : definition.source_premises)
      if (!source_occurrences.count(premise)) return result;
    std::set<uint32_t> variables;
    uint64_t expected_literals = 0;
    for (uint32_t i = 0; i < plan.arity; ++i) {
      const int32_t input = definition.inputs[i];
      if (!input || input == INT32_MIN ||
          !variables.insert(variable(input)).second ||
          definition.formula[i] !=
              std::vector<int32_t>({definition.output, -input}))
        return result;
      const Lit internal = kernel_internalize(k, input);
      if (internal == INVALID_LIT || kernel_var_retired(k, var_of(internal)))
        return result;
      expected_literals += 2;
    }
    std::vector<int32_t> reverse{-definition.output};
    reverse.insert(reverse.end(), definition.inputs.begin(),
                   definition.inputs.end());
    if (definition.formula.back() != reverse) return result;
    expected_literals += reverse.size();
    if (definition.literal_occurrences != expected_literals) return result;
    additions += definition.proof_additions;
    literal_occurrences += definition.literal_occurrences;
    arena_words += reverse.size() + CLAUSE_HEADER_WORDS;
  }
  if (additions != plan.proof_additions ||
      literal_occurrences != plan.literal_occurrences ||
      arena_words + k.arena.size() >= INVALID_REF ||
      k.next_clause_id > uint64_t(INT64_MAX) - additions)
    return result;

  // Commit starts here.  Semantic validity, termination, caller budgets, and
  // kernel capacity/identity bounds have all been checked.  Allocation or
  // record I/O failure remains fatal under the kernel's commit convention.
  k.close_startup_certificate();
  const Var first = kernel_new_extension_vars(
      k, static_cast<uint32_t>(plan.definitions.size()));
  std::fprintf(request.records,
      "{\"kind\":\"exact-cover-batch\",\"action\":%llu,"
      "\"arity\":%u,\"definitions\":%llu,\"additions\":%llu,"
      "\"literal_occurrences\":%llu,\"extent_growth\":%u,"
      "\"recognition\":\"single-retained-source-image\"}\n",
      (unsigned long long)request.action_id, plan.arity,
      (unsigned long long)plan.definitions.size(),
      (unsigned long long)plan.proof_additions,
      (unsigned long long)plan.literal_occurrences, plan.extent_growth);
  for (const Premise &premise : plan.source) {
    std::fprintf(request.records,
        "{\"kind\":\"exact-cover-batch-source\",\"action\":%llu,"
        "\"occurrence\":%llu,\"clause\":[",
        (unsigned long long)request.action_id,
        (unsigned long long)premise.occurrence);
    for (size_t i = 0; i < premise.literals.size(); ++i)
      std::fprintf(request.records, "%s%d", i ? "," : "",
                   premise.literals[i]);
    std::fputs("]}\n", request.records);
  }

  for (uint32_t slot = 0; slot < plan.definitions.size(); ++slot) {
    const PlannedDefinition &definition = plan.definitions[slot];
    const Lit output = make_lit(first + slot, false);
    if (kernel_externalize(k, output) != definition.output) std::abort();
    std::fprintf(request.records,
        "{\"kind\":\"exact-cover-theory-variable\",\"action\":%llu,"
        "\"semantic\":%llu,\"semantic_slot\":%u,\"block\":%u,"
        "\"output\":%d,\"role\":\"block-occupancy\","
        "\"meaning\":\"output iff OR(inputs)\","
        "\"projection\":\"omit-fresh-output\","
        "\"lifetime\":\"ordinary-live-clauses\",\"inputs\":[",
        (unsigned long long)request.action_id,
        (unsigned long long)definition.semantic_id, definition.semantic_slot,
        definition.block, definition.output);
    for (size_t i = 0; i < definition.inputs.size(); ++i)
      std::fprintf(request.records, "%s%d", i ? "," : "",
                   definition.inputs[i]);
    std::fputs("],\"source_premises\":[", request.records);
    for (size_t i = 0; i < definition.source_premises.size(); ++i)
      std::fprintf(request.records, "%s%llu", i ? "," : "",
          (unsigned long long)definition.source_premises[i]);
    std::fprintf(request.records,
        "],\"duplicate_key\":[");
    for (size_t i = 0; i < definition.duplicate_key.disjunction.size(); ++i)
      std::fprintf(request.records, "%s%d", i ? "," : "",
                   definition.duplicate_key.disjunction[i]);
    std::fprintf(request.records,
        "],\"proof_additions\":%llu,\"literal_occurrences\":%llu,"
        "\"extent_growth\":%u}\n",
        (unsigned long long)definition.proof_additions,
        (unsigned long long)definition.literal_occurrences,
        definition.extent_growth);

    std::vector<int64_t> hints(plan.arity);
    for (uint32_t i = 0; i < plan.arity; ++i) {
      const Lit input = kernel_internalize(k, definition.inputs[i]);
      const Lit clause[] = {output, negate(input)};
      const uint64_t id = ++k.next_clause_id;
      if (k.proof) k.proof->add_rat(id, clause, 2, nullptr, 0);
      kernel_add_binary_clause(k, clause[0], clause[1], id);
      hints[i] = -static_cast<int64_t>(id);
      std::fprintf(request.records,
          "{\"kind\":\"exact-cover-definition-rat\","
          "\"action\":%llu,\"semantic\":%llu,\"id\":%llu,"
          "\"clause\":[%d,%d]}\n",
          (unsigned long long)request.action_id,
          (unsigned long long)definition.semantic_id,
          (unsigned long long)id, definition.output, -definition.inputs[i]);
    }
    std::vector<Lit> reverse(plan.arity + 1);
    reverse[0] = negate(output);
    for (uint32_t i = 0; i < plan.arity; ++i)
      reverse[i + 1] = kernel_internalize(k, definition.inputs[i]);
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_rat(id, reverse.data(), reverse.size(), hints.data(),
                       hints.size());
    kernel_add_large_clause(k, reverse.data(), reverse.size(), false, 0, id);
    std::fprintf(request.records,
        "{\"kind\":\"exact-cover-definition-rat\","
        "\"action\":%llu,\"semantic\":%llu,\"id\":%llu,"
        "\"clause\":[",
        (unsigned long long)request.action_id,
        (unsigned long long)definition.semantic_id, (unsigned long long)id);
    for (size_t i = 0; i < definition.formula.back().size(); ++i)
      std::fprintf(request.records, "%s%d", i ? "," : "",
                   definition.formula.back()[i]);
    std::fputs("]}\n", request.records);
  }
  if (std::fflush(request.records) || std::ferror(request.records)) std::abort();
  result.result = ApplyResult::Ok;
  result.definitions = plan.definitions.size();
  result.additions = plan.proof_additions;
  result.literal_occurrences = plan.literal_occurrences;
  result.extent_growth = plan.extent_growth;
  return result;
}

} // namespace tactix::exact_cover_semantic
