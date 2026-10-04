#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "policies/random_controller_policy.hpp"
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"
#include "strategy/online_inprocess.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <unistd.h>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

namespace {

struct Formula {
  std::vector<std::vector<int32_t>> clauses;
  std::vector<std::vector<int32_t>> values;
  void domain(std::vector<int32_t> literals) {
    values.push_back(literals);
    clauses.push_back(literals);
    for (unsigned a = 0; a < 3; ++a)
      for (unsigned b = a + 1; b < 3; ++b)
        clauses.push_back({-literals[a], -literals[b]});
  }
  void equation(std::vector<uint32_t> domains,
                std::vector<uint8_t> coefficients, uint8_t rhs) {
    uint64_t tuples = 1;
    for (size_t i = 0; i < domains.size(); ++i) tuples *= 3;
    for (uint64_t tuple = 0; tuple < tuples; ++tuple) {
      uint64_t rest = tuple;
      uint8_t sum = 0;
      std::vector<int32_t> clause;
      for (size_t i = 0; i < domains.size(); ++i) {
        const uint8_t value = rest % 3;
        rest /= 3;
        sum = (sum + coefficients[i] * value) % 3;
        clause.push_back(-values[domains[i]][value]);
      }
      if (sum != rhs) clauses.push_back(std::move(clause));
    }
  }
};

Formula chained_units() {
  Formula f;
  f.domain({1, 2, 3});
  f.domain({4, 5, 6});
  f.domain({7, 8, 9});
  f.equation({0, 1}, {1, 1}, 0);
  f.equation({1, 2}, {1, 1}, 0);
  f.equation({0, 2}, {1, 1}, 1);
  return f;
}

Kernel install() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 9);
  for (const auto &external : chained_units().clauses) {
    std::vector<Lit> clause;
    for (int32_t literal : external)
      clause.push_back(kernel_internalize(kernel, literal));
    const uint64_t id = ++kernel.next_clause_id;
    if (clause.size() == 2)
      kernel_add_binary_clause(kernel, clause[0], clause[1], id);
    else
      kernel_add_large_clause(kernel, clause.data(), clause.size(), false, 0,
                              id);
  }
  return kernel;
}

Kernel install_parsed() {
  Kernel kernel;
  for (const auto &external : chained_units().clauses) {
    std::vector<Lit> clause;
    for (int32_t literal : external)
      clause.push_back(internalize(literal));
    ingest_clause(kernel, clause);
  }
  finish_input(kernel);
  return kernel;
}

Gf3GaussianActionLimits limits(const Gf3GaussianConsequenceParameters &p = {}) {
  return {{p.maximum_source_clauses, p.maximum_source_literals,
           p.maximum_domains, p.maximum_equations,
           p.maximum_equation_arity, p.maximum_tuples_per_equation},
          {{p.maximum_bridge_work, p.maximum_bridge_source_clauses,
            p.maximum_bridge_source_literals, p.maximum_bridge_domains,
            p.maximum_bridge_equations, p.maximum_bridge_terms,
            p.maximum_authentication_references,
            p.maximum_references_per_equation,
            {p.maximum_gaussian_work, p.maximum_fill_entries,
             p.maximum_intermediate_width, p.maximum_provenance_terms,
             p.maximum_derivations, p.maximum_retained_plan_bytes}},
           p.maximum_certificate_work, p.maximum_events,
           p.maximum_event_literals, p.maximum_active_clauses,
           p.maximum_retained_source_references, p.maximum_event_width,
           p.maximum_branch_domains, p.maximum_consequences},
          p.maximum_capture_work, p.maximum_revalidation_work,
          p.maximum_proof_additions, p.maximum_proof_literal_occurrences,
          p.maximum_journal_bytes, p.maximum_identity_bytes,
          p.maximum_external_extent, p.maximum_session_keys,
          p.maximum_session_work, p.maximum_session_identity_bytes};
}

struct Bound {
  Kernel kernel = install();
  Gf3GaussianActionSession session;
  FILE *records = std::tmpfile();
  Bound() {
    assert(records);
    kernel.structural_provenance = records;
    kernel.gf3_gaussian_action_session = &session;
  }
  ~Bound() { std::fclose(records); }
};

void singular_batch_and_atomic_publication() {
  Bound b;
  const auto offered = gf3_gaussian_action_offer(b.kernel, b.session, limits());
  assert(offered.status == Gf3GaussianActionStatus::Applied);
  assert(offered.offers.size() == 1);
  assert(offered.offers[0].rfind("g3b1:", 0) == 0);
  assert(offered.live.effort.identity_bytes >= offered.offers[0].size());
  assert(offered.live.effort.identity_work ==
         offered.live.effort.identity_bytes);
  const auto applied = gf3_gaussian_action_apply(
      b.kernel, b.session, limits(), offered.offers[0]);
  assert(applied.status == Gf3GaussianActionStatus::Applied);
  assert(applied.result == ApplyResult::Ok);
  assert(applied.live.published_units.size() >= 3);
  assert(b.session.keys() == offered.offers);
  assert(b.kernel.stats.gf3_gaussian_attempts == 0);
  record_gf3_gaussian_action_feedback(b.kernel, applied);
  assert(b.kernel.stats.gf3_gaussian_applied == 1);
  // All consequences in the authenticated closure were consumed in the one
  // declared action before its root units closed the assignment-free boundary.
  assert(!b.kernel.trail.empty());
}

void corruption_rebinding_and_limits_decline_without_mutation() {
  Bound b;
  const auto offered = gf3_gaussian_action_offer(b.kernel, b.session, limits());
  assert(offered.offers.size() == 1);
  const auto before_id = b.kernel.next_clause_id;
  std::string corrupt = offered.offers[0];
  corrupt.back() = corrupt.back() == '0' ? '1' : '0';
  assert(gf3_gaussian_action_apply(b.kernel, b.session, limits(), corrupt)
             .status == Gf3GaussianActionStatus::Stale);
  assert(b.kernel.next_clause_id == before_id && b.kernel.trail.empty() &&
         b.session.keys().empty());

  Gf3GaussianConsequenceParameters small;
  small.maximum_identity_bytes = 1;
  assert(gf3_gaussian_action_offer(b.kernel, b.session, limits(small)).status ==
         Gf3GaussianActionStatus::ResourceExhausted);
  assert(b.kernel.next_clause_id == before_id && b.kernel.trail.empty());

  Gf3GaussianConsequenceParameters small_session;
  small_session.maximum_session_identity_bytes = offered.offers[0].size() - 1;
  const auto session_limited = gf3_gaussian_action_offer(
      b.kernel, b.session, limits(small_session));
  assert(session_limited.status ==
             Gf3GaussianActionStatus::ResourceExhausted &&
         session_limited.candidates == 1 && !session_limited.complete &&
         session_limited.offers.empty());
  assert(b.kernel.next_clause_id == before_id && b.kernel.trail.empty());

  Kernel other = install();
  other.structural_provenance = b.records;
  Gf3GaussianActionSession foreign;
  // Bind foreign to the first kernel through a successful application.
  assert(gf3_gaussian_action_apply(b.kernel, foreign, limits(),
                                   offered.offers[0]).status ==
         Gf3GaussianActionStatus::Applied);
  assert(gf3_gaussian_action_offer(other, foreign, limits()).status ==
         Gf3GaussianActionStatus::Stale);
}

void public_wrapper_applies_with_dsr() {
  Bound b;
  const auto offered = gf3_gaussian_action_offer(b.kernel, b.session, limits());
  assert(offered.offers.size() == 1);
  char path[] = "/tmp/tactix-gf3-gaussian-public-XXXXXX";
  const int descriptor = mkstemp(path);
  assert(descriptor >= 0 && !close(descriptor));
  std::remove(path);
  Proof proof;
  assert(proof.open(path, ProofFormat::DsrAscii));
  proof.bind_variable_map(&b.kernel.internal_to_external);
  proof.bind_external_variable_map(&b.kernel.external_to_internal);
  b.kernel.proof = &proof;
  const auto applied = gf3_gaussian_action_apply(
      b.kernel, b.session, limits(), offered.offers.front());
  assert(applied.status == Gf3GaussianActionStatus::Applied &&
         applied.result == ApplyResult::Ok);
  proof.flush();
  b.kernel.proof = nullptr;
  FILE *stream = std::fopen(path, "rb");
  assert(stream && !std::fseek(stream, 0, SEEK_END) && std::ftell(stream) > 0);
  std::fclose(stream);
  std::remove(path);
}

void typed_replay_and_random_reach() {
  Bound source;
  const auto offered =
      gf3_gaussian_action_offer(source.kernel, source.session, limits());
  OnlineInvocation step;
  step.id = 26;
  step.rule = "Gf3GaussianConsequence";
  step.parameters = Gf3GaussianConsequenceParameters{};
  step.granularity = InvocationGranularity::OneInstance;
  step.gf3_gaussian_instances = offered.offers;
  step.work_unit = WorkUnit::Applications;
  step.budget = WorkLimit::bounded(1);
  OnlineStrategyProgram program{{step}, {}};
  const std::string text = serialize_online_strategy_program(program);
  OnlineStrategyProgram replay;
  std::string error;
  assert(deserialize_online_strategy_program(text, replay, &error));
  assert(replay.sequence[0].gf3_gaussian_instances == offered.offers);

  Bound executed;
  OnlineStrategyExecutor executor;
  assert(executor.configure(replay, 71, ReconfigureMode::ResetRuntime, &error));
  const auto result = executor.advance(executed.kernel);
  assert(result.status == InvocationStatus::Completed);
  assert(result.applications == 1 && result.instance == 0);
  assert(executed.kernel.stats.gf3_gaussian_applied == 1);

  bool reached = false;
  for (uint64_t seed = 1; seed <= 32 && !reached; ++seed) {
    Bound candidate;
    RandomControllerConfig config;
    config.parameters.rules = {"Gf3GaussianConsequence"};
    config.gf3_gaussian_action_producer.enabled = true;
    RandomControllerSelector selector(seed);
    ActionRequest request;
    auto observation = observe_kernel(candidate.kernel);
    assert(selector.choose(candidate.kernel, observation, config, request,
                           error));
    if (request.type == ActionType::Rule) {
      reached = true;
      assert(request.invocation.rule == "Gf3GaussianConsequence");
      assert(request.invocation.gf3_gaussian_instances.size() == 1);
      assert(request.producer_effort.size() == 1);
      const auto &effort = request.producer_effort.front();
      const auto &raw = offered.live.effort;
      const uint64_t expected =
          raw.capture_work + raw.bridge_work + raw.certificate_work +
          raw.validation_work + raw.revalidation_work + raw.journal_work +
          raw.identity_work + offered.session_work;
      assert(effort.complete && effort.candidates == 1 &&
             effort.offers == 1 && effort.preparation_work == expected &&
             effort.total_work == expected);
    }
  }
  assert(reached);
}

void c_v6_replays_the_exact_batch_identity() {
  Bound source;
  source.kernel = install_parsed();
  source.kernel.structural_provenance = source.records;
  source.kernel.gf3_gaussian_action_session = &source.session;
  const auto source_offer =
      gf3_gaussian_action_offer(source.kernel, source.session, limits());
  assert(source_offer.status == Gf3GaussianActionStatus::Applied &&
         source_offer.offers.size() == 1);
  const std::string identity = source_offer.offers.front();
  tactix_c_handle *solver = tactix_init();
  FILE *records = std::tmpfile();
  assert(records && tactix_bind_structural_provenance(solver, records) == 0);
  for (const auto &clause : chained_units().clauses) {
    for (int32_t literal : clause) tactix_add(solver, literal);
    tactix_add(solver, 0);
  }
  const Gf3GaussianConsequenceParameters p;
  tactix_online_step_v6 step{};
  auto &base = step.base.base.base.base.base;
  base.step_id = 26;
  base.rule_id = 37;
  base.parameters.kind = TACTIX_ONLINE_PARAMETERS_GF3_GAUSSIAN_CONSEQUENCE;
  auto &c = base.parameters.value.gf3_gaussian_consequence;
  c = {p.maximum_source_clauses, p.maximum_source_literals,
       p.maximum_domains, p.maximum_equations, p.maximum_equation_arity,
       p.maximum_tuples_per_equation, p.maximum_bridge_work,
       p.maximum_bridge_source_clauses, p.maximum_bridge_source_literals,
       p.maximum_bridge_domains, p.maximum_bridge_equations,
       p.maximum_bridge_terms, p.maximum_authentication_references,
       p.maximum_references_per_equation, p.maximum_gaussian_work,
       p.maximum_fill_entries, p.maximum_intermediate_width,
       p.maximum_provenance_terms, p.maximum_derivations,
       p.maximum_retained_plan_bytes, p.maximum_certificate_work,
       p.maximum_events, p.maximum_event_literals, p.maximum_active_clauses,
       p.maximum_retained_source_references, p.maximum_event_width,
       p.maximum_branch_domains, p.maximum_consequences,
       p.maximum_capture_work, p.maximum_revalidation_work,
       p.maximum_proof_additions, p.maximum_proof_literal_occurrences,
       p.maximum_journal_bytes, p.maximum_identity_bytes,
       p.maximum_external_extent, p.maximum_session_keys,
       p.maximum_session_work, p.maximum_session_identity_bytes};
  base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base.budget = 1;
  const tactix_online_gf3_gaussian_instance selected{identity.c_str()};
  step.gf3_gaussian_instances = &selected;
  step.gf3_gaussian_instance_count = 1;
  const tactix_online_program_v6 program{
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  const uint64_t maximum_identity_bytes = c.maximum_identity_bytes;
  c.maximum_identity_bytes = identity.size() - 1;
  assert(tactix_online_configure_v6(solver, &program, 87,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  c.maximum_identity_bytes = maximum_identity_bytes;
  assert(tactix_online_configure_v6(solver, &program, 88,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_online_result result{};
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.rule_id == 37 && result.status == TACTIX_ONLINE_COMPLETED);
  tactix_release(solver);
  std::fclose(records);
}

} // namespace

int main() {
  singular_batch_and_atomic_publication();
  corruption_rebinding_and_limits_decline_without_mutation();
  public_wrapper_applies_with_dsr();
  typed_replay_and_random_reach();
  c_v6_replays_the_exact_batch_identity();
  std::puts("test_gf3_gaussian_public: passed");
}
