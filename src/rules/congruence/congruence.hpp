// V6 Congruence — gate congruence closure over AND/XOR/ITE gates: two gates
// with the same canonical inputs have equivalent outputs, so add the two
// equivalence binaries. It does not substitute; V3 Substitute, run next in the
// phase, collapses the equivalences. Precondition, effect, proof delta and
// soundness: `docs/calculus.md` V6 (and V6a for the binary-resolution unit
// channel); deviations: `docs/implementation-notes.md` §2; transliterated from
// kissat congruence.c (MIT).
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
  CompactVector(CompactVector &&other) noexcept {
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
  CompactVector &operator=(CompactVector &&other) noexcept {
    if (this != &other) {
      if (on_heap())
        delete[] storage_.heap;
      capacity_ = INLINE_CAPACITY;
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

// Extracted gate, output = f(inputs) per kind (`docs/calculus.md` V6 gives each
// kind's defining clauses). Storage conventions the closure relies on:
//   And: `inputs` sorted, `input_bin_ids` the (¬output ∨ inputᵢ) ids, parallel.
//   Xor: `inputs` sorted and all-positive — the clause parity is absorbed into
//        `output`'s sign; `base_id` is the all-positive base.
//   Ite: `inputs` = [cond, then, else] in that fixed order, not sorted;
//        `normalize_ite` puts cond and then positive, absorbing any flip into
//        the output sign.
struct CongGate {
  struct ProofData {
    uint64_t base_id = 0;
    std::vector<uint64_t> input_bin_ids;
  };
  GateKind kind = GateKind::And;
  Lit output = 0;
  CompactVector<Lit> inputs;           // canonical (sorted) — structural key
  std::unique_ptr<ProofData> proof_data;

  void set_base_id(uint64_t id) { mutable_proof_data().base_id = id; }
  uint64_t base_id() const { return proof_data ? proof_data->base_id : 0; }
  void reserve_input_bin_ids(size_t size) {
    mutable_proof_data().input_bin_ids.reserve(size);
  }
  void push_input_bin_id(uint64_t id) {
    mutable_proof_data().input_bin_ids.push_back(id);
  }
  uint64_t &input_bin_id(size_t index) {
    return proof_data->input_bin_ids[index];
  }
  const std::vector<uint64_t> &input_bin_ids() const {
    return proof_data ? proof_data->input_bin_ids : empty_input_bin_ids();
  }

private:
  ProofData &mutable_proof_data() {
    if (!proof_data)
      proof_data.reset(new ProofData);
    return *proof_data;
  }
  static const std::vector<uint64_t> &empty_input_bin_ids() {
    static const std::vector<uint64_t> empty;
    return empty;
  }
};

// Extract AND/OR gates from the irredundant large clauses (bounded arity). Rule-
// local; uses the arena scan (dense index is per-literal, not all-clauses).
void congruence_extract(const Kernel &, std::vector<CongGate> &out);

// The extraction half of `congruence_apply`: AND gates always, XOR and ITE when
// `with_xor_ite`. Exposed so a test can pin the `output` sign convention
// (output must evaluate to f(inputs)): the closure only compares two gates'
// outputs to each other, where a shared mis-signing cancels, but a unit/merge
// channel reading that polarity has no such luxury.
void congruence_extract_gates(const Kernel &, std::vector<CongGate> &out,
                              bool with_xor_ite);

RuleEligibility congruence_guard(const Kernel &); // quiescent-root precondition

// The canonical full-round apply: extract AND/XOR/ITE gates and run the union-find
// congruence closure, adding equivalence binaries (+ proof). Exposed for tests.
ApplyResult congruence_apply(Kernel &);

// What a caller wants the replay for; reading one as the other silently removes
// an admission gate (`benchmarks/reports/2026-08-20-sweep-drat-admission-gate.md`,
// pinned by `tests/test_sweep.cpp::test_rup_admission_is_format_independent`).
//
//  - `ChainOnly` — the lemma is RUP by construction (Congruence's own
//    families), so the replay only reads its antecedent chain and may be
//    skipped whenever no chain is written; kissat likewise emits these lines
//    directly (`learn_congruence_unit`, `add_binary_clause`).
//  - `Decide` — the replay is the caller's admission test: Sweep and
//    HyperBinaryResolution hold a semantic certificate and ask the separate
//    question of whether a checker can re-derive the line by unit propagation.
//    Same answer in every proof format, so it must run in all of them.
enum class RupReplay : uint8_t { ChainOnly, Decide };

// Replay-based RUP chain builder: assume the negation of `lits` at a probing
// scope, propagate, and read the antecedents off the conflict's reason walk, so
// the chain comes out in the checker's own propagation order by construction.
// Returns false when the lemma is not RUP over the current DB, in which case
// the caller must emit nothing. Shared with Sweep's RUP fast path.
bool congruence_replay_rup(Kernel &, const Lit *lits, uint32_t n,
                           std::vector<uint64_t> &chain,
                           RupReplay purpose = RupReplay::ChainOnly);

} // namespace tactix
