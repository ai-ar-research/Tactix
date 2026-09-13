#undef NDEBUG
#include "rules/lucky/onehot_model.hpp"

#include <array>
#include <cassert>

using namespace tactix;

static Var indicator(unsigned domain, unsigned value) {
  return static_cast<Var>(3 * domain + value);
}

static void add_domain(Kernel &k, unsigned domain) {
  Lit some[3];
  for (unsigned value = 0; value != 3; ++value)
    some[value] = make_lit(indicator(domain, value), false);
  kernel_add_large_clause(k, some, 3, false, 0);
  for (unsigned a = 0; a != 3; ++a)
    for (unsigned b = a + 1; b != 3; ++b)
      kernel_add_binary_clause(k, make_lit(indicator(domain, a), true),
                              make_lit(indicator(domain, b), true));
}

static void add_equation(Kernel &k, std::array<unsigned, 3> coefficients,
                         unsigned rhs, int omit = -1) {
  int ordinal = 0;
  for (unsigned a = 0; a != 3; ++a)
    for (unsigned b = 0; b != 3; ++b)
      for (unsigned c = 0; c != 3; ++c) {
        if ((coefficients[0] * a + coefficients[1] * b +
             coefficients[2] * c) % 3 == rhs)
          continue;
        if (ordinal++ == omit)
          continue;
        const Lit clause[3] = {make_lit(indicator(0, a), true),
                               make_lit(indicator(1, b), true),
                               make_lit(indicator(2, c), true)};
        kernel_add_large_clause(k, clause, 3, false, 0);
      }
}

static Kernel fixture(bool complete = true) {
  Kernel k;
  kernel_reserve_vars(k, 9);
  for (unsigned domain = 0; domain != 3; ++domain)
    add_domain(k, domain);
  add_equation(k, {1, 2, 1}, 2, complete ? -1 : 0);
  return k;
}

static bool satisfied(const Kernel &k) {
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage()) {
      bool sat = false;
      for (const Lit lit : *clause)
        sat |= k.value(lit) > 0;
      if (!sat)
        return false;
    }
    ref += clause_actual_words(clause);
  }
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    for (const WatchEntry watch : k.watches.entries(lit))
      if (watch.binary() && lit <= watch.other() && k.value(lit) <= 0 &&
          k.value(watch.other()) <= 0)
        return false;
  return true;
}

int main() {
  {
    Kernel k = fixture();
    assert(onehot_model_guard(k));
    assert(onehot_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.onehot_model_calls == 1);
    assert(k.stats.onehot_model_domains == 3);
    assert(k.stats.onehot_model_equations == 1);
    assert(k.stats.onehot_model_rank == 1);
    assert(k.stats.onehot_model_candidates == 1);
    assert(k.stats.onehot_model_sat == 1);
    assert(k.unassigned_active == 0 && satisfied(k));
  }
  {
    Kernel k = fixture();
    // The first affine candidate has domain 1 at value zero. Force the next
    // free assignment and prove that candidate enumeration is real.
    kernel_add_binary_clause(k, make_lit(indicator(1, 1), false),
                            make_lit(indicator(2, 1), false));
    assert(onehot_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.onehot_model_candidates > 1 && satisfied(k));
  }
  {
    Kernel k = fixture();
    // The all-zero free assignment chooses value zero in domain 1. A root
    // unit forbidding precisely that indicator must reject the candidate;
    // testing it as true and then skipping it during materialization used to
    // return a model with this domain entirely false.
    kernel_assign_unit(k, make_lit(indicator(1, 0), true));
    assert(!kernel_propagate(k));
    assert(onehot_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.onehot_model_candidates > 1 && satisfied(k));
    assert(k.value(make_lit(indicator(1, 0), false)) < 0);
  }
  {
    Kernel k = fixture(false);
    assert(onehot_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.onehot_model_equations == 0 && k.trail.empty());
  }
  {
    Kernel k = fixture();
    strategy::OnehotModelParameters parameters;
    parameters.maximum_constraints = 4;
    strategy::ScopedOnehotModelParameters posed(k, parameters);
    assert(onehot_model_apply(k) == ApplyResult::NoOp && k.trail.empty());
  }
  {
    Kernel k = fixture();
    strategy::OnehotModelParameters parameters;
    parameters.candidate_budget = 0;
    strategy::ScopedOnehotModelParameters posed(k, parameters);
    assert(onehot_model_apply(k) == ApplyResult::NoOp && k.trail.empty());
  }
  {
    Kernel k = fixture();
    volatile sig_atomic_t stop = 1;
    k.stop = &stop;
    assert(onehot_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.onehot_model_interrupted == 1 && k.trail.empty());
  }
  {
    strategy::OnehotModelParameters invalid;
    invalid.maximum_domains = 0;
    assert(!invalid.valid());
    invalid.maximum_domains = 3;
    invalid.minimum_domains = 4;
    assert(!invalid.valid());
    invalid.minimum_domains = 1;
    invalid.maximum_equation_arity = 0;
    assert(!invalid.valid());
    invalid.maximum_equation_arity = 3;
    invalid.maximum_constraints = 0;
    assert(!invalid.valid());
  }
}
