#pragma once

#include "proof/derivation_observer.hpp"
#include <utility>

namespace tactix::session {

struct DerivationRecord {
  DerivationKind kind = DerivationKind::Rup;
  uint64_t query = 0, id = 0, source_id = 0;
  std::vector<int32_t> clause, witness;
  std::vector<std::pair<int32_t, int32_t>> mappings;
  std::vector<PbpParityEquation> parity_equations;
  uint32_t external_variables = 0;
  bool propagates = false;
};
struct DerivationSnapshot {
  size_t records = 0, literals = 0;
  uint64_t dropped = 0;
};

// Optional, bounded logical history, not a trusted premise registry. A caller
// must independently license a record in the destination permanent prefix.
// Deletions and SR/RAT witnesses are retained for provenance integration, but
// parity recipes require independent complete-premise and consequence checks. Input records
// include query assumptions and must never seed that independent oracle.
class DerivationHistory final : public DerivationObserver {
public:
  DerivationHistory(size_t records, size_t literals)
      : maximum_records_(records), maximum_literals_(literals) {}
  DerivationHistory(const DerivationHistory &) = delete;
  DerivationHistory &operator=(const DerivationHistory &) = delete;
  DerivationHistory(DerivationHistory &&) = delete;
  DerivationHistory &operator=(DerivationHistory &&) = delete;
  void start_query(uint64_t query) { query_ = query; }
  void observe(const DerivationEvent &) noexcept override;
  std::span<const DerivationRecord> records() const { return records_; }
  DerivationSnapshot snapshot() const { return {records_.size(), literals_, dropped_}; }
private:
  void capture(const DerivationEvent &);
  size_t maximum_records_ = 0, maximum_literals_ = 0, literals_ = 0;
  uint64_t query_ = 0, dropped_ = 0;
  std::vector<DerivationRecord> records_;
};
} // namespace tactix::session
