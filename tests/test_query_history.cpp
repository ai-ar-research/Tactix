#undef NDEBUG
#include "session/query_history.hpp"
#include "test_io.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix::session;

static std::string render(const QueryHistory &history, uint64_t id) {
  FILE *out = std::tmpfile();
  assert(out);
  assert(history.write_query_dimacs(id, out));
  const std::string result = stream_contents(out);
  assert(std::fclose(out) == 0);
  return result;
}

static void clause(QueryHistory &history, std::initializer_list<int32_t> lits) {
  for (const int32_t lit : lits)
    assert(history.add(lit) == InputStatus::Ok);
  assert(history.add(0) == InputStatus::Ok);
}

// Independent semantic oracle for the exact DIMACS handed to a query checker.
// Fixed small fixtures only: every assignment is enumerated, with no solver,
// random generator or normalization shared with QueryHistory.
static uint64_t models(const std::string &text) {
  std::istringstream input(text);
  std::string p, cnf;
  unsigned variables, clauses;
  assert(input >> p >> cnf >> variables >> clauses);
  assert(p == "p" && cnf == "cnf" && variables <= 6);
  std::vector<std::vector<int>> formula;
  std::vector<int> current;
  int literal;
  while (input >> literal) {
    if (literal) current.push_back(literal);
    else {
      formula.push_back(current);
      current.clear();
    }
  }
  assert(current.empty() && formula.size() == clauses);
  uint64_t result = 0;
  for (unsigned assignment = 0; assignment < (1u << variables); ++assignment) {
    bool valid = true;
    for (const auto &c : formula) {
      bool satisfied = false;
      for (const int l : c) {
        const unsigned v = static_cast<unsigned>(l < 0 ? -l : l);
        assert(v && v <= variables);
        satisfied |= ((assignment >> (v - 1)) & 1u) == (l > 0);
      }
      valid &= satisfied;
    }
    if (valid) result |= uint64_t{1} << assignment;
  }
  return result;
}

static void query_scope_and_prefix() {
  QueryHistory history;
  assert(history.state() == InputState::Input && history.queries() == 0);
  assert(!history.query(0) && !history.query(1));
  assert(!history.active_query() && !history.result_query());
  assert(history.begin_query() == InputStatus::Ok);
  assert(history.active_query()->id == 1);
  assert(render(history, 1) == "p cnf 0 0\n");
  assert(history.finish_query(1, QueryResult::Sat) == InputStatus::Ok);
  assert(history.result_query()->id == 1);

  clause(history, {1, 1, -2});  // exact multiplicity matters to proof input
  clause(history, {2, -2});     // preserve a tautology and its original order
  clause(history, {1, 1, -2});  // preserve duplicate clauses too
  assert(history.assume(-1) == InputStatus::Ok);
  assert(history.assume(2) == InputStatus::Ok);
  assert(history.assume(2) == InputStatus::Ok);
  assert(!history.result_query());
  assert(history.begin_query() == InputStatus::Ok);
  const auto &record = *history.active_query();
  assert(record.id == 2 && record.clauses == 3 && record.input_words == 11);
  assert(record.formula_variables == 2 && record.query_variables == 2);
  assert(record.assumptions == std::vector<int32_t>({-1, 2, 2}));
  assert(history.pending_assumptions().empty());
  const std::string query2 = "p cnf 2 6\n1 1 -2 0\n2 -2 0\n1 1 -2 0\n"
                             "-1 0\n2 0\n2 0\n";
  assert(render(history, 2) == query2);
  assert(models(query2) == 0);
  assert(history.finish_query(2, QueryResult::Unsat) == InputStatus::Ok);
  assert(history.state() == InputState::Unsat && history.result_query()->id == 2);

  // UNSAT under assumptions never becomes permanent base UNSAT.
  assert(history.begin_query() == InputStatus::Ok);
  assert(history.active_query()->assumptions.empty());
  assert(models(render(history, 3)) == 0b1011);
  assert(history.finish_query(3, QueryResult::Sat) == InputStatus::Ok);
  assert(history.assume(-6) == InputStatus::Ok); // new assumption-only variable
  assert(history.begin_query() == InputStatus::Ok);
  assert(history.active_query()->formula_variables == 2);
  assert(history.active_query()->query_variables == 6);
  assert(history.finish_query(4, QueryResult::Unknown) == InputStatus::Ok);
  assert(history.state() == InputState::Input && !history.result_query());
  assert(history.query(4)->completed);
  assert(history.begin_query() == InputStatus::Ok);
  assert(history.active_query()->query_variables == 2);
  assert(history.active_query()->assumptions.empty());
  assert(history.finish_query(5, QueryResult::Sat) == InputStatus::Ok);

  clause(history, {-4});
  assert(history.formula_variables() == 4);
  assert(history.begin_query() == InputStatus::Ok);
  assert(history.active_query()->clauses == 4);
  assert(history.finish_query(6, QueryResult::Sat) == InputStatus::Ok);
  assert(render(history, 1) == "p cnf 0 0\n");
  assert(render(history, 2) == query2); // old queries bind the old prefix
  assert(render(history, 3) == "p cnf 2 3\n1 1 -2 0\n2 -2 0\n1 1 -2 0\n");

  clause(history, {});
  assert(!history.result_query());
  assert(history.begin_query() == InputStatus::Ok);
  assert(models(render(history, 7)) == 0); // empty permanent clause survives
  assert(history.finish_query(7, QueryResult::Unsat) == InputStatus::Ok);
}

static void invalid_calls_are_transactional() {
  QueryHistory history;
  constexpr auto min = std::numeric_limits<int32_t>::min();
  assert(history.add(min) == InputStatus::InvalidLiteral);
  assert(history.assume(min) == InputStatus::InvalidLiteral);
  assert(history.assume(0) == InputStatus::InvalidLiteral);
  assert(history.finish_query(1, QueryResult::Sat) == InputStatus::InvalidState);
  assert(history.assume(-3) == InputStatus::Ok);
  assert(history.add(1) == InputStatus::Ok);
  assert(history.clauses() == 0 && history.input().empty());
  assert(history.formula_variables() == 0);
  assert(history.begin_query() == InputStatus::UnterminatedClause);
  assert(history.queries() == 0 && history.pending_assumptions().size() == 1);
  assert(history.pending_clause().size() == 1);
  assert(history.add(0) == InputStatus::Ok);
  assert(history.begin_query() == InputStatus::Ok);
  assert(history.begin_query() == InputStatus::InvalidState);
  assert(history.add(0) == InputStatus::InvalidState);
  assert(history.add(4) == InputStatus::InvalidState);
  assert(history.assume(2) == InputStatus::InvalidState);
  assert(history.clauses() == 1 && history.pending_clause().empty());
  assert(history.finish_query(0, QueryResult::Sat) == InputStatus::InvalidQuery);
  assert(history.finish_query(2, QueryResult::Unsat) == InputStatus::InvalidQuery);
  assert(history.finish_query(1, static_cast<QueryResult>(1)) ==
         InputStatus::InvalidResult);
  assert(history.active_query()->id == 1 && !history.active_query()->completed);
  assert(history.finish_query(1, QueryResult::Unknown) == InputStatus::Ok);
  assert(history.finish_query(1, QueryResult::Sat) == InputStatus::InvalidState);
  assert(history.pending_assumptions().empty());
  assert(!history.write_query_dimacs(1, nullptr));
  FILE *out = std::tmpfile();
  assert(out);
  assert(!history.write_query_dimacs(0, out));
  assert(!history.write_query_dimacs(2, out));
  assert(std::ftell(out) == 0);
  assert(std::fclose(out) == 0);

  // Invalid literals must not invalidate an existing successful result.
  assert(history.begin_query() == InputStatus::Ok);
  assert(history.finish_query(2, QueryResult::Sat) == InputStatus::Ok);
  assert(history.assume(0) == InputStatus::InvalidLiteral);
  assert(history.add(min) == InputStatus::InvalidLiteral);
  assert(history.result_query()->id == 2);
}

static void bounds_and_lifetimes() {
  QueryHistory history;
  const auto max = std::numeric_limits<int32_t>::max();
  clause(history, {-max, max});
  assert(history.assume(max) == InputStatus::Ok);
  assert(history.begin_query() == InputStatus::Ok);
  const std::string first = "p cnf 2147483647 2\n-2147483647 2147483647 0\n"
                            "2147483647 0\n";
  assert(render(history, 1) == first);
  assert(history.finish_query(1, QueryResult::Unknown) == InputStatus::Ok);
  for (uint64_t id = 2; id <= 1024; ++id) {
    clause(history, {static_cast<int32_t>(id)});
    assert(history.assume(-static_cast<int32_t>(id)) == InputStatus::Ok);
    assert(history.begin_query() == InputStatus::Ok);
    assert(history.active_query()->id == id);
    assert(history.finish_query(id, QueryResult::Unsat) == InputStatus::Ok);
  }
  assert(render(history, 1) == first);
  assert(history.query(1)->assumptions == std::vector<int32_t>({max}));
  assert(history.query(512)->assumptions == std::vector<int32_t>({-512}));
  assert(!history.query(std::numeric_limits<uint64_t>::max()));
}

static void independent_histories_and_write_errors() {
  QueryHistory a, b;
  clause(a, {1});
  clause(b, {-1});
  assert(a.begin_query() == InputStatus::Ok);
  assert(b.begin_query() == InputStatus::Ok);
  assert(models(render(a, 1)) == 0b10);
  assert(models(render(b, 1)) == 0b01);
  assert(a.finish_query(1, QueryResult::Sat) == InputStatus::Ok);
  assert(b.state() == InputState::Solving);
  // /dev/full is available on Linux. Unbuffered I/O makes failure observable
  // to the writer itself rather than deferred to its caller's flush.
  if (FILE *full = std::fopen("/dev/full", "w")) {
    assert(std::setvbuf(full, nullptr, _IONBF, 0) == 0);
    assert(!a.write_query_dimacs(1, full));
    std::fclose(full);
    assert(a.result_query()->id == 1);
  }
  assert(b.finish_query(1, QueryResult::Sat) == InputStatus::Ok);
}

static void original_query_models() {
  QueryHistory history;
  clause(history, {1, -2});
  clause(history, {2, -2});
  assert(history.assume(-3) == InputStatus::Ok);
  assert(history.begin_query() == InputStatus::Ok);
  const auto value = [](unsigned bits) {
    return [bits](int32_t variable) {
      assert(variable > 0 && variable <= 3);
      return (bits >> (variable - 1)) & 1 ? 1 : -1;
    };
  };
  const uint64_t expected = models(render(history, 1));
  for (unsigned assignment = 0; assignment < 8; ++assignment) {
    const ModelCheck result = history.check_model(1, value(assignment));
    assert((result.status == ModelStatus::Valid) ==
           ((expected >> assignment) & 1));
  }
  const auto wrong_assumption = history.check_model(1, value(0b101));
  assert(wrong_assumption.status == ModelStatus::FalsifiedAssumption);
  assert(wrong_assumption.index == 1);
  const auto wrong_clause = history.check_model(1, value(0b010));
  assert(wrong_clause.status == ModelStatus::FalsifiedClause);
  assert(wrong_clause.index == 1);
  for (const int invalid : {0, -2, 2}) {
    const auto check = history.check_model(1, [invalid](int32_t) { return invalid; });
    assert(check.status == ModelStatus::InvalidValue);
  }
  assert(history.check_model(0, value(0)).status == ModelStatus::InvalidQuery);
  assert(history.finish_query(1, QueryResult::Sat) == InputStatus::Ok);
  clause(history, {-1});
  assert(history.begin_query() == InputStatus::Ok);
  assert(history.check_model(1, value(1)).status == ModelStatus::Valid);
  assert(history.check_model(2, value(1)).status == ModelStatus::FalsifiedClause);
  assert(history.finish_query(2, QueryResult::Sat) == InputStatus::Ok);
  clause(history, {});
  assert(history.begin_query() == InputStatus::Ok);
  const auto empty_clause = history.check_model(3, value(0));
  assert(empty_clause.status == ModelStatus::FalsifiedClause && empty_clause.index == 4);
}

int main() {
  query_scope_and_prefix();
  invalid_calls_are_transactional();
  bounds_and_lifetimes();
  independent_histories_and_write_errors();
  original_query_models();
  std::puts("query history: PASS");
}
