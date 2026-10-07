#include "rules/semantic_extension/direct_csp_guarded_cover_action.hpp"

#include "rules/semantic_extension/direct_csp_guarded_cover_work.hpp"

#include <algorithm>
#include <sstream>

namespace tactix {

  struct DirectCspGuardedCoverSessionAccess {

    static const auto&keys(const DirectCspGuardedCoverActionSession&s){
      return s.keys_;
    }

    static const auto&owners(const DirectCspGuardedCoverActionSession&s){
      return s.owners_;
    }

    static void commit(DirectCspGuardedCoverActionSession&s,const Kernel&k, std::vector<std::string> keys, std::vector<DirectCspGuardedCoverOwnerSnapshot> owners){

      s.owner_=&k;
      s.keys_.swap(keys);
      s.owners_.swap(owners);

    }

  };

  const DirectCspGuardedCoverOwnerSnapshot*DirectCspGuardedCoverActionSession::snapshot( const DirectCspGuardedCoverActionSelection&selection)const{

    for(const auto&o:owners_)if(direct_csp_guarded_cover_owner_shape_equal(o.selection,selection))return &o;

    return nullptr;

  }

  namespace {

    direct_csp_guarded_cover::Bounds bounds(const DirectCspGuardedCoverActionLimits&l){

      return {
        l.maximum_source_clauses,l.maximum_source_literals,l.maximum_domains,l.maximum_domain_values, l.maximum_cover_literals,l.maximum_or_definitions,l.maximum_or_inputs,l.maximum_output_literals
      };

    }

    DirectCspSupportActionStatus status(DirectCspSupportLiveStatus s){
      switch(s){

        case DirectCspSupportLiveStatus::Applied:return DirectCspSupportActionStatus::Applied;

        case DirectCspSupportLiveStatus::Invalid:return DirectCspSupportActionStatus::Invalid;

        case DirectCspSupportLiveStatus::Inapplicable:return DirectCspSupportActionStatus::Inapplicable;

        case DirectCspSupportLiveStatus::NoStructure:return DirectCspSupportActionStatus::NoStructure;

        case DirectCspSupportLiveStatus::NoConsequence:return DirectCspSupportActionStatus::NoConsequence;

        case DirectCspSupportLiveStatus::Duplicate:return DirectCspSupportActionStatus::AlreadyPublished;

        case DirectCspSupportLiveStatus::ResourceExhausted:return DirectCspSupportActionStatus::ResourceExhausted;

        case DirectCspSupportLiveStatus::Interrupted:return DirectCspSupportActionStatus::Interrupted;

        case DirectCspSupportLiveStatus::UnsupportedProof:return DirectCspSupportActionStatus::UnsupportedProof;

        case DirectCspSupportLiveStatus::Stale:return DirectCspSupportActionStatus::Stale;

      }
      std::abort();
    }

    void copy_effort(DirectCspSupportActionResult&r){
      const auto&v=r.live;
      auto&e=r.effort;

      e.source_clauses=v.source_clauses;
      e.source_literals=v.source_literals;
      e.recognition_work+=v.recognition_work;

      e.source_scan_work=v.source_scan_work;
      e.planning_work=v.planning_work;
      e.revalidation_work=v.revalidation_work;

      e.proof_work=v.proof_work;
      e.planned_additions=v.planned_additions;
      e.emitted_additions=v.emitted_additions;

      e.proof_literal_occurrences=v.proof_literal_occurrences;
      e.journal_work=v.journal_work;
      e.journal_bytes=v.journal_bytes;
    }

  }

  bool DirectCspGuardedCoverActionLimits::valid()const{
    return bounds(*this).valid()&&maximum_recognition_work&& maximum_proof_work&&maximum_proof_additions&&maximum_proof_literal_occurrences&&maximum_journal_bytes&& maximum_external_extent&&maximum_session_keys&&maximum_session_work;
  }

  RuleEligibility direct_csp_guarded_cover_action_guard(const Kernel&k){
    return direct_csp_support_action_guard(k);
  }

  DirectCspGuardedCoverActionResult direct_csp_guarded_cover_action_apply(Kernel&k, DirectCspGuardedCoverActionSession&session,const DirectCspGuardedCoverActionLimits&l, const DirectCspGuardedCoverActionSelection&s){

    DirectCspGuardedCoverActionResult out;
    if(!l.valid())return out;

    uint64_t work=0;
    bool interrupted=false;

    auto charge=[&](uint64_t n){
      if(kernel_terminating(k)){
        interrupted=true;
        return false;
      }

      if(n>l.maximum_session_work-work){
        return false;
      }

      work+=n;
      return true;
    };

    auto exhausted=[&](){
      out.status=interrupted?DirectCspSupportActionStatus::Interrupted:DirectCspSupportActionStatus::ResourceExhausted;

      out.live.status=interrupted?DirectCspSupportLiveStatus::Interrupted:DirectCspSupportLiveStatus::ResourceExhausted;
      out.effort.session_work=work;
      return out;
    };

    auto image=direct_csp_guarded_cover::expected_clauses(s,bounds(l),charge);

    if(image.status!=direct_csp_guarded_cover::Status::Planned){

      if(image.status==direct_csp_guarded_cover::Status::ResourceExhausted){
        return exhausted();
      }

      return out;
    }

    if(session.retained()&&!session.belongs_to(k)){
      out.status=DirectCspSupportActionStatus::Stale;
      out.live.status=DirectCspSupportLiveStatus::Stale;
      return out;
    }

    if(!direct_csp_guarded_cover_action_guard(k)){
      out.status=DirectCspSupportActionStatus::Inapplicable;
      out.live.status=DirectCspSupportLiveStatus::Inapplicable;
      return out;
    }

    // Precharge worst-case text conversion, tuple normalization and copies.
    uint64_t key_work = 8;

    for (const auto &clause : image.clauses) {

      if (!charge(direct_csp_guarded_cover::detail::add(1, direct_csp_guarded_cover::detail::multiply(16, clause.size())))) return exhausted();

    }

    if (!charge(direct_csp_guarded_cover::detail::add(key_work, direct_csp_guarded_cover::detail::multiply(s.domains.size(), direct_csp_guarded_cover::detail::add(s.domains.size(), 16))))) return exhausted();

    std::ostringstream text;
    text<<"dgc2:";

    for(const auto&c:image.clauses){
      for(int32_t x:c)text<<x<<',';
      text<<';';
    }

    text<<'|';
    std::vector<int32_t> tuple;
    for(size_t d=0;d<s.domains.size();++d)tuple.push_back(s.domains[d][s.selected_values[d]]);

    std::sort(tuple.begin(),tuple.end());
    for(int32_t x:tuple)text<<x<<',';
    const std::string key=text.str();

    if(!charge(key.size()))return exhausted();

    for(const auto&old:DirectCspGuardedCoverSessionAccess::keys(session)){

      if(!charge(1+old.size()))return exhausted();

      if(old==key){
        out.status=DirectCspSupportActionStatus::AlreadyPublished;
        out.live.status=DirectCspSupportLiveStatus::Duplicate;

        out.committed_keys=session.committed_keys();
        out.retained_owners=session.retained_owners();
        out.owner_retained=true;
        out.effort.session_work=work;
        return out;
      }
    }

    if(session.committed_keys()>=l.maximum_session_keys)return exhausted();

    for (const auto &old : DirectCspGuardedCoverSessionAccess::keys(session)) if (!charge(1 + old.size())) return exhausted();

    std::vector<std::string> keys=DirectCspGuardedCoverSessionAccess::keys(session);
    keys.push_back(key);

    for(const auto&o:DirectCspGuardedCoverSessionAccess::owners(session)){

      for(const auto&p:o.source)if(!charge(1+p.literals.size()))return exhausted();
    }

    std::vector<DirectCspGuardedCoverOwnerSnapshot> owners=DirectCspGuardedCoverSessionAccess::owners(session);

    DirectCspGuardedCoverLiveRequest r;
    r.selection=s;
    r.bounds=bounds(l);
    r.maximum_recognition_work=l.maximum_recognition_work;

    r.maximum_proof_work=l.maximum_proof_work;
    r.maximum_proof_additions=l.maximum_proof_additions;

    r.maximum_proof_literal_occurrences=l.maximum_proof_literal_occurrences;
    r.maximum_journal_bytes=l.maximum_journal_bytes;

    r.maximum_external_extent=l.maximum_external_extent;
    r.action_id=k.structural_action_sequence+1;
    r.semantic_id=r.action_id;
    r.records=k.structural_provenance;

    out.proposed_action_id=r.action_id;
    out.semantic_id=r.semantic_id;

    const DirectCspGuardedCoverOwnerSnapshot *retained = nullptr;

    for (const auto &owner : DirectCspGuardedCoverSessionAccess::owners(session)) {

      uint64_t shape_work = direct_csp_guarded_cover::detail::add(1, direct_csp_guarded_cover::detail::add(owner.selection.cover.size(), direct_csp_guarded_cover::detail::multiply(owner.selection.blockers.size(), 4)));

      for (const auto &domain : owner.selection.domains) shape_work = direct_csp_guarded_cover::detail::add(shape_work, direct_csp_guarded_cover::detail::add(1, domain.size()));

      for (const auto &definition : owner.selection.exact_or) shape_work = direct_csp_guarded_cover::detail::add(shape_work, direct_csp_guarded_cover::detail::add(1, definition.inputs.size()));

      if (!charge(shape_work)) return exhausted();

      if (direct_csp_guarded_cover_owner_shape_equal(owner.selection, s)) {

        retained = &owner;

        break;

      }

    }

    out.owner_retained=retained!=nullptr;

    if(retained)r.retained_owner=retained;

    else {

      DirectCspGuardedCoverOwnerSnapshot captured;
      auto capture=direct_csp_guarded_cover_live_capture_owner(k,r,captured);

      out.effort.owner_capture_work=capture.recognition_work;
      out.effort.recognition_work=capture.recognition_work;

      if(capture.status!=DirectCspSupportLiveStatus::Applied){
        out.live.status=capture.status;
        out.live.recognition_limit_exhausted=capture.recognition_limit_exhausted;
        out.status=status(capture.status);
        return out;
      }

      if (capture.recognition_work >= r.maximum_recognition_work) {

        out.status = DirectCspSupportActionStatus::ResourceExhausted;

        out.live.status = DirectCspSupportLiveStatus::ResourceExhausted;

        out.live.recognition_limit_exhausted = true;

        out.live.recognition_work = capture.recognition_work;

        out.effort.session_work = work;

        return out;

      }

      r.maximum_recognition_work-=capture.recognition_work;

      for(const auto&p:captured.source)if(!charge(1+p.literals.size()))return exhausted();

      owners.push_back(std::move(captured));
      r.retained_owner=&owners.back();

    }

    // All allocation for advice precedes irreversible proof/clause publication.
    if (!charge(tuple.size())) return exhausted();

    out.semantic_variables.reserve(tuple.size());

    for (int32_t x : tuple) {

      uint32_t v = uint32_t(x < 0 ? -int64_t(x) : x);

      out.semantic_variables.push_back({v, strategy::SemanticVariableRole::ExistingTheory, 1});

    }

    out.effort.session_work=work;
    out.live=direct_csp_guarded_cover_live_apply(k,r);
    copy_effort(out);
    out.status=status(out.live.status);
    out.result=out.live.result;

    if(out.live.status!=DirectCspSupportLiveStatus::Applied){
      out.semantic_variables.clear();
      return out;
    }

    DirectCspGuardedCoverSessionAccess::commit(session,k,std::move(keys),std::move(owners));

    out.owner_retained=true;
    out.committed_keys=session.committed_keys();
    out.retained_owners=session.retained_owners();

    out.committed_action_id=out.proposed_action_id;
    k.structural_action_sequence=out.proposed_action_id;

    out.semantic_candidate_ready=!out.semantic_variables.empty();
    return out;

  }

}
// namespace tactix
