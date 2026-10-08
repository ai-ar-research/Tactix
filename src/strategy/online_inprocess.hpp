#pragma once

#include "rules/factor/factor.hpp"
#include "rules/condition/condition.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/direct_csp_guarded_cover_action.hpp"
#include "rules/semantic_extension/affine_basis_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_transitivity_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_amo_action.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"
#include "rules/semantic_extension/gf3_general_row_action.hpp"
#include "rules/lucky/tseitin_cycle_action.hpp"
#include "strategy/inprocess_program.hpp"
#include "strategy/work_settings.hpp"
#include "strategy/rule_invocation.hpp"
#include "strategy/bve_occurrence_cache.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tactix::strategy {

enum class RepeatMode : uint8_t {
  Once,
  Count,
  UntilScopedFixpoint,
  // UntilScopedFixpoint whose round ceiling is the owner's size scale (satsuma.h
  // iteration_max): one initial pass plus
  // clamp(ceil((2^23 / (literals + variables))^1.5), 1, limit) repetitions, read
  // from the live formula at every round. A zero limit leaves the scale itself
  // as the only ceiling.
  UntilSizeScaledFixpoint,
};

inline bool fixpoint_repeat(RepeatMode mode) {
  return mode == RepeatMode::UntilScopedFixpoint ||
         mode == RepeatMode::UntilSizeScaledFixpoint;
}

struct RepeatSpec {
  RepeatMode mode = RepeatMode::Once;
  // Count: exact positive count. UntilScopedFixpoint: zero is unbounded,
  // otherwise a caller-selected safety ceiling which reports exhaustion.
  uint64_t limit = 1;
};

struct Gf3AffineInstance {
  uint32_t first_equation = 0;
  uint32_t second_equation = 0;
  bool operator==(const Gf3AffineInstance &) const = default;
};

// The round ceiling a fixpoint repeat applies now; zero is unbounded.
uint64_t repeat_round_limit(const Kernel &, const RepeatSpec &);

// A typed atomic follow-up owned by the composition layer.  Propagation is a
// separate proof rule, but CompleteCurrentPass may contain many semantic
// applications before the surrounding sequence regains control.  Selecting
// Propagate therefore closes each application inside the executor's
// state-version transaction instead of forcing a caller-side mutation that
// would make the retained selection stale.
enum class PostApplication : uint8_t {
  None,
  Propagate,
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
  // Both vectors retain external literal identities until each atomic apply.
  std::vector<ConditionInstance> condition_instances = {};
  // Variable-length positive stable external vertex sets for certified
  // IndependentSetCover consequences. Each inner vector is one action.
  std::vector<std::vector<uint32_t>> independent_set_cover_instances = {};
  std::vector<Gf3AffineInstance> gf3_affine_instances = {};
  // Complete exact GF(2) row/source identities.  These are deep-copied action
  // values; live apply rederives and authenticates every field.
  std::vector<AffineBasisRowSelection> affine_basis_instances = {};
  // Canonical opaque identities returned by gf3_gaussian_action_offer().
  // They encode exact semantics, derivation provenance, live premise IDs and
  // source clauses; they are stable replay keys rather than mutable ordinals.
  std::vector<std::string> gf3_gaussian_instances = {};
  std::vector<std::string> tseitin_cycle_instances = {};
  std::vector<DirectCspSupportActionSelection> direct_csp_support_instances = {};
  std::vector<NaryCspEqualityActionSelection> nary_csp_equality_instances = {};
  std::vector<NaryCspEqualityTransitivitySelection> nary_csp_equality_transitivity_instances = {};
  std::vector<NaryCspEqualityAmoSelection> nary_csp_equality_amo_instances = {};
  std::vector<Gf3GeneralRowActionSelection> gf3_general_row_instances = {};
  std::vector<DirectCspGuardedCoverActionSelection> direct_csp_guarded_cover_instances = {};
  RepeatSpec repeat;
  PostApplication post_application = PostApplication::None;
  RuleWorkBudget rule_work_budget;
  RuleMaterialSettings material;
  WorkUnit work_unit = WorkUnit::Applications;
  WorkLimit budget = WorkLimit::unlimited();
};

// A separately selected Propagate may close an already settled producer
// boundary.  The self-Propagate marker is part of the serialized request, so
// replay distinguishes this explicit metered fixpoint service from an
// ordinary inapplicable Propagate choice. It performs no state transition.
inline bool explicit_fixpoint_propagation_service(
    const OnlineInvocation &step, const Kernel &kernel) {
  return step.rule == "Propagate" &&
         step.post_application == PostApplication::Propagate &&
         !kernel.level && !kernel.inconsistent && !kernel.conflict_valid &&
         kernel_propagation_fixpoint(kernel);
}

struct OnlineStrategyProgram {
  std::vector<OnlineInvocation> sequence;
  RepeatSpec repeat;
};

using OnlineInprocessProgram = OnlineStrategyProgram;

// Composition-only feedback for a shared SR-discovery allowance.  A detected
// exact generator may fund more work inside the same backend session, up to
// the scope's hard ceiling.  Zero disables inline continuation.
struct DiscoveryFeedbackPolicy {
  uint64_t ticks_per_generator = 0;
  uint64_t hard_limit = 0;
};

enum class ReconfigureMode : uint8_t {
  ResetRuntime,
  RetainMatchingStepBudgets,
};

bool validate_online_strategy_program(const OnlineStrategyProgram &,
                                      std::string *error = nullptr,
                                      bool allow_native_offer = false);
std::string serialize_online_strategy_program(const OnlineStrategyProgram &);
bool deserialize_online_strategy_program(const std::string &,
                                         OnlineStrategyProgram &,
                                         std::string *error = nullptr,
                                         bool direct_csp_producer_template =
                                             false,
                                         bool allow_native_offer = false);

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
  InvocationResult advance(Kernel &, volatile sig_atomic_t *stop = nullptr,
                           Allowance *sr_discovery_allowance = nullptr,
                           DiscoveryFeedbackPolicy discovery_feedback = {});
  void reset_runtime();

  uint64_t revision() const { return revision_; }
  size_t step_index() const { return step_index_; }
  uint64_t sequence_passes() const { return sequence_passes_; }
  uint64_t step_consumed(StepId) const;
  bool complete() const { return complete_; }
  // Since configuration, a generated SR discovery stopped at an owner-
  // equivalent detection limit or found its shared allowance exhausted.
  bool detection_limit_reached() const { return detection_limit_reached_; }
  // Restart-controller atomic doses can renew composition allowance while
  // retaining an unfinished native selection/discovery cursor. This never
  // changes the bound solver state or silently refreshes a stale selection.
  bool pending_atomic_scope() const;
  bool renew_atomic_dose(WorkLimit allowance, uint64_t revision);

private:
  struct Runtime {
    uint64_t consumed = 0;
    uint64_t passes = 0;
    size_t cursor = 0;
    bool pass_progress = false;
    bool selection_ready = false;
    bool selection_truncated = false;
    bool selection_discovery_exhausted = false;
    bool graph_entry_requested = false;
    // Bind pending payloads to their arena and the state left by the last
    // application. External formula changes require caller reconfiguration.
    const Kernel *selection_owner = nullptr;
    uint64_t selection_generation = 0;
    StateVersion expected_state;
    std::vector<uint64_t> selection;
    std::vector<FactorGrid> structured_selection;
    std::vector<SubstitutionRedundancyInstance> sr_selection;
    UniqueLiteralOccurrenceCache unique_literal_cache;
    SubstitutionRedundancyGeneralCursor sr_general_cursor;
    bool sr_general_mode = false;
    BveOccurrenceCache bve_occurrences;
  };

  bool prepare_selection(Kernel &, const OnlineInvocation &, Runtime &,
                         InvocationResult &, volatile sig_atomic_t *stop,
                         Allowance *sr_discovery_allowance,
                         DiscoveryFeedbackPolicy discovery_feedback);
  bool finish_step_pass(const Kernel &, const OnlineInvocation &, Runtime &,
                        InvocationResult &);
  bool finish_sequence(const Kernel &, InvocationResult &);

  OnlineStrategyProgram program_;
  std::vector<Runtime> runtime_;
  size_t step_index_ = 0;
  uint64_t sequence_passes_ = 0;
  bool sequence_progress_ = false;
  bool detection_limit_reached_ = false;
  bool configured_ = false;
  bool complete_ = false;
  uint64_t revision_ = 0;
};

using OnlineInprocessExecutor = OnlineStrategyExecutor;

} // namespace tactix::strategy
