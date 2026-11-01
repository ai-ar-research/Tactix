#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tactix {

// Mutable indexed max-heap with Kissat's observable topology: equal scores do
// not bubble up, and bubble-down chooses the left child at equality.
template <class Item, class Score>
class IndexedMaxHeap {
  std::vector<Item> stack_;
  std::vector<uint32_t> positions_;
  std::vector<Score> scores_;

  void bubble_up(Item item) {
    uint32_t at = positions_[item];
    while (at) {
      const uint32_t parent_at = (at - 1) / 2;
      const Item parent = stack_[parent_at];
      if (scores_[parent] >= scores_[item]) break;
      stack_[at] = parent;
      positions_[parent] = at;
      at = parent_at;
    }
    stack_[at] = item;
    positions_[item] = at;
  }

  void bubble_down(Item item) {
    uint32_t at = positions_[item];
    for (;;) {
      uint32_t child_at = 2 * at + 1;
      if (child_at >= stack_.size()) break;
      Item child = stack_[child_at];
      const uint32_t sibling_at = child_at + 1;
      if (sibling_at < stack_.size() &&
          scores_[stack_[sibling_at]] > scores_[child]) {
        child_at = sibling_at;
        child = stack_[child_at];
      }
      if (scores_[child] <= scores_[item]) break;
      stack_[at] = child;
      positions_[child] = at;
      at = child_at;
    }
    stack_[at] = item;
    positions_[item] = at;
  }

public:
  void reset(size_t extent) {
    stack_.clear();
    positions_.assign(extent, UINT32_MAX);
    scores_.assign(extent, Score{});
  }

  void ensure(Item item) {
    const size_t extent = static_cast<size_t>(item) + 1;
    if (positions_.size() >= extent) return;
    positions_.resize(extent, UINT32_MAX);
    scores_.resize(extent, Score{});
  }

  bool empty() const { return stack_.empty(); }
  uint32_t size() const { return static_cast<uint32_t>(stack_.size()); }
  size_t extent() const { return positions_.size(); }
  bool contains(Item item) const {
    return item < positions_.size() && positions_[item] != UINT32_MAX;
  }
  uint32_t position(Item item) const {
    return contains(item) ? positions_[item] : UINT32_MAX;
  }
  Score score(Item item) const {
    return item < scores_.size() ? scores_[item] : Score{};
  }
  const std::vector<Item> &members() const { return stack_; }
  const std::vector<Score> &scores() const { return scores_; }

  void assign_score(Item item, Score score) {
    ensure(item);
    scores_[item] = score;
  }

  void update_score(Item item, Score score) {
    ensure(item);
    const Score old = scores_[item];
    scores_[item] = score;
    if (!contains(item)) return;
    if (score > old)
      bubble_up(item);
    else if (score < old)
      bubble_down(item);
  }

  void insert(Item item, Score score) {
    ensure(item);
    assert(!contains(item));
    scores_[item] = score;
    positions_[item] = static_cast<uint32_t>(stack_.size());
    stack_.push_back(item);
    bubble_up(item);
  }

  void erase(Item item) {
    assert(contains(item));
    const uint32_t at = positions_[item];
    const Item last = stack_.back();
    stack_.pop_back();
    positions_[item] = UINT32_MAX;
    if (at == stack_.size()) return;
    stack_[at] = last;
    positions_[last] = at;
    bubble_up(last);
    bubble_down(last);
  }

  Item pop_max() {
    assert(!stack_.empty());
    const Item result = stack_[0];
    const Item last = stack_.back();
    stack_.pop_back();
    positions_[result] = UINT32_MAX;
    if (!stack_.empty()) {
      stack_[0] = last;
      positions_[last] = 0;
      bubble_down(last);
    }
    return result;
  }
};

} // namespace tactix
