#undef NDEBUG
#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#include "strategy/policy_feedback.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

using namespace tactix;
using namespace tactix::strategy::controller;

namespace tactix {
struct DefaultNolWalkTestAccess {
  static uint64_t affine_due(const DefaultPolicy &policy) {
    return policy.affine_service_next_ticks_;
  }
};
} // namespace tactix

static std::string complete_hypergraph(unsigned groups, unsigned active) {
  std::ostringstream body;
  uint64_t clauses = 0;
  for (unsigned vertex = 0; vertex < active; ++vertex) {
    body << 3 * vertex + 1 << ' ' << 3 * vertex + 2 << ' '
         << 3 * vertex + 3 << " 0\n";
    ++clauses;
  }
  for (unsigned a = 0; a < active; ++a)
    for (unsigned b = a + 1; b < active; ++b)
      for (unsigned c = b + 1; c < active; ++c)
        for (unsigned color = 0; color < 3; ++color) {
          body << -int(3 * a + color + 1) << ' '
               << -int(3 * b + color + 1) << ' '
               << -int(3 * c + color + 1) << " 0\n";
          ++clauses;
        }
  std::ostringstream cnf;
  cnf << "p cnf " << 3 * groups << ' ' << clauses << '\n' << body.str();
  return cnf.str();
}

static std::string rank_shape_hypergraph(unsigned groups, unsigned active,
                                         unsigned edge_limit) {
  std::vector<std::array<unsigned, 3>> selected;
  for (unsigned a = 0; a < 7; ++a)
    for (unsigned b = a + 1; b < 7; ++b)
      for (unsigned c = b + 1; c < 7; ++c)
        selected.push_back({a, b, c});
  for (unsigned a = 0; a < active && selected.size() < edge_limit; ++a)
    for (unsigned b = a + 1; b < active && selected.size() < edge_limit; ++b)
      for (unsigned c = b + 1; c < active && selected.size() < edge_limit; ++c) {
        if (c < 7) continue;
        selected.push_back({a, b, c});
      }
  assert(selected.size() == edge_limit);
  std::ostringstream body;
  uint64_t clauses = active;
  for (unsigned vertex = 0; vertex < active; ++vertex)
    body << 3 * vertex + 1 << ' ' << 3 * vertex + 2 << ' '
         << 3 * vertex + 3 << " 0\n";
  for (const auto &edge : selected)
    for (unsigned color = 0; color < 3; ++color) {
          body << -int(3 * edge[0] + color + 1) << ' '
               << -int(3 * edge[1] + color + 1) << ' '
               << -int(3 * edge[2] + color + 1) << " 0\n";
          ++clauses;
    }
  std::ostringstream cnf;
  cnf << "p cnf " << 3 * groups << ' ' << clauses << '\n' << body.str();
  return cnf.str();
}

static Kernel parse(const std::string &text) {
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  return kernel;
}

static DefaultStrategyConfig quiet_config() {
  DefaultStrategyConfig config;
  config.affine_root_closure.enabled = false;
  config.affine_model.enabled = false;
  config.parity_model.enabled = false;
  config.onehot_model.enabled = false;
  config.ramsey_startup.enabled = false;
  config.signed_cardinality_proof.enabled = false;
  config.balanced_signature_model.enabled = false;
  config.exact_cover_model.enabled = false;
  config.independent_set_model.enabled = false;
  config.fermat_model.enabled = false;
  config.cerny_model.enabled = false;
  config.lookup_circuit_model.enabled = false;
  config.simon_model.enabled = false;
  config.stconn_proof.enabled = false;
  config.inprocess.program.clear();
  config.inprocess.append_unlisted_rules = false;
  config.inprocess.structured_root_stages.clear();
  config.inprocess.structured_root_bounded_backtracking_stages.clear();
  config.inprocess.structured_root_large_domain_stages.clear();
  config.inprocess.initial_fast_elimination = false;
  config.rephase.walk_warmup = false;
  config.rephase.late_lucky = false;
  config.mode.force = 1;
  return config;
}

enum class Scenario {
  AffineOrdering,
  NolRestart,
  AuthenticationAllowance,
  AuthenticationExhausted,
  ScheduleOptOut,
  PartialRestart,
  Disabled
};

struct ConsumptionRecord {
  std::vector<uint64_t> candidates;
  std::vector<uint64_t> objectives;
  std::vector<uint64_t> install_intervals;
  int64_t decision_instance = 0;
  bool saw_decision = false;
  uint64_t hints_offered = 0;
  uint64_t install_attempts = 0;
  uint64_t installs = 0;
  uint64_t guided_decisions = 0;
  uint64_t guided_matches = 0;

  bool operator==(const ConsumptionRecord &) const = default;
};

struct ConsumptionCapture final : FeedbackReceiver {
  ConsumptionRecord record;
  uint64_t search_intervals = 0;

  void after_action(const ActionFeedback &action) override {
    if (action.identity == "LocalSearchGuidance") {
      // Every improved publication is consumed at either the initial root or
      // a later full-restart root, never inside a continuing search interval.
      assert(action.before.level == 0 && action.after.level == 0);
      assert(strategy::local_search_candidate_kind(action.instance) ==
             strategy::LocalSearchKind::HypergraphColoring);
      const auto candidate = std::find_if(
          action.after.local_search_candidates.begin(),
          action.after.local_search_candidates.end(), [&](const auto &hint) {
            return hint.id == action.instance;
          });
      assert(candidate != action.after.local_search_candidates.end());
      assert(candidate->objective_exact);
      record.candidates.push_back(action.instance);
      record.objectives.push_back(candidate->objective);
      record.install_intervals.push_back(search_intervals);
    } else if (action.identity == "Decide" && !record.saw_decision) {
      assert(!record.candidates.empty() && action.after.level == 1);
      record.decision_instance = static_cast<int64_t>(action.instance);
      record.saw_decision = true;
    }
  }

  void search_interval(const SearchIntervalFeedback &) override {
    ++search_intervals;
  }
};

struct StructuredConsumptionCapture final : FeedbackReceiver {
  bool saw_structured_action = false;
  bool saw_guidance = false;
  uint64_t structured_sequence = 0;
  uint64_t guidance_sequence = 0;
  uint64_t root_population_at_guidance = 0;
  uint64_t slices_at_guidance = 0;
  DefaultPolicy *policy = nullptr;
  bool search_started = false;

  void after_action(const ActionFeedback &action) override {
    if (action.identity == "SubstitutionRedundancy") {
      // The first noL slice must not have run on the pre-symmetry root.
      assert(!saw_guidance);
      saw_structured_action = true;
      structured_sequence = action.sequence;
    } else if (action.identity == "LocalSearchGuidance" &&
               strategy::local_search_candidate_kind(action.instance) ==
                   strategy::LocalSearchKind::HypergraphColoring) {
      assert(saw_structured_action);
      assert(!search_started);
      assert(action.before.level == 0 && action.after.level == 0);
      assert(policy && policy->nol_walk_diagnostics().slices == 1);
      const auto candidate = std::find_if(
          action.after.local_search_candidates.begin(),
          action.after.local_search_candidates.end(), [&](const auto &hint) {
            return hint.id == action.instance;
          });
      assert(candidate != action.after.local_search_candidates.end());
      assert(candidate->formula_root_assignments ==
             action.after.cumulative.root_assignments);
      saw_guidance = true;
      guidance_sequence = action.sequence;
      root_population_at_guidance = action.after.root_population;
      slices_at_guidance = policy->nol_walk_diagnostics().slices;
    }
  }

  void search_interval(const SearchIntervalFeedback &) override {
    search_started = true;
  }
};

struct StopAfterStructuredGuidance final : BranchingPolicy {
  volatile sig_atomic_t &stop;
  unsigned picks = 0;
  explicit StopAfterStructuredGuidance(volatile sig_atomic_t &signal)
      : stop(signal) {}
  const char *name() const override { return "structured-noL-order-test"; }
  Lit pick(Kernel &kernel) override {
    assert(!picks++);
    stop = 1;
    for (Var variable = 0; variable < kernel.nvars; ++variable)
      if (!kernel.value(make_lit(variable, false)) &&
          !kernel_var_retired(kernel, variable))
        return make_lit(variable, false);
    return INVALID_LIT;
  }
};

struct Capture final : FeedbackReceiver {
  DefaultPolicy *policy = nullptr;
  Scenario scenario = Scenario::Disabled;
  std::vector<uint64_t> restart_levels;
  std::vector<SearchIntervalFeedback> intervals;
  uint64_t slices_before_restart = 0;
  uint64_t affine_before_nol = 0;
  uint64_t initial_affine_before_nol = 0;
  bool awaiting_affine = false;

  Capture(DefaultPolicy *owner, Scenario kind)
      : policy(owner), scenario(kind) {}

  void after_action(const ActionFeedback &action) override {
    if ((action.identity == "AffineMatrixInitialize" ||
         action.identity == "AffineMatrixSearch") &&
        policy->nol_walk_diagnostics().slices == 0) {
      assert(policy->nol_walk_diagnostics().authentication_attempts == 1);
      ++initial_affine_before_nol;
    }
    if (action.identity == "Restart") {
      restart_levels.push_back(action.instance);
      slices_before_restart = policy->nol_walk_diagnostics().slices;
      awaiting_affine = scenario == Scenario::AffineOrdering;
      if (scenario == Scenario::AffineOrdering || scenario == Scenario::NolRestart)
        assert(action.instance == 0);
    }
    if (awaiting_affine &&
        (action.identity == "AffineMatrixInitialize" ||
         action.identity == "AffineMatrixSearch")) {
      // The callback runs after the affine action and before noL is called.
      assert(policy->nol_walk_diagnostics().slices == slices_before_restart);
      ++affine_before_nol;
      awaiting_affine = false;
    }
  }

  void search_interval(const SearchIntervalFeedback &interval) override {
    intervals.push_back(interval);
    if (interval.stop == StopReason::Restart)
      // Restart closes the search interval before either root-only service.
      assert(policy->nol_walk_diagnostics().slices == slices_before_restart);
  }
};

static Var extreme_unassigned(const Kernel &kernel, bool high) {
  assert(kernel.h);
  Var selected = INVALID_VAR;
  for (Var variable = 0; variable < kernel.nvars; ++variable) {
    if (kernel.values[make_lit(variable, false)] != 0 ||
        kernel_var_retired(kernel, variable))
      continue;
    if (selected == INVALID_VAR ||
        (high ? kernel.h->links[variable].stamp > kernel.h->links[selected].stamp
              : kernel.h->links[variable].stamp < kernel.h->links[selected].stamp))
      selected = variable;
  }
  assert(selected != INVALID_VAR);
  return selected;
}

struct ControlledBranching final : BranchingPolicy {
  DefaultPolicy &policy;
  Capture &capture;
  volatile sig_atomic_t &stop;
  Scenario scenario;
  unsigned picks = 0;
  uint32_t partial_keep = 0;

  ControlledBranching(DefaultPolicy &owner, Capture &receiver,
                      volatile sig_atomic_t &signal, Scenario kind)
      : policy(owner), capture(receiver), stop(signal), scenario(kind) {}

  const char *name() const override { return "noL-restart-loop-test"; }

  Lit pick(Kernel &kernel) override {
    const auto slices = policy.nol_walk_diagnostics().slices;
    if (scenario == Scenario::AffineOrdering) {
      assert(slices == 0);
      assert(kernel.stats.nol_hypergraph_model_calls == slices);
      if (picks) {
        assert(kernel.level == 0);
        assert(capture.restart_levels.size() == picks);
        assert(capture.affine_before_nol >= picks);
        stop = 1;
      } else {
        assert(capture.initial_affine_before_nol > 0);
        const uint64_t due = std::max(
            kernel.stats.search_ticks,
            DefaultNolWalkTestAccess::affine_due(policy));
        kernel.stats.search_ticks = std::max(kernel.stats.search_ticks, due);
      }
      ++picks;
      return make_lit(extreme_unassigned(kernel, true), false);
    }

    if (scenario == Scenario::NolRestart) {
      // The first call follows initial affine service at root; later calls are
      // after full-root search intervals.
      assert(slices == picks + 1);
      assert(kernel.stats.nol_hypergraph_model_calls == slices);
      if (picks) {
        assert(kernel.level == 0);
        assert(capture.restart_levels.size() == picks);
        assert(kernel.stats.nol_hypergraph_model_recognized == 1);
      }
      if (picks == 2) {
        stop = 1;
      } else {
        const uint64_t due = policy.nol_walk_diagnostics().next_ticks;
        kernel.stats.search_ticks = std::max(kernel.stats.search_ticks, due);
      }
      ++picks;
      return make_lit(extreme_unassigned(kernel, true), false);
    }

    if (scenario == Scenario::AuthenticationAllowance) {
      assert(picks == 0 && slices == 1);
      assert(policy.nol_walk_diagnostics().authentication_attempts == 1);
      assert(policy.nol_walk_diagnostics().authentication_successes == 1);
      assert(policy.nol_walk_diagnostics().authentication_exhausted == 0);
      assert(policy.nol_walk_diagnostics().authentication_work >
             policy.strategy_config().nol_walk.work_quantum);
      assert(kernel.stats.nol_hypergraph_model_calls == 1);
      assert(kernel.stats.nol_hypergraph_model_recognized == 1);
      stop = 1;
      ++picks;
      return make_lit(extreme_unassigned(kernel, true), false);
    }

    if (scenario == Scenario::AuthenticationExhausted) {
      assert(picks == 0 && slices == 0);
      assert(policy.nol_walk_diagnostics().authentication_attempts == 1);
      assert(policy.nol_walk_diagnostics().authentication_successes == 0);
      assert(policy.nol_walk_diagnostics().authentication_exhausted == 1);
      assert(kernel.stats.nol_hypergraph_model_calls == 0);
      assert(kernel.stats.nol_hypergraph_model_recognized == 0);
      stop = 1;
      ++picks;
      return make_lit(extreme_unassigned(kernel, true), false);
    }

    if (scenario == Scenario::ScheduleOptOut) {
      assert(picks == 0 && slices == 0);
      assert(policy.nol_walk_diagnostics().authentication_attempts == 1);
      assert(policy.nol_walk_diagnostics().authentication_no_structure == 1);
      assert(policy.nol_walk_diagnostics().delay_ticks ==
             policy.strategy_config().nol_walk.initial_tick_interval);
      assert(policy.nol_walk_diagnostics().next_ticks != 0);
      assert(kernel.stats.nol_hypergraph_model_calls == 0);
      stop = 1;
      ++picks;
      return make_lit(extreme_unassigned(kernel, true), false);
    }

    if (scenario == Scenario::Disabled) {
      assert(slices == 0);
      assert(kernel.stats.nol_hypergraph_model_calls == 0);
      stop = 1;
      ++picks;
      return make_lit(extreme_unassigned(kernel, true), false);
    }

    assert(slices == 1);
    assert(kernel.stats.nol_hypergraph_model_calls == 1);

    if (picks == 1) {
      // Arm the real ordinary restart clock. Both focused adaptive glue and
      // stable reluctant modes are made due, so the test is mode-agnostic.
      policy.restart.next_conflicts = kernel.stats.conflicts;
      policy.reluctant.trigger = true;
      for (auto &average : policy.averages) {
        average.initialized = true;
        average.fast_glue.value = 10;
        average.slow_glue.value = 1;
      }
    } else if (picks == 2) {
      assert(partial_keep > 0);
      assert(kernel.level == partial_keep);
      assert(kernel.stats.restarts_reused > 0);
      assert(capture.restart_levels.size() == 1);
      assert(capture.restart_levels.front() == partial_keep);
      stop = 1;
    }
    const bool high = picks == 0;
    ++picks;
    return make_lit(extreme_unassigned(kernel, high), false);
  }

  Lit pick_for_restart_reuse(Kernel &kernel) override {
    assert(scenario == Scenario::PartialRestart && kernel.level > 1);
    Var selected = INVALID_VAR;
    for (Var variable = 0; variable < kernel.nvars; ++variable) {
      if (kernel.values[make_lit(variable, false)] != 0 ||
          kernel_var_retired(kernel, variable))
        continue;
      const uint32_t keep = heuristics_restart_reuse_level_for(kernel, variable);
      if (keep && keep < kernel.level) {
        selected = variable;
        partial_keep = keep;
        break;
      }
    }
    assert(selected != INVALID_VAR && partial_keep > 0 && partial_keep < kernel.level);
    return make_lit(selected, false);
  }
};

static std::string affine_search_formula() {
  constexpr unsigned pigeons = 4, holes = 5;
  uint32_t variables = pigeons * holes;
  std::vector<std::vector<int32_t>> clauses;
  const auto cell = [](unsigned pigeon, unsigned hole) {
    return int32_t(1 + pigeon * holes + hole);
  };
  for (unsigned pigeon = 0; pigeon < pigeons; ++pigeon) {
    std::vector<int32_t> cover;
    for (unsigned hole = 0; hole < holes; ++hole)
      cover.push_back(cell(pigeon, hole));
    clauses.push_back(std::move(cover));
  }
  for (unsigned hole = 0; hole < holes; ++hole)
    for (unsigned first = 0; first < pigeons; ++first)
      for (unsigned second = first + 1; second < pigeons; ++second)
        clauses.push_back({-cell(first, hole), -cell(second, hole)});
  for (const auto &scope : {std::vector<int32_t>{1, 7, 13},
                            std::vector<int32_t>{7, 13, 19, 20}})
    for (auto clause : affine_semantic_extension::clausal_form(
             scope, scope.front() == 7))
      clauses.push_back(std::move(clause));
  clauses.push_back({-1, 19});
  const int32_t old_variables = variables;
  variables += 6;
  for (int32_t gate : {old_variables + 1, -(old_variables + 2),
                       old_variables + 5, -(old_variables + 6)})
    for (int32_t a : {old_variables + 3, -(old_variables + 3)})
      for (int32_t b : {old_variables + 4, -(old_variables + 4)})
        clauses.push_back({gate, a, b});
  std::ostringstream cnf;
  cnf << "p cnf " << variables << ' ' << clauses.size() << '\n';
  for (const auto &clause : clauses) {
    for (int32_t literal : clause) cnf << literal << ' ';
    cnf << "0\n";
  }
  return cnf.str();
}

static void schedule_opt_out_case() {
  Kernel kernel = parse(affine_search_formula());
  NolHypergraphWalkSession session;
  kernel.nol_hypergraph_walk_session = &session;
  ScopedHeuristics heuristics(kernel);
  auto config = quiet_config();
  config.nol_walk.enabled = true;
  config.nol_walk.initial_tick_interval = 37;
  config.nol_walk.maximum_delay_ticks = 148;
  DefaultPolicy policy(config);
  policy.reset_schedule_per_solve = false;
  volatile sig_atomic_t stop = 0;
  Capture capture{&policy, Scenario::ScheduleOptOut};
  ControlledBranching branching{policy, capture, stop, Scenario::ScheduleOptOut};
  policy.branching = &branching;
  policy.set_feedback(&capture);
  assert(policy.solve(kernel, &stop) == 0);
  assert(branching.picks == 1);
  assert(policy.nol_walk_diagnostics().delay_ticks == 37);
  assert(policy.nol_walk_diagnostics().next_ticks == 37);
  assert(policy.nol_walk_diagnostics().slices == 0);
  assert(capture.restart_levels.empty());
  kernel.nol_hypergraph_walk_session = nullptr;
}

static void affine_ordering_case() {
  Kernel kernel = parse(affine_search_formula());
  NolHypergraphWalkSession nol_session;
  AffineMatrixSearchSession affine_session;
  kernel.nol_hypergraph_walk_session = &nol_session;
  kernel.affine_matrix_search_session = &affine_session;
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  char proof_path[] = "/tmp/tactix-default-nol-loop-XXXXXX";
  const int proof_fd = mkstemp(proof_path);
  assert(proof_fd >= 0);
  close(proof_fd);
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;

  ScopedHeuristics heuristics(kernel);
  auto config = quiet_config();
  config.nol_walk.enabled = true;
  config.nol_walk.work_quantum = 8000;
  config.nol_walk.initial_tick_interval = UINT64_C(1) << 60;
  config.nol_walk.maximum_delay_ticks = UINT64_C(1) << 60;
  config.affine_search.enabled = true;
  config.affine_search.maximum_retained_bytes = UINT64_C(1000000000);
  config.affine_search.maximum_initialization_work = UINT64_C(100000000);
  config.affine_search.maximum_service_work = 10000000;
  DefaultPolicy policy(config);
  volatile sig_atomic_t stop = 0;
  Capture capture{&policy, Scenario::AffineOrdering};
  ControlledBranching branching{policy, capture, stop, Scenario::AffineOrdering};
  policy.branching = &branching;
  policy.set_feedback(&capture);
  const int verdict = policy.solve(kernel, &stop);
  if (verdict != 0)
    std::fprintf(stderr, "affine ordering verdict=%d picks=%u slices=%llu restarts=%zu affine=%llu equations=%llu\n",
        verdict, branching.picks,
        (unsigned long long)policy.nol_walk_diagnostics().slices,
        capture.restart_levels.size(),
        (unsigned long long)capture.affine_before_nol,
        (unsigned long long)policy.affine_search_diagnostics().recognized_equations);
  assert(verdict == 0);

  assert(branching.picks == 2);
  assert(policy.affine_search_diagnostics().recognized_equations == 2);
  assert(policy.nol_walk_diagnostics().slices == 0);
  assert(kernel.stats.nol_hypergraph_model_calls == 0);
  assert(kernel.stats.nol_hypergraph_model_recognized == 0);
  assert(policy.nol_walk_diagnostics().authentication_attempts == 1);
  assert(policy.nol_walk_diagnostics().authentication_no_structure == 1);
  assert(capture.restart_levels == std::vector<uint64_t>({0}));
  assert(capture.affine_before_nol >= 1);
  assert(capture.intervals.size() == 2);
  assert(capture.intervals[0].start == IntervalStart::ResumeSearch);
  assert(capture.intervals[0].stop == StopReason::Restart);
  assert(capture.intervals[0].after.level == 0);
  assert(capture.intervals[1].start == IntervalStart::NativeContinuation);
  assert(capture.intervals[1].stop == StopReason::Interrupted);

  affine_session.disable(kernel);
  kernel.proof = nullptr;
  kernel.structural_provenance = nullptr;
  kernel.affine_matrix_search_session = nullptr;
  kernel.nol_hypergraph_walk_session = nullptr;
  std::fclose(records);
  assert(std::remove(proof_path) == 0);
}

static void nol_restart_case() {
  Kernel kernel = parse(complete_hypergraph(10, 7));
  NolHypergraphWalkSession session;
  kernel.nol_hypergraph_walk_session = &session;
  ScopedHeuristics heuristics(kernel);
  auto config = quiet_config();
  config.nol_walk.enabled = true;
  config.nol_walk.work_quantum = 8000;
  config.nol_walk.initial_tick_interval = UINT64_C(1) << 60;
  config.nol_walk.maximum_delay_ticks = UINT64_C(1) << 60;
  DefaultPolicy policy(config);
  volatile sig_atomic_t stop = 0;
  Capture capture{&policy, Scenario::NolRestart};
  ControlledBranching branching{policy, capture, stop, Scenario::NolRestart};
  policy.branching = &branching;
  policy.set_feedback(&capture);
  assert(policy.solve(kernel, &stop) == 0);

  assert(branching.picks == 3);
  assert(policy.nol_walk_diagnostics().slices == 3);
  assert(kernel.stats.nol_hypergraph_model_calls == 3);
  assert(kernel.stats.nol_hypergraph_model_recognized == 1);
  assert(policy.nol_walk_diagnostics().authentication_attempts == 1);
  assert(policy.nol_walk_diagnostics().authentication_successes == 1);
  assert(capture.restart_levels == std::vector<uint64_t>({0, 0}));
  assert(capture.intervals.size() == 3);
  assert(capture.intervals[0].start == IntervalStart::ResumeSearch);
  assert(capture.intervals[0].stop == StopReason::Restart);
  assert(capture.intervals[0].after.level == 0);
  assert(capture.intervals[1].start == IntervalStart::NativeContinuation);
  assert(capture.intervals[1].stop == StopReason::Restart);
  assert(capture.intervals[1].after.level == 0);
  assert(capture.intervals[2].start == IntervalStart::NativeContinuation);
  assert(capture.intervals[2].stop == StopReason::Interrupted);
  kernel.nol_hypergraph_walk_session = nullptr;
}

static void authentication_allowance_case() {
  // Rank401's retained source shape needs more than the recurring 100k Walk
  // dose to authenticate. The separate initialization allowance captures all
  // 473 triples / 363 active vertices / 2486 edges once, before the first
  // trajectory slice and before preprocessing can change the clause image.
  Kernel kernel = parse(rank_shape_hypergraph(473, 363, 2486));
  NolHypergraphWalkSession session;
  kernel.nol_hypergraph_walk_session = &session;
  ScopedHeuristics heuristics(kernel);
  auto config = quiet_config();
  config.nol_walk.enabled = true;
  config.nol_walk.maximum_authentication_work = 10'000'000;
  config.nol_walk.work_quantum = 100'000;
  config.nol_walk.initial_tick_interval = UINT64_C(1) << 60;
  config.nol_walk.maximum_delay_ticks = UINT64_C(1) << 60;
  DefaultPolicy policy(config);
  volatile sig_atomic_t stop = 0;
  Capture capture{&policy, Scenario::AuthenticationAllowance};
  ControlledBranching branching{
      policy, capture, stop, Scenario::AuthenticationAllowance};
  policy.branching = &branching;
  policy.set_feedback(&capture);
  assert(policy.solve(kernel, &stop) == 0);
  assert(branching.picks == 1);
  assert(session.authenticated(kernel));
  assert(session.steps() == 0 && session.trajectories() == 0);
  assert(policy.nol_walk_diagnostics().slices == 1);
  assert(policy.nol_walk_diagnostics().pending_slices == 1);
  assert(kernel.stats.nol_hypergraph_model_vertices == 363);
  assert(kernel.stats.nol_hypergraph_model_edges == 2486);
  assert(capture.restart_levels.empty());
  kernel.nol_hypergraph_walk_session = nullptr;
}

static void authentication_exhaustion_case() {
  const auto input = rank_shape_hypergraph(473, 363, 2486);
  Kernel measured = parse(input);
  NolHypergraphWalkSession measured_session;
  measured.nol_hypergraph_walk_session = &measured_session;
  const auto measurement = nol_hypergraph_model_authenticate(measured);
  assert(measurement.status == NolHypergraphAuthenticationStatus::Authenticated);
  assert(measurement.work > 0);

  Kernel kernel = parse(input);
  NolHypergraphWalkSession session;
  kernel.nol_hypergraph_walk_session = &session;
  ScopedHeuristics heuristics(kernel);
  auto config = quiet_config();
  config.nol_walk.enabled = true;
  config.nol_walk.maximum_authentication_work = measurement.work - 1;
  // This deliberately exceeds authentication cost: if policy accidentally
  // invoked the legacy slice fallback, it would recognize the source.
  config.nol_walk.work_quantum = 10'000'000;
  config.nol_walk.initial_tick_interval = 1;
  config.nol_walk.maximum_delay_ticks = 64;
  DefaultPolicy policy(config);
  volatile sig_atomic_t stop = 0;
  Capture capture{&policy, Scenario::AuthenticationExhausted};
  ControlledBranching branching{
      policy, capture, stop, Scenario::AuthenticationExhausted};
  policy.branching = &branching;
  policy.set_feedback(&capture);
  assert(policy.solve(kernel, &stop) == 0);
  assert(branching.picks == 1);
  assert(!session.authenticated(kernel));
  assert(policy.nol_walk_diagnostics().slices == 0);
  assert(kernel.stats.nol_hypergraph_model_calls == 0);
  assert(capture.restart_levels.empty());
  kernel.nol_hypergraph_walk_session = nullptr;
}

static ConsumptionRecord phase_consumption_case() {
  Kernel kernel = parse(complete_hypergraph(7, 7));
  NolHypergraphWalkSession session;
  kernel.nol_hypergraph_walk_session = &session;
  ScopedHeuristics heuristics(kernel);
  kernel.h->stable = true;
  auto config = quiet_config();
  config.nol_walk.enabled = true;
  config.nol_walk.work_quantum = 8000;
  config.nol_walk.initial_tick_interval = UINT64_C(1) << 60;
  config.nol_walk.maximum_delay_ticks = UINT64_C(1) << 60;
  config.mode.force = 2; // stable CDCL consumes target phases
  DefaultPolicy policy(config);
  ConsumptionCapture capture;
  policy.set_feedback(&capture);
  assert(policy.solve(kernel) == 20);

  assert(!capture.record.candidates.empty());
  assert(capture.record.objectives.size() == capture.record.candidates.size());
  assert(capture.record.install_intervals.size() ==
         capture.record.candidates.size());
  assert(capture.record.install_intervals.front() == 0);
  assert(std::all_of(capture.record.objectives.begin(),
                     capture.record.objectives.end(),
                     [](uint64_t value) { return value < UINT64_MAX; }));
  assert(capture.record.saw_decision);
  assert(policy.nol_walk_diagnostics().slices >= 1);
  assert(policy.nol_walk_diagnostics().useful_hints ==
         capture.record.candidates.size());
  assert(policy.nol_walk_diagnostics().phase_install_attempts ==
         capture.record.candidates.size());
  assert(policy.nol_walk_diagnostics().phase_installs ==
         capture.record.candidates.size());
  assert(kernel.stats.local_search_hints_offered >=
         capture.record.candidates.size());
  assert(kernel.stats.local_search_target_install_attempts ==
         capture.record.candidates.size());
  assert(kernel.stats.local_search_target_installs ==
         capture.record.candidates.size());
  assert(kernel.stats.local_search_hints_applied ==
         capture.record.candidates.size());
  assert(kernel.stats.local_search_target_decisions >= 1);
  assert(kernel.stats.local_search_target_decision_matches >= 1);
  assert(kernel.stats.local_search_target_decision_overrides == 0);
  assert(capture.search_intervals >= 1);

  capture.record.hints_offered = kernel.stats.local_search_hints_offered;
  capture.record.install_attempts =
      kernel.stats.local_search_target_install_attempts;
  capture.record.installs = kernel.stats.local_search_target_installs;
  capture.record.guided_decisions = kernel.stats.local_search_target_decisions;
  capture.record.guided_matches =
      kernel.stats.local_search_target_decision_matches;
  kernel.nol_hypergraph_walk_session = nullptr;
  return capture.record;
}

static void structured_root_before_initial_nol_case() {
  Kernel kernel = parse(complete_hypergraph(7, 7));
  NolHypergraphWalkSession session;
  kernel.nol_hypergraph_walk_session = &session;
  ScopedHeuristics heuristics(kernel);
  kernel.h->stable = true;
  auto config = quiet_config();
  strategy::OnlineInvocation symmetry;
  bool found_symmetry = false;
  for (const auto &stage :
       DefaultStrategyConfig{}.inprocess.structured_root_stages) {
    for (const auto &step : stage.program.sequence)
      if (const auto *parameters =
              std::get_if<SubstitutionRedundancyParameters>(&step.parameters);
          parameters &&
          parameters->producer ==
              SubstitutionRedundancyProducer::GraphSymmetry) {
        symmetry = step;
        found_symmetry = true;
        break;
      }
    if (found_symmetry) break;
  }
  assert(found_symmetry);
  strategy::OnlineStrategyProgram program;
  program.sequence = {symmetry};
  config.inprocess.structured_root_stages = {
      {program, strategy::BudgetExhaustionPolicy::Continue}};
  config.nol_walk.enabled = true;
  config.nol_walk.work_quantum = 8000;
  config.nol_walk.initial_tick_interval = UINT64_C(1) << 60;
  config.nol_walk.maximum_delay_ticks = UINT64_C(1) << 60;
  config.mode.force = 2;
  DefaultPolicy policy(config);
  volatile sig_atomic_t stop = 0;
  StopAfterStructuredGuidance branching(stop);
  policy.branching = &branching;
  StructuredConsumptionCapture capture;
  capture.policy = &policy;
  policy.set_feedback(&capture);
  assert(policy.solve(kernel, &stop) == 0);
  assert(branching.picks == 1);
  assert(capture.saw_structured_action && capture.saw_guidance);
  assert(capture.structured_sequence < capture.guidance_sequence);
  assert(capture.slices_at_guidance == 1);
  assert(capture.root_population_at_guidance > 0);
  assert(policy.nol_walk_diagnostics().authentication_attempts == 1);
  assert(kernel.stats.nol_hypergraph_model_recognized == 1);
  assert(kernel.stats.nol_hypergraph_model_calls >= 1);
  const auto &hint = kernel.local_search_hints[
      size_t(strategy::LocalSearchKind::HypergraphColoring)];
  assert(hint.available());
  for (Var variable = 0; variable < kernel.nvars; ++variable) {
    const Lit positive = make_lit(variable, false);
    if (!kernel.value(positive) || kernel.assigned[variable].level) continue;
    const Var external = kernel.internal_to_external[variable];
    assert(external < hint.external_phases.size());
    assert(hint.external_phases[external] ==
           (kernel.value(positive) > 0 ? 1 : -1));
  }
  kernel.nol_hypergraph_walk_session = nullptr;
}

static void structured_root_terminal_skips_initial_nol_case() {
  Kernel kernel = parse(complete_hypergraph(7, 7));
  NolHypergraphWalkSession session;
  kernel.nol_hypergraph_walk_session = &session;
  ScopedHeuristics heuristics(kernel);
  auto config = quiet_config();
  const DefaultStrategyConfig reference;
  config.inprocess.structured_root_stages =
      reference.inprocess.structured_root_stages;
  config.nol_walk.enabled = true;
  config.nol_walk.work_quantum = 8000;
  DefaultPolicy policy(config);
  assert(policy.solve(kernel) == 20);
  assert(kernel.stats.sr_applications > 0);
  assert(policy.nol_walk_diagnostics().authentication_attempts == 1);
  assert(policy.nol_walk_diagnostics().authentication_successes == 1);
  assert(policy.nol_walk_diagnostics().slices == 0);
  assert(kernel.stats.nol_hypergraph_model_calls == 0);
  assert(session.authenticated(kernel));
  assert(!session.initialized() && !session.steps() &&
         !session.trajectories());
  kernel.nol_hypergraph_walk_session = nullptr;
}

static void partial_restart_case() {
  Kernel kernel = parse(complete_hypergraph(10, 7));
  NolHypergraphWalkSession session;
  kernel.nol_hypergraph_walk_session = &session;
  ScopedHeuristics heuristics(kernel);
  auto config = quiet_config();
  config.nol_walk.enabled = true;
  config.nol_walk.work_quantum = 8000;
  config.nol_walk.initial_tick_interval = UINT64_C(1) << 60;
  config.nol_walk.maximum_delay_ticks = UINT64_C(1) << 60;
  DefaultPolicy policy(config);
  volatile sig_atomic_t stop = 0;
  Capture capture{&policy, Scenario::PartialRestart};
  ControlledBranching branching{policy, capture, stop, Scenario::PartialRestart};
  policy.branching = &branching;
  policy.set_feedback(&capture);
  assert(policy.solve(kernel, &stop) == 0);

  assert(branching.picks == 3 && branching.partial_keep > 0);
  assert(policy.nol_walk_diagnostics().slices == 1);
  assert(kernel.stats.nol_hypergraph_model_calls == 1);
  assert(kernel.stats.nol_hypergraph_model_recognized == 1);
  assert(policy.nol_walk_diagnostics().authentication_attempts == 1);
  assert(capture.restart_levels ==
         std::vector<uint64_t>({branching.partial_keep}));
  assert(capture.intervals.size() == 2);
  assert(capture.intervals[0].stop == StopReason::Restart);
  assert(capture.intervals[0].after.level == branching.partial_keep);
  assert(capture.intervals[1].start == IntervalStart::PartialContinuation);
  assert(capture.intervals[1].stop == StopReason::Interrupted);
  kernel.nol_hypergraph_walk_session = nullptr;
}

static void disabled_default_case() {
  assert(!DefaultStrategyConfig{}.nol_walk.enabled);
  Kernel kernel = parse(complete_hypergraph(10, 7));
  NolHypergraphWalkSession session;
  kernel.nol_hypergraph_walk_session = &session;
  ScopedHeuristics heuristics(kernel);
  auto config = quiet_config();
  assert(!config.nol_walk.enabled);
  DefaultPolicy policy(config);
  volatile sig_atomic_t stop = 0;
  Capture capture{&policy, Scenario::Disabled};
  ControlledBranching branching{policy, capture, stop, Scenario::Disabled};
  policy.branching = &branching;
  policy.set_feedback(&capture);
  assert(policy.solve(kernel, &stop) == 0);
  assert(branching.picks == 1);
  assert(policy.nol_walk_diagnostics().slices == 0);
  assert(kernel.stats.nol_hypergraph_model_calls == 0);
  assert(kernel.stats.nol_hypergraph_model_recognized == 0);
  assert(policy.nol_walk_diagnostics().authentication_attempts == 0);
  assert(capture.restart_levels.empty());
  kernel.nol_hypergraph_walk_session = nullptr;
}

int main() {
  schedule_opt_out_case();
  affine_ordering_case();
  nol_restart_case();
  authentication_allowance_case();
  authentication_exhaustion_case();
  // Exact publication identities, installed targets and the first consuming
  // CDCL decision replay identically from the same source image and budgets.
  assert(phase_consumption_case() == phase_consumption_case());
  structured_root_before_initial_nol_case();
  structured_root_terminal_skips_initial_nol_case();
  partial_restart_case();
  disabled_default_case();
  std::puts("test_default_nol_restart_loop OK");
}
