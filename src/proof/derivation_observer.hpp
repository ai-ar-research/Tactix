#pragma once

#include "population/population_observer.hpp"
#include "proof/parity_equation.hpp"
#include <span>

namespace tactix {
struct DsrLiteralMapping;

// Logical calls independent of serializer expansion. Borrowed views use stable external
// coordinates; the mapping span uses the accompanying native-to-external map.
// These events are also delivered when the search-facing writer emits no bytes.
enum class DerivationKind : uint8_t { Input, Rup, Rat, SubstitutionRedundancy, Delete, ParityReason };
struct DerivationEvent {
  DerivationKind kind = DerivationKind::Rup;
  uint64_t id = 0, source_id = 0;
  PopulationClauseView clause, witness;
  std::span<const DsrLiteralMapping> mappings;
  std::span<const PbpParityEquation> parity_equations;
  const std::vector<Var> *variable_map = nullptr;
  uint32_t external_variables = 0;
  bool propagates = false;
};

class DerivationObserver {
public:
  virtual ~DerivationObserver() = default;
  virtual void observe(const DerivationEvent &) noexcept = 0;
};
} // namespace tactix
