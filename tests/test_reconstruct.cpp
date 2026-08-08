// Reconstruction stack (rho) and model extension: replay in reverse, force an
// unsatisfied segment's witness true, leave satisfied ones alone.
// Substitution-specific witness semantics live in tests/test_substitute.cpp.
#undef NDEBUG
#include "kernel/kernel.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// a witness whose clause is unsatisfied gets forced true at extend
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

// a witness whose clause is already satisfied is left untouched: completed to
// the definite default (false), never forced true
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
  assert(k.values[x1] < 0); // witness not forced; completed to false
}

static void test_reverse_replay_order() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit x1 = make_lit(0, false);
  const Lit x2 = make_lit(1, false);
  const Lit rest[1] = {x2};
  kernel_push_witness(k, x1, rest, 1);    // seg1 {x1,x2} witness x1 (pushed 1st)
  kernel_push_witness(k, x2, nullptr, 0); // seg2 {x2}    witness x2 (pushed 2nd)
  kernel_extend_model(k);
  // seg2 replays first, so seg1's clause is satisfied and x1 is not forced
  assert(k.values[x2] > 0);
  assert(k.values[x1] < 0); // completed to false, not left indefinite
}

// Extend must read a TOTAL assignment. A variable left value-0 (retired
// without ever being assigned) reads false in BOTH polarities during replay,
// which voids the tautological-resolvent exclusion: a pos- and a neg-side
// window of one pivot can both look unsatisfied, and the reverse walk
// double-flips the pivot. Shape of the EDP3-11000 invalid model
// (benchmarks/reports/2026-08-08-n2-frozen-pivot.md).
static void test_frozen_partner_no_double_flip() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit x = make_lit(0, false); // eliminated pivot, never assigned
  const Lit t = make_lit(1, false); // frozen partner: retired, no witness
  const Lit u = make_lit(4, false); // root-false remainder literal
  k.values[u] = -1;
  k.values[negate(u)] = 1;
  // BVE windows for x over C0={x,t,u}, D0={-x,-t,u} (u stripped as
  // root-false, taut resolvent t/-t dropped): pos side then neg side.
  const Lit rest_pos[1] = {t};
  kernel_push_witness(k, x, rest_pos, 1); // clause {x,t}, witness x
  const Lit rest_neg[1] = {negate(t)};
  kernel_push_witness(k, negate(x), rest_neg, 1); // clause {-x,-t}, witness -x
  kernel_extend_model(k);
  // every variable definite, and both original windows satisfied
  assert(k.values[x] != 0 && k.values[t] != 0);
  assert(k.values[x] > 0 || k.values[t] > 0);                 // C0
  assert(k.values[negate(x)] > 0 || k.values[negate(t)] > 0); // D0
}

// Substitute's equivalence pair for l ≡ s, with the representative s LATER
// eliminated by BVE and left frozen at 0: replay must not flip l against an
// indefinite s. Shape of the PRP_30_38 invalid model (clause 31966 -31965).
static void test_substitute_repr_later_eliminated() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit l = make_lit(0, false); // substituted, l ≡ s
  const Lit s = make_lit(1, false); // representative, then BVE-eliminated
  const Lit a = make_lit(2, false);
  k.values[a] = 1;
  k.values[negate(a)] = -1;
  // Substitute pair (pushed first)
  const Lit rest_a[1] = {s};
  kernel_push_witness(k, negate(l), rest_a, 1); // clause {s,-l}, witness -l
  const Lit rest_b[1] = {negate(s)};
  kernel_push_witness(k, l, rest_b, 1); // clause {-s,l}, witness l
  // BVE windows for s over {s,a} and {-s,a}; both rest-satisfied by a
  const Lit rest_s[1] = {a};
  kernel_push_witness(k, s, rest_s, 1);
  kernel_push_witness(k, negate(s), rest_s, 1);
  kernel_extend_model(k);
  assert(k.values[l] != 0 && k.values[s] != 0);
  // the equivalence l ≡ s must hold in the printed model
  assert((k.values[l] > 0) == (k.values[s] > 0));
}

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
  test_frozen_partner_no_double_flip();
  test_substitute_repr_later_eliminated();
  puts("test_reconstruct OK");
  return 0;
}
