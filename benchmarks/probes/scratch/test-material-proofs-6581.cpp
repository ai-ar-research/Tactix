// Exhaustive fixed 3/4-variable C2 controls; no search or benchmark execution.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/strengthen/strengthen.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
using namespace tactix;
static bool holds(int lit,unsigned mask) {
  const bool value=(mask>>(std::abs(lit)-1))&1;
  return lit>0 ? value : !value;
}
int main(int argc,char **argv) {
  assert(argc==8);
  const unsigned mode=std::atoi(argv[1]), n=std::atoi(argv[2]);
  const unsigned missing=std::atoi(argv[3]), style=std::atoi(argv[4]);
  assert(mode<2 && (n==3 || n==4) && missing<2 && style<3);
  Kernel k; kernel_reserve_vars(k,n);
  std::vector<std::vector<int>> input;
  std::ofstream cnf(argv[5]); assert(cnf);
  cnf<<"p cnf "<<n<<' '<<(1u<<n)-missing<<'\n';
  for (unsigned signs=0; signs<(1u<<n)-missing; ++signs) {
    std::vector<Lit> lits; std::vector<int> row;
    for (unsigned v=0; v<n; ++v) {
      const int lit=(signs&(1u<<v)) ? -int(v+1) : int(v+1);
      row.push_back(lit); lits.push_back(internalize(lit)); cnf<<lit<<' ';
    }
    cnf<<"0\n"; input.push_back(row);
    kernel_add_large_clause(k,lits.data(),lits.size(),false,0,++k.next_clause_id);
  }
  cnf.close();
  unsigned before=0;
  for (unsigned mask=0;mask<(1u<<n);++mask) {
    bool model=true;
    for (const auto &row:input) {
      bool satisfied=false; for (int lit:row) satisfied|=holds(lit,mask);
      model&=satisfied;
    }
    if (model) before|=1u<<mask;
  }
  assert(bool(before)==bool(missing));
  Proof proof; assert(proof.open(argv[6],mode?ProofFormat::Lrat:ProofFormat::DratAscii));
  k.proof=&proof; proof.bind_variable_map(&k.internal_to_external);
  if (style==1) { k.midround_budget=true; k.work_budget=1; }
  unsigned rounds=0;
  const auto &rule=tactix_rules()[tactix_rule_id("Strengthen")];
  while (!k.inconsistent && rounds++<100) {
    // Force one partial scan, then test its retained work with a completing
    // allowance. A permanently one-step allowance can revisit the same
    // overlapping prefix forever; policy normally gives a fresh larger dose.
    if (style==1 && rounds==2) { k.midround_budget=false; k.work_budget=0; }
    const uint64_t changed=k.stats.strengthened+k.stats.strengthen_subsumed;
    if (style==2) {
      // Empty advice must not hide any explicit instance. Re-enumerate after
      // each mutation because arena refs are not stable across collection.
      kernel_clear_subsume_material(k);
      std::vector<Ref> candidates; strengthen_candidates(k,candidates,true);
      for (Ref ref:candidates) {
        const auto result=rule.apply_instance(k,ref);
        if (result!=ApplyResult::NoOp) break;
      }
      if (!k.inconsistent && changed==k.stats.strengthened+k.stats.strengthen_subsumed) {
        // Explicit C2 instances act on large candidates, not the canonical
        // binary prepass. Finish any residual binary contradiction through
        // that actual rule service; public actions need not be a solver.
        k.touch_filter=false;
        rule.apply(k);
        break;
      }
    } else {
      rule.apply(k);
      if (!k.inconsistent && k.strengthen_complete) break;
    }
  }
  assert(rounds<100 && k.stats.strengthened>0);
  if (!missing) assert(k.inconsistent);
  else {
    assert(!k.inconsistent); unsigned after=0;
    for (unsigned mask=0;mask<(1u<<n);++mask) {
      bool model=true;
      for (Lit lit=0;lit<2*n;++lit) {
        if (k.value(lit)>0) model&=holds(kernel_externalize(k,lit),mask);
        for (const WatchEntry w:k.watches.entries(lit)) if (w.binary())
          model&=holds(kernel_externalize(k,lit),mask)||holds(kernel_externalize(k,w.other()),mask);
      }
      for (Ref ref=0;ref<k.arena.size();) {
        const Clause *c=clause_at(k.arena,ref);
        if (!c->garbage()) {
          bool satisfied=false;
          for (Lit lit:*c) satisfied|=holds(kernel_externalize(k,lit),mask);
          model&=satisfied;
        }
        ref+=clause_actual_words(c);
      }
      if (model) after|=1u<<mask;
    }
    assert(after==before);
  }
  proof.flush(); k.proof=nullptr;
  std::ofstream evidence(argv[7]);
  evidence<<"{\"rounds\":"<<rounds<<",\"strengthened\":"<<k.stats.strengthened
          <<",\"budget_stops\":"<<k.stats.budget_stops_c2<<",\"models\":"<<before<<"}\n";
  std::printf("PASS mode=%u n=%u missing=%u style=%u rounds=%u\n",mode,n,missing,style,rounds);
}
