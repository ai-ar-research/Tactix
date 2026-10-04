#undef NDEBUG
#include "rules/lucky/graph_labeling_completion.hpp"
#include "policies/heuristics.hpp"

#include <cassert>

using namespace tactix;
using namespace tactix::graph_labeling;

static void chain(Kernel &k) {
  kernel_reserve_vars(k, 6);
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  kernel_add_binary_clause(k, internalize(-4), internalize(5));
  const Lit large[] = {internalize(2), internalize(-5), internalize(6)};
  kernel_add_large_clause(k, large, 3, false, 0);
}

int main() {
  Kernel k;
  ScopedHeuristics h(k);
  chain(k);
  const std::vector<Lit> trail(k.trail.begin(), k.trail.end());
  const auto saved = k.h->saved, best = k.h->best, target = k.h->target;
  const auto arena = k.arena;
  std::vector<uint8_t> model;
  const std::vector<int8_t> partial{1, -1, 0, 0, 0, 0};
  uint64_t total_work = 0;
  const auto count = [&](uint64_t amount) { total_work += amount; return true; };
  assert(complete_model(k, partial, 4096, count, model) == CompletionStatus::Model);
  assert((model == std::vector<uint8_t>{1, 0, 1, 1, 1, 1}));
  assert(k.trail == trail && k.arena == arena);
  assert(k.h->saved == saved && k.h->best == best && k.h->target == target);
  for (uint64_t limit = 0; limit < total_work; ++limit) {
    uint64_t used = 0;
    const auto bounded = [&](uint64_t amount) {
      if (amount > limit - used) return false;
      used += amount;
      return true;
    };
    assert(complete_model(k, partial, 4096, bounded, model) == CompletionStatus::Limited);
    assert(model.empty() && used <= limit && k.trail == trail);
  }
  const auto always = [](uint64_t) { return true; };
  assert(complete_model(k, partial, 1, always, model) == CompletionStatus::Limited);
  assert(model.empty());
  // A graph-level candidate contradicting any additional live constraint is
  // rejected without a SAT publication, including retained redundant clauses.
  const Lit contradiction[] = {internalize(-3), internalize(-4), internalize(-5)};
  kernel_add_large_clause(k, contradiction, 3, true, 0);
  assert(complete_model(k, partial, 4096, always, model) == CompletionStatus::NoModel);
  assert(model.empty() && k.trail == trail);
  {
    Kernel root;
    kernel_reserve_vars(root, 2);
    kernel_assign_unit(root, internalize(-1));
    assert(complete_model(root, {1, 0}, 4096, always, model) == CompletionStatus::NoModel);
    assert(model.empty() && root.trail.size() == 1);
    assert(complete_model(root, {0, 0}, 4096, always, model) == CompletionStatus::Model);
    assert((model == std::vector<uint8_t>{0, 0}));
  }
  {
    Kernel retired;
    kernel_reserve_vars(retired, 2);
    kernel_add_binary_clause(retired, internalize(1), internalize(2));
    retired.var_status.assign(2, VarStatus::Active);
    retired.var_status[0] = VarStatus::Eliminated;
    assert(complete_model(retired, {1, -1}, 4096, always, model) == CompletionStatus::NoModel);
    assert(model.empty() && retired.trail.empty());
  }
}
