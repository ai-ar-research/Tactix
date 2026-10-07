#undef NDEBUG
#include "rules/semantic_extension/affine_root_image.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
using namespace tactix;
using namespace tactix::affine_semantic_extension;
static RootImageLimits limits() {
  RootImageLimits l;
  l.image = {100,100,100,100000,8,16,32,4};
  l.maximum_sources=100;l.maximum_literals=1000;l.maximum_units=16;
  l.maximum_work=1000000;l.maximum_bytes=1000000;l.maximum_proof_additions=100;
  return l;
}
static std::vector<AffineLiveSource> fixture(int32_t guard, std::vector<int32_t> vars) {
  std::vector<AffineLiveSource> sources;
  std::vector<int32_t> positive;
  bool rhs=false;
  for(int32_t literal:vars){positive.push_back(literal<0?-literal:literal);rhs^=literal<0;}
  std::sort(positive.begin(),positive.end());
  for(auto clause:clausal_form(positive,rhs)){
    clause.push_back(-guard);
    AffineLiveSource source;source.occurrence=sources.size()+1;
    source.clause_id=100+source.occurrence;source.clause=clause;
    sources.push_back(source);
  }
  AffineLiveSource unit;unit.kind=AffineSourceKind::CheckerPremise;
  unit.occurrence=sources.size()+1;unit.clause_id=100+unit.occurrence;
  unit.clause={guard};sources.push_back(unit);return sources;
}
static void controls(int32_t guard,const std::vector<int32_t> &vars) {
  auto sources=fixture(guard,vars);RootImagePlan plan;
  const auto charge=[](uint64_t){return true;};
  assert(prepare_affine_root_image(sources,{guard},limits(),charge,plan)==RootImageStatus::Prepared);
  assert(plan.image.equations.size()==1 && plan.residuals.size()==4 && plan.rup_checks==5);
  assert(plan.certification_sources.size()==5 && plan.certified_root_facts==std::vector<int32_t>{guard});
  for(const auto &residual:plan.residuals){
    assert(residual.clause.size()==3 && residual.physical_source.clause.size()==4);
    assert(residual.physical_source.clause_id==100+residual.physical_source.occurrence);
    assert(residual.removed_root_facts==std::vector<int32_t>{guard});
  }
  const auto saved_work=plan.work;const auto saved_sources=plan.certification_sources.size();
  auto derived=sources;
  derived.back().clause={113};
  AffineLiveSource bridge;bridge.kind=AffineSourceKind::Binary;
  bridge.occurrence=derived.size()+1;bridge.clause_id=999;
  bridge.clause={-113,guard};derived.push_back(bridge);
  RootImagePlan derived_plan;
  assert(prepare_affine_root_image(derived,{guard},limits(),charge,derived_plan)==RootImageStatus::Prepared);
  assert(derived_plan.image.equations.size()>=1 && derived_plan.rup_checks==5);

  auto unrelated=sources;
  AffineLiveSource noise;noise.clause_id=2000;noise.clause={-guard,7,8,9};
  unrelated.insert(unrelated.begin(),noise);
  for(size_t i=0;i<unrelated.size();++i)unrelated[i].occurrence=i+1;
  RootImagePlan selected;
  assert(prepare_affine_root_image(unrelated,{guard},limits(),charge,selected)==RootImageStatus::Prepared);
  assert(selected.residuals.size()==4 && selected.rup_checks==5);
  for(size_t i=0;i<selected.residuals.size();++i){
    assert(selected.residuals[i].occurrence==i+1);
    assert(selected.residuals[i].physical_source.clause_id!=2000);
  }
  for(uint64_t occurrence:selected.image.equations.front().premises)
    assert(occurrence && occurrence<=selected.residuals.size());
  auto absent=sources;absent.pop_back();
  assert(prepare_affine_root_image(absent,{guard},limits(),charge,plan)==RootImageStatus::UncertifiedRoot);
  assert(prepare_affine_root_image(sources,{-guard},limits(),charge,plan)==RootImageStatus::UncertifiedRoot);
  auto missing=sources;missing.erase(missing.begin());
  assert(prepare_affine_root_image(missing,{guard},limits(),charge,plan)==RootImageStatus::NoStructure);
  assert(plan.work==saved_work && plan.certification_sources.size()==saved_sources);
  // A fact arriving later must be certified against the new immutable source
  // view; no fact from a prior failed attempt is retained.
  RootImagePlan late;
  assert(prepare_affine_root_image(absent,{guard},limits(),charge,late)==RootImageStatus::UncertifiedRoot);
  assert(prepare_affine_root_image(sources,{guard},limits(),charge,late)==RootImageStatus::Prepared);
  for(unsigned cap=0;cap<5;++cap){auto l=limits();
    if(cap==0)l.maximum_work=1;
    if(cap==1)l.maximum_bytes=1;
    if(cap==2)l.maximum_literals=1;
    if(cap==3)l.maximum_sources=1;
    if(cap==4)l.maximum_proof_additions=1;
    assert(prepare_affine_root_image(sources,{guard},l,charge,plan)==RootImageStatus::ResourceExhausted);
    assert(plan.work==saved_work && plan.certification_sources.size()==saved_sources);
  }
  uint64_t full_calls=0;
  RootImagePlan counted;
  assert(prepare_affine_root_image(sources,{guard},limits(),[&](uint64_t){++full_calls;return true;},counted)==RootImageStatus::Prepared);
  uint64_t late_calls=0;
  assert(prepare_affine_root_image(sources,{guard},limits(),[&](uint64_t){return ++late_calls<full_calls;},plan)==RootImageStatus::Interrupted);
  assert(plan.work==saved_work && plan.certification_sources.size()==saved_sources);
  uint64_t calls=0;
  assert(prepare_affine_root_image(sources,{guard},limits(),[&](uint64_t){return ++calls<25;},plan)==RootImageStatus::Interrupted);
  assert(plan.work==saved_work && plan.certification_sources.size()==saved_sources);
}
static void identity_controls() {
  auto sources=fixture(4,{1,2,3});
  const auto charge=[](uint64_t){return true;};
  RootImagePlan plan;
  auto &unit=sources.back();unit.kind=AffineSourceKind::RootUnit;
  unit.clause_id=0;unit.proof_id_exact=false;
  assert(prepare_affine_root_image(sources,{4},limits(),charge,plan)==RootImageStatus::Prepared);
  assert(plan.certification_sources.back().clause_id==0);
  assert(!plan.certification_sources.back().proof_id_exact);
  assert(plan.certification_sources.back().kind==AffineSourceKind::RootUnit);
  const auto saved_work=plan.work;
  auto malformed=sources;malformed.back().kind=AffineSourceKind::CheckerPremise;
  assert(prepare_affine_root_image(malformed,{4},limits(),charge,plan)==RootImageStatus::Invalid);
  malformed=sources;malformed.back().kind=AffineSourceKind::Large;
  assert(prepare_affine_root_image(malformed,{4},limits(),charge,plan)==RootImageStatus::Invalid);
  malformed=sources;malformed.back().proof_id_exact=true;
  assert(prepare_affine_root_image(malformed,{4},limits(),charge,plan)==RootImageStatus::Invalid);
  malformed=sources;malformed.back().clause_id=1;
  assert(prepare_affine_root_image(malformed,{4},limits(),charge,plan)==RootImageStatus::Invalid);
  malformed=sources;malformed.back().clause={4,5};
  assert(prepare_affine_root_image(malformed,{4},limits(),charge,plan)==RootImageStatus::Invalid);
  malformed=sources;malformed.back().kind=static_cast<AffineSourceKind>(255);
  assert(prepare_affine_root_image(malformed,{4},limits(),charge,plan)==RootImageStatus::Invalid);
  assert(plan.work==saved_work);
  AffineLiveSource binary;binary.kind=AffineSourceKind::Binary;
  binary.occurrence=sources.size()+1;binary.clause_id=0;
  binary.proof_id_exact=false;binary.clause={7,8};sources.push_back(binary);
  assert(prepare_affine_root_image(sources,{4},limits(),charge,plan)==RootImageStatus::Prepared);
  assert(plan.certification_sources.back().clause_id==0);
  assert(!plan.certification_sources.back().proof_id_exact);
  malformed=sources;malformed.back().proof_id_exact=true;
  assert(prepare_affine_root_image(malformed,{4},limits(),charge,plan)==RootImageStatus::Invalid);
  malformed=sources;malformed.back().clause={7};
  assert(prepare_affine_root_image(malformed,{4},limits(),charge,plan)==RootImageStatus::Invalid);
  malformed=sources;malformed.back().occurrence=1;
  assert(prepare_affine_root_image(malformed,{4},limits(),charge,plan)==RootImageStatus::Invalid);
  // Removing the authenticated unit cannot make an unproved request a fact.
  sources.erase(sources.end()-2);
  assert(prepare_affine_root_image(sources,{4},limits(),charge,plan)==RootImageStatus::UncertifiedRoot);
}

static void forty_equations() {
  std::vector<AffineLiveSource> sources;
  std::vector<int32_t> roots;
  for (int32_t row=0;row<40;++row) {
    const int32_t first=1+4*row,guard=first+3;
    roots.push_back(guard);
    for (auto source:fixture(guard,{first,first+1,first+2})) {
      source.occurrence=sources.size()+1;
      source.clause_id=1000+source.occurrence;
      sources.push_back(std::move(source));
    }
  }
  auto l=limits();l.image.maximum_clauses=1000;l.image.maximum_groups=1000;
  l.image.maximum_patterns=1000;l.maximum_sources=1000;l.maximum_literals=10000;
  l.maximum_units=100;l.maximum_proof_additions=1000;l.maximum_work=10000000;
  l.maximum_bytes=100000000; // Ordinary Default's existing retained-byte cap.
  RootImagePlan plan;
  const auto started=std::chrono::steady_clock::now();
  assert(prepare_affine_root_image(sources,roots,l,[](uint64_t){return true;},plan)==RootImageStatus::Prepared);
  const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now()-started).count();
  std::printf("root-image rows=40 sources=200 rup_checks=%llu work=%llu accounted_peak_bytes=%llu wall_nanos=%llu\n",
      (unsigned long long)plan.rup_checks,(unsigned long long)plan.work,
      (unsigned long long)plan.accounted_peak_bytes,(unsigned long long)elapsed);
  assert(plan.image.equations.size()==40 && plan.residuals.size()==160);
  assert(plan.image.consequences.empty() && plan.rup_checks==200);
  assert(plan.accounted_peak_bytes>0 && plan.accounted_peak_bytes<l.maximum_bytes);
  assert(plan.reserved_bytes==plan.accounted_peak_bytes);
  auto small=l;small.maximum_bytes=plan.accounted_peak_bytes-1;
  const auto old_work=plan.work;
  assert(prepare_affine_root_image(sources,roots,small,[](uint64_t){return true;},plan)==RootImageStatus::ResourceExhausted);
  assert(plan.work==old_work && plan.image.equations.size()==40);
}

static void census_controls() {
  const auto charge=[](uint64_t){return true;};
  auto sources=fixture(4,{1,2,3});
  RootImagePlan p; RootScopeCensus first;
  assert(prepare_affine_root_image(sources,{4},limits(),charge,p,nullptr,&first)==RootImageStatus::Prepared);
  assert(!first.empty());
  RootScopeCensus repeated; RootScopeEffort effort;
  assert(prepare_affine_root_image(sources,{4},limits(),charge,p,&first,&repeated,&effort)==RootImageStatus::NoStructure);
  assert(effort.recognized_groups==0 && effort.skipped_groups>0 && effort.recognition_work==0);
  assert(p.image.equations.size()==1);
  auto missing=sources;missing.erase(missing.begin());
  RootScopeCensus unresolved;
  assert(prepare_affine_root_image(missing,{4},limits(),charge,p,nullptr,&unresolved)==RootImageStatus::NoStructure);
  RootScopeCensus repaired;
  assert(prepare_affine_root_image(sources,{4},limits(),charge,p,&unresolved,&repaired)==RootImageStatus::Prepared);
  auto reordered=sources; std::reverse(reordered.begin(),reordered.end());
  for(auto &source:reordered) {source.occurrence+=100;std::reverse(source.clause.begin(),source.clause.end());}
  assert(prepare_affine_root_image(reordered,{4},limits(),charge,p,&first,&repeated)==RootImageStatus::NoStructure);
  auto duplicate=sources;
  auto extra=duplicate.front();extra.occurrence=900;extra.clause_id=900;
  duplicate.push_back(extra);
  RootScopeCensus multiplicity;
  assert(prepare_affine_root_image(duplicate,{4},limits(),charge,p,&first,&multiplicity)==RootImageStatus::Prepared);
  RootScopeCensus mixed;
  RootScopeRecord incomplete;
  for(const auto &record:first) if(record.scope.size()==3) incomplete=record;
  incomplete.normalized_bodies.push_back({1,2,3});
  mixed.push_back(incomplete);
  for(auto record:first) {record.admitted=true;mixed.push_back(std::move(record));}
  RootScopeEffort subset_effort;RootScopeCensus subset_completed;
  assert(prepare_affine_root_image(sources,{4},limits(),charge,p,&mixed,&subset_completed,&subset_effort)==RootImageStatus::NoStructure);
  assert(subset_effort.recognized_groups==0 && subset_effort.recognition_work==0);
  auto changed=fixture(4,{-1,2,3});
  for(auto &record:first) record.admitted=true;
  assert(prepare_affine_root_image(changed,{4},limits(),charge,p,&first,&repeated)==RootImageStatus::Prepared);
  const auto saved=repeated.size();
  assert(prepare_affine_root_image(sources,{4},limits(),[](uint64_t){return false;},p,&first,&repeated)==RootImageStatus::Interrupted);
  assert(repeated.size()==saved);
  auto tiny=limits();tiny.maximum_bytes=1;
  assert(prepare_affine_root_image(sources,{4},tiny,charge,p,&first,&repeated)==RootImageStatus::ResourceExhausted);
  assert(repeated.size()==saved);
}

static void oracle_only_controls() {
  auto sources=fixture(2,{3,4,5});
  sources.back().kind=AffineSourceKind::RootUnit;
  sources.back().clause={1};sources.back().recognition_candidate=false;
  AffineLiveSource bridge;bridge.kind=AffineSourceKind::Binary;
  bridge.occurrence=sources.size()+1;bridge.clause_id=106;bridge.clause={-1,2};
  bridge.recognition_candidate=false;sources.push_back(bridge);
  RootImagePlan plan;RootScopeCensus census;RootScopeEffort effort;
  const auto charge=[](uint64_t){return true;};
  assert(prepare_affine_root_image(sources,{2},limits(),charge,plan,nullptr,&census,&effort)==RootImageStatus::Prepared);
  assert(plan.image.equations.size()==1&&plan.image.equations[0].variables==std::vector<int32_t>({3,4,5}));
  assert(effort.recognized_groups==1&&census.size()==1&&census[0].scope==std::vector<int32_t>({3,4,5}));
  assert(plan.certification_sources.size()==6&&!plan.certification_sources.back().recognition_candidate);
  sources.pop_back();RootImagePlan declined;
  assert(prepare_affine_root_image(sources,{2},limits(),charge,declined)==RootImageStatus::UncertifiedRoot);
  assert(declined.image.equations.empty());
}

int main(int argc,char **argv){
  assert(argc==1 || argc==2);
  controls(4,{1,2,3});controls(-97,{-11,23,-61});
  forty_equations();
  identity_controls();
  census_controls();
  oracle_only_controls();
  if(argc==2){
    const auto sources=fixture(4,{1,2,3});RootImagePlan p;
    assert(prepare_affine_root_image(sources,{4},limits(),[](uint64_t){return true;},p)==RootImageStatus::Prepared);
    std::string stem=argv[1];std::ofstream cnf(stem+".cnf");
    cnf<<"p cnf 4 5\n";for(const auto&s:sources){for(int x:s.clause)cnf<<x<<' ';cnf<<"0\n";}
    // Nonrefuting prefixes against the satisfiable ORIGINAL input.
    std::ofstream dsr(stem+".dsr"),drat(stem+".drat");
    for(const auto&r:p.residuals){for(int x:r.clause){dsr<<x<<' ';drat<<x<<' ';}dsr<<"0\n";drat<<"0\n";}
    std::ofstream bad(stem+"-bad.drat");bad<<"1 0\n";
    std::ofstream model(stem+".model");model<<"s SATISFIABLE\nv -1 -2 -3 4 0\n";
  }
}
