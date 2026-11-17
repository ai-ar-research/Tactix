#include "rules/factor/fx.hpp"
#include "rules/extended_resolution/extended_resolution.hpp"
#include <algorithm>
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
struct Bucket { std::vector<Parent> positive, negative; };
using Key = std::array<Lit, 4>;
bool charge_work(Kernel &k, uint64_t &remaining, uint64_t work) {
  if (kernel_terminating(k) || work > remaining ||
      work > UINT64_MAX - k.stats.factor_ticks) return false;
  remaining -= work;
  k.stats.factor_ticks += work;
  return true;
}
}

std::optional<FxPlan> factor_find_fx(Kernel &k, Lit selector,
                                     std::span<const Ref> sources,
                                     const FxOptions &options, uint64_t &remaining,
                                     Lit partner_limit) {
  if (!options.valid() || selector >= 2 * k.nvars || selector >= partner_limit || k.level ||
      kernel_var_retired(k, var_of(selector))) return {};
  selector = make_lit(var_of(selector), false);
  auto charge = [&](uint64_t work) {
    return charge_work(k, remaining, work);
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
      if (other == head || other >= partner_limit) continue;
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
      auto it = plans.try_emplace(key, FxPlan{gate, left, right, otherwise, {},
                                             &k, k.arena_generation}).first;
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

int32_t factor_publish_fx(Kernel &k, const FxPlan &plan, uint64_t &remaining) {
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
  uint64_t words = 4 * uint64_t(clause_words(3));
  uint64_t publication_work = words;
  std::unordered_set<Ref> wanted;
  for (const FxContext &context : plan.contexts) {
    if (!charge_work(k, remaining, 1 + context.remainder.size())) return 0;
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
    publication_work += size + 6 * (size + 1);
    if (words >= INVALID_REF || words >= INVALID_REF - k.arena.size()) return 0;
  }
  // Authenticate actual record starts before dereferencing caller-owned Refs.
  size_t found = 0;
  for (auto [ref, clause] : arena_records(k.arena)) {
    if (!charge_work(k, remaining, 1)) return 0;
    found += wanted.count(ref);
  }
  if (found != wanted.size()) return 0;
  for (const FxContext &context : plan.contexts) {
    const Clause *p = clause_at(k.arena, context.positive);
    const Clause *n = clause_at(k.arena, context.negative);
    if (p->garbage() || n->garbage() || p->detached_reason() || n->detached_reason() ||
        p->id() != context.positive_id || n->id() != context.negative_id) return 0;
    if (!charge_work(k, remaining, 4 * (context.remainder.size() + 2))) return 0;
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
  if (!charge_work(k, remaining, publication_work)) return 0;

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
