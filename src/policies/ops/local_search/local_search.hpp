#pragma once

#include "rules/registry.hpp"
#include "strategy/local_search.hpp"

namespace tactix {

RuleEligibility local_search_guidance_guard(const Kernel &);
ApplyResult local_search_guidance_apply(Kernel &);
ApplyResult local_search_guidance_apply_kind(Kernel &,
                                             strategy::LocalSearchKind);
ApplyResult local_search_guidance_apply_next(
    Kernel &, const std::vector<strategy::LocalSearchKind> &, uint64_t &cursor);

} // namespace tactix
