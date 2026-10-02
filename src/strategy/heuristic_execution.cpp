#include "strategy/heuristic_execution.hpp"
#include "policies/heuristics.hpp"

#include <utility>

namespace tactix::execution {

void execute_action(Kernel &k, const HeuristicAttachmentRequest &request) {
  assert(request.previous != &request.storage);
  if (request.previous) request.storage = std::move(*request.previous);
  k.h = &request.storage;
  k.saved_phases = &request.storage.saved;
  if (request.previous) return;
  if (k.nvars) heuristics_reserve_vars(k, 0, k.nvars);
  if (!k.input_activation_order.empty()) {
    heuristics_seed_activation_order(k, k.input_activation_order);
    k.input_activation_order.clear();
    if (request.activation_storage == ActivationStorage::Shrink)
      k.input_activation_order.shrink_to_fit();
  }
  // Native startup/reattachment excludes already fixed or retired identities
  // from both rankings, without changing their kernel/public identities.
  heuristics_deactivate_inactive_vars(k);
}

void execute_action(Kernel &k, const HeuristicDetachmentRequest &request) {
  if (request.previous) *request.previous = std::move(request.storage);
  k.h = request.previous;
  k.saved_phases = request.previous_saved;
}

size_t execute_action(Kernel &k, ResidualActivationRequest) {
  return heuristics_prepare_residual_activation_order(k);
}
void execute_action(Kernel &k, const ResidualImportRequest &request) {
  heuristics_seed_residual_import_order(k, request.order);
}

} // namespace tactix::execution
