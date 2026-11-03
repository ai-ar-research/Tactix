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
// Trestle SR.checkLine, per non-RUP line: every committed slot (a deleted
// slot is one flag test) and every live clause (reduced literal by literal).
// Micro-benchmark on a compute node (randomly laid out formulas, so an upper
// bound for real proofs).
constexpr double kTrestleDeletedSlotNs = 2.5;
constexpr double kTrestleLiveLiteralNs = 16.0;
// Whole-route wall time per certificate byte: medians over the fixed
// regression sets' verified rows (B28+C5, Rank1-50, WV100, WV2-89) at the
// current writers. Witness-free DSR: DSR-as-DRAT, drat-trim, CakeLPR (27
// rows). Witnessed DSR: dsr-trim, dsr-compress, Trestle (99 rows). PBP:
// VeriPB then CakePB (135 rows, deletion by id).
constexpr double kWitnessFreeRouteNsPerByte = 1316.0;
constexpr double kWitnessedRouteNsPerByte = 5716.0;
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
  const double non_rup =
      static_cast<double>(census.rat_lines) + static_cast<double>(census.sr_lines);
  if (witnessed) {
    estimate.dsr_scan_seconds =
        kNs * (kTrestleDeletedSlotNs * static_cast<double>(census.deleted_slot_scan) +
               kTrestleLiveLiteralNs * static_cast<double>(census.live_literal_scan));
    estimate.dsr_seconds = kNs * kWitnessedRouteNsPerByte *
                               static_cast<double>(dsr_bytes) +
                           estimate.dsr_scan_seconds;
  } else {
    const double slots = 2.0 * static_cast<double>(max_variable) + 1.0;
    estimate.dsr_scan_seconds =
        kNs * (kDratTrimSlotNs * non_rup * slots +
               kDratTrimScanUnitNs *
                   (static_cast<double>(census.watch_scan) +
                    static_cast<double>(census.watched_literal_scan)));
    estimate.dsr_seconds = kNs * kWitnessFreeRouteNsPerByte *
                               static_cast<double>(dsr_bytes) +
                           estimate.dsr_scan_seconds;
  }
  estimate.pbp_seconds =
      kNs * kPbpRouteNsPerByte * static_cast<double>(pbp_bytes);
  return estimate;
}

} // namespace tactix
