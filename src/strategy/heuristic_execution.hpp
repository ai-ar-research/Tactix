#pragma once

#include "kernel/kernel.hpp"

namespace tactix::execution {

enum class ActivationStorage : uint8_t { Retain, Shrink };

// Storage and lifetime belong to the calling policy. A supplied previous H
// transfers its current state, including later growth, rather than a snapshot
// to restore on exit. Without one, initialize a fresh H from the native seed.
struct HeuristicAttachmentRequest {
  Heuristics &storage;
  Heuristics *previous = nullptr;
  // Shrink preserves the native nonbinding shrink_to_fit request.
  ActivationStorage activation_storage = ActivationStorage::Shrink;
};
struct HeuristicDetachmentRequest {
  Heuristics &storage;
  Heuristics *previous = nullptr;
  std::vector<int8_t> *previous_saved = nullptr;
};
struct ResidualActivationRequest {};
struct ResidualImportRequest {
  const std::vector<Var> &order;
};

void execute_action(Kernel &, const HeuristicAttachmentRequest &);
void execute_action(Kernel &, const HeuristicDetachmentRequest &);
size_t execute_action(Kernel &, ResidualActivationRequest);
void execute_action(Kernel &, const ResidualImportRequest &);

} // namespace tactix::execution
