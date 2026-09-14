#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

#include <functional>
#include <limits>
#include <utility>
#include <vector>

namespace tactix {

struct SubstitutionRedundancyInstance {
  std::vector<Lit> clause;
  std::vector<Lit> witness;
  std::vector<std::pair<Lit, Lit>> substitution;
  bool operator==(const SubstitutionRedundancyInstance &) const = default;
};

struct SubstitutionRedundancyParameters {
  // Work spent validating the SR condition by unit propagation. UINT64_MAX is
  // unbounded; zero is a valid admission limit.
  uint64_t maximum_validation_ticks = UINT64_MAX;
  bool orbitopal_producer = false;
  uint32_t minimum_orbitopal_rows = 2;
  uint32_t minimum_orbitopal_columns = 2;
  uint64_t maximum_orbitopal_matrices = UINT64_MAX;
  uint64_t maximum_orbitopal_instances = UINT64_MAX;
  uint64_t maximum_detection_ticks = UINT64_MAX;
  bool general_producer = false;
  uint64_t maximum_general_candidates_per_advance = UINT64_MAX;

  bool valid() const {
    return !(orbitopal_producer && general_producer) &&
           (!orbitopal_producer ||
            (minimum_orbitopal_rows >= 2 && minimum_orbitopal_columns >= 2));
  }
};

enum class SubstitutionRedundancyApplicationStatus : uint8_t {
  NoChange,
  Applied,
  Unsat,
  Invalid,
  Inapplicable,
  UnsupportedProofFormat,
  Interrupted,
  ResourceExhausted,
};

struct SubstitutionRedundancyApplicationResult {
  SubstitutionRedundancyApplicationStatus status =
      SubstitutionRedundancyApplicationStatus::Invalid;
  ApplyResult rule_result = ApplyResult::NoOp;
  uint64_t validation_ticks = 0;
  uint64_t witness_literals = 0;
};

enum class OrbitopalEnumerationStatus : uint8_t {
  Complete,
  Interrupted,
  LimitReached,
};

struct OrbitopalEnumerationResult {
  OrbitopalEnumerationStatus status = OrbitopalEnumerationStatus::Complete;
  uint64_t matrices = 0;
  uint64_t instances = 0;
  uint64_t ticks = 0;
};

struct SubstitutionRedundancyGeneralCursor {
  const Kernel *owner = nullptr;
  uint64_t arena_generation = 0;
  uint64_t touch_epoch = 0;
  std::vector<Var> variables;
  std::vector<uint32_t> candidate_digits;
  std::vector<uint32_t> witness_digits;
  std::vector<uint32_t> substitution_digits;
  uint32_t pivot_choice = 0;
  bool initialized = false;
  bool complete = false;
};

enum class SubstitutionRedundancyGeneralStatus : uint8_t {
  Found,
  Complete,
  Interrupted,
  ResourceExhausted,
  LimitReached,
};

struct SubstitutionRedundancyGeneralResult {
  SubstitutionRedundancyGeneralStatus status =
      SubstitutionRedundancyGeneralStatus::Complete;
  uint64_t candidates = 0;
};

using OrbitopalInstanceVisitor =
    std::function<bool(const SubstitutionRedundancyInstance &)>;
OrbitopalEnumerationResult substitution_redundancy_enumerate_orbitopal(
    const Kernel &, const SubstitutionRedundancyParameters &,
    const OrbitopalInstanceVisitor &, volatile sig_atomic_t *stop = nullptr);
SubstitutionRedundancyGeneralResult substitution_redundancy_next_general(
    const Kernel &, const SubstitutionRedundancyParameters &,
    SubstitutionRedundancyGeneralCursor &, SubstitutionRedundancyInstance &,
    volatile sig_atomic_t *stop = nullptr);

RuleEligibility substitution_redundancy_guard(const Kernel &);
ApplyResult substitution_redundancy_apply(Kernel &);
SubstitutionRedundancyApplicationResult substitution_redundancy_apply_detailed(
    Kernel &, const SubstitutionRedundancyInstance &,
    const SubstitutionRedundancyParameters & = {},
    volatile sig_atomic_t *stop = nullptr);
ApplyResult substitution_redundancy_apply(
    Kernel &, const SubstitutionRedundancyInstance &,
    const SubstitutionRedundancyParameters & = {},
    volatile sig_atomic_t *stop = nullptr);
ApplyResult substitution_redundancy_apply_orbitopal(
    Kernel &, const SubstitutionRedundancyParameters &,
    volatile sig_atomic_t *stop = nullptr);

} // namespace tactix
