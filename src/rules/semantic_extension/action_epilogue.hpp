#pragma once

#include "strategy/semantic_variable_advice.hpp"

#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

namespace tactix {

// A Kernel-owned action session: the owning Kernel and the sorted keys of its
// committed actions. Only `Access`, a type the action's translation unit
// completes, can commit, and only at the action's atomic commit boundary.
template <class Key, class Access> class KernelKeyedSession {
public:
  bool retained() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  const std::vector<Key> &keys() const { return keys_; }

  void commit(Access, const Kernel &kernel, std::vector<Key> keys) {
    owner_ = &kernel;
    keys_ = std::move(keys);
  }

private:
  const Kernel *owner_ = nullptr;
  std::vector<Key> keys_;
};

// Stage `existing` then `introduced` positive external variables, at priority
// 1, as `producer`'s semantic-variable candidate for `action_id`. A
// non-positive variable or a refused candidate is a commit-boundary
// programming error and aborts.
inline void stage_theory_variables_or_abort(
    Kernel &kernel, strategy::SemanticVariableProducer producer,
    uint64_t action_id, const std::vector<int32_t> &existing,
    const std::vector<int32_t> &introduced) {
  std::vector<strategy::SemanticVariable> variables;
  variables.reserve(existing.size() + introduced.size());
  for (int32_t external : existing) {
    if (external <= 0) std::abort();
    variables.push_back({static_cast<uint32_t>(external),
                         strategy::SemanticVariableRole::ExistingTheory, 1});
  }
  for (int32_t external : introduced) {
    if (external <= 0) std::abort();
    variables.push_back({static_cast<uint32_t>(external),
                         strategy::SemanticVariableRole::IntroducedTheory, 1});
  }
  if (!strategy::stage_semantic_variable_candidate(kernel, producer, action_id,
                                                   variables))
    std::abort();
}

} // namespace tactix
