#pragma once

#include "rules/direct_php/direct_php_action.hpp"
#include "rules/lucky/tseitin_cycle_action.hpp"
#include "rules/semantic_extension/affine_basis_action.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include "rules/semantic_extension/direct_csp_guarded_cover_action.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/exact_cover_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"
#include "rules/semantic_extension/gf3_general_row_action.hpp"
#include "rules/semantic_extension/independent_set_cover_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_amo_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_transitivity_action.hpp"
#include "rules/semantic_extension/nary_csp_relation_owner.hpp"

namespace tactix {

struct StructuralActionSessions {
  AffineBasisActionSession affine_basis;
  AffineMatrixSearchSession affine_matrix_search;
  ExactCoverExtensionActionSession exact_cover_extension;
  IndependentSetCoverActionSession independent_set_cover;
  Gf3AffineActionSession gf3_affine;
  DirectCspSupportActionSession direct_csp_support;
  DirectCspGuardedCoverActionSession direct_csp_guarded_cover;
  NaryCspRelationOwnerSession nary_csp_relation_owner;
  NaryCspEqualitySession nary_csp_equality;
  NaryCspEqualityTransitivitySession nary_csp_equality_transitivity;
  NaryCspEqualityAmoSession nary_csp_equality_amo;
  DirectPhpActionSession direct_php;
  Gf3GaussianActionSession gf3_gaussian;
  Gf3GeneralRowActionSession gf3_general_row;
  TseitinCycleBreakingSession tseitin_cycle_breaking;

  explicit StructuralActionSessions(Kernel &kernel) : direct_php(kernel) {
    kernel.affine_basis_action_session = &affine_basis;
    kernel.affine_matrix_search_session = &affine_matrix_search;
    kernel.exact_cover_extension_action_session = &exact_cover_extension;
    kernel.independent_set_cover_action_session = &independent_set_cover;
    kernel.gf3_affine_action_session = &gf3_affine;
    kernel.direct_csp_support_action_session = &direct_csp_support;
    kernel.direct_csp_guarded_cover_action_session = &direct_csp_guarded_cover;
    kernel.nary_csp_relation_owner_session = &nary_csp_relation_owner;
    kernel.nary_csp_equality_session = &nary_csp_equality;
    kernel.nary_csp_equality_transitivity_session =
        &nary_csp_equality_transitivity;
    kernel.nary_csp_equality_amo_session = &nary_csp_equality_amo;
    kernel.direct_php_action_session = &direct_php;
    kernel.gf3_gaussian_action_session = &gf3_gaussian;
    kernel.gf3_general_row_action_session = &gf3_general_row;
    kernel.tseitin_cycle_breaking_session = &tseitin_cycle_breaking;
  }
};

} // namespace tactix
