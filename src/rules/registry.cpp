#include "rules/registry.hpp"

#include <algorithm>
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
    return rules;
  }();
  return sorted;
}

int tactix_rule_id(const char *name) {
  const auto &rules = tactix_rules();
  for (size_t i = 0; i < rules.size(); i++)
    if (!std::strcmp(rules[i].name, name))
      return static_cast<int>(i);
  return -1;
}

} // namespace tactix
