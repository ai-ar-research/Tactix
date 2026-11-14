#include "unit_test.hpp"
#include "session/restriction_knowledge.hpp"
using namespace tactix::session;
static RetentionLimits limits() {return {64,4096,256,8192,64,256,8192};}
static void clause(QueryHistory &h,std::initializer_list<int32_t> c) {
  for(int32_t l:c)assert(h.add(l)==InputStatus::Ok);
  assert(h.add(0)==InputStatus::Ok);
}
static void base(QueryHistory &h) {
  clause(h,{1,2});clause(h,{-1,-2});clause(h,{-1,3});clause(h,{-2,4});
}
static DerivationRecord restriction() {
  DerivationRecord r;r.kind=DerivationKind::SubstitutionRedundancy;r.clause={1};r.witness={-2,3};return r;
}
static void ownership_and_failures() {
  QueryHistory h;base(h);assert(h.assume(4)==InputStatus::Ok&&h.begin_query()==InputStatus::Ok);
  RestrictionKnowledge checked(h,*h.active_query(),limits());
  const int32_t p[]{3};
  assert(checked.admit(p,UINT64_MAX).status==RetentionStatus::NotRup);
  assert(checked.admit(restriction(),UINT64_MAX).status==RetentionStatus::Accepted);
  assert(checked.steps().size()==1&&checked.steps()[0].restriction);
  assert(checked.admit(p,UINT64_MAX).status==RetentionStatus::Accepted);
  assert(!checked.steps()[1].restriction);
  RetainedClauses permanent(limits());
  assert(permanent.admit(h,1,p,UINT64_MAX).status==RetentionStatus::NotRup);
  assert(checked.admit(restriction(),UINT64_MAX).status==RetentionStatus::AlreadyPresent);
  QueryHistory incompatible;base(incompatible);
  assert(incompatible.assume(-3)==InputStatus::Ok&&incompatible.begin_query()==InputStatus::Ok);
  RestrictionKnowledge rejected(incompatible,*incompatible.active_query(),limits());
  assert(rejected.admit(restriction(),UINT64_MAX).status==RetentionStatus::NotRup);
  assert(rejected.steps().empty()&&rejected.admit(p,UINT64_MAX).status==RetentionStatus::NotRup);
  assert(rejected.admit(restriction(),1).status==RetentionStatus::Exhausted);
  volatile sig_atomic_t stop=1;
  assert(rejected.admit(restriction(),UINT64_MAX,&stop).status==RetentionStatus::Interrupted);
  QueryHistory appended;base(appended);clause(appended,{-3});assert(appended.begin_query()==InputStatus::Ok);
  RestrictionKnowledge changed(appended,*appended.active_query(),limits());
  assert(changed.admit(restriction(),UINT64_MAX).status==RetentionStatus::NotRup&&changed.steps().empty());
  auto bad=restriction();bad.witness={-2,-3};
  RestrictionKnowledge fresh(h,*h.active_query(),limits());
  assert(fresh.admit(bad,UINT64_MAX).status==RetentionStatus::NotRup);
  bad=restriction();bad.mappings={{2,3}};assert(fresh.admit(bad,UINT64_MAX).status==RetentionStatus::Invalid);
  bad=restriction();bad.witness.push_back(1);assert(fresh.admit(bad,UINT64_MAX).status==RetentionStatus::Invalid);
  bad=restriction();bad.mappings={{5,1}};assert(fresh.admit(bad,UINT64_MAX).status==RetentionStatus::Invalid);
  auto small=limits();small.maximum_literals=2;
  RestrictionKnowledge bounded(h,*h.active_query(),small);
  assert(bounded.admit(restriction(),UINT64_MAX).status==RetentionStatus::Exhausted);
  assert(bounded.steps().empty());
  assert(fresh.admit(restriction(),UINT64_MAX).status==RetentionStatus::Accepted);
  RestrictionKnowledge measured(h,*h.active_query(),limits());
  const auto full=measured.admit(restriction(),UINT64_MAX);
  assert(full.status==RetentionStatus::Accepted&&full.ticks>1);
  unsigned exhausted=0;
  for(uint64_t budget=1;budget<full.ticks;++budget) {
    RestrictionKnowledge cut(h,*h.active_query(),limits());
    const auto result=cut.admit(restriction(),budget);
    // BCP can finish its last atomic cascade beyond a finite allowance.
    if(result.status==RetentionStatus::Accepted)continue;
    assert(result.status==RetentionStatus::Exhausted);++exhausted;
    assert(cut.steps().empty()&&cut.admit(p,UINT64_MAX).status==RetentionStatus::NotRup);
    assert(cut.admit(restriction(),UINT64_MAX).status==RetentionStatus::Accepted);
    assert(cut.admit(p,UINT64_MAX).status==RetentionStatus::Accepted);
  }
  assert(exhausted);
  QueryHistory mapped;base(mapped);assert(mapped.begin_query()==InputStatus::Ok);
  RestrictionKnowledge normalized(mapped,*mapped.active_query(),limits());
  DerivationRecord swap;swap.kind=DerivationKind::SubstitutionRedundancy;swap.clause={1};
  swap.mappings={{-1,-2},{2,1},{3,4},{4,3}};
  assert(normalized.admit(swap,UINT64_MAX).status==RetentionStatus::Accepted);
  assert(normalized.steps()[0].restriction&&normalized.steps()[0].mappings[0]==std::make_pair(1,2));
  assert(h.finish_query(1,QueryResult::Unknown)==InputStatus::Ok);
  assert(fresh.admit(restriction(),UINT64_MAX).status==RetentionStatus::Stale);
}
static bool holds(const std::vector<int32_t> &c,unsigned mask) {
  for(int32_t l:c)if(bool(mask&(1u<<(external_variable_of(l)-1)))==(l>0))return true;
  return false;
}
static void exhaustive_two_variables() {
  unsigned accepted=0,repaired=0,mapped=0;
  const std::vector<std::vector<int32_t>> clauses{{1},{-1},{2},{-2},{1,2},{1,-2},{-1,2},{-1,-2}};
  const std::vector<std::vector<int32_t>> cubes{{},{1},{-1},{2},{-2},{1,2},{1,-2},{-1,2},{-1,-2}};
  for(unsigned selection=0;selection<(1u<<clauses.size());++selection)
    for(int32_t pivot:{1,-1})for(unsigned xmap=0;xmap<3;++xmap)for(unsigned ymap=0;ymap<5;++ymap)
      for(const auto &cube:cubes) {
        QueryHistory h;clause(h,{2,-2}); // declare both coordinates
        for(unsigned i=0;i<clauses.size();++i)if(selection&(1u<<i)) {
          for(int32_t l:clauses[i])assert(h.add(l)==InputStatus::Ok);
          assert(h.add(0)==InputStatus::Ok);
        }
        for(int32_t l:cube)assert(h.assume(l)==InputStatus::Ok);
        assert(h.begin_query()==InputStatus::Ok);
        DerivationRecord r;r.kind=DerivationKind::SubstitutionRedundancy;r.clause={pivot};
        if(xmap)r.mappings.push_back({1,xmap==1?2:-2});
        if(ymap==1||ymap==2)r.witness={ymap==1?2:-2};
        if(ymap>=3)r.mappings.push_back({2,ymap==3?1:-1});
        RestrictionKnowledge knowledge(h,*h.active_query(),limits());
        const auto result=knowledge.admit(r,UINT64_MAX);
        if(result.status!=RetentionStatus::Accepted)continue;
        ++accepted;
        for(unsigned mask=0;mask<4;++mask) {
          bool model=true;
          for(int32_t l:cube)model&=bool(mask&(1u<<(external_variable_of(l)-1)))==(l>0);
          for(unsigned i=0;i<clauses.size();++i)if(selection&(1u<<i))model&=holds(clauses[i],mask);
          if(!model||holds(r.clause,mask))continue;
          ++repaired;
          if(xmap||ymap>=3)++mapped;
          const bool x=xmap ? (bool(mask&2)==(xmap==1)) : pivot>0;
          const bool y=ymap==0?bool(mask&2):ymap==1?true:ymap==2?false:ymap==3?bool(mask&1):!bool(mask&1);
          const unsigned repaired=unsigned(x)+2*unsigned(y);
          assert(holds(r.clause,repaired));
          for(int32_t l:cube)assert(bool(repaired&(1u<<(external_variable_of(l)-1)))==(l>0));
          for(unsigned i=0;i<clauses.size();++i)if(selection&(1u<<i))assert(holds(clauses[i],repaired));
        }
      }
  assert(accepted&&repaired&&mapped); // semantic checks must reach real repairs
}
int main() {ownership_and_failures();exhaustive_two_variables();}
