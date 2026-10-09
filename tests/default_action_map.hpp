#pragma once

// DefaultPolicy action-route map (gap plan Step 0,
// benchmarks/reports/2026-10-09-defaultpolicy-api-gap-plan/README.md §1).
// Route: how DefaultPolicy executes a feedback record now.
//   Public - registry dispatch (apply_rule*, RegisteredRuleRequest) or an
//            online invocation;
//   Shared - a typed request or registered function without dispatch, whose
//            transition a declared public request (id + instance) expresses;
//   Kernel - no public request expresses it yet.
// test_default_action_map ratchets the internal (Shared/Kernel) entries.

#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/registry.hpp"
#include "strategy/native_feedback.hpp"
#include "strategy/policy_feedback.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace action_map {

using namespace tactix;
using strategy::controller::ActionFeedback;
using strategy::controller::FeedbackKind;
using strategy::controller::FeedbackReceiver;
using strategy::controller::Observation;
using strategy::controller::SearchIntervalFeedback;
using strategy::controller::StopReason;

enum class Route { Public, Shared, Kernel };
// Interval: a step legitimately inside the native search interval (plan §2);
// Crossing: an action at or across a declared action boundary.
enum class Boundary { Interval, Crossing };
// How the Step 0 replay reporter expresses an entry through tactix.h.
enum class Replay {
  RegistryInstance, // tactix_apply_instance(rule id of identity, record instance)
  Decide,           // tactix_decide(record instance as DIMACS literal)
  Marker,           // no state transition of its own
  LoadProfile,      // a permanent parameter install; replayable only at load
  None              // no public request yet: the replay stops here
};

inline const char *route_name(Route r) {
  return r == Route::Public ? "P" : r == Route::Shared ? "S" : "K";
}

inline std::string_view description(const ActionFeedback &r) {
  return r.native_request ? std::string_view(r.native_request->description)
                          : std::string_view();
}
inline bool starts(std::string_view text, std::string_view prefix) {
  return text.substr(0, prefix.size()) == prefix;
}
inline bool lucky_graph_session(uint64_t instance) {
  return instance == LUCKY_GRAPH_AUTHENTICATE || instance == LUCKY_GRAPH_CONTINUE;
}

struct Entry {
  const char *id;
  FeedbackKind kind;
  const char *identity; // nullptr matches any identity of this kind
  bool (*matches)(const ActionFeedback &);
  Route route;
  Boundary boundary;
  Replay replay;
  // Records of a Public entry must show registry dispatch (strategy_epoch
  // advances inside the record); internal entries whose identity names a
  // registry rule must not, so a moved route cannot hide from the ratchet.
  bool check_dispatch;
  const char *public_route;
  const char *plan;
};

// Inprocessing slot records whose rule goes through RegisteredRuleRequest or
// apply_rule_instance. The exceptions below have their own entries.
inline bool slot_registered(const ActionFeedback &r) {
  const auto d = description(r);
  const bool slot = starts(d, "native-dosed-candidate") ||
                    starts(d, "native-budgeted-batch") ||
                    starts(d, "native-unbudgeted-batch");
  return slot && !(r.identity == "BVE" && starts(d, "native-dosed-candidate")) &&
         !(r.identity == "Sweep" && starts(d, "native-budgeted-batch")) &&
         r.identity != "Substitute";
}

inline const std::vector<Entry> &entries() {
  using K = FeedbackKind;
  static const std::vector<Entry> table = {
      // --- search-interval internals (plan §2: static micro dispatch) ---
      {"propagate-search", K::Rule, "Propagate",
       [](const ActionFeedback &r) { return description(r) == "search"; },
       Route::Shared, Boundary::Interval, Replay::RegistryInstance, true,
       "Propagate canonical (H root consumption is policy publication)", "A1"},
      {"decide", K::SearchService, "Decide", nullptr, Route::Shared,
       Boundary::Interval, Replay::Decide, true,
       "tactix_decide(literal) / Decide instance", "A2"},
      {"conflict-analysis", K::SearchService, "ConflictAnalysis", nullptr,
       Route::Kernel, Boundary::Interval, Replay::None, true,
       "none: native target after learning, per-conflict bump payload and "
       "OTFS retier hook (target now recorded as ;target=)", "A3,C1,C4"},
      {"restart-scheduled", K::SearchService, "Restart", nullptr, Route::Public,
       Boundary::Interval, Replay::RegistryInstance, true,
       "Restart instance = keep-level", "A4"},
      {"resume-search", K::ResumeSearch, "ResumeSearch", nullptr, Route::Public,
       Boundary::Interval, Replay::Marker, false,
       "interval marker (ResumeSearch native configuration: step 7)", "D1"},
      // --- macro actions at interval boundaries ---
      {"reduce", K::Rule, "Reduce", nullptr, Route::Public, Boundary::Crossing,
       Replay::RegistryInstance, true,
       "Reduce canonical under scoped ReduceParameters", "A12"},
      {"switch-mode", K::Rule, "SwitchMode", nullptr, Route::Public,
       Boundary::Crossing, Replay::RegistryInstance, true, "SwitchMode canonical",
       "A13"},
      {"reorder", K::Rule, "Reorder", nullptr, Route::Public, Boundary::Crossing,
       Replay::RegistryInstance, true, "Reorder instance = target", "A13"},
      {"rephase", K::Rule, "Rephase", nullptr, Route::Public, Boundary::Crossing,
       Replay::RegistryInstance, true, "Rephase instance = type", "A13"},
      {"walk", K::Rule, "Walk", nullptr, Route::Public, Boundary::Crossing,
       Replay::None, true,
       "Walk canonical under scoped WalkParameters (online Walk parameters)",
       "A13"},
      {"lucky-opportunity", K::Rule, "Lucky",
       [](const ActionFeedback &r) { return !lucky_graph_session(r.instance); },
       Route::Public, Boundary::Crossing, Replay::RegistryInstance, true,
       "Lucky instance (canonical = cascade, 6-24 opportunities)", "A6"},
      {"lucky-graph-session", K::Rule, "Lucky",
       [](const ActionFeedback &r) { return lucky_graph_session(r.instance); },
       Route::Public, Boundary::Crossing, Replay::RegistryInstance, true,
       "Lucky instance 26 (authenticate) / 27 (continue)", "A9"},
      {"local-search-guidance", K::Rule, "LocalSearchGuidance", nullptr,
       Route::Public, Boundary::Crossing, Replay::RegistryInstance, true,
       "LocalSearchGuidance instance = hint id", "A9"},
      {"bce-post-structured", K::Rule, "BCE", nullptr, Route::Public,
       Boundary::Crossing, Replay::RegistryInstance, true, "BCE canonical", "A7"},
      {"propagate-root", K::Rule, "Propagate",
       [](const ActionFeedback &r) { return description(r) != "search"; },
       Route::Shared, Boundary::Crossing, Replay::RegistryInstance, true,
       "Propagate canonical (H root consumption is policy publication)", "A1"},
      {"affine-matrix-initialize", K::Rule, "AffineMatrixInitialize", nullptr,
       Route::Kernel, Boundary::Crossing, Replay::None, true,
       "partial: AffineMatrixInitialize op; no public disable", "A8"},
      {"affine-matrix-search", K::SearchService, "AffineMatrixSearch", nullptr,
       Route::Kernel, Boundary::Crossing, Replay::None, false,
       "partial: AffineMatrixInitialize SearchStep operation", "A8"},
      // --- inprocessing slots (A10) ---
      {"slot-registered", K::Rule, nullptr,
       [](const ActionFeedback &r) { return slot_registered(r); },
       Route::Public, Boundary::Crossing, Replay::None, true,
       "registry dispatch of the slot rule (scoped slot parameters)", "A10"},
      {"slot-bve-candidate", K::Rule, "BVE",
       [](const ActionFeedback &r) {
         return starts(description(r), "native-dosed-candidate");
       },
       Route::Public, Boundary::Crossing, Replay::None, true,
       "BVE instance dispatched by the shared typed BVE invocation session "
       "(the policy's heap/score hook is C4, outside the record)", "A10,C4"},
      {"structured-root-online", K::Rule, nullptr,
       [](const ActionFeedback &r) {
         return starts(description(r), "structured-stage=");
       },
       Route::Public, Boundary::Crossing, Replay::None, true,
       "online invocation of the structured-root program", "A10"},
      {"slot-sweep-batch", K::Rule, "Sweep",
       [](const ActionFeedback &r) {
         return starts(description(r), "native-budgeted-batch");
       },
       Route::Shared, Boundary::Crossing, Replay::None, true,
       "Sweep canonical under SweepLimits", "A10"},
      {"slot-substitute-rounds", K::Rule, "Substitute", nullptr, Route::Kernel,
       Boundary::Crossing, Replay::None, true,
       "none: max-rounds and per-round effort break are policy closures", "A10"},
      {"slot-vivify-band", K::Rule, "Vivify",
       [](const ActionFeedback &r) {
         return starts(description(r), "native-vivify-band");
       },
       Route::Kernel, Boundary::Crossing, Replay::None, true,
       "none: cumulative vivify band ledger", "A10"},
      {"slot-fastel-occurrence", K::Rule, "SubsumeForward",
       [](const ActionFeedback &r) {
         return starts(description(r), "fastel-occurrence");
       },
       Route::Kernel, Boundary::Crossing, Replay::None, false,
       "none: fastel occurrence traversal calls rule internals", "A10"},
      // --- support transactions (A5, A11, C6) ---
      {"install-parameters", K::Support, "InstallRuleParameters", nullptr,
       Route::Kernel, Boundary::Crossing, Replay::LoadProfile, false,
       "partial: legacy tactix_set_*; online parameters are step-scoped", "C6"},
      {"install-walk-guidance", K::Support, "InstallWalkGuidance", nullptr,
       Route::Kernel, Boundary::Crossing, Replay::LoadProfile, false,
       "none: permanent Walk guidance install", "C6"},
      {"install-material-settings", K::Support, "InstallMaterialSettings",
       nullptr, Route::Kernel, Boundary::Crossing, Replay::LoadProfile, false,
       "none: permanent material settings", "C6"},
      {"close-startup-certificate", K::Support, "CloseStartupCertificate",
       nullptr, Route::Public, Boundary::Crossing, Replay::Marker, false,
       "implied by the next non-startup registry dispatch", "A11"},
      {"support-transaction", K::Support, nullptr, nullptr, Route::Kernel,
       Boundary::Crossing, Replay::None, false,
       "none: support vocabulary (RootPreparation, preparation Restart, "
       "SparseCollect, CompactVariables, Warmup, ...) is step 4", "A5,A11"},
  };
  return table;
}

inline const Entry *classify(const ActionFeedback &record) {
  for (const Entry &entry : entries()) {
    if (entry.kind != record.kind) continue;
    if (entry.identity && record.identity != entry.identity) continue;
    if (entry.matches && !entry.matches(record)) continue;
    return &entry;
  }
  return nullptr;
}

inline bool registry_identity(const ActionFeedback &record) {
  return tactix_rule_id(record.identity.c_str()) >= 0;
}

inline bool dispatched(const ActionFeedback &record) {
  return record.after.state.strategy_epoch > record.before.state.strategy_epoch;
}

// ---------------------------------------------------------------------------
// Fixtures: the default-execution-signature profiles plus targeted startup
// opportunities. Each run returns its complete feedback stream.

struct Capture final : FeedbackReceiver {
  std::vector<ActionFeedback> actions;
  void after_action(const ActionFeedback &value) override { actions.push_back(value); }
  void search_interval(const SearchIntervalFeedback &) override {}
};

struct StopAtFirstDecision final : BranchingPolicy {
  volatile sig_atomic_t &stop;
  explicit StopAtFirstDecision(volatile sig_atomic_t &s) : stop(s) {}
  const char *name() const override { return "stop-at-first-decision"; }
  Lit pick(Kernel &kernel) override {
    stop = 1;
    for (Var variable = 0; variable < kernel.nvars; ++variable)
      if (!kernel.value(make_lit(variable, false)) &&
          !kernel_var_retired(kernel, variable))
        return make_lit(variable, false);
    return INVALID_LIT;
  }
};

struct Fixture {
  std::string name;
  std::function<void(Kernel &)> load;
  std::function<void(DefaultStrategyConfig &, Kernel &)> configure;
  bool stop_at_first_decision = false;
  // Structured-root programs run only without a proof or with native DSR.
  bool clausal_proof = true;
  // The same input as DIMACS clauses for the public-API replay (empty: the
  // fixture is not replayed), plus kernel parameters set at load.
  std::vector<std::vector<int>> dimacs;
  std::function<void(Kernel &)> tune;
};

inline std::vector<std::vector<int>> pigeonhole_dimacs(unsigned pigeons, unsigned holes) {
  std::vector<std::vector<int>> out;
  auto cell = [=](unsigned p, unsigned h) { return int(p * holes + h + 1); };
  for (unsigned p = 0; p < pigeons; ++p) {
    out.emplace_back();
    for (unsigned h = 0; h < holes; ++h) out.back().push_back(cell(p, h));
    for (unsigned q = p + 1; q < pigeons; ++q)
      for (unsigned h = 0; h < holes; ++h) out.push_back({-cell(p, h), -cell(q, h)});
  }
  return out;
}

inline std::vector<std::vector<int>> file_dimacs(const std::string &path) {
  std::vector<std::vector<int>> out(1);
  FILE *input = std::fopen(path.c_str(), "r");
  assert(input);
  char line[4096];
  while (std::fgets(line, sizeof line, input)) {
    if (line[0] == 'c' || line[0] == 'p') continue;
    for (char *cursor = line;;) {
      char *end = nullptr;
      const long value = std::strtol(cursor, &end, 10);
      if (end == cursor) break;
      cursor = end;
      if (value) out.back().push_back(static_cast<int>(value));
      else out.emplace_back();
    }
  }
  std::fclose(input);
  out.pop_back();
  return out;
}

inline void pigeonhole(Kernel &k, unsigned pigeons, unsigned holes) {
  kernel_reserve_vars(k, pigeons * holes);
  auto cell = [=](unsigned p, unsigned h) { return make_lit(p * holes + h, false); };
  for (unsigned p = 0; p < pigeons; ++p) {
    std::vector<Lit> clause;
    for (unsigned h = 0; h < holes; ++h) clause.push_back(cell(p, h));
    kernel_add_large_clause(k, clause.data(), holes, false, 0, ++k.next_clause_id);
    for (unsigned q = p + 1; q < pigeons; ++q)
      for (unsigned h = 0; h < holes; ++h)
        kernel_add_binary_clause(k, negate(cell(p, h)), negate(cell(q, h)),
                                 ++k.next_clause_id);
  }
}

// The six DefaultPolicy profiles of test_default_execution_signature.
inline void signature_profile(DefaultStrategyConfig &config, unsigned profile) {
  if (!profile) return;
  config.inprocess.program.clear();
  config.inprocess.append_unlisted_rules = false;
  config.inprocess.structured_root_stages.clear();
  config.inprocess.structured_root_bounded_backtracking_stages.clear();
  config.inprocess.structured_root_large_domain_stages.clear();
  config.inprocess.initial_fast_elimination = false;
  config.rephase.walk_warmup = false;
  config.rephase.late_lucky = false;
  config.mode.force = profile <= 2 ? profile : 2;
  config.analysis.chronological_levels = profile == 2 ? 0 : 100;
  config.analysis.reuse_ranked_trail_suffix = profile == 1;
  config.restart.reluctant_period = 2;
  config.reduction.initial_interval = 5;
  config.rephase.initial_interval = 10;
  config.reorder.initial_interval = 10;
  if (profile >= 3) {
    config.rephase.walk_warmup = true;
    config.rephase.stable_only = false;
    config.rephase.initial_interval = 1;
    config.rephase.interval = 1;
    config.rephase.rotation = {DefaultRephaseSlot::Walk};
    config.rephase.walk_permille = 1;
    if (profile == 4) {
      config.mode.force = 0;
      config.mode.initial_conflicts = 1;
      config.mode.interval = 1;
    }
    if (profile == 5)
      config.rephase.rotation = {DefaultRephaseSlot::Best,
          DefaultRephaseSlot::Original, DefaultRephaseSlot::Inverted};
  }
}

// A 4x4 permutation with an over-strong same-column objective: graph labeling
// authenticates, continues and publishes a LocalSearchGuidance hint
// (tests/test_graph_labeling_model.cpp).
inline void graph_parameters(Kernel &k) {
  k.graph_labeling_parameters.work.fill(100000);
  k.graph_labeling_parameters.workspace_bytes = 1 << 20;
  k.graph_labeling_parameters.repair_rounds = 3;
  k.graph_labeling_parameters.anneal_iterations = 20;
  k.graph_labeling_parameters.stall_anneal_iterations = 3;
}

inline std::vector<std::vector<int>> graph_permutation_dimacs() {
  constexpr int n = 4;
  std::vector<std::vector<int>> out;
  for (int column = 0; column < n; ++column) {
    out.emplace_back();
    for (int row = 0; row < n; ++row) out.back().push_back(row * n + column + 1);
  }
  for (int row = 0; row < n; ++row)
    for (int a = 0; a < n; ++a)
      for (int b = a + 1; b < n; ++b) out.push_back({-(row * n + a + 1), -(row * n + b + 1)});
  for (int a = 0; a < n; ++a)
    for (int b = a + 1; b < n; ++b) out.push_back({-(a * n + 3), -(b * n + 3)});
  return out;
}

inline void graph_permutation(Kernel &k) {
  constexpr uint32_t n = 4;
  kernel_reserve_vars(k, n * n);
  for (uint32_t column = 0; column < n; ++column) {
    std::vector<Lit> cover;
    for (uint32_t row = 0; row < n; ++row) cover.push_back(make_lit(row * n + column, false));
    kernel_add_large_clause(k, cover.data(), cover.size(), false, 0);
  }
  for (uint32_t row = 0; row < n; ++row)
    for (uint32_t a = 0; a < n; ++a)
      for (uint32_t b = a + 1; b < n; ++b)
        kernel_add_binary_clause(k, make_lit(row * n + a, true), make_lit(row * n + b, true));
  for (uint32_t a = 0; a < n; ++a)
    for (uint32_t b = a + 1; b < n; ++b)
      kernel_add_binary_clause(k, make_lit(a * n + 2, true), make_lit(b * n + 2, true));
  graph_parameters(k);
}

// CMake compiles tests from absolute paths, so the fixtures directory is found
// beside this header without a test argument.
inline std::string fixtures_dir() {
  const std::string here = __FILE__;
  return here.substr(0, here.find_last_of('/')) + "/fixtures/";
}

inline std::vector<Fixture> fixtures() {
  std::vector<Fixture> out;
  for (unsigned profile = 0; profile <= 5; ++profile)
    for (unsigned pigeons : {4u, 6u})
      out.push_back({"profile-" + std::to_string(profile) + "-pigeons-" +
                         std::to_string(pigeons),
                     [=](Kernel &k) { pigeonhole(k, pigeons, 5); },
                     [=](DefaultStrategyConfig &c, Kernel &) { signature_profile(c, profile); },
                     false, true, pigeonhole_dimacs(pigeons, 5), nullptr});
  for (const char *name : {"online-config-3.cnf", "online-config-4.cnf",
                           "sweep_interrupted_direction.cnf", "sweep-retarget-lrat.cnf"}) {
    const std::string path = fixtures_dir() + name;
    out.push_back({name, [=](Kernel &k) {
                     FILE *input = std::fopen(path.c_str(), "r");
                     assert(input && !parse_dimacs(input, k));
                     std::fclose(input);
                   }, nullptr, false, true, file_dimacs(path), nullptr});
  }
  out.push_back({"graph-labeling-permutation", graph_permutation,
                 [](DefaultStrategyConfig &c, Kernel &k) {
                   signature_profile(c, 1);
                   c.rephase.walk_warmup = false;
                   for (bool *enabled : {&c.affine_root_closure.enabled,
                            &c.affine_model.enabled, &c.parity_model.enabled,
                            &c.onehot_model.enabled, &c.ramsey_startup.enabled,
                            &c.signed_cardinality_proof.enabled,
                            &c.balanced_signature_model.enabled,
                            &c.exact_cover_model.enabled,
                            &c.independent_set_model.enabled,
                            &c.fermat_model.enabled, &c.cerny_model.enabled,
                            &c.lookup_circuit_model.enabled,
                            &c.simon_model.enabled, &c.stconn_proof.enabled})
                     *enabled = false;
                   c.graph_labeling_model.enabled = true;
                   c.graph_labeling_model.parameters = k.graph_labeling_parameters;
                 }, true, true, graph_permutation_dimacs(), graph_parameters});
  out.push_back({"post-structured-bce", [](Kernel &k) {
                   kernel_reserve_vars(k, 7);
                   const Lit source[] = {internalize(1), internalize(2), internalize(3)};
                   kernel_add_large_clause(k, source, 3, false, 0, ++k.next_clause_id);
                   kernel_add_binary_clause(k, internalize(4), internalize(6), ++k.next_clause_id);
                   kernel_add_binary_clause(k, internalize(-4), internalize(7), ++k.next_clause_id);
                 },
                 [](DefaultStrategyConfig &config, Kernel &) {
                   // tests/test_native_policy_feedback.cpp: unique-literal AMO
                   // completion plus a blocked addition triggers cleanup BCE.
                   signature_profile(config, 1);
                   strategy::OnlineInvocation complete;
                   complete.id = 1;
                   complete.rule = "SubstitutionRedundancy";
                   SubstitutionRedundancyParameters ulc;
                   ulc.producer = SubstitutionRedundancyProducer::Orbitopal;
                   ulc.maximum_orbitopal_matrices = 0;
                   ulc.complete_unique_literal_amos = true;
                   complete.parameters = ulc;
                   complete.granularity = strategy::InvocationGranularity::CompleteCurrentPass;
                   strategy::OnlineInvocation added;
                   added.id = 2;
                   added.rule = complete.rule;
                   added.parameters = SubstitutionRedundancyParameters{};
                   added.granularity = strategy::InvocationGranularity::OneInstance;
                   added.substitution_redundancy_instances = {
                       {{internalize(5), internalize(7)}, {}, {}}};
                   strategy::OnlineStrategyProgram program;
                   program.sequence = {complete, added};
                   config.inprocess.structured_root_stages = {
                       {program, strategy::BudgetExhaustionPolicy::Continue}};
                   config.inprocess.structured_root_before_lucky = true;
                 }, false, false, {}, nullptr});
  return out;
}

// Run DefaultPolicy on an already loaded kernel and return its feedback.
inline std::vector<ActionFeedback> run(const Fixture &fixture, Kernel &k) {
  DefaultStrategyConfig config;
  if (fixture.configure) fixture.configure(config, k);
  DefaultPolicy policy(config);
  volatile sig_atomic_t stop = 0;
  StopAtFirstDecision branching(stop);
  if (fixture.stop_at_first_decision) policy.branching = &branching;
  Capture capture;
  policy.set_feedback(&capture);
  const int verdict = policy.solve(k, fixture.stop_at_first_decision ? &stop : nullptr);
  assert(verdict == 0 || verdict == 10 || verdict == 20);
  return capture.actions;
}

} // namespace action_map
