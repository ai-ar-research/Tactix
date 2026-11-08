#undef NDEBUG
#include "session/retained_clauses.hpp"
#include "engine/subsolver.hpp"
#include "kernel/types.hpp"

#include <cassert>
#include <limits>

using namespace tactix;
using namespace tactix::session;

static RetentionLimits limits() { return {16, 1000, 32, 256, 16, 64, 512}; }
static void clause(QueryHistory &h, std::initializer_list<int32_t> literals) {
  for (int32_t literal : literals) assert(h.add(literal) == InputStatus::Ok);
  assert(h.add(0) == InputStatus::Ok);
}
static void formula(QueryHistory &h) {
  clause(h, {1, 2});
  clause(h, {1, -2});
  clause(h, {-1, 3, 4});
  clause(h, {-1, 3, -4});
}

static void propagation_admission() {
  SubSolver s;
  assert(s.admit_rup({}, UINT64_MAX) == SubRupAdmission::Unavailable);
  s.reset(4);
  s.add_clause({internalize(1), internalize(2)});
  s.add_clause({internalize(1), internalize(-2)});
  s.add_clause({internalize(-1), internalize(3), internalize(4)});
  s.add_clause({internalize(-1), internalize(3), internalize(-4)});
  const auto unlimited = UINT64_MAX;
  assert(s.admit_rup({internalize(3)}, unlimited) == SubRupAdmission::NotRup);
  assert(s.admit_rup({internalize(1)}, 0) == SubRupAdmission::Budget);
  volatile sig_atomic_t stop = 1;
  assert(s.admit_rup({internalize(1)}, unlimited, &stop) == SubRupAdmission::Interrupted);
  assert(s.admit_rup({internalize(1), internalize(-1)}, unlimited) == SubRupAdmission::Tautology);
  assert(s.admit_rup({internalize(5)}, unlimited) == SubRupAdmission::Invalid);
  assert(s.admit_rup({internalize(1), internalize(1)}, unlimited) == SubRupAdmission::Accepted);
  assert(s.admit_rup({internalize(3)}, unlimited) == SubRupAdmission::Accepted);
  assert(s.fixed_literal(internalize(1)) > 0 && s.fixed_literal(internalize(3)) > 0);
  assert(s.admit_rup({internalize(2)}, unlimited) == SubRupAdmission::NotRup);
  assert(s.solve({}, 0) == SubResult::Sat);
  assert(s.admit_rup({internalize(1)}, unlimited) == SubRupAdmission::Unavailable);
  assert(s.propagate_assumptions_only({}, unlimited) == SubPropagationResult::Consistent);
  assert(s.admit_rup({internalize(1)}, unlimited) == SubRupAdmission::Unavailable);
  s.reset(1, SubSearchMode::Kitten);
  assert(s.admit_rup({}, unlimited) == SubRupAdmission::Unavailable);
  s.reset(0);
  s.add_clause({});
  assert(s.admit_rup({}, unlimited) == SubRupAdmission::Accepted);
  assert(s.solve({}, 0) == SubResult::Unsat);
  assert(s.refutation_target()); // the original empty-clause source survives
  s.reset(3);
  s.add_clause({internalize(-1)});
  s.add_clause({internalize(1), internalize(2), internalize(3)});
  s.add_clause({internalize(1), internalize(2), internalize(-3)});
  assert(s.admit_rup({internalize(1), internalize(2)}, unlimited) == SubRupAdmission::Accepted);
  assert(s.propagate_assumptions_only({}, unlimited) == SubPropagationResult::Consistent);
  assert(s.fixed_literal(internalize(2)) > 0); // new watched clause re-arms roots
}

static void dependency_and_prefix_binding() {
  QueryHistory h;
  formula(h);
  assert(h.assume(2) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  RetainedClauses pool(limits());
  const int32_t a[] = {1}, b[] = {2}, c[] = {3};
  assert(pool.admit(h, 1, b, UINT64_MAX).status == RetentionStatus::NotRup);
  assert(pool.admit(h, 1, c, UINT64_MAX).status == RetentionStatus::NotRup);
  assert(pool.admit(h, 1, a, UINT64_MAX).status == RetentionStatus::Accepted);
  assert(pool.admit(h, 1, c, UINT64_MAX).status == RetentionStatus::Accepted);
  assert(pool.clauses().size() == 2 && pool.literals() == 2);
  assert(pool.clauses()[0].source_query == 1);
  assert(pool.clauses()[0].source_input_words == h.query(1)->input_words);
  assert(pool.admit(h, 1, a, UINT64_MAX).status == RetentionStatus::AlreadyPresent);
  QueryHistory foreign;
  formula(foreign);
  assert(foreign.begin_query() == InputStatus::Ok);
  assert(pool.admit(foreign, 1, a, UINT64_MAX).status == RetentionStatus::Stale);
  assert(h.finish_query(1, QueryResult::Unknown) == InputStatus::Ok);
  clause(h, {5});
  assert(h.begin_query() == InputStatus::Ok);
  const int32_t fresh[] = {5};
  assert(pool.admit(h, 2, fresh, UINT64_MAX).status == RetentionStatus::Accepted);
  assert(pool.admit(h, 1, a, UINT64_MAX).status == RetentionStatus::Stale);
  assert(pool.clauses().size() == 3);
}

static void append_oracle_inputs() {
  SubSolver s;
  const auto unlimited=UINT64_MAX;
  assert(!s.append_input(1,{internalize(1)}));
  s.reset(2);
  assert(s.add_clause({internalize(1),internalize(2)})==1);
  assert(s.propagate_assumptions_only({internalize(-1)},unlimited)==SubPropagationResult::Consistent);
  assert(s.append_input(3,{internalize(-2),internalize(3)}));
  assert(s.original_clauses()==2 && s.nvars()==3);
  assert(!s.fixed_literal(internalize(1)) && !s.fixed_literal(internalize(2)));
  assert(s.admit_rup({internalize(2)},unlimited)==SubRupAdmission::NotRup);
  assert(s.admit_rup({internalize(1),internalize(3)},unlimited)==SubRupAdmission::Accepted);
  assert(s.append_input(3,{internalize(-1)}));
  assert(s.original_clauses()==4); // input/accepted-lemma identities never restart
  assert(s.propagate_assumptions_only({},unlimited)==SubPropagationResult::Consistent);
  assert(s.fixed_literal(internalize(3))>0);
  assert(!s.append_input(2,{}) && !s.append_input(3,{internalize(4)}));
  assert(!s.append_input(UINT32_MAX,{}));
  assert(s.nvars()==3 && s.original_clauses()==4);
  assert(s.solve({},0)==SubResult::Sat);
  assert(!s.append_input(3,{})); // never invalidate a full-search derivation

  s.reset(40,SubSearchMode::Kitten);
  s.add_clause({internalize(-40)});
  assert(!s.append_input(40,{}));
  s.reset(2);
  s.add_clause({internalize(-1)});s.add_clause({internalize(-2)});
  assert(s.propagate_assumptions_only({},unlimited)==SubPropagationResult::Consistent);
  // Extend within old high-water allocations; both old endpoint literals are
  // false, and duplicates must not masquerade as two distinct open watches.
  assert(s.append_input(40,{internalize(1),internalize(2),internalize(40),internalize(40)}));
  assert(s.propagate_assumptions_only({},unlimited)==SubPropagationResult::Consistent);
  assert(s.fixed_literal(internalize(40))>0);
  assert(s.append_input(40,{internalize(-40)}));
  assert(s.inconsistent() && s.solve({},0)==SubResult::Unsat);
  assert(s.refutation_target());
  s.reset(0);
  assert(s.append_input(0,{}) && s.inconsistent());
  s.reset(2);
  s.add_clause({internalize(1)});
  s.add_clause({internalize(-1),internalize(2)});
  s.add_clause({internalize(-1),internalize(-2)});
  assert(s.propagate_assumptions_only({},unlimited)==SubPropagationResult::Conflict);
  assert(s.inconsistent());
  assert(s.append_input(3,{internalize(3)}));
  assert(s.propagate_assumptions_only({},unlimited)==SubPropagationResult::Conflict);
  assert(s.solve({},0)==SubResult::Unsat && s.refutation_target());
  s.reset(2);
  s.add_clause({internalize(1),internalize(2)});
  assert(s.propagate_assumptions_only({internalize(-1),internalize(-2)},unlimited)==SubPropagationResult::Conflict);
  assert(!s.inconsistent()); // an assumption refutation never becomes base UNSAT
  assert(s.append_input(3,{internalize(3)}));
  assert(s.solve({},0)==SubResult::Sat);
}

static void persistent_prefix_roots() {
  QueryHistory h;
  clause(h,{1});
  for(int32_t v=1;v<8;++v)clause(h,{-v,v+1});
  assert(h.begin_query()==InputStatus::Ok);
  RetainedClauses pool(limits());
  const int32_t eight[]{8},nine[]{9},ten[]{10};
  assert(pool.admit(h,1,eight,UINT64_MAX).status==RetentionStatus::Accepted);
  assert(h.finish_query(1,QueryResult::Unknown)==InputStatus::Ok);
  clause(h,{9});assert(h.begin_query()==InputStatus::Ok);
  volatile sig_atomic_t stop=1;
  assert(pool.admit(h,2,nine,1,&stop).status==RetentionStatus::Interrupted);
  const auto fresh=pool.admit(h,2,nine,1);
  assert(fresh.status==RetentionStatus::Accepted && fresh.ticks==0);
  // Rebuilding the prefix would spend the budget propagating the old chain.
  assert(pool.clauses()[0].source_query==1 && pool.clauses()[1].source_query==2);
  assert(pool.admit(h,1,eight,UINT64_MAX).status==RetentionStatus::Stale);
  assert(h.finish_query(2,QueryResult::Unknown)==InputStatus::Ok);
  clause(h,{-9,10});assert(h.begin_query()==InputStatus::Ok);
  assert(pool.admit(h,3,ten,1).status==RetentionStatus::Accepted);
  assert(pool.clauses().size()==3);
}

static void disabled_bounds_and_retry() {
  QueryHistory h;
  formula(h);
  assert(h.begin_query() == InputStatus::Ok);
  const int32_t a[] = {1}, c[] = {3}, invalid[] = {0}, min[] = {INT32_MIN};
  RetainedClauses disabled({});
  assert(disabled.admit(h, 1, a, UINT64_MAX).status == RetentionStatus::Disabled);
  RetainedClauses pool(limits());
  assert(pool.admit(h, 0, a, UINT64_MAX).status == RetentionStatus::Stale);
  assert(pool.admit(h, 1, invalid, UINT64_MAX).status == RetentionStatus::Invalid);
  assert(pool.admit(h, 1, min, UINT64_MAX).status == RetentionStatus::Invalid);
  assert(pool.admit(h, 1, a, 0).status == RetentionStatus::Exhausted);
  volatile sig_atomic_t stop = 1;
  assert(pool.admit(h, 1, a, UINT64_MAX, &stop).status == RetentionStatus::Interrupted);
  assert(pool.clauses().empty());
  assert(pool.admit(h, 1, a, UINT64_MAX).status == RetentionStatus::Accepted);
  auto bounded = limits(); bounded.maximum_clauses = 1;
  RetainedClauses one(bounded);
  assert(one.admit(h, 1, a, UINT64_MAX).status == RetentionStatus::Accepted);
  assert(one.admit(h, 1, c, UINT64_MAX).status == RetentionStatus::Exhausted);
  bounded = limits(); bounded.maximum_input_words = 1;
  RetainedClauses small(bounded);
  assert(small.admit(h, 1, a, UINT64_MAX).status == RetentionStatus::Exhausted);
  assert(small.clauses().empty());
  QueryHistory sparse;
  clause(sparse, {INT32_MAX});
  assert(sparse.begin_query() == InputStatus::Ok);
  RetainedClauses bounded_variables(limits());
  const int32_t huge[] = {INT32_MAX};
  assert(bounded_variables.admit(sparse, 1, huge, UINT64_MAX).status == RetentionStatus::Exhausted);
  assert(bounded_variables.clauses().empty());
}

int main() {
  propagation_admission();
  append_oracle_inputs();
  persistent_prefix_roots();
  dependency_and_prefix_binding();
  disabled_bounds_and_retry();
}
