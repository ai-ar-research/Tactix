// Rule registry (plan §5.3): the ONLY central list of rule families.
// A rule module registers itself with one TACTIX_REGISTER_RULE line; the
// registry is what tactix_rules_applicable / policies / the fuzzer's
// random policy discover rules through. No switch statements anywhere.
#pragma once

#include "kernel/kernel.hpp"

namespace tactix {

// Micro = per-conflict hot path (statically composed in DefaultPolicy);
// Macro = individually scheduled (Restart, Reduce, ...); Inprocess = a
// member of the quiescent-root inprocessing SLOT — DefaultPolicy's
// registry-driven scheduler builds the slot from this tag (rev 26
// schedule-as-data; the tier's reader, plan §5.3)
enum class RuleTier { Micro, Macro, Inprocess };

// ActionKind (plan §4): a registry entry is either a PROOF RULE — its
// effect on σ = ⟨F,R,τ,ρ,π⟩ is non-trivial, so it carries a proof obligation and
// a docs/calculus.md entry — or a POLICY OPERATION: H-only search guidance
// (Rephase, SwitchMode, Walk, Reorder), a sub-policy with no proof delta. BOTH
// live in this one name-sorted registry so ids never split (splitting into two
// id spaces would renumber the frozen v1 API). The kind is metadata
// only: the kernel and DefaultPolicy never read it; it is exposed via
// tactix_action_kind and mirrored to C as TACTIX_ACTION_* (include/tactix.h).
// Values are frozen (they cross the C ABI).
enum class ActionKind { ProofRule = 0, PolicyOp = 1 };

enum class ApplyResult {
  Ok,       // progress made
  NoOp,     // guard held but nothing to do (empty canonical instance)
  Conflict, // conflict found (analyze is now applicable)
  Sat,      // all variables assigned, formula satisfied
  Unsat     // empty clause derived (proof emitted)
};

// Instance payloads (plan §5.1, O3 decided): a rule instance is a
// uint64 whose meaning the rule documents (Decide: DIMACS literal, see
// lit_instance below; Restart: keep-level; Backjump: target level; Reduce:
// victim count). INSTANCE_CANONICAL always denotes the canonical pick.
constexpr uint64_t INSTANCE_CANONICAL = UINT64_MAX;

// --- literal payloads (Decide, Probe, Backbone, TransitiveReduce) ----------
// The DIMACS literal's 32-bit pattern, ZERO-extended (plan §5.1).
// Zero-extension is load-bearing, not a detail: SIGN-extending a negative
// literal into the uint64 maps DIMACS -1 onto 0xFFFF'FFFF'FFFF'FFFF ==
// INSTANCE_CANONICAL, so a policy asking to branch on -1 is silently given the
// canonical pick instead — losing exactly the branching choice N1 exists to
// guarantee. Zero-extension confines the payload space to [1, UINT32_MAX],
// which cannot reach the marker. This encoding lives HERE, beside the marker it
// must stay disjoint from, so the invariant is checkable in one place.
inline uint64_t lit_instance(Lit l) {
  return static_cast<uint64_t>(static_cast<uint32_t>(externalize(l)));
}
// Decode a literal payload; false = not one, so the rule returns NoOp
// (apply_instance() totality). Rejects INSTANCE_CANONICAL and anything else above
// UINT32_MAX (callers test the marker first), 0 (not a DIMACS literal), and
// INT32_MIN (internalize's `-elit` would overflow). The var may still be out of
// range for the kernel — that is the rule's own bounds check.
inline bool lit_of_instance(uint64_t inst, Lit *out) {
  if (inst > UINT32_MAX)
    return false;
  const int32_t elit = static_cast<int32_t>(static_cast<uint32_t>(inst));
  if (elit == 0 || elit == INT32_MIN)
    return false;
  *out = internalize(elit);
  return true;
}

// guard — the SOUNDNESS precondition per the formal model (plan §4).
// Whenever it holds, apply() is total and sound — any policy, any order.
// Guards may encode conflict/handoff state-machine constraints (a
// recorded conflict must be analyzed before τ changes) but NEVER
// scheduling heuristics: WHEN to apply a rule is the Policy object's
// decision and state, exclusively (plan §5).
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
  // proof rule vs H-only policy operation — set by the registration macro
  // (TACTIX_REGISTER_RULE ⇒ ProofRule, TACTIX_REGISTER_POLICY_OP ⇒ PolicyOp),
  // not by the brace-init, so existing registration lines are unchanged.
  ActionKind kind = ActionKind::ProofRule;
  // This rule's id == its index in tactix_rules(). Stamped there after the
  // name-sort, so a vtable reference can charge its own timing slot without the
  // caller threading the id through. Not set by registration.
  uint32_t id = 0;
};

// --- rule-dispatch timing (diagnostic; see Stats' wall-clock block) ---------
// Monotonic nanoseconds. CLOCK_MONOTONIC via the vDSO is ~20 ns, and these
// wrappers only ever fire on registry dispatch — the inprocessing slot, the C
// API, and the fuzz/stress policies — never on the per-conflict search path,
// which DefaultPolicy dispatches statically.
uint64_t now_nanos();

// Apply a rule THROUGH the registry, charging its wall time to the rule's slot.
// Every registry dispatch should go through these two so the accounting covers
// N1's whole surface (any policy, plus tactix_apply/tactix_apply_instance) and
// not merely DefaultPolicy's slot. Timing is pure bookkeeping: the return value
// and every kernel effect are exactly vt.apply's.
ApplyResult apply_rule(const RuleVTable &vt, Kernel &k);
ApplyResult apply_rule_instance(const RuleVTable &vt, Kernel &k, uint64_t inst);

// registration (called from static initializers in rule modules); the macros
// pass the ActionKind, which stamps the vtable before it is stored.
int tactix_register_rule(ActionKind kind, const RuleVTable &);

// stable view: sorted by name once all rules are registered, so rule ids
// are deterministic regardless of link order
const std::vector<RuleVTable> &tactix_rules();
int tactix_rule_id(const char *name); // -1 if unknown

// one line per rule: guard, canonical apply, and optionally the instance
// enumeration/apply pair (plan §5.3)
#define TACTIX_REGISTER_RULE(NAME, TIER, ...) \
  static const int tactix_rule_registered_##NAME = \
      ::tactix::tactix_register_rule( \
          ::tactix::ActionKind::ProofRule, {#NAME, TIER, __VA_ARGS__})

// A policy operation (plan §4): H-only search guidance, no proof
// obligation. Same registry, same name-sorted id space, same guard/apply(/
// instance) contract — only the ActionKind differs, so a rule reclassified in
// place (Rephase, SwitchMode) keeps its id.
#define TACTIX_REGISTER_POLICY_OP(NAME, TIER, ...) \
  static const int tactix_op_registered_##NAME = \
      ::tactix::tactix_register_rule( \
          ::tactix::ActionKind::PolicyOp, {#NAME, TIER, __VA_ARGS__})

} // namespace tactix
