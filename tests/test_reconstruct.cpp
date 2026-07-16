// K1+K2: reconstruction stack (rho) + model extension — the generic
// machinery. The substitution-specific witness semantics are exercised by
// test_substitute (V3); here we pin only the primitive: replay in reverse,
// force an unsatisfied segment's witness true, leave satisfied ones alone.
#undef NDEBUG
#include "kernel/kernel.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// a witness whose clause is UNSATISFIED gets forced true at extend
static void test_unsatisfied_witness_forced() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit x1 = make_lit(0, false);
  kernel_push_witness(k, x1, nullptr, 0); // segment {x1}, witness x1
  assert(k.values[x1] == 0);
  kernel_extend_model(k);
  assert(k.values[x1] > 0); // witness forced true
  assert(k.values[negate(x1)] < 0);
}

// a witness whose clause is already SATISFIED is left untouched
static void test_satisfied_witness_untouched() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit x1 = make_lit(0, false);
  const Lit x2 = make_lit(1, false);
  k.values[x2] = 1; // x2 true -> clause {x1, x2} satisfied
  k.values[negate(x2)] = -1;
  const Lit rest[1] = {x2};
  kernel_push_witness(k, x1, rest, 1); // witness x1, rest {x2}
  kernel_extend_model(k);
  assert(k.values[x1] == 0); // witness NOT forced
}

// reverse replay: a later-pushed segment can satisfy an earlier one's clause
static void test_reverse_replay_order() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit x1 = make_lit(0, false);
  const Lit x2 = make_lit(1, false);
  const Lit rest[1] = {x2};
  kernel_push_witness(k, x1, rest, 1);    // seg1 {x1,x2} witness x1 (pushed 1st)
  kernel_push_witness(k, x2, nullptr, 0); // seg2 {x2}    witness x2 (pushed 2nd)
  kernel_extend_model(k);
  // reverse order: seg2 first -> x2 true; then seg1's clause is satisfied,
  // so x1 is NOT forced
  assert(k.values[x2] > 0);
  assert(k.values[x1] == 0);
}

// idempotency: extend runs at most once
static void test_extend_idempotent() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  const Lit x1 = make_lit(0, false);
  kernel_push_witness(k, x1, nullptr, 0);
  kernel_extend_model(k);
  assert(k.values[x1] > 0);
  k.values[x1] = 0;       // scribble
  kernel_extend_model(k); // second call is a no-op (model_extended)
  assert(k.values[x1] == 0);
}

int main() {
  test_unsatisfied_witness_forced();
  test_satisfied_witness_untouched();
  test_reverse_replay_order();
  test_extend_idempotent();
  puts("test_reconstruct OK");
  return 0;
}
