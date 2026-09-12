#pragma once

#include <cstdint>

namespace tactix {
struct Kernel;

namespace strategy {

struct BceParameters {
  uint32_t occurrence_limit = 0;
  constexpr bool valid() const { return true; }
  static BceParameters from_kernel(const Kernel &);
};

struct TernaryParameters {
  uint32_t occurrence_limit = 100;
  uint32_t maximum_add_percent = 1000;
  constexpr bool valid() const { return true; }
  static TernaryParameters from_kernel(const Kernel &);
};

struct FactorParameters {
  uint32_t maximum_clause_size = 5;
  uint32_t growth_bound = 0;
  uint32_t candidate_rounds = 2;
  bool composite = false;
  bool demote_fresh = true;
  constexpr bool valid() const { return true; }
  static FactorParameters from_kernel(const Kernel &);
};

struct TransitiveReduceParameters {
  bool early_exit = true;
  bool live_meter = true;
  bool checked_budget_meter = false;
  constexpr bool valid() const { return true; }
  static TransitiveReduceParameters from_kernel(const Kernel &);
};

struct SubstituteParameters {
  bool demote_substituted = false;
  constexpr bool valid() const { return true; }
  static SubstituteParameters from_kernel(const Kernel &);
};

struct ReduceParameters {
  uint32_t tier1 = 2;
  uint32_t tier2 = 6;
  constexpr bool valid() const { return tier1 <= tier2; }
  static ReduceParameters from_kernel(const Kernel &);
};

struct WalkParameters {
  static constexpr uint64_t STEP_BUDGET_DEFAULT = 1000000;

  // Exact occurrence-list entries to scan. Zero is an explicit zero-work
  // request; omitting WalkParameters retains the canonical reference default.
  uint64_t step_budget = STEP_BUDGET_DEFAULT;
  uint64_t random_seed = 0;
  constexpr bool valid() const { return true; }
  static WalkParameters from_kernel(const Kernel &);
};

struct ReorderParameters {
  static constexpr uint32_t MAXIMUM_CLAUSE_SIZE_DEFAULT = 100;
  uint32_t maximum_clause_size = MAXIMUM_CLAUSE_SIZE_DEFAULT;
  constexpr bool valid() const { return maximum_clause_size >= 2; }
  static ReorderParameters from_kernel(const Kernel &);
};

void install_rule_parameters(Kernel &, const BceParameters &);
void install_rule_parameters(Kernel &, const TernaryParameters &);
void install_rule_parameters(Kernel &, const FactorParameters &);
void install_rule_parameters(Kernel &, const TransitiveReduceParameters &);
void install_rule_parameters(Kernel &, const SubstituteParameters &);
void install_rule_parameters(Kernel &, const ReduceParameters &);
void install_rule_parameters(Kernel &, const WalkParameters &);
void install_rule_parameters(Kernel &, const ReorderParameters &);

template <class Parameters> class ScopedRuleParameters {
public:
  ScopedRuleParameters(Kernel &kernel, const Parameters &parameters)
      : kernel_(kernel), saved_(Parameters::from_kernel(kernel)) {
    install_rule_parameters(kernel_, parameters);
  }
  ScopedRuleParameters(const ScopedRuleParameters &) = delete;
  ScopedRuleParameters &operator=(const ScopedRuleParameters &) = delete;
  ~ScopedRuleParameters() { install_rule_parameters(kernel_, saved_); }

private:
  Kernel &kernel_;
  Parameters saved_;
};

using ScopedBceParameters = ScopedRuleParameters<BceParameters>;
using ScopedTernaryParameters = ScopedRuleParameters<TernaryParameters>;
using ScopedFactorParameters = ScopedRuleParameters<FactorParameters>;
using ScopedTransitiveReduceParameters =
    ScopedRuleParameters<TransitiveReduceParameters>;
using ScopedSubstituteParameters = ScopedRuleParameters<SubstituteParameters>;
using ScopedReduceParameters = ScopedRuleParameters<ReduceParameters>;
using ScopedReorderParameters = ScopedRuleParameters<ReorderParameters>;

class ScopedWalkParameters {
public:
  ScopedWalkParameters(Kernel &, const WalkParameters &);
  ScopedWalkParameters(const ScopedWalkParameters &) = delete;
  ScopedWalkParameters &operator=(const ScopedWalkParameters &) = delete;
  ~ScopedWalkParameters();

private:
  Kernel &kernel_;
  uint64_t saved_budget_;
  uint64_t saved_seed_;
  bool saved_bounded_;
};

} // namespace strategy
} // namespace tactix
