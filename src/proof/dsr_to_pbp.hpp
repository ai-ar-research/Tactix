// One-way conversion of a native-DSR proof prefix to streaming-PBP lines, for
// the automatic proof's switch (plan §5).
//   addition          rup <clause> ;            RUP
//                     red <clause> : pivot ;    a marked RAT line
//                     red <clause> : witness ;  a witnessed SR line
//   deletion          del spec <clause> ;       canonical, as VeriPB reads CNF
//   unit deletion     nothing                   the DSR checkers ignore them
// The writer keeps the ordinals of its witness-free RAT additions; each
// addition derives exactly one PBP constraint, in order.
#pragma once

#include "kernel/checker_names.hpp"
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

// Reads the stream behind `fd` from offset 0 and appends PBP lines to `out`.
// `rat_additions` are the increasing 0-based ordinals of witness-free RAT
// lines. `helpers`: once solver variables reached `extent`, `helpers` helper
// names existed; PBP restores solver names and puts helpers above them. Not ok
// on I/O failure or a malformed stream (`out` may hold a partial prefix).
DsrToPbpResult convert_dsr_to_pbp(int fd, bool binary,
                                  const std::vector<uint64_t> &rat_additions,
                                  const std::vector<DsrHelperNames> &helpers,
                                  FILE *out);

} // namespace tactix
