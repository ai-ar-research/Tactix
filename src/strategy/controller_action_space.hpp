#pragma once

// Versioned restart-controller choices. Execution uses the existing online
// invocation vocabulary; these identities do not change public registry IDs.
#include "strategy/online_inprocess.hpp"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace tactix::strategy::controller {

inline constexpr uint32_t ACTION_SPACE_VERSION = 3;

enum class Branching : uint8_t { Vmtf, Evsids };
enum class Restart : uint8_t { AdaptiveGlue, Reluctant };
enum class Phase : uint8_t { Saved, Target, Positive, Negative, FocusedRotation };

struct SearchConfiguration {
  Branching branching = Branching::Vmtf;
  Restart restart = Restart::AdaptiveGlue;
  Phase phase = Phase::Saved;
  bool operator==(const SearchConfiguration &) const = default;
};

inline constexpr std::array<std::string_view, 2> BRANCHING_NAMES = {
    "vmtf", "evsids"};
inline constexpr std::array<std::string_view, 2> RESTART_NAMES = {
    "adaptive-glue", "reluctant"};
inline constexpr std::array<std::string_view, 5> PHASE_NAMES = {
    "saved", "target", "positive", "negative", "focused-rotation"};

inline bool valid(SearchConfiguration configuration) {
  return static_cast<unsigned>(configuration.branching) < BRANCHING_NAMES.size() &&
         static_cast<unsigned>(configuration.restart) < RESTART_NAMES.size() &&
         static_cast<unsigned>(configuration.phase) < PHASE_NAMES.size();
}

inline std::string name(SearchConfiguration configuration) {
  if (!valid(configuration))
    return {};
  return std::string(BRANCHING_NAMES[static_cast<unsigned>(configuration.branching)]) +
         "/" + std::string(RESTART_NAMES[static_cast<unsigned>(configuration.restart)]) +
         "/" + std::string(PHASE_NAMES[static_cast<unsigned>(configuration.phase)]);
}

inline std::vector<SearchConfiguration> search_configurations() {
  std::vector<SearchConfiguration> result;
  for (unsigned branch = 0; branch < BRANCHING_NAMES.size(); ++branch)
    for (unsigned restart = 0; restart < RESTART_NAMES.size(); ++restart)
      for (unsigned phase = 0; phase < PHASE_NAMES.size(); ++phase)
        result.push_back({static_cast<Branching>(branch),
                          static_cast<Restart>(restart),
                          static_cast<Phase>(phase)});
  return result;
}

inline bool parse_search_configuration(std::string_view text,
                                       SearchConfiguration &out) {
  for (const auto configuration : search_configurations())
    if (name(configuration) == text) {
      out = configuration;
      return true;
    }
  return false;
}

// The root action set excludes search services and mode coupling. Their N1
// public surfaces remain unchanged; the controller owns the full restart and
// the search loop, and installs strategy choices only in ResumeSearch.
inline std::string_view root_exclusion(const RuleVTable &rule) {
  const std::string_view identity(rule.name);
  if (identity == "Decide" || identity == "ConflictAnalysis")
    return "search-service";
  if (identity == "Restart")
    return "controller-full-restart";
  if (identity == "SwitchMode")
    return "ResumeSearch-selects-configuration";
  return {};
}

enum class ActionType : uint8_t { Rule, ResumeSearch };

// Allowances are controller configuration, not learner-selected payloads.
// One application is the default atomic action dose. Native limits may be
// bounded independently in the rule's own meter; actual overshoot is retained.
struct ActionAllowance {
  WorkUnit unit = WorkUnit::Applications;
  uint64_t amount = 1;
  RuleWorkBudget native;
};

struct ActionRequest {
  uint32_t version = ACTION_SPACE_VERSION;
  uint64_t sequence = 0;
  StateVersion expected;
  ActionType type = ActionType::Rule;
  OnlineInvocation invocation;
  SearchConfiguration search;
};

struct ActionResult {
  uint64_t sequence = 0;
  ActionType type = ActionType::Rule;
  InvocationResult invocation;
  StateVersion before, after;
  uint64_t propagation_ticks = 0;
  uint64_t root_assignments = 0;
  uint64_t elapsed_nanos = 0;
  uint64_t composition_overshoot = 0;
  bool resumed_search = false;
};

} // namespace tactix::strategy::controller
