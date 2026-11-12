#undef NDEBUG
#include "session/retained_clauses.hpp"
#include <cassert>
using namespace tactix;
using namespace tactix::session;
static RetentionLimits limits() {return {64,8192,64,4096,16,64,4096};}
static const std::vector<PbpParityEquation> equations{{{1,2,3},false},{{1,2,4},false}};
static void source(QueryHistory &h,const PbpParityEquation &row) {
  assert(visit_parity_source(row,[&](const auto &clause) {
    for(int32_t literal:clause) assert(h.add(literal)==InputStatus::Ok);
    return h.add(0)==InputStatus::Ok;
  }));
}
static void formula(QueryHistory &h) {
  for(const auto &row:equations) source(h,row);
  assert(h.begin_query()==InputStatus::Ok);
}
static RetentionResult admit(RetainedClauses &pool,const QueryHistory &h,
    const std::vector<PbpParityEquation> &rows=equations,
    const std::vector<int32_t> &reason={-3,4},uint64_t budget=UINT64_MAX,
    const volatile sig_atomic_t *stop=nullptr) {
  return pool.admit_parity(h,h.active_query()->id,rows,reason,budget,stop);
}
int main() {
  QueryHistory h;formula(h);
  RetainedClauses pool(limits());const int32_t reason[]{-3,4};
  assert(pool.admit(h,1,reason,UINT64_MAX).status==RetentionStatus::NotRup);
  auto result=admit(pool,h);
  assert(result.status==RetentionStatus::Accepted && result.ticks);
  assert(pool.literals()==2 && pool.recipe_literals()==8 && pool.clauses().size()==1);
  assert(pool.clauses()[0].parity_equations==equations);
  assert(admit(pool,h).status==RetentionStatus::AlreadyPresent);
  for(unsigned mask=0;mask<16;++mask) {
    bool premises=true;
    for(const auto &row:equations) {
      bool parity=false;for(int v:row.variables)parity^=bool(mask&(1u<<(v-1)));
      premises&=parity==row.rhs;
    }
    if(premises) assert(!(mask&4) || (mask&8));
  }
  assert(h.finish_query(1,QueryResult::Unknown)==InputStatus::Ok);
  assert(h.add(3)==InputStatus::Ok && h.add(0)==InputStatus::Ok && h.begin_query()==InputStatus::Ok);
  const int32_t four[]{4};
  assert(pool.admit(h,2,four,UINT64_MAX).status==RetentionStatus::Accepted);
  assert(pool.admit_parity(h,1,equations,reason,UINT64_MAX).status==RetentionStatus::Stale);
  QueryHistory foreign;formula(foreign);
  assert(admit(pool,foreign).status==RetentionStatus::Stale);

  QueryHistory plain;formula(plain);
  RetainedClauses bad(limits());
  assert(admit(bad,plain,{},{}).status==RetentionStatus::Invalid);
  assert(admit(bad,plain,equations,{3,4}).status==RetentionStatus::Invalid);
  assert(admit(bad,plain,equations,{-3}).status==RetentionStatus::Invalid);
  assert(admit(bad,plain,equations,{3,-3,4}).status==RetentionStatus::Tautology);
  for(auto row:std::vector<PbpParityEquation>{{{},false},{{1},false},{{2,1},false},{{1,1},false},{{0,1},false},{{1,5},false}})
    assert(admit(bad,plain,{row}).status==RetentionStatus::Invalid);
  auto wrong=equations;wrong[1].rhs=true;
  assert(admit(bad,plain,wrong,{3,4}).status==RetentionStatus::NotRup);
  assert(bad.clauses().empty() && !bad.literals() && !bad.recipe_literals());
  assert(bad.admit(plain,1,reason,UINT64_MAX).status==RetentionStatus::NotRup);
  assert(admit(bad,plain,equations,{-3,4},1).status==RetentionStatus::Exhausted);
  volatile sig_atomic_t stop=1;
  assert(admit(bad,plain,equations,{-3,4},UINT64_MAX,&stop).status==RetentionStatus::Interrupted);
  assert(admit(bad,plain,equations,{1,-3,4}).status==RetentionStatus::Accepted);
  assert(admit(bad,plain,{equations[0],equations[0]},{}).status==RetentionStatus::Invalid);
  auto bounded=limits();bounded.maximum_literals=9;RetainedClauses small(bounded);
  assert(admit(small,plain).status==RetentionStatus::Exhausted);
  bounded.maximum_literals=10;RetainedClauses exact(bounded);
  assert(admit(exact,plain).status==RetentionStatus::Accepted);
  const int32_t original[]{1,2,-3};
  assert(exact.admit(plain,1,original,UINT64_MAX).status==RetentionStatus::Exhausted);
  RetainedClauses disabled({});assert(admit(disabled,plain).status==RetentionStatus::Disabled);

  // An assumption making an equation true is not a permanent premise.
  QueryHistory conditional;
  assert(conditional.add(1)==InputStatus::Ok && conditional.add(2)==InputStatus::Ok && conditional.add(0)==InputStatus::Ok);
  assert(conditional.assume(1)==InputStatus::Ok && conditional.assume(-2)==InputStatus::Ok && conditional.begin_query()==InputStatus::Ok);
  RetainedClauses scoped(limits());
  assert(admit(scoped,conditional,{{{1,2},true}},{1,2}).status==RetentionStatus::NotRup);
  assert(scoped.clauses().empty());
  QueryHistory inconsistent;
  source(inconsistent,equations[0]);auto opposite=equations[0];opposite.rhs=true;source(inconsistent,opposite);
  assert(inconsistent.begin_query()==InputStatus::Ok);
  RetainedClauses contradiction(limits());
  assert(admit(contradiction,inconsistent,{equations[0],opposite},{}).status==RetentionStatus::Accepted);
}
