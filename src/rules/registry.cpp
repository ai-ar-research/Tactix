#include "rules/registry.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace tactix {

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
  if (vt.id >= Stats::MAX_TIMED_RULES)
    return vt.apply(k);
  const uint64_t t0 = now_nanos();
  const ApplyResult r = vt.apply(k);
  k.stats.rule_nanos[vt.id] += now_nanos() - t0;
  k.stats.rule_calls[vt.id]++;
  return r;
}

ApplyResult apply_rule_instance(const RuleVTable &vt, Kernel &k,
                                uint64_t inst) {
  if (vt.id >= Stats::MAX_TIMED_RULES)
    return vt.apply_instance(k, inst);
  const uint64_t t0 = now_nanos();
  const ApplyResult r = vt.apply_instance(k, inst);
  k.stats.rule_nanos[vt.id] += now_nanos() - t0;
  k.stats.rule_calls[vt.id]++;
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
