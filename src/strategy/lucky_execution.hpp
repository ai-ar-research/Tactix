#pragma once

#include "rules/lucky/affine_model.hpp"
#include "rules/lucky/balanced_signature_model.hpp"
#include "rules/lucky/cerny_model.hpp"
#include "rules/lucky/count_preservation.hpp"
#include "rules/lucky/exact_cover_model.hpp"
#include "rules/lucky/exact_one_refutation.hpp"
#include "rules/lucky/fermat_model.hpp"
#include "rules/lucky/independent_set_model.hpp"
#include "rules/lucky/graph_labeling_model.hpp"
#include "rules/lucky/lookup_circuit_model.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/lucky/nol_hypergraph_model.hpp"
#include "rules/lucky/onehot_model.hpp"
#include "rules/lucky/parity_model.hpp"
#include "rules/lucky/parity_refutation.hpp"
#include "rules/lucky/ramsey_startup.hpp"
#include "rules/lucky/signed_cardinality_proof.hpp"
#include "rules/lucky/simon_model.hpp"
#include "rules/lucky/stconn_proof.hpp"

namespace tactix::execution {

// Native opportunities under already established entry/guard conditions.
// Public Lucky instances still own root closure and their frozen payloads.
// Compile-time identities preserve native static dispatch and parameter wires.
enum class LuckyOpportunity : uint8_t {
  Cascade, Parity, Onehot, Affine, DirectCsp, BalancedSignature, Fermat, Cerny,
  LookupCircuit, Simon, Stconn, SignedCardinality, ExactCover, IndependentSet,
  StartupParity, StartupCount, GraphLabeling, StartupRamsey,
  ParityProof, ExactOneProof, NolHypergraph,
};

template <LuckyOpportunity Opportunity> struct LuckyRequest {};

template <LuckyOpportunity Opportunity>
ApplyResult execute_action(Kernel &k, LuckyRequest<Opportunity>) {
  if constexpr (Opportunity == LuckyOpportunity::Cascade)
    return lucky_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::Parity)
    return parity_model_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::Onehot)
    return onehot_model_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::Affine)
    return affine_model_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::DirectCsp)
    return direct_csp_model_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::BalancedSignature)
    return balanced_signature_model_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::Fermat)
    return fermat_model_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::Cerny)
    return cerny_model_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::LookupCircuit)
    return lookup_circuit_model_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::Simon)
    return simon_model_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::Stconn)
    return stconn_proof_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::SignedCardinality)
    return signed_cardinality_proof_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::ExactCover)
    return exact_cover_model_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::IndependentSet)
    return independent_set_model_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::GraphLabeling)
    return graph_labeling_model_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::StartupRamsey)
    return ramsey_startup_action(k);
  else if constexpr (Opportunity == LuckyOpportunity::StartupParity)
    return startup_parity_apply(k);
  else if constexpr (Opportunity == LuckyOpportunity::ParityProof)
    return parity_refutation_apply(
        k, strategy::ParityModelParameters::from_kernel(k));
  else if constexpr (Opportunity == LuckyOpportunity::ExactOneProof)
    return exact_one_refutation_apply(
        k, strategy::OnehotModelParameters::from_kernel(k));
  else if constexpr (Opportunity == LuckyOpportunity::NolHypergraph)
    return nol_hypergraph_model_apply(k);
  else {
    static_assert(Opportunity == LuckyOpportunity::StartupCount);
    return count_preservation_apply(k);
  }
}

struct LuckyProbeRequest {
  uint64_t probe = LUCKY_ALL_TRUE;
};
inline ApplyResult execute_action(Kernel &k, const LuckyProbeRequest &request) {
  return lucky_apply_probe(k, request.probe);
}

// An explicit native onehot proof opportunity, also used inside the complete
// Onehot action. It is not a new registered Lucky payload or public rule ID.
struct ExactOneRefutationRequest {
  strategy::OnehotModelParameters parameters;
};
inline ApplyResult execute_action(Kernel &k,
                                  const ExactOneRefutationRequest &request) {
  return exact_one_refutation_apply(k, request.parameters);
}

} // namespace tactix::execution
