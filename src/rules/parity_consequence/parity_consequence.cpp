#include "rules/parity_consequence/parity_consequence.hpp"
#include "rules/lucky/parity_live.hpp"
#include "proof/proof.hpp"
#include <cstdlib>
#include <limits>
namespace tactix {
RuleEligibility parity_consequence_guard(const Kernel &k) {
  return RuleEligibility{}
      .fact("consistent", !k.inconsistent)
      .fact("root", !k.level)
      .fact("no-conflict", !k.conflict_valid && !k.pending.valid)
      .fact("propagated", kernel_propagation_fixpoint(k))
      .fact("sparse", !k.dense && k.large_watches_connected)
      .fact("no-probe-transaction", !k.probing.recording)
      .fact("provenance-bound", k.structural_provenance != nullptr)
      .fact("action-id-available", k.structural_action_sequence != UINT64_MAX)
      .fact("clausal-certificate", !k.proof || (!k.proof->lrat() && !k.proof->pb()));
}
ApplyResult parity_consequence_apply(Kernel &k) {
  const auto &p = k.parity_consequence_parameters;
  if (!parity_consequence_guard(k) || !p.valid() ||
      p.consequence > std::numeric_limits<size_t>::max()) return ApplyResult::NoOp;
  const uint64_t action = ++k.structural_action_sequence;
  FILE *out = k.structural_provenance;
  std::fprintf(out, "{\"action\":%llu,\"kind\":\"action-context\",\"version\":1,\"rule\":\"ParityConsequence\",\"strategy_epoch\":%llu,\"arena_generation\":%llu,\"touch_epoch\":%llu,\"proof_id_before\":%llu,\"external_extent\":%zu,\"original_variables\":%u,\"reconstruction\":\"unchanged;proof-only-helpers-projected\"}\n",
      (unsigned long long)action, (unsigned long long)k.strategy_epoch,
      (unsigned long long)k.arena_generation, (unsigned long long)k.touch_epoch,
      (unsigned long long)k.next_clause_id, k.external_to_internal.size(),
      kernel_num_output_vars(k));
  if (std::ferror(out)) std::abort();
  const auto result = parity_live_apply(k, {{p.maximum_clauses, p.maximum_groups},
      {p.selection_work, p.intermediate_width, p.source_count, p.output_width},
      p.descending, p.maximum_proof_additions, p.maximum_tree_variables,
      static_cast<size_t>(p.consequence), action, out});
  ++k.stats.parity_consequence_attempts;
  k.stats.parity_consequence_clauses += result.clauses;
  k.stats.parity_consequence_equations += result.equations;
  k.stats.parity_consequence_selection_work += result.work;
  k.stats.parity_consequence_preflight_additions += result.preflight_additions;
  k.stats.parity_consequence_additions += result.additions;
  k.stats.parity_consequence_deletions += result.deletions;
  k.stats.parity_consequence_helpers += result.helpers;
  k.stats.parity_consequence_published += result.published;
  std::fprintf(out, "{\"action\":%llu,\"kind\":\"action-complete\",\"result\":%u,\"clauses\":%llu,\"equations\":%llu,\"selection_work\":%llu,\"preflight_additions\":%llu,\"additions\":%llu,\"deletions\":%llu,\"helpers\":%u,\"published\":%llu,\"proof_id_after\":%llu}\n",
      (unsigned long long)action, static_cast<unsigned>(result.result),
      (unsigned long long)result.clauses, (unsigned long long)result.equations,
      (unsigned long long)result.work, (unsigned long long)result.preflight_additions,
      (unsigned long long)result.additions, (unsigned long long)result.deletions,
      result.helpers, (unsigned long long)result.published,
      (unsigned long long)k.next_clause_id);
  std::fflush(out);
  if (std::ferror(out)) std::abort();
  return result.result;
}
TACTIX_REGISTER_RULE(ParityConsequence, RuleTier::Inprocess,
                     parity_consequence_guard, parity_consequence_apply);
} // namespace tactix
