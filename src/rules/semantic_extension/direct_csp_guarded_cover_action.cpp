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

    // Shared durable identity: preserve the exact established dgc2 spelling.
    bool build_key(const DirectCspGuardedCoverActionSelection &selection,
                   const direct_csp_guarded_cover::Expected &image,
                   const direct_csp_guarded_cover::Charge &charge,
                   std::string &key, std::vector<int32_t> &tuple) {
      namespace detail = direct_csp_guarded_cover::detail;
      if (!charge) return false;
      for (const auto &clause : image.clauses)
        if (!charge(detail::add(1, detail::multiply(16, clause.size()))))
          return false;
      if (!charge(detail::add(8, detail::multiply(selection.domains.size(),
                         detail::add(selection.domains.size(), 16)))))
        return false;
      std::ostringstream text;
      text << "dgc2:";
      for (const auto &clause : image.clauses) {
        for (int32_t literal : clause) text << literal << ',';
        text << ';';
      }
      text << '|';
      std::vector<int32_t> normalized_tuple;
      for (size_t domain = 0; domain < selection.domains.size(); ++domain)
        normalized_tuple.push_back(selection.domains[domain]
                                  [selection.selected_values[domain]]);
      std::sort(normalized_tuple.begin(), normalized_tuple.end());
      for (int32_t literal : normalized_tuple) text << literal << ',';
      std::string candidate_key = text.str();
      if (!charge(candidate_key.size())) return false;
      key = std::move(candidate_key);
      tuple = std::move(normalized_tuple);
      return true;
    }

    // Clause identities may advance for a freshly proved replacement large
    // premise. Literal image, occurrence ordering and binary archive/sentinel
    // identities must remain exact; this never residualizes or remaps sources.
    bool refresh_compatible(const DirectCspGuardedCoverOwnerSnapshot &old,
                            const DirectCspGuardedCoverOwnerSnapshot &fresh,
                            const direct_csp_guarded_cover::Charge &charge) {
      namespace detail = direct_csp_guarded_cover::detail;
      uint64_t shape_work = detail::add(1, detail::add(
          old.selection.cover.size(), detail::multiply(4, old.selection.blockers.size())));
      for (const auto &domain : old.selection.domains)
        shape_work = detail::add(shape_work, detail::add(1, domain.size()));
      for (const auto &definition : old.selection.exact_or)
        shape_work = detail::add(shape_work, detail::add(1, definition.inputs.size()));
      if (!charge(shape_work) ||
          !direct_csp_guarded_cover_owner_shape_equal(old.selection,
                                                    fresh.selection) ||
          old.source_literals != fresh.source_literals ||
          old.source.size() != fresh.source.size()) return false;
      for (size_t index = 0; index < old.source.size(); ++index) {
        const auto &a = old.source[index], &b = fresh.source[index];
        if (!charge(1 + a.literals.size()) ||
            a.occurrence != b.occurrence || a.literals != b.literals ||
            (a.literals.size() < 3 && a.native_proof_id != b.native_proof_id))
          return false;
      }
      return true;
    }

  }

  DirectCspGuardedCoverKeyLookup
  DirectCspGuardedCoverActionSession::lookup_committed(
      const DirectCspGuardedCoverActionSelection &selection,
      const direct_csp_guarded_cover::Bounds &limits,
      direct_csp_guarded_cover::Charge charge) const {
    const auto image = direct_csp_guarded_cover::expected_clauses(
        selection, limits, charge);
    if (image.status != direct_csp_guarded_cover::Status::Planned)
      return image.status == direct_csp_guarded_cover::Status::ResourceExhausted
                 ? DirectCspGuardedCoverKeyLookup::ResourceExhausted
                 : DirectCspGuardedCoverKeyLookup::Invalid;
    std::string key;
    std::vector<int32_t> tuple;
    if (!build_key(selection, image, charge, key, tuple))
      return DirectCspGuardedCoverKeyLookup::ResourceExhausted;
    for (const auto &committed : keys_) {
      if (!charge(1 + committed.size()))
        return DirectCspGuardedCoverKeyLookup::ResourceExhausted;
      if (committed == key) return DirectCspGuardedCoverKeyLookup::Present;
    }
    return DirectCspGuardedCoverKeyLookup::Absent;
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

    std::string key;
    std::vector<int32_t> tuple;
    if (!build_key(s, image, charge, key, tuple)) return exhausted();

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

    // Capture every requested source image freshly. A retained owner may have
    // the same semantic premises under new large-clause proof identities.
    // Stage that refresh only in the detached vector; failed work never
    // modifies the session's authoritative snapshot or committed tuple keys.
    DirectCspGuardedCoverOwnerSnapshot captured;
    const auto capture = direct_csp_guarded_cover_live_capture_owner(k,r,captured);
    out.effort.owner_capture_work=capture.recognition_work;
    out.effort.recognition_work=capture.recognition_work;
    if(capture.status!=DirectCspSupportLiveStatus::Applied){
      out.live.status=retained&&capture.status==DirectCspSupportLiveStatus::NoStructure
                          ?DirectCspSupportLiveStatus::Stale:capture.status;
      out.live.recognition_limit_exhausted=capture.recognition_limit_exhausted;
      out.status=direct_csp_support_action_status(out.live.status);
      return out;
    }
    if(capture.recognition_work>=r.maximum_recognition_work){
      out.status=DirectCspSupportActionStatus::ResourceExhausted;
      out.live.status=DirectCspSupportLiveStatus::ResourceExhausted;
      out.live.recognition_limit_exhausted=true;
      out.effort.session_work=work;
      return out;
    }
    r.maximum_recognition_work-=capture.recognition_work;
    for(const auto &premise:captured.source)
      if(!charge(1+premise.literals.size()))return exhausted();
    if(retained){
      bool comparison_stopped=false;
      auto compare_charge=[&](uint64_t amount){
        if(charge(amount))return true;
        comparison_stopped=true;return false;
      };
      if(!refresh_compatible(*retained,captured,compare_charge)){
        if(comparison_stopped)return exhausted();
        out.status=DirectCspSupportActionStatus::Stale;
        out.live.status=DirectCspSupportLiveStatus::Stale;
        out.effort.session_work=work;
        return out;
      }
      const size_t index=static_cast<size_t>(retained-
          DirectCspGuardedCoverSessionAccess::owners(session).data());
      owners[index]=std::move(captured);
      r.retained_owner=&owners[index];
    }else{
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
    direct_csp_support_action_absorb_live_effort(out,out.live);
    out.status=direct_csp_support_action_status(out.live.status);
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
