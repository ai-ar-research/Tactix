#pragma once

#include "rules/factor/factor.hpp"
#include "strategy/inprocess_program.hpp"
#include "strategy/rule_invocation.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tactix::strategy {

enum class RepeatMode : uint8_t {
  Once,
  Count,
  UntilScopedFixpoint,
};

struct RepeatSpec {
  RepeatMode mode = RepeatMode::Once;
  // Count: exact positive count. UntilScopedFixpoint: zero is unbounded,
  // otherwise a caller-selected safety ceiling which reports exhaustion.
  uint64_t limit = 1;
};

enum class RuleWorkBudgetMode : uint8_t {
  Inherit,
  Unlimited,
  Bounded,
};

// A rule's native internal effort ceiling is separate from the composition
// budget below, which counts completed semantic applications. Inherit keeps a
// caller profile; Unlimited explicitly clears it; Bounded also represents
// zero. The executor poses this only for one atomic application.
struct RuleWorkBudget {
  RuleWorkBudgetMode mode = RuleWorkBudgetMode::Inherit;
  uint64_t amount = 0;
};

enum class SettingOverride : uint8_t { Inherit, Disable, Enable };

// Canonical round admission shared by the touched-material rules. Explicit
// semantic instances remain unfiltered; these settings affect only discovery
// inside a canonical application and restore after that application.
struct RuleMaterialSettings {
  SettingOverride touch_filter = SettingOverride::Inherit;
  SettingOverride likely_kept_redundant = SettingOverride::Inherit;
};

struct OnlineInvocation {
  StepId id = 0;
  std::string rule;
  InprocessParameters parameters;
  InvocationGranularity granularity =
      InvocationGranularity::CompleteCurrentPass;
  std::vector<uint64_t> instances;
  // Exact E2 grids are variable-sized semantic instances. They use the same
  // OneInstance/SelectedBatch machinery; InvocationResult::instance is the
  // zero-based index in this immutable request snapshot.
  std::vector<FactorGrid> factor_grids;
  // General SR payloads are likewise variable-sized semantic instances.
  std::vector<SubstitutionRedundancyInstance>
      substitution_redundancy_instances = {};
  RepeatSpec repeat;
  RuleWorkBudget rule_work_budget;
  RuleMaterialSettings material;
  WorkUnit work_unit = WorkUnit::Applications;
  WorkLimit budget = WorkLimit::unlimited();
};

struct OnlineStrategyProgram {
  std::vector<OnlineInvocation> sequence;
  RepeatSpec repeat;
};

using OnlineInprocessProgram = OnlineStrategyProgram;

enum class ReconfigureMode : uint8_t {
  ResetRuntime,
  RetainMatchingStepBudgets,
};

bool validate_online_strategy_program(const OnlineStrategyProgram &,
                                      std::string *error = nullptr);

inline bool validate_online_inprocess_program(const OnlineInprocessProgram &p,
                                              std::string *error = nullptr) {
  return validate_online_strategy_program(p, error);
}

InvocationStatus enumerate_complete_instances(Kernel &, const RuleVTable &,
                                              std::vector<uint64_t> &,
                                              volatile sig_atomic_t *stop = nullptr);

// Single-threaded online executor. Each advance performs at most one atomic
// registered rule application. Configuration is replace-transactional and may
// occur between advances; no partial proof or watch mutation is ever exposed.
// A bound batch invalidated by collection/external formula mutation reports
// StaleRequest until configure supplies a fresh scope; spent budgets are retained
// when requested through RetainMatchingStepBudgets.
class OnlineStrategyExecutor {
public:
  bool configure(const OnlineStrategyProgram &, uint64_t revision,
                 ReconfigureMode mode = ReconfigureMode::ResetRuntime,
                 std::string *error = nullptr);
  InvocationResult advance(Kernel &, volatile sig_atomic_t *stop = nullptr);
  void reset_runtime();

  uint64_t revision() const { return revision_; }
  size_t step_index() const { return step_index_; }
  uint64_t step_consumed(StepId) const;
  bool complete() const { return complete_; }

private:
  struct Runtime {
    uint64_t consumed = 0;
    uint64_t passes = 0;
    size_t cursor = 0;
    bool pass_progress = false;
    bool selection_ready = false;
    bool selection_truncated = false;
    // Bind pending payloads to their arena and the state left by the last
    // application. External formula changes require caller reconfiguration.
    const Kernel *selection_owner = nullptr;
    uint64_t selection_generation = 0;
    StateVersion expected_state;
    std::vector<uint64_t> selection;
    std::vector<FactorGrid> structured_selection;
    std::vector<SubstitutionRedundancyInstance> sr_selection;
    SubstitutionRedundancyGeneralCursor sr_general_cursor;
    bool sr_general_mode = false;
  };

  bool prepare_selection(Kernel &, const OnlineInvocation &, Runtime &,
                         InvocationResult &, volatile sig_atomic_t *stop);
  bool finish_step_pass(const OnlineInvocation &, Runtime &,
                        InvocationResult &);
  bool finish_sequence(InvocationResult &);

  OnlineStrategyProgram program_;
  std::vector<Runtime> runtime_;
  size_t step_index_ = 0;
  uint64_t sequence_passes_ = 0;
  bool sequence_progress_ = false;
  bool configured_ = false;
  bool complete_ = false;
  uint64_t revision_ = 0;
};

using OnlineInprocessExecutor = OnlineStrategyExecutor;

} // namespace tactix::strategy
