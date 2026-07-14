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

// Instance payloads (plan §5.1, O3 decided rev 20): a rule instance is a
// uint64 whose meaning the rule documents (Decide: DIMACS literal as
// int64; Restart: keep-level; Backjump: target level; Reduce: victim
// count). INSTANCE_CANONICAL always denotes the canonical pick.
constexpr uint64_t INSTANCE_CANONICAL = UINT64_MAX;

// guard — the SOUNDNESS precondition per the formal model (plan §4).
// Whenever it holds, apply() is total and sound — any policy, any order.
// Guards may encode conflict/handoff state-machine constraints (a
// recorded conflict must be analyzed before τ changes) but NEVER
// scheduling heuristics: WHEN to apply a rule is the Policy object's
// decision and state, exclusively (plan §5 rev 17).
struct RuleVTable {
  const char *name;
  RuleTier tier;
  bool (*guard)(const Kernel &);
  ApplyResult (*apply)(Kernel &); // canonical instance
  // instance surface (null for rules without per-application choice —
  // their only instance is the canonical one). instances() fills up to
  // cap payloads ranked canonical-first (lazy: never the whole space);
  // apply_instance() must be total for every payload instances() may
  // yield and clamp/reject anything else.
  uint32_t (*instances)(Kernel &, uint64_t *out, uint32_t cap) = nullptr;
  ApplyResult (*apply_instance)(Kernel &, uint64_t inst) = nullptr;
};

// registration (called from static initializers in rule modules)
int tactix_register_rule(const RuleVTable &);

// stable view: sorted by name once all rules are registered, so rule ids
// are deterministic regardless of link order
const std::vector<RuleVTable> &tactix_rules();
int tactix_rule_id(const char *name); // -1 if unknown

// one line per rule: guard, canonical apply, and optionally the instance
// enumeration/apply pair (plan §5.3 rev 20)
#define TACTIX_REGISTER_RULE(NAME, TIER, ...) \
  static const int tactix_rule_registered_##NAME = \
      ::tactix::tactix_register_rule( \
          {#NAME, TIER, __VA_ARGS__})

} // namespace tactix
