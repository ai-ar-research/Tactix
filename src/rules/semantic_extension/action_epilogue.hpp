#pragma once

#include "strategy/semantic_variable_advice.hpp"

#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

namespace tactix {

// The owning Kernel and sorted committed keys of an action session; only
// `Access` commits, at the action's commit boundary.
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

// `keys` with `key` inserted before `position`, allocated once.
template <class Key>
std::vector<Key> sorted_with(const std::vector<Key> &keys,
                             typename std::vector<Key>::const_iterator position,
                             const Key &key) {
  std::vector<Key> merged;
  merged.reserve(keys.size() + 1);
  merged.insert(merged.end(), keys.begin(), position);
  merged.push_back(key);
  merged.insert(merged.end(), position, keys.end());
  return merged;
}

// Stage `existing` then `introduced` variables at priority 1 as `producer`'s
// candidate for `action_id`; a non-positive variable or refusal aborts.
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
