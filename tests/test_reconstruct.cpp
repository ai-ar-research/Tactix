// Reconstruction stack and model extension: reverse replay forces an
// unsatisfied segment's witness. Substitute witnesses: tests/test_substitute.cpp.
#include "unit_test.hpp"
#include "kernel/kernel.hpp"

// a witness whose clause is unsatisfied gets forced true at extend
TEST(test_unsatisfied_witness_forced) {
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
TEST(test_satisfied_witness_untouched) {
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

TEST(test_reverse_replay_order) {
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

// A conditional-autarky segment can repair a clause with multiple assignments
// at once.  It must leave the cube alone if the clause is already true.
TEST(test_cube_witness_replay) {
  const Lit x = make_lit(0, false);
  const Lit y = make_lit(1, false);
  const Lit z = make_lit(2, false);
  const Lit clause[] = {x, y};
  const Lit cube[] = {x, z};
  {
    Kernel k;
    kernel_reserve_vars(k, 3);
    kernel_push_cube_witness(k, clause, 2, cube, 2);
    kernel_extend_model(k);
    assert(k.values[x] > 0 && k.values[z] > 0 && k.values[y] < 0);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 3);
    k.values[y] = 1;
    k.values[negate(y)] = -1;
    kernel_push_cube_witness(k, clause, 2, cube, 2);
    kernel_extend_model(k);
    assert(k.values[y] > 0 && k.values[x] < 0 && k.values[z] < 0);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 3);
    kernel_push_cube_witness(k, clause, 2, cube, 2);
    kernel_extend_model(k, false); // public model access must not alter state
    assert(k.model_values[x] > 0 && k.model_values[z] > 0);
    assert(k.values[x] == 0 && k.values[z] == 0);
  }
}

// A cube and an older single-literal segment share the same reverse stack:
// the cube can satisfy the older clause before its witness is considered.
TEST(test_cube_interleaves_with_single_witness) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit x = make_lit(0, false);
  const Lit y = make_lit(1, false);
  const Lit z = make_lit(2, false);
  const Lit rest[] = {x};
  kernel_push_witness(k, y, rest, 1); // older clause {x,y}
  const Lit clause[] = {x};
  const Lit cube[] = {x, z};
  kernel_push_cube_witness(k, clause, 1, cube, 2);
  kernel_extend_model(k);
  assert(k.values[x] > 0 && k.values[z] > 0 && k.values[y] < 0);
}

// Extend reads a total assignment: a never-assigned retired variable must not
// let both windows of one pivot look unsatisfied (EDP3-11000 shape).
TEST(test_frozen_partner_no_double_flip) {
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

// A Substitute pair whose representative BVE later froze at 0 must not flip l
// (PRP_30_38 shape).
TEST(test_substitute_repr_later_eliminated) {
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

TEST(test_extend_idempotent) {
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
