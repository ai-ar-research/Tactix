#pragma once

#include "rules/substitution_redundancy/literal_permutation.hpp"
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tactix {

struct CutOrbitMember {
  uint32_t level;
  Lit base;
  Lit member;
};

// Exact earlier-cut obligations for already-validated sparse permutations.
// Only observed levels occupy storage; numeric gaps and UINT32_MAX are legal.
class CutOrbitIndex {
  struct Cut {
    uint32_t level;
    Lit base = INVALID_LIT;
    std::unordered_set<Lit> orbit;
  };
  std::vector<Cut> cuts_;
  std::unordered_map<Lit, uint32_t> first_base_;
  std::unordered_map<Lit, std::vector<size_t>> memberships_;
  bool valid_ = true;

public:
  explicit CutOrbitIndex(std::span<const CutOrbitMember> members) {
    std::vector<uint32_t> levels;
    levels.reserve(members.size());
    for (const auto &member : members) levels.push_back(member.level);
    std::sort(levels.begin(), levels.end());
    levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
    cuts_.reserve(levels.size());
    for (const uint32_t level : levels) cuts_.push_back({level, INVALID_LIT, {}});
    for (const auto &member : members) {
      const size_t index = std::lower_bound(levels.begin(), levels.end(),
                                           member.level) - levels.begin();
      Cut &cut = cuts_[index];
      if (cut.base != INVALID_LIT && cut.base != member.base) {
        valid_ = false;
        return;
      }
      cut.base = member.base;
      cut.orbit.insert(member.base);
      cut.orbit.insert(member.member);
    }
    // The level-major sweep makes every membership list strictly increasing,
    // regardless of the caller's row order or repeated physical cut members.
    for (size_t index = 0; index < cuts_.size(); ++index) {
      const Cut &cut = cuts_[index];
      first_base_.try_emplace(cut.base, cut.level);
      for (const Lit member : cut.orbit)
        memberships_[member].push_back(index);
    }
  }

  bool valid() const { return valid_; }

  bool preserves(const SparseLiteralPermutation &image, uint32_t level) const {
    if (!valid_) return false;
    // Every point outside the support is fixed and satisfies its obligations.
    // Only moved sources can violate a base or leave an earlier complete orbit.
    for (const auto &[source, target] : image.moved) {
      const auto base = first_base_.find(source);
      if (base != first_base_.end() && base->second < level) return false;
      const auto memberships = memberships_.find(source);
      if (memberships == memberships_.end()) continue;
      for (const size_t earlier : memberships->second) {
        const Cut &cut = cuts_[earlier];
        if (cut.level >= level) break;
        if (!cut.orbit.count(target)) return false;
      }
    }
    return true;
  }
};

} // namespace tactix
