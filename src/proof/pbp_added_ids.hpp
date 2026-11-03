#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace tactix {

// Proof ids of a certificate emitter's own additions, keyed by their sorted
// external literals, for emitters that later erase those clauses by literals.
// Streaming PBP deletes by identity, and an id-0 deletion of a clause that was
// added with an id switches that off for the rest of the run. Each key keeps a
// stack of ids; an erase names the newest live copy.
class PbpAddedIds {
public:
  void add(const int32_t *lits, uint32_t size, uint64_t id) {
    ids_[key(lits, size)].push_back(id);
  }

  // The id of the newest live addition with exactly these literals, or 0.
  uint64_t take(const int32_t *lits, uint32_t size) {
    const auto found = ids_.find(key(lits, size));
    if (found == ids_.end())
      return 0;
    const uint64_t id = found->second.back();
    found->second.pop_back();
    if (found->second.empty())
      ids_.erase(found);
    return id;
  }

private:
  struct Hash {
    size_t operator()(const std::vector<int32_t> &key) const noexcept {
      uint64_t hash = 0x9e3779b97f4a7c15ull ^ key.size();
      for (const int32_t literal : key) {
        hash ^= static_cast<uint32_t>(literal);
        hash *= 0xff51afd7ed558ccdull;
        hash ^= hash >> 32;
      }
      return static_cast<size_t>(hash);
    }
  };

  const std::vector<int32_t> &key(const int32_t *lits, uint32_t size) {
    scratch_.assign(lits, lits + size);
    std::sort(scratch_.begin(), scratch_.end());
    return scratch_;
  }

  std::unordered_map<std::vector<int32_t>, std::vector<uint64_t>, Hash> ids_;
  std::vector<int32_t> scratch_;
};

} // namespace tactix
