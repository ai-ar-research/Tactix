#pragma once

#include "kernel/types.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <exception>
#include <limits>
#include <numeric>
#include <cstring>
#include <memory>
#include <memory_resource>
#include <string_view>
#include <type_traits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace tactix {

struct SymmetryClauseCheckStats {
  uint64_t fixed_clauses = 0;
  uint64_t mapped_clauses = 0;
};

// Caller-owned scratch for repeated checks of immutable clause snapshots.
// Generation marks avoid clearing a formula-sized bitmap for every map.
struct SymmetryClauseVisitWorkspace {
  std::vector<uint32_t> marks;
  uint32_t generation = 0;
  std::vector<size_t> touched;
  std::vector<Lit> mapped;
  std::vector<Lit> literal_image;
};

// Exact clause-multiset index owning one immutable detector snapshot.
// The hasher template permits collision controls without solver settings.
template <class Hasher = std::hash<std::string_view>,
          class BinaryHasher = std::hash<uint64_t>>
class BasicSymmetryClauseMultiset {
  std::vector<std::vector<Lit>> clauses_;
  // Immutable after build: no erase/tombstones and no iteration-order
  // dependency. A zero count denotes an empty bucket, so every key value
  // (including the empty clause and binary key zero) remains representable.
  template <class Key, class Hash> class Counts {
    struct Entry {
      Key key{};
      uint64_t count = 0;
    };
    Hash hash_{};
    std::vector<Entry> entries_;
    size_t size_ = 0;

    size_t bucket(const Key &key) const {
      // std::hash<uint64_t> may be identity. Mix both packed literals before
      // the power-of-two mask; exact key equality still decides membership.
      uint64_t hash = hash_(key);
      if constexpr (std::is_same_v<Key, uint64_t>) {
        hash ^= hash >> 30;
        hash *= 0xbf58476d1ce4e5b9ull;
        hash ^= hash >> 27;
        hash *= 0x94d049bb133111ebull;
        hash ^= hash >> 31;
      }
      return static_cast<size_t>(hash) & (entries_.size() - 1);
    }

  public:
    static size_t capacity_for(size_t maximum_keys) {
      // At most half full guarantees an empty bucket and bounded lookup,
      // even if every hash collides. No growth invalidates build-time counts.
      if (maximum_keys > (SIZE_MAX / 2)) std::terminate();
      const size_t needed = std::max(size_t{2}, 2 * maximum_keys);
      size_t capacity = 2;
      while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) std::terminate();
        capacity *= 2;
      }
      return capacity;
    }
    static size_t storage_for(size_t maximum_keys) {
      const size_t capacity = capacity_for(maximum_keys);
      if (capacity > SIZE_MAX / sizeof(Entry)) std::terminate();
      return capacity * sizeof(Entry);
    }
    void reserve(size_t maximum_keys) {
      const size_t capacity = capacity_for(maximum_keys);
      if (capacity > entries_.max_size()) std::terminate();
      std::vector<Entry>(capacity).swap(entries_);
      size_ = 0;
    }
    size_t size() const { return size_; }
    const uint64_t *find(const Key &key) const {
      if (entries_.empty()) return nullptr;
      size_t at = bucket(key);
      while (entries_[at].count) {
        if (entries_[at].key == key) return &entries_[at].count;
        at = (at + 1) & (entries_.size() - 1);
      }
      return nullptr;
    }
    template <class Own> uint64_t &increment(const Key &key, Own &&own) {
      assert(!entries_.empty());
      size_t at = bucket(key);
      while (entries_[at].count) {
        if (entries_[at].key == key) return ++entries_[at].count;
        at = (at + 1) & (entries_.size() - 1);
      }
      // reserve() receives a physical-clause upper bound. Enforce the empty
      // bucket invariant in Release too, without a check on each read probe.
      if (size_ >= entries_.size() / 2) std::terminate();
      // Persist a new key only after finding its slot. A scratch-backed view
      // is never stored on a hit, and every published entry has positive count.
      entries_[at].key = own();
      entries_[at].count = 1;
      ++size_;
      return entries_[at].count;
    }
    uint64_t &increment(const Key &key) {
      return increment(key, [&] { return key; });
    }
  };
  struct ViewHash {
    Hasher hasher;
    size_t operator()(std::string_view key) const {
      if constexpr (std::is_invocable_v<const Hasher &, std::string_view>)
        return hasher(key);
      else
        return hasher(std::string(key)); // Preserve custom string hashers.
    }
  };
  // Copies share only immutable key bytes. Rebuild creates a fresh arena;
  // declare it before the table so the table dies before its backing bytes.
  std::shared_ptr<std::pmr::monotonic_buffer_resource> canonical_storage_;
  Counts<std::string_view, ViewHash> multiplicity_;
  Counts<uint64_t, BinaryHasher> binary_multiplicity_;
  // Dense domains use one bit per unordered literal pair. The count table
  // then stores only duplicate excesses; absent entries have multiplicity one.
  // Select this representation only when its total storage fits the ordinary
  // table's storage. Sparse literal domains retain the ordinary table.
  std::vector<uint64_t> binary_membership_;
  uint64_t binary_domain_ = 0;
  size_t binary_distinct_count_ = 0;
  std::vector<uint64_t> source_multiplicity_;
  std::vector<std::vector<size_t>> literal_clauses_;

  static uint64_t binary_key(Lit first, Lit second) {
    if (first > second)
      std::swap(first, second);
    return (static_cast<uint64_t>(first) << 32) | second;
  }
  static uint64_t triangular(uint64_t n) {
    return n % 2 ? n * ((n + 1) / 2) : (n / 2) * (n + 1);
  }
  static uint64_t binary_bit(Lit first, Lit second) {
    if (first > second) std::swap(first, second);
    return triangular(second) + first;
  }
  bool binary_present(Lit first, Lit second) const {
    if (first >= binary_domain_ || second >= binary_domain_) return false;
    const uint64_t bit = binary_bit(first, second);
    return binary_membership_[bit / 64] & (uint64_t{1} << (bit % 64));
  }
  // Returns whether this unordered pair was already present.
  bool insert_binary_bit(Lit first, Lit second) {
    const uint64_t bit = binary_bit(first, second);
    auto &word = binary_membership_[bit / 64];
    const uint64_t mask = uint64_t{1} << (bit % 64);
    const bool present = word & mask;
    word |= mask;
    return present;
  }

public:
  static constexpr bool hashes_bytes_in_place =
      std::is_invocable_v<const Hasher &, std::string_view>;

  const std::vector<std::vector<Lit>> &clauses() const { return clauses_; }
  size_t distinct_clause_count() const {
    return multiplicity_.size() + binary_distinct_count_;
  }

  void build(std::vector<std::vector<Lit>> clauses) {
    clauses_ = std::move(clauses);
    source_multiplicity_.clear();
    literal_clauses_.clear();
    const size_t binaries = std::count_if(
        clauses_.begin(), clauses_.end(),
        [](const auto &clause) { return clause.size() == 2; });
    multiplicity_.reserve(clauses_.size() - binaries);
    binary_multiplicity_ = {};
    std::vector<uint64_t>().swap(binary_membership_);
    binary_domain_ = 0;
    binary_distinct_count_ = 0;
    for (const auto &clause : clauses_)
      if (clause.size() == 2)
        binary_domain_ = std::max(binary_domain_,
            uint64_t{std::max(clause[0], clause[1])} + 1);
    const uint64_t words = (triangular(binary_domain_) + 63) / 64;
    const size_t ordinary_bytes = binary_multiplicity_.storage_for(binaries);
    size_t duplicates = 0;
    if (binaries && words <= ordinary_bytes / sizeof(uint64_t) &&
        words <= binary_membership_.max_size()) {
      binary_membership_.assign(static_cast<size_t>(words), 0);
      for (const auto &clause : clauses_)
        if (clause.size() == 2)
          duplicates += insert_binary_bit(clause[0], clause[1]);
      const size_t bitmap_bytes = static_cast<size_t>(words) * sizeof(uint64_t);
      if (binary_multiplicity_.storage_for(duplicates) > ordinary_bytes - bitmap_bytes)
        std::vector<uint64_t>().swap(binary_membership_);
      else
        std::fill(binary_membership_.begin(), binary_membership_.end(), 0);
    }
    const bool dense = !binary_membership_.empty();
    binary_multiplicity_.reserve(dense ? duplicates : binaries);
    binary_distinct_count_ = dense ? binaries - duplicates : 0;
    canonical_storage_ = std::make_shared<std::pmr::monotonic_buffer_resource>();
    source_multiplicity_.reserve(clauses_.size());
    // The tables are fully reserved before insertion. Use stable count
    // addresses during construction, then copy final counts for safe copies.
    std::vector<const uint64_t *> counts;
    counts.reserve(clauses_.size());
    std::vector<Lit> normalized;
    for (const auto &clause : clauses_) {
      if (clause.size() == 2) {
        if (dense) {
          if (insert_binary_bit(clause[0], clause[1]))
            binary_multiplicity_.increment(binary_key(clause[0], clause[1]));
          counts.push_back(nullptr);
          continue;
        }
        auto &count = binary_multiplicity_.increment(binary_key(clause[0], clause[1]));
        counts.push_back(&count);
        continue;
      }
      normalized.assign(clause.begin(), clause.end());
      std::sort(normalized.begin(), normalized.end());
      const std::string_view key = normalized.empty() ? std::string_view{} :
          std::string_view(reinterpret_cast<const char *>(normalized.data()),
                           normalized.size() * sizeof(Lit));
      auto &count = multiplicity_.increment(key, [&] {
        std::string_view owned;
        if (!key.empty()) {
          auto *bytes = static_cast<char *>(canonical_storage_->allocate(key.size(), alignof(Lit)));
          std::memcpy(bytes, key.data(), key.size());
          owned = std::string_view(bytes, key.size());
        }
        return owned;
      });
      counts.push_back(&count);
    }
    if (!dense) binary_distinct_count_ = binary_multiplicity_.size();
    for (size_t i = 0; i < counts.size(); ++i) {
      if (counts[i]) source_multiplicity_.push_back(*counts[i]);
      else {
        const auto &clause = clauses_[i];
        const auto extra = binary_multiplicity_.find(binary_key(clause[0], clause[1]));
        source_multiplicity_.push_back(1 + (extra ? *extra : 0));
      }
    }
    for (size_t clause = 0; clause < clauses_.size(); ++clause)
      for (const Lit literal : clauses_[clause]) {
        if (literal >= literal_clauses_.size())
          literal_clauses_.resize(static_cast<size_t>(literal) + 1);
        literal_clauses_[literal].push_back(clause);
      }
  }

  // PRECONDITION: mapping is a validated bijection on the literal domain.
  // A bijection induces an injection on normalized clauses. Membership plus
  // equal source/image multiplicity on finite support therefore proves exact
  // equality with the snapshot multiset, without constructing an image map.
  // Signedness, retired variables and bounds are checked by the caller first.
  template <class Charge>
  bool preserves(const std::vector<Lit> &mapping, Charge &&charge,
                 SymmetryClauseCheckStats *stats = nullptr) const {
    if (stats)
      *stats = {};
    if (mapping.size() < literal_clauses_.size())
      return false;
    assert(clauses_.size() == source_multiplicity_.size());
    assert(([&] {
      std::vector<bool> seen(mapping.size(), false);
      for (const Lit target : mapping) {
        if (target >= mapping.size() || seen[target])
          return false;
        seen[target] = true;
      }
      return true;
    }()));
    std::vector<Lit> mapped;
    bool valid = true;
    for (size_t i = 0; i < clauses_.size(); ++i) {
      const auto &clause = clauses_[i];
      if (!charge(clause.size()))
        return false;
      bool fixed = false;
      valid &= preserves_clause(i, mapping, mapped, fixed);
      if (stats) {
        stats->fixed_clauses += fixed;
        stats->mapped_clauses += !fixed;
      }
      // Continue charging after mismatches to preserve this reference path.

    }
    return valid;
  }
  // Clauses outside the moved literal support are pointwise fixed. The
  // incidence index includes each physical copy, including repeated literals;
  // the local marks suppress repeated visits, never clause multiplicity.
  template <class Charge>
  bool preserves_support(const std::vector<Lit> &mapping, Charge &&charge,
                         SymmetryClauseCheckStats *stats = nullptr) const {
    return preserves_support_impl<true>(mapping, charge, stats);
  }

  // Under the same validated-bijection precondition, image membership proves
  // equality of the finite clause set. Physical duplicates remain in the
  // snapshot and work accounting, but do not restrict semantic symmetries.
  template <class Charge>
  bool preserves_set_support(const std::vector<Lit> &mapping, Charge &&charge,
                             SymmetryClauseCheckStats *stats = nullptr) const {
    return preserves_support_impl<false>(mapping, charge, stats);
  }

  // Sparse generator equivalent of preserves_set_support, including its work
  // accounting. Identity gaps are charged algebraically; first-touch clause
  // order and every nonidentity occurrence/clause charge remain unchanged.
  // PRECONDITION: a validated bijection with exactly these sorted moved pairs.
  template <class Mapping, class Charge>
  bool preserves_set_moved_reference(const Mapping &mapping,
                                    std::span<const std::pair<Lit, Lit>> moved,
                                    SymmetryClauseVisitWorkspace &workspace,
                                    Charge &&charge,
                                    SymmetryClauseCheckStats *stats = nullptr) const {
    if (stats) *stats = {};
    if (mapping.size() < literal_clauses_.size() || !charge(clauses_.size()))
      return false;
    if (workspace.marks.size() != clauses_.size() ||
        workspace.generation == std::numeric_limits<uint32_t>::max()) {
      workspace.marks.assign(clauses_.size(), 0);
      workspace.generation = 0;
    }
    ++workspace.generation;
    workspace.touched.clear();
    size_t next = 0;
    for (const auto &[source, target] : moved) {
      (void)target;
      if (source >= literal_clauses_.size()) break;
      if (!charge(static_cast<size_t>(source) - next + 1)) return false;
      next = static_cast<size_t>(source) + 1;
      for (const size_t clause : literal_clauses_[source]) {
        if (!charge(1)) return false;
        if (workspace.marks[clause] == workspace.generation) continue;
        workspace.marks[clause] = workspace.generation;
        workspace.touched.push_back(clause);
      }
    }
    if (next < literal_clauses_.size() && !charge(literal_clauses_.size() - next))
      return false;
    if (stats) stats->fixed_clauses = clauses_.size() - workspace.touched.size();
    if (workspace.literal_image.size() != mapping.size()) {
      workspace.literal_image.resize(mapping.size());
      std::iota(workspace.literal_image.begin(), workspace.literal_image.end(), Lit{0});
    }
    struct RestoreImage {
      std::vector<Lit> &image;
      std::span<const std::pair<Lit, Lit>> moved;
      ~RestoreImage() {
        for (const auto &[source, target] : moved) {
          (void)target;
          image[source] = source;
        }
      }
    } restore{workspace.literal_image, moved};
    for (const auto &[source, target] : moved) workspace.literal_image[source] = target;
    bool valid = true;
    for (const size_t clause : workspace.touched) {
      if (!charge(clauses_[clause].size())) return false;
      bool fixed = false;
      valid &= preserves_clause<false>(clause, workspace.literal_image, workspace.mapped, fixed);
      if (stats) {
        stats->fixed_clauses += fixed;
        stats->mapped_clauses += !fixed;
      }
    }
    return valid;
  }

  // PRECONDITION: mapping is a validated bijection, identity outside exactly
  // the supplied moved sources. Mapping needs size() and literal lookup only;
  // it may itself be sparse. No full-domain image or scan is required.
  template <class Mapping, class Charge>
  bool preserves_set_moved(const Mapping &mapping,
                           std::span<const std::pair<Lit, Lit>> moved,
                           SymmetryClauseVisitWorkspace &workspace,
                           Charge &&charge,
                           SymmetryClauseCheckStats *stats = nullptr) const {
    if (stats) *stats = {};
    if (mapping.size() < literal_clauses_.size() || !charge(0))
      return false;
    if (workspace.marks.size() != clauses_.size() ||
        workspace.generation == std::numeric_limits<uint32_t>::max()) {
      if (!charge(clauses_.size())) return false;
      workspace.marks.assign(clauses_.size(), 0);
      workspace.generation = 0;
    }
    ++workspace.generation;
    workspace.touched.clear();
    for (const auto &[source, target] : moved) {
      (void)target;
      if (!charge(1)) return false;
      if (source >= literal_clauses_.size()) continue;
      for (const size_t clause : literal_clauses_[source]) {
        if (!charge(1)) return false;
        if (workspace.marks[clause] == workspace.generation) continue;
        workspace.marks[clause] = workspace.generation;
        workspace.touched.push_back(clause);
      }
    }
    if (stats) stats->fixed_clauses = clauses_.size() - workspace.touched.size();
    // One identity image per batch provides constant-time clause literal
    // lookup. Each row changes and restores only its support; no per-row
    // domain scan and no per-literal binary search through moved pairs.
    if (workspace.literal_image.size() != mapping.size()) {
      if (!charge(mapping.size())) return false;
      workspace.literal_image.resize(mapping.size());
      std::iota(workspace.literal_image.begin(), workspace.literal_image.end(), Lit{0});
    }
    if (moved.size() > UINT64_MAX / 2 || !charge(2 * moved.size())) return false;
    struct RestoreImage {
      std::vector<Lit> &image;
      std::span<const std::pair<Lit, Lit>> moved;
      ~RestoreImage() {
        for (const auto &[source, target] : moved) {
          (void)target;
          image[source] = source;
        }
      }
    } restore{workspace.literal_image, moved};
    for (const auto &[source, target] : moved)
      workspace.literal_image[source] = target;
    bool valid = true;
    for (const size_t clause : workspace.touched) {
      if (!charge(clauses_[clause].size())) return false;
      bool fixed = false;
      valid &= preserves_clause<false>(clause, workspace.literal_image, workspace.mapped, fixed);
      if (stats) {
        stats->fixed_clauses += fixed;
        stats->mapped_clauses += !fixed;
      }
    }
    return valid;
  }

private:
  template <bool MatchMultiplicity, class Charge>
  bool preserves_support_impl(const std::vector<Lit> &mapping, Charge &&charge,
                              SymmetryClauseCheckStats *stats) const {
    if (stats)
      *stats = {};
    if (mapping.size() < literal_clauses_.size())
      return false;
    assert(([&] {
      std::vector<bool> seen(mapping.size(), false);
      for (const Lit target : mapping) {
        if (target >= mapping.size() || seen[target])
          return false;
        seen[target] = true;
      }
      return true;
    }()));
    if (!charge(clauses_.size()))
      return false;
    std::vector<uint8_t> marked(clauses_.size(), 0);
    std::vector<size_t> touched;
    for (size_t literal = 0; literal < literal_clauses_.size(); ++literal) {
      if (!charge(1))
        return false;
      if (mapping[literal] == literal)
        continue;
      for (const size_t clause : literal_clauses_[literal]) {
        if (!charge(1))
          return false;
        if (!marked[clause]) {
          marked[clause] = 1;
          touched.push_back(clause);
        }
      }
    }
    if (stats)
      stats->fixed_clauses = clauses_.size() - touched.size();
    std::vector<Lit> mapped;
    bool valid = true;
    for (const size_t i : touched) {
      if (!charge(clauses_[i].size()))
        return false;
      bool fixed = false;
      valid &= preserves_clause<MatchMultiplicity>(i, mapping, mapped, fixed);
      if (stats) {
        stats->fixed_clauses += fixed;
        stats->mapped_clauses += !fixed;
      }
    }
    return valid;
  }

  template <bool MatchMultiplicity = true, class Mapping>
  bool preserves_clause(size_t i, const Mapping &mapping,
                        std::vector<Lit> &mapped,
                        bool &fixed) const {
    const auto &clause = clauses_[i];
    if (clause.size() == 2) {
      assert(clause[0] < mapping.size() && clause[1] < mapping.size());
      const Lit first = mapping[clause[0]], second = mapping[clause[1]];
      fixed = first == clause[0] && second == clause[1];
      if (fixed)
        return true;
      if (!binary_membership_.empty()) {
        if (!binary_present(first, second)) return false;
        if constexpr (!MatchMultiplicity) return true;
        const auto extra = binary_multiplicity_.find(binary_key(first, second));
        return 1 + (extra ? *extra : 0) == source_multiplicity_[i];
      }
      const auto found = binary_multiplicity_.find(binary_key(first, second));
      return found &&
             (!MatchMultiplicity || *found == source_multiplicity_[i]);
    }
    mapped.resize(clause.size());
    fixed = true;
    for (size_t j = 0; j < clause.size(); ++j) {
      assert(clause[j] < mapping.size());
      mapped[j] = mapping[clause[j]];
      fixed &= mapped[j] == clause[j];
    }
    if (fixed)
      return true;
    std::sort(mapped.begin(), mapped.end());
    const std::string_view key = mapped.empty() ? std::string_view{} :
        std::string_view(reinterpret_cast<const char *>(mapped.data()),
                         mapped.size() * sizeof(Lit));
    const auto found = multiplicity_.find(key);
    return found &&
           (!MatchMultiplicity || *found == source_multiplicity_[i]);
  }

};

using SymmetryClauseMultiset = BasicSymmetryClauseMultiset<>;
static_assert(SymmetryClauseMultiset::hashes_bytes_in_place,
              "production probes must hash clause bytes without a string copy");

} // namespace tactix
