#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

enum class FailedLiteralPropagation : uint8_t { Full, Binary };

struct FailedLiteralStats {
  uint64_t Stats::*rounds;
  uint64_t Stats::*probed;
  uint64_t Stats::*failed;
};

RuleEligibility failed_literal_guard(const Kernel &);
void failed_literal_candidates(const Kernel &, std::vector<Lit> &);
ApplyResult failed_literal_apply(Kernel &, FailedLiteralPropagation,
                                 const FailedLiteralStats &);
// Kissat/CaDiCaL's binary Backbone keeps successful assumptions on one trail,
// extracts the first binary UIP on a conflict, and repeats after committing the
// derived units at root.  This is deliberately separate from V1 Probe's
// independent one-assumption semantics.
ApplyResult failed_literal_apply_chained_binary(Kernel &,
                                                const FailedLiteralStats &);
ApplyResult failed_literal_try(Kernel &, Lit, FailedLiteralPropagation,
                               const FailedLiteralStats &);
uint32_t failed_literal_instances(Kernel &, uint64_t *out, uint32_t cap);
ApplyResult failed_literal_apply_instance(Kernel &, uint64_t,
                                          FailedLiteralPropagation,
                                          const FailedLiteralStats &);

} // namespace tactix
