#undef NDEBUG
#include "proof/proof.hpp"
#include "temporary_file.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <sstream>
#include <string>
using namespace tactix;

struct Observer final : PopulationObserver {
  struct Event { bool deletion; std::vector<int32_t> named; };
  std::vector<Event> events;
  unsigned helpers_added=0, helpers_deleted=0;
  uint32_t maximum=0;
  int32_t third=0, fourth=0;
  void observe(const CheckerPopulationEvent &event) noexcept override {
    if (event.kind!=CheckerPopulationEventKind::Add &&
        event.kind!=CheckerPopulationEventKind::DeleteRequested) return;
    const bool deletion=event.kind==CheckerPopulationEventKind::DeleteRequested;
    std::vector<int32_t> named;
    for (uint32_t i=0;i<event.clause.size();++i) {
      const int32_t literal=event.checker_literal_at(i);
      named.push_back(literal);maximum=std::max(maximum,external_variable_of(literal));
    }
    events.push_back({deletion,std::move(named)});
    if (event.role==CheckerClauseRole::DsrSubstitutionHelper) {
      assert(event.clause_is_named && event.proof_only);
      deletion ? ++helpers_deleted : ++helpers_added;
    }
    if (!deletion && !event.clause_is_named && event.clause.size()==1) {
      if (event.clause.at(0)==3) third=event.checker_literal_at(0);
      if (event.clause.at(0)==4) fourth=event.checker_literal_at(0);
    }
  }
};

struct UnlicensedSources final : QueryProofSources {
  bool snapshot(std::vector<QueryProofOwnedClause> &sources,
      std::vector<int32_t> &,uint32_t &extent) const override {
    sources.push_back({99,{3}}); extent=3; return true;
  }
};

static void fixture(const std::string &prefix, const std::string &mode) {
  const std::vector<QueryProofOwnedClause> original{
      {1,{1,2}},{2,{-1,2}},{3,{1,-2}},{4,{-1,-2}}};
  FILE *cnf=std::fopen((prefix+".cnf").c_str(),"w"); assert(cnf);
  std::fputs("p cnf 2 4\n1 2 0\n-1 2 0\n1 -2 0\n-1 -2 0\n",cnf);assert(std::fclose(cnf)==0);
  Observer observer;
  Proof proof;
  if (mode=="auto") assert(proof.open_automatic((prefix+".proof").c_str(),false));
  else if (mode=="pbp") assert(proof.open((prefix+".proof").c_str(),ProofFormat::PbpStreaming));
  else assert(proof.open_dsr_writer((prefix+".proof").c_str(),ProofFormat::DsrAscii));
  std::vector<Var> map{0,1}; proof.bind_variable_map(&map);
  proof.bind_population_observer(&observer);
  UnlicensedSources unlicensed;
  if(mode=="unlicensed") proof.enable_query_certificates(unlicensed,{8,64,256,1000000});
  std::vector<QueryProofClause> sources;
  for (const auto &source:original) {
    std::vector<Lit> body;for(int32_t l:source.literals)body.push_back(internalize(l));
    proof.seed_population(source.id,body.data(),body.size());
    sources.push_back({source.id,source.literals});
  }
  proof.finish_input_transaction();
  if(mode=="unlicensed") {
    const Lit removed[]{internalize(1),internalize(2)};
    proof.del(20,1,removed,2);map.push_back(2);
    const Lit candidate=internalize(3);proof.add_rat(21,&candidate,1);
    assert(false && "unlicensed native snapshot must not be copied");
  }
  assert(proof.isolate_query(sources,{},2));
  map.push_back(2);
  Lit fresh=internalize(3);assert(proof.add_sr(5,&fresh,1,nullptr,0,nullptr,0,3,false));
  map.push_back(3);
  fresh=internalize(4);assert(proof.add_sr(6,&fresh,1,nullptr,0,nullptr,0,4,false));
  PbpParityBoundsCache cache;
  const auto parity_reason = [&](uint64_t id,uint32_t bound) {
    const PbpParityEquation equation{{1,2},false};
    const int32_t reason[]{-1,2};PbpParityReasonPlan plan;
    assert(proof.plan_pbp_parity_reason({equation},reason,2,bound,cache,plan));
    assert(proof.add_prevalidated_pbp_parity_reason(id,{equation},reason,2,cache,plan));
  };
  if(mode=="pbp" || mode=="auto") parity_reason(9,4);
  std::vector<QueryProofOwnedClause> current=original;
  current.push_back({5,{3}});current.push_back({6,{4}});
  sources.clear();for(const auto &source:current)sources.push_back({source.id,source.literals});
  const uint32_t old_maximum=observer.maximum;
  const size_t old_events=observer.events.size();
  assert(proof.isolate_query(sources,{},4));
  for(size_t i=old_events;i<observer.events.size();++i)
    for(int32_t literal:observer.events[i].named)
      assert(external_variable_of(literal)>old_maximum);
  if(mode=="pbp" || mode=="auto") parity_reason(10,5);
  const Lit one=internalize(1);proof.add_rup(7,&one,1);proof.add_empty(8);proof.flush();
  assert(observer.third && observer.fourth);
  if(mode=="dsr") {
    assert(observer.fourth>observer.third+1); // helpers occupy the gap
    assert(observer.helpers_added==60 && observer.helpers_deleted==60);
    // Every serialized addition/deletion, including each proof-only helper,
    // has one matching callback with exact physical names and byte order.
    std::ifstream input(prefix+".proof");std::string line;size_t at=0;
    while(std::getline(input,line)) {
      std::istringstream words(line);std::string first;assert(words>>first);
      const bool deletion=first=="d";std::vector<int32_t> body;
      int32_t literal=deletion ? 0 : std::stoi(first);
      if(deletion) assert(words>>literal);
      if(literal) body.push_back(literal);
      while(literal && words>>literal && literal) {
        if(!deletion && literal==body.front()) break; // witness begins
        body.push_back(literal);
      }
      assert(at<observer.events.size());
      assert(observer.events[at].deletion==deletion && observer.events[at].named==body);++at;
    }
    assert(at==observer.events.size());
  }
  proof.bind_population_observer(nullptr);
}

int main(int argc,char **argv) {
  if(argc==3) { fixture(argv[1],argv[2]);return 0; }
  assert(argc==1);TemporaryFile file;fixture(file.path,"dsr");
  assert(std::remove((file.path+".cnf").c_str())==0);
  assert(std::remove((file.path+".proof").c_str())==0);
}
