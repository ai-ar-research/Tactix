#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

namespace tactix {

struct WordAllocator {
  static void *resize(void *block, size_t bytes) {
    return std::realloc(block, bytes);
  }
  static void release(void *block) { std::free(block); }
};

// Private kernel word storage, not a replacement for general std::vector.
// Four-byte implicit-lifetime values need no constructors/destructors. Growth
// may move the block: the existing FlatLists global invalidation rule remains.
// Three pointers retain the old vector footprint. Capacity choices match the
// existing libstdc++ vector operations; this owner changes allocation, not the
// literal-slice growth/packing policy. The allocator template is a fixed-test
// seam; production has no runtime allocator callback or diagnostic counter.
template <class T, class Allocator = WordAllocator> class ReallocWords {
  static_assert(sizeof(T) == sizeof(uint32_t));
  static_assert(std::is_trivially_copyable_v<T> &&
                std::is_trivially_destructible_v<T> &&
                std::is_trivially_default_constructible_v<T>);
  static_assert(alignof(T) <= alignof(std::max_align_t));
  T *first_ = nullptr, *last_ = nullptr, *limit_ = nullptr;

  [[noreturn]] static void allocation_failure() { std::abort(); }
  size_t grown_capacity(size_t wanted) const {
    if (wanted > max_size())
      allocation_failure();
    const size_t old = size();
    const size_t doubled = old > max_size() - old ? max_size() : old + old;
    return std::max(wanted, doubled);
  }

public:
  using value_type = T;
  ReallocWords() = default;
  ~ReallocWords() { Allocator::release(first_); }
  ReallocWords(const ReallocWords &other) {
    reserve(other.size());
    if (other.size()) {
      std::memcpy(first_, other.first_, other.size() * sizeof(T));
      last_ = first_ + other.size();
    }
  }
  ReallocWords &operator=(const ReallocWords &other) {
    if (this == &other)
      return *this;
    reserve(other.size());
    if (other.size())
      std::memcpy(first_, other.first_, other.size() * sizeof(T));
    last_ = first_ ? first_ + other.size() : nullptr;
    return *this;
  }
  ReallocWords(ReallocWords &&other) noexcept { swap(other); }
  ReallocWords &operator=(ReallocWords &&other) noexcept {
    if (this != &other) {
      ReallocWords empty;
      swap(empty);
      swap(other);
    }
    return *this;
  }
  void swap(ReallocWords &other) noexcept {
    std::swap(first_, other.first_);
    std::swap(last_, other.last_);
    std::swap(limit_, other.limit_);
  }
  static constexpr size_t max_size() {
    return static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max()) /
           sizeof(T);
  }
  size_t size() const { return first_ ? static_cast<size_t>(last_ - first_) : 0; }
  size_t capacity() const {
    return first_ ? static_cast<size_t>(limit_ - first_) : 0;
  }
  bool empty() const { return first_ == last_; }
  bool operator==(const ReallocWords &other) const {
    return size() == other.size() &&
           (empty() || std::equal(begin(), end(), other.begin()));
  }
  T *data() { return first_; }
  const T *data() const { return first_; }
  T *begin() { return first_; }
  const T *begin() const { return first_; }
  T *end() { return last_; }
  const T *end() const { return last_; }
  T &operator[](size_t i) { assert(i < size()); return first_[i]; }
  const T &operator[](size_t i) const { assert(i < size()); return first_[i]; }

  // Failure leaves all old values and ownership intact. The normal reserve
  // wrapper terminates on allocation failure, as the exception-disabled old
  // vector path did; it never turns resource exhaustion into a SAT verdict.
  bool try_reserve(size_t wanted) {
    if (wanted > max_size())
      return false;
    if (wanted <= capacity())
      return true;
    const size_t used = size();
    void *block = Allocator::resize(first_, wanted * sizeof(T));
    if (!block)
      return false;
    first_ = static_cast<T *>(block);
    last_ = first_ + used;
    limit_ = first_ + wanted;
    return true;
  }
  void reserve(size_t wanted) {
    if (!try_reserve(wanted))
      allocation_failure();
  }
  void resize(size_t wanted) {
    const size_t used = size();
    if (wanted > capacity())
      reserve(grown_capacity(wanted));
    if (wanted > used)
      std::fill(first_ + used, first_ + wanted, T{});
    last_ = first_ ? first_ + wanted : nullptr;
  }
  void clear() { last_ = first_; }
  // The only caller appends a distinct old arena into a fresh packing arena.
  // It does not pass a range borrowed from this same allocation.
  void insert(T *position, const T *begin, const T *end) {
    assert(position == last_);
    (void)position;
    const size_t count = begin == end ? 0 : static_cast<size_t>(end - begin);
    const size_t used = size();
    if (count > max_size() - used)
      allocation_failure();
    if (used + count > capacity())
      reserve(grown_capacity(used + count));
    if (count) {
      std::memcpy(first_ + used, begin, count * sizeof(T));
      last_ = first_ + used + count;
    }
  }
};

} // namespace tactix
