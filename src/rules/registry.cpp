#include "rules/registry.hpp"
#include "diagnostics/event_trace.hpp"
#include "diagnostics/rule_journal.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace tactix {

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
        "Walk"};
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
                          Apply &&apply) {
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
  k.close_startup_certificate();
  if (k.proof && k.proof->startup_pb_available())
    k.proof->commit_dsr();
  return dispatch_rule(vt, k, [] { return std::string("canonical"); },
                       [&] { return vt.apply(k); });
}

ApplyResult apply_rule_instance(const RuleVTable &vt, Kernel &k,
                                uint64_t inst) {
  if (!vt.startup_instance || !vt.startup_instance(inst)) {
    k.close_startup_certificate();
    if (k.proof && k.proof->startup_pb_available())
      k.proof->commit_dsr();
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
    k.close_startup_certificate();
    if (k.proof && k.proof->startup_pb_available())
      k.proof->commit_dsr();
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
  k.close_startup_certificate();
  if (k.proof && k.proof->startup_pb_available())
    k.proof->commit_dsr();
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
