#pragma once

#include "rules/semantic_extension/direct_csp_guarded_cover_offer.hpp"
#include <string>
#include <string_view>

namespace tactix::strategy {
// Versioned producer limits are distinct from the certified action profile.
// Parsing replaces the destination only after every positive limit is valid.
bool parse_guarded_cover_offer_limits(std::string_view,
                                     DirectCspGuardedCoverOfferLimits &,
                                     std::string *error = nullptr);
std::string serialize_guarded_cover_offer_limits(
    const DirectCspGuardedCoverOfferLimits &);
} // namespace tactix::strategy
