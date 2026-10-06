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
  assert(matrix.owner_identity() == 991 && matrix.trail().empty() &&
         (matrix.admitted_variables() ==
          std::vector<External>{1, 2, 3, 4, 5}));

  MatrixEvent event;
  assert(matrix.synchronize({{1, false}, {2, false}},
                            [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Implication);
  assert(event.implied_variable == 5 && !event.implied_value);
  assert((event.row.source_equations == std::vector<size_t>{0, 1}));
  assert((event.reason == std::vector<External>{1, 2, -5}));
  check_reason(input, event);

  uint64_t unchanged_work = 0;
  assert(matrix.synchronize({{1, false}, {2, false}},
                            [&](uint64_t amount) {
                              unchanged_work += amount;
                              return true;
                            }, event) == MatrixStatus::Complete);
  assert(!unchanged_work && matrix.last_synchronization_incremental() &&
         matrix.last_synchronization_prefix() == 2 &&
         matrix.last_synchronization_conditioned() == 0 &&
         event.kind == MatrixEventKind::Implication &&
         event.implied_variable == 5 && !event.implied_value);

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

static void bounded_partial_admission_is_deterministic() {
  const std::vector<Equation> width_input = {
      equation({1, 2, 3}, false), equation({1, 4, 5}, true),
      equation({6, 7}, false)};
  auto width = limits();
  width.maximum_row_width = 3;
  AffineMatrix first;
  assert(build_affine_matrix(width_input, 41, width,
                             [](uint64_t) { return true; }, first) ==
         MatrixStatus::Partial);
  assert(!first.complete_admission());
  assert((first.admitted_sources() == std::vector<size_t>{0, 2}));
  assert(first.admitted_rows().size() == 2);

  AffineMatrix repeated;
  assert(build_affine_matrix(width_input, 41, width,
                             [](uint64_t) { return true; }, repeated) ==
         MatrixStatus::Partial);
  assert(first.admitted_rows() == repeated.admitted_rows());
  assert(first.admitted_sources() == repeated.admitted_sources());
  assert(first.work() == repeated.work());

  const std::vector<Equation> source_input = {
      equation({1, 2}, false), equation({1, 3}, true),
      equation({4, 5}, false)};
  auto sources = limits();
  sources.maximum_sources = 1;
  AffineMatrix source_limited;
  assert(build_affine_matrix(source_input, 42, sources,
                             [](uint64_t) { return true; }, source_limited) ==
         MatrixStatus::Partial);
  assert((source_limited.admitted_sources() ==
          std::vector<size_t>{0, 2}));

  auto work = limits();
  work.maximum_work = 3;
  AffineMatrix work_limited;
  assert(build_affine_matrix(source_input, 43, work,
                             [](uint64_t) { return true; }, work_limited) ==
         MatrixStatus::Partial);
  assert((work_limited.admitted_sources() == std::vector<size_t>{0}));

  auto none = limits();
  none.maximum_row_width = 1;
  AffineMatrix sentinel;
  assert(build_affine_matrix(source_input, 44, none,
                             [](uint64_t) { return true; }, sentinel) ==
         MatrixStatus::Exhausted);
  assert(!sentinel.owner_identity() && sentinel.rows().empty());
}

static void partial_implication_conflict_and_backtrack() {
  // The final source exceeds the width bound.  The first two authenticated
  // rows still form a useful admitted submatrix.
  const std::vector<Equation> input = {
      equation({1, 3, 4}, false), equation({2, 3, 4, 5}, false),
      equation({6, 7, 8, 9, 10}, true)};
  auto bounded = limits();
  bounded.maximum_row_width = 4;
  AffineMatrix matrix;
  assert(build_affine_matrix(input, 45, bounded,
                             [](uint64_t) { return true; }, matrix) ==
         MatrixStatus::Partial);
  assert((matrix.admitted_sources() == std::vector<size_t>{0, 1}));

  MatrixEvent event;
  assert(matrix.synchronize({{1, false}, {2, false}},
                            [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Implication &&
         event.implied_variable == 5 && !event.implied_value &&
         (event.row.source_equations == std::vector<size_t>{0, 1}));
  check_reason(input, event);

  assert(matrix.synchronize({{1, false}, {2, false}, {5, true}},
                            [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Conflict &&
         (event.row.source_equations == std::vector<size_t>{0, 1}));
  check_reason(input, event);

  assert(matrix.synchronize({{1, false}, {2, true}},
                            [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Implication &&
         event.implied_variable == 5 && event.implied_value &&
         (event.row.source_equations == std::vector<size_t>{0, 1}));
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

static uint64_t synchronize_work(AffineMatrix &matrix,
                                 const std::vector<MatrixAssignment> &trail,
                                 MatrixStatus &status, MatrixEvent &event) {
  uint64_t work = 0;
  status = matrix.synchronize(
      trail,
      [&](uint64_t amount) {
        work += amount;
        return true;
      },
      event);
  return work;
}

static bool rows_satisfied(const std::vector<MatrixRow> &rows,
                           uint64_t values) {
  for (const MatrixRow &row : rows)
    if (parity(row.variables, values) != row.rhs) return false;
  return true;
}

static void incremental_extension_matches_snapshot_reducer() {
  // Compare every incremental prefix against a fresh full-snapshot reduction.
  // This covers basis collisions after assigned pivots disappear, not merely
  // rows that can be conditioned independently.
  for (uint32_t first_mask = 1; first_mask < 16; ++first_mask)
    for (uint32_t second_mask = 1; second_mask < 16; ++second_mask)
      for (uint32_t right = 0; right < 4; ++right)
        for (uint32_t values = 0; values < 16; ++values) {
          std::vector<Equation> input(2);
          for (External variable = 1; variable <= 4; ++variable) {
            if (first_mask & (1u << (variable - 1)))
              input[0].variables.push_back(variable);
            if (second_mask & (1u << (variable - 1)))
              input[1].variables.push_back(variable);
          }
          input[0].rhs = right & 1;
          input[1].rhs = right & 2;
          AffineMatrix pristine;
          assert(build_affine_matrix(input, 73, limits(),
                                     [](uint64_t) { return true; }, pristine) ==
                 MatrixStatus::Complete);
          AffineMatrix incremental = pristine;
          std::vector<MatrixAssignment> trail;
          for (External variable = 1; variable <= 4; ++variable) {
            trail.push_back(
                {variable, bool(values & (1u << (variable - 1)))});
            MatrixStatus incremental_status, snapshot_status;
            MatrixEvent incremental_event, snapshot_event;
            (void)synchronize_work(incremental, trail, incremental_status,
                                   incremental_event);
            AffineMatrix snapshot = pristine;
            (void)synchronize_work(snapshot, trail, snapshot_status,
                                   snapshot_event);
            assert(incremental_status == snapshot_status &&
                   incremental_event.kind == snapshot_event.kind);
            for (uint64_t completion = 0; completion < 16; ++completion)
              assert(rows_satisfied(incremental.rows(), completion) ==
                     rows_satisfied(snapshot.rows(), completion));
            if (incremental_event.kind != MatrixEventKind::None) {
              check_reason(input, incremental_event);
              check_reason(input, snapshot_event);
            }
            assert(incremental.last_synchronization_incremental() &&
                   incremental.last_synchronization_prefix() ==
                       size_t(variable - 1));
          }
        }
}

static void incremental_multiple_events_backtrack_and_interruption() {
  const std::vector<Equation> input = {
      equation({1, 3}, false), equation({2, 4}, false)};
  AffineMatrix matrix;
  assert(build_affine_matrix(input, 74, limits(),
                             [](uint64_t) { return true; }, matrix) ==
         MatrixStatus::Complete);

  MatrixEvent event;
  assert(matrix.synchronize({{1, false}, {2, false}},
                            [](uint64_t) { return true; }, event) ==
             MatrixStatus::Complete &&
         event.kind == MatrixEventKind::Implication &&
         event.implied_variable == 3 && !event.implied_value);
  assert(matrix.last_synchronization_incremental() &&
         matrix.last_synchronization_prefix() == 0);

  assert(matrix.synchronize({{1, false}, {2, false}, {3, false}},
                            [](uint64_t) { return true; }, event) ==
             MatrixStatus::Complete &&
         event.kind == MatrixEventKind::Implication &&
         event.implied_variable == 4 && !event.implied_value);
  assert(matrix.last_synchronization_incremental() &&
         matrix.last_synchronization_prefix() == 2);

  const auto retained_trail = matrix.trail();
  const auto retained_rows = matrix.rows();
  const uint64_t retained_work = matrix.work();
  MatrixEvent sentinel;
  sentinel.implied_variable = 99;
  assert(matrix.synchronize(
             {{1, false}, {2, false}, {3, false}, {4, true}},
             [](uint64_t) { return false; }, sentinel) ==
         MatrixStatus::Interrupted);
  assert(matrix.trail() == retained_trail && matrix.rows() == retained_rows &&
         matrix.work() == retained_work && sentinel.implied_variable == 99 &&
         matrix.last_synchronization_kind() ==
             MatrixSynchronizationKind::Extension);

  assert(matrix.synchronize(
             {{1, false}, {2, false}, {3, false}, {4, true}},
             [](uint64_t) { return true; }, event) == MatrixStatus::Complete &&
         event.kind == MatrixEventKind::Conflict);
  assert(matrix.last_synchronization_incremental() &&
         matrix.last_synchronization_prefix() == 3);

  // A shorter, differently valued snapshot is a real backtrack/reassignment.
  assert(matrix.synchronize({{1, true}}, [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(!matrix.last_synchronization_incremental() &&
         matrix.last_synchronization_prefix() == 0 &&
         matrix.last_synchronization_kind() ==
             MatrixSynchronizationKind::Backtrack &&
         event.kind == MatrixEventKind::Implication &&
         event.implied_variable == 3 && event.implied_value);

  // Same-length changed assignments are classified independently from a
  // shorter trail, even though both safely rebuild the immutable basis.
  assert(matrix.synchronize({{1, false}}, [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(!matrix.last_synchronization_incremental() &&
         matrix.last_synchronization_kind() ==
             MatrixSynchronizationKind::Reassignment);
}

static void partial_synchronization_forces_next_rebuild() {
  const std::vector<Equation> input = {
      equation({1}, false), equation({3}, false), equation({2, 3}, false)};
  auto bounded = limits();
  bounded.maximum_row_width = 2;
  bounded.maximum_sources = 1;
  AffineMatrix matrix;
  assert(build_affine_matrix(input, 76, bounded,
                             [](uint64_t) { return true; }, matrix) ==
         MatrixStatus::Complete);
  MatrixEvent event;
  assert(matrix.synchronize({{2, false}}, [](uint64_t) { return true; }, event) ==
         MatrixStatus::Partial);
  assert(matrix.last_synchronization_kind() ==
             MatrixSynchronizationKind::Extension &&
         matrix.last_synchronization_conditioned() == 1);
  assert(matrix.synchronize({{2, false}}, [](uint64_t) { return true; }, event) ==
         MatrixStatus::Partial);
  assert(!matrix.last_synchronization_incremental() &&
         matrix.last_synchronization_kind() ==
             MatrixSynchronizationKind::PriorPartial &&
         matrix.last_synchronization_conditioned() == 1);
}

static void retained_rows_reduce_extension_work() {
  std::vector<Equation> input;
  for (External row = 0; row < 16; ++row) {
    Equation item;
    for (External column = 0; column < 8; ++column)
      item.variables.push_back(1 + row * 8 + column);
    item.rhs = row & 1;
    input.push_back(std::move(item));
  }
  auto wide = limits();
  wide.maximum_rows = 256;
  wide.maximum_variables = 256;
  wide.maximum_row_width = 256;
  wide.maximum_sources = 256;
  wide.maximum_assignments = 256;
  AffineMatrix pristine;
  assert(build_affine_matrix(input, 75, wide,
                             [](uint64_t) { return true; }, pristine) ==
         MatrixStatus::Complete);
  AffineMatrix incremental = pristine;
  std::vector<MatrixAssignment> trail;
  uint64_t incremental_total = 0, snapshot_total = 0;
  for (External column = 0; column < 6; ++column) {
    for (External row = 0; row < 16; ++row)
      trail.push_back({1 + row * 8 + column, bool((row + column) & 1)});
    MatrixStatus status;
    MatrixEvent event;
    incremental_total +=
        synchronize_work(incremental, trail, status, event);
    assert(status == MatrixStatus::Complete);
    AffineMatrix snapshot = pristine;
    snapshot_total += synchronize_work(snapshot, trail, status, event);
    assert(status == MatrixStatus::Complete);
  }
  assert(incremental_total < snapshot_total &&
         incremental.last_synchronization_prefix() == 80);
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

static void bounded_predecessor_restore() {
  const std::vector<Equation> input = {
      equation({1, 3, 4}, false), equation({2, 3, 4, 5}, false)};
  for (bool row_budget : {false, true})
    for (bool trail_budget : {false, true}) {
      auto bounded = limits();
      if (row_budget) bounded.maximum_rows = 2;
      if (trail_budget) bounded.maximum_assignments = 2;
      AffineMatrix matrix;
      assert(build_affine_matrix(input, 93, bounded,
                                 [](uint64_t) { return true; }, matrix) ==
             MatrixStatus::Complete);
      MatrixEvent event;
      const std::vector<MatrixAssignment> prefix = {{1, false}};
      assert(matrix.synchronize(prefix, [](uint64_t) { return true; }, event) ==
             MatrixStatus::Complete);
      const auto prefix_rows = matrix.rows();
      assert(matrix.synchronize({{1, false}, {2, false}},
                                [](uint64_t) { return true; }, event) ==
             MatrixStatus::Complete);
      assert(event.kind == MatrixEventKind::Implication);
      const auto retained_rows = matrix.rows();
      const auto retained_trail = matrix.trail();
      const auto retained_work = matrix.work();
      MatrixEvent sentinel;
      sentinel.implied_variable = 99;
      assert(matrix.synchronize(prefix, [](uint64_t) { return false; },
                                sentinel) == MatrixStatus::Interrupted);
      assert(matrix.rows() == retained_rows && matrix.trail() == retained_trail &&
             matrix.work() == retained_work && sentinel.implied_variable == 99);
      MatrixStatus status;
      const uint64_t work = synchronize_work(matrix, prefix, status, event);
      assert(status == MatrixStatus::Complete && matrix.rows() == prefix_rows &&
             matrix.trail() == prefix && event.kind == MatrixEventKind::None &&
             matrix.last_synchronization_kind() ==
                 MatrixSynchronizationKind::Backtrack);
      if (!row_budget && !trail_budget) {
        assert(matrix.last_synchronization_restored_predecessor());
        assert(work == 1 + prefix.size());
        assert(matrix.last_synchronization_conditioned() == 0);
      } else {
        assert(!matrix.last_synchronization_restored_predecessor());
        assert(work > 1 + prefix.size());
        assert(matrix.last_synchronization_conditioned() == prefix.size());
      }
      // Restoration consumes the one checkpoint. A deeper backtrack rebuilds
      // from the authenticated admission rather than retaining a trail stack.
      assert(matrix.synchronize({}, [](uint64_t) { return true; }, event) ==
             MatrixStatus::Complete);
      assert(matrix.rows() == matrix.admitted_rows());
      assert(!matrix.last_synchronization_restored_predecessor());
    }

  AffineMatrix matrix;
  assert(build_affine_matrix(input, 94, limits(),
                             [](uint64_t) { return true; }, matrix) ==
         MatrixStatus::Complete);
  MatrixEvent event;
  const std::vector<MatrixAssignment> prefix = {{1, false}, {2, false}};
  assert(matrix.synchronize(prefix, [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  const auto expected = event;
  assert(matrix.synchronize({{1, false}, {2, false}, {5, true}},
                            [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete && event.kind == MatrixEventKind::Conflict);
  MatrixStatus status;
  assert(synchronize_work(matrix, prefix, status, event) == 3 &&
         status == MatrixStatus::Complete);
  assert(matrix.last_synchronization_restored_predecessor());
  assert(event.row == expected.row && event.reason == expected.reason &&
         event.kind == expected.kind);
  assert(matrix.authenticates(94, input, event,
                              [](uint64_t) { return true; }));
  check_reason(input, event);
  AffineMatrix rebuilt;
  assert(build_affine_matrix(input, 94, limits(),
                             [](uint64_t) { return true; }, rebuilt) ==
         MatrixStatus::Complete);
  MatrixEvent rebuilt_event;
  assert(synchronize_work(rebuilt, prefix, status, rebuilt_event) == 17 &&
         status == MatrixStatus::Complete && rebuilt.rows() == matrix.rows() &&
         rebuilt_event.row == event.row && rebuilt_event.reason == event.reason);
  event.row.source_equations = {0};
  assert(!matrix.authenticates(94, input, event,
                               [](uint64_t) { return true; }));
  // A failed extension preserves diagnostics as well as rows/trail/event.
  const auto restored_rows = matrix.rows();
  MatrixEvent sentinel;
  sentinel.implied_variable = 77;
  assert(matrix.synchronize({{1, false}, {2, false}, {5, true}},
                            [](uint64_t) { return false; }, sentinel) ==
         MatrixStatus::Interrupted);
  assert(matrix.last_synchronization_restored_predecessor() &&
         matrix.rows() == restored_rows && matrix.trail() == prefix &&
         sentinel.implied_variable == 77);
  assert(matrix.synchronize(prefix, [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(!matrix.last_synchronization_restored_predecessor());
}

static void nonmatching_predecessor_failure_is_atomic() {
  const std::vector<Equation> input = {equation({1, 2}, false)};
  for (const bool interrupt : {false, true}) {
    auto bounded = limits();
    // Build costs 3; an unsuccessful predecessor comparison costs 2,
    // leaving too little for the fresh row's 3-unit conditioning charge.
    if (!interrupt) bounded.maximum_work = 4;
    AffineMatrix matrix;
    assert(build_affine_matrix(input, 95, bounded,
                               [](uint64_t) { return true; }, matrix) ==
           MatrixStatus::Complete);
    MatrixEvent event;
    const std::vector<MatrixAssignment> prefix = {{1, false}};
    assert(matrix.synchronize(prefix, [](uint64_t) { return true; }, event) ==
           MatrixStatus::Complete);
    assert(matrix.synchronize({{1, false}, {2, true}},
                              [](uint64_t) { return true; }, event) ==
           MatrixStatus::Complete && event.kind == MatrixEventKind::Conflict);
    const auto rows = matrix.rows();
    const auto trail = matrix.trail();
    const auto work = matrix.work();
    const auto kind = matrix.last_synchronization_kind();
    const auto incremental = matrix.last_synchronization_incremental();
    const auto restored = matrix.last_synchronization_restored_predecessor();
    const auto conditioned = matrix.last_synchronization_conditioned();
    const auto retained = matrix.last_synchronization_prefix();
    const auto prior_event = event;
    std::vector<uint64_t> charges;
    const auto result = matrix.synchronize(
        {{1, true}},
        [&](uint64_t amount) {
          charges.push_back(amount);
          return !interrupt || charges.size() == 1;
        }, event);
    assert(result == (interrupt ? MatrixStatus::Interrupted
                                : MatrixStatus::Exhausted));
    assert((charges == (interrupt ? std::vector<uint64_t>{2, 3}
                                  : std::vector<uint64_t>{2})));
    assert(matrix.rows() == rows && matrix.trail() == trail &&
           matrix.work() == work && matrix.last_synchronization_kind() == kind &&
           matrix.last_synchronization_incremental() == incremental &&
           matrix.last_synchronization_restored_predecessor() == restored &&
           matrix.last_synchronization_conditioned() == conditioned &&
           matrix.last_synchronization_prefix() == retained);
    assert(event.kind == prior_event.kind && event.row == prior_event.row &&
           event.reason == prior_event.reason &&
           event.implied_variable == prior_event.implied_variable &&
           event.implied_value == prior_event.implied_value);
    assert(matrix.authenticates(95, input, event,
                                [](uint64_t) { return true; }));
    // The failed nonmatching request must also preserve the hidden checkpoint.
    MatrixStatus status;
    assert(synchronize_work(matrix, prefix, status, event) == 2 &&
           status == MatrixStatus::Complete &&
           matrix.last_synchronization_restored_predecessor() &&
           event.kind == MatrixEventKind::Implication &&
           event.implied_variable == 2 && !event.implied_value);
    check_reason(input, event);
  }
}

int main() {
  derived_implication_conflict_and_backtrack();
  reason_excludes_unrelated_assignments();
  bounded_partial_admission_is_deterministic();
  partial_implication_conflict_and_backtrack();
  transactional_limits_and_validation();
  incremental_extension_matches_snapshot_reducer();
  incremental_multiple_events_backtrack_and_interruption();
  partial_synchronization_forces_next_rebuild();
  retained_rows_reduce_extension_work();
  exhaustive_small_reason_oracle();
  bounded_predecessor_restore();
  nonmatching_predecessor_failure_is_atomic();
  std::puts("affine matrix: exact sources, implication/conflict reasons, "
            "backtrack, and transactional bounds PASS");
}
