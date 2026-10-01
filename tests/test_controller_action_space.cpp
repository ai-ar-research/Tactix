#undef NDEBUG
#include "strategy/controller_action_space.hpp"

#include <cassert>
#include <set>

using namespace tactix;
using namespace tactix::strategy::controller;

int main() {
  const auto configurations = search_configurations();
  assert(configurations.size() == 20);
  std::set<std::string> identities;
  for (const auto configuration : configurations) {
    assert(valid(configuration));
    assert(identities.insert(name(configuration)).second);
    SearchConfiguration parsed;
    assert(parse_search_configuration(name(configuration), parsed));
    assert(parsed == configuration);
  }
  SearchConfiguration unchanged{Branching::Evsids, Restart::Reluctant, Phase::Target};
  const auto original = unchanged;
  assert(!parse_search_configuration("evsids/reluctant/unknown", unchanged));
  assert(unchanged == original);
  assert(!valid({static_cast<Branching>(2), Restart::Reluctant, Phase::Target}));
  assert(name({Branching::Vmtf, static_cast<Restart>(255), Phase::Saved}).empty());
  assert(!valid({Branching::Vmtf, Restart::AdaptiveGlue, static_cast<Phase>(5)}));

  unsigned excluded = 0;
  for (const auto &rule : tactix_rules()) {
    const auto reason = root_exclusion(rule);
    if (!reason.empty()) {
      ++excluded;
      assert(std::string_view(rule.name) == "Decide" ||
             std::string_view(rule.name) == "ConflictAnalysis" ||
             std::string_view(rule.name) == "Restart" ||
             std::string_view(rule.name) == "SwitchMode");
    }
  }
  assert(excluded == 4);
  assert(root_exclusion(tactix_rules()[tactix_rule_id("BVE")]).empty());
  assert(root_exclusion(tactix_rules()[tactix_rule_id("SubstitutionRedundancy")]).empty());
  assert(root_exclusion(tactix_rules()[tactix_rule_id("Rephase")]).empty());
}
