#include "rules/factor/fx.hpp"
#include <algorithm>
#include <unordered_map>

namespace tactix {
namespace {
struct Hash {
  template<class Range> size_t operator()(const Range &xs) const {
    uint64_t hash = 1469598103934665603ull;
    for (auto x : xs) { hash ^= x; hash *= 1099511628211ull; }
    return size_t(hash);
  }
};
struct Parent { Lit other; Ref ref; uint64_t id; };
struct Bucket { std::vector<Parent> positive, negative; };
using Key = std::array<Lit, 4>;
}

std::optional<FxPlan> factor_find_fx(Kernel &k, Lit selector,
                                     std::span<const Ref> sources,
                                     const FxOptions &options, uint64_t &remaining) {
  if (!options.valid() || selector >= 2 * k.nvars || k.level ||
      kernel_var_retired(k, var_of(selector))) return {};
  selector = make_lit(var_of(selector), false);
  auto charge = [&](uint64_t work) {
    if (kernel_terminating(k) || work > remaining ||
        work > UINT64_MAX - k.stats.factor_ticks) return false;
    remaining -= work;
    k.stats.factor_ticks += work;
    return true;
  };
  std::unordered_map<std::vector<Lit>, Bucket, Hash> buckets;
  for (Ref ref : sources) {
    if (!charge(1)) return {};
    const Clause *clause = clause_at(k.arena, ref);
    if (clause->garbage() || clause->detached_reason() || clause->size < 3 ||
        clause->size > options.maximum_clause_size) continue;
    if ((options.scope == FxClauseScope::Irredundant && clause->redundant()) ||
        (options.scope == FxClauseScope::Redundant && !clause->redundant())) continue;
    if (!charge(clause->size)) return {};
    bool positive = false, negative = false, unavailable = false;
    for (Lit l : *clause) {
      positive |= l == selector;
      negative |= l == negate(selector);
      unavailable |= k.value(l) != 0 || kernel_var_retired(k, var_of(l));
    }
    if (unavailable || positive == negative) continue;
    const Lit head = positive ? selector : negate(selector);
    for (Lit other : *clause) {
      if (other == head) continue;
      if (!charge(clause->size)) return {};
      std::vector<Lit> remainder;
      for (Lit l : *clause) if (l != head && l != other) remainder.push_back(l);
      std::sort(remainder.begin(), remainder.end());
      auto &bucket = buckets[remainder];
      (positive ? bucket.positive : bucket.negative).push_back({other, ref, clause->id()});
    }
  }
  std::unordered_map<Key, FxPlan, Hash> plans;
  for (auto &[remainder, bucket] : buckets) {
    auto unique_parents = [](auto &parents) {
      std::sort(parents.begin(), parents.end(), [](const Parent &a, const Parent &b) {
        return a.other != b.other ? a.other < b.other : a.ref < b.ref;
      });
      parents.erase(std::unique(parents.begin(), parents.end(),
          [](const Parent &a, const Parent &b) { return a.other == b.other; }), parents.end());
    };
    unique_parents(bucket.positive);
    unique_parents(bucket.negative);
    for (const Parent &p : bucket.positive) for (const Parent &n : bucket.negative) {
      if (!charge(1 + remainder.size())) return {};
      if (p.other == n.other) continue;
      FxGate gate;
      Lit left = selector, right, otherwise = INVALID_LIT;
      bool negative_output;
      if (p.other == negate(n.other)) {
        if (!(options.gates & unsigned(FxGate::Xor))) continue;
        gate = FxGate::Xor;
        right = make_lit(var_of(p.other), false);
        negative_output = negated(p.other);
        if (right < left) std::swap(left, right);
      } else {
        if (!(options.gates & unsigned(FxGate::Ite)) ||
            var_of(p.other) == var_of(n.other)) continue;
        gate = FxGate::Ite;
        right = n.other;
        otherwise = p.other;
        negative_output = std::pair{negate(right), negate(otherwise)} <
                          std::pair{right, otherwise};
        if (negative_output) { right = negate(right); otherwise = negate(otherwise); }
      }
      const Key key{Lit(gate), left, right, otherwise};
      auto it = plans.try_emplace(key, FxPlan{gate, left, right, otherwise, {}}).first;
      // A gate may have both output polarities for the same remainder.
      it->second.contexts.push_back({p.ref, n.ref, p.id, n.id, remainder, negative_output});
    }
  }
  std::optional<FxPlan> best;
  Key best_key{};
  for (auto &[key, plan] : plans) {
    if (plan.reduction() < int64_t(options.minimum_reduction)) continue;
    if (!best || plan.reduction() > best->reduction() ||
        (plan.reduction() == best->reduction() && key < best_key)) {
      best = std::move(plan);
      best_key = key;
    }
  }
  if (best) std::sort(best->contexts.begin(), best->contexts.end(),
      [](const FxContext &a, const FxContext &b) { return a.positive < b.positive; });
  return best;
}
}
