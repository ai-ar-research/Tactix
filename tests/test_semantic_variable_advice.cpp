#undef NDEBUG
#include "policies/heuristics.hpp"
#include "policies/ops/semantic_variable_advice/semantic_variable_advice.hpp"
#include "strategy/controller_observation.hpp"
#include "strategy/feedback_trace.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <cassert>
#include <string>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;

static std::vector<double> scores(const Kernel &k) {
  return k.h->scores.score;
}

int main() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  ScopedHeuristics owner(k);
  const auto original_scores = scores(k);
  const Var original_last = k.h->queue_last;
  const SemanticVariableAdviceLimits limits{3, 6};

  SemanticVariableAdvice advice;
  advice.action_id = 7;
  advice.source = state_version(k);
  advice.variables = {{1, SemanticVariableRole::ExistingTheory, 1},
                      {3, SemanticVariableRole::IntroducedTheory, 3}};
  const uint64_t source_strategy_epoch = advice.source.strategy_epoch;
  const auto applied = apply_semantic_variable_advice(k, advice, limits);
  assert(applied.status == SemanticVariableAdviceStatus::Applied);
  assert(applied.variables == 2 && applied.priority_sum == 4);
  assert(k.stats.semantic_variable_advice_attempts == 1);
  assert(k.stats.semantic_variable_advice_applied == 1);
  assert(k.stats.semantic_variable_advice_variables == 2);
  assert(k.stats.semantic_variable_advice_priority == 4);
  assert(k.strategy_epoch == source_strategy_epoch + 1);
  assert(k.h->queue_last == k.external_to_internal[2]);
  assert(k.h->scores.score[k.external_to_internal[2]] >
         k.h->scores.score[k.external_to_internal[0]]);

  // A stale request and every malformed/nonsemantic request are atomic.
  const auto after_scores = scores(k);
  const Var after_last = k.h->queue_last;
  assert(apply_semantic_variable_advice(k, advice, limits).status ==
         SemanticVariableAdviceStatus::Stale);
  assert(k.stats.semantic_variable_advice_stale == 1);
  assert(scores(k) == after_scores && k.h->queue_last == after_last);

  advice.source = state_version(k);
  advice.variables = {{2, SemanticVariableRole::ProofOnly, 1}};
  assert(apply_semantic_variable_advice(k, advice, limits).status ==
         SemanticVariableAdviceStatus::Invalid);
  assert(k.stats.semantic_variable_advice_invalid == 1);
  assert(scores(k) == after_scores && k.h->queue_last == after_last);

  advice.variables = {{1, SemanticVariableRole::ExistingTheory, 4},
                      {2, SemanticVariableRole::IntroducedTheory, 3}};
  assert(apply_semantic_variable_advice(k, advice, limits).status ==
         SemanticVariableAdviceStatus::ResourceExhausted);
  assert(k.stats.semantic_variable_advice_resource_exhausted == 1);
  assert(scores(k) == after_scores && k.h->queue_last == after_last);

  advice.variables = {{1, SemanticVariableRole::ExistingTheory, 1},
                      {1, SemanticVariableRole::IntroducedTheory, 1}};
  assert(apply_semantic_variable_advice(k, advice, limits).status ==
         SemanticVariableAdviceStatus::Invalid);
  assert(k.stats.semantic_variable_advice_invalid == 2);
  assert(k.stats.semantic_variable_advice_attempts == 5);
  assert(scores(k) == after_scores && k.h->queue_last == after_last);
  assert(original_scores.size() == scores(k).size() && original_last != INVALID_VAR);

  // Producer declarations become visible only after the registry-owned epoch
  // transition and bind advice to that exact post-action state.
  begin_semantic_variable_candidate_action(k);
  assert(stage_semantic_variable_candidate(
      k, SemanticVariableProducer::ExactCover, 91,
      {{2, SemanticVariableRole::IntroducedTheory, 99},
       {4, SemanticVariableRole::IntroducedTheory, 99}}));
  ++k.strategy_epoch;
  finish_semantic_variable_candidate_action(k, ApplyResult::Ok);
  const auto candidates = semantic_variable_candidates(k);
  assert(candidates.size() == 1 && candidates[0].action_id == 91 &&
         candidates[0].introduced_variables == 2 &&
         !candidates[0].existing_variables);
  assert(k.stats.semantic_variable_candidates_published == 1);
  assert(k.stats.semantic_variable_candidate_existing == 0);
  assert(k.stats.semantic_variable_candidate_introduced == 2);
  const auto observed = controller::observe_kernel(k);
  assert(observed.semantic_variable_candidates == candidates);
  const std::string serialized = controller::serialize_observation(observed);
  assert(serialized.find("\"semantic_variable_candidates\":[{\"id\":" +
                         std::to_string(candidates[0].id)) !=
         std::string::npos);
  assert(serialized.find("\"action_id\":91") != std::string::npos);
  SemanticVariableAdvice selected;
  assert(make_semantic_variable_advice(k, candidates[0].id, 1, 2, selected));
  assert(selected.source.strategy_epoch == k.strategy_epoch);
  assert(selected.variables[0].priority == 2 &&
         selected.variables[1].priority == 2);
  assert(apply_semantic_variable_advice(k, selected, limits).status ==
         SemanticVariableAdviceStatus::Applied);
  assert(k.stats.semantic_variable_advice_attempts == 6);
  assert(k.stats.semantic_variable_advice_applied == 2);
  assert(k.stats.semantic_variable_advice_variables == 4);
  assert(k.stats.semantic_variable_advice_priority == 8);
  assert(semantic_variable_candidates(k).empty());

  begin_semantic_variable_candidate_action(k);
  assert(stage_semantic_variable_candidate(
      k, SemanticVariableProducer::Affine, 93,
      {{1, SemanticVariableRole::ExistingTheory, 1}}));
  ++k.strategy_epoch;
  finish_semantic_variable_candidate_action(k, ApplyResult::Ok);
  assert(k.stats.semantic_variable_candidates_published == 2);
  assert(k.stats.semantic_variable_candidate_existing == 1);
  assert(k.stats.semantic_variable_candidate_introduced == 2);
  SemanticVariableAdviceParameters policy_parameters;
  policy_parameters.maximum_variables = 2;
  policy_parameters.maximum_priority_sum = 3;
  policy_parameters.existing_theory_priority = 2;
  policy_parameters.introduced_theory_priority = 1;
  install_rule_parameters(k, policy_parameters);
  uint64_t instance = 0;
  assert(semantic_variable_advice_rule_instances(k, &instance, 1) == 1);
  const uint64_t before_rule_epoch = k.strategy_epoch;
  assert(semantic_variable_advice_rule_apply_instance(k, instance) ==
         ApplyResult::Ok);
  assert(k.stats.semantic_variable_advice_attempts == 7);
  assert(k.stats.semantic_variable_advice_applied == 3);
  assert(k.stats.semantic_variable_advice_variables == 5);
  assert(k.stats.semantic_variable_advice_priority == 10);
  // The rule body leaves the single epoch transition to registry dispatch.
  assert(k.strategy_epoch == before_rule_epoch);
  ++k.strategy_epoch;
  assert(semantic_variable_candidates(k).empty());

  begin_semantic_variable_candidate_action(k);
  assert(!stage_semantic_variable_candidate(
      k, SemanticVariableProducer::Affine, 92,
      {{1, SemanticVariableRole::ProofOnly, 1}}));
  finish_semantic_variable_candidate_action(k, ApplyResult::NoOp);

  // A producer cannot leave the first of two declarations publishable after
  // violating the one-candidate transaction contract.
  begin_semantic_variable_candidate_action(k);
  assert(stage_semantic_variable_candidate(
      k, SemanticVariableProducer::IndependentSet, 94,
      {{3, SemanticVariableRole::ExistingTheory, 1}}));
  assert(!stage_semantic_variable_candidate(
      k, SemanticVariableProducer::IndependentSet, 94,
      {{4, SemanticVariableRole::ExistingTheory, 1}}));
  ++k.strategy_epoch;
  finish_semantic_variable_candidate_action(k, ApplyResult::Ok);
  assert(k.stats.semantic_variable_candidates_published == 2);
  assert(semantic_variable_candidates(k).empty());
}
