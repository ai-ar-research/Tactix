#include "strategy/region_program.hpp"
#include "kernel/kernel.hpp"
#include "policies/rule_publication.hpp"
#include "strategy/publication_execution.hpp"
namespace tactix::strategy {
bool RegionProgram::enqueue(const RegionQuery &q) {
  if (!valid_query(q)) return false;
  queries_.push_back(q); return true;
}
ApplyResult RegionProgram::run() {
  auto &k = owner_; bool changed = false;
  const int propagate = tactix_rule_id("Propagate");
  const auto published = [&](auto &&action) {
    return execution::execute_action(k, execution::NATIVE_RULE_PUBLICATION,
        action, HeuristicRulePublisher{}).result;
  };
  while (cursor_ < queries_.size()) {
    if (kernel_terminating(k)) return changed ? ApplyResult::Ok : ApplyResult::NoOp;
    if (!kernel_propagation_fixpoint(k) &&
        published([&] { return apply_rule(tactix_rules()[propagate], k); }) == ApplyResult::Unsat)
      return ApplyResult::Unsat;
    const auto result = published([&] { return apply(queries_[cursor_]); });
    if (result == ApplyResult::Unsat) return result;
    if (result == ApplyResult::NoOp && kernel_terminating(k))
      return changed ? ApplyResult::Ok : ApplyResult::NoOp;
    changed |= result == ApplyResult::Ok; ++cursor_;
  }
  if (!kernel_propagation_fixpoint(k) &&
      published([&] { return apply_rule(tactix_rules()[propagate], k); }) == ApplyResult::Unsat)
    return ApplyResult::Unsat;
  return changed ? ApplyResult::Ok : ApplyResult::NoOp;
}
} // namespace tactix::strategy
