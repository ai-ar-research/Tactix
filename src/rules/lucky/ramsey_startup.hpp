// Bounded original-input certificate adapter and shared startup action.
#pragma once
#include "rules/lucky/ramsey_recognition.hpp"
#include "rules/registry.hpp"
#include <cstdio>
namespace tactix {
struct RamseyStartupRequest {
  ramsey_recovery::Limits recognition;
  uint64_t certificate_work = 0, maximum_nodes = 0, maximum_additions = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t action_id = 0;
  FILE *records = nullptr; // Borrowed semantic/proof provenance; mandatory.
};
struct RamseyStartupResult {
  ApplyResult result = ApplyResult::NoOp;
  bool recognized = false, interrupted = false, exhausted = false;
  uint64_t recognition_work = 0, certificate_work = 0;
  uint64_t nodes = 0, additions = 0, deletions = 0;
  uint32_t vertices = 0, helpers = 0;
};
RuleEligibility ramsey_startup_guard(const Kernel &);
ApplyResult ramsey_startup_action(Kernel &);
RamseyStartupResult ramsey_startup_apply(Kernel &, const RamseyStartupRequest &);
} // namespace tactix
