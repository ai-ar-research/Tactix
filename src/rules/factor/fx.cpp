#include "rules/factor/fx.hpp"
#include "rules/extended_resolution/extended_resolution.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <memory_resource>
#include <unordered_map>
#include <unordered_set>

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
struct Bucket {
  explicit Bucket(std::pmr::memory_resource *resource)
      : positive(resource), negative(resource) {}
  std::pmr::vector<Parent> positive, negative;
};
using Key = std::array<Lit, 4>;
bool charge_work(Kernel &k, uint64_t &remaining, uint64_t work, uint64_t &meter) {
  if (kernel_terminating(k) || work > remaining ||
      work > UINT64_MAX - meter) return false;
  remaining -= work;
  meter += work;
  return true;
}
}

std::optional<FxPlan> factor_find_fx(Kernel &k, Lit selector,
                                     std::span<const Ref> sources,
                                     const FxOptions &options, uint64_t &remaining,
                                     Lit partner_limit, uint64_t *work_meter) {
  if (!options.valid() || selector >= 2 * k.nvars || selector >= partner_limit || k.level ||
      kernel_var_retired(k, var_of(selector))) return {};
  selector = make_lit(var_of(selector), false);
  uint64_t &meter = work_meter ? *work_meter : k.stats.factor_ticks;
  struct Candidate { FxPlan plan; uint64_t work; };
  // The temporary occurrence tables share one allocation lifetime. The plan
  // and its contexts own their storage after discovery returns.
  std::byte local[4096];
  std::pmr::monotonic_buffer_resource scratch(local, sizeof local);
  std::pmr::unordered_map<Key, Candidate, Hash> plans(&scratch);
  Candidate *best = nullptr; // unordered_map rehash preserves node references.
  Key best_key{};
  auto finish = [&]() -> std::optional<FxPlan> {
    if (!best || kernel_terminating(k)) return {};
    std::sort(best->plan.contexts.begin(), best->plan.contexts.end(),
        [](const FxContext &a, const FxContext &b) { return a.positive < b.positive; });
    return std::move(best->plan);
  };
  auto charge = [&](uint64_t work) {
    if (work > remaining || (best && best->work > remaining - work)) return false;
    return charge_work(k, remaining, work, meter);
  };
  std::pmr::unordered_map<Var, std::array<uint64_t, 2>> partner_counts(&scratch);
  std::pmr::vector<std::pair<Ref, bool>> eligible(&scratch);
  for (Ref ref : sources) {
    if (!charge(1)) return finish();
    const Clause *clause = clause_at(k.arena, ref);
    if (clause->garbage() || clause->detached_reason() || clause->size < 3 ||
        clause->size > options.maximum_clause_size) continue;
    if ((options.scope == FxClauseScope::Irredundant && clause->redundant()) ||
        (options.scope == FxClauseScope::Redundant && !clause->redundant())) continue;
    if (!charge(clause->size)) return finish();
    bool positive = false, negative = false, unavailable = false;
    for (Lit l : *clause) {
      positive |= l == selector;
      negative |= l == negate(selector);
      unavailable |= k.value(l) != 0 || kernel_var_retired(k, var_of(l));
    }
    if (unavailable || positive == negative) continue;
    const Lit head = positive ? selector : negate(selector);
    if (!charge(clause->size)) return finish();
    eligible.emplace_back(ref, positive);
    for (Lit other : *clause)
      if (other != head && other < partner_limit)
        ++partner_counts[var_of(other)][positive];
  }
  // Each context needs a distinct parent on each side of the selector.
  // Count both partner polarities because one definition can use either sign.
  const uint64_t required = uint64_t(options.minimum_reduction) + 4;
  const auto supported = [&](bool positive, Lit partner) {
    const auto it = partner_counts.find(var_of(partner));
    return it != partner_counts.end() && it->second[positive] >= required;
  };
  const bool allow_ite = options.gates & unsigned(FxGate::Ite);
  std::pmr::unordered_map<std::pmr::vector<Lit>, Bucket, Hash> buckets(&scratch);
  std::pmr::vector<Lit> tail(&scratch), key_buffer(&scratch);
  for (const auto [ref, positive] : eligible) {
    if (!charge(1)) return finish();
    const Clause *clause = clause_at(k.arena, ref);
    const Lit head = positive ? selector : negate(selector);
    tail.clear();
    for (Lit other : *clause) {
      if (other == head || other >= partner_limit || !supported(positive, other) ||
          (!allow_ite && !supported(!positive, other))) continue;
      if (!charge(clause->size)) return finish();
      if (tail.empty()) {
        tail.reserve(clause->size - 1);
        key_buffer.reserve(clause->size - 2);
        for (Lit l : *clause) if (l != head) tail.push_back(l);
        std::sort(tail.begin(), tail.end());
      }
      key_buffer.clear();
      for (Lit l : tail) if (l != other) key_buffer.push_back(l);
      auto &bucket = buckets.try_emplace(key_buffer, &scratch).first->second;
      (positive ? bucket.positive : bucket.negative).push_back({other, ref, clause->id()});
    }
  }
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
      if (!charge(1 + remainder.size())) return finish();
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
      auto &candidate = plans.try_emplace(key, Candidate{
          FxPlan{gate, left, right, otherwise, k}, FxPlan::gate_work()}).first->second;
      const uint64_t extra = FxPlan::context_work(remainder.size());
      if (extra > remaining || candidate.work > remaining - extra) continue;
      // A gate may have both output polarities for the same remainder.
      candidate.plan.contexts.push_back({p.ref, n.ref, p.id, n.id,
          std::vector<Lit>(remainder.begin(), remainder.end()), negative_output});
      candidate.work += extra;
      if (candidate.plan.reduction() >= int64_t(options.minimum_reduction) &&
          (!best || candidate.plan.reduction() > best->plan.reduction() ||
           (candidate.plan.reduction() == best->plan.reduction() && key < best_key))) {
        best = &candidate;
        best_key = key;
      }
    }
  }
  return finish();
}

int32_t factor_publish_fx(Kernel &k, const FxPlan &plan, uint64_t &remaining,
                          uint64_t *work_meter) {
  if (plan.owner != &k || plan.arena_generation != k.arena_generation ||
      !extended_resolution_guard(k) || plan.reduction() < 0 ||
      k.arena.size() >= INVALID_REF ||
      (plan.gate != FxGate::Xor && plan.gate != FxGate::Ite)) return 0;
  auto available = [&](Lit l) {
    return l < 2 * k.nvars && !k.value(l) && !kernel_var_retired(k, var_of(l));
  };
  if (!available(plan.left) || !available(plan.right) ||
      var_of(plan.left) == var_of(plan.right)) return 0;
  if (plan.gate == FxGate::Ite &&
      (!available(plan.otherwise) || var_of(plan.otherwise) == var_of(plan.left) ||
       var_of(plan.otherwise) == var_of(plan.right))) return 0;

  if (plan.contexts.size() > (uint64_t(INT64_MAX) - 4) / 3) return 0;
  const uint64_t additions = 4 + 3 * uint64_t(plan.contexts.size());
  if (additions > uint64_t(INT64_MAX) || k.next_clause_id > uint64_t(INT64_MAX) - additions)
    return 0;
  uint64_t &meter = work_meter ? *work_meter : k.stats.factor_ticks;
  if (!charge_work(k, remaining, plan.publication_work(), meter)) return 0;
  uint64_t words = FxPlan::gate_work();
  std::unordered_set<Ref> wanted;
  for (const FxContext &context : plan.contexts) {
    if (context.remainder.empty() || context.remainder.size() > UINT32_MAX - 2 ||
        !std::is_sorted(context.remainder.begin(), context.remainder.end()) ||
        std::adjacent_find(context.remainder.begin(), context.remainder.end()) != context.remainder.end())
      return 0;
    Lit previous = INVALID_LIT;
    for (Lit l : context.remainder) {
      if (!available(l) || (previous != INVALID_LIT && previous == negate(l)) ||
          var_of(l) == var_of(plan.left) || var_of(l) == var_of(plan.right) ||
          (plan.gate == FxGate::Ite && var_of(l) == var_of(plan.otherwise))) return 0;
      previous = l;
    }
    for (Ref ref : {context.positive, context.negative})
      if (ref >= k.arena.size() || !wanted.insert(ref).second) return 0;
    const uint64_t size = context.remainder.size() + 1;
    if (size >= 3) words += uint64_t(CLAUSE_HEADER_WORDS) + size;
    if (words >= INVALID_REF || words >= INVALID_REF - k.arena.size()) return 0;
  }
  // The immutable plan retains discovery's authenticated record starts.
  // The owner/generation guard above rejects relocation before any dereference.
  for (const FxContext &context : plan.contexts) {
    const Clause *p = clause_at(k.arena, context.positive);
    const Clause *n = clause_at(k.arena, context.negative);
    if (p->garbage() || n->garbage() || p->detached_reason() || n->detached_reason() ||
        p->id() != context.positive_id || n->id() != context.negative_id) return 0;
    std::vector<Lit> positive = context.remainder, negative = context.remainder;
    positive.push_back(plan.left);
    negative.push_back(negate(plan.left));
    if (plan.gate == FxGate::Xor) {
      const Lit right = context.negate_output ? negate(plan.right) : plan.right;
      positive.push_back(right);
      negative.push_back(negate(right));
    } else {
      positive.push_back(context.negate_output ? negate(plan.otherwise) : plan.otherwise);
      negative.push_back(context.negate_output ? negate(plan.right) : plan.right);
    }
    auto matches = [](const Clause *c, const std::vector<Lit> &expected) {
      return c->size == expected.size() &&
             std::is_permutation(c->begin(), c->end(), expected.begin(), expected.end());
    };
    if (!((matches(p, positive) && matches(n, negative)) ||
          (matches(p, negative) && matches(n, positive)))) return 0;
  }

  const int32_t left = kernel_externalize(k, plan.left);
  const int32_t right = kernel_externalize(k, plan.right);
  const int32_t output = plan.gate == FxGate::Xor
      ? extended_resolution_define(k, {ExtensionGate::Xor, left, right})
      : extended_resolution_define_ite(k, left, right, kernel_externalize(k, plan.otherwise));
  if (!output) return 0;
  const Lit z = kernel_internalize(k, output);
  std::vector<Lit> quotient, helper;
  for (const FxContext &context : plan.contexts) {
    quotient.assign(1, context.negate_output ? negate(z) : z);
    quotient.insert(quotient.end(), context.remainder.begin(), context.remainder.end());
    const uint64_t positive_helper = ++k.next_clause_id;
    const uint64_t negative_helper = ++k.next_clause_id;
    const uint64_t id = ++k.next_clause_id;
    // A quotient is resolution-implied but not directly unit-implied. Split
    // on the selector, derive both resolvents, then resolve them by RUP.
    if (k.proof) {
      helper = quotient;
      helper.push_back(plan.left);
      k.proof->add_rup(positive_helper, helper.data(), helper.size());
      helper.back() = negate(plan.left);
      k.proof->add_rup(negative_helper, helper.data(), helper.size());
      k.proof->add_rup(id, quotient.data(), quotient.size());
    }
    if (quotient.size() == 2)
      kernel_add_binary_clause(k, quotient[0], quotient[1], id);
    else
      kernel_add_large_clause(k, quotient.data(), quotient.size(), false, 0, id);
    if (k.proof) {
      helper.back() = plan.left;
      k.proof->del(k.next_clause_id, positive_helper, helper.data(), helper.size());
      helper.back() = negate(plan.left);
      k.proof->del(k.next_clause_id, negative_helper, helper.data(), helper.size());
    }
  }
  for (const FxContext &context : plan.contexts) {
    kernel_delete_clause(k, clause_at(k.arena, context.positive));
    kernel_delete_clause(k, clause_at(k.arena, context.negative));
  }
  return output;
}
}
