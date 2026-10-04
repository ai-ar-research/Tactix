// Rule registry (plan §5.3): the one central list of rule families. A rule
// module registers itself with a single TACTIX_REGISTER_RULE line, and the
// registry is what tactix_rules_applicable, the policies and the fuzzer's
// random policy discover rules through. No switch statements anywhere.
#pragma once

#include "kernel/kernel.hpp"

#include <functional>
#ifdef TACTIX_ENABLE_RULE_JOURNAL
#include <string>
#include <vector>
#endif

namespace tactix {

// Micro = per-conflict hot path (statically composed in DefaultPolicy);
// Macro = individually scheduled (Restart, Reduce, ...); Inprocess = a member
// of the quiescent-root inprocessing slot, which DefaultPolicy's
// registry-driven scheduler builds from this tag (plan §5.3).
enum class RuleTier { Micro, Macro, Inprocess };

// ActionKind (plan §4): a registry entry is either a proof rule — its effect on
// σ = ⟨F,R,τ,ρ,π⟩ is non-trivial, so it carries a proof obligation and a
// docs/calculus.md entry — or a policy operation: H-only search guidance
// (Rephase, SwitchMode, Walk, Reorder), a sub-policy with no proof delta. Both
// live in this one append-only ID registry so ids never split; splitting into
// two id spaces would renumber the frozen v1 API. The kind is metadata only: the
// kernel and DefaultPolicy never read it, and it is exposed via
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

// The guard is the executable precondition contract.  Its individual facts
// control eligibility and, in a journal build, are serialized from this same
// result.  Thus the journal never reconstructs or duplicates guard logic.
// Production retains only the conjunction; fact names/storage disappear and
// the fluent calls inline to ordinary boolean ANDs.
struct RuleEligibility {
  bool eligible = true;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  struct Fact {
    const char *name;
    bool value;
  };
  std::vector<Fact> facts;
#endif

  RuleEligibility &fact(const char *name, bool value) {
    eligible = eligible && value;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
    facts.push_back({name, value});
#else
    (void)name;
#endif
    return *this;
  }
  operator bool() const { return eligible; }
};

// Instance payloads (plan §5.1): a rule instance is a uint64 whose meaning the
// rule documents (Decide: DIMACS literal; Restart: keep-level; Backjump: target
// level; Reduce: victim count). INSTANCE_CANONICAL denotes the canonical pick.
constexpr uint64_t INSTANCE_CANONICAL = UINT64_MAX;

// --- literal payloads (Decide, Probe, Backbone, TransitiveReduce) ----------
// The DIMACS literal's 32-bit pattern, ZERO-extended (plan §5.1). The
// zero-extension is load-bearing: sign-extending a negative literal maps
// DIMACS -1 onto INSTANCE_CANONICAL, so a policy asking to branch on -1 would
// silently get the canonical pick instead, losing exactly the branching choice
// N1 exists to guarantee. Zero-extension confines the payload space to
// [1, UINT32_MAX], which cannot reach the marker; the encoding lives here,
// beside that marker, so the invariant is checkable in one place.
inline uint64_t lit_instance(Lit l) {
  return static_cast<uint64_t>(static_cast<uint32_t>(externalize(l)));
}
inline uint64_t lit_instance(const Kernel &k, Lit l) {
  return static_cast<uint64_t>(
      static_cast<uint32_t>(kernel_externalize(k, l)));
}
// Decode a literal payload; false = not one, so the rule returns NoOp
// (apply_instance totality). Rejects INSTANCE_CANONICAL and anything else above
// UINT32_MAX, 0 (not a DIMACS literal), and INT32_MIN (internalize's `-elit`
// would overflow). The var may still be out of range for the kernel — that is
// the rule's own bounds check.
inline bool lit_of_instance(uint64_t inst, Lit *out) {
  if (inst > UINT32_MAX)
    return false;
  const int32_t elit = static_cast<int32_t>(static_cast<uint32_t>(inst));
  if (elit == 0 || elit == INT32_MIN)
    return false;
  *out = internalize(elit);
  return true;
}
inline bool lit_of_instance(const Kernel &k, uint64_t inst, Lit *out) {
  Lit external;
  if (!lit_of_instance(inst, &external))
    return false;
  const Var ev = var_of(external);
  if (ev >= k.external_to_internal.size())
    return false;
  const Var iv = k.external_to_internal[ev];
  if (iv == INVALID_VAR)
    return false;
  *out = make_lit(iv, negated(external));
  return true;
}

// `guard` is the soundness precondition of the formal model (plan §4):
// whenever it holds, apply() is total and sound under any policy, any order. A
// guard may encode conflict/handoff state-machine constraints — a recorded
// conflict must be analyzed before τ changes — but never a scheduling
// heuristic: when to apply a rule is the Policy object's decision (plan §5).
struct RuleVTable {
  const char *name;
  RuleTier tier;
  RuleEligibility (*guard)(const Kernel &);
  ApplyResult (*apply)(Kernel &); // canonical instance
  // uint64 instance surface (null for rules with no such choice; Condition
  // instead has a structured tuple through the C API). instances() fills cap
  // payloads ranked canonical-first and is lazy — never the whole space;
  // apply_instance() must
  // be total for every payload instances() may yield, and reject anything else.
  uint32_t (*instances)(Kernel &, uint64_t *out, uint32_t cap) = nullptr;
  ApplyResult (*apply_instance)(Kernel &, uint64_t inst) = nullptr;
  // Additive v2 surface for rules whose legacy payload grouped several
  // semantic choices.  Existing policies keep using the frozen v1 callbacks;
  // flexible/online callers prefer these finer instances when present.
  uint32_t (*semantic_instances)(Kernel &, uint64_t *out,
                                 uint32_t cap) = nullptr;
  ApplyResult (*apply_semantic_instance)(Kernel &, uint64_t inst) = nullptr;
  // Proof rule vs H-only policy operation, set by the registration macro rather
  // than the brace-init so existing registration lines are unchanged.
  ActionKind kind = ActionKind::ProofRule;
  // This rule's id == its index in tactix_rules(), stamped there after the
  // frozen ID ordering so a vtable reference can charge its own timing slot
  // without the caller threading the id through. Not set by registration.
  uint32_t id = 0;
  // Declares explicit instances belonging to the original-input certificate
  // boundary. Choosing any other action closes that boundary for this run.
  bool (*startup_instance)(uint64_t) = nullptr;
};

// --- rule-dispatch timing (diagnostic; see Stats' wall-clock block) ---------
// Monotonic nanoseconds. These wrappers only ever fire on registry dispatch —
// the inprocessing slot, the C API, the fuzz/stress policies — never on the
// per-conflict search path, which DefaultPolicy dispatches statically.
uint64_t now_nanos();

// Apply a rule THROUGH the registry, charging its wall time to the rule's slot.
// Every registry dispatch goes through these two so the accounting covers N1's
// whole surface, not merely DefaultPolicy's slot. Timing is pure bookkeeping:
// the return value and every kernel effect are exactly vt.apply's.
ApplyResult apply_rule(const RuleVTable &vt, Kernel &k);
ApplyResult apply_rule_instance(const RuleVTable &vt, Kernel &k, uint64_t inst);
ApplyResult apply_rule_semantic_instance(const RuleVTable &vt, Kernel &k,
                                         uint64_t inst);
ApplyResult apply_rule_structured(const RuleVTable &, Kernel &,
                                  const char *description,
                                  const std::function<ApplyResult()> &);
// Structured actions that authenticate and commit their own startup-boundary
// transition use this wrapper so recoverable declines stay mutation-free.
ApplyResult apply_rule_structured_preserve_startup(
    const RuleVTable &, Kernel &, const char *description,
    const std::function<ApplyResult()> &);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
// DefaultPolicy statically composes hot micro/macro calls in production.  A
// journal build brackets those exact calls through this diagnostic-only
// adapter, so it records the real chosen payload without changing production
// dispatch or introducing a hot-path branch there.
ApplyResult journal_direct_rule(Kernel &, const char *name,
                                const std::string &candidate,
                                const std::function<ApplyResult()> &apply,
                                ApplyResult declined_result =
                                    ApplyResult::NoOp);
#endif

// registration (called from static initializers in rule modules); the macros
// pass the ActionKind, which stamps the vtable before it is stored.
int tactix_register_rule(ActionKind kind, const RuleVTable &);

// Stable view: the original name-sorted IDs are frozen, and new actions must
// be appended to the explicit order in registry.cpp, regardless of link order.
const std::vector<RuleVTable> &tactix_rules();
int tactix_rule_id(const char *name); // -1 if unknown

// one line per rule: guard, canonical apply, and optionally the instance
// enumeration/apply pair (plan §5.3)
#define TACTIX_REGISTER_RULE(NAME, TIER, ...) \
  static const int tactix_rule_registered_##NAME = \
      ::tactix::tactix_register_rule( \
          ::tactix::ActionKind::ProofRule, {#NAME, TIER, __VA_ARGS__})

// A policy operation (plan §4): H-only search guidance, no proof obligation.
// Same registry, same append-only id space, same guard/apply(/instance)
// contract — only the ActionKind differs, so a rule reclassified in place
// keeps its id.
#define TACTIX_REGISTER_POLICY_OP(NAME, TIER, ...) \
  static const int tactix_op_registered_##NAME = \
      ::tactix::tactix_register_rule( \
          ::tactix::ActionKind::PolicyOp, {#NAME, TIER, __VA_ARGS__})

} // namespace tactix
