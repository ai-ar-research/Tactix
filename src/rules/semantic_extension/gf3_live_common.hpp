#pragma once

#include "kernel/watch.hpp"
#include "rules/semantic_extension/gf3_affine_live.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tactix::gf3_live_common {

inline void append_u8(std::string &bytes, uint8_t value) {
  bytes.push_back(static_cast<char>(value));
}

inline void append_u32(std::string &bytes, uint32_t value) {
  for (unsigned shift = 0; shift != 32; shift += 8)
    append_u8(bytes, static_cast<uint8_t>(value >> shift));
}

inline void append_u64(std::string &bytes, uint64_t value) {
  for (unsigned shift = 0; shift != 64; shift += 8)
    append_u8(bytes, static_cast<uint8_t>(value >> shift));
}

inline bool append_reference(std::string &bytes,
                             const gf3_affine::SourceReference &reference,
                             const Gf3AffineOwnerSnapshot &owner) {
  append_u64(bytes, reference.occurrence);
  append_u64(bytes, reference.native_proof_id);
  if (!reference.occurrence || reference.occurrence > owner.sources.size())
    return false;
  const auto &clause = owner.sources[reference.occurrence - 1].clause;
  append_u64(bytes, clause.size());
  for (int32_t literal : clause)
    append_u32(bytes, static_cast<uint32_t>(literal));
  return true;
}

inline std::string hex(std::string_view prefix, std::string_view bytes,
                       bool checked_size) {
  std::string result(prefix);
  if (checked_size &&
      bytes.size() > (std::numeric_limits<size_t>::max() - result.size()) / 2)
    return {};
  result.reserve(result.size() + 2 * bytes.size());
  static constexpr char digits[] = "0123456789abcdef";
  for (unsigned char byte : bytes) {
    result.push_back(digits[byte >> 4]);
    result.push_back(digits[byte & 15]);
  }
  return result;
}

template <class Meter, class Consume>
bool scan_live_external_clauses(const Kernel &kernel, Meter &meter,
                                Consume &&consume) {
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    const uint32_t words = clause_actual_words(clause);
    if (!words || words > kernel.arena.size() - reference) return false;
    reference += words;
    if (!meter.charge(uint64_t{1} + clause->size)) return false;
    if (clause->garbage()) continue;
    std::vector<int32_t> external;
    external.reserve(clause->size);
    for (Lit literal : *clause)
      external.push_back(kernel_externalize(kernel, literal));
    consume(std::move(external));
  }
  for (Lit first = 0; first < 2 * kernel.nvars; ++first)
    for (const WatchEntry watch : kernel.watches.entries(first)) {
      if (!meter.charge(1)) return false;
      if (!watch.binary() || first >= watch.other()) continue;
      consume(std::vector<int32_t>{kernel_externalize(kernel, first),
                                   kernel_externalize(kernel, watch.other())});
    }
  return true;
}

} // namespace tactix::gf3_live_common
