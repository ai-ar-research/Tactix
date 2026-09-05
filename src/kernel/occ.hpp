// Kissat vector scheme (vector.c, inlinevector.h; MIT) for the dense
// occurrence index: every literal's list is a slice of ONE flat arena rather
// than an independent allocation. A tail slice extends in place while the
// arena allocation has room; otherwise it relocates to a doubled slice and the
// abandoned words join `holes`, exactly `vectors.usable`. Only an explicit
// checkpoint defragments, so a reader's pointers stay valid until it pushes.
#pragma once

#include "kernel/types.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

namespace tactix {

// A literal's slice. Valid until the owning list is pushed to (a push may
// relocate the slice or the whole arena), which is the same precondition
// Kissat states for `BEGIN_WATCHES`.
template <class T> struct FlatSpan {
  T *first = nullptr, *last = nullptr;
  T *begin() const { return first; }
  T *end() const { return last; }
  size_t size() const { return static_cast<size_t>(last - first); }
  bool empty() const { return first == last; }
  T &operator[](size_t i) const { return first[i]; }
  T &back() const { return last[-1]; }
};

template <class T> class FlatLists {
public:
  static_assert(sizeof(T) == sizeof(uint32_t),
                "a dense occurrence payload is one Kissat vector word");

  struct Head {
    uint32_t offset = 0, size = 0, capacity = 0;
  };

  // Reachability readouts, drained into Kernel::stats at every dense
  // disconnect: a slice relocation is the cost capacity retention exists to
  // avoid, and a defragmentation is the only thing that returns abandoned
  // slices to the arena.
  uint64_t relocations = 0, defrags = 0;

  size_t lits() const { return heads_.size(); }
  // Number of per-literal lists, not a list's length; `size(Lit)` is that.
  size_t size() const { return heads_.size(); }

  // Release payload and headers both: a phase with no consumer must not
  // retain several GiB of cold capacity.
  void release() {
    std::vector<T>().swap(arena_);
    std::vector<Head>().swap(heads_);
    holes_ = 0;
  }

  FlatSpan<T> operator[](Lit l) {
    assert(l < heads_.size());
    Head &head = heads_[l];
    T *const base = arena_.data() + head.offset;
    return {base, base + head.size};
  }
  FlatSpan<const T> operator[](Lit l) const {
    assert(l < heads_.size());
    const Head &head = heads_[l];
    const T *const base = arena_.data() + head.offset;
    return {base, base + head.size};
  }
  uint32_t size(Lit l) const {
    assert(l < heads_.size());
    return heads_[l].size;
  }
  bool empty(Lit l) const { return !heads_[l].size; }
  // No list at all, as opposed to an empty one: the index was released.
  bool empty() const { return heads_.empty(); }
  const T *data(Lit l) const { return arena_.data() + heads_[l].offset; }
  // Whole-index storage: one payload arena and one head array, where a
  // per-literal container held 2*nvars independent allocations.
  const void *arena_data() const { return arena_.data(); }
  const void *heads_data() const { return heads_.data(); }
  size_t arena_capacity_bytes() const { return arena_.capacity() * sizeof(T); }
  size_t heads_capacity_bytes() const {
    return heads_.capacity() * sizeof(Head);
  }
  uint32_t capacity(Lit l) const { return heads_[l].capacity; }

  // Grow the head array in place; new literals start empty. A var-adding rule
  // (Factor/BVA) can extend nvars while the index is connected.
  void resize_lits(size_t nlits) {
    assert(nlits >= heads_.size());
    heads_.resize(nlits);
  }

  void push(Lit l, T value) {
    Head &head = heads_[l];
    if (head.size == head.capacity)
      grow(l);
    assert(holes_);
    holes_--;
    Head &fresh = heads_[l];
    arena_[fresh.offset + fresh.size++] = value;
  }

  void clear(Lit l) {
    Head &head = heads_[l];
    holes_ += head.size;
    head.size = 0;
  }

  // Shrink after an in-place compaction. Capacity is retained and reused.
  void shrink(Lit l, size_t n) {
    Head &head = heads_[l];
    assert(n <= head.size);
    holes_ += head.size - static_cast<uint32_t>(n);
    head.size = static_cast<uint32_t>(n);
  }

  void erase_at(Lit l, size_t at) {
    Head &head = heads_[l];
    assert(at < head.size);
    T *const base = arena_.data() + head.offset;
    std::copy(base + at + 1, base + head.size, base + at);
    head.size--;
    holes_++;
  }

  // Batch layout for a full rebuild. The caller counts every forthcoming
  // append first and then receives one arena of contiguous slices, so the fill
  // relocates nothing. Existing contents are discarded; existing CAPACITY is
  // not. Kissat's dense connect pushes into vectors that kept their capacity
  // from the previous connect, so a literal whose occurrence count is stable
  // never relocates again — dropping to an exact fit instead would make the
  // first push of the next phase relocate every literal that grows at all.
  void begin_exact_layout(size_t nlits) {
    if (pending_.size() != nlits)
      pending_.assign(nlits, 0);
    else
      std::fill(pending_.begin(), pending_.end(), 0u);
  }
  void count(Lit l) {
    assert(pending_[l] < UINT32_MAX);
    pending_[l]++;
  }
  void count(Lit l, uint32_t n) {
    assert(UINT32_MAX - pending_[l] >= n);
    pending_[l] += n;
  }
  void finish_exact_layout() {
    const size_t nlits = pending_.size();
    heads_.resize(nlits);
    size_t total = 0;
    for (size_t lit = 0; lit < nlits; lit++) {
      Head &head = heads_[lit];
      head.size = 0;
      if (head.capacity < pending_[lit])
        head.capacity = pending_[lit];
      total += head.capacity;
    }
    assert(total <= UINT32_MAX);
    arena_.resize(total);
    uint32_t next = 0;
    for (size_t lit = 0; lit < nlits; lit++) {
      heads_[lit].offset = next;
      next += heads_[lit].capacity;
    }
    // Every reserved word is spare until the fill consumes it, which is what
    // `push` decrements; that is exactly `vectors.usable` over the live stack.
    holes_ = total;
  }

  // collect.h defaults, as transliterated in watch.hpp: inspect only above
  // 2^18 words and only when more than 75 % of the stack is unusable.
  void defrag_if_needed() {
    constexpr size_t SIZE_LIMIT = size_t{1} << 18;
    const size_t stack_size = arena_.empty() ? 0 : arena_.size() + 1;
    if (stack_size <= SIZE_LIMIT)
      return;
    if (holes_ <= (uint64_t{stack_size} * 75) / 100)
      return;
    defrag();
  }

private:
  void grow(Lit l) {
    Head &head = heads_[l];
    assert(head.size == head.capacity);
    if (arena_.capacity() == 0)
      arena_.reserve(3);
    // A slice ending at the live end of the arena extends in place whenever
    // the allocation has room (inlinevector.h).
    if (static_cast<size_t>(head.offset) + head.size == arena_.size() &&
        arena_.size() < arena_.capacity()) {
      arena_.resize(arena_.size() + 1);
      heads_[l].capacity++;
      holes_++;
      return;
    }
    const uint32_t ncap = head.size ? 2 * head.size : 1;
    const size_t needed = arena_.size() + ncap;
    assert(needed <= UINT32_MAX);
    if (needed > arena_.capacity()) {
      size_t stack_capacity = 4;
      while (stack_capacity - 1 < needed)
        stack_capacity *= 2;
      arena_.reserve(stack_capacity - 1);
    }
    const uint32_t noff = static_cast<uint32_t>(arena_.size());
    arena_.resize(arena_.size() + ncap);
    relocations++;
    Head &fresh = heads_[l];
    std::copy(arena_.begin() + fresh.offset,
              arena_.begin() + fresh.offset + fresh.size,
              arena_.begin() + noff);
    // The abandoned image and the unused half of the new one are both usable,
    // the accounting of vector.c:kissat_enlarge_vector.
    holes_ += ncap;
    fresh.offset = noff;
    fresh.capacity = ncap;
  }

  void defrag() {
    defrags++;
    std::vector<Lit> order;
    order.reserve(heads_.size());
    for (Lit lit = 0; lit < heads_.size(); lit++)
      if (heads_[lit].size)
        order.push_back(lit);
    std::sort(order.begin(), order.end(), [&](Lit a, Lit b) {
      return heads_[a].offset < heads_[b].offset;
    });
    size_t live = 0;
    for (const Lit lit : order)
      live += heads_[lit].size;
    size_t stack_capacity = 1;
    while (stack_capacity < live + 1)
      stack_capacity *= 2;
    std::vector<T> fresh;
    if (live)
      fresh.reserve(stack_capacity - 1);
    for (const Lit lit : order) {
      Head &head = heads_[lit];
      const uint32_t noff = static_cast<uint32_t>(fresh.size());
      fresh.insert(fresh.end(), arena_.begin() + head.offset,
                   arena_.begin() + head.offset + head.size);
      head.offset = noff;
      head.capacity = head.size;
    }
    for (Head &head : heads_)
      if (!head.size)
        head = Head{};
    arena_.swap(fresh);
    holes_ = 0;
  }

  std::vector<T> arena_;
  std::vector<Head> heads_;
  std::vector<uint32_t> pending_; // exact-layout counts, reused across calls
  // arena_.size() - sum(head.size): abandoned images plus unconsumed slice
  // capacity, exactly `vectors.usable`.
  uint64_t holes_ = 0;
};

} // namespace tactix
