#include "rules/registry.hpp"
#include "diagnostics/rule_journal.hpp"

#include <algorithm>
#include <chrono>
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
                      bool eligible, ApplyResult result) {
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
  record.guards = "rule_guard=" + std::to_string(eligible) +
                  ";consistent=" + std::to_string(!k.inconsistent);
  record.antecedents = "rule-owned";
  record.outcome = eligible ? journal_outcome(result) : "declined";
  record.decline = eligible ? "none" : "rule-guard-false";
  const uint64_t post_digest = rule_journal_digest(k);
  record.deltas =
      pre_digest == post_digest ? "none" : "state-digest-change";
  record.witnesses = "rule-owned";
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
  static const std::vector<RuleVTable> sorted = [] {
    auto rules = mutable_rules();
    std::sort(rules.begin(), rules.end(),
              [](const RuleVTable &a, const RuleVTable &b) {
                return std::strcmp(a.name, b.name) < 0;
              });
    // id == index, stamped once the order is final (see RuleVTable::id)
    for (size_t i = 0; i < rules.size(); i++)
      rules[i].id = static_cast<uint32_t>(i);
    return rules;
  }();
  return sorted;
}

uint64_t now_nanos() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

// The two charge-and-dispatch wrappers. An id at or beyond MAX_TIMED_RULES
// would silently corrupt neighbouring counters, so it drops the measurement
// instead — the registry may outgrow the array, but a diagnostic must never
// change behaviour or scribble.
ApplyResult apply_rule(const RuleVTable &vt, Kernel &k) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  const bool journal = vt.kind == ActionKind::ProofRule &&
                       rule_journal_wants(k, vt.name, "canonical");
  uint64_t pre_digest = 0, formula_epoch = 0, root_epoch = 0;
  if (journal) {
    formula_epoch = k.arena_generation;
    root_epoch = k.touch_fixed_head;
    pre_digest = rule_journal_digest(k);
    const bool eligible = vt.guard(k);
    if (!eligible) {
      journal_dispatch(vt, k, "canonical", pre_digest, formula_epoch,
                       root_epoch, false, ApplyResult::NoOp);
      return ApplyResult::NoOp;
    }
  }
#endif
  if (vt.id >= Stats::MAX_TIMED_RULES)
  {
    const ApplyResult result = vt.apply(k);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
    if (journal)
      journal_dispatch(vt, k, "canonical", pre_digest, formula_epoch,
                       root_epoch, true, result);
#endif
    return result;
  }
  const uint64_t t0 = now_nanos();
  const ApplyResult r = vt.apply(k);
  k.stats.rule_nanos[vt.id] += now_nanos() - t0;
  k.stats.rule_calls[vt.id]++;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  if (journal)
    journal_dispatch(vt, k, "canonical", pre_digest, formula_epoch,
                     root_epoch, true, r);
#endif
  return r;
}

ApplyResult apply_rule_instance(const RuleVTable &vt, Kernel &k,
                                uint64_t inst) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  const std::string candidate =
      inst == INSTANCE_CANONICAL ? "canonical" : std::to_string(inst);
  const bool journal = vt.kind == ActionKind::ProofRule &&
                       rule_journal_wants(k, vt.name, candidate);
  uint64_t pre_digest = 0, formula_epoch = 0, root_epoch = 0;
  if (journal) {
    formula_epoch = k.arena_generation;
    root_epoch = k.touch_fixed_head;
    pre_digest = rule_journal_digest(k);
    const bool eligible = vt.guard(k);
    if (!eligible) {
      journal_dispatch(vt, k, candidate.c_str(), pre_digest, formula_epoch,
                       root_epoch, false, ApplyResult::NoOp);
      return ApplyResult::NoOp;
    }
  }
#endif
  if (vt.id >= Stats::MAX_TIMED_RULES)
  {
    const ApplyResult result = vt.apply_instance(k, inst);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
    if (journal)
      journal_dispatch(vt, k, candidate.c_str(), pre_digest, formula_epoch,
                       root_epoch, true, result);
#endif
    return result;
  }
  const uint64_t t0 = now_nanos();
  const ApplyResult r = vt.apply_instance(k, inst);
  k.stats.rule_nanos[vt.id] += now_nanos() - t0;
  k.stats.rule_calls[vt.id]++;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  if (journal)
    journal_dispatch(vt, k, candidate.c_str(), pre_digest, formula_epoch,
                     root_epoch, true, r);
#endif
  return r;
}

int tactix_rule_id(const char *name) {
  const auto &rules = tactix_rules();
  for (size_t i = 0; i < rules.size(); i++)
    if (!std::strcmp(rules[i].name, name))
      return static_cast<int>(i);
  return -1;
}

} // namespace tactix
