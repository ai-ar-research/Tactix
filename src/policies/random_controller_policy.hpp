#pragma once

#include "policies/policy.hpp"
#include "policies/controller_trace.hpp"
#include "policies/restart_controller.hpp"
#include "strategy/random_online.hpp"

#include <map>

namespace tactix {

struct RandomControllerConfig {
  DefaultStrategyConfig search;
  // The native ranked-prefix mechanism is shared with DefaultPolicy. Full
  // restart remains an explicit profile for root-boundary coverage/embeddings.
  bool reuse_search_trail = true;
  strategy::RandomOnlineConfig parameters;
  // Caller-provided semantic choices, including structured Condition/SR/Factor
  // payloads. These supplement native producers without hiding API capability.
  std::map<std::string, std::vector<strategy::OnlineInvocation>> offered_actions;
  strategy::controller::ActionAllowance default_allowance{
      strategy::WorkUnit::Applications, 1,
      {strategy::RuleWorkBudgetMode::Bounded,
       DefaultStrategyConfig::INPROCESS_MINIMUM_EFFORT_DEFAULT}};
  std::map<std::string, strategy::controller::ActionAllowance> allowances;
  // Treat a typed program's steps as a collection of offered alternatives,
  // not as an ordered mandatory sequence. Replacement is transactional.
  bool configure_actions(const strategy::OnlineStrategyProgram &,
                         std::string *error = nullptr);

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
  uint64_t draw(uint64_t bound);
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
