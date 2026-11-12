#include "session/derivation_history.hpp"
#include "proof/proof.hpp"

#include <new>
#include <stdexcept>

namespace tactix::session {
namespace {
bool copy(PopulationClauseView view, std::vector<int32_t> &to) {
  to.reserve(view.size());
  for (uint32_t at = 0; at < view.size(); ++at) {
    const int32_t literal = view.at(at);
    if (!literal || literal == INT32_MIN) return false;
    to.push_back(literal);
  }
  return true;
}
} // namespace

void DerivationHistory::observe(const DerivationEvent &event) noexcept {
  try { capture(event); }
  catch (const std::bad_alloc &) { if (dropped_ != UINT64_MAX) ++dropped_; }
  catch (const std::length_error &) { if (dropped_ != UINT64_MAX) ++dropped_; }
}

void DerivationHistory::capture(const DerivationEvent &event) {
  const auto drop = [&] { if (dropped_ != UINT64_MAX) ++dropped_; };
  uint64_t words = uint64_t(event.clause.size()) + event.witness.size();
  if (!query_ || records_.size() >= maximum_records_ ||
      event.mappings.size() > (UINT64_MAX-words)/2 ||
      words + 2*event.mappings.size() > maximum_literals_-literals_) {
    drop();
    return;
  }
  words += 2*event.mappings.size();
  for (const auto &row : event.parity_equations) {
    if (words>=maximum_literals_-literals_ ||
        row.variables.size()>maximum_literals_-literals_-words-1) { drop(); return; }
    words += row.variables.size()+1;
  }
  DerivationRecord record;
  record.kind = event.kind;
  record.query = query_;
  record.id = event.id;
  record.source_id = event.source_id;
  record.external_variables = event.external_variables;
  record.propagates = event.propagates;
  record.parity_equations.assign(event.parity_equations.begin(),event.parity_equations.end());
  if (!copy(event.clause, record.clause) || !copy(event.witness, record.witness)) {
    drop();
    return;
  }
  record.mappings.reserve(event.mappings.size());
  for (const auto &mapping : event.mappings) {
    if (mapping.from == INVALID_LIT || mapping.to == INVALID_LIT) { drop(); return; }
    const int32_t from = PopulationClauseView::internal_lits(&mapping.from, 1, event.variable_map).at(0);
    const int32_t to = PopulationClauseView::internal_lits(&mapping.to, 1, event.variable_map).at(0);
    if (!from || from == INT32_MIN || !to || to == INT32_MIN) { drop(); return; }
    record.mappings.emplace_back(from, to);
  }
  records_.push_back(std::move(record));
  literals_ += words;
}
} // namespace tactix::session
