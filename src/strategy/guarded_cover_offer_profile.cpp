#include "strategy/guarded_cover_offer_profile.hpp"

#include <array>
#include <charconv>
#include <limits>

namespace tactix::strategy {
bool parse_guarded_cover_offer_limits(std::string_view input,
                                     DirectCspGuardedCoverOfferLimits &out,
                                     std::string *error) {
  const auto reject = [&](const char *reason) {
    if (error) *error = reason;
    return false;
  };
  while (!input.empty() && (input.back() == '\n' || input.back() == '\r'))
    input.remove_suffix(1);
  constexpr std::string_view prefix = "guarded-cover-offer1(";
  if (!input.starts_with(prefix) || input.back() != ')')
    return reject("expected guarded-cover-offer1 with 25 positive limits");
  input.remove_prefix(prefix.size());
  input.remove_suffix(1);
  std::array<uint64_t, 25> values{};
  for (size_t i = 0; i < values.size(); ++i) {
    const size_t comma = input.find(',');
    const auto field = input.substr(0, comma);
    if (field.empty()) return reject("empty guarded-cover producer limit");
    const auto result = std::from_chars(field.data(), field.data() + field.size(), values[i]);
    if (result.ec != std::errc{} || result.ptr != field.data() + field.size() || !values[i])
      return reject("guarded-cover producer limits must be positive unsigned integers");
    if (i + 1 == values.size()) {
      if (comma != std::string_view::npos) return reject("too many guarded-cover producer limits");
    } else {
      if (comma == std::string_view::npos) return reject("too few guarded-cover producer limits");
      input.remove_prefix(comma + 1);
    }
  }
  for (size_t i = 2; i < 8; ++i)
    if (values[i] > std::numeric_limits<uint32_t>::max())
      return reject("guarded-cover theory dimension exceeds uint32 range");
  DirectCspGuardedCoverOfferLimits replacement;
  replacement.theory = {values[0], values[1], uint32_t(values[2]), uint32_t(values[3]),
      uint32_t(values[4]), uint32_t(values[5]), uint32_t(values[6]), uint32_t(values[7])};
  replacement.proposals = {values[8], values[9], values[10], values[11], values[12],
      values[13], values[14]};
  replacement.maximum_work = values[15];
  replacement.maximum_index_clauses = values[16];
  replacement.maximum_index_literals = values[17];
  replacement.maximum_index_bytes = values[18];
  replacement.maximum_cover_candidates = values[19];
  replacement.maximum_role_queries = values[20];
  replacement.maximum_owners = values[21];
  replacement.maximum_offers = values[22];
  replacement.maximum_binary_assignments = values[23];
  replacement.maximum_binary_edges = values[24];
  if (!replacement.valid()) return reject("guarded-cover producer limits are invalid");
  out = replacement;
  if (error) error->clear();
  return true;
}

std::string serialize_guarded_cover_offer_limits(const DirectCspGuardedCoverOfferLimits &p) {
  const std::array<uint64_t, 25> values{
      p.theory.maximum_source_clauses, p.theory.maximum_source_literals,
      p.theory.maximum_domains, p.theory.maximum_domain_values,
      p.theory.maximum_cover_literals, p.theory.maximum_or_definitions,
      p.theory.maximum_or_inputs, p.theory.maximum_output_literals,
      p.proposals.maximum_work, p.proposals.maximum_proposals,
      p.proposals.maximum_single_moves, p.proposals.maximum_pair_moves,
      p.proposals.maximum_repair_steps, p.proposals.maximum_visited_keys,
      p.proposals.maximum_nominations, p.maximum_work, p.maximum_index_clauses,
      p.maximum_index_literals, p.maximum_index_bytes, p.maximum_cover_candidates,
      p.maximum_role_queries, p.maximum_owners, p.maximum_offers,
      p.maximum_binary_assignments, p.maximum_binary_edges};
  std::string text = "guarded-cover-offer1(";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) text += ',';
    text += std::to_string(values[i]);
  }
  return text + ')';
}
} // namespace tactix::strategy
