#pragma once

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace tactix::rup_helpers {

inline void emit(Kernel &kernel, uint64_t lrat_id, uint64_t non_lrat_id,
                 const Lit *literals, uint32_t size,
                 std::vector<std::pair<uint32_t, uint64_t>> &entries,
                 uint64_t closing_id) {
  if (!kernel.proof) return;
  if (!kernel.proof->lrat()) {
    kernel.proof->add_rup(non_lrat_id, literals, size, nullptr, 0);
    return;
  }
  kernel_build_lrat_chain(entries, closing_id, kernel.lrat_chain);
  kernel.proof->add_rup(lrat_id, literals, size, kernel.lrat_chain.data(),
                        static_cast<uint32_t>(kernel.lrat_chain.size()));
}

inline void emit(Kernel &kernel, uint64_t lrat_id, uint64_t non_lrat_id,
                 const std::vector<Lit> &literals,
                 std::vector<std::pair<uint32_t, uint64_t>> &entries,
                 uint64_t closing_id) {
  emit(kernel, lrat_id, non_lrat_id, literals.data(),
       static_cast<uint32_t>(literals.size()), entries, closing_id);
}

inline void remove_assumption_unit_hints(
    std::vector<std::pair<uint32_t, uint64_t>> &entries,
    std::vector<uint64_t> &unit_ids) {
  if (unit_ids.empty()) return;
  std::sort(unit_ids.begin(), unit_ids.end());
  unit_ids.erase(std::unique(unit_ids.begin(), unit_ids.end()), unit_ids.end());
  entries.erase(std::remove_if(entries.begin(), entries.end(),
                               [&](const auto &entry) {
                                 return std::binary_search(
                                     unit_ids.begin(), unit_ids.end(),
                                     entry.second);
                               }),
                entries.end());
}

} // namespace tactix::rup_helpers
