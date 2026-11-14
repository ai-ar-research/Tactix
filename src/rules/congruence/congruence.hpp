// V6 Congruence: gate congruence closure over AND/XOR/ITE gates; gates with
// equal canonical inputs get the two equivalence binaries, and Substitute
// collapses them. Contract: `docs/dev/calculus.md` V6 (V6a binary units);
// deviations: implementation-notes §2; from kissat congruence.c (MIT).
#pragma once

#include "rules/registry.hpp"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace tactix {

enum class GateKind : uint8_t { And, Xor, Ite };

template <typename T> class CompactVector {
public:
  static_assert(std::is_trivially_copyable<T>::value);
  CompactVector() = default;
  CompactVector(const CompactVector &other) { assign(other.begin(), other.end()); }
  CompactVector &operator=(const CompactVector &other) {
    if (this != &other)
      assign(other.begin(), other.end());
    return *this;
  }
  CompactVector(CompactVector &&other) noexcept { take(other); }
  CompactVector &operator=(CompactVector &&other) noexcept {
    if (this != &other) {
      if (on_heap())
        delete[] storage_.heap;
      capacity_ = INLINE_CAPACITY;
      take(other);
    }
    return *this;
  }
  CompactVector &operator=(std::initializer_list<T> values) {
    assign(values.begin(), values.end());
    return *this;
  }
  ~CompactVector() {
    if (on_heap())
      delete[] storage_.heap;
  }

  void reserve(size_t capacity) {
    if (capacity <= capacity_)
      return;
    T *replacement = new T[capacity];
    for (uint32_t i = 0; i < size_; i++)
      replacement[i] = data()[i];
    if (on_heap())
      delete[] storage_.heap;
    storage_.heap = replacement;
    capacity_ = static_cast<uint32_t>(capacity);
  }
  void push_back(const T &value) {
    if (size_ == capacity_)
      reserve(capacity_ ? 2 * capacity_ : 4);
    data()[size_++] = value;
  }
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  T *begin() { return data(); }
  T *end() { return data() + size_; }
  const T *begin() const { return data(); }
  const T *end() const { return data() + size_; }
  T &operator[](size_t index) { return data()[index]; }
  const T &operator[](size_t index) const { return data()[index]; }

  friend bool operator==(const CompactVector &a, const std::vector<T> &b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
  }
  friend bool operator!=(const CompactVector &a, const std::vector<T> &b) {
    return !(a == b);
  }

private:
  // Move `other`'s elements into this inline, storage-free vector, leaving
  // `other` empty and inline.
  void take(CompactVector &other) noexcept {
    if (other.on_heap()) {
      storage_.heap = other.storage_.heap;
      capacity_ = other.capacity_;
      other.storage_.heap = nullptr;
      other.capacity_ = INLINE_CAPACITY;
    } else
      for (uint32_t i = 0; i < other.size_; i++)
        storage_.inlined[i] = other.storage_.inlined[i];
    size_ = other.size_;
    other.size_ = 0;
  }
  template <typename Iterator> void assign(Iterator begin, Iterator end) {
    if (begin == end) {
      size_ = 0;
      return;
    }
    const size_t count = static_cast<size_t>(end - begin);
    if (count > capacity_) {
      reserve(count);
    }
    size_ = static_cast<uint32_t>(count);
    for (uint32_t i = 0; i < size_; i++)
      data()[i] = begin[i];
  }
  static constexpr uint32_t INLINE_CAPACITY = 2;
  union Storage {
    T *heap;
    T inlined[INLINE_CAPACITY];
    Storage() : heap(nullptr) {}
  } storage_;
  bool on_heap() const { return capacity_ > INLINE_CAPACITY; }
  T *data() { return on_heap() ? storage_.heap : storage_.inlined; }
  const T *data() const {
    return on_heap() ? storage_.heap : storage_.inlined;
  }
  uint32_t size_ = 0;
  uint32_t capacity_ = INLINE_CAPACITY;
};

// Extracted gate, output = f(inputs) (calculus V6). And: `inputs` sorted. Xor:
// sorted and all-positive, parity in `output`'s sign. Ite: [cond, then, else],
// with `normalize_ite` making cond and then positive.
struct CongGate {
  GateKind kind = GateKind::And;
  Lit output = 0;
  CompactVector<Lit> inputs;           // canonical (sorted) — structural key
};

// Extract AND/OR gates from the irredundant large clauses (bounded arity).
// Rule- local; uses the arena scan (dense index is per-literal, not
// all-clauses).
void congruence_extract(const Kernel &, std::vector<CongGate> &out);

// The extraction half of `congruence_apply` (XOR/ITE when `with_xor_ite`);
// exposed so a test can pin the output sign convention.
void congruence_extract_gates(const Kernel &, std::vector<CongGate> &out,
                              bool with_xor_ite);

RuleEligibility congruence_guard(const Kernel &); // quiescent-root precondition

// The canonical full-round apply: extract AND/XOR/ITE gates and run the
// union-find congruence closure, adding equivalence binaries (+ proof). Exposed
// for tests.
ApplyResult congruence_apply(Kernel &);

// Replay-based RUP test at a probing scope: the admission test for callers that
// hold a semantic certificate, identical in every proof format. False means
// emit nothing (`benchmarks/reports/2026-08-20-sweep-drat-admission-gate.md`,
// `tests/test_sweep.cpp::test_rup_admission_is_format_independent`).
bool congruence_replay_rup(Kernel &, const Lit *lits, uint32_t n);

} // namespace tactix
