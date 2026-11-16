#pragma once
// Shared PHP fixtures; clause order is part of each trajectory control.
#include <cstdint>
#include <utility>
#include <vector>

enum class PhpOrder { HoleMajor, PigeonMajor };

inline std::vector<std::vector<int32_t>> pigeonhole_clauses(
    uint32_t pigeons, uint32_t holes, PhpOrder order = PhpOrder::HoleMajor,
    int32_t first_variable = 1) {
  const auto cell = [=](uint32_t pigeon, uint32_t hole) {
    return int32_t(first_variable + pigeon * holes + hole);
  };
  std::vector<std::vector<int32_t>> clauses;
  for (uint32_t pigeon = 0; pigeon < pigeons; ++pigeon) {
    std::vector<int32_t> alo;
    for (uint32_t hole = 0; hole < holes; ++hole)
      alo.push_back(cell(pigeon, hole));
    clauses.push_back(std::move(alo));
    if (order == PhpOrder::PigeonMajor)
      for (uint32_t next = pigeon + 1; next < pigeons; ++next)
        for (uint32_t hole = 0; hole < holes; ++hole)
          clauses.push_back({-cell(pigeon, hole), -cell(next, hole)});
  }
  if (order == PhpOrder::HoleMajor)
    for (uint32_t hole = 0; hole < holes; ++hole)
      for (uint32_t pigeon = 0; pigeon < pigeons; ++pigeon)
        for (uint32_t next = pigeon + 1; next < pigeons; ++next)
          clauses.push_back({-cell(pigeon, hole), -cell(next, hole)});
  return clauses;
}
