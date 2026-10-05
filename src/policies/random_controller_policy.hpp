#pragma once

#include "policies/policy.hpp"
#include "policies/controller_trace.hpp"
#include "policies/restart_controller.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/nary_csp_prefix_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"
#include "rules/lucky/tseitin_cycle_action.hpp"
#include "strategy/random_online.hpp"

#include <map>
#include <optional>

namespace tactix {

// Native structured-action discovery is an opt-in controller facility.  The
// producer window is separate from the rule's semantic/proof limits: one
// selection call inspects at most one such page, while the exact returned
// payload is still governed by the typed reference parameters below.
struct Gf3ActionProducerConfig {
  bool enabled = false;
  uint64_t maximum_pair_candidates = 64;
  uint64_t maximum_offers = 8;
  uint64_t maximum_enumeration_work = 100000;

  bool valid() const {
    return !enabled || (maximum_pair_candidates && maximum_offers &&
                        maximum_enumeration_work);
  }
};

struct AffineBasisActionProducerConfig {
  bool enabled = false;
  uint32_t maximum_offers = 8;
  bool valid() const { return !enabled || maximum_offers; }
};

struct DirectCspActionProducerConfig {
  bool enabled = false;
  uint64_t maximum_domain_candidates = 64;
  uint64_t maximum_pair_candidates = 64;
  uint64_t maximum_row_candidates = 64;
  uint64_t maximum_offers = 8;
  uint64_t maximum_enumeration_work = 100000;

  bool valid() const {
    return !enabled ||
           (maximum_domain_candidates && maximum_pair_candidates &&
            maximum_row_candidates && maximum_offers &&
            maximum_enumeration_work);
  }
};

struct NaryCspActionProducerConfig {
  bool enabled = false;
  uint32_t relation_arity = 3;
  // noL has 363 exact-one colour domains. Discovery is clause-indexed, so
  // admitting their census no longer implies enumerating domain triples.
  uint64_t maximum_domain_candidates = 512;
  uint64_t maximum_relation_candidates = 64;
  uint64_t maximum_prefix_candidates = 1024;
  uint64_t maximum_offers = 8;
  uint64_t maximum_enumeration_work = 10000000;

  bool valid() const {
    return !enabled ||
           (relation_arity >= 3 && maximum_domain_candidates &&
            maximum_relation_candidates && maximum_prefix_candidates &&
            maximum_offers && maximum_enumeration_work);
  }
};

struct Gf3GaussianActionProducerConfig {
  bool enabled = false;
  bool valid() const { return true; }
};

struct TseitinCycleActionProducerConfig {
  bool enabled = false;
  bool valid() const { return true; }
};

struct RandomControllerConfig {
  DefaultStrategyConfig search;
  // The native ranked-prefix mechanism is shared with DefaultPolicy. Full
  // restart remains an explicit profile for root-boundary coverage/embeddings.
  bool reuse_search_trail = true;
  strategy::RandomOnlineConfig parameters;
  // Caller-provided semantic choices, including structured Condition/SR/Factor
  // payloads.  A non-empty structured family takes precedence over the native
  // producer below, making an explicit collection exact and replay-stable.
  std::map<std::string, std::vector<strategy::OnlineInvocation>> offered_actions;
  Gf3ActionProducerConfig gf3_action_producer;
  AffineBasisActionProducerConfig affine_basis_action_producer;
  DirectCspActionProducerConfig direct_csp_action_producer;
  NaryCspActionProducerConfig nary_csp_action_producer;
  Gf3GaussianActionProducerConfig gf3_gaussian_action_producer;
  TseitinCycleActionProducerConfig tseitin_cycle_action_producer;
  strategy::controller::ActionAllowance default_allowance{
      strategy::WorkUnit::Applications, 1,
      {strategy::RuleWorkBudgetMode::Bounded,
       DefaultStrategyConfig::INPROCESS_MINIMUM_EFFORT_DEFAULT}};
  std::map<std::string, strategy::controller::ActionAllowance> allowances;
  // Treat a typed program's steps as a collection of offered alternatives,
  // not as an ordered mandatory sequence. Replacement is transactional.
  bool configure_actions(const strategy::OnlineStrategyProgram &,
                         std::string *error = nullptr);
  // Install the typed limits and fixed dose for native GF(2) row discovery.
  // The profile never becomes a static action: exact rows remain live,
  // producer-owned payloads and the replacement is transactional.
  bool configure_affine_basis_producer_profile(
      const strategy::OnlineStrategyProgram &, std::string *error = nullptr);

  strategy::controller::ActionAllowance allowance(const std::string &rule) const {
    const auto found = allowances.find(rule);
    if (found != allowances.end())
      return found->second;
    auto result = default_allowance;
    // These two shared adapters require their existing native composition
    // currencies. The dose remains fixed and one advance stays atomic.
    if (rule == "BVE") {
      result.unit = strategy::WorkUnit::BveTicks;
      result.amount = parameters.maximum_bve_tick_budget;
    } else if (rule == "Propagate") {
      result.unit = strategy::WorkUnit::Propagations;
      result.amount = parameters.maximum_propagation_budget;
    }
    return result;
  }
};

struct ControllerActionAvailability {
  std::string rule;
  bool eligible = false;
  std::string reason;
};

std::vector<ControllerActionAvailability>
controller_action_availability(const Kernel &, const RandomControllerConfig &);
std::string controller_configuration(const RandomControllerConfig &);

struct ControllerRuleCoverage {
  uint64_t requests = 0, applications = 0, effective = 0;
  uint64_t exhausted = 0, stale = 0, interrupted = 0, work = 0, overshoot = 0;
  uint64_t producer_calls = 0, producer_work = 0;
  uint64_t producer_candidates = 0, producer_offers = 0;
};
struct ControllerSearchCoverage {
  uint64_t requests = 0, intervals = 0, decisions = 0, conflicts = 0;
  uint64_t continuations = 0;
};
struct ControllerCoverage {
  std::map<std::string, ControllerRuleCoverage> rules;
  std::map<std::string, ControllerSearchCoverage> search;
  uint64_t full_restarts = 0, configuration_changes = 0;
  uint64_t partial_restarts = 0;
  uint64_t maximum_root_actions = 0;
};

// Selection may perform bounded native discovery; observation and feedback
// remain value-only. This seam is called at root, outside static search dispatch.
class ControllerSelection {
public:
  virtual ~ControllerSelection() = default;
  virtual bool choose(Kernel &, const strategy::controller::Observation &,
                      const RandomControllerConfig &,
                      strategy::controller::ActionRequest &, std::string &error,
                      volatile sig_atomic_t *stop = nullptr) = 0;
};

// Seeded root-action chooser. Action generation is separate from observation:
// semantic instance enumeration may perform discovery and consume randomness.
class RandomControllerSelector final : public ControllerSelection {
public:
  explicit RandomControllerSelector(uint64_t seed) : rng_(seed ? seed : 1) {}
  bool choose(Kernel &, const strategy::controller::Observation &,
              const RandomControllerConfig &,
              strategy::controller::ActionRequest &, std::string &error,
              volatile sig_atomic_t *stop = nullptr) override;

private:
  struct AffineBasisDiscoveryState {
    const Kernel *owner = nullptr;
    const AffineBasisActionSession *session = nullptr;
    strategy::StateVersion state;
    size_t session_keys = 0;
    bool terminal = false;
    bool reconfiguration_required = false;
    bool more = false;
    std::string profile;
    AffineBasisOfferCursor cursor;
    std::vector<AffineBasisRowSelection> offers;
    std::optional<AffineBasisRowSelection> pending;
    uint64_t pending_sequence = 0;
  };
  struct Gf3DiscoveryState {
    const Kernel *owner = nullptr;
    const Gf3AffineActionSession *session = nullptr;
    strategy::StateVersion state;
    size_t session_keys = 0;
    bool terminal = false;
    bool reconfiguration_required = false;
    bool complete = false;
    bool have_after = false;
    std::string profile;
    Gf3AffineActionKey after;
    std::vector<Gf3AffineActionKey> offers;
    std::optional<Gf3AffineActionKey> pending;
    uint64_t pending_sequence = 0;
  };
  struct DirectCspDiscoveryState {
    const Kernel *owner = nullptr;
    const DirectCspSupportActionSession *session = nullptr;
    strategy::StateVersion state;
    size_t session_keys = 0;
    size_t retained_owners = 0;
    bool terminal = false;
    bool reconfiguration_required = false;
    bool complete = false;
    bool have_after = false;
    std::string profile;
    DirectCspSupportActionKey after;
    std::vector<DirectCspSupportActionSelection> offers;
    std::optional<DirectCspSupportActionSelection> pending;
    uint64_t pending_sequence = 0;
  };
  struct Gf3GaussianDiscoveryState {
    const Kernel *owner = nullptr;
    const Gf3GaussianActionSession *session = nullptr;
    strategy::StateVersion state;
    size_t session_keys = 0;
    bool terminal = false;
    bool reconfiguration_required = false;
    std::string profile;
    std::optional<std::string> offer;
    std::optional<std::string> pending;
    uint64_t pending_sequence = 0;
  };
  struct NaryCspDiscoveryState {
    const Kernel *owner = nullptr;
    const NaryCspPrefixActionSession *session = nullptr;
    strategy::StateVersion state;
    size_t session_keys = 0;
    size_t retained_owners = 0;
    bool terminal = false;
    bool reconfiguration_required = false;
    bool complete = false;
    bool have_after = false;
    std::string profile;
    NaryCspPrefixActionKey after;
    std::vector<NaryCspPrefixActionSelection> offers;
    std::optional<NaryCspPrefixActionSelection> pending;
    uint64_t pending_sequence = 0;
  };
  struct TseitinCycleDiscoveryState {
    const Kernel *owner = nullptr;
    const TseitinCycleBreakingSession *session = nullptr;
    strategy::StateVersion state;
    size_t session_keys = 0;
    bool terminal = false;
    bool reconfiguration_required = false;
    std::string profile;
    std::vector<std::string> offers;
    std::optional<std::string> pending;
    uint64_t pending_sequence = 0;
  };

  uint64_t draw(uint64_t bound);
  AffineBasisDiscoveryState affine_basis_discovery_;
  Gf3DiscoveryState gf3_discovery_;
  DirectCspDiscoveryState direct_csp_discovery_;
  NaryCspDiscoveryState nary_csp_discovery_;
  Gf3GaussianDiscoveryState gf3_gaussian_discovery_;
  TseitinCycleDiscoveryState tseitin_cycle_discovery_;
  uint64_t rng_;
};

class RandomControllerPolicy final : public Policy {
public:
  explicit RandomControllerPolicy(uint64_t seed,
                                 RandomControllerConfig config = {},
                                 ControllerTrace *trace = nullptr,
                                 ControllerSelection *selection = nullptr,
                                 strategy::controller::FeedbackReceiver *feedback = nullptr)
      : seed_(seed), config_(std::move(config)), trace_(trace),
        selection_(selection), feedback_(feedback) {}
  int solve(Kernel &, volatile sig_atomic_t *stop = nullptr) override;
  void set_feedback(strategy::controller::FeedbackReceiver *feedback) { feedback_ = feedback; }
  const ControllerCoverage &coverage() const { return coverage_; }

private:
  uint64_t seed_;
  RandomControllerConfig config_;
  ControllerTrace *trace_;
  ControllerCoverage coverage_;
  ControllerSelection *selection_;
  strategy::controller::FeedbackReceiver *feedback_;
};

} // namespace tactix
