#pragma once

// DefaultPolicy action-route map (gap plan,
// benchmarks/reports/2026-10-09-defaultpolicy-api-gap-plan/README.md §1).
// Route: how DefaultPolicy executes a feedback record now.
//   Public - registry dispatch, an online invocation, a public support, or a
//            registered function the port twin proves (plan §3);
//   Shared - a typed request without dispatch whose transition a declared
//            public request (id + instance) expresses;
//   Kernel - no public request expresses it yet;
//   Declared - a declared non-action.
// test_default_action_map ratchets the internal (Shared/Kernel) entries.

// Rename the public alias as api.cpp does: its `tactix` typedef would collide
// with the internal namespace.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/analyze/analyze.hpp"
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

enum class Route { Public, Shared, Kernel, Declared };
// Interval: a step legitimately inside the native search interval (plan §2);
// Crossing: an action at or across a declared action boundary.
enum class Boundary { Interval, Crossing };
// How the Step 0 replay reporter expresses an entry through tactix.h.
enum class Replay {
  RegistryInstance, // tactix_apply_instance(rule id of identity, record instance)
  Decide,           // tactix_decide(record instance as DIMACS literal)
  Marker,           // no state transition of its own
  Install,          // tactix_install_parameters / tactix_install_material_settings
  Support,          // tactix_support(entry support, record argument)
  Online,           // tactix_online_configure_v2 + tactix_online_advance
  Analysis,         // install the record's analysis payload, then
                    // tactix_apply_semantic_instance(ANALYZE_TARGET_CHRONO)
  None              // no public request yet: the replay stops here
};

inline const char *route_name(Route r) {
  return r == Route::Public   ? "P"
         : r == Route::Shared ? "S"
         : r == Route::Kernel ? "K"
                              : "D";
}
inline bool declared_route(Route r) { return r == Route::Declared; }
inline bool internal(Route r) { return r == Route::Shared || r == Route::Kernel; }

inline std::string_view description(const ActionFeedback &r) {
  return r.native_request ? std::string_view(r.native_request->description)
                          : std::string_view();
}
inline bool starts(std::string_view text, std::string_view prefix) {
  return text.substr(0, prefix.size()) == prefix;
}
// A step of the structured-root online program, whatever its rule.
inline bool structured_stage(const ActionFeedback &r) {
  return starts(description(r), "structured-stage=");
}
inline bool installs_rule(const ActionFeedback &r, std::string_view rule) {
  return r.native_request && r.native_request->invocation &&
         r.native_request->invocation->rule == rule;
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
  int support = 0; // TACTIX_SUPPORT_* for Replay::Support
};

// The tactix_support argument a support record carries: its instance, or
// zero for a canonical record and for a context leave (whose instance names
// the context it leaves).
inline uint64_t support_argument(const Entry &entry, const ActionFeedback &r) {
  if (entry.support == TACTIX_SUPPORT_LEAVE_CONTEXT || r.instance == INSTANCE_CANONICAL)
    return 0;
  return r.instance;
}

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
       Route::Public, Boundary::Interval, Replay::RegistryInstance, false,
       "tactix_apply(Propagate); production calls the registered propagate_apply "
       "statically (default_public_correspondence)", "A1"},
      {"decide", K::SearchService, "Decide", nullptr, Route::Public,
       Boundary::Interval, Replay::Decide, false,
       "tactix_decide(literal); production calls the registered decide_apply "
       "statically (default_public_correspondence)", "A2"},
      {"conflict-analysis", K::SearchService, "ConflictAnalysis",
       [](const ActionFeedback &r) { return r.instance == ANALYZE_TARGET_CHRONO; },
       Route::Public, Boundary::Interval, Replay::Analysis, false,
       "ConflictAnalysis semantic instance ANALYZE_TARGET_CHRONO under the "
       "installed bump payload (tactix_install_parameters) and retier schedule "
       "(tactix_install_retier_schedule); the policy reads only the completed "
       "transaction's statistics. A static in-interval instantiation (plan "
       "section 3), so no registry dispatch is expected", "A3,C1,C3,C4"},
      {"restart-scheduled", K::SearchService, "Restart", nullptr, Route::Public,
       Boundary::Interval, Replay::RegistryInstance, true,
       "Restart instance = keep-level", "A4"},
      {"resume-search", K::ResumeSearch, "ResumeSearch", nullptr, Route::Public,
       Boundary::Interval, Replay::Marker, false,
       "interval marker (ResumeSearch native configuration: step 7)", "D1"},
      // --- macro actions at interval boundaries ---
      {"reduce", K::Rule, "Reduce",
       [](const ActionFeedback &r) { return !structured_stage(r); },
       Route::Public, Boundary::Crossing, Replay::RegistryInstance, true,
       "Reduce canonical under scoped ReduceParameters", "A12"},
      {"switch-mode", K::Rule, "SwitchMode", nullptr, Route::Public,
       Boundary::Crossing, Replay::RegistryInstance, true, "SwitchMode canonical",
       "A13"},
      {"reorder", K::Rule, "Reorder", nullptr, Route::Public, Boundary::Crossing,
       Replay::RegistryInstance, true, "Reorder instance = target", "A13"},
      {"rephase", K::Rule, "Rephase", nullptr, Route::Public, Boundary::Crossing,
       Replay::RegistryInstance, true, "Rephase instance = type", "A13"},
      {"walk", K::Rule, "Walk", nullptr, Route::Public, Boundary::Crossing,
       Replay::Online, true,
       "Walk as one online step under its scoped WalkParameters (step budget, "
       "seed, seed candidate)",
       "A13"},
      {"lucky-opportunity", K::Rule, "Lucky",
       [](const ActionFeedback &r) {
         return !lucky_graph_session(r.instance) && !structured_stage(r);
       },
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
       Route::Public, Boundary::Crossing, Replay::RegistryInstance, false,
       "tactix_apply(Propagate); production calls the registered propagate_apply "
       "statically (default_public_correspondence)", "A1"},
      {"affine-matrix-initialize", K::Rule, "AffineMatrixInitialize", nullptr,
       Route::Public, Boundary::Crossing, Replay::None, false,
       "AffineMatrixInitialize under tactix_install_affine_matrix_profile "
       "(complete-snapshot or persistent operation), its typed result read back "
       "(tactix_read_affine_matrix_result), released by "
       "TACTIX_SUPPORT_RELEASE_AFFINE_MATRIX; production calls the registered "
       "functions statically (default_public_correspondence)", "A8"},
      {"affine-matrix-search", K::SearchService, "AffineMatrixSearch", nullptr,
       Route::Public, Boundary::Crossing, Replay::None, false,
       "AffineMatrixInitialize under tactix_install_affine_matrix_profile "
       "(complete-snapshot or persistent operation), its typed result read back "
       "(tactix_read_affine_matrix_result), released by "
       "TACTIX_SUPPORT_RELEASE_AFFINE_MATRIX; production calls the registered "
       "functions statically (default_public_correspondence)", "A8"},
      // --- inprocessing slots (A10) ---
      {"slot-registered", K::Rule, nullptr,
       [](const ActionFeedback &r) { return slot_registered(r); },
       Route::Public, Boundary::Crossing, Replay::Online, true,
       "online invocation of the slot rule (scoped slot parameters, rule "
       "work budget = ledger remaining)", "A10"},
      {"slot-bve-candidate", K::Rule, "BVE",
       [](const ActionFeedback &r) {
         return starts(description(r), "native-dosed-candidate");
       },
       Route::Public, Boundary::Crossing, Replay::Online, true,
       "BVE instance dispatched by the shared typed BVE invocation session, "
       "which owns the score-heap ordering profile and the connected unit "
       "flush", "A10,C4"},
      {"structured-root-online", K::Rule, nullptr,
       [](const ActionFeedback &r) { return structured_stage(r); },
       Route::Public, Boundary::Crossing, Replay::Online, true,
       "online invocation of the structured-root program", "A10"},
      {"slot-sweep-batch", K::Rule, "Sweep",
       [](const ActionFeedback &r) {
         return starts(description(r), "native-budgeted-batch");
       },
       Route::Public, Boundary::Crossing, Replay::Online, true,
       "Sweep canonical under SweepLimits (registry dispatch)", "A10"},
      {"slot-substitute-rounds", K::Rule, "Substitute", nullptr, Route::Public,
       Boundary::Crossing, Replay::Online, true,
       "Substitute under SubstituteParameters{max_rounds, "
       "round_effort_ceiling} against the rule work budget", "A10"},
      {"slot-vivify-band", K::Rule, "Vivify",
       [](const ActionFeedback &r) {
         return starts(description(r), "native-vivify-band");
       },
       Route::Public, Boundary::Crossing, Replay::Online, true,
       "Vivify under VivifyParameters{scope, band ledger}", "A10"},
      {"slot-fastel-occurrence", K::Rule, "SubsumeForward",
       [](const ActionFeedback &r) {
         return starts(description(r), "fastel-occurrence");
       },
       Route::Public, Boundary::Crossing, Replay::RegistryInstance, false,
       "SubsumeForward instance TACTIX_SUBSUME_FORWARD_FASTEL (clause start, "
       "occurrence limit) inside the Eliminate occurrence session", "A10"},
      // --- permanent installs (C2, C3, C5, C6) and support (A5, A11) ---
      {"install-affine-matrix-parameters", K::Support, "InstallRuleParameters",
       [](const ActionFeedback &r) { return installs_rule(r, "AffineMatrixInitialize"); },
       Route::Public, Boundary::Crossing, Replay::None, false,
       "tactix_install_parameters (AffineMatrixInitialize, AffineConsequence profile)", "A8"},
      {"install-parameters", K::Support, "InstallRuleParameters", nullptr,
       Route::Public, Boundary::Crossing, Replay::Install, false,
       "tactix_install_parameters(rule, typed profile)", "C2,C3,C5,C6"},
      {"install-walk-guidance", K::Support, "InstallWalkGuidance", nullptr,
       Route::Public, Boundary::Crossing, Replay::Install, false,
       "tactix_install_parameters(Walk, guidance profile)", "C6"},
      {"install-material-settings", K::Support, "InstallMaterialSettings",
       nullptr, Route::Public, Boundary::Crossing, Replay::Install, false,
       "tactix_install_material_settings", "C6"},
      // --- support transactions (A5, A11): tactix_support ---
      {"close-startup-certificate", K::Support, "CloseStartupCertificate",
       nullptr, Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_CLOSE_STARTUP_CERTIFICATE", "A11",
       TACTIX_SUPPORT_CLOSE_STARTUP_CERTIFICATE},
      {"support-preparation-restart", K::Support, "Restart", nullptr,
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_PREPARATION_RESTART(keep level)", "A5",
       TACTIX_SUPPORT_PREPARATION_RESTART},
      {"support-root-preparation", K::Support, "RootPreparation", nullptr,
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_ROOT_PREPARATION(flags)", "A5,A11",
       TACTIX_SUPPORT_ROOT_PREPARATION},
      {"support-sparse-collect", K::Support, "SparseCollect", nullptr,
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_SPARSE_COLLECT", "A11", TACTIX_SUPPORT_SPARSE_COLLECT},
      {"support-compact-random-order", K::Support, "CompactRandomOrder", nullptr,
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_COMPACT_RANDOM_ORDER (H)", "A11",
       TACTIX_SUPPORT_COMPACT_RANDOM_ORDER},
      {"support-compact-variables", K::Support, "CompactVariables", nullptr,
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_COMPACT_VARIABLES(flags)", "A11,A12",
       TACTIX_SUPPORT_COMPACT_VARIABLES},
      {"support-reduction-preparation", K::Support, "ReductionPreparation",
       nullptr, Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_REDUCTION_PREPARATION", "A11,A12",
       TACTIX_SUPPORT_REDUCTION_PREPARATION},
      {"support-collect-garbage", K::Support, "CollectGarbage", nullptr,
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_COLLECT_GARBAGE(flags)", "A11",
       TACTIX_SUPPORT_COLLECT_GARBAGE},
      {"support-bve-definition-session", K::Support, "BeginBveDefinitionSession",
       nullptr, Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_BEGIN_BVE_DEFINITION_SESSION", "A11",
       TACTIX_SUPPORT_BEGIN_BVE_DEFINITION_SESSION},
      {"support-warmup", K::Support, "Warmup",
       [](const ActionFeedback &r) { return r.instance != INSTANCE_CANONICAL; },
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_WARMUP(phase): H's decision order (H)", "A11",
       TACTIX_SUPPORT_WARMUP},
      {"support-residual-guidance", K::Support, "PrepareResidualGuidance",
       nullptr, Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_RESEED_GUIDANCE (with the attach and import-order "
       "records that follow; H)", "A11", TACTIX_SUPPORT_RESEED_GUIDANCE},
      {"support-residual-import-order", K::Support, "SeedResidualImportOrder",
       nullptr, Route::Public, Boundary::Crossing, Replay::Marker, false,
       "part of TACTIX_SUPPORT_RESEED_GUIDANCE", "A11"},
      {"support-reset-graph-labeling", K::Support, "ResetGraphLabelingSession",
       nullptr, Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_RESET_GRAPH_LABELING_SESSION", "C5",
       TACTIX_SUPPORT_RESET_GRAPH_LABELING_SESSION},
      {"support-release-input-order", K::Support, "ReleaseInputBinaryOrder",
       nullptr, Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_RELEASE_INPUT_ORDER", "C5",
       TACTIX_SUPPORT_RELEASE_INPUT_ORDER},
      {"support-consume-root-prefix", K::Support, "ConsumeRootPrefix", nullptr,
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_CONSUME_ROOT_PREFIX", "C5",
       TACTIX_SUPPORT_CONSUME_ROOT_PREFIX},
      {"support-enter-context", K::Support, "EnterExecutionContext", nullptr,
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_ENTER_CONTEXT(context)", "A11",
       TACTIX_SUPPORT_ENTER_CONTEXT},
      {"support-leave-context", K::Support, "LeaveExecutionContext", nullptr,
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_LEAVE_CONTEXT", "A11", TACTIX_SUPPORT_LEAVE_CONTEXT},
      {"materialize-cold-state", K::Support, "MaterializeColdState", nullptr,
       Route::Declared, Boundary::Crossing, Replay::Marker, false,
       "declared non-action: allocates per-variable state in its initial "
       "value; predicate-equivalent, and a C session never defers it", "A11"},
      {"attach-heuristics", K::Support, "AttachHeuristics", nullptr,
       Route::Declared, Boundary::Crossing, Replay::Marker, false,
       "declared non-action: a C session owns H from tactix_init "
       "(reseeding is TACTIX_SUPPORT_RESEED_GUIDANCE)", "A11"},
      {"support-phase-entry", K::Support, "InprocessPhaseEntry",
       [](const ActionFeedback &r) { return r.instance != INSTANCE_CANONICAL; },
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_ENTER_INPROCESS_PHASE(eliminate flag, Factor bound)",
       "A11", TACTIX_SUPPORT_ENTER_INPROCESS_PHASE},
      {"support-occurrence-session", K::Support, "OpenOccurrenceSession", nullptr,
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION", "A11",
       TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION},
      {"support-phase-exit", K::Support, "InprocessPhaseExit", nullptr,
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE", "A11",
       TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE},
      {"support-forward-closure", K::Support, "ForwardClosure", nullptr,
       Route::Public, Boundary::Crossing, Replay::Marker, false,
       "the preceding Strengthen step's PostApplication::ForwardClosure "
       "(TACTIX_ONLINE_POST_FORWARD_CLOSURE)", "A10"},
      {"support-begin-forward-round", K::Support, "BeginForwardRound", nullptr,
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_BEGIN_FORWARD_ROUND", "C5",
       TACTIX_SUPPORT_BEGIN_FORWARD_ROUND},
      {"support-reschedule-bve", K::Support, "RescheduleBveCandidates", nullptr,
       Route::Public, Boundary::Crossing, Replay::Support, false,
       "TACTIX_SUPPORT_RESCHEDULE_BVE", "C5", TACTIX_SUPPORT_RESCHEDULE_BVE},
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

struct Fixture {
  std::string name;
  std::function<void(Kernel &)> load;
  std::function<void(DefaultStrategyConfig &, Kernel &)> configure;
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
                     true, pigeonhole_dimacs(pigeons, 5), nullptr});
  for (const char *name : {"online-config-3.cnf", "online-config-4.cnf",
                           "sweep_interrupted_direction.cnf", "sweep-retarget-lrat.cnf"}) {
    const std::string path = fixtures_dir() + name;
    out.push_back({name, [=](Kernel &k) {
                     FILE *input = std::fopen(path.c_str(), "r");
                     assert(input && !parse_dimacs(input, k));
                     std::fclose(input);
                   }, nullptr, true, file_dimacs(path), nullptr});
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
                 }, true, graph_permutation_dimacs(), graph_parameters});
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
                 }, false, {}, nullptr});
  return out;
}

// Run DefaultPolicy on an already loaded kernel, reporting to `receiver`.
inline void run(const Fixture &fixture, Kernel &k, FeedbackReceiver &receiver) {
  DefaultStrategyConfig config;
  if (fixture.configure) fixture.configure(config, k);
  DefaultPolicy policy(config);
  policy.set_feedback(&receiver);
  const int verdict = policy.solve(k);
  assert(verdict == 10 || verdict == 20);
}

// Run DefaultPolicy on an already loaded kernel and return its feedback.
inline std::vector<ActionFeedback> run(const Fixture &fixture, Kernel &k) {
  Capture capture;
  run(fixture, k, capture);
  return capture.actions;
}

} // namespace action_map
