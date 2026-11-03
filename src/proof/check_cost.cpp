#include "proof/check_cost.hpp"

namespace tactix {

namespace {

// Unit costs in nanoseconds of the pinned reference checkers on the cpu-q
// benchmark nodes; Tactix-dev/benchmark-reports/2026-11-03-dsr-route-rat-scan
// records each measurement. They are measured, not fitted to selections.
//
// drat-trim checkRAT, per RAT line: one literal slot for each of
// -maxVar..maxVar, then the watch entries and clause bodies of the live
// database. Measured in place on two real proofs (perf, uncontended): the slot
// loop at 2.4 ns per slot (ncc_none_21015); entries and bodies at 2.7 and 1.0
// ns per unit (ncc_none_21015, cfi-rigid-s2-0064), of which the geometric mean
// is taken.
constexpr double kDratTrimSlotNs = 2.4;
constexpr double kDratTrimScanUnitNs = 1.65;
// Trestle SR.checkLine, per non-RUP line: at least one flag test per deleted
// slot (live clauses cost more, so counting deleted slots alone is a lower
// bound). Micro-benchmark of the pinned srcheck on a compute node: 2.7 ns per
// deleted slot, a sequential scan that does not depend on the proof's layout.
constexpr double kTrestleDeletedSlotNs = 2.7;
// PBP route wall time per certificate byte: the median over the fixed
// regression sets' verified rows (B28+C5, Rank1-50, WV100, WV2-89; 135 rows,
// deletion by id), VeriPB then CakePB.
constexpr double kPbpRouteNsPerByte = 3667.0;

constexpr double kNs = 1e-9;

} // namespace

AutomaticCheckEstimate estimate_automatic_checks(const DsrCheckCensus &census,
                                                 bool witnessed,
                                                 uint32_t max_variable,
                                                 uint64_t dsr_bytes,
                                                 uint64_t pbp_bytes) noexcept {
  AutomaticCheckEstimate estimate;
  estimate.witnessed = witnessed;
  estimate.max_variable = max_variable;
  estimate.dsr_bytes = dsr_bytes;
  estimate.pbp_bytes = pbp_bytes;
  estimate.census = census;
  if (witnessed) {
    estimate.dsr_scan_seconds =
        kNs * kTrestleDeletedSlotNs * static_cast<double>(census.deleted_slot_scan);
  } else {
    const double non_rup =
        static_cast<double>(census.rat_lines) + static_cast<double>(census.sr_lines);
    const double slots = 2.0 * static_cast<double>(max_variable) + 1.0;
    estimate.dsr_scan_seconds =
        kNs * (kDratTrimSlotNs * non_rup * slots +
               kDratTrimScanUnitNs *
                   (static_cast<double>(census.watch_scan) +
                    static_cast<double>(census.watched_literal_scan)));
  }
  estimate.pbp_seconds =
      kNs * kPbpRouteNsPerByte * static_cast<double>(pbp_bytes);
  return estimate;
}

} // namespace tactix
