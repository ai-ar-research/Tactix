#undef NDEBUG
#include "rules/semantic_extension/affine_matrix.hpp"

#include <cassert>
#include <cstdio>
#include <initializer_list>

using namespace tactix::affine_semantic_extension;

static Equation equation(std::initializer_list<External> variables, bool rhs) {
  Equation result;
  result.variables = variables;
  result.rhs = rhs;
  return result;
}

static MatrixLimits limits() { return {1000000, 64, 64, 64, 64, 64}; }

static bool parity(const std::vector<External> &variables, uint64_t values) {
  bool result = false;
  for (External variable : variables)
    result ^= bool(values & (uint64_t{1} << (variable - 1)));
  return result;
}

static bool clause(const std::vector<External> &literals, uint64_t values) {
  for (External literal : literals) {
    const External variable = literal < 0 ? -literal : literal;
    const bool value = values & (uint64_t{1} << (variable - 1));
    if (value == (literal > 0)) return true;
  }
  return false;
}

static void check_reason(const std::vector<Equation> &input,
                         const MatrixEvent &event) {
  for (uint64_t values = 0; values < 32; ++values) {
    bool sources = true;
    for (size_t source : event.row.source_equations)
      sources &= parity(input[source].variables, values) == input[source].rhs;
    if (sources) assert(clause(event.reason, values));
  }
}

static void derived_implication_conflict_and_backtrack() {
  // Neither source row is unit after x1=x2=0. Their exact XOR is x5=0.
  const std::vector<Equation> input = {
      equation({1, 3, 4}, false), equation({2, 3, 4, 5}, false)};
  AffineMatrix matrix;
  assert(build_affine_matrix(input, 991, limits(),
                             [](uint64_t) { return true; }, matrix) ==
         MatrixStatus::Complete);
  assert(matrix.owner_identity() == 991 && matrix.trail().empty());

  MatrixEvent event;
  assert(matrix.synchronize({{1, false}, {2, false}},
                            [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Implication);
  assert(event.implied_variable == 5 && !event.implied_value);
  assert((event.row.source_equations == std::vector<size_t>{0, 1}));
  assert((event.reason == std::vector<External>{1, 2, -5}));
  check_reason(input, event);

  assert(matrix.synchronize({{1, false}, {2, false}, {5, true}},
                            [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Conflict);
  assert((event.row.source_equations == std::vector<size_t>{0, 1}));
  assert((event.reason == std::vector<External>{1, 2, -5}));
  check_reason(input, event);

  // A shorter replacement snapshot is a backtrack. The same matrix now
  // derives the opposite phase from x2=1, without rebuilding its source owner.
  assert(matrix.synchronize({{1, false}, {2, true}},
                            [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Implication);
  assert(event.implied_variable == 5 && event.implied_value);
  assert((event.reason == std::vector<External>{1, -2, 5}));
  check_reason(input, event);
}

static void reason_excludes_unrelated_assignments() {
  const std::vector<Equation> input = {equation({1, 2}, true)};
  AffineMatrix matrix;
  assert(build_affine_matrix(input, 7, limits(),
                             [](uint64_t) { return true; }, matrix) ==
         MatrixStatus::Complete);
  MatrixEvent event;
  assert(matrix.synchronize({{9, true}, {1, false}},
                            [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Implication);
  assert(event.implied_variable == 2 && event.implied_value);
  assert((event.reason == std::vector<External>{1, 2}));
  check_reason(input, event);
}

static void transactional_limits_and_validation() {
  const std::vector<Equation> input = {
      equation({1, 3, 4}, false), equation({2, 3, 4, 5}, false)};
  AffineMatrix sentinel;
  auto bounded = limits();
  bounded.maximum_work = 1;
  assert(build_affine_matrix(input, 1, bounded,
                             [](uint64_t) { return true; }, sentinel) ==
         MatrixStatus::Exhausted);
  assert(sentinel.owner_identity() == 0);
  assert(build_affine_matrix(input, 1, limits(),
                             [](uint64_t) { return false; }, sentinel) ==
         MatrixStatus::Interrupted);
  assert(sentinel.owner_identity() == 0);
  auto invalid = input;
  invalid[0].variables = {3, 1};
  assert(build_affine_matrix(invalid, 1, limits(),
                             [](uint64_t) { return true; }, sentinel) ==
         MatrixStatus::Invalid);
  assert(build_affine_matrix(input, 1, limits(),
                             [](uint64_t) { return true; }, sentinel) ==
         MatrixStatus::Complete);

  MatrixEvent event;
  event.implied_variable = 77;
  const auto rows = sentinel.rows();
  assert(sentinel.synchronize({{1, false}}, [](uint64_t) { return false; },
                              event) == MatrixStatus::Interrupted);
  assert(event.implied_variable == 77 && sentinel.trail().empty() &&
         sentinel.rows() == rows);
  assert(sentinel.synchronize({{1, false}, {1, false}},
                              [](uint64_t) { return true; }, event) ==
         MatrixStatus::Invalid);
  assert(sentinel.trail().empty());
}

static bool literal_value(External literal,
                          const std::vector<MatrixAssignment> &trail,
                          bool &assigned) {
  const External variable = literal < 0 ? -literal : literal;
  for (const MatrixAssignment item : trail)
    if (item.variable == variable) {
      assigned = true;
      return item.value == (literal > 0);
    }
  assigned = false;
  return false;
}

static void exhaustive_small_reason_oracle() {
  // Exercise every pair of nonempty four-variable equations and every partial
  // assignment. This checks the semantic artifact returned to the proof bridge,
  // independently of which pivot sequence produced it.
  for (uint32_t first_mask = 1; first_mask < 16; ++first_mask)
    for (uint32_t second_mask = 1; second_mask < 16; ++second_mask)
      for (uint32_t right = 0; right < 4; ++right) {
        std::vector<Equation> input(2);
        for (External variable = 1; variable <= 4; ++variable) {
          if (first_mask & (1u << (variable - 1)))
            input[0].variables.push_back(variable);
          if (second_mask & (1u << (variable - 1)))
            input[1].variables.push_back(variable);
        }
        input[0].rhs = right & 1;
        input[1].rhs = right & 2;
        AffineMatrix matrix;
        assert(build_affine_matrix(input, 1, limits(),
                                   [](uint64_t) { return true; }, matrix) ==
               MatrixStatus::Complete);
        for (uint32_t ternary = 0; ternary < 81; ++ternary) {
          uint32_t value = ternary;
          std::vector<MatrixAssignment> trail;
          for (External variable = 1; variable <= 4; ++variable) {
            const uint32_t digit = value % 3;
            value /= 3;
            if (digit) trail.push_back({variable, digit == 2});
          }
          MatrixEvent event;
          assert(matrix.synchronize(trail, [](uint64_t) { return true; },
                                    event) == MatrixStatus::Complete);
          if (event.kind == MatrixEventKind::None) continue;
          check_reason(input, event);
          const size_t antecedents =
              event.kind == MatrixEventKind::Implication
                  ? event.reason.size() - 1
                  : event.reason.size();
          for (size_t i = 0; i < antecedents; ++i) {
            bool assigned = false;
            assert(!literal_value(event.reason[i], trail, assigned) && assigned);
          }
          if (event.kind == MatrixEventKind::Implication) {
            bool assigned = false;
            (void)literal_value(event.reason.back(), trail, assigned);
            assert(!assigned);
          }
        }
      }
}

int main() {
  derived_implication_conflict_and_backtrack();
  reason_excludes_unrelated_assignments();
  transactional_limits_and_validation();
  exhaustive_small_reason_oracle();
  std::puts("affine matrix: exact sources, implication/conflict reasons, "
            "backtrack, and transactional bounds PASS");
}
