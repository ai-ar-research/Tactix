// Kissat vector scheme (vector.c, inlinevector.h; MIT) for the dense
// occurrence index: every literal's list is a slice of ONE flat arena rather
// than an independent allocation. A tail slice extends in place while the
// arena allocation has room; otherwise it relocates to a doubled slice and the
// abandoned words join `holes`, exactly `vectors.usable`. Only an explicit
// checkpoint defragments, so a reader's pointers stay valid until it pushes.
#pragma once

#include "kernel/types.hpp"
#include "kernel/realloc_words.hpp"
#include "kernel/vector_stack.hpp"

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
    ReallocWords<T>().swap(arena_);
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

  // The collect.h checkpoint shared with watch.hpp (vector_stack.hpp).
  void defrag_if_needed() {
    if (!vector_stack_needs_defrag(arena_, holes_))
      return;
    defrags++;
    vector_stack_defrag(arena_, heads_, holes_);
  }

private:
  void grow(Lit l) {
    if (vector_stack_grow(arena_, heads_, l, holes_))
      relocations++;
  }

  ReallocWords<T> arena_;
  std::vector<Head> heads_;
  std::vector<uint32_t> pending_; // exact-layout counts, reused across calls
  // arena_.size() - sum(head.size): abandoned images plus unconsumed slice
  // capacity, exactly `vectors.usable`.
  uint64_t holes_ = 0;
};

} // namespace tactix
