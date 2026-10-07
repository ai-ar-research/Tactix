#include "rules/registry.hpp"
#include "diagnostics/event_trace.hpp"
#include "diagnostics/rule_journal.hpp"
#include "strategy/action_execution.hpp"
#include "strategy/semantic_variable_advice.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/nary_csp_prefix_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_transitivity_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_amo_action.hpp"
#include "rules/semantic_extension/gf3_general_row_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"
#include "rules/direct_php/direct_php_action.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"
#include "rules/lucky/tseitin_cycle_action.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace tactix {

namespace {

ApplyResult gf3_affine_canonical_noop(Kernel &) { return ApplyResult::NoOp; }
ApplyResult direct_csp_support_canonical_noop(Kernel &) {
  return ApplyResult::NoOp;
}
ApplyResult gf3_gaussian_canonical_noop(Kernel &) { return ApplyResult::NoOp; }
ApplyResult tseitin_cycle_breaking_canonical_noop(Kernel &) {
  return ApplyResult::NoOp;
}
ApplyResult nary_csp_prefix_canonical_noop(Kernel &) {
  return ApplyResult::NoOp;
}
ApplyResult nary_csp_equality_canonical_noop(Kernel &) {
  return ApplyResult::NoOp;
}
ApplyResult nary_csp_equality_amo_canonical_noop(Kernel &) {
  return ApplyResult::NoOp;
}

DirectPhpActionLimits direct_php_limits(const Kernel &kernel) {
  const auto &p = kernel.direct_php_refutation_parameters;
  return {{p.maximum_snapshot_clauses, p.maximum_snapshot_literals,
           p.maximum_snapshot_work},
          {p.maximum_recognition_clauses, p.maximum_recognition_literals,
           p.maximum_recognition_work, p.maximum_row_candidates,
           p.maximum_row_combinations, p.maximum_mapping_trials,
           p.maximum_holes},
          {p.maximum_proof_additions, p.maximum_proof_deletions,
           p.maximum_proof_literals, p.maximum_fresh_variables,
           p.maximum_proof_events},
          p.maximum_replay_work};
}

ApplyResult direct_php_canonical_apply(Kernel &kernel) {
  if (!kernel.direct_php_action_session) return ApplyResult::NoOp;
  const DirectPhpActionRequest request{1, 1, direct_php_limits(kernel)};
  const auto result = direct_php_action_apply(
      kernel, *kernel.direct_php_action_session, request);
  record_direct_php_action_feedback(kernel, result);
  return result.status == DirectPhpActionStatus::Applied
             ? ApplyResult::Unsat : ApplyResult::NoOp;
}

RuleEligibility gf3_affine_registry_guard(const Kernel &kernel) {
  RuleEligibility result = gf3_affine_action_guard(kernel);
  return result.fact("session-bound",
                     kernel.gf3_affine_action_session != nullptr);
}

RuleEligibility direct_csp_support_registry_guard(const Kernel &kernel) {
  RuleEligibility result = direct_csp_support_action_guard(kernel);
  return result.fact("session-bound",
                     kernel.direct_csp_support_action_session != nullptr);
}

RuleEligibility direct_php_registry_guard(const Kernel &kernel) {
  RuleEligibility result = direct_php_action_guard(kernel);
  return result
      .fact("session-bound", kernel.direct_php_action_session &&
                                 kernel.direct_php_action_session->belongs_to(kernel))
      .fact("parameters-valid", direct_php_limits(kernel).valid());
}

RuleEligibility gf3_gaussian_registry_guard(const Kernel &kernel) {
  RuleEligibility result = gf3_gaussian_action_guard(kernel);
  return result.fact("session-bound",
                     kernel.gf3_gaussian_action_session != nullptr);
}

RuleEligibility tseitin_cycle_breaking_registry_guard(const Kernel &kernel) {
  RuleEligibility result = tseitin_cycle_breaking_guard(kernel);
  return result.fact("session-bound",
                     kernel.tseitin_cycle_breaking_session != nullptr);
}

RuleEligibility nary_csp_prefix_registry_guard(const Kernel &kernel) {
  RuleEligibility result = nary_csp_prefix_action_guard(kernel);
  return result.fact("session-bound",
                     kernel.nary_csp_prefix_action_session != nullptr);
}

RuleEligibility nary_csp_equality_registry_guard(const Kernel &kernel) {
  RuleEligibility result = nary_csp_equality_action_guard(kernel);
  return result
      .fact("relation-session-bound",
            kernel.nary_csp_relation_owner_session != nullptr)
      .fact("equality-session-bound",
            kernel.nary_csp_equality_session != nullptr);
}

RuleEligibility nary_csp_equality_amo_registry_guard(const Kernel &kernel) {
  RuleEligibility result = nary_csp_equality_amo_guard(kernel);
  return result
      .fact("relation-session-bound",
            kernel.nary_csp_relation_owner_session != nullptr)
      .fact("equality-session-bound",
            kernel.nary_csp_equality_session != nullptr)
      .fact("amo-session-bound",
            kernel.nary_csp_equality_amo_session != nullptr);
}

ApplyResult nary_csp_equality_transitivity_canonical_noop(Kernel &) { return ApplyResult::NoOp; }
RuleEligibility nary_csp_equality_transitivity_registry_guard(const Kernel &k) {
 return nary_csp_equality_transitivity_guard(k).fact("relation-session-bound", k.nary_csp_relation_owner_session != nullptr).fact("equality-session-bound", k.nary_csp_equality_session != nullptr).fact("transitivity-session-bound", k.nary_csp_equality_transitivity_session != nullptr);
}

ApplyResult gf3_general_row_canonical_noop(Kernel &) {
  return ApplyResult::NoOp;
}

RuleEligibility gf3_general_row_registry_guard(const Kernel &kernel) {
  RuleEligibility result = gf3_general_row_action_guard(kernel);
  return result.fact("session-bound",
                     kernel.gf3_general_row_action_session != nullptr);
}

TACTIX_REGISTER_RULE(Gf3AffineConsequence, RuleTier::Inprocess,
                     gf3_affine_registry_guard, gf3_affine_canonical_noop);
TACTIX_REGISTER_RULE(DirectCspSupport, RuleTier::Inprocess,
                     direct_csp_support_registry_guard,
                     direct_csp_support_canonical_noop);
TACTIX_REGISTER_RULE(DirectPhpRefutation, RuleTier::Inprocess,
                     direct_php_registry_guard, direct_php_canonical_apply);
TACTIX_REGISTER_RULE(Gf3GaussianConsequence, RuleTier::Inprocess,
                     gf3_gaussian_registry_guard,
                     gf3_gaussian_canonical_noop);
TACTIX_REGISTER_RULE(TseitinCycleBreaking, RuleTier::Inprocess,
                     tseitin_cycle_breaking_registry_guard,
                     tseitin_cycle_breaking_canonical_noop);
TACTIX_REGISTER_OPT_IN_RULE(NaryCspPrefix, RuleTier::Inprocess,
                            nary_csp_prefix_registry_guard,
                            nary_csp_prefix_canonical_noop);
TACTIX_REGISTER_RULE(NaryCspEquality, RuleTier::Inprocess,
                     nary_csp_equality_registry_guard,
                     nary_csp_equality_canonical_noop);
TACTIX_REGISTER_RULE(Gf3GeneralRowConsequence, RuleTier::Inprocess,
                     gf3_general_row_registry_guard,
                     gf3_general_row_canonical_noop);
TACTIX_REGISTER_RULE(NaryCspEqualityTransitivity, RuleTier::Inprocess, nary_csp_equality_transitivity_registry_guard, nary_csp_equality_transitivity_canonical_noop);

TACTIX_REGISTER_RULE(NaryCspEqualityAmo, RuleTier::Inprocess,
                     nary_csp_equality_amo_registry_guard,
                     nary_csp_equality_amo_canonical_noop);

} // namespace

#ifdef TACTIX_ENABLE_RULE_JOURNAL
namespace {

const char *journal_outcome(ApplyResult result) {
  switch (result) {
  case ApplyResult::Ok:
    return "applied";
  case ApplyResult::NoOp:
    return "no-op";
  case ApplyResult::Conflict:
    return "conflict";
  case ApplyResult::Sat:
    return "sat";
  case ApplyResult::Unsat:
    return "refuted";
  }
  return "unknown";
}

void journal_dispatch(const RuleVTable &vt, const Kernel &k,
                      const char *candidate, uint64_t pre_digest,
                      uint64_t formula_epoch, uint64_t root_epoch,
                      const RuleEligibility &eligibility, ApplyResult result,
                      uint64_t effort_before) {
  if (vt.kind != ActionKind::ProofRule)
    return;
  RuleJournalRecord record;
  record.rule = vt.name;
  record.event = "rule-attempt";
  record.candidate = candidate;
  record.trigger = "registry-dispatch";
  record.context = "tier=" + std::to_string(static_cast<unsigned>(vt.tier)) +
                   ";level=" + std::to_string(k.level) +
                   ";pending=" + std::to_string(k.pending.valid) +
                   ";conflict=" + std::to_string(k.conflict_valid);
  record.formula_epoch = formula_epoch;
  record.root_epoch = root_epoch;
  record.pre_digest = pre_digest;
  record.guards.clear();
  for (const RuleEligibility::Fact &fact : eligibility.facts) {
    if (!record.guards.empty())
      record.guards.push_back(';');
    record.guards += fact.name;
    record.guards.push_back('=');
    record.guards.push_back(fact.value ? '1' : '0');
  }
  record.outcome = eligibility ? journal_outcome(result) : "declined";
  record.decline = "none";
  if (!eligibility) {
    for (const RuleEligibility::Fact &fact : eligibility.facts)
      if (!fact.value) {
        record.decline = std::string("guard:") + fact.name;
        break;
      }
  }
  const uint64_t post_digest = rule_journal_digest(k);
  record.deltas = rule_journal_transaction_deltas();
  if (record.deltas == "none" && pre_digest != post_digest)
    record.deltas = "state-digest-change";
  record.antecedents = rule_journal_transaction_antecedents();
  record.witnesses = rule_journal_transaction_witnesses();
  record.effort = rule_journal_effort(k) - effort_before;
  record.budget = k.work_budget;
  record.post_digest = post_digest;
  rule_journal_emit(k, record);
}

} // namespace
#endif

static std::vector<RuleVTable> &mutable_rules() {
  static std::vector<RuleVTable> rules;
  return rules;
}

int tactix_register_rule(ActionKind kind, const RuleVTable &vt) {
  RuleVTable e = vt;
  e.kind = kind;
  mutable_rules().push_back(e);
  return static_cast<int>(mutable_rules().size()) - 1;
}

const std::vector<RuleVTable> &tactix_rules() {
  static const std::vector<RuleVTable> ordered = [] {
    // Public IDs 0..27 were originally assigned by name sort. They are now
    // frozen: append each future action here instead of inserting it into
    // alphabetical order and silently renumbering the C API.
    static constexpr const char *names[] = {
        "BCE",         "BVE",              "Backbone",
        "CCE",         "ConflictAnalysis", "Congruence",
        "Decide",      "Factor",           "HyperBinaryRes",
        "HyperTernaryRes", "Instantiate",  "LocalSearchGuidance",
        "Lucky",       "Probe",            "Propagate",
        "Reduce",      "Reorder",          "Rephase",
        "Restart",     "Strengthen",       "Substitute",
        "SubstitutionRedundancy", "SubsumeForward", "Sweep",
        "SwitchMode",  "TransitiveReduce", "Vivify",
        "Walk",        "Condition", "ParityConsequence",
        "AffineConsequence", "ExactCoverExtension", "IndependentSetCover",
        "SemanticVariableAdvice", "Gf3AffineConsequence",
        "DirectCspSupport", "DirectPhpRefutation",
        "Gf3GaussianConsequence", "TseitinCycleBreaking", "NaryCspPrefix",
        "GraphIntervalEquivalence", "AffineMatrixInitialize",
        "NaryCspEquality", "Gf3GeneralRowConsequence",
        "NaryCspEqualityAmo", "NaryCspEqualityTransitivity"};
    const auto &registered = mutable_rules();
    constexpr size_t count = sizeof names / sizeof *names;
    if (registered.size() != count) {
      std::fprintf(stderr,
                   "c ERROR: registry has %zu actions but %zu frozen IDs; "
                   "append new action names to registry.cpp\n",
                   registered.size(), count);
      std::abort();
    }
    std::vector<RuleVTable> rules;
    rules.reserve(count);
    for (const char *name : names) {
      const auto match = std::find_if(
          registered.begin(), registered.end(),
          [name](const RuleVTable &rule) {
            return std::strcmp(rule.name, name) == 0;
          });
      if (match == registered.end() ||
          std::find_if(match + 1, registered.end(),
                       [name](const RuleVTable &rule) {
                         return std::strcmp(rule.name, name) == 0;
                       }) != registered.end()) {
        std::fprintf(stderr,
                     "c ERROR: registry action '%s' missing or duplicated\n",
                     name);
        std::abort();
      }
      rules.push_back(*match);
      rules.back().id = static_cast<uint32_t>(rules.size() - 1);
    }
    return rules;
  }();
  return ordered;
}

uint64_t now_nanos() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

// The charge-and-dispatch wrapper. An id at or beyond MAX_TIMED_RULES
// would silently corrupt neighbouring counters, so it drops the measurement
// instead — the registry may outgrow the array, but a diagnostic must never
// change behaviour or scribble.
namespace {

template <typename Candidate, typename Apply>
ApplyResult dispatch_rule(const RuleVTable &vt, Kernel &k,
                          [[maybe_unused]] Candidate &&candidate_name,
                          Apply &&apply, bool publish_semantic_candidate = true) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  const std::string candidate = candidate_name();
  const bool journal = vt.kind == ActionKind::ProofRule &&
                       rule_journal_wants(k, vt.name, candidate);
  uint64_t pre_digest = 0, formula_epoch = 0, root_epoch = 0,
           effort_before = 0;
  RuleEligibility eligibility;
  if (journal) {
    formula_epoch = k.arena_generation;
    root_epoch = k.touch_fixed_head;
    pre_digest = rule_journal_digest(k);
    effort_before = rule_journal_effort(k);
    eligibility = vt.guard(k);
    if (!eligibility) {
      journal_dispatch(vt, k, candidate.c_str(), pre_digest, formula_epoch,
                       root_epoch, eligibility, ApplyResult::NoOp,
                       effort_before);
      return ApplyResult::NoOp;
    }
    rule_journal_transaction_begin(vt.name, candidate);
  }
#endif
  struct ScopedTraceRule {
    const char *previous;
    explicit ScopedTraceRule(const char *name)
        : previous(event_trace_rule_enter(name)) {}
    ~ScopedTraceRule() { event_trace_rule_leave(previous); }
  } trace_rule(vt.name);
  strategy::begin_semantic_variable_candidate_action(k);
  ApplyResult result;
  if (vt.id >= Stats::MAX_TIMED_RULES) {
    result = apply();
  } else {
    const uint64_t t0 = now_nanos();
    result = apply();
    k.stats.rule_nanos[vt.id] += now_nanos() - t0;
    k.stats.rule_calls[vt.id]++;
  }
  ++k.strategy_epoch;
  if (publish_semantic_candidate)
    strategy::finish_semantic_variable_candidate_action(k, result);
  else if (result != ApplyResult::Ok)
    strategy::begin_semantic_variable_candidate_action(k);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  if (journal) {
    journal_dispatch(vt, k, candidate.c_str(), pre_digest, formula_epoch,
                     root_epoch, eligibility, result, effort_before);
    rule_journal_transaction_end();
  }
#endif
  return result;
}

} // namespace

ApplyResult apply_rule(const RuleVTable &vt, Kernel &k) {
  execution::execute_action(k, execution::CloseStartupCertificateRequest{});
  return dispatch_rule(vt, k, [] { return std::string("canonical"); },
                       [&] { return vt.apply(k); });
}

ApplyResult apply_rule_instance(const RuleVTable &vt, Kernel &k,
                                uint64_t inst) {
  if (!vt.startup_instance || !vt.startup_instance(inst)) {
    execution::execute_action(k, execution::CloseStartupCertificateRequest{});
  }
  return dispatch_rule(vt, k,
                       [&] {
                         return inst == INSTANCE_CANONICAL
                                    ? std::string("canonical")
                                    : std::to_string(inst);
                       },
                       [&] { return vt.apply_instance(k, inst); });
}

ApplyResult apply_rule_semantic_instance(const RuleVTable &vt, Kernel &k,
                                         uint64_t inst) {
  if (!vt.startup_instance || !vt.startup_instance(inst)) {
    execution::execute_action(k, execution::CloseStartupCertificateRequest{});
  }
  return dispatch_rule(vt, k,
                       [&] {
                         return inst == INSTANCE_CANONICAL
                                    ? std::string("canonical")
                                    : std::to_string(inst);
                       },
                       [&] { return vt.apply_semantic_instance(k, inst); });
}

ApplyResult apply_rule_structured(const RuleVTable &vt, Kernel &k,
                                  const char *description,
                                  const std::function<ApplyResult()> &apply) {
  execution::execute_action(k, execution::CloseStartupCertificateRequest{});
  return dispatch_rule(vt, k, [description] { return std::string(description); },
                       apply);
}

ApplyResult apply_rule_structured_defer_candidate(
    const RuleVTable &vt, Kernel &k, const char *description,
    const std::function<ApplyResult()> &apply) {
  execution::execute_action(k, execution::CloseStartupCertificateRequest{});
  return dispatch_rule(vt, k, [description] { return std::string(description); },
                       apply, false);
}

ApplyResult apply_rule_structured_preserve_startup(
    const RuleVTable &vt, Kernel &k, const char *description,
    const std::function<ApplyResult()> &apply) {
  return dispatch_rule(vt, k, [description] { return std::string(description); },
                       apply);
}

#ifdef TACTIX_ENABLE_RULE_JOURNAL
ApplyResult journal_direct_rule(Kernel &k, const char *name,
                                const std::string &candidate,
                                const std::function<ApplyResult()> &apply,
                                ApplyResult declined_result) {
  const int id = tactix_rule_id(name);
  assert(id >= 0);
  const RuleVTable &vt = tactix_rules()[static_cast<size_t>(id)];
  assert(vt.kind == ActionKind::ProofRule);
  if (!rule_journal_wants(k, name, candidate))
    return apply();
  const uint64_t formula_epoch = k.arena_generation;
  const uint64_t root_epoch = k.touch_fixed_head;
  const uint64_t pre_digest = rule_journal_digest(k);
  const uint64_t effort_before = rule_journal_effort(k);
  const RuleEligibility eligibility = vt.guard(k);
  if (!eligibility) {
    journal_dispatch(vt, k, candidate.c_str(), pre_digest, formula_epoch,
                     root_epoch, eligibility, ApplyResult::NoOp,
                     effort_before);
    return declined_result;
  }
  rule_journal_transaction_begin(name, candidate);
  const ApplyResult result = apply();
  journal_dispatch(vt, k, candidate.c_str(), pre_digest, formula_epoch,
                   root_epoch, eligibility, result, effort_before);
  rule_journal_transaction_end();
  return result;
}
#endif

int tactix_rule_id(const char *name) {
  const auto &rules = tactix_rules();
  for (size_t i = 0; i < rules.size(); i++)
    if (!std::strcmp(rules[i].name, name))
      return static_cast<int>(i);
  return -1;
}

} // namespace tactix
