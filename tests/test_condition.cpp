// X5 conditional-autarky legality, all-copy deletion and reconstruction.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/condition/condition.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

using namespace tactix;

static ConditionInstance cube(std::initializer_list<int> clause,
                              std::initializer_list<int> witness) {
  ConditionInstance result;
  for (const int lit : clause)
    result.clause.push_back(internalize(lit));
  for (const int lit : witness)
    result.witness.push_back(internalize(lit));
  return result;
}

static void test_binary_cube_and_model() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[2] = {internalize(1), internalize(2)};
  kernel_add_binary_clause(k, c[0], c[1]);
  kernel_add_binary_clause(k, c[0], c[1]);
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  const ConditionInstance insufficient = cube({1, 2}, {1});
  const size_t before = k.reconstruction.size();
  assert(!condition_apply_explicit(k, insufficient));
  assert(k.reconstruction.size() == before);
  assert(kernel_binary_copies(k, c[0], c[1]) == 2);

  const ConditionInstance valid = cube({2, 1}, {1, 3, 4});
  assert(condition_apply_explicit(k, valid));
  assert(!kernel_binary_live(k, c[0], c[1]));
  assert(k.reconstruction.size() > before);
  kernel_assign_unit(k, internalize(-1));
  kernel_assign_unit(k, internalize(-2));
  kernel_assign_unit(k, internalize(-3));
  kernel_assign_unit(k, internalize(-4));
  kernel_extend_model(k, false);
  assert(k.model_values[internalize(1)] > 0);
  assert(k.model_values[internalize(3)] > 0);
  assert(k.model_values[internalize(4)] > 0);
  assert(k.model_values[internalize(2)] < 0);
  assert(k.model_values[internalize(1)] > 0 ||
         k.model_values[internalize(2)] > 0);
  assert(k.model_values[internalize(-1)] > 0 ||
         k.model_values[internalize(3)] > 0);
  assert(k.model_values[internalize(-3)] > 0 ||
         k.model_values[internalize(4)] > 0);
}

static void test_large_and_negative_cases() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[3] = {internalize(1), internalize(2), internalize(4)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  kernel_add_binary_clause(k, internalize(-4), internalize(1));
  assert(!condition_apply_explicit(k, cube({1, 2, 4}, {1, 3})));
  assert(!condition_apply_explicit(k, cube({1, 2, 4}, {1, 3, -3})));
  assert(!condition_apply_explicit(k, cube({1, 2, 4}, {1, 3, 4, 4})));
  volatile sig_atomic_t stop = 1;
  k.stop = &stop;
  assert(!condition_apply_explicit(k, cube({1, 2, 4}, {1, 3, 4})));
  k.stop = nullptr;
  assert(condition_apply_explicit(k, cube({4, 1, 2}, {1, 3, 4})));
  assert(clause_at(k.arena, 0)->garbage());
}

static bool satisfied(int first, int second, unsigned assignment) {
  const auto true_lit = [assignment](int lit) {
    const bool value = (assignment >> (std::abs(lit) - 1)) & 1u;
    return lit > 0 ? value : !value;
  };
  return true_lit(first) || true_lit(second);
}

static void test_independent_local_implication_oracle() {
  for (int va = 1; va <= 3; ++va)
    for (int vb = va + 1; vb <= 3; ++vb)
      for (int sa : {-1, 1})
        for (int sb : {-1, 1})
          for (int cube_tail : {0, 3, -3}) {
            const int a = sa * va, b = sb * vb;
            Kernel k;
            kernel_reserve_vars(k, 3);
            kernel_add_binary_clause(k, internalize(1), internalize(2));
            kernel_add_binary_clause(k, internalize(a), internalize(b));
            ConditionInstance instance = cube({1, 2}, {1});
            if (cube_tail)
              instance.witness.push_back(internalize(cube_tail));
            bool expected = true;
            for (unsigned bits = 0; bits < 8; ++bits) {
              if (!satisfied(a, b, bits) || satisfied(1, 2, bits))
                continue;
              unsigned after = bits | 1u;
              if (cube_tail > 0)
                after |= 4u;
              else if (cube_tail < 0)
                after &= ~4u;
              expected &= satisfied(1, 2, after) &&
                          satisfied(a, b, after);
            }
            ConditionPrepared prepared;
            assert(condition_prepare(k, instance, prepared) == expected);
          }
}

static void test_root_false_candidate_literal() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(4));
  kernel_assign_unit(k, internalize(-3));
  assert(!kernel_propagate(k));
  assert(condition_guard(k));
  assert(condition_apply_explicit(k, cube({3, 2, 1}, {1, 4})));
  assert(clause_at(k.arena, 0)->garbage());
}

static void test_root_true_reason_candidates() {
  {
    Kernel k;
    kernel_reserve_vars(k, 2);
    kernel_add_binary_clause(k, internalize(1), internalize(2));
    kernel_assign_unit(k, internalize(-2));
    assert(!kernel_propagate(k));
    assert(k.values[internalize(1)] > 0);
    assert(k.assigned[0].binary);
    assert(condition_apply_explicit(k, cube({2, 1}, {1})));
    assert(!kernel_binary_live(k, internalize(1), internalize(2)));
    assert(k.assigned[0].reason == UNIT_REASON);
    assert(k.values[internalize(1)] > 0);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 3);
    const Lit c[] = {internalize(1), internalize(2), internalize(3)};
    const Ref ref = kernel_add_large_clause(k, c, 3, false, 0);
    kernel_assign_unit(k, internalize(-2));
    kernel_assign_unit(k, internalize(-3));
    assert(!kernel_propagate(k));
    assert(k.values[internalize(1)] > 0);
    assert(!k.assigned[0].binary && k.assigned[0].reason == ref);
    assert(condition_apply_explicit(k, cube({3, 1, 2}, {1})));
    assert(clause_at(k.arena, ref)->garbage());
    assert(k.assigned[0].reason == UNIT_REASON);
    assert(k.values[internalize(1)] > 0);
  }
}

static void test_cube_on_compacted_external_identity(bool emit_model = false) {
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(4));
  kernel_add_binary_clause(k, internalize(-4), internalize(5));
  k.var_status.assign(k.nvars, VarStatus::Active);
  k.var_status[2] = VarStatus::Eliminated;
  k.unassigned_active--;
  assert(kernel_compact_variables(k));
  assert(k.external_to_internal[2] == INVALID_VAR);
  assert(!condition_apply_explicit(k, cube({1, 2}, {1, 3, 4, 5, 5})));
  assert(!condition_apply_explicit(k, cube({1, 2}, {1, 3, 4, 6})));
  assert(condition_apply_explicit(k, cube({1, 2}, {1, 3, 4, 5})));
  assert(!kernel_binary_live(k, internalize(1), internalize(2)));
  kernel_assign_unit(k, internalize(-1));
  kernel_assign_unit(k, internalize(-2));
  kernel_assign_unit(k, kernel_internalize(k, -4));
  kernel_assign_unit(k, kernel_internalize(k, -5));
  kernel_extend_model(k, false);
  assert(k.model_values[internalize(1)] > 0);
  assert(k.model_values[internalize(3)] > 0);
  assert(k.model_values[internalize(4)] > 0);
  assert(k.model_values[internalize(5)] > 0);
  if (emit_model) {
    std::puts("s SATISFIABLE");
    std::fputs("v", stdout);
    for (int v = 1; v <= 5; ++v)
      std::printf(" %d", k.model_values[internalize(v)] > 0 ? v : -v);
    std::puts(" 0");
  }
}

static void test_cube_respects_archived_root_value() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(4));
  kernel_add_binary_clause(k, internalize(-4), internalize(5));
  kernel_assign_unit(k, internalize(3));
  kernel_assign_unit(k, internalize(6));
  assert(!kernel_propagate(k));
  assert(kernel_compact_variables(k, true));
  assert(k.external_to_internal[5] == INVALID_VAR);
  assert(k.fixed_external_values[5] > 0);
  assert(!condition_apply_explicit(k, cube({1, 2}, {1, 4, 5, -6})));
  assert(condition_apply_explicit(k, cube({1, 2}, {1, 4, 5, 6})));
}

int main(int argc, char **argv) {
  if (argc == 2 && !std::strcmp(argv[1], "--compacted-model")) {
    test_cube_on_compacted_external_identity(true);
    return 0;
  }
  assert(argc == 1);
  test_binary_cube_and_model();
  test_large_and_negative_cases();
  test_independent_local_implication_oracle();
  test_root_false_candidate_literal();
  test_root_true_reason_candidates();
  test_cube_on_compacted_external_identity();
  test_cube_respects_archived_root_value();
  std::puts("condition OK");
}
