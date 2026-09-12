#pragma once

#include "kernel/occ.hpp"

#include <iterator>

namespace tactix {

// Eliminate's mixed stream with a large-only projection. One owner retains every entry,
// including duplicates and stale large references. A cached population and a
// read-only filtered range replace the second formula-wide payload image.
// Probe and forward's partial index are separate migration obligations.
class ConnectedOccurrences {
public:
  struct Entry {
    uint32_t payload : 31;
    uint32_t binary : 1;
    static constexpr uint32_t MAX_PAYLOAD = UINT32_MAX >> 1;
    static Entry from_binary(Lit other) {
      assert(other <= MAX_PAYLOAD);
      return {other, true};
    }
    static Entry from_large(Ref ref) {
      assert(ref <= MAX_PAYLOAD);
      return {ref, false};
    }
    bool operator==(const Entry &) const = default;
  };
  static_assert(sizeof(Entry) == sizeof(uint32_t));

  class LargeIterator {
    const Entry *at_ = nullptr, *end_ = nullptr;
    void skip_binary() {
      while (at_ != end_ && at_->binary)
        ++at_;
    }
  public:
    using value_type = Ref;
    using reference = Ref;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using iterator_category = std::forward_iterator_tag;
    LargeIterator() = default;
    LargeIterator(const Entry *at, const Entry *end) : at_(at), end_(end) {
      skip_binary();
    }
    Ref operator*() const {
      assert(at_ != end_ && !at_->binary);
      return at_->payload;
    }
    LargeIterator &operator++() {
      assert(at_ != end_);
      ++at_;
      skip_binary();
      return *this;
    }
    LargeIterator operator++(int) {
      const LargeIterator old = *this;
      ++*this;
      return old;
    }
    bool operator==(const LargeIterator &) const = default;
  };

  struct LargeRange {
    const Entry *first, *last;
    uint32_t count;
    LargeIterator begin() const { return {first, last}; }
    LargeIterator end() const { return {last, last}; }
    uint32_t size() const { return count; }
    bool empty() const { return !count; }
  };

  size_t lits() const { return streams_.lits(); }
  size_t size() const { return lits(); }
  bool empty() const { return !lits(); }
  uint32_t size(Lit lit) const { return streams_.size(lit); }
  uint32_t large_size(Lit lit) const { return large_sizes_[lit]; }
  uint32_t binary_size(Lit lit) const { return size(lit) - large_size(lit); }
  FlatSpan<const Entry> entries(Lit lit) const {
    // Avoid pointer arithmetic on an empty arena; empty lists share no range.
    if (!size(lit))
      return {nullptr, nullptr};
    return streams_[lit];
  }
  FlatSpan<const Entry> operator[](Lit lit) const { return entries(lit); }
  LargeRange large_refs(Lit lit) const {
    const auto span = entries(lit);
    return {span.first, span.last, large_size(lit)};
  }

  // As with FlatLists, all ranges expire at any append, explicit packing or
  // rebuild, and mutations invalidate iterators to the changed list. There
  // is no retained arena pointer and copying the owner makes an independent
  // image. Capacity/packing policy stays with the existing flat-list owner.
  void begin_exact_layout(size_t nlits) {
    streams_.begin_exact_layout(nlits);
    large_sizes_.assign(nlits, 0);
  }
  void count(Lit lit, uint32_t n = 1) { streams_.count(lit, n); }
  void finish_exact_layout() { streams_.finish_exact_layout(); }
  void resize_lits(size_t nlits) {
    streams_.resize_lits(nlits);
    large_sizes_.resize(nlits, 0);
  }
  void push(Lit lit, Entry entry) {
    streams_.push(lit, entry);
    large_sizes_[lit] += !entry.binary;
  }
  void clear(Lit lit) {
    streams_.clear(lit);
    large_sizes_[lit] = 0;
  }
  void erase_at(Lit lit, size_t at) {
    assert(at < size(lit));
    large_sizes_[lit] -= !streams_[lit][at].binary;
    streams_.erase_at(lit, at);
  }
  void replace_at(Lit lit, size_t at, Entry entry) {
    assert(at < size(lit));
    auto span = streams_[lit];
    large_sizes_[lit] -= !span[at].binary;
    span[at] = entry;
    large_sizes_[lit] += !entry.binary;
  }
  bool erase_large_once(Lit lit, Ref ref) {
    const auto span = entries(lit);
    size_t at = 0;
    for (const Entry entry : span) {
      if (!entry.binary && entry.payload == ref) {
        erase_at(lit, at);
        return true;
      }
      ++at;
    }
    return false;
  }
  // Stable one-pass publication. The predicate sees each original entry once
  // in mixed order and must not mutate this owner. It may inspect Kernel, but
  // this storage layer does not invent garbage/assignment/score semantics.
  template <class Keep> void compact(Lit lit, Keep keep) {
    if (!size(lit))
      return;
    auto span = streams_[lit];
    uint32_t out = 0, large = 0;
    for (const Entry entry : span) {
      if (!keep(entry))
        continue;
      span[out++] = entry;
      large += !entry.binary;
    }
    streams_.shrink(lit, out);
    large_sizes_[lit] = large;
  }
  template <class Keep> void compact_large(Lit lit, Keep keep) {
    compact(lit, [&](Entry entry) {
      return entry.binary || keep(static_cast<Ref>(entry.payload));
    });
  }
  void defrag_if_needed() { streams_.defrag_if_needed(); }
  uint64_t relocations() const { return streams_.relocations; }
  uint64_t defrags() const { return streams_.defrags; }
  uint64_t take_relocations() {
    const uint64_t count = streams_.relocations;
    streams_.relocations = 0;
    return count;
  }
  uint64_t take_defrags() {
    const uint64_t count = streams_.defrags;
    streams_.defrags = 0;
    return count;
  }
  const void *arena_data() const { return streams_.arena_data(); }
  const void *heads_data() const { return streams_.heads_data(); }
  const void *counts_data() const { return large_sizes_.data(); }
  size_t arena_capacity_bytes() const { return streams_.arena_capacity_bytes(); }
  size_t heads_capacity_bytes() const { return streams_.heads_capacity_bytes(); }
  size_t payload_capacity_bytes() const { return streams_.arena_capacity_bytes(); }
  size_t count_capacity_bytes() const { return large_sizes_.capacity() * sizeof(uint32_t); }
  void release() {
    streams_.release();
    std::vector<uint32_t>().swap(large_sizes_);
  }

private:
  FlatLists<Entry> streams_;
  std::vector<uint32_t> large_sizes_;
};

static_assert(std::forward_iterator<ConnectedOccurrences::LargeIterator>);

} // namespace tactix
