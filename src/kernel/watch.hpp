// Watch scheme (transliterated from kissat/src/watch.h, MIT): watch lists
// are flat uint32_t vectors. A binary clause occupies ONE slot — the other
// literal tagged with the low bit. A large clause occupies TWO consecutive
// slots — the blocking literal (untagged) then the arena reference. The
// hot loop therefore streams homogeneous 32-bit words.
#pragma once

#include "kernel/types.hpp"

#include <vector>

namespace tactix {

struct Watch {
  uint32_t raw;
  bool binary() const { return raw & 1; }
  Lit other() const { return raw >> 1; }    // binary: the other literal
  Lit blocking() const { return raw >> 1; } // large: blocking literal
};

inline Watch binary_watch(Lit other) { return {(other << 1) | 1u}; }
inline Watch blocking_watch(Lit blocking) { return {blocking << 1}; }

using WatchList = std::vector<uint32_t>;

inline void push_binary_watch(WatchList &ws, Lit other) {
  ws.push_back(binary_watch(other).raw);
}
inline void push_large_watch(WatchList &ws, Lit blocking, Ref ref) {
  ws.push_back(blocking_watch(blocking).raw);
  ws.push_back(ref);
}

} // namespace tactix
