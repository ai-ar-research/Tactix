// Shared proof-only Boolean counting engine, extracted from the signed
// cardinality proof implementation. No kernel mutation or policy selection.
#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tactix::cardinality_certificate {
using External = int32_t;
using Clause = std::vector<External>;
// Sink contract: stopped; charge_work(units), new_variable(out), add(clause,RAT),
// record_definition(auxiliary,condition,high,low), all returning success.
// Fresh identities must be collision-free. high/low are positive helper literals
// or INT64_MAX (true) / INT64_MIN (false), defining aux <-> ITE(condition,high,low).
// A validated root unit may only be requested for a certified input or consequence.
// The engine does not prove its supplied premises. Failed calls may have emitted
// a valid prefix: native callers must count-only preflight before atomic commit.
// Normalized inequalities use strictly increasing positive variables, nonzero
// coefficients and representable signed coefficient sums. INT32_MAX is reserved.
template <class Sink> class BddProof {
  using Ref = uint32_t;
  static constexpr Ref FALSE_REF = 0;
  static constexpr Ref TRUE_REF = 1;
  static constexpr int64_t TRUE_LITERAL = INT64_MAX;
  static constexpr int64_t FALSE_LITERAL = INT64_MIN;

  struct Node {
    External variable = 0;
    Ref high = FALSE_REF;
    Ref low = FALSE_REF;
    External proof_variable = 0;
  };
  struct NodeKey {
    External variable;
    Ref high;
    Ref low;
    friend bool operator==(const NodeKey &, const NodeKey &) = default;
  };
  struct NodeKeyHash {
    size_t operator()(const NodeKey &key) const {
      uint64_t hash = static_cast<uint32_t>(key.variable);
      hash = hash * 0x9e3779b185ebca87ULL + key.high;
      hash = hash * 0x9e3779b185ebca87ULL + key.low;
      return static_cast<size_t>(hash);
    }
  };
  struct PairKey {
    Ref first;
    Ref second;
    friend bool operator==(const PairKey &, const PairKey &) = default;
  };
  struct PairKeyHash {
    size_t operator()(const PairKey &key) const {
      return static_cast<size_t>((uint64_t{key.first} << 32) | key.second);
    }
  };
  struct ImplicationKey {
    Ref first;
    Ref second;
    Ref result;
    friend bool operator==(const ImplicationKey &,
                           const ImplicationKey &) = default;
  };
  struct ImplicationKeyHash {
    size_t operator()(const ImplicationKey &key) const {
      uint64_t hash = key.first;
      hash = hash * 0x9e3779b185ebca87ULL + key.second;
      hash = hash * 0x9e3779b185ebca87ULL + key.result;
      return static_cast<size_t>(hash);
    }
  };

  Sink &sink_;
  std::vector<Node> nodes_{{}, {}};
  std::unordered_map<NodeKey, Ref, NodeKeyHash> unique_;
  std::unordered_map<PairKey, Ref, PairKeyHash> and_cache_;
  std::unordered_set<ImplicationKey, ImplicationKeyHash> implication_cache_;

  bool charge(uint64_t amount) {
    if (sink_.stopped) return false;
    if (sink_.charge_work(amount)) return true;
    sink_.stopped = true;
    return false;
  }
  bool emit(const Clause &clause, bool rat) {
    if (sink_.stopped) return false;
    if (sink_.add(clause, rat)) return true;
    sink_.stopped = true;
    return false;
  }
  bool leaf(Ref ref) const { return ref <= TRUE_REF; }
  bool value(Ref ref) const { return ref == TRUE_REF; }
  External variable(Ref ref) const {
    return leaf(ref) ? INT32_MAX : nodes_[ref].variable;
  }
  Ref high(Ref ref, External split) const {
    return !leaf(ref) && nodes_[ref].variable == split ? nodes_[ref].high : ref;
  }
  Ref low(Ref ref, External split) const {
    return !leaf(ref) && nodes_[ref].variable == split ? nodes_[ref].low : ref;
  }
  int64_t literal(Ref ref, bool positive) const {
    if (ref == TRUE_REF)
      return positive ? TRUE_LITERAL : FALSE_LITERAL;
    if (ref == FALSE_REF)
      return positive ? FALSE_LITERAL : TRUE_LITERAL;
    return positive ? nodes_[ref].proof_variable
                    : -int64_t(nodes_[ref].proof_variable);
  }

  bool clean(const std::vector<int64_t> &raw, Clause &clause,
             bool &tautology) const {
    clause.clear();
    tautology = false;
    for (const int64_t item : raw) {
      if (item == TRUE_LITERAL) {
        tautology = true;
        clause.clear();
        return true;
      }
      if (item == FALSE_LITERAL)
        continue;
      if (!item || item <= INT32_MIN || item > INT32_MAX)
        return false;
      const External literal = static_cast<External>(item);
      bool duplicate = false;
      for (const External prior : clause) {
        if (prior == -literal) {
          tautology = true;
          clause.clear();
          return true;
        }
        duplicate |= prior == literal;
      }
      if (!duplicate)
        clause.push_back(literal);
    }
    return true;
  }

  bool add_raw(const std::vector<int64_t> &raw, bool rat,
               Clause *normalized = nullptr) {
    Clause clause;
    bool tautology = false;
    if (!clean(raw, clause, tautology)) {
      sink_.stopped = true;
      return false;
    }
    if (normalized)
      *normalized = clause;
    return tautology || emit(clause, rat);
  }

  bool emit_definitions(Ref ref) {
    const Node &node = nodes_[ref];
    const int64_t proof = node.proof_variable;
    const int64_t split = node.variable;
    return add_raw({proof, -split, literal(node.high, false)}, true) &&
           add_raw({proof, split, literal(node.low, false)}, true) &&
           add_raw({-proof, -split, literal(node.high, true)}, true) &&
           add_raw({-proof, split, literal(node.low, true)}, true);
  }

  Ref make_node(External variable, Ref high_ref, Ref low_ref) {
    if (sink_.stopped || !charge(1)) return FALSE_REF;
    if (high_ref == low_ref)
      return high_ref;
    const NodeKey key{variable, high_ref, low_ref};
    if (const auto found = unique_.find(key); found != unique_.end())
      return found->second;
    External proof_variable = 0;
    if (!sink_.new_variable(proof_variable)) {
      sink_.stopped = true;
      return FALSE_REF;
    }
    const Ref ref = static_cast<Ref>(nodes_.size());
    nodes_.push_back(Node{variable, high_ref, low_ref, proof_variable});
    unique_.emplace(key, ref);
    if (!sink_.record_definition(proof_variable, variable,
                                 literal(high_ref, true), literal(low_ref, true))) {
      sink_.stopped = true;
      return FALSE_REF;
    }
    if (!emit_definitions(ref))
      return FALSE_REF;
    return ref;
  }

  Ref and_bdd(Ref first, Ref second) {
    if (sink_.stopped || !charge(1)) return FALSE_REF;
    if (first == FALSE_REF || second == FALSE_REF)
      return FALSE_REF;
    if (first == TRUE_REF)
      return second;
    if (second == TRUE_REF || first == second)
      return first;
    if (second < first)
      std::swap(first, second);
    const PairKey key{first, second};
    if (const auto found = and_cache_.find(key); found != and_cache_.end())
      return found->second;
    const External split = std::min(variable(first), variable(second));
    const Ref result = make_node(split,
                                 and_bdd(high(first, split), high(second, split)),
                                 and_bdd(low(first, split), low(second, split)));
    and_cache_.emplace(key, result);
    return result;
  }

  bool target_clause(Ref first, Ref second, Ref result,
                     std::vector<int64_t> &raw) const {
    raw = {literal(first, false), literal(second, false),
           literal(result, true)};
    return true;
  }

public:
  explicit BddProof(Sink &sink) : sink_(sink) {
    unique_.reserve(8192);
    and_cache_.reserve(8192);
    implication_cache_.reserve(32768);
  }

  Ref build_clause(const Clause &clause) {
    if (sink_.stopped || !charge(clause.size() + 1)) return FALSE_REF;
    // Reject malformed premises before reserving a helper or emitting a byte.
    std::unordered_set<External> seen;
    for (const External literal : clause) {
      if (!literal || literal == INT32_MIN || literal == INT32_MAX ||
          literal == -INT32_MAX || !seen.insert(std::abs(literal)).second) {
        sink_.stopped = true;
        return FALSE_REF;
      }
    }
    std::vector<External> sorted = clause;
    std::sort(sorted.begin(), sorted.end(), [](External a, External b) {
      return std::abs(a) > std::abs(b);
    });
    Ref root = FALSE_REF;
    for (const External literal : sorted) {
      const External variable = static_cast<External>(
          literal < 0 ? -int64_t(literal) : int64_t(literal));
      root = literal > 0 ? make_node(variable, TRUE_REF, root)
                         : make_node(variable, root, TRUE_REF);
      if (sink_.stopped)
        break;
    }
    return root;
  }

  Ref build_constraint(const std::vector<std::pair<External, int64_t>> &terms,
                       int64_t bound) {
    const size_t count = terms.size();
    if (sink_.stopped || !charge(count + 1)) return FALSE_REF;
    if (count > UINT32_MAX) { sink_.stopped = true; return FALSE_REF; }
    External previous = 0;
    int64_t negative = 0, positive = 0;
    for (const auto &[variable, coefficient] : terms) {
      if (variable <= previous || variable == INT32_MAX || !coefficient ||
          (coefficient < 0 && negative < INT64_MIN - coefficient) ||
          (coefficient > 0 && positive > INT64_MAX - coefficient)) {
        sink_.stopped = true;
        return FALSE_REF;
      }
      previous = variable;
      if (coefficient < 0) negative += coefficient;
      else positive += coefficient;
    }
    std::vector<int64_t> minimum(count + 1, 0), maximum(count + 1, 0);
    for (size_t at = count; at-- > 0;) {
      minimum[at] = minimum[at + 1] + std::min<int64_t>(0, terms[at].second);
      maximum[at] = maximum[at + 1] + std::max<int64_t>(0, terms[at].second);
    }
    struct State {
      uint32_t at;
      int64_t need;
      bool operator==(const State &other) const {
        return at == other.at && need == other.need;
      }
    };
    struct Hash {
      size_t operator()(const State &state) const {
        return static_cast<size_t>((uint64_t{state.at} << 32) ^
                                   static_cast<uint64_t>(state.need));
      }
    };
    std::unordered_map<State, Ref, Hash> memo;
    // Avoid quadratic speculative allocation; states grow under the work bound.
    memo.reserve(count);
    const auto build = [&](auto &&self, uint32_t at, int64_t need) -> Ref {
      if (sink_.stopped || !charge(1)) return FALSE_REF;
      if (maximum[at] < need)
        return FALSE_REF;
      if (minimum[at] >= need)
        return TRUE_REF;
      const State state{at, need};
      if (const auto found = memo.find(state); found != memo.end())
        return found->second;
      assert(at < count);
      const auto [variable, coefficient] = terms[at];
      const auto high_branch = [&]() -> Ref {
        // Saturation is semantic: an unrepresentably high residual is false,
        // and an unrepresentably low residual is true. Never overflow int64.
        if (coefficient < 0 && need > INT64_MAX + coefficient) return FALSE_REF;
        if (coefficient > 0 && need < INT64_MIN + coefficient) return TRUE_REF;
        return self(self, at + 1, need - coefficient);
      };
      const Ref result = make_node(variable, high_branch(),
                                   self(self, at + 1, need));
      memo.emplace(state, result);
      return result;
    };
    return build(build, 0, bound);
  }

  bool prove_and_imply(Ref first, Ref second, Ref result) {
    if (sink_.stopped || !charge(1)) return false;
    if (second < first)
      std::swap(first, second);
    const ImplicationKey key{first, second, result};
    if (implication_cache_.count(key))
      return true;
    if ((leaf(first) && !value(first)) || (leaf(second) && !value(second)) ||
        (leaf(result) && value(result))) {
      implication_cache_.insert(key);
      return true;
    }
    if (leaf(first) && leaf(second) && leaf(result))
      return false;

    External split = std::min(variable(first), variable(second));
    split = std::min(split, variable(result));
    if (split == INT32_MAX)
      return false;
    if (!prove_and_imply(high(first, split), high(second, split),
                         high(result, split)) ||
        !prove_and_imply(low(first, split), low(second, split),
                         low(result, split)))
      return false;

    std::vector<int64_t> raw;
    target_clause(first, second, result, raw);
    Clause target;
    bool target_tautology = false;
    if (!clean(raw, target, target_tautology))
      return false;
    if (!target_tautology) {
      std::vector<int64_t> high_raw;
      high_raw.reserve(target.size() + 1);
      high_raw.push_back(-int64_t(split));
      for (const External literal : target)
        high_raw.push_back(literal);
      Clause intermediate;
      bool intermediate_tautology = false;
      if (!clean(high_raw, intermediate, intermediate_tautology))
        return false;
      if (!intermediate_tautology && !emit(intermediate, false))
        return false;
      if ((intermediate_tautology || intermediate != target) &&
          !emit(target, false))
        return false;
    }
    implication_cache_.insert(key);
    return !sink_.stopped;
  }

  bool unit(Ref ref) {
    if (sink_.stopped || !charge(1)) return false;
    return add_raw({literal(ref, true)}, false);
  }

  Ref conjunction(std::vector<Ref> roots) {
    if (sink_.stopped || !charge(roots.size() + 1)) return FALSE_REF;
    while (roots.size() > 1 && !sink_.stopped) {
      std::vector<Ref> next;
      next.reserve((roots.size() + 1) / 2);
      for (size_t at = 0; at < roots.size(); at += 2) {
        if (at + 1 == roots.size()) {
          next.push_back(roots[at]);
          continue;
        }
        const Ref first = roots[at], second = roots[at + 1];
        const Ref result = and_bdd(first, second);
        if (result != first && result != second) {
          if (!prove_and_imply(first, second, result) || !unit(result))
            return FALSE_REF;
        }
        next.push_back(result);
      }
      roots = std::move(next);
    }
    return roots.empty() ? TRUE_REF : roots.front();
  }

  bool validate_clause(const Clause &clause, Ref &root) {
    root = build_clause(clause);
    return !sink_.stopped && unit(root);
  }

  bool imply_and_validate(Ref first, Ref second, Ref result) {
    return prove_and_imply(first, second, result) && unit(result);
  }

  int64_t root_literal(Ref ref) const {
    assert(ref < nodes_.size());
    return literal(ref, true);
  }
  Ref truth() const { return TRUE_REF; }
  Ref falsity() const { return FALSE_REF; }
};

} // namespace tactix::cardinality_certificate
