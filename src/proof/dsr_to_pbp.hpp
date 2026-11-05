// One-way conversion of a native-DSR proof prefix to streaming-PBP lines, the
// price of the automatic proof's switch to PBP (plan §5).
//
//   addition          rup <clause> ;            RUP
//                     red <clause> : pivot ;    a marked RAT line
//                     red <clause> : witness ;  a witnessed SR line
//   deletion          del spec <clause> ;       canonical, as VeriPB reads CNF
//   unit deletion     nothing                   the DSR checkers ignore them
//
// A witness-free DSR line does not say whether it is RUP or RAT, so the writer
// keeps the ordinals of its RAT additions. Every addition line derives exactly
// one PBP constraint, in order.
#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

namespace tactix {

struct DsrToPbpResult {
  bool ok = false;
  uint64_t derived = 0;    // constraints written: one per addition line
  uint64_t dsr_bytes = 0;  // bytes read
  uint64_t pbp_bytes = 0;  // bytes written
};

// Reads the whole stream behind `fd` from offset 0 and appends its PBP lines
// to `out`, which the caller flushes. `rat_additions` holds the increasing
// Proof-only variables the DSR writer named: once the solver's variables reached
// `extent`, `helpers` helpers existed in total. A solver variable created later
// has a DSR name above them. The converted PBP gives solver variables back their
// own names, because the PBP writer continues with those, and helpers names above
// every solver variable.
struct DsrHelperNames {
  uint32_t extent = 0;
  uint32_t helpers = 0;
};

// 0-based addition ordinals of witness-free RAT lines. Not ok on a read or
// write failure or a malformed stream; `out` may then hold a partial prefix.
DsrToPbpResult convert_dsr_to_pbp(int fd, bool binary,
                                  const std::vector<uint64_t> &rat_additions,
                                  const std::vector<DsrHelperNames> &helpers,
                                  FILE *out);

} // namespace tactix
