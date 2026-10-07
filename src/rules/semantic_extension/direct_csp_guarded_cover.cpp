#include "rules/semantic_extension/direct_csp_guarded_cover.hpp"

#include "rules/semantic_extension/direct_csp_guarded_cover_work.hpp"

#include <algorithm>
#include <climits>
#include <map>
#include <set>

namespace tactix::direct_csp_guarded_cover {

  namespace {

    uint32_t var(int32_t l) {
      return uint32_t(l < 0 ? -int64_t(l) : l);
    }

    bool literal(int32_t l) {
      return l && l != INT32_MIN;
    }

    std::vector<int32_t> norm(std::vector<int32_t> c) {

      std::sort(c.begin(), c.end());
      return c;

    }

    bool canonical(const std::vector<int32_t> &v) {

      if(v.empty()) return false;

      for(size_t i=0;i<v.size();++i) if(!literal(v[i]) || (i && var(v[i-1]) >= var(v[i]))) return false;

      return true;

    }

  }

  bool Bounds::valid() const {

    return maximum_source_clauses && maximum_source_literals && maximum_domains && maximum_domain_values >= 2 && maximum_cover_literals && maximum_or_definitions && maximum_or_inputs && maximum_output_literals;

  }

  Expected expected_clauses(const Selection &s, const Bounds &b, Charge charge) {

    Expected r;

    auto stop=[&](Status status){
      r.status=status;
      r.clauses.clear();
      return r;
    };

    auto spend=[&](uint64_t n){
      if(n>UINT64_MAX-r.work){
        return false;
      }

      if (!charge || !charge(n)) return false;
      r.work+=n;
      return true;
    };

    if(!b.valid() || s.domains.empty() || s.selected_values.size()!=s.domains.size() || s.cover.empty() || s.exact_or.empty() || s.blockers.size()!=s.cover.size()) return stop(Status::Invalid);

    if(s.domains.size()>b.maximum_domains || s.domains.size()>b.maximum_output_literals || s.cover.size()>b.maximum_cover_literals || s.exact_or.size()>b.maximum_or_definitions) return stop(Status::ResourceExhausted);

    std::set<uint32_t> domain_vars, outputs, blocker_vars;

    std::set<std::vector<int32_t>> clauses;

    uint64_t literals=0;

    auto add=[&](std::vector<int32_t> c){

      if(!spend(detail::add(1, detail::multiply(detail::add(c.size(), clauses.size()), detail::add(c.size(), 1)))))return false;

      c=norm(std::move(c));
      if(clauses.contains(c))return true;

      if(clauses.size()==b.maximum_source_clauses || c.size()>b.maximum_source_literals-literals)return false;

      literals+=c.size();
      clauses.insert(std::move(c));
      return true;

    };

    for(size_t d=0;d<s.domains.size();++d){

      const auto&v=s.domains[d];

      if(!spend(1+v.size()))return stop(Status::ResourceExhausted);

      if(v.size()<2 || !canonical(v) || s.selected_values[d]>=v.size())return stop(Status::Invalid);

      if(v.size()>b.maximum_domain_values)return stop(Status::ResourceExhausted);

      for(int32_t x:v)if(!domain_vars.insert(var(x)).second)return stop(Status::Invalid);

      if(!add(v))return stop(Status::ResourceExhausted);

      for(size_t i=0;i<v.size();++i)for(size_t j=i+1;j<v.size();++j) if(!add({-v[i],-v[j]}))return stop(Status::ResourceExhausted);

    }

    for(const auto&o:s.exact_or){

      if(!spend(1+o.inputs.size()))return stop(Status::ResourceExhausted);

      if(!literal(o.output) || !canonical(o.inputs) || domain_vars.contains(var(o.output)) || !outputs.insert(var(o.output)).second)return stop(Status::Invalid);

      if(o.inputs.size()>b.maximum_or_inputs)return stop(Status::ResourceExhausted);

    }

    if(!canonical(s.cover))return stop(Status::Invalid);

    for(int32_t x:s.cover) if(domain_vars.contains(var(x)) || outputs.contains(var(x)) || !blocker_vars.insert(var(x)).second) return stop(Status::Invalid);

    for(const auto&o:s.exact_or){

      std::vector<int32_t> reverse{
        -o.output
      };

      for(int32_t x:o.inputs){

        if(outputs.contains(var(x)) || blocker_vars.contains(var(x)))return stop(Status::Invalid);

        if(!add({-x,o.output}))return stop(Status::ResourceExhausted);

        reverse.push_back(x);

      }

      if(!add(std::move(reverse)))return stop(Status::ResourceExhausted);

    }

    if(!add(s.cover))return stop(Status::ResourceExhausted);

    std::set<int32_t> covered;
    std::set<uint32_t> used_or;

    for(const auto&x:s.blockers){

      if(!spend(1+s.cover.size()))return stop(Status::ResourceExhausted);

      if(!literal(x.literal) || x.domain>=s.domains.size() || x.value>=s.domains[x.domain].size() || x.or_definition>=s.exact_or.size() || !covered.insert(x.literal).second || std::find(s.cover.begin(),s.cover.end(),x.literal)==s.cover.end())return stop(Status::Invalid);

      used_or.insert(x.or_definition);

      if(!add({-x.literal,s.domains[x.domain][x.value]}) || !add({-x.literal,-s.exact_or[x.or_definition].output}))return stop(Status::ResourceExhausted);

    }

    if(used_or.size()!=s.exact_or.size())return stop(Status::Invalid);

    if (!spend(clauses.size() + literals)) return stop(Status::ResourceExhausted);

    r.clauses.assign(clauses.begin(),clauses.end());
    r.status=Status::Planned;
    return r;

  }

  Plan plan(const std::vector<Premise>&source,const Selection&s,const Bounds&b,Charge charge){

    Plan r;
    auto stop=[&](Status status){
      r.status=status;
      r.additions.clear();
      return r;
    };

    auto spend=[&](uint64_t n){
      if(n>UINT64_MAX-r.work || !charge || !charge(n))return false;
      r.work+=n;
      return true;
    };

    Expected image=expected_clauses(s,b,spend);

    if(image.status!=Status::Planned)return stop(image.status);

    if(source.size()>b.maximum_source_clauses)return stop(Status::ResourceExhausted);

    std::map<std::vector<int32_t>,const Premise*> actual;
    std::map<uint64_t,const Premise*> by_id;
    std::set<uint64_t> ids;

    for(const auto&p:source){

      if(!spend(detail::add(1, detail::multiply(detail::add(p.literals.size(), source.size()), detail::add(p.literals.size(), 1)))))return stop(Status::ResourceExhausted);

      if(!p.occurrence || !ids.insert(p.occurrence).second || p.literals.empty())return stop(Status::IncompleteTheory);

      std::set<uint32_t> pv;
      for(int32_t x:p.literals)if(!literal(x)||!pv.insert(var(x)).second)return stop(Status::IncompleteTheory);

      if(p.literals.size()>b.maximum_source_literals-r.source_literals)return stop(Status::ResourceExhausted);

      r.source_literals+=p.literals.size();

      by_id.emplace(p.occurrence,&p);

      if(!actual.emplace(norm(p.literals),&p).second)return stop(Status::IncompleteTheory);

    }

    if(actual.size()!=image.clauses.size())return stop(Status::IncompleteTheory);

    for (const auto &clause : image.clauses) {

      if (!spend(detail::multiply(detail::add(1, actual.size()), detail::add(1, clause.size())))) return stop(Status::ResourceExhausted);

      if (!actual.contains(clause)) return stop(Status::IncompleteTheory);

    }

    if (!spend(detail::multiply(s.domains.size(), detail::add(1, s.domains.size())))) return stop(Status::ResourceExhausted);

    std::vector<int32_t> selected;
    std::set<int32_t> selected_set;

    for(size_t d=0;d<s.domains.size();++d){
      int32_t x=s.domains[d][s.selected_values[d]];
      selected.push_back(x);
      selected_set.insert(x);
    }

    std::set<uint64_t> antecedents;

    auto use=[&](std::vector<int32_t> c){
      const Premise*p=actual.at(norm(std::move(c)));
      antecedents.insert(p->occurrence);
    };

    use(s.cover);

    for(const auto&x:s.blockers){

      if(!spend(detail::add(1, detail::add(detail::multiply(s.exact_or[x.or_definition].inputs.size(), detail::add(1, selected.size())), detail::multiply(3, actual.size())))))return stop(Status::ResourceExhausted);

      const auto&d=s.domains[x.domain];
      int32_t chosen=selected[x.domain], value=d[x.value];

      if(chosen!=value){
        use({-x.literal,value});
        use({-chosen,-value});
      }

      else {
        const auto&o=s.exact_or[x.or_definition];
        auto it=std::find_if(o.inputs.begin(),o.inputs.end(),[&](int32_t l){return selected_set.contains(l);});

        if(it==o.inputs.end())return stop(Status::NoConsequence);

        use({-*it,o.output});
        use({-x.literal,-o.output});
      }

    }

    Addition addition;
    addition.proof=direct_csp_support::ProofStep::Rup;

    for(int32_t x:selected)addition.clause.push_back(-x);

    if (!spend(detail::add(1, detail::multiply(selected.size(), detail::add(1, selected.size()))))) return stop(Status::ResourceExhausted);

    addition.clause=norm(std::move(addition.clause));

    for(uint64_t id:antecedents){
      if(!spend(1))return stop(Status::ResourceExhausted);
      const Premise*p=by_id.at(id);

      addition.antecedents.push_back({direct_csp_support::ReferenceKind::SourceOccurrence,id,p->native_proof_id});
    }

    r.additions.push_back(std::move(addition));
    r.status=Status::Planned;
    return r;

  }

}
// namespace tactix::direct_csp_guarded_cover
