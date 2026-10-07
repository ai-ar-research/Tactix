#undef NDEBUG
#include "rules/semantic_extension/direct_csp_guarded_cover_offer.hpp"
#include "frontend/parse.hpp"
#include <cassert>
#include <algorithm>
#include <map>
#include <csignal>
#include <sstream>
using namespace tactix;
namespace gc = tactix::direct_csp_guarded_cover;
using S = DirectCspGuardedCoverOfferStatus;

struct FormulaImage {
  size_t arena, trail; uint64_t id, generation, sequence;
  FormulaImage(const Kernel &k):arena(k.arena.size()),trail(k.trail.size()),
    id(k.next_clause_id),generation(k.arena_generation),sequence(k.structural_action_sequence){}
  bool operator==(const FormulaImage &)const=default;
};
DirectCspGuardedCoverOfferLimits limits() {
  return {{1000,10000,32,16,64,64,16,32},
          {1000000,100,100,100,100,100,8},
          10000000,1000,10000,1000000,1000,100000,100,8,1000,10000};
}
int32_t rename(int32_t x,bool signed_names) {
  if(!signed_names)return x;
  int32_t v=std::abs(x), renamed=2*v+1;
  return ((v%2)?-1:1)*(x<0?-1:1)*renamed;
}
gc::Selection fixture(bool signed_names=false) {
  gc::Selection s{{{1,2,3},{4,5}}, {0,0}, {8,9,10},
    {{6,{4,7}}}, {{8,0,0,0},{9,0,1,0},{10,0,2,0}}};
  for(auto &d:s.domains)for(auto &x:d)x=rename(x,signed_names);
  for(auto &x:s.cover)x=rename(x,signed_names);
  for(auto &o:s.exact_or){o.output=rename(o.output,signed_names);for(auto &x:o.inputs)x=rename(x,signed_names);}
  for(auto &b:s.blockers)b.literal=rename(b.literal,signed_names);
  return s;
}
void populate(Kernel &k,const gc::Selection &s,int omit=-1,bool duplicate=false,const std::vector<std::vector<int32_t>> &extra={}) {
  auto expected=gc::expected_clauses(s,limits().theory,[](uint64_t){return true;});
  assert(expected.status==gc::Status::Planned);
  std::vector<std::vector<int32_t>> clauses;
  for(size_t i=0;i<expected.clauses.size();++i)if(int(i)!=omit)clauses.push_back(expected.clauses[i]);
  if(duplicate)for(const auto &c:expected.clauses)if(c.size()>2){clauses.push_back(c);break;}
  clauses.insert(clauses.end(),extra.begin(),extra.end());
  std::ostringstream text;text<<"p cnf 30 "<<clauses.size()<<'\n';
  for(const auto &c:clauses){for(auto x:c)text<<x<<' ';text<<"0\n";}
  auto str=text.str();FILE *f=std::tmpfile();assert(f);
  assert(std::fwrite(str.data(),1,str.size(),f)==str.size());std::rewind(f);
  assert(!parse_dimacs(f,k));std::fclose(f);
  k.structural_provenance=std::tmpfile();assert(k.structural_provenance);
}
bool rup(const std::vector<gc::Premise> &clauses,const std::vector<int32_t> &lemma) {
  std::map<int32_t,int> assigned;
  auto variable=[](int32_t literal){return literal<0?-literal:literal;};
  for(int32_t literal:lemma) assigned[variable(literal)]=literal>0?-1:1;
  bool changed=true;
  while(changed) {
    changed=false;
    for(const auto &clause:clauses) {
      bool satisfied=false;size_t unset=0;int32_t unit=0;
      for(int32_t literal:clause.literals) {
        int value=assigned[variable(literal)]*(literal>0?1:-1);
        if(value>0)satisfied=true;
        if(!value){++unset;unit=literal;}
      }
      if(satisfied)continue;
      if(!unset)return true;
      if(unset==1){assigned[variable(unit)]=unit>0?1:-1;changed=true;}
    }
  }
  return false;
}
void check_offers(const DirectCspGuardedCoverOfferResult &r,const gc::Selection &s) {

  assert(r.status==S::Offered && !r.offers.empty());
  auto expected=gc::expected_clauses(s,limits().theory,[](uint64_t){return true;});
  std::vector<gc::Premise> source;uint64_t id=1;
  for(auto &c:expected.clauses)source.push_back({id++,c,0});
  for(const auto &offer:r.offers) {
    auto p=gc::plan(source,offer.selection,limits().theory,[](uint64_t){return true;});
    assert(p.status==gc::Status::Planned && p.additions[0].clause==offer.addition.clause);
    assert(rup(source,offer.addition.clause));
  }
}
int main() {
  for(bool signed_names:{false,true}) {
    Kernel k;auto s=fixture(signed_names);populate(k,s);
    DirectCspGuardedCoverActionSession actions;DirectCspGuardedCoverOfferSession session;
    DirectCspGuardedCoverOfferWindow window;
    window.preferred_literals={s.domains[0][0],s.domains[1][0]};
    auto before=FormulaImage(k);
    auto result=direct_csp_guarded_cover_offer(k,actions,session,limits(),window);
    check_offers(result,s);assert(FormulaImage(k)==before);
    assert(result.arena_index_complete && !result.all_schema_owners_complete);
    // Certified learned source bodies are legal, still individually authenticated.
    for(Ref ref=0;ref<k.arena.size();) {
      Clause *c=clause_at(k.arena,ref);ref+=clause_actual_words(c);c->set_redundant(true);
    }
    result=direct_csp_guarded_cover_offer(k,actions,session,limits(),window);
    check_offers(result,s);assert(FormulaImage(k)==before);
    volatile sig_atomic_t stop=1;k.stop=&stop;
    auto interrupted=direct_csp_guarded_cover_offer(k,actions,session,limits(),window);
    assert(interrupted.status==S::Interrupted && interrupted.offers.empty());k.stop=nullptr;
    auto bounded=limits();bounded.maximum_work=1;
    auto exhausted=direct_csp_guarded_cover_offer(k,actions,session,bounded,window);
    assert(exhausted.status==S::ResourceExhausted && exhausted.offers.empty());
    bounded=limits();bounded.maximum_index_bytes=1;
    assert(direct_csp_guarded_cover_offer(k,actions,session,bounded,window).status==S::ResourceExhausted);
    bounded=limits();bounded.maximum_binary_edges=1;
    exhausted=direct_csp_guarded_cover_offer(k,actions,session,bounded,window);
    assert(exhausted.status==S::ResourceExhausted && exhausted.offers.empty());
    window.after_cover=1;window.stamp=result.stamp;++window.stamp.arena_words;
    assert(direct_csp_guarded_cover_offer(k,actions,session,limits(),window).status==S::Stale);
    assert(FormulaImage(k)==before);
    std::fclose(k.structural_provenance);k.structural_provenance=nullptr;
  }
  {
    Kernel k;auto s=fixture();populate(k,s,-1,true);
    DirectCspGuardedCoverActionSession a;DirectCspGuardedCoverOfferSession session;
    auto r=direct_csp_guarded_cover_offer(k,a,session,limits(),{});
    assert(r.offers.empty() && r.status==S::NoOffer);
    std::fclose(k.structural_provenance);k.structural_provenance=nullptr;
  }
  {
    Kernel k;auto s=fixture();auto expected=gc::expected_clauses(s,limits().theory,[](uint64_t){return true;});
    int missing=-1;
    for(size_t i=0;i<expected.clauses.size();++i)if(expected.clauses[i]==std::vector<int32_t>{-8,-6})missing=i;
    assert(missing>=0);populate(k,s,missing);
    DirectCspGuardedCoverActionSession a;DirectCspGuardedCoverOfferSession session;
    auto r=direct_csp_guarded_cover_offer(k,a,session,limits(),{});
    assert(r.offers.empty() && r.status==S::NoOffer);
    std::fclose(k.structural_provenance);k.structural_provenance=nullptr;
  }
  {
    // General 16x4/32x5 cover geometry from the retained admission certificate,
    // generated with unrelated literals; no target CNF or solver invocation.
    gc::Selection s;
    for(uint32_t d=0;d<16;++d) {
      s.domains.push_back({int32_t(4*d+1),int32_t(4*d+2),int32_t(4*d+3),int32_t(4*d+4)});
      s.selected_values.push_back(0);
    }
    for(uint32_t b=0;b<32;++b) {
      s.cover.push_back(200+b);
      gc::ExactOr o;o.output=100+b;
      for(uint32_t j=0;j<5;++j)o.inputs.push_back(4*(1+(b+j)%15)+1);
      std::sort(o.inputs.begin(),o.inputs.end());
      s.exact_or.push_back(o);s.blockers.push_back({int32_t(200+b),0,b%4,b});
    }
    auto l=limits();l.maximum_work=100000000;l.maximum_role_queries=1000000;l.proposals.maximum_work=50000000;l.maximum_binary_assignments=10000;l.maximum_binary_edges=100000;
    auto expected=gc::expected_clauses(s,l.theory,[](uint64_t){return true;});
    assert(expected.status==gc::Status::Planned);
    std::ostringstream text;text<<"p cnf 240 "<<expected.clauses.size()<<'\n';
    for(auto &c:expected.clauses){for(auto x:c)text<<x<<' ';text<<"0\n";}
    auto str=text.str();FILE *f=std::tmpfile();assert(f);
    std::fwrite(str.data(),1,str.size(),f);std::rewind(f);
    Kernel k;assert(!parse_dimacs(f,k));std::fclose(f);
    k.structural_provenance=std::tmpfile();assert(k.structural_provenance);
    DirectCspGuardedCoverActionSession a;DirectCspGuardedCoverOfferSession session;
    auto r=direct_csp_guarded_cover_offer(k,a,session,l,{});


    assert(r.status==S::Offered);
    bool full_geometry=false;
    for(auto &offer:r.offers)if(offer.selection.cover.size()==32 && offer.selection.domains.size()==16 && offer.selection.exact_or.size()==32 && offer.addition.clause.size()==16)full_geometry=true;
    assert(full_geometry);
    std::vector<gc::Premise> source;uint64_t id=1;
    for(auto &c:expected.clauses)source.push_back({id++,c,0});
    for(auto &offer:r.offers)assert(rup(source,offer.addition.clause));
    std::fclose(k.structural_provenance);k.structural_provenance=nullptr;
  }

  {
    Kernel k;auto s=fixture();populate(k,s,-1,false,{{-1,13},{-13,-4}});
    DirectCspGuardedCoverActionSession a;DirectCspGuardedCoverOfferSession session;
    auto l=limits();l.proposals.maximum_nominations=1;
    auto r=direct_csp_guarded_cover_offer(k,a,session,l,{});
    assert(r.status==S::NoOffer && r.nomination_limit_reached);
    assert(r.effort.binary_implied_rejections>0);
    l.proposals.maximum_nominations=2;
    r=direct_csp_guarded_cover_offer(k,a,session,l,{});
    assert(r.status==S::Offered && r.nomination_limit_reached);
    assert(r.effort.binary_implied_rejections>0);
    // Producer/action causality, existing publication remains the authority.
    DirectCspGuardedCoverActionLimits action_limits{1000,10000,10000000,32,16,64,64,16,32,10000000,8,10000,1000000,100,100,10000000};
    auto applied=direct_csp_guarded_cover_action_apply(k,a,action_limits,r.offers[0].selection);
    assert(applied.status==DirectCspSupportActionStatus::Applied);
    auto duplicate=direct_csp_guarded_cover_offer(k,a,session,l,{});
    assert(duplicate.offers.empty() && duplicate.effort.committed_rejections>0);
    k.semantic_variable_candidate_pending=true;
    assert(direct_csp_guarded_cover_offer(k,a,session,l,{}).status==S::Inapplicable);
    k.semantic_variable_candidate_pending=false;
    std::fclose(k.structural_provenance);k.structural_provenance=nullptr;
  }

  {
    // Explicit alternate-role windows keep a second authenticated owner reachable.
    Kernel k;auto s=fixture();populate(k,s,-1,false,{{-5,14},{-7,14},{-14,5,7},{-8,-14}});
    DirectCspGuardedCoverActionSession a;DirectCspGuardedCoverOfferSession session;
    DirectCspGuardedCoverOfferWindow window;window.role_blocker=0;window.role_alternative=1;
    auto r=direct_csp_guarded_cover_offer(k,a,session,limits(),window);
    assert(r.status==S::Offered);
    bool alternate=false;
    for(auto &offer:r.offers)for(auto &o:offer.selection.exact_or)if(o.output==14)alternate=true;
    assert(alternate);
    std::fclose(k.structural_provenance);k.structural_provenance=nullptr;
  }
  {
    // In-place bodies are recaptured even without epoch/size changes, as in
    // native test helpers which do not traverse public dispatch stamping.
    Kernel k;auto s=fixture();populate(k,s);
    DirectCspGuardedCoverActionSession a;DirectCspGuardedCoverOfferSession session;
    assert(direct_csp_guarded_cover_offer(k,a,session,limits(),{}).status==S::Offered);
    Clause *cover=nullptr;
    for(Ref ref=0;ref<k.arena.size();) {
      Clause *c=clause_at(k.arena,ref);ref+=clause_actual_words(c);
      bool all_cover=c->size==3;
      for(uint32_t i=0;i<c->size;++i)if(kernel_externalize(k,c->lits[i])<8)all_cover=false;
      if(all_cover)cover=c;
    }
    assert(cover);Lit saved=cover->lits[0];cover->lits[0]=kernel_internalize(k,1);
    auto r=direct_csp_guarded_cover_offer(k,a,session,limits(),{});
    assert(r.status==S::NoOffer && r.offers.empty());
    cover->lits[0]=saved;
    assert(direct_csp_guarded_cover_offer(k,a,session,limits(),{}).status==S::Offered);
    std::fclose(k.structural_provenance);k.structural_provenance=nullptr;
  }

}
