#include "rules/semantic_extension/direct_csp_guarded_cover_offer.hpp"
#include "rules/semantic_extension/direct_csp_guarded_cover_work.hpp"
#include <algorithm>
#include <climits>
#include <map>

namespace tactix {
namespace {
namespace gc = direct_csp_guarded_cover;
using S = DirectCspGuardedCoverOfferStatus;
using ClauseImage = std::vector<int32_t>;
struct Record { ClauseImage literals; uint64_t id; };
DirectCspGuardedCoverOfferStamp stamp(const Kernel &k) {
  return {&k, k.arena_generation, k.touch_epoch, k.strategy_epoch,
          k.arena.size(), k.next_clause_id, k.nvars, k.external_to_internal.size()};
}
struct Search {
  Kernel &k;
  const DirectCspGuardedCoverOfferLimits &limits;
  DirectCspGuardedCoverOfferResult out;
  std::vector<Record> index;
  uint64_t literal_bytes = 0;
  Search(Kernel &kernel, const DirectCspGuardedCoverOfferLimits &bounds)
      : k(kernel), limits(bounds) {}
  bool stopped = false;
  bool charge(uint64_t n) {
    if (stopped) return false;
    if (kernel_terminating(k)) { out.status = S::Interrupted; stopped = true; return false; }
    if (n > limits.maximum_work - out.effort.work) {
      out.status = S::ResourceExhausted; stopped = true; return false;
    }
    out.effort.work += n;
    return true;
  }
  bool resource() { out.status = S::ResourceExhausted; stopped = true; return false; }
  bool role() {
    if (!charge(1)) return false;
    if (out.effort.role_queries == limits.maximum_role_queries) return resource();
    ++out.effort.role_queries;
    return true;
  }
  bool canonical(ClauseImage &c) {
    // Bounded insertion sorting charges every comparison and moved literal.
    for (size_t i = 1; i < c.size(); ++i) {
      int32_t x = c[i]; size_t j = i;
      while (j) {
        if (!charge(2)) return false;
        if (c[j - 1] <= x) break;
        c[j] = c[j - 1]; --j;
      }
      c[j] = x;
    }
    return true;
  }
  bool domain_order(ClauseImage &c) {
    for (size_t i = 1; i < c.size(); ++i) {
      int32_t x = c[i]; size_t j = i;
      while (j) {
        if (!charge(2)) return false;
        if (std::abs(c[j-1]) < std::abs(x) ||
            (std::abs(c[j-1]) == std::abs(x) && c[j-1] <= x)) break;
        c[j] = c[j-1]; --j;
      }
      c[j] = x;
    }
    return true;
  }
  bool live(int32_t x) {
    if (!charge(1)) return false;
    if (!x || x == INT32_MIN) return false;
    Lit l = kernel_internalize(k, x);
    return l != INVALID_LIT && !kernel_var_retired(k, var_of(l)) &&
           !k.value(l) && kernel_externalize(k,l) == x;
  }
  // Physical multiplicity is checked at one canonical endpoint. A zero native
  // ID is the existing untracked binary sentinel, not an occurrence identity.
  unsigned binary(int32_t a, int32_t b, uint64_t *id = nullptr) {
    if (!live(a) || !live(b)) return 0;
    Lit first = kernel_internalize(k,a), second = kernel_internalize(k,b);
    if (first > second) std::swap(first,second);
    unsigned found = 0;
    int32_t low=std::min(a,b), high=std::max(a,b);
    for (const auto &record:index) {
      if (!charge(1)) return 0;
      if (record.literals.size()!=2) continue;
      if (!charge(2)) return 0;
      if (record.literals[0]==low && record.literals[1]==high) {
        found=std::min(2u,found+1);
        if (id) *id=record.id;
      }
    }
    bool watch_found=false;
    for (auto w : k.watches.entries(first)) {
      if (!charge(1)) return 0;
      if (w.binary() && w.other() == second) { found=std::min(2u,found+1); watch_found=true; }
    }
    if (watch_found && id) *id = k.binary_ids.tracks_values() ? kernel_binary_id(k,first,second) : 0;
    return found;
  }
  bool build() {
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      ref += clause_actual_words(c);
      if (!charge(1)) return false;
      if (c->garbage()) continue;
      if (out.effort.index_clauses == limits.maximum_index_clauses ||
          c->size > limits.maximum_index_literals - out.effort.index_literals) return resource();
      uint64_t extra = gc::detail::multiply(c->size,sizeof(int32_t));
      // Count both old/new metadata storage at vector growth; literal capacities
      // are fixed to the native clause size. This bounds requested index heap,
      // not allocator overhead or process RSS.
      if (index.size() == index.capacity()) {
        uint64_t capacity = index.capacity() ? gc::detail::multiply(index.capacity(),2) : 1;
        capacity = std::min(capacity, limits.maximum_index_clauses);
        uint64_t peak = gc::detail::add(literal_bytes,gc::detail::multiply(gc::detail::add(capacity,index.capacity()),sizeof(Record)));
        if (peak > limits.maximum_index_bytes || !charge(capacity)) return resource();
        index.reserve(capacity);
      }
      uint64_t current = gc::detail::add(literal_bytes,gc::detail::multiply(index.capacity(),sizeof(Record)));
      if (current > limits.maximum_index_bytes || extra > limits.maximum_index_bytes-current) return resource();
      if (!charge(1 + c->size)) return false;
      Record r; r.id = c->id(); r.literals.reserve(c->size);
      bool mapped = true;
      for (uint32_t i=0;i<c->size;++i) {
        int32_t x=kernel_externalize(k,c->lits[i]);
        if (!x || x==INT32_MIN || kernel_var_retired(k,var_of(c->lits[i])) || k.value(c->lits[i])) mapped=false;
        r.literals.push_back(x);
      }
      ++out.effort.index_clauses; out.effort.index_literals += c->size;
      if (!mapped) {
        if(c->size<=2){out.status=S::Stale;stopped=true;return false;}
        continue;
      }
      literal_bytes += extra;
      if (!canonical(r.literals)) return false;
      index.push_back(std::move(r));
    }
    out.arena_index_complete=true;
    return true;
  }
  unsigned occurrence(const ClauseImage &image, uint64_t *id=nullptr) {
    if (!charge(1+image.size())) return 0;
    ClauseImage c=image;
    if (!canonical(c)) return 0;
    if (c.size()==2) return binary(c[0],c[1],id);
    unsigned found=0;
    for (const auto &r:index) {
      if (!charge(1+c.size())) return 0;
      if (r.literals==c) { found=std::min(2u,found+1); if(id)*id=r.id; }
    }
    return found;
  }
  std::vector<int32_t> neighbors(int32_t literal) {
    std::vector<int32_t> result;
    if (!live(literal)) return result;
    Lit l=kernel_internalize(k,literal);
    for (const auto &record:index) {
      if (!charge(1)) return {};
      if (record.literals.size()!=2) continue;
      for(size_t side=0;side<2;++side) {
        if(!charge(1))return {};
        if(record.literals[side]!=literal)continue;
        int32_t x=record.literals[side^1];
        if(!live(x))continue;
        if(result.size()==limits.maximum_index_literals){resource();return {};}
        if(!charge(1+result.size()))return {};
        if(std::find(result.begin(),result.end(),x)==result.end())result.push_back(x);
      }
    }
    for (auto w:k.watches.entries(l)) {
      if (!charge(1)) return {};
      if (!w.binary()) continue;
      int32_t x=kernel_externalize(k,w.other());
      if (!live(x)) continue;
      if (result.size()==limits.maximum_index_literals) { resource(); return {}; }
      if (!charge(1+result.size())) return {};
      if (std::find(result.begin(),result.end(),x)==result.end()) result.push_back(x);
    }
    domain_order(result);
    return result;
  }
  bool exact_domain(const ClauseImage &d) {
    if (d.size()<2 || d.size()>limits.theory.maximum_domain_values || occurrence(d)!=1) return false;
    for (size_t i=0;i<d.size();++i) for(size_t j=i+1;j<d.size();++j) {
      if (!role() || std::abs(d[i])==std::abs(d[j]) || binary(-d[i],-d[j])!=1) return false;
    }
    return !stopped;
  }
  ClauseImage find_domain(int32_t value) {
    for (const auto &r:index) {
      if (!role() || !charge(r.literals.size())) return {};
      if (std::find(r.literals.begin(),r.literals.end(),value)==r.literals.end()) continue;
      if (!charge(1+r.literals.size())) return {};
      ClauseImage d=r.literals;
      if (!domain_order(d)) return {};
      if (exact_domain(d)) return d;
    }
    for (int32_t other:neighbors(value)) {
      if (!role()) return {};
      ClauseImage d{value,other};
      if (!domain_order(d)) return {};
      if (exact_domain(d)) return d;
    }
    return {};
  }
  bool exact_or(const gc::ExactOr &o) {
    if(o.inputs.empty() || o.inputs.size()>limits.theory.maximum_or_inputs) return false;
    if (!charge(1+o.inputs.size())) return false;
    ClauseImage reverse=o.inputs; reverse.push_back(-o.output);
    if(occurrence(reverse)!=1) return false;
    for(int32_t x:o.inputs) if(!role() || std::abs(x)==std::abs(o.output) || binary(-x,o.output)!=1) return false;
    return !stopped;
  }
  gc::ExactOr find_or(int32_t output) {
    for (const auto &r:index) {
      if (!role() || !charge(r.literals.size())) return {};
      if(std::find(r.literals.begin(),r.literals.end(),-output)==r.literals.end()) continue;
      gc::ExactOr o; o.output=output;
      for(int32_t x:r.literals) if(x!=-output)o.inputs.push_back(x);
      if (!domain_order(o.inputs)) return {};
      if(exact_or(o))return o;
    }
    for(int32_t input:neighbors(-output)) {
      if(!role())return {};
      gc::ExactOr o{output,{input}};
      if(exact_or(o))return o;
    }
    return {};
  }
  uint32_t add_domain(gc::Selection &s, const ClauseImage &d) {
    if(!charge(gc::detail::multiply(1+s.domains.size(),1+d.size())))return UINT32_MAX;
    for(uint32_t i=0;i<s.domains.size();++i)if(s.domains[i]==d)return i;
    // Overlap is a shape rejection, not evidence of no alternative owner.
    for(const auto &old:s.domains)for(int32_t x:old)for(int32_t y:d) {
      if(!charge(1))return UINT32_MAX;
      if(std::abs(x)==std::abs(y))return UINT32_MAX;
    }
    for(int32_t x:d) {
      for(int32_t b:s.cover) {
        if(!charge(1))return UINT32_MAX;
        if(std::abs(x)==std::abs(b))return UINT32_MAX;
      }
      for(const auto &o:s.exact_or) {
        if(!charge(1))return UINT32_MAX;
        if(std::abs(x)==std::abs(o.output))return UINT32_MAX;
      }
    }
    if(s.domains.size()==limits.theory.maximum_domains)return UINT32_MAX;
    s.domains.push_back(d);s.selected_values.push_back(0);
    return s.domains.size()-1;
  }
  bool owner(const ClauseImage &cover,gc::Selection &s, uint32_t role_blocker, uint64_t role_alternative) {
    if (!charge(1+cover.size())) return false;
    s.cover=cover;
    if(!domain_order(s.cover))return false;
    uint32_t blocker_position=0;
    for(int32_t b:s.cover) {
      uint64_t valid_role=0;
      const uint64_t wanted_role=blocker_position++ == role_blocker ? role_alternative : 0;
      auto links=neighbors(-b);
      bool chosen=false;
      for(int32_t x:links) {
        if(!role())return false;
        auto d=find_domain(x); if(d.empty())continue;
        for(int32_t negative:links) {
          if(!role())return false;
          auto o=find_or(-negative); if(!o.output)continue;
          bool admissible=true;
          for(int32_t v:d) {
            if(!charge(1))return false;
            if(std::abs(v)==std::abs(o.output))admissible=false;
          }
          for(int32_t v:s.cover) {
            if(!charge(1+o.inputs.size()))return false;
            if(std::abs(v)==std::abs(o.output))admissible=false;
            for(int32_t input:o.inputs)if(std::abs(v)==std::abs(input))admissible=false;
          }
          for(const auto &old:s.domains)for(int32_t v:old) {
            if(!charge(1))return false;
            if(std::abs(v)==std::abs(o.output))admissible=false;
          }
          for(const auto &old:s.exact_or) {
            if(!charge(1+d.size()+o.inputs.size()+old.inputs.size()))return false;
            if(old.output!=o.output && std::abs(old.output)==std::abs(o.output))admissible=false;
            for(int32_t v:d)if(std::abs(v)==std::abs(old.output))admissible=false;
            for(int32_t input:o.inputs)if(std::abs(input)==std::abs(old.output))admissible=false;
            for(int32_t input:old.inputs)if(std::abs(input)==std::abs(o.output))admissible=false;
          }
          if(!admissible || valid_role++ != wanted_role)continue;
          uint32_t di=add_domain(s,d); if(di==UINT32_MAX)continue;
          if(!charge(1+d.size()+s.exact_or.size()*(1+o.inputs.size())))return false;
          uint32_t vi=std::find(d.begin(),d.end(),x)-d.begin();
          uint32_t oi=0;
          while(oi<s.exact_or.size() && !(s.exact_or[oi]==o))++oi;
          if(oi==s.exact_or.size()) {
            if(oi==limits.theory.maximum_or_definitions)return false;
            s.exact_or.push_back(std::move(o));
          }
          s.blockers.push_back({b,di,vi,oi});chosen=true;break;
        }
        if(chosen)break;
      }
      if(!chosen)return false;
    }
    // Add independently authenticated, nonoverlapping input domains so phase
    // preferences can discharge OR roles rather than only AMO roles.
    for(const auto &o:s.exact_or)for(int32_t x:o.inputs) {
      auto d=find_domain(x);if(!d.empty())add_domain(s,std::move(d));
      if(stopped)return false;
    }
    return true;
  }
  bool source(const gc::Selection &s,std::vector<gc::Premise>&result) {
    auto expected=gc::expected_clauses(s,limits.theory,[&](uint64_t n){return charge(n);});
    if(expected.status!=gc::Status::Planned)return false;
    uint64_t occurrence_id=1;
    for(const auto &c:expected.clauses) {
      uint64_t id=0;unsigned count=occurrence(c,&id);
      if(count!=1) {
        if(count>1)++out.effort.ambiguous_source_rejections;
        return false;
      }
      for(int32_t x:c)if(!live(x))return false;
      if(!charge(1+c.size()))return false;
      result.push_back({occurrence_id++,c,id});
    }
    return true;
  }
  // Closure is local, never mutates Kernel and never treats bounded truncation
  // as absence. All live binary edges reachable from the selected tuple count.
  bool not_binary_implied(const gc::Selection &s) {
    std::map<Var,bool> values;
    std::vector<Lit> queue;
    bool conflict=false;
    auto assign=[&](Lit l) {
      if(!charge(2+values.size()))return false;
      auto it=values.find(var_of(l));
      bool sign=bool(l&1);
      if(it!=values.end()) { if(it->second!=sign)conflict=true;return true; }
      if(out.effort.binary_assignments==limits.maximum_binary_assignments)return resource();
      ++out.effort.binary_assignments;values.emplace(var_of(l),sign);queue.push_back(l);return true;
    };
    for(size_t i=0;i<s.domains.size();++i) {
      int32_t x=s.domains[i][s.selected_values[i]];
      if(!live(x)||!assign(kernel_internalize(k,x)))return false;
    }
    for(size_t i=0;i<queue.size()&&!conflict;++i) {
      for (const auto &record:index) {
        if (!charge(1)) return false;
        if (record.literals.size()!=2) continue;
        int32_t false_literal=kernel_externalize(k,queue[i]^1);
        for (size_t side=0;side<2;++side) {
          if (!charge(1)) return false;
          if (record.literals[side]!=false_literal) continue;
          if(out.effort.binary_edges==limits.maximum_binary_edges)return resource();
          ++out.effort.binary_edges;
          int32_t other=record.literals[side^1];
          if (!live(other) || !assign(kernel_internalize(k,other))) return false;
        }
      }
      for(auto w:k.watches.entries(queue[i]^1)) {
        if(!charge(1))return false;
        if(!w.binary())continue;
        if(out.effort.binary_edges==limits.maximum_binary_edges)return resource();
        ++out.effort.binary_edges;
        if(kernel_var_retired(k,var_of(w.other()))) {
          out.status=S::Stale;stopped=true;return false;
        }
        if(!assign(w.other()))return false;
        if(conflict)break;
      }
    }
    if(conflict) { ++out.effort.binary_implied_rejections;return false; }
    return charge(1);
  }
};
} // namespace

bool DirectCspGuardedCoverOfferLimits::valid() const {
  return theory.valid() && proposals.valid() && maximum_work &&
      maximum_index_clauses && maximum_index_literals && maximum_index_bytes &&
      maximum_cover_candidates && maximum_role_queries && maximum_owners &&
      maximum_offers && maximum_binary_assignments && maximum_binary_edges;
}
DirectCspGuardedCoverOfferResult direct_csp_guarded_cover_offer(
    Kernel &k,const DirectCspGuardedCoverActionSession &actions,
    DirectCspGuardedCoverOfferSession &session,
    const DirectCspGuardedCoverOfferLimits &limits,
    const DirectCspGuardedCoverOfferWindow &window) {
  Search search{k,limits};auto &out=search.out;out.stamp=stamp(k);
  if(!limits.valid())return out;
  if(actions.retained()&&!actions.belongs_to(k)){out.status=S::Stale;return out;}
  if(window.after_cover && !(window.stamp==out.stamp)){out.status=S::Stale;return out;}
  if(!direct_csp_guarded_cover_action_guard(k).eligible) {out.status=S::Inapplicable;return out;}
  ++session.calls;
  if(!search.build())return out;
  uint64_t cover_number=0;
  for(const auto &record:search.index) {
    ++cover_number;
    if(cover_number<=window.after_cover)continue;
    out.after_cover=cover_number;
    if(!search.charge(1))break;
    if(out.effort.cover_candidates==limits.maximum_cover_candidates) {
      out.after_cover=cover_number-1;break;
    }
    ++out.effort.cover_candidates;
    if(record.literals.size()<3 || record.literals.size()>limits.theory.maximum_cover_literals)continue;
    gc::Selection owner;
    if(!search.owner(record.literals,owner,window.role_blocker,window.role_alternative)){if(search.stopped)break;continue;}
    std::vector<gc::Premise> source;
    if(!search.source(owner,source)){if(search.stopped)break;continue;}
    uint64_t shape_cost = gc::detail::add(1, owner.cover.size());
    for (const auto &d : owner.domains) shape_cost = gc::detail::add(shape_cost, 1+d.size());
    for (const auto &o : owner.exact_or) shape_cost = gc::detail::add(shape_cost, 1+o.inputs.size());
    shape_cost = gc::detail::add(shape_cost,gc::detail::multiply(owner.blockers.size(),4));
    if (!search.charge(gc::detail::multiply(1+actions.retained_owners(),shape_cost))) break;
    if (const auto *retained = actions.snapshot(owner)) {
      if (!search.charge(1+source.size())) break;
      if (retained->source.size()!=source.size()) {out.status=S::Stale;search.stopped=true;break;}
      bool stale=false;
      for (size_t i=0;i<source.size();++i) {
        if (!search.charge(1+source[i].literals.size())) break;
        if (retained->source[i].literals!=source[i].literals ||
            retained->source[i].occurrence!=source[i].occurrence ||
            (source[i].literals.size()==2 && retained->source[i].native_proof_id!=source[i].native_proof_id)) stale=true;
      }
      if (search.stopped) break;
      // Fresh large IDs are supported by the staged action owner refresh;
      // binary proof-ID/sentinel changes remain exact and ineligible.
      if (stale) {out.status=S::Stale;search.stopped=true;break;}
    }
    if(out.effort.owners==limits.maximum_owners){out.after_cover=cover_number-1;break;}
    ++out.effort.owners;
    if (!search.charge(1+owner.domains.size())) break;
    std::vector<uint32_t> preferences(owner.domains.size(),0);
    for(size_t i=0;i<owner.domains.size();++i)for(int32_t preferred:window.preferred_literals) {
      if(!search.charge(1+owner.domains[i].size()))break;
      auto it=std::find(owner.domains[i].begin(),owner.domains[i].end(),preferred);
      if(it!=owner.domains[i].end()){preferences[i]=it-owner.domains[i].begin();break;}
    }
    if(search.stopped)break;
    auto proposal_limits=limits.proposals;
    proposal_limits.maximum_work=std::min(proposal_limits.maximum_work,limits.maximum_work-out.effort.work);
    if(!proposal_limits.maximum_work){search.resource();break;}
    auto proposed=gc::proposals::propose(source,owner,preferences,limits.theory,proposal_limits,[&]{return kernel_terminating(k);});
    if(!search.charge(proposed.effort.work))break;
    out.nomination_limit_reached|=proposed.nomination_limit_reached;
    if(proposed.status==gc::proposals::Status::Interrupted){out.status=S::Interrupted;search.stopped=true;break;}
    if(proposed.status==gc::proposals::Status::ResourceExhausted){search.resource();break;}
    for(auto &nomination:proposed.nominations) {
      auto key=actions.lookup_committed(nomination.selection,limits.theory,[&](uint64_t n){return search.charge(n);});
      if(search.stopped)break;
      if(key==DirectCspGuardedCoverKeyLookup::ResourceExhausted){search.resource();break;}
      if(key==DirectCspGuardedCoverKeyLookup::Invalid){out.status=S::Invalid;search.stopped=true;break;}
      if(key==DirectCspGuardedCoverKeyLookup::Present){++out.effort.committed_rejections;continue;}
      if(search.occurrence(nomination.addition.clause)) {++out.effort.existing_rejections;continue;}
      if(search.stopped)break;
      if(!search.not_binary_implied(nomination.selection)){if(search.stopped)break;continue;}
      bool duplicate=false;
      for(const auto &old:out.offers) {
        if(!search.charge(1+nomination.addition.clause.size()))break;
        if(old.addition.clause==nomination.addition.clause)duplicate=true;
      }
      if(search.stopped)break;
      if(!duplicate) {
        if(!search.charge(1+out.offers.size()))break;
        out.offers.push_back(std::move(nomination));
      }
      if(out.offers.size()==limits.maximum_offers){out.offer_limit_reached=true;break;}
    }
    if(search.stopped)break;
    if(out.offer_limit_reached || proposed.nomination_limit_reached) {
      // Retain this owner window: filtered or withheld nominations are not
      // exhaustion and must remain reachable with new preference/budget.
      out.after_cover=cover_number-1;
      break;
    }
  }
  if(search.stopped){out.offers.clear();return out;}
  if(!(stamp(k)==out.stamp)){out.status=S::Stale;out.offers.clear();return out;}
  if(!search.charge(1)){out.offers.clear();return out;}
  out.cover_window_complete=!out.nomination_limit_reached && !out.offer_limit_reached && out.after_cover>=search.index.size();
  out.status=out.offers.empty()?S::NoOffer:S::Offered;
  return out;
}
} // namespace tactix
