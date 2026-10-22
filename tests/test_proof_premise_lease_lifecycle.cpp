#undef NDEBUG
#include "proof/proof.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <unistd.h>
using namespace tactix;
namespace {
struct Observer final:PopulationObserver {
  uint64_t events=0,adds=0;
  CheckerClauseRole last_add_role=CheckerClauseRole::None;
  void observe(const CheckerPopulationEvent &event) noexcept override {
   ++events;
   if(event.kind==CheckerPopulationEventKind::Add){++adds;last_add_role=event.role;}
  }
};
std::string contents(const std::string &path){
 FILE *f=std::fopen(path.c_str(),"rb");assert(f);std::string bytes;char buffer[256];
 while(size_t n=std::fread(buffer,1,sizeof(buffer),f))bytes.append(buffer,n);
 assert(!std::ferror(f));std::fclose(f);return bytes;
}
void shared_copies(ProofFormat format){
 const std::string path="/tmp/tactix-lease-copies-"+std::to_string(getpid())+"-"+std::to_string(unsigned(format));
 Proof p;assert(p.open(path.c_str(),format));Observer observer;p.bind_population_observer(&observer);
 const Lit native[]={internalize(1),internalize(-2),internalize(3)};
 const int32_t external[]={3,1,-2};
 p.seed_population(1,native,3);p.seed_population(2,native,3);p.arm_population_shadow();
 std::vector<uint64_t> handles;
 assert(p.retain_premise_groups({{{1,-2,3}},{{3,1,-2}}},8,100,100000,[](uint64_t){return true;},handles));
 assert(handles.size()==2 && p.premise_literals()==6);
 p.flush();const auto initial=contents(path);const auto events=observer.events;
 p.del(3,1,native,3);assert(p.del_external(3,2,external,3));p.flush();
 assert(contents(path)==initial && observer.events==events && (!p.population_shadow_available() || p.population_contains(1,native,3)));
 assert(p.premise_deletions_deferred()==2 && p.premise_maintenance_work()>0);
 const auto bytes=p.premise_bytes(),epoch=p.premise_epoch();
 std::vector<uint64_t> untouched{999};
 assert(!p.retain_premise_groups({{}},8,100,100000,[](uint64_t){return true;},untouched));
 assert(untouched==std::vector<uint64_t>{999} && p.premise_bytes()==bytes && p.premise_epoch()==epoch);
 assert(!p.retain_premise_groups({{{4,5,6}}},8,100,1,[](uint64_t){return true;},untouched));
 assert(!p.retain_premise_groups({{{4,5,6}}},8,100,100000,[](uint64_t){return false;},untouched));
 assert(p.premise_bytes()==bytes && p.premise_literals()==6 && observer.events==events);
 p.release_premise_groups({handles[0]},4);p.flush();
 assert(contents(path)==initial && p.premise_group_active(handles[1]));
 assert(p.premise_literals()==3 && (!p.population_shadow_available() || p.population_contains(1,native,3)));
 p.release_premise_groups({handles[1]},5);p.flush();
 assert(!p.premise_bytes() && !p.premise_literals() && (!p.population_shadow_available() || !p.population_contains(1,native,3)));
 assert(p.premise_deletions_deferred()==2 && !p.unbacked_deletions());
 assert(observer.events>events && contents(path)!=initial);
 const auto final=contents(path);p.release_premise_groups(handles,6);p.flush();assert(contents(path)==final);
 p.bind_population_observer(nullptr);std::remove(path.c_str());
}
void pivot_retirement(ProofFormat format,bool external_rat){
 const std::string path="/tmp/tactix-lease-pivot-"+std::to_string(getpid())+"-"+std::to_string(unsigned(format))+(external_rat?"-external":"-native");
 Proof p;assert(p.open(path.c_str(),format));
 const Lit source[]={internalize(-1),internalize(2)};
 const Lit unrelated[]={internalize(3),internalize(4),internalize(5)};
 p.seed_population(1,source,2);p.seed_population(2,unrelated,3);p.arm_population_shadow();
 std::vector<uint64_t> handles;
 assert(p.retain_premise_groups({{{-1,2}},{{3,4,5}}},8,100,100000,[](uint64_t){return true;},handles));
 p.del(3,1,source,2);
 assert(p.premise_group_active(handles[0]) && (!p.population_shadow_available() || p.population_contains(1,source,2)));
 // [1] is vacuous RAT only after the native-deleted [-1,2] checker copy
 // disappears. Keeping that copy creates the unprovable resolvent [2].
 if(external_rat){const int32_t unit=1;assert(p.add_external_rat(4,&unit,1));}
 else{const Lit unit=internalize(1);p.add_rat(4,&unit,1,nullptr,0);}
 assert(!p.premise_group_active(handles[0]) && p.premise_group_active(handles[1]));
 assert((!p.population_shadow_available() || !p.population_contains(1,source,2)) && (!p.population_shadow_available() || p.population_contains(2,unrelated,3)));
 p.release_premise_groups(handles,5);p.flush();assert(!p.premise_bytes() && !p.unbacked_deletions());
 if(format==ProofFormat::DsrAscii || format==ProofFormat::DratAscii){
  const auto stream=contents(path);const auto deletion=stream.find("d -1 2 0\n");
  const auto addition=stream.find("1 0\n",deletion+9);
  assert(deletion!=std::string::npos && addition!=std::string::npos && deletion<addition);
 }
 std::remove(path.c_str());
}
void bounded_deletions(ProofFormat format){
 const std::string path="/tmp/tactix-lease-bounds-"+std::to_string(getpid())+"-"+std::to_string(unsigned(format));
 for(bool wide:{false,true}){
  Proof p;assert(p.open(path.c_str(),format));Observer observer;p.bind_population_observer(&observer);
  const Lit body[]={internalize(1),internalize(-2),internalize(3)};
  const Lit unrelated[]={internalize(4),internalize(5)};
  for(uint64_t id=1;id<=3;++id)p.seed_population(id,body,3);
  p.seed_population(4,unrelated,2);p.arm_population_shadow();
  std::vector<uint64_t> handles;
  const Proof::DeferredDeletionLimits limits{2,wide?4u:100u};
  assert(p.retain_premise_groups({{{1,-2,3}},{{3,1,-2}},{{4,5}}},8,100,100000,
      [](uint64_t){return true;},handles,limits));
  const auto events=observer.events;
  p.del(5,1,body,3);
  assert(p.premise_pending_deletions()==1 && p.premise_pending_literal_work()==4);
  const auto epoch=p.premise_epoch(),bytes=p.premise_bytes();
  // New helper reservations include outstanding physical deletions and cannot
  // bypass a stricter shared cap by offering a larger incoming allowance.
  const Proof::PremiseGroup two_helpers{{6,7},{-6,-7}};
  assert(!p.can_defer_premise_group_deletions({two_helpers},{1000,1000},
      [](uint64_t){return true;}));
  assert(p.can_defer_premise_group_deletions({{{6,7}}},{1000,1000},
      [](uint64_t){return true;})==!wide);
  assert(!p.can_defer_premise_group_deletions({{{6,7}}},{1000,1000},
      [](uint64_t){return false;}));
  assert(p.premise_epoch()==epoch && p.premise_bytes()==bytes &&
      p.premise_pending_deletions()==1 && p.premise_pending_literal_work()==4);
  std::vector<uint64_t> rejected{999};
  assert(!p.retain_premise_groups({{{6,7}}},8,100,100000,
      [](uint64_t){return true;},rejected,{1,3}));
  assert(rejected==std::vector<uint64_t>{999} && p.premise_epoch()==epoch && p.premise_bytes()==bytes);
  const int32_t external[]={3,1,-2};
  assert(p.del_external(6,2,external,3));
  if(!wide){
   assert(p.premise_pending_deletions()==2 && p.premise_pending_literal_work()==8);
   assert(observer.events==events);
   p.del(7,3,body,3);
  }
  assert(!p.premise_group_active(handles[0]) && !p.premise_group_active(handles[1]));
  assert(p.premise_group_active(handles[2]));
  assert(!p.premise_pending_deletions() && !p.premise_pending_literal_work());
  assert(p.premise_deletions_deferred()==(wide?1u:2u));
  assert(observer.events==events+(wide?2u:3u) && !p.unbacked_deletions());
  p.release_premise_groups(handles,8);assert(!p.premise_bytes());
  p.bind_population_observer(nullptr);p.flush();
 }
 std::remove(path.c_str());
}
void witnessless_rup(ProofFormat format,const char *prefix){
 const std::string stem=prefix?std::string(prefix)+"-rup-"+std::to_string(unsigned(format)):
     "/tmp/tactix-lease-rup-"+std::to_string(getpid())+"-"+std::to_string(unsigned(format));
 const std::string cnf=stem+".cnf",path=stem+".proof";
 FILE *input=std::fopen(cnf.c_str(),"wb");assert(input);
 assert(std::fputs("p cnf 4 2\n1 2 0\n3 4 0\n",input)>=0);assert(!std::fclose(input));
 Proof p;assert(p.open(path.c_str(),format));Observer observer;p.bind_population_observer(&observer);
 const Lit body[]={internalize(1),internalize(2)},other[]={internalize(3),internalize(4)};
 p.seed_population(1,body,2);p.seed_population(2,other,2);p.arm_population_shadow();
 std::vector<uint64_t> handles;
 assert(p.retain_premise_groups({{{1,2}},{{3,4}}},8,100,100000,
     [](uint64_t){return true;},handles,{4,100}));
 p.del(3,1,body,2);p.del(4,2,other,2);
 const auto epoch=p.premise_epoch(),pending=p.premise_pending_deletions();
 const auto literal_work=p.premise_pending_literal_work(),events=observer.events,adds=observer.adds;
 const DsrLiteralMapping identity{internalize(1),internalize(1)};const Lit metadata=internalize(3);
 assert(plan_dsr_witness(body,2,&metadata,1,&identity,1).form==DsrWitnessForm::Rup);
 if(format==ProofFormat::DsrAscii || format==ProofFormat::DsrBinary)
  assert(p.add_sr(5,body,2,&metadata,1,&identity,1,4));
 else p.add_rup(5,body,2,nullptr,0);
 assert(p.premise_epoch()==epoch && p.premise_pending_deletions()==pending);
 assert(p.premise_pending_literal_work()==literal_work && pending==2);
 assert(p.premise_group_active(handles[0]) && p.premise_group_active(handles[1]));
 assert(observer.events==events+1 && observer.adds==adds+1 && observer.last_add_role==CheckerClauseRole::Rup);
 assert(!p.population_shadow_available() || (p.population_contains(1,body,2) && p.population_contains(2,other,2)));
 p.release_premise_groups(handles,6);p.flush();assert(!p.unbacked_deletions());
 p.bind_population_observer(nullptr);
 if(!prefix){std::remove(path.c_str());std::remove(cnf.c_str());}
}
void format_boundaries(){
 const std::string path="/tmp/tactix-lease-routes-"+std::to_string(getpid());
 {Proof p;assert(p.open(path.c_str(),ProofFormat::Lrat));std::vector<uint64_t> handles{77};
  assert(!p.retain_premise_groups({{{1,2}}},8,100,100000,[](uint64_t){return true;},handles));
  assert(handles==std::vector<uint64_t>{77} && !p.premise_bytes());}
 {Proof p;assert(p.open(path.c_str(),ProofFormat::PbpStreaming));p.finish_input_transaction();
  assert(p.begin_pb() && p.pb());std::vector<uint64_t> handles{77};
  assert(!p.retain_premise_groups({{{1,2}}},8,100,100000,[](uint64_t){return true;},handles));
  const int32_t source[]={1,2};assert(!p.del_external(1,0,source,2));
  assert(handles==std::vector<uint64_t>{77} && !p.premise_bytes());}
 {Proof p;assert(p.open(path.c_str(),ProofFormat::DsrBinary));
  const Lit source[]={internalize(1),internalize(2)};p.seed_population(1,source,2);p.arm_population_shadow();
  std::vector<uint64_t> handles;
  assert(p.retain_premise_groups({{{1,2}}},8,100,100000,[](uint64_t){return true;},handles));
  p.del(2,1,source,2);
  assert(p.dsr());p.release_premise_groups(handles,3);p.flush();assert(!p.unbacked_deletions());}
 std::remove(path.c_str());
}
}
int main(int argc,char **argv){
 for(auto f:{ProofFormat::DsrAscii,ProofFormat::DsrBinary,ProofFormat::DratAscii,ProofFormat::DratBinary}){
  witnessless_rup(f,argc>1?argv[1]:nullptr);bounded_deletions(f);shared_copies(f);pivot_retirement(f,false);pivot_retirement(f,true);
 }
 format_boundaries();
}
