#undef NDEBUG
#include "rules/semantic_extension/affine_matrix.hpp"

#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <set>
#include <utility>

using namespace tactix::affine_semantic_extension;

static Equation equation(std::initializer_list<External> variables, bool rhs) {
  Equation result;
  result.variables = variables;
  result.rhs = rhs;
  return result;
}

static MatrixLimits limits() { return {1000000, 64, 64, 64, 64, 64, true}; }

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

static void exact_core_reason_is_direct_and_mode_stable() {
  // Variables 3,4,6,7 occur in the source union but cancel from the selected
  // XOR. SourceSupport deliberately retains assigned canceled variables as a
  // clausal weakening; ExactCore must omit them even when the identical
  // conditioned matrix snapshot is reused immediately afterwards.
  const std::vector<Equation> input = {
      equation({1, 2, 3, 4, 6, 7}, false),
      equation({3, 4, 5, 6, 7}, true)};
  AffineMatrix matrix;
  assert(build_affine_matrix(input, 17, limits(),
                             [](uint64_t) { return true; }, matrix) ==
         MatrixStatus::Complete);
  const std::vector<MatrixAssignment> trail = {
      {1, false}, {2, false}, {3, false}, {4, false}};
  const MatrixIncrementalLimits incremental{1u << 24};
  MatrixEvent source;
  assert(matrix.synchronize(trail, [](uint64_t) { return true; }, source,
                            true, &incremental, false) ==
         MatrixStatus::Complete);
  assert(source.kind == MatrixEventKind::Implication &&
         source.implied_variable == 5 && source.implied_value &&
         (source.reason == std::vector<External>{1, 2, 3, 4, 5}));

  MatrixEvent exact;
  assert(matrix.synchronize(trail, [](uint64_t) { return true; }, exact,
                            true, &incremental, true) ==
         MatrixStatus::Complete);
  assert(exact.kind == MatrixEventKind::Implication &&
         exact.implied_variable == 5 && exact.implied_value &&
         (exact.reason == std::vector<External>{1, 2, 5}));

  MatrixEvent deferred;
  assert(matrix.synchronize(trail, [](uint64_t) { return true; }, deferred,
                            true, &incremental, true, true) ==
         MatrixStatus::Complete);
  assert(deferred.kind == MatrixEventKind::Implication &&
         deferred.implied_variable == 5 && deferred.implied_value &&
         deferred.reason.empty() && deferred.reason_width == 3 &&
         !deferred.deferred_exact_support.empty());
  std::vector<External> antecedents;
  assert(matrix.inspect_deferred_exact_support(
      deferred, [&](External variable) {
        antecedents.push_back(variable);
        return true;
      }));
  assert((antecedents == std::vector<External>{1, 2}));

  // A stale assignment declines atomically: the signed reason remains absent.
  assert(!matrix.materialize_deferred_exact_reason(
      deferred, [](External variable) {
        return variable == 1 ? int8_t{-1} : int8_t{0};
      }));
  assert(deferred.reason.empty());
  assert(matrix.materialize_deferred_exact_reason(
      deferred, [](External variable) {
        return variable == 1 || variable == 2 ? int8_t{-1} : int8_t{0};
      }));
  assert(deferred.reason == exact.reason);
  check_reason(input, deferred);
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
  // 17 for conditioning and the pivot collision, 1 for the Gauss-Jordan
  // pivot lookup in the reduced row {3,4}.
  assert(synchronize_work(rebuilt, prefix, status, rebuilt_event) == 18 &&
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

static void sliced_reduction_controls() {
  const std::vector<Equation> input={equation({1,3,4},false),equation({2,3,4,5},false)};
  auto l=limits();l.maximum_sources=8;l.maximum_row_width=8;
  AffineMatrix original;
  assert(build_affine_matrix(input,777,l,[](uint64_t){return true;},original)==MatrixStatus::Complete);
  const std::vector<MatrixAssignment> trail={{1,false},{2,false}};
  auto one=original;
  MatrixEvent expected;
  assert(one.synchronize(trail,[](uint64_t){return true;},expected,true)==MatrixStatus::Complete);
  assert(expected.kind==MatrixEventKind::Implication);
  auto run=[&](AffineMatrix &matrix, const std::vector<MatrixAssignment> &values,
               MatrixEvent &event, std::vector<uint64_t> &steps) {
    const auto old_rows=matrix.rows();const auto old_trail=matrix.trail();
    MatrixStatus status=MatrixStatus::Pending;
    uint64_t previous=0;
    size_t calls=0;
    while(status==MatrixStatus::Pending) {
      uint64_t charged=0;
      status=matrix.synchronize_sliced(values,[&](uint64_t n){charged+=n;return true;},event,{120,1000000});
      const auto &effort=matrix.last_slice_effort();
      assert(charged==effort.work && charged<=120);
      assert(effort.cumulative_work>previous);previous=effort.cumulative_work;
      steps.push_back(effort.completed_steps);
      if(status==MatrixStatus::Pending) {
        assert(matrix.rows()==old_rows && matrix.trail()==old_trail);
        assert(matrix.continuation_pending() && matrix.continuation_bytes()>0);
      }
      assert(++calls<100);
    }
    assert(status==MatrixStatus::Complete && calls>1);
    assert(!matrix.continuation_pending());
  };
  auto sliced=original;MatrixEvent event;
  std::vector<uint64_t> trace;
  run(sliced,trail,event,trace);
  assert(event.kind==expected.kind && event.row==expected.row && event.reason==expected.reason);
  assert(sliced.rows()==one.rows() && sliced.trail()==one.trail());
  assert(sliced.authenticates(777,input,event,[](uint64_t){return true;}));
  check_reason(input,event);
  auto replay=original;MatrixEvent replay_event;std::vector<uint64_t> replay_trace;
  run(replay,trail,replay_event,replay_trace);assert(trace==replay_trace);
  // Total completed algebra steps do not depend on the number of slices.
  auto large=original;MatrixEvent large_event;
  assert(large.synchronize_sliced(trail,[](uint64_t){return true;},large_event,{1000000,1000000})==MatrixStatus::Complete);
  uint64_t total=0;for(auto n:trace)total+=n;
  assert(total==large.last_slice_effort().completed_steps);
  // Full-basis slicing is independent of previously conditioned mutable rows.
  auto previously_synchronized=one;MatrixEvent from_previous;
  std::vector<uint64_t> previous_trace;
  run(previously_synchronized,trail,from_previous,previous_trace);
  assert(previous_trace==trace && from_previous.row==expected.row && from_previous.reason==expected.reason);
  const std::vector<MatrixAssignment> conflict_trail={{1,false},{2,false},{5,true}};
  auto conflict_reference=original;MatrixEvent conflict_expected;
  assert(conflict_reference.synchronize(conflict_trail,[](uint64_t){return true;},conflict_expected,true)==MatrixStatus::Complete);
  auto conflict_sliced=one;MatrixEvent conflict_event;std::vector<uint64_t> conflict_trace;
  run(conflict_sliced,conflict_trail,conflict_event,conflict_trace);
  assert(conflict_event.kind==MatrixEventKind::Conflict && conflict_event.row==conflict_expected.row && conflict_event.reason==conflict_expected.reason);
  check_reason(input,conflict_event);
  // Backtracking after a completed slice uses the immutable source basis.
  MatrixEvent backtracked;std::vector<uint64_t> backtrack_trace;
  run(conflict_sliced,trail,backtracked,backtrack_trace);
  assert(backtracked.reason==expected.reason && backtracked.row==expected.row);
  auto too_small_retry=original;MatrixEvent retry_event;
  assert(too_small_retry.synchronize_sliced(trail,[](uint64_t){return true;},retry_event,{120,1000000})==MatrixStatus::Pending);
  const auto prior_cumulative=too_small_retry.last_slice_effort().cumulative_work;
  assert(too_small_retry.synchronize_sliced(trail,[](uint64_t){return true;},retry_event,{3,1000000})==MatrixStatus::Exhausted);
  assert(too_small_retry.continuation_pending() && too_small_retry.last_slice_effort().cumulative_work==prior_cumulative);
  assert(too_small_retry.synchronize_sliced({{2,false},{1,false}},[](uint64_t){return true;},retry_event,{1000000,1000000})==MatrixStatus::Invalid);
  assert(!too_small_retry.continuation_pending());
  auto huge_limits=l;huge_limits.maximum_rows=UINT32_MAX;huge_limits.maximum_variables=UINT32_MAX;
  huge_limits.maximum_row_width=UINT32_MAX;huge_limits.maximum_sources=UINT32_MAX;
  huge_limits.maximum_assignments=UINT32_MAX;
  AffineMatrix huge;
  assert(build_affine_matrix(input,888,huge_limits,[](uint64_t){return true;},huge)==MatrixStatus::Complete);
  assert(huge.synchronize_sliced(trail,[](uint64_t){return true;},retry_event,{1000000,1000000})==MatrixStatus::Complete);
  assert(retry_event.reason==expected.reason && !huge.continuation_pending());
  auto default_limits=l;default_limits.maximum_rows=100000;default_limits.maximum_variables=100000;
  default_limits.maximum_row_width=100000;default_limits.maximum_sources=100000;
  AffineMatrix default_shaped;
  assert(build_affine_matrix(input,999,default_limits,[](uint64_t){return true;},default_shaped)==MatrixStatus::Complete);
  assert(default_shaped.synchronize_sliced(trail,[](uint64_t){return true;},retry_event,{1000000,100000000})==MatrixStatus::Complete);
  assert(retry_event.reason==expected.reason);
  std::vector<Equation> family;
  for(int i=0;i<40;++i) family.push_back(equation({3*i+1,3*i+2,3*i+3},false));
  auto family_limits=default_limits;family_limits.maximum_assignments=100000;
  AffineMatrix family_owner;
  assert(build_affine_matrix(family,1000,family_limits,[](uint64_t){return true;},family_owner)==MatrixStatus::Complete);
  MatrixEvent family_event;MatrixStatus family_status=MatrixStatus::Pending;
  size_t family_calls=0;uint64_t family_peak=0;
  while(family_status==MatrixStatus::Pending) {
    family_status=family_owner.synchronize_sliced({},[](uint64_t){return true;},family_event,{120,100000000});
    family_peak=std::max(family_peak,family_owner.continuation_bytes());
    assert(++family_calls<1000);
  }
  assert(family_status==MatrixStatus::Complete && family_calls>1 && family_peak<100000000);
  assert(family_owner.rows()==family_owner.admitted_rows() && family_event.kind==MatrixEventKind::None);
  std::printf("sliced default-shaped rows=40 calls=%zu private_peak=%llu\n",family_calls,(unsigned long long)family_peak);
  auto interrupted=original;MatrixEvent untouched=expected;
  assert(interrupted.synchronize_sliced(trail,[](uint64_t){return true;},untouched,{120,1000000})==MatrixStatus::Pending);
  size_t interrupted_charges=0;
  assert(interrupted.synchronize_sliced(trail,[&](uint64_t){return interrupted_charges++==0;},untouched,{120,1000000})==MatrixStatus::Interrupted);
  assert(interrupted_charges==2 && interrupted.last_slice_effort().work>0);
  assert(!interrupted.continuation_pending() && interrupted.rows()==original.rows());
  assert(untouched.reason==expected.reason);
  // Reassignment discards the private prefix and matches a fresh reducer.
  auto changed=original;
  assert(changed.synchronize_sliced(trail,[](uint64_t){return true;},untouched,{120,1000000})==MatrixStatus::Pending);
  const std::vector<MatrixAssignment> opposite={{1,true},{2,false}};
  assert(changed.synchronize_sliced(opposite,[](uint64_t){return true;},untouched,{120,1000000})==MatrixStatus::Pending);
  assert(changed.last_slice_effort().invalidated && !changed.last_slice_effort().resumed);
  MatrixStatus changed_status=MatrixStatus::Pending;
  while(changed_status==MatrixStatus::Pending)
    changed_status=changed.synchronize_sliced(opposite,[](uint64_t){return true;},untouched,{120,1000000});
  auto changed_reference=original;MatrixEvent changed_expected;
  assert(changed_reference.synchronize(opposite,[](uint64_t){return true;},changed_expected,true)==MatrixStatus::Complete);
  assert(changed_status==MatrixStatus::Complete && untouched.row==changed_expected.row && untouched.reason==changed_expected.reason);
  changed.cancel_continuation();assert(!changed.continuation_pending());
  assert(changed.synchronize_sliced(trail,[](uint64_t){return true;},untouched,{120,1})==MatrixStatus::Exhausted);
  assert(!changed.continuation_pending());
  assert(changed.synchronize_sliced(trail,[](uint64_t){return true;},untouched,{10,1000000})==MatrixStatus::Exhausted);
  assert(changed.last_slice_effort().minimum_slice_work>10);
  changed.cancel_continuation();
  assert(changed.synchronize_sliced(trail,[](uint64_t){return true;},untouched,{120,1000000})==MatrixStatus::Pending);
  assert(changed.project({},[](uint64_t){return true;})==MatrixStatus::Complete);
  assert(!changed.continuation_pending());
}

// Gauss-Jordan completeness (CryptoMiniSat keeps its matrix in reduced
// row-echelon form): a conditioned system yields a conflict exactly when it is
// inconsistent, otherwise an implication exactly when some open variable is
// forced. Full rebuild, dirty incremental, sliced and predecessor paths agree.
static void gauss_jordan_completeness_oracle() {
  uint64_t seed = 0x9e3779b97f4a7c15ull;
  const auto next = [&] { seed = seed * 6364136223846793005ull + 1442695040888963407ull; return seed >> 33; };
  const External variables = 5;
  const auto check = [&](const std::vector<Equation> &input,
                         const std::vector<MatrixAssignment> &trail,
                         const MatrixEvent &event) {
    uint64_t fixed = 0, values = 0;
    for (const auto assignment : trail) {
      fixed |= uint64_t{1} << (assignment.variable - 1);
      if (assignment.value) values |= uint64_t{1} << (assignment.variable - 1);
    }
    uint64_t always_true = (uint64_t{1} << variables) - 1, always_false = always_true;
    bool consistent = false;
    for (uint64_t completion = 0; completion < (uint64_t{1} << variables); ++completion) {
      if ((completion & fixed) != values) continue;
      bool satisfied = true;
      for (const auto &equation : input)
        satisfied &= parity(equation.variables, completion) == equation.rhs;
      if (!satisfied) continue;
      consistent = true;
      always_true &= completion;
      always_false &= ~completion;
    }
    if (!consistent) { assert(event.kind == MatrixEventKind::Conflict); return; }
    const uint64_t forced = (always_true | always_false) & ~fixed &
                            ((uint64_t{1} << variables) - 1);
    if (!forced) { assert(event.kind == MatrixEventKind::None); return; }
    assert(event.kind == MatrixEventKind::Implication);
    const uint64_t bit = uint64_t{1} << (event.implied_variable - 1);
    assert(forced & bit);
    assert(event.implied_value == bool(always_true & bit));
    check_reason(input, event);
  };
  for (int round = 0; round < 300; ++round) {
    std::vector<Equation> input(3);
    for (auto &equation : input) {
      uint64_t mask = 0;
      while (!mask) mask = next() & 31;
      for (External variable = 1; variable <= variables; ++variable)
        if (mask & (uint64_t{1} << (variable - 1))) equation.variables.push_back(variable);
      equation.rhs = next() & 1;
    }
    AffineMatrix pristine;
    const auto built = build_affine_matrix(input, 31, limits(), [](uint64_t) { return true; }, pristine);
    if (built == MatrixStatus::NoStructure) continue;
    assert(built == MatrixStatus::Complete);
    uint32_t ternary_count = 1;
    for (External variable = 1; variable <= variables; ++variable) ternary_count *= 3;
    for (uint32_t ternary = 0; ternary < ternary_count; ++ternary) {
      uint32_t value = ternary;
      std::vector<MatrixAssignment> trail;
      for (External variable = 1; variable <= variables; ++variable) {
        const uint32_t digit = value % 3;
        value /= 3;
        if (digit) trail.push_back({variable, digit == 2});
      }
      AffineMatrix full = pristine;
      MatrixEvent event;
      assert(full.synchronize(trail, [](uint64_t) { return true; }, event) == MatrixStatus::Complete);
      check(input, trail, event);
      // Sliced reduction of the same snapshot.
      AffineMatrix sliced = pristine;
      MatrixStatus status = MatrixStatus::Pending;
      while (status == MatrixStatus::Pending)
        status = sliced.synchronize_sliced(trail, [](uint64_t) { return true; }, event, {1000, 1000000});
      assert(status == MatrixStatus::Complete);
      check(input, trail, event);
      // Dirty incremental extension: index the empty trail, then extend by
      // each sorted prefix of the snapshot.
      AffineMatrix dirty = pristine;
      const MatrixIncrementalLimits index{1u << 24};
      assert(dirty.synchronize({}, [](uint64_t) { return true; }, event, true, &index) == MatrixStatus::Complete);
      std::vector<MatrixAssignment> prefix;
      for (const auto assignment : trail) {
        prefix.push_back(assignment);
        assert(dirty.synchronize(prefix, [](uint64_t) { return true; }, event, true, &index) == MatrixStatus::Complete);
        check(input, prefix, event);
      }
      // Ordinary extension then predecessor restore on backtrack.
      if (!trail.empty()) {
        AffineMatrix restore = pristine;
        std::vector<MatrixAssignment> shorter(trail.begin(), trail.end() - 1);
        assert(restore.synchronize(shorter, [](uint64_t) { return true; }, event) == MatrixStatus::Complete);
        assert(restore.synchronize(trail, [](uint64_t) { return true; }, event) == MatrixStatus::Complete);
        check(input, trail, event);
        assert(restore.synchronize(shorter, [](uint64_t) { return true; }, event) == MatrixStatus::Complete);
        check(input, shorter, event);
      }
    }
  }
}

static void packed_provenance_crosses_words() {
  std::vector<Equation> input;
  input.reserve(130);
  for (size_t source = 0; source < 130; ++source) {
    Equation equation;
    equation.variables = {1, 2};
    equation.rhs = source != 0;
    input.push_back(std::move(equation));
  }
  auto roomy = limits();
  roomy.maximum_rows = 256;
  roomy.maximum_variables = 256;
  roomy.maximum_row_width = 256;
  roomy.maximum_sources = 256;
  roomy.maximum_assignments = 256;
  roomy.maximum_work = 10000000;
  AffineMatrix matrix;
  assert(build_affine_matrix(input, 5150, roomy,
                             [](uint64_t) { return true; }, matrix) ==
         MatrixStatus::Complete);
  std::vector<bool> seen(130, false);
  for (const MatrixRow &row : matrix.rows()) {
    if (!row.variables.empty()) {
      assert((row.variables == std::vector<External>{1, 2}) && !row.rhs &&
             row.source_equations == std::vector<size_t>{0});
      continue;
    }
    assert(row.rhs && row.source_equations.size() == 2 &&
           row.source_equations.front() == 0 &&
           row.source_equations.back() < seen.size());
    seen[row.source_equations.back()] = true;
  }
  assert(!seen[0]);
  for (size_t source = 1; source < seen.size(); ++source)
    assert(seen[source]);
}

static void source_components_are_exact_and_stable() {
  const std::vector<Equation> disconnected = {
      equation({1, 2}, false), equation({1, 3}, true),
      equation({4, 5}, false), equation({4, 6}, true)};
  AffineMatrix first;
  assert(build_affine_matrix(disconnected, 6001, limits(),
                             [](uint64_t) { return true; }, first) ==
         MatrixStatus::Complete);
  std::set<uint32_t> components;
  for (const MatrixRow &row : first.admitted_rows()) {
    components.insert(row.component);
    for (size_t source : row.source_equations)
      assert((source < 2) == (row.component == 0));
  }
  assert((components == std::set<uint32_t>{0, 1}));

  AffineMatrix repeated;
  assert(build_affine_matrix(disconnected, 6001, limits(),
                             [](uint64_t) { return true; }, repeated) ==
         MatrixStatus::Complete);
  assert(first.admitted_rows() == repeated.admitted_rows() &&
         first.work() == repeated.work());

  auto connected = disconnected;
  connected.push_back(equation({3, 4}, false));
  AffineMatrix joined;
  assert(build_affine_matrix(connected, 6002, limits(),
                             [](uint64_t) { return true; }, joined) ==
         MatrixStatus::Complete);
  for (const MatrixRow &row : joined.admitted_rows())
    assert(row.component == 0);
}

int main() {
  sliced_reduction_controls();
  derived_implication_conflict_and_backtrack();
  reason_excludes_unrelated_assignments();
  exact_core_reason_is_direct_and_mode_stable();
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
  gauss_jordan_completeness_oracle();
  packed_provenance_crosses_words();
  source_components_are_exact_and_stable();
  std::puts("affine matrix: exact sources, implication/conflict reasons, "
            "backtrack, and transactional bounds PASS");
}
