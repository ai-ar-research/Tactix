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
  // UntilScopedFixpoint with the owner's size-scaled round ceiling (satsuma.h
  // iteration_max): one pass plus clamp(ceil((2^23 / (literals +
  // variables))^1.5), 1, limit) repetitions, read live each round; zero leaves
  // only the scale.
  UntilSizeScaledFixpoint,
};

inline bool fixpoint_repeat(RepeatMode mode) {
  return mode == RepeatMode::UntilScopedFixpoint ||
         mode == RepeatMode::UntilSizeScaledFixpoint;
}

struct RepeatSpec {
  RepeatMode mode = RepeatMode::Once;
  // Count: exact. UntilScopedFixpoint: zero unbounded, else a ceiling.
  uint64_t limit = 1;
};

struct Gf3AffineInstance {
  uint32_t first_equation = 0;
  uint32_t second_equation = 0;
  bool operator==(const Gf3AffineInstance &) const = default;
};

// The round ceiling a fixpoint repeat applies now; zero is unbounded.
uint64_t repeat_round_limit(const Kernel &, const RepeatSpec &);

// Atomic follow-up owned by composition: Propagate closes each application of a
// CompleteCurrentPass inside the executor's state-version transaction, rather
// than a caller-side mutation that would stale the selection.
enum class PostApplication : uint8_t {
  None,
  Propagate,
  // Strengthen only: reconnect watches and close root propagation.
  ForwardClosure,
};

struct OnlineInvocation {
  StepId id = 0;
  std::string rule;
  InprocessParameters parameters;
  InvocationGranularity granularity =
      InvocationGranularity::CompleteCurrentPass;
  std::vector<uint64_t> instances;
  // Exact E2 grids are variable-sized instances on the
  // OneInstance/SelectedBatch machinery; InvocationResult::instance indexes
  // this request snapshot.
  std::vector<FactorGrid> factor_grids;
  // General SR payloads are likewise variable-sized semantic instances.
  std::vector<SubstitutionRedundancyInstance>
      substitution_redundancy_instances = {};
  // Both vectors retain external literal identities until each atomic apply.
  std::vector<ConditionInstance> condition_instances = {};
  // Stable external vertex sets for IndependentSetCover, one per action.
  std::vector<std::vector<uint32_t>> independent_set_cover_instances = {};
  std::vector<Gf3AffineInstance> gf3_affine_instances = {};
  // Exact GF(2) row/source identities, reauthenticated on apply.
  std::vector<AffineBasisRowSelection> affine_basis_instances = {};
  // Stable opaque replay keys from gf3_gaussian_action_offer().
  std::vector<std::string> gf3_gaussian_instances = {};
  std::vector<std::string> tseitin_cycle_instances = {};
  std::vector<DirectCspSupportActionSelection> direct_csp_support_instances = {};
  std::vector<NaryCspEqualityActionSelection> nary_csp_equality_instances = {};
  std::vector<NaryCspEqualityTransitivitySelection> nary_csp_equality_transitivity_instances = {};
  std::vector<NaryCspEqualityAmoSelection> nary_csp_equality_amo_instances = {};
  std::vector<Gf3GeneralRowActionSelection> gf3_general_row_instances = {};
  // Each selection is one explicitly nominated system of signed count bounds.
  std::vector<std::vector<CardinalitySearchBound>> cardinality_search_instances = {};
  std::vector<DirectCspGuardedCoverActionSelection> direct_csp_guarded_cover_instances = {};
  RepeatSpec repeat;
  PostApplication post_application = PostApplication::None;
  RuleWorkBudget rule_work_budget;
  RuleMaterialSettings material;
  WorkUnit work_unit = WorkUnit::Applications;
  WorkLimit budget = WorkLimit::unlimited();
};

// A separate Propagate may close a settled producer boundary; the serialized
// self-Propagate marker lets replay distinguish this metered fixpoint service
// from an inapplicable Propagate. No state transition.
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

// Composition-only feedback for a shared SR-discovery allowance: a detected
// exact generator may fund more work in the same session up to the hard
// ceiling; zero disables it.
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
// Permanent rule-profile installation (tactix_install_parameters) with an
// online step's rule/family and value checks; instance restrictions belong to
// applications. Walk's permanent profile is its scoring guidance, keeping
// per-application budget, seed and objective defaults. Uses the family's native
// setter and changes no formula, assignment or guidance; false for a family
// without one.
bool validate_parameter_installation(const std::string &rule,
                                     const InprocessParameters &,
                                     std::string *error = nullptr);
bool install_permanent_parameters(Kernel &, const InprocessParameters &);
std::string serialize_online_strategy_program(const OnlineStrategyProgram &);

InvocationStatus enumerate_complete_instances(Kernel &, const RuleVTable &,
                                              std::vector<uint64_t> &,
                                              volatile sig_atomic_t *stop = nullptr);

// Single-threaded online executor: each advance performs at most one atomic
// registered rule application. Configuration is replace-transactional between
// advances and never exposes partial proof or watch mutation. A batch
// invalidated by collection or external mutation reports StaleRequest until
// reconfigured; RetainMatchingStepBudgets keeps spent budgets.
class OnlineStrategyExecutor {
public:
  bool configure(const OnlineStrategyProgram &, uint64_t revision,
                 ReconfigureMode mode = ReconfigureMode::ResetRuntime,
                 std::string *error = nullptr);
  InvocationResult advance(Kernel &, volatile sig_atomic_t *stop = nullptr,
                           Allowance *sr_discovery_allowance = nullptr,
                           DiscoveryFeedbackPolicy discovery_feedback = {});

  uint64_t revision() const { return revision_; }
  const OnlineStrategyProgram &program() const { return program_; }
  size_t step_index() const { return step_index_; }
  uint64_t sequence_passes() const { return sequence_passes_; }
  uint64_t step_consumed(StepId) const;
  bool complete() const { return complete_; }
  // Restart-controller doses can renew composition allowance while keeping an
  // unfinished native cursor; bound state never changes and stale selections
  // are not refreshed.
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
    // Bind payloads to their arena and the last application's state.
    const Kernel *selection_owner = nullptr;
    uint64_t selection_generation = 0;
    StateVersion expected_state;
    std::vector<uint64_t> selection;
    std::vector<FactorGrid> structured_selection;
    std::vector<SubstitutionRedundancyInstance> sr_selection;
    SubstitutionRedundancyGeneralCursor sr_general_cursor;
    bool sr_general_mode = false;
    BveOccurrenceCache bve_occurrences;

    // Drop bound payloads; only a general SR pass keeps its cursor.
    void clear_selection(bool general_sr = false) {
      selection.clear();
      structured_selection.clear();
      sr_selection.clear();
      cursor = 0;
      selection_truncated = false;
      selection_discovery_exhausted = false;
      sr_general_mode = general_sr;
      if (!general_sr)
        sr_general_cursor = {};
    }
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
  bool configured_ = false;
  bool complete_ = false;
  uint64_t revision_ = 0;
};

using OnlineInprocessExecutor = OnlineStrategyExecutor;

} // namespace tactix::strategy
