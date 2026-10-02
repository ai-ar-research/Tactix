#undef NDEBUG
#include "policies/default_policy.hpp"
#include "strategy/feedback_trace.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

using namespace tactix;
using namespace tactix::strategy;

static std::string fixture(Kernel &k, unsigned scenario) {
  std::vector<std::vector<int>> clauses;
  if (scenario == 0 || scenario >= 3) {
    kernel_defer_cold_state(k);
    kernel_reserve_vars(k, scenario >= 3 ? 22 : 20);
    for (unsigned pigeon = 0; pigeon < 5; ++pigeon) {
      clauses.push_back({int(4 * pigeon + 1), int(4 * pigeon + 2),
                         int(4 * pigeon + 3), int(4 * pigeon + 4)});
      for (unsigned next = pigeon + 1; next < 5; ++next)
        for (unsigned hole = 0; hole < 4; ++hole)
          clauses.push_back({-int(4 * pigeon + hole + 1), -int(4 * next + hole + 1)});
    }
    if (scenario >= 3) {
      clauses.push_back({21, 22});
      clauses.push_back({21, -22});
    }
  } else if (scenario == 1) {
    kernel_defer_cold_state(k);
    kernel_reserve_vars(k, 2);
    clauses = {{1, 2}};
  } else {
    kernel_reserve_vars(k, 7);
    clauses = {{1, 2, 3}, {4, 6}, {-4, 7}};
  }
  std::ostringstream cnf;
  cnf << "p cnf " << k.nvars << ' ' << clauses.size() << '\n';
  for (const auto &clause : clauses) {
    std::vector<Lit> literals;
    for (int literal : clause) { cnf << literal << ' '; literals.push_back(internalize(literal)); }
    cnf << "0\n";
    if (literals.size() == 2)
      kernel_add_binary_clause(k, literals[0], literals[1], ++k.next_clause_id);
    else kernel_add_large_clause(k, literals.data(), literals.size(), false, 0, ++k.next_clause_id);
  }
  return cnf.str();
}

static DefaultStrategyConfig profile(unsigned scenario) {
  DefaultStrategyConfig config;
  config.inprocess.program.clear();
  config.inprocess.append_unlisted_rules = false;
  config.inprocess.structured_root_stages.clear();
  config.inprocess.structured_root_bounded_backtracking_stages.clear();
  config.inprocess.structured_root_large_domain_stages.clear();
  config.inprocess.initial_fast_elimination = false;
  config.rephase.late_lucky = false;
  if (scenario == 2) {
    OnlineInvocation complete;
    complete.id = 1; complete.rule = "SubstitutionRedundancy";
    SubstitutionRedundancyParameters ulc;
    ulc.producer = SubstitutionRedundancyProducer::Orbitopal;
    ulc.maximum_orbitopal_matrices = 0;
    ulc.complete_unique_literal_amos = true;
    complete.parameters = ulc;
    complete.granularity = InvocationGranularity::CompleteCurrentPass;
    OnlineInvocation added;
    added.id = 2; added.rule = complete.rule;
    added.parameters = SubstitutionRedundancyParameters{};
    added.granularity = InvocationGranularity::OneInstance;
    added.substitution_redundancy_instances = {{{internalize(5), internalize(7)}, {}, {}}};
    OnlineStrategyProgram program;
    program.sequence = {complete, added};
    config.inprocess.structured_root_stages = {{program, BudgetExhaustionPolicy::Continue}};
    config.inprocess.structured_root_before_lucky = true;
  } else if (scenario >= 3) {
    OnlineInvocation fixing;
    fixing.id = 1; fixing.rule = "SubstitutionRedundancy";
    fixing.parameters = SubstitutionRedundancyParameters{};
    fixing.granularity = InvocationGranularity::OneInstance;
    // The two disconnected binaries entail 21. Root fixing makes the
    // preserved residual handoff effective before the hard search residual.
    fixing.substitution_redundancy_instances = {{{internalize(21)}, {}, {}}};
    fixing.post_application = PostApplication::Propagate;
    OnlineStrategyProgram program;
    program.sequence = {fixing};
    config.inprocess.structured_root_stages = {{program, BudgetExhaustionPolicy::Continue}};
    config.inprocess.structured_root_before_lucky = true;
  }
  return config;
}

int main(int argc, char **argv) {
  char temporary[] = "/tmp/tactix-startup-support-XXXXXX";
  assert(argc == 1 || argc == 2 || (argc == 3 && std::string(argv[2]) == "feedback"));
  if (argc == 1) assert(mkdtemp(temporary));
  const std::string directory = argc > 1 ? argv[1] : temporary;
  for (unsigned scenario = 0; scenario < 5; ++scenario) {
    const std::string stem = directory + "/scenario-" + std::to_string(scenario);
    Kernel k;
    const std::string cnf = fixture(k, scenario);
    std::optional<ScopedHeuristics> embedding;
    if (scenario == 4) embedding.emplace(k);
    std::ofstream(stem + ".cnf") << cnf;
    Proof proof;
    assert(proof.open((stem + ".dsr").c_str(), ProofFormat::DsrAscii));
    proof.bind_variable_map(&k.internal_to_external);
    k.proof = &proof;
    DefaultPolicy policy(profile(scenario));
    strategy::controller::FeedbackTrace feedback;
    if (argc == 3) {
      assert(feedback.write((stem + ".feedback.jsonl").c_str(),
          "startup-scenario=" + std::to_string(scenario) + policy.strategy_config_snapshot()));
      policy.set_feedback(&feedback);
    }
    const int result = policy.solve(k);
    assert(result == (scenario == 1 || scenario == 2 ? 10 : 20));
    assert(bool(k.h) == (scenario == 4) && bool(k.saved_phases) == (scenario == 4));
    assert(!k.pending.valid && !k.conflict_valid);
    assert(scenario == 1 ? k.cold_state_deferred : !k.cold_state_deferred);
    if (scenario == 2) assert(k.stats.bce_deleted && k.stats.sr_post_amo_nonhelper_applied);
    if (scenario >= 3) {
      assert(k.value(internalize(21)) > 0 && k.stats.sr_applications);
      assert(bool(k.stats.residual_handoff_calls) == (scenario == 3));
    }
    if (argc == 3) assert(feedback.terminal() && feedback.error().empty());
    proof.flush();
    kernel_check_invariants(k);
    std::cout << "scenario=" << scenario << " verdict=" << result << ' '
              << strategy::controller::serialize_observation(strategy::controller::observe_kernel(k)) << '\n';
    if (result == 10) {
      kernel_extend_model(k, false);
      std::ofstream model(stem + ".model");
      model << "v ";
      for (Var v = 0; v < k.nvars; ++v)
        model << (k.model_values[make_lit(v, false)] > 0 ? int(v + 1) : -int(v + 1)) << ' ';
      model << "0\n";
    }
    if (argc == 1) {
      assert(unlink((stem + ".cnf").c_str()) == 0);
      assert(unlink((stem + ".dsr").c_str()) == 0);
      if (result == 10) assert(unlink((stem + ".model").c_str()) == 0);
    }
  }
  if (argc == 1) assert(rmdir(temporary) == 0);
}
