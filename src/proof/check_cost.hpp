#pragma once
// Predicted checking cost of an automatic run's two complete proofs (plan §5).
//
// The reference DSR routes end in checkers that scan the whole formula for
// every non-RUP line: drat-trim's checkRAT walks every literal slot
// (-maxVar..maxVar), every watch entry and every live clause body before it
// looks at a candidate (drat-trim.c:555-573), and Trestle's SR.checkLine visits
// every clause slot ever committed and reduces every live clause
// (SRChecker.lean:286-341). VeriPB and CakePB find a `red` step's goals through
// occurrence lists, so the same lines cost them no formula scan. The census
// below counts that scan work while the DSR copy is written; the estimate adds
// it to each route's per-byte cost. Evidence and the measurements behind the
// constants: Tactix-dev/benchmark-reports/2026-11-03-dsr-route-rat-scan.

#include <cstdint>

namespace tactix {

// Checker-visible database of a native-DSR stream, and the scan work the
// reference checkers spend on its non-RUP lines. Units are kept out of the
// watch census (drat-trim assigns them; both DSR elaborators ignore unit
// deletions by default). Each non-RUP line is charged against the database
// before it is added, as a backward checker sees it.
struct DsrCheckCensus {
  uint64_t live_watched = 0;    // live clauses with at least two literals
  uint64_t live_watched_literals = 0;
  uint64_t live = 0;            // live clauses, units included
  uint64_t live_literals = 0;
  uint64_t committed = 0;       // clauses ever committed (Trestle's slots)
  uint64_t rat_lines = 0;       // witness-free non-RUP additions
  uint64_t sr_lines = 0;        // witnessed (substitution) additions
  // Sums over non-RUP lines, saturating.
  uint64_t watch_scan = 0;        // 2 * live_watched (drat-trim watch entries)
  uint64_t watched_literal_scan = 0; // live_watched_literals (clause bodies)
  uint64_t deleted_slot_scan = 0; // committed - live (Trestle's dead slots)
  uint64_t live_scan = 0;         // live (Trestle's live clauses)
  uint64_t live_literal_scan = 0; // live_literals

  static void add_saturating(uint64_t &total, uint64_t value) noexcept {
    total = value > UINT64_MAX - total ? UINT64_MAX : total + value;
  }
  void line(bool deletion, uint32_t size) noexcept {
    if (!size)
      return; // the empty clause
    if (!deletion) {
      ++committed;
      ++live;
      live_literals += size;
      if (size >= 2) {
        ++live_watched;
        live_watched_literals += size;
      }
      return;
    }
    if (size < 2)
      return; // unit deletions are ignored by both DSR elaborators
    if (live) {
      --live;
      live_literals -= live_literals < size ? live_literals : size;
    }
    if (live_watched) {
      --live_watched;
      live_watched_literals -=
          live_watched_literals < size ? live_watched_literals : size;
    }
  }
  void non_rup_line(bool witnessed) noexcept {
    ++(witnessed ? sr_lines : rat_lines);
    add_saturating(watch_scan, 2 * live_watched);
    add_saturating(watched_literal_scan, live_watched_literals);
    add_saturating(deleted_slot_scan, committed - (live < committed ? live : committed));
    add_saturating(live_scan, live);
    add_saturating(live_literal_scan, live_literals);
  }
};

struct AutomaticCheckEstimate {
  bool witnessed = false;      // the DSR copy takes the Trestle route
  uint32_t max_variable = 0;   // drat-trim's maxVar
  uint64_t dsr_bytes = 0, pbp_bytes = 0;
  DsrCheckCensus census;
  double dsr_scan_seconds = 0; // formula scans for non-RUP lines
  double dsr_seconds = 0;      // the whole DSR route
  double pbp_seconds = 0;      // VeriPB then CakePB
  bool pbp_faster() const noexcept { return pbp_seconds < dsr_seconds; }
};

AutomaticCheckEstimate estimate_automatic_checks(const DsrCheckCensus &,
                                                 bool witnessed,
                                                 uint32_t max_variable,
                                                 uint64_t dsr_bytes,
                                                 uint64_t pbp_bytes) noexcept;

} // namespace tactix
