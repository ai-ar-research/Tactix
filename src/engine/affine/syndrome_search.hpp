#pragma once
#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>
#include "engine/affine/work.hpp"

namespace tactix::strategy::affine {
using Bits = std::vector<uint64_t>;
enum class SearchStatus { Found, Exhausted, Budget, Interrupted, Invalid };
struct SearchLimits {
  size_t left_size;
  size_t left_weight;
  size_t right_weight;
  size_t total_weight;
  uint64_t maximum_entries;
  uint64_t maximum_bytes;
};
struct SearchStats {
  uint64_t work = 0;
  uint64_t right_entries = 0;
  uint64_t left_entries = 0;
  uint64_t matches = 0;
  uint64_t table_bytes = 0;
};

// Enumerate precisely the stated two-sided weight scope. Hash collisions and
// equal syndromes retain every right support; neither can hide a candidate.
// accept() sees a full projected assignment. It may reject it after checking
// the remaining constraints, in which case the next matching support is tried.
template<class Meter, class Accept>
SearchStatus search(const std::vector<Bits> &columns, const Bits &target,
                    const SearchLimits &limits, Meter &meter, Accept &&accept,
                    SearchStats &stats) {
  stats = {};
  const size_t size = columns.size(), words = target.size();
  if (limits.left_size > size || !words ||
      std::any_of(columns.begin(), columns.end(),
                  [&](const Bits &row) { return row.size() != words; }))
    return SearchStatus::Invalid;
  SearchStatus status = SearchStatus::Exhausted;
  const auto charge = [&](uint64_t amount) {
    if (!meter.charge(amount)) {
      status = meter.interrupted() ? SearchStatus::Interrupted : SearchStatus::Budget;
      return false;
    }
    stats.work += amount;
    return true;
  };
  if (!charge(1)) return status;
  const size_t right_size = size - limits.left_size;
  const size_t right_weight = std::min({limits.right_weight, right_size,
                                       limits.total_weight});
  const size_t left_weight = std::min({limits.left_weight, limits.left_size,
                                      limits.total_weight});
  uint64_t entries = 1, choose = 1;
  for (size_t weight = 1; weight <= right_weight; ++weight) {
    if (!charge(1)) return status;
    // Exact binomial bound in a wider accumulator, before any allocation.
    const unsigned __int128 next =
        static_cast<unsigned __int128>(choose) * (right_size - weight + 1) / weight;
    if (next > UINT64_MAX || next > limits.maximum_entries ||
        entries > limits.maximum_entries - static_cast<uint64_t>(next))
      return SearchStatus::Budget;
    choose = static_cast<uint64_t>(next);
    entries += choose;
  }
  if (entries > limits.maximum_entries || entries > SIZE_MAX / 2)
    return SearchStatus::Budget;
  size_t buckets = 2;
  while (buckets < 2 * entries) {
    if (buckets > SIZE_MAX / 2) return SearchStatus::Budget;
    buckets *= 2;
  }
  if (size > SIZE_MAX - 63) return SearchStatus::Invalid;
  const size_t mask_words = (size + 63) / 64;
  const unsigned __int128 bytes =
      static_cast<unsigned __int128>(entries) *
          ((static_cast<unsigned __int128>(words) + mask_words) * sizeof(uint64_t) +
           sizeof(size_t)) +
      static_cast<unsigned __int128>(buckets) * sizeof(size_t);
  if (bytes > limits.maximum_bytes || bytes > SIZE_MAX ||
      (words && entries > SIZE_MAX / words) ||
      (mask_words && entries > SIZE_MAX / mask_words))
    return SearchStatus::Budget;
  stats.table_bytes = static_cast<uint64_t>(bytes);
  const size_t none = SIZE_MAX;
  if (!charge(buckets)) return status;
  std::vector<size_t> heads(buckets, none), next;
  std::vector<uint64_t> syndromes, masks;
  next.reserve(static_cast<size_t>(entries));
  syndromes.reserve(static_cast<size_t>(entries) * words);
  masks.reserve(static_cast<size_t>(entries) * mask_words);
  Bits syndrome(words, 0), mask(mask_words, 0), wanted(words, 0), candidate(mask_words);
  const auto bucket = [&](const Bits &row) {
    uint64_t hash = 0xcbf29ce484222325ull;
    for (uint64_t word : row) {
      hash ^= word;
      hash *= 0x100000001b3ull;
      hash ^= hash >> 32;
    }
    return static_cast<size_t>(hash) & (buckets - 1);
  };
  const auto toggle = [&](size_t column) {
    for (size_t word = 0; word < words; ++word)
      syndrome[word] ^= columns[column][word];
    mask[column / 64] ^= uint64_t{1} << (column % 64);
  };
  const auto enumerate = [&](size_t begin, size_t end, size_t limit,
                              auto &&visit) -> bool {
    std::vector<size_t> selected;
    selected.reserve(std::min(limit, end - begin));
    if (!charge(1) || !visit(0)) return false;
    size_t column = begin;
    for (;;) {
      if (selected.size() < limit && column < end) {
        if (!charge(words + 2)) return false;
        toggle(column);
        selected.push_back(column++);
        if (!charge(1) || !visit(selected.size())) return false;
      } else {
        if (selected.empty()) return true;
        if (!charge(words + 2)) return false;
        column = selected.back();
        selected.pop_back();
        toggle(column++);
      }
    }
  };
  const auto insert = [&](size_t) {
    if (!charge(2 * words + mask_words + 1)) return false;
    const size_t at = next.size(), slot = bucket(syndrome);
    syndromes.insert(syndromes.end(), syndrome.begin(), syndrome.end());
    masks.insert(masks.end(), mask.begin(), mask.end());
    next.push_back(heads[slot]);
    heads[slot] = at;
    ++stats.right_entries;
    return true;
  };
  if (!enumerate(limits.left_size, size, right_weight, insert))
    return status;
  const auto lookup = [&](size_t chosen) {
    ++stats.left_entries;
    if (!charge(2 * words)) return false;
    for (size_t word = 0; word < words; ++word)
      wanted[word] = target[word] ^ syndrome[word];
    for (size_t at = heads[bucket(wanted)]; at != none; at = next[at]) {
      if (!charge(words)) return false;
      if (!std::equal(wanted.begin(), wanted.end(), syndromes.begin() + at * words))
        continue;
      if (!charge(2 * mask_words)) return false;
      size_t right_count = 0;
      for (size_t word = 0; word < mask_words; ++word) {
        const uint64_t right = masks[at * mask_words + word];
        right_count += static_cast<size_t>(std::popcount(right));
        candidate[word] = mask[word] | right;
      }
      if (right_count > limits.total_weight - chosen) continue;
      ++stats.matches;
      if (accept(candidate)) { status = SearchStatus::Found; return false; }
    }
    return true;
  };
  enumerate(0, limits.left_size, left_weight, lookup);
  return status;
}
} // namespace tactix::strategy::affine
