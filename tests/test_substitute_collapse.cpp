// Retained binary-collapse endpoints receive native removal advice. Shared
// replacement, connected Sweep and surviving large rewrites stay unchanged.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/substitute/substitute.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <unistd.h>
using namespace tactix;
static bool check(unsigned copies, bool redundant, bool large, int mode,
                  bool explicit_action) {
  Kernel k; kernel_reserve_vars(k, 4);
  char path[]="substitute-collapse-proof-XXXXXX";
  const int fd=mkstemp(path); assert(fd>=0); close(fd);
  Proof proof;
  if (mode) {
    assert(proof.open(path,mode==1 ? ProofFormat::DratAscii : ProofFormat::Lrat));
    k.proof=&proof;
    proof.bind_variable_map(&k.internal_to_external);
  }
  kernel_add_binary_clause(k, internalize(-1), internalize(2), ++k.next_clause_id);
  kernel_add_binary_clause(k, internalize(1), internalize(-2), ++k.next_clause_id);
  const Lit source[] = {internalize(1), internalize(2), internalize(3), internalize(4)};
  for (unsigned i=0;i<copies;++i)
    kernel_add_large_clause(k, source, large ? 4 : 3, redundant,
                            redundant ? 2 : 0, ++k.next_clause_id);
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), 0);
  const auto before=k.stats.bve_marks;
  const auto &rule=tactix_rules()[tactix_rule_id("Substitute")];
  assert(rule.guard(k)); uint64_t payload[8]; assert(rule.instances(k,payload,8)>0);
  assert((explicit_action ? rule.apply_instance(k,payload[0]) : rule.apply(k))==ApplyResult::Ok);
  assert(kernel_var_retired(k,1));
  const bool retry=k.bve_retry[2];
  const auto marks=k.stats.bve_marks-before;
  bool ok=retry==(!redundant && !large);
  if (!redundant && !large) {
    assert(kernel_binary_live(k,internalize(1),internalize(3)));
    ok=ok && marks==4+3*copies;
  }
  assert(!k.bve_retry[3]);
  const auto after=k.stats.bve_marks;
  assert(rule.apply(k)==ApplyResult::NoOp && k.stats.bve_marks==after);
  k.level=1; assert(!rule.guard(k)); k.level=0;
  kernel_check_invariants(k);
  k.proof=nullptr; if(mode)proof.flush(); assert(std::remove(path)==0);
  assert(default_solve(k)==10); kernel_extend_model(k);
  const auto val=[&](int x){return k.value(internalize(x))>0;};
  assert((val(-1)||val(2)) && (val(1)||val(-2)));
  assert(val(1)||val(2)||val(3)||(large&&val(4)));
  return ok;
}
int main(int argc,char **argv) {
  unsigned missing=0;
  for(unsigned copies:{1u,2u,5u}) for(bool red:{false,true})
    for(bool large:{false,true}) for(int mode:{0,1,2})
      for(bool explicit_action:{false,true})
        missing+=!check(copies,red,large,mode,explicit_action);
  if(argc==2 && std::string(argv[1])=="--expect-old-missing")assert(missing==18);
  else assert(!missing);
  std::printf("PASS: 72 collapse/large/redundancy/public/proof/model fixtures; missing=%u\n",missing);
}
