#include "session/learned_clause_export.hpp"
#include <algorithm>

namespace tactix::session {
void LearnedClauseExport::drain(const QueryHistory &history, uint64_t query,
    uint64_t stream, std::span<const DerivationRecord> records, uint64_t dropped,
    const volatile sig_atomic_t *stop, bool (*poll)(void *), void *poll_state) {
  if (!enabled() || draining_) return;
  const auto *input = history.query(query);
  if (!input) return;
  if (snapshot_.query != query) {
    snapshot_ = {}; snapshot_.query = query;
    active_budget_ = budget_; emitted_.clear(); exhausted_ = false;
  }
  if (exhausted_) return;
  if (stream_ != stream) { stream_ = stream; cursor_ = 0; }
  snapshot_.incomplete |= dropped != 0;
  if (!oracle_) oracle_ = std::make_unique<RetainedClauses>(limits_);
  draining_ = true;
  struct Done { bool &flag; ~Done() { flag = false; } } done{draining_};
  for (; cursor_ < records.size(); ++cursor_) {
    const auto &record = records[cursor_];
    if (record.query != query || record.kind == DerivationKind::Input ||
        record.kind == DerivationKind::Delete) continue;
    if ((stop && *stop) || (poll && poll(poll_state))) {
      snapshot_.interrupted = true; return;
    }
    if (snapshot_.ticks >= active_budget_) { snapshot_.incomplete = exhausted_ = true; return; }
    // Include a conservative complete-import allowance even when the private
    // oracle can reuse its prefix. A small explicit budget must not authorize
    // an unbounded first import before RUP's own tick accounting starts.
    const __uint128_t imports = __uint128_t(input->input_words) +
        input->formula_variables + 2*__uint128_t(input->clauses) + oracle_->literals() +
        oracle_->recipe_literals() + 3*__uint128_t(oracle_->clauses().size()) +
        record.clause.size();
    if (imports >= active_budget_-snapshot_.ticks) {
      snapshot_.incomplete = exhausted_ = true; return;
    }
    snapshot_.ticks += uint64_t(imports);
    ++snapshot_.considered;
    // SR/RAT labels are never a license, but an independently F-RUP conclusion
    // is safe regardless of the producer. Helpers and assumption-only names
    // are rejected by the F-only admission service's stable-coordinate checks.
    const uint64_t allowance = active_budget_ - snapshot_.ticks;
    const auto result = record.kind == DerivationKind::ParityReason
        ? oracle_->admit_parity(history, query, record.parity_equations,
                               record.clause, allowance, stop)
        : oracle_->admit(history, query, record.clause, allowance, stop);
    snapshot_.ticks += std::min(result.ticks, UINT64_MAX-snapshot_.ticks);
    if (result.status == RetentionStatus::Interrupted) {
      snapshot_.interrupted = true; return;
    }
    if (result.status == RetentionStatus::Exhausted ||
        result.status == RetentionStatus::Disabled || result.status == RetentionStatus::Stale) {
      snapshot_.incomplete = exhausted_ = true; return;
    }
    if (result.status != RetentionStatus::Accepted &&
        result.status != RetentionStatus::AlreadyPresent) {
      ++snapshot_.rejected; continue;
    }
    ++snapshot_.licensed;
    std::vector<int32_t> key(record.clause.begin(), record.clause.end());
    std::sort(key.begin(), key.end(), external_literal_less);
    key.erase(std::unique(key.begin(), key.end()), key.end());
    if (key.size() > size_t(maximum_length_)) continue;
    if (!emitted_.insert(key).second) continue;
    // The callback owns no pointer into native clauses, proof records or the
    // licensing pool. Its permitted writes die with this temporary C buffer.
    std::vector<int> clause(key.begin(), key.end());
    clause.push_back(0);
    ++snapshot_.exported;
    learn_(state_, clause.data());
  }
}
} // namespace tactix::session
