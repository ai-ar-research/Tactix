#undef NDEBUG
#include "policies/heuristics.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <cassert>
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
  assert(k.strategy_epoch == source_strategy_epoch + 1);
  assert(k.h->queue_last == k.external_to_internal[2]);
  assert(k.h->scores.score[k.external_to_internal[2]] >
         k.h->scores.score[k.external_to_internal[0]]);

  // A stale request and every malformed/nonsemantic request are atomic.
  const auto after_scores = scores(k);
  const Var after_last = k.h->queue_last;
  assert(apply_semantic_variable_advice(k, advice, limits).status ==
         SemanticVariableAdviceStatus::Stale);
  assert(scores(k) == after_scores && k.h->queue_last == after_last);

  advice.source = state_version(k);
  advice.variables = {{2, SemanticVariableRole::ProofOnly, 1}};
  assert(apply_semantic_variable_advice(k, advice, limits).status ==
         SemanticVariableAdviceStatus::Invalid);
  assert(scores(k) == after_scores && k.h->queue_last == after_last);

  advice.variables = {{1, SemanticVariableRole::ExistingTheory, 4},
                      {2, SemanticVariableRole::IntroducedTheory, 3}};
  assert(apply_semantic_variable_advice(k, advice, limits).status ==
         SemanticVariableAdviceStatus::ResourceExhausted);
  assert(scores(k) == after_scores && k.h->queue_last == after_last);

  advice.variables = {{1, SemanticVariableRole::ExistingTheory, 1},
                      {1, SemanticVariableRole::IntroducedTheory, 1}};
  assert(apply_semantic_variable_advice(k, advice, limits).status ==
         SemanticVariableAdviceStatus::Invalid);
  assert(scores(k) == after_scores && k.h->queue_last == after_last);
  assert(original_scores.size() == scores(k).size() && original_last != INVALID_VAR);
}
