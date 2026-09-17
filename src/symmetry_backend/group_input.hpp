#pragma once

#include "symmetry_backend/dejavu_backend.hpp"
#include <algorithm>
#include <vector>

namespace tactix::symmetry_backend::detail {

template <class Poll>
bool validate_generator_rows(const GroupInput &input, Poll &&poll) {
  if (!input.generator_offsets) {
    if (!input.domain_size || (input.generator_count && !input.generator_images) ||
        input.generator_count > SIZE_MAX / sizeof(uint32_t) / input.domain_size)
      return false;
    if (!input.generator_count) return true;
    std::vector<uint8_t> seen(input.domain_size);
    for (uint64_t row = 0; row < input.generator_count; ++row) {
      poll();
      if (row) std::fill(seen.begin(), seen.end(), 0);
      for (uint32_t point = 0; point < input.domain_size; ++point) {
        if (!(point & 1023u)) poll();
        const uint32_t image = input.generator_images[row * input.domain_size + point];
        if (image >= input.domain_size || seen[image]) return false;
        seen[image] = 1;
      }
    }
    return true;
  }
  if (input.generator_count >= SIZE_MAX / sizeof(uint64_t) ||
      input.generator_entries > SIZE_MAX / sizeof(uint32_t) ||
      input.generator_offsets[0] ||
      input.generator_offsets[input.generator_count] != input.generator_entries ||
      (input.generator_entries && (!input.generator_images || !input.generator_points)))
    return false;
  std::vector<uint32_t> targets;
  for (uint64_t row = 0; row < input.generator_count; ++row) {
    poll();
    const uint64_t begin = input.generator_offsets[row], end = input.generator_offsets[row + 1];
    if (begin > end || end > input.generator_entries) return false;
    targets.clear();
    for (uint64_t at = begin; at < end; ++at) {
      if (!((at - begin) & 1023u)) poll();
      const uint32_t point = input.generator_points[at], target = input.generator_images[at];
      if (point >= input.domain_size || target >= input.domain_size || point == target ||
          (at > begin && input.generator_points[at - 1] >= point)) return false;
      targets.push_back(target);
    }
    std::sort(targets.begin(), targets.end());
    for (uint64_t at = begin; at < end; ++at) {
      if (!((at - begin) & 1023u)) poll();
      // Distinct targets alone are insufficient: a target outside the support
      // would collide with the fixed identity at that point.
      if (targets[at - begin] != input.generator_points[at]) return false;
    }
  }
  return true;
}

// Preconditions: validated rows. Both paths visit nonidentity sources in the
// same ascending order, preserving Schreier insertion and component numbering.
template <class Poll, class Visitor>
void visit_generator_moves(const GroupInput &input, uint64_t row, Poll &&poll,
                           Visitor &&visit) {
  if (input.generator_offsets) {
    const uint64_t begin = input.generator_offsets[row], end = input.generator_offsets[row + 1];
    for (uint64_t at = begin; at < end; ++at) {
      if (!((at - begin) & 1023u)) poll();
      visit(input.generator_points[at], input.generator_images[at]);
    }
  } else {
    for (uint32_t point = 0; point < input.domain_size; ++point) {
      if (!(point & 1023u)) poll();
      const uint32_t target = input.generator_images[row * input.domain_size + point];
      if (target != point) visit(point, target);
    }
  }
}

inline uint32_t generator_image(const GroupInput &input, uint64_t row, uint32_t point) {
  if (!input.generator_offsets) return input.generator_images[row * input.domain_size + point];
  const uint64_t begin = input.generator_offsets[row], end = input.generator_offsets[row + 1];
  if (begin == end) return point;
  const auto *at = std::lower_bound(input.generator_points + begin,
                                  input.generator_points + end, point);
  return at != input.generator_points + end && *at == point
      ? input.generator_images[at - input.generator_points] : point;
}

} // namespace tactix::symmetry_backend::detail
