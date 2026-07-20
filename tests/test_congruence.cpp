// V6 Congruence — gate congruence closure. Two gates of the same kind over the
// same inputs have equivalent outputs; congruence_apply adds the equivalence
// binaries (¬a∨b),(a∨¬b). These tests build one gate pair per kind (AND/XOR/ITE)
// and assert the closure fires; plus a no-op when inputs differ.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// definitive live-binary test (watch scan)
static bool has_binary(const Kernel &k, Lit a, Lit b) {
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[a].offset;
  for (uint32_t i = 0; i < k.watches.heads[a].size;) {
    const Watch w{ws[i]};
    if (w.binary()) {
      if (w.other() == b)
        return true;
      i += 1;
    } else {
      i += 2;
    }
  }
  return false;
}

// the two equivalence binaries between vars ea,eb (external DIMACS) are present
static bool equivalence_added(const Kernel &k, int ea, int eb) {
  return has_binary(k, internalize(-ea), internalize(eb)) &&
         has_binary(k, internalize(ea), internalize(-eb));
}

static void add3(Kernel &k, int a, int b, int c) {
  Lit lits[3] = {internalize(a), internalize(b), internalize(c)};
  kernel_add_large_clause(k, lits, 3, /*redundant=*/false, /*glue=*/0);
}

// out = AND(x1,x2): binaries (¬out∨x1),(¬out∨x2) + base (out∨¬x1∨¬x2)
static void and_gate(Kernel &k, int out, int x1, int x2) {
  kernel_add_binary_clause(k, internalize(-out), internalize(x1));
  kernel_add_binary_clause(k, internalize(-out), internalize(x2));
  add3(k, out, -x1, -x2);
}

// out = XOR(x1,x2): the 4 odd-parity clauses over {out,x1,x2}
static void xor_gate(Kernel &k, int out, int x1, int x2) {
  add3(k, -out, x1, x2);
  add3(k, out, -x1, x2);
  add3(k, out, x1, -x2);
  add3(k, -out, -x1, -x2);
}

// out = c ? t : e : the 4 defining ITE ternaries
static void ite_gate(Kernel &k, int out, int c, int t, int e) {
  add3(k, c, -e, out);
  add3(k, c, e, -out);
  add3(k, -c, -t, out);
  add3(k, -c, t, -out);
}

// Two AND gates over the same inputs → outputs merged.
static void test_and_congruent() {
  Kernel k;
  kernel_reserve_vars(k, 4); // x1=1 x2=2 a=3 b=4
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(equivalence_added(k, 3, 4)); // a ≡ b
}

// Two XOR gates over the same inputs → outputs merged (exercises the ladder).
static void test_xor_congruent() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  xor_gate(k, 3, 1, 2);
  xor_gate(k, 4, 1, 2);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(equivalence_added(k, 3, 4));
}

// Two ITE gates over the same (cond,then,else) → outputs merged.
static void test_ite_congruent() {
  Kernel k;
  kernel_reserve_vars(k, 5); // c=1 t=2 e=3 a=4 b=5
  ite_gate(k, 4, 1, 2, 3);
  ite_gate(k, 5, 1, 2, 3);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(equivalence_added(k, 4, 5));
}

// Different inputs → not congruent → NoOp, no equivalence added.
static void test_no_congruence() {
  Kernel k;
  kernel_reserve_vars(k, 6); // a=AND(1,2)=5, b=AND(3,4)=6
  and_gate(k, 5, 1, 2);
  and_gate(k, 6, 3, 4);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::NoOp);
  assert(!equivalence_added(k, 5, 6));
}

// Mixed kinds over the same inputs must NOT collide: AND(1,2)=4 and XOR(1,2)=5
// compute different functions, so their outputs must stay independent.
static void test_kind_isolation() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  and_gate(k, 4, 1, 2);
  xor_gate(k, 5, 1, 2);
  assert(congruence_guard(k));
  const ApplyResult r = congruence_apply(k);
  assert(r == ApplyResult::NoOp); // no same-kind pair
  assert(!equivalence_added(k, 4, 5));
}

int main() {
  test_and_congruent();
  test_xor_congruent();
  test_ite_congruent();
  test_no_congruence();
  test_kind_isolation();
  std::printf("test_congruence: all passed\n");
  return 0;
}
