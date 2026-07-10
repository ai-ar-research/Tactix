// Rule registry (plan §5.3): the ONLY central list of rule families.
// A rule module registers itself with one TACTIX_REGISTER_RULE line; the
// registry is what tactix_rules_applicable / policies / the fuzzer's
// random policy discover rules through. No switch statements anywhere.
#pragma once

#include "kernel/kernel.hpp"

namespace tactix {

enum class RuleTier { Micro, Macro };

enum class ApplyResult {
  Ok,       // progress made
  NoOp,     // guard held but nothing to do (empty canonical instance)
  Conflict, // conflict found (analyze is now applicable)
  Sat,      // all variables assigned, formula satisfied
  Unsat     // empty clause derived (proof emitted)
};

struct RuleVTable {
  const char *name;
  RuleTier tier;
  bool (*guard)(const Kernel &);
  ApplyResult (*apply)(Kernel &); // canonical instance
};

// registration (called from static initializers in rule modules)
int tactix_register_rule(const RuleVTable &);

// stable view: sorted by name once all rules are registered, so rule ids
// are deterministic regardless of link order
const std::vector<RuleVTable> &tactix_rules();
int tactix_rule_id(const char *name); // -1 if unknown

#define TACTIX_REGISTER_RULE(NAME, TIER, GUARD, APPLY) \
  static const int tactix_rule_registered_##NAME = \
      ::tactix::tactix_register_rule( \
          {#NAME, TIER, GUARD, APPLY})

} // namespace tactix
