#undef NDEBUG
#include "query_test_support.hpp"
#include "temporary_file.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include <string>
#include <cstdio>
using namespace tactix;
using namespace tactix::session;
static void fixture(const std::string &prefix, const std::string &mode) {
  QueryHistory h;
  query_test::clause(h,{1,2}); query_test::clause(h,{-1,3});
  for (const auto &clause : {std::vector<int32_t>{4,5},{-4,5},{4,-5},{-4,-5}}) {
    for (int32_t l:clause) assert(h.add(l)==InputStatus::Ok);
    assert(h.add(0)==InputStatus::Ok);
  }
  assert(h.begin_query()==InputStatus::Ok);
  QueryKernel s(query_test::small_limits());
  assert(s.replay(h)==ReplayStatus::Ok);
  Kernel &k=*s.kernel(s.ticket());
  const auto &bve=tactix_rules()[tactix_rule_id("BVE")];
  assert(bve.apply_instance(k,lit_instance(k,kernel_internalize(k,1)))==ApplyResult::Ok);
  assert(h.finish_query(1,QueryResult::Unknown)==InputStatus::Ok);
  assert(h.assume(2)==InputStatus::Ok && h.begin_query()==InputStatus::Ok);
  query_test::export_query(h, prefix);
  auto p = query_test::writer(prefix, mode);
  const auto admitted=s.advance(h,std::move(p));
  assert(admitted.status==NativeAdvanceStatus::Ok && admitted.restored==0);
  SubstitutionRedundancyInstance sr;
  sr.clause={kernel_internalize(k,-3)};
  sr.witness={kernel_internalize(k,2),kernel_internalize(k,-3)};
  assert(substitution_redundancy_apply_detailed(k,sr).rule_result==ApplyResult::Ok);
  assert(!s.projection_state(s.ticket()).restore);
  assert(query_test::drive(s,h,false)==20);
  assert(s.flush_proof(s.ticket()));
  std::printf("result=20 conflicts=%llu decisions=%llu rho=%zu\n",(unsigned long long)k.stats.conflicts,(unsigned long long)k.stats.decisions,k.reconstruction.size());
}

int main(int argc, char **argv) {
  if (argc == 3) { fixture(argv[1], argv[2]); return 0; }
  assert(argc == 1);
  TemporaryFile input;
  fixture(input.path, "none");
  assert(std::remove((input.path+".cnf").c_str()) == 0);
}
