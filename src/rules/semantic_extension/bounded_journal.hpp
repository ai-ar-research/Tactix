#pragma once

#include "rules/structural_text.hpp"

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace tactix::semantic_extension {

// Journal text under `maximum`. With TrackWork each append's size is charged
// to `work` before the cap test, because publishers report the work of refused
// appends; an append that would overflow `work` is refused, and with
// SaturateWorkOverflow it also saturates `work`.
template <bool TrackWork = false, bool SaturateWorkOverflow = false>
struct BoundedJournal {
  uint64_t maximum = 0;
  JournalText text;
  uint64_t work = 0;

  template <class... P> bool append(P &&...pieces) {
    const auto charge = [&](uint64_t size) {
      if constexpr (TrackWork) {
        if (size > UINT64_MAX - work) {
          if constexpr (SaturateWorkOverflow) work = UINT64_MAX;
          return false;
        }
        work += size;
      }
      return true;
    };
    return text.append_if(maximum, charge, std::forward<P>(pieces)...);
  }
  // One append per value: its separating comma and its digits. Unary + spells
  // small integer types as std::to_string spells them.
  template <class Integer> bool integers(const std::vector<Integer> &values) {
    for (size_t index = 0; index < values.size(); ++index)
      if (!append(index ? "," : "", +values[index])) return false;
    return true;
  }
};

// One record per retained source premise, in source order:
// {"kind":"<kind>","action":A,"occurrence":O,"native_proof_id":N,"clause":[..]}
template <class Journal, class Premises>
bool append_source_premises(Journal &journal, std::string_view kind,
                            uint64_t action, const Premises &source) {
  for (const auto &premise : source)
    if (!journal.append("{\"kind\":\"", kind, "\",\"action\":", action,
                        ",\"occurrence\":", premise.occurrence,
                        ",\"native_proof_id\":", premise.native_proof_id,
                        ",\"clause\":[") ||
        !journal.integers(premise.literals) || !journal.append("]}\n"))
      return false;
  return true;
}

} // namespace tactix::semantic_extension
