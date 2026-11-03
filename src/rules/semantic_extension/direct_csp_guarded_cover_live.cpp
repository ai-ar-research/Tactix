#include "rules/semantic_extension/direct_csp_guarded_cover_live.hpp"
#include "proof/proof.hpp"

#include "rules/semantic_extension/bounded_journal.hpp"
#include "rules/semantic_extension/csp_physical_scan.hpp"
#include "rules/semantic_extension/direct_csp_guarded_cover_work.hpp"
#include "rules/semantic_extension/interruptible_work.hpp"
#include "rules/semantic_extension/live_adapter.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>

namespace tactix {

  namespace {

    namespace gc=direct_csp_guarded_cover;

    using Status=DirectCspSupportLiveStatus;

    using Meter=InterruptibleWork;

    Status mapped(gc::Status s){
      switch(s){

        case gc::Status::Planned:return Status::Applied;

        case gc::Status::Invalid:return Status::Invalid;

        case gc::Status::IncompleteTheory:return Status::NoStructure;

        case gc::Status::NoConsequence:return Status::NoConsequence;

        case gc::Status::ResourceExhausted:return Status::ResourceExhausted;

        case gc::Status::Interrupted:return Status::Interrupted;

      }
      std::abort();
    }

    bool sort(std::vector<int32_t>&c,Meter&m){

      for(size_t i=1;i<c.size();++i){
        int32_t x=c[i];
        size_t j=i;

        while(j){
          if(!m.charge(1))return false;
          if(c[j-1]<=x)break;
          c[j]=c[j-1];
          --j;
        }
        c[j]=x;
      }

      return true;

    }

    Status capture(const Kernel&k,const DirectCspGuardedCoverLiveRequest&r,Meter&m, DirectCspGuardedCoverOwnerSnapshot&out){

      auto expected=gc::expected_clauses(r.selection,r.bounds,[&](uint64_t n){return m.charge(n);});

      if(expected.status!=gc::Status::Planned)return stopped_status(m,mapped(expected.status));

      uint64_t image_literals = 0;

      for (const auto &clause : expected.clauses) image_literals = gc::detail::add(image_literals, clause.size());

      if (!m.charge(gc::detail::multiply(gc::detail::add(1, expected.clauses.size()), gc::detail::add(1, image_literals)))) return stopped_status(m,Status::Invalid);

      std::set<std::vector<int32_t>> wanted(expected.clauses.begin(),expected.clauses.end());

      std::map<std::vector<int32_t>,uint64_t> found;
      uint64_t total=0;
      Status failure=Status::Applied;

      for(const auto&c:expected.clauses)for(int32_t external:c){

        if(!m.charge(1))return stopped_status(m,Status::Invalid);

        Lit l=kernel_internalize(k,external);

        if(l==INVALID_LIT||kernel_var_retired(k,var_of(l))||
           kernel_externalize(k,l)!=external||k.value(l))return Status::Stale;

      }

      auto observe=[&](const Lit*ls,uint32_t n,uint64_t id){

        if(!m.charge(1+uint64_t(n))){
          failure=stopped_status(m,Status::Invalid);
          return false;
        }

        std::vector<int32_t> c;
        c.reserve(n);

        for(uint32_t i=0;i<n;++i)c.push_back(kernel_externalize(k,ls[i]));

        if(!sort(c,m)){
          failure=stopped_status(m,Status::Invalid);
          return false;
        }

        if (!m.charge(gc::detail::multiply(gc::detail::add(1, wanted.size()), gc::detail::add(1, c.size())))) {

          failure = stopped_status(m,Status::Invalid);

          return false;

        }

        if(!wanted.contains(c))return true;

        if(found.contains(c)){
          failure=Status::Invalid;
          return false;
        }

        if(found.size()==r.bounds.maximum_source_clauses||n>r.bounds.maximum_source_literals-total){
          failure=Status::ResourceExhausted;
          return false;
        }

        total+=n;
        found.emplace(std::move(c),id);
        return true;

      };

      const csp_physical_scan::Status scan=csp_physical_scan::run(k,m,observe);

      if(scan!=csp_physical_scan::Status::Complete)return scan==csp_physical_scan::Status::Stopped?stopped_status(m,Status::Invalid):failure;

      if(found.size()!=wanted.size())return Status::NoStructure;

      if (!m.charge(gc::detail::add(1, gc::detail::add(image_literals, gc::detail::multiply(r.selection.blockers.size(), 4))))) return stopped_status(m,Status::Invalid);

      DirectCspGuardedCoverOwnerSnapshot result;
      result.selection=r.selection;
      result.source_literals=total;

      for(const auto&[c,id]:found){
        if(!m.charge(1+c.size()))return stopped_status(m,Status::Invalid);

        result.source.push_back({result.source.size()+1,c,id});
      }

      out=std::move(result);
      return Status::Applied;

    }

    bool same(const DirectCspGuardedCoverOwnerSnapshot&a,const DirectCspGuardedCoverOwnerSnapshot&b,Meter&m){
      return direct_csp_guarded_cover_same_source(a,b,[&](uint64_t n){return m.charge(n);},false);
    }

    bool valid(const DirectCspGuardedCoverLiveRequest&r){
      return r.bounds.valid()&&r.maximum_recognition_work&& r.maximum_proof_work&&r.maximum_proof_additions&&r.maximum_proof_literal_occurrences&& r.maximum_journal_bytes&&r.maximum_external_extent&&r.action_id&&r.semantic_id&&r.records;
    }

    struct Prepared {

      DirectCspSupportLiveResult result;

      DirectCspGuardedCoverOwnerSnapshot owner;

      gc::Plan plan;

      std::string journal;

      std::vector<Lit> literals;

      uint64_t id=0;

    };

    void prepare(Kernel&k,const DirectCspGuardedCoverLiveRequest&r,Prepared&p){

      auto&out=p.result;
      if(!valid(r))return;

      if(!live_admitted(k,csp_physical_scan::root_ready,out.status))return;

      Meter m{
        k,r.maximum_recognition_work
      };

      auto finish=[&](Status s){
        out.status=stopped_status(m,s);
        out.recognition_work=m.used;
        out.recognition_limit_exhausted=m.exhausted;
      };

      const uint64_t scan=m.used;
      Status captured=capture(k,r,m,p.owner);

      out.source_scan_work=m.used-scan;

      if(captured!=Status::Applied){
        finish(r.retained_owner&&captured==Status::NoStructure?Status::Stale:captured);
        return;
      }

      if(r.retained_owner){

        if(!direct_csp_guarded_cover_owner_shape_equal(r.retained_owner->selection,r.selection)){
          finish(Status::Invalid);
          return;
        }

        if(!same(*r.retained_owner,p.owner,m)){
          finish(Status::Stale);
          return;
        }

      }

      out.source_clauses=p.owner.source.size();
      out.source_literals=p.owner.source_literals;

      const uint64_t begin=m.used;
      p.plan=gc::plan(p.owner.source,r.selection,r.bounds,[&](uint64_t n){return m.charge(n);});

      out.planning_work=m.used-begin;

      if(p.plan.status!=gc::Status::Planned){
        finish(mapped(p.plan.status));
        return;
      }

      const auto&addition=p.plan.additions.front();

      if(k.external_to_internal.size()>r.maximum_external_extent||k.next_clause_id>=uint64_t(INT64_MAX)){

        finish(Status::ResourceExhausted);
        return;
      }

      out.planned_additions=1;
      out.proof_literal_occurrences=addition.clause.size();

      out.proof_work=1+addition.clause.size()+addition.antecedents.size();

      if(out.proof_work>r.maximum_proof_work||out.proof_literal_occurrences>r.maximum_proof_literal_occurrences){
        finish(Status::ResourceExhausted);
        return;
      }

      if(addition.clause.size()>=3){
        uint64_t words=CLAUSE_HEADER_WORDS+addition.clause.size();

        if(k.arena.size()>=INVALID_REF||words>=INVALID_REF-k.arena.size()){
          finish(Status::ResourceExhausted);
          return;
        }
      }

      p.literals.reserve(addition.clause.size());

      for(int32_t x:addition.clause){
        if(!m.charge(1)){
          finish(Status::Invalid);
          return;
        }
        Lit l=kernel_internalize(k,x);

        if(l==INVALID_LIT||kernel_var_retired(k,var_of(l))||
           kernel_externalize(k,l)!=x||k.value(l)){
          finish(Status::Stale);
          return;
        }
        p.literals.push_back(l);
      }

      // Scan exact live clauses for duplicate conclusion; no residualized source.
      bool duplicate=false;

      auto equal=[&](const Lit*ls,uint32_t n){
        if(!m.charge(1+uint64_t(n)))return false;

        if(n!=p.literals.size()){
          return true;
        }

        std::vector<int32_t> c;
        for(uint32_t i=0;i<n;++i)c.push_back(kernel_externalize(k,ls[i]));

        if(!sort(c,m)){
          return false;
        }

        if (c == addition.clause) duplicate = true;

        return true;
      };

      for(const auto [ref,c]:arena_records(k.arena)){
        if(!m.charge(1)){
          finish(Status::Invalid);
          return;
        }

        if(!c->garbage()&&!equal(c->lits,c->size)){
          finish(Status::Invalid);
          return;
        }
      }

      if(p.literals.size()==2)for(const WatchEntry w:k.watches.entries(p.literals[0])){

        if(!m.charge(1)){
          finish(Status::Invalid);
          return;
        }
        if(w.binary()&&w.other()==p.literals[1])duplicate=true;
      }

      if(duplicate){
        finish(Status::Duplicate);
        return;
      }

      p.id=k.next_clause_id+1;

      // Conservative bound charged before constructing the provenance image.
      uint64_t journal_bound = 256;
      journal_bound = gc::detail::add(journal_bound, gc::detail::multiply(p.owner.source.size(), 100));
      journal_bound = gc::detail::add(journal_bound, gc::detail::multiply(p.owner.source_literals, 16));
      journal_bound = gc::detail::add(journal_bound, gc::detail::multiply(addition.clause.size(), 16));
      journal_bound = gc::detail::add(journal_bound, gc::detail::multiply(addition.antecedents.size(), 24));

      if (journal_bound > r.maximum_journal_bytes) {

        finish(Status::ResourceExhausted);

        return;

      }

      if (!m.charge(journal_bound)) {
        finish(Status::Invalid);
        return;
      }

      std::ostringstream journal;
      journal<<"{\"kind\":\"direct-csp-guarded-cover-v2\",\"action_id\":"<<r.action_id<<",\"semantic_id\":"<<r.semantic_id<<",\"source\":[";

      for(size_t i=0;i<p.owner.source.size();++i){
        const auto&s=p.owner.source[i];
        if(i)journal<<',';

        journal<<"{\"occurrence\":"<<s.occurrence<<",\"native_id\":"<<s.native_proof_id<<",\"literals\":[";

        for(size_t j=0;j<s.literals.size();++j){
          if(j)journal<<',';
          journal<<s.literals[j];
        }
        journal<<"]}";
      }

      journal<<"],\"clause_id\":"<<p.id<<",\"clause\":[";

      for(size_t i=0;i<addition.clause.size();++i){
        if(i)journal<<',';
        journal<<addition.clause[i];
      }

      journal<<"],\"antecedents\":[";
      for(size_t i=0;i<addition.antecedents.size();++i){
        if(i)journal<<',';
        journal<<addition.antecedents[i].id;
      }

      journal<<"],\"extent_growth\":0}\n";
      p.journal=journal.str();
      out.journal_work=p.journal.size();

      if(p.journal.size()>r.maximum_journal_bytes){
        finish(Status::ResourceExhausted);
        return;
      }

      DirectCspGuardedCoverOwnerSnapshot current;
      const uint64_t revalidation=m.used;

      Status recaptured=capture(k,r,m,current);

      if(recaptured!=Status::Applied||!same(p.owner,current,m)){
        out.revalidation_work=m.used-revalidation;
        finish(Status::Stale);
        return;
      }

      out.revalidation_work=m.used-revalidation;

      if(kernel_terminating(k)){
        finish(Status::Interrupted);
        return;
      }

      if (!m.charge(addition.clause.size() + r.selection.domains.size())) {

        finish(Status::Invalid);

        return;

      }

      out.published_support = addition.clause;

      out.semantic_variables.reserve(r.selection.domains.size());

      for (size_t d = 0; d < r.selection.domains.size(); ++d) out.semantic_variables.push_back({r.selection.domains[d][r.selection.selected_values[d]], DirectCspSemanticRole::SelectedDomainValue, false, {}});

      finish(Status::Applied);

    }

  }

  bool direct_csp_guarded_cover_owner_shape_equal(const gc::Selection&a,const gc::Selection&b){

    return a.domains==b.domains&&a.cover==b.cover&&a.exact_or==b.exact_or&&a.blockers==b.blockers;

  }

  uint64_t direct_csp_guarded_cover_shape_work(const gc::Selection&s){

    uint64_t shape_work = gc::detail::add(1, gc::detail::add(s.cover.size(), gc::detail::multiply(s.blockers.size(), 4)));

    for (const auto &domain : s.domains) shape_work = gc::detail::add(shape_work, gc::detail::add(1, domain.size()));

    for (const auto &definition : s.exact_or) shape_work = gc::detail::add(shape_work, gc::detail::add(1, definition.inputs.size()));

    return shape_work;

  }

  bool direct_csp_guarded_cover_same_source(const DirectCspGuardedCoverOwnerSnapshot&a,const DirectCspGuardedCoverOwnerSnapshot&b,const gc::Charge&charge,bool refresh_large_ids){

    if(!charge(direct_csp_guarded_cover_shape_work(a.selection))||!direct_csp_guarded_cover_owner_shape_equal(a.selection,b.selection)||a.source_literals!=b.source_literals||a.source.size()!=b.source.size())return false;

    for(size_t i=0;i<a.source.size();++i){
      const auto&x=a.source[i];
      const auto&y=b.source[i];
      if(!charge(1+x.literals.size())||x.occurrence!=y.occurrence||x.literals!=y.literals||((!refresh_large_ids||x.literals.size()<3)&&x.native_proof_id!=y.native_proof_id))return false;
    }

    return true;

  }

  DirectCspSupportOwnerCaptureResult direct_csp_guarded_cover_live_capture_owner( Kernel&k,const DirectCspGuardedCoverLiveRequest&r,DirectCspGuardedCoverOwnerSnapshot&owner){

    DirectCspSupportOwnerCaptureResult out;
    if(!r.bounds.valid()||!r.maximum_recognition_work)return out;

    if(!root_admitted(k,csp_physical_scan::root_ready,out.status))return out;

    Meter m{
      k,r.maximum_recognition_work
    };
    DirectCspGuardedCoverOwnerSnapshot temporary;
    out.status=capture(k,r,m,temporary);

    out.recognition_work=m.used;
    out.recognition_limit_exhausted=m.exhausted;

    if(out.status==Status::Applied){
      out.source_clauses=temporary.source.size();
      out.source_literals=temporary.source_literals;
      owner=std::move(temporary);
    }
    return out;

  }

  DirectCspSupportLiveResult direct_csp_guarded_cover_live_preflight(Kernel&k,const DirectCspGuardedCoverLiveRequest&r){
    Prepared p;
    prepare(k,r,p);
    return p.result;
  }

  DirectCspSupportLiveResult direct_csp_guarded_cover_live_apply(Kernel&k,const DirectCspGuardedCoverLiveRequest&r){

    Prepared p;
    prepare(k,r,p);
    auto out=std::move(p.result);
    if(out.status!=Status::Applied)return out;

    k.close_startup_certificate();
    uint64_t id=++k.next_clause_id;
    if(id!=p.id)std::abort();

    if(k.proof)k.proof->add_rup(id,p.literals.data(),p.literals.size(),nullptr,0);

    if(p.literals.size()==1){
      k.unit_ids[var_of(p.literals[0])]=id;
      kernel_assign_unit(k,p.literals[0]);
    }

    else if(p.literals.size()==2)kernel_add_binary_clause(k,p.literals[0],p.literals[1],id);

    else kernel_add_large_clause(k,p.literals.data(),p.literals.size(),true,p.literals.size(),id);

    semantic_extension::write_journal_or_abort(r.records,p.journal);

    out.result=ApplyResult::Ok;
    out.emitted_additions=1;
    out.published_clause_id=id;

    out.journal_bytes=p.journal.size();
    out.propagation_pending=!kernel_propagation_fixpoint(k);

    return out;

  }

}
// namespace tactix
