// Rule registry (plan §5.3): the central list of rule families, registered with
// TACTIX_REGISTER_RULE and discovered by the API, policies and fuzzer.
#pragma once

#include "kernel/kernel.hpp"

#include <functional>
#ifdef TACTIX_ENABLE_RULE_JOURNAL
#include <string>
#include <vector>
#endif

namespace tactix {

// Micro = per-conflict hot path; Macro = individually scheduled; Inprocess =
// member of DefaultPolicy's registry-driven inprocessing slot (plan §5.3).
enum class RuleTier { Micro, Macro, Inprocess };

// ActionKind (plan §4): a proof rule (non-trivial effect on σ, proof
// obligation, calculus entry) or a policy operation (H-only guidance). One
// append-only id space; metadata only, exposed as tactix_action_kind /
// TACTIX_ACTION_*. Values are frozen (C ABI).
enum class ActionKind { ProofRule = 0, PolicyOp = 1 };

enum class ApplyResult {
  Ok,       // progress made
  NoOp,     // guard held but nothing to do (empty canonical instance)
  Conflict, // conflict found (analyze is now applicable)
  Sat,      // all variables assigned, formula satisfied
  Unsat     // empty clause derived (proof emitted)
};

// The guard is the executable precondition contract; a journal build
// serializes its individual facts, production keeps only the conjunction.
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

// Instance payloads (plan §5.1): a uint64 the rule documents.
// INSTANCE_CANONICAL denotes the canonical pick.
constexpr uint64_t INSTANCE_CANONICAL = UINT64_MAX;

// --- literal payloads (Decide, Probe, Backbone, TransitiveReduce) ---
// The DIMACS literal's 32-bit pattern, zero-extended (plan §5.1): sign
// extension would map -1 onto INSTANCE_CANONICAL.
inline uint64_t lit_instance(Lit l) {
  return static_cast<uint64_t>(static_cast<uint32_t>(externalize(l)));
}
inline uint64_t lit_instance(const Kernel &k, Lit l) {
  return static_cast<uint64_t>(
      static_cast<uint32_t>(kernel_externalize(k, l)));
}
// Decode a literal payload; false = not one (rule returns NoOp). Rejects values
// above UINT32_MAX, 0 and INT32_MIN; the var range is the rule's check.
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

// --- clause-literal payloads (Instantiate, Strengthen) ---
// Arena Ref in the high word, internal literal in the low word; a snapshot.
inline uint64_t clause_literal_instance(Ref clause, Lit literal) {
  return (static_cast<uint64_t>(clause) << 32) |
         static_cast<uint32_t>(literal);
}
inline void clause_literal_of_instance(uint64_t instance, Ref *clause,
                                       Lit *literal) {
  *clause = static_cast<Ref>(instance >> 32);
  *literal = static_cast<Lit>(static_cast<uint32_t>(instance));
}

// `guard` is the soundness precondition (plan §4): when it holds, apply() is
// total and sound under any policy and order. It may encode conflict/handoff
// state constraints but never a scheduling heuristic (plan §5).
struct RuleVTable {
  const char *name;
  RuleTier tier;
  RuleEligibility (*guard)(const Kernel &);
  ApplyResult (*apply)(Kernel &); // canonical instance
  // uint64 instance surface (null without such a choice). instances() fills up
  // to cap payloads, canonical-first; apply_instance() is total over them and
  // rejects anything else.
  uint32_t (*instances)(Kernel &, uint64_t *out, uint32_t cap) = nullptr;
  ApplyResult (*apply_instance)(Kernel &, uint64_t inst) = nullptr;
  // Additive v2 surface with finer semantic instances.
  uint32_t (*semantic_instances)(Kernel &, uint64_t *out,
                                 uint32_t cap) = nullptr;
  ApplyResult (*apply_semantic_instance)(Kernel &, uint64_t inst) = nullptr;
  // Proof rule vs H-only policy operation, set by the registration macro rather
  // than the brace-init so existing registration lines are unchanged.
  ActionKind kind = ActionKind::ProofRule;
  // Index in tactix_rules(), stamped there; not set by registration.
  uint32_t id = 0;
  // Declares explicit instances belonging to the original-input certificate
  // boundary. Choosing any other action closes that boundary for this run.
  bool (*startup_instance)(uint64_t) = nullptr;
  // Explicit capability for DefaultPolicy's registry-driven append.
  bool default_inprocess = true;
  // Frozen registry holes are observable but cannot be selected or dispatched.
  bool retired = false;
};

// --- rule-dispatch timing (diagnostic) ---
// Monotonic nanoseconds; only registry dispatch is timed, never the
// statically dispatched search path.
uint64_t now_nanos();

// Apply a rule through the registry, charging its wall time to its slot; the
// result and effects are exactly vt.apply's.
ApplyResult apply_rule(const RuleVTable &vt, Kernel &k);
ApplyResult apply_rule_instance(const RuleVTable &vt, Kernel &k, uint64_t inst);
ApplyResult apply_rule_semantic_instance(const RuleVTable &vt, Kernel &k,
                                         uint64_t inst);
ApplyResult apply_rule_structured(const RuleVTable &, Kernel &,
                                  const char *description,
                                  const std::function<ApplyResult()> &);
// Leaves a staged semantic-variable candidate in Kernel; the caller publishes
// it with finish_semantic_variable_candidate_action or discards it with
// begin_semantic_variable_candidate_action.
ApplyResult apply_rule_structured_defer_candidate(
    const RuleVTable &, Kernel &, const char *description,
    const std::function<ApplyResult()> &);
// Structured actions that authenticate and commit their own startup-boundary
// transition use this wrapper so recoverable declines stay mutation-free.
ApplyResult apply_rule_structured_preserve_startup(
    const RuleVTable &, Kernel &, const char *description,
    const std::function<ApplyResult()> &);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
// Journal-build adapter around DefaultPolicy's statically composed calls.
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
// Registry-wide legality, including cross-rule state-machine debts such as a
// deferred affine reason that must be materialized before any reason consumer.
RuleEligibility rule_guard(const RuleVTable &, const Kernel &);

// one line per rule: guard, canonical apply, and optionally the instance
// enumeration/apply pair (plan §5.3)
#define TACTIX_REGISTER_RULE(NAME, TIER, ...) \
  static const int tactix_rule_registered_##NAME = \
      ::tactix::tactix_register_rule( \
          ::tactix::ActionKind::ProofRule, {#NAME, TIER, __VA_ARGS__})

// An explicit strategy action, excluded from the default inprocessing append.
#define TACTIX_REGISTER_OPT_IN_RULE(NAME, TIER, ...) \
  static const int tactix_rule_registered_##NAME = \
      ::tactix::tactix_register_rule(::tactix::ActionKind::ProofRule, [] { \
        ::tactix::RuleVTable rule{#NAME, TIER, __VA_ARGS__}; \
        rule.default_inprocess = false; \
        return rule; \
      }())

// A policy operation (plan §4): same registry, id space and contract; only the
// ActionKind differs.
#define TACTIX_REGISTER_POLICY_OP(NAME, TIER, ...) \
  static const int tactix_op_registered_##NAME = \
      ::tactix::tactix_register_rule( \
          ::tactix::ActionKind::PolicyOp, {#NAME, TIER, __VA_ARGS__})

} // namespace tactix
