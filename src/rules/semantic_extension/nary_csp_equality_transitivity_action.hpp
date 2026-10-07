#pragma once
#include "rules/semantic_extension/nary_csp_equality_transitivity.hpp"
#include "rules/semantic_extension/nary_csp_equality_live.hpp"
#include "rules/semantic_extension/nary_csp_prefix_action.hpp"
#include <array>
namespace tactix {
struct NaryCspEqualityTransitivitySelection {
  equality_transitivity::Domains domains;
  // Exact requested AB, AD, BD owner identities, with expected coordinates.
  std::array<NaryCspEqualityKey,3> equalities;
  std::array<uint32_t,3> equality_variables{};
  bool operator==(const NaryCspEqualityTransitivitySelection &) const = default;
  bool operator<(const NaryCspEqualityTransitivitySelection &o) const {
    if(domains!=o.domains)return domains<o.domains;
    if(equalities!=o.equalities)return equalities<o.equalities;
    return equality_variables<o.equality_variables;
  }
};
using NaryCspEqualityTransitivityKey=NaryCspEqualityTransitivitySelection;
struct NaryCspEqualityTransitivityLimits {
  uint64_t maximum_source_clauses=0,maximum_source_literals=0;
  uint64_t maximum_recognition_work=0,maximum_planning_work=0;
  uint32_t maximum_domain_values=0;
  uint64_t maximum_forbidden_tuples=0;
  uint64_t maximum_proof_work=0,maximum_proof_additions=0;
  uint64_t maximum_proof_literal_occurrences=0,maximum_journal_bytes=0;
  uint32_t maximum_external_extent=0;
  uint64_t maximum_session_keys=0,maximum_session_bytes=0;
  uint64_t maximum_arena_words=0,maximum_watch_words=0;
  uint64_t maximum_preparation_bytes=0;
  bool valid() const;
};
class NaryCspEqualityTransitivitySession {
public:
  bool retained() const{return owner_!=nullptr;}
  bool belongs_to(const Kernel &k)const{return owner_==&k;}
  const auto &keys()const{return keys_;}
private:
  friend struct NaryCspEqualityTransitivitySessionAccess;
  const Kernel *owner_=nullptr;
  std::vector<NaryCspEqualityTransitivityKey> keys_;
};
enum class NaryCspEqualityTransitivityStatus:uint8_t {
  Invalid,Inapplicable,MissingEquality,Stale,Duplicate,ResourceExhausted,
  Interrupted,UnsupportedProof,Applied,Ready
};
struct NaryCspEqualityTransitivitySource {
  std::vector<int32_t> literals;
  uint64_t native_proof_id=0;
  // Binary presence sentinel is NOT a unique proof occurrence identity.
  bool exact_proof_id=false;
};
struct NaryCspEqualityTransitivityResult {
  NaryCspEqualityTransitivityStatus status=NaryCspEqualityTransitivityStatus::Invalid;
  ApplyResult result=ApplyResult::NoOp;
  uint64_t proposed_action_id=0,committed_action_id=0;
  uint64_t recognition_work=0,planning_work=0,preparation_work=0;
  uint64_t proof_work=0,proof_additions=0,proof_deletions=0;
  uint64_t proof_literal_occurrences=0,journal_bytes=0;
  uint64_t source_clauses=0,source_literals=0,session_bytes=0;
  bool propagation_pending=false;
  std::vector<NaryCspEqualityTransitivitySource> sources;
  std::array<uint64_t,3> published_clause_ids{};
  std::vector<std::vector<int32_t>> published_consequences;
};
RuleEligibility nary_csp_equality_transitivity_guard(const Kernel &);
bool nary_csp_equality_transitivity_selection_valid(const NaryCspEqualityTransitivitySelection &);
NaryCspEqualityTransitivityResult nary_csp_equality_transitivity_apply(
    Kernel &,const NaryCspPrefixActionSession &,const NaryCspEqualitySession &,
    NaryCspEqualityTransitivitySession &,const NaryCspEqualityTransitivityLimits &,
    const NaryCspEqualityTransitivitySelection &,FILE *records);
// Caller-bounded deterministic record triples; no native installation implied.
struct NaryCspEqualityTransitivityOfferWindow {
  uint64_t maximum_candidates=0,maximum_offers=0,maximum_enumeration_work=0;
  bool have_after=false;
  NaryCspEqualityTransitivityKey after;
};
struct NaryCspEqualityTransitivityOfferResult {
  NaryCspEqualityTransitivityStatus status=NaryCspEqualityTransitivityStatus::Invalid;
  uint64_t candidates=0,enumeration_work=0;
  bool complete=false,have_resume_after=false;
  NaryCspEqualityTransitivityKey resume_after;
  std::vector<NaryCspEqualityTransitivitySelection> offers;
};
NaryCspEqualityTransitivityOfferResult nary_csp_equality_transitivity_offer(
    Kernel &,const NaryCspEqualitySession &,const NaryCspEqualityTransitivitySession &,
    const NaryCspEqualityTransitivityLimits &,const NaryCspEqualityTransitivityOfferWindow &);
} // namespace tactix
