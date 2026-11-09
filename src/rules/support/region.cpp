#include "rules/support/region.hpp"
#include "kernel/kernel.hpp"
#include "rules/instance_helpers.hpp"
#include "rules/structural_action_guard.hpp"
#include <algorithm>
#include <climits>
#include <cstdlib>

namespace tactix {
RegionSession::RegionSession(Kernel &k)
    : owner_(k), previous_(k.population_observer),
      probe_(tactix_rule_id("Probe")), sweep_(tactix_rule_id("Sweep")), binding_(k, *this) {
  binding_epoch_ = k.population_binding_epoch;
}

void RegionSession::invalidate() {
  if (valid_) { valid_ = false; ++owner_.stats.region_invalidated; }
}
void RegionSession::observe(const RawInputPopulationEvent &e) noexcept {
  if (previous_) previous_->observe(e);
}
void RegionSession::observe(const CheckerPopulationEvent &e) noexcept {
  if (previous_) previous_->observe(e);
}
void RegionSession::observe(const SolverReplacementEvent &e) noexcept {
  if (e.phase == SolverReplacementPhase::Begin) invalidate();
  if (previous_) previous_->observe(e);
}
void RegionSession::observe(const SolverPopulationEvent &e) noexcept {
  if (e.kind == SolverPopulationEventKind::Delete ||
      e.kind == SolverPopulationEventKind::StageDelete ||
      e.kind == SolverPopulationEventKind::MetadataUpdate) invalidate();
  if (capturing_ && valid_ && e.kind == SolverPopulationEventKind::Add) {
    if (e.clause.size() < 2 || e.clause.size() > 3 ||
        clauses_.size() == UINT32_MAX) invalidate();
    else {
      std::array<int32_t, 3> clause{};
      for (uint32_t i = 0; i < e.clause.size(); ++i) clause[i] = e.clause.at(i);
      clauses_.push_back(clause);
    }
  }
  if (previous_) previous_->observe(e);
}
void RegionSession::begin_definition() {
  if (owner_.population_binding_epoch != binding_epoch_ ||
      binding_epoch_ == UINT64_MAX || owner_.population_observer != this)
    invalidate();
  capturing_ = true;
  capture_begin_ = static_cast<uint32_t>(clauses_.size());
}
void RegionSession::end_definition(int output) {
  capturing_ = false;
  if (output > 0 && valid_)
    definitions_.push_back({output, {capture_begin_,
                                    static_cast<uint32_t>(clauses_.size())}});
  else clauses_.resize(capture_begin_);
}
const RegionSession::Definition *RegionSession::definition(int output) const {
  if (definitions_.empty() || output < definitions_.front().output) return nullptr;
  // ER-only programs have contiguous outputs. Other fresh-variable producers
  // can leave gaps; retain a sparse logarithmic fallback, never pad the gap.
  const size_t direct = size_t(output - definitions_.front().output);
  if (direct < definitions_.size() && definitions_[direct].output == output)
    return &definitions_[direct];
  const auto it = std::lower_bound(definitions_.begin(), definitions_.end(), output,
      [](const Definition &d, int x) { return d.output < x; });
  return it != definitions_.end() && it->output == output ? &*it : nullptr;
}
int RegionSession::add_region(std::span<const int32_t> input) {
  if (regions_.size() >= INT_MAX || input.size() > UINT32_MAX - vertices_.size())
    return -1;
  for (int x : input) {
    if (x <= 0 || size_t(x) > owner_.external_to_internal.size()) return -1;
    const Lit l = kernel_internalize(owner_, x);
    if (l == INVALID_LIT && !kernel_external_value(owner_, internalize(x))) return -1;
  }
  const auto begin = static_cast<uint32_t>(vertices_.size());
  vertices_.insert(vertices_.end(), input.begin(), input.end());
  std::sort(vertices_.begin() + begin, vertices_.end());
  vertices_.erase(std::unique(vertices_.begin() + begin, vertices_.end()), vertices_.end());
  regions_.push_back({begin, static_cast<uint32_t>(vertices_.size())});
  return static_cast<int>(regions_.size() - 1);
}
bool RegionSession::valid_query(const RegionQuery &q) const {
  if ((q.rule != probe_ && q.rule != sweep_) || !q.literal ||
      q.literal == INT_MIN || q.region >= regions_.size() ||
      (q.rule == probe_ && q.ticks))
    return false;
  const auto r = regions_[q.region];
  return std::binary_search(vertices_.begin() + r.begin,
                            vertices_.begin() + r.end, std::abs(q.literal));
}
bool RegionSession::prepare(const RegionQuery &q) {
  auto &k = owner_;
  if (!++epoch_) { std::fill(stamps_.begin(), stamps_.end(), 0); ++epoch_; }
  stamps_.resize(k.nvars); local_.resize(k.nvars); mapped_.clear();
  const auto r = regions_[q.region];
  for (uint32_t at = r.begin; at < r.end; ++at) {
    const int x = vertices_[at]; const Lit l = kernel_internalize(k, x);
    if (l == INVALID_LIT) {
      if (!kernel_external_value(k, internalize(x))) return false;
      mapped_.push_back(INVALID_VAR);
    } else {
      const Var v = var_of(l);
      if (kernel_var_retired(k, v) && !kernel_root_value(k, l)) return false;
      local_[v] = static_cast<uint32_t>(mapped_.size()); stamps_[v] = epoch_;
      mapped_.push_back(v);
    }
  }
  return true;
}
bool RegionSession::compile(const RegionQuery &q) {
  auto &k = owner_;
  solver_.reset(static_cast<uint32_t>(mapped_.size())); work_ = mapped_.size();
  const auto add = [&](const Lit *lits, uint32_t count) {
    scratch_.clear();
    for (uint32_t i = 0; i < count; ++i) {
      ++work_; const Lit l = lits[i]; const int root = kernel_root_value(k, l);
      if (root > 0) return;
      if (root < 0) continue;
      const Var v = var_of(l);
      if (v >= stamps_.size() || stamps_[v] != epoch_) return;
      scratch_.push_back(sub_lit(local_[v], negated(l)));
    }
    solver_.add_clause(scratch_);
  };
  const auto r = regions_[q.region];
  if (k.population_binding_epoch != binding_epoch_ ||
      binding_epoch_ == UINT64_MAX || k.population_observer != this)
    invalidate();
  if (!valid_) return false;
  for (uint32_t at = r.begin; at < r.end; ++at) {
    const Definition *d = definition(vertices_[at]); if (!d) continue;
    for (uint32_t ci = d->clauses.begin; ci < d->clauses.end; ++ci) {
      Lit body[3]; uint32_t count = 0; bool skip = false;
      for (int x : clauses_[ci]) {
        if (!x) break;
        const Lit l = kernel_internalize(k, x);
        if (l == INVALID_LIT) {
          const int root = kernel_external_value(k, internalize(x));
          if (root < 0) continue;
          skip = true; break;
        }
        body[count++] = l;
      }
      if (!skip) add(body, count);
    }
  }
  return true;
}
ApplyResult RegionSession::unit(Lit u) {
  auto &k = owner_; const uint64_t id = ++k.next_clause_id;
  if (k.proof) k.proof->add_rup(id, &u, 1);
  ++k.stats.region_units; ++k.stats.region_lemmas;
  assert(!k.value(u));
  k.unit_ids[var_of(u)] = id;
  kernel_assign_unit(k, u);
  return ApplyResult::Ok;
}
ApplyResult RegionSession::infer(const RegionQuery &q) {
  auto &k = owner_; const Lit p = kernel_internalize(k, q.literal);
  if (p == INVALID_LIT || kernel_var_retired(k, var_of(p)) || k.value(p) ||
      !prepare(q)) return ApplyResult::NoOp;
  if (q.rule == probe_) {
    ++k.stats.region_queries; ++k.stats.probed;
    const uint64_t ticks = k.stats.search_ticks;
    kernel_probing_begin(k, false); kernel_assign_decision(k, p);
    const bool conflict = kernel_propagate_region(k, {stamps_, epoch_});
    probe_unwind(k);
    k.stats.region_ticks += k.stats.search_ticks - ticks;
    if (!conflict) return ApplyResult::NoOp;
    ++k.stats.probe_failed; return unit(negate(p));
  }
  if (!compile(q)) return ApplyResult::NoOp;
  ++k.stats.region_queries;
  const SubLit assumption = sub_lit(local_[var_of(p)], negated(p));
  const auto result = solver_.solve_interruptible(
      {assumption}, q.ticks, SubTickMode::Entries, k.stop);
  k.stats.region_ticks += work_ + solver_.ticks();
  kernel_charge_ticks(k, work_ + solver_.ticks());
  if (result != SubResult::Unsat) {
    if (result == SubResult::Budget) ++k.stats.region_budget;
    return ApplyResult::NoOp;
  }
  SubDerivation proof;
  if (!solver_.derivation(proof) || proof.steps.empty()) return ApplyResult::NoOp;
  const auto &target = proof.steps.back().lits;
  if (target.size() > 1 || (!target.empty() && target[0] != sub_negate(assumption)) ||
      proof.steps.size() + 1 > uint64_t(INT64_MAX) - k.next_clause_id)
    return ApplyResult::NoOp;
  for (const auto &step : proof.steps)
    for (SubLit l : step.lits)
      if (sub_var_of(l) >= mapped_.size() || mapped_[sub_var_of(l)] == INVALID_VAR)
        return ApplyResult::NoOp;
  const bool terminal = target.empty();
  const Lit goal = negate(p);
  std::vector<TransientLemma> transients;
  std::vector<Lit> clause;
  for (size_t i = 0; i + 1 < proof.steps.size(); ++i) {
    clause.clear();
    for (SubLit l : proof.steps[i].lits)
      clause.push_back(make_lit(mapped_[sub_var_of(l)], l & 1));
    // Match Sweep: proof-only intermediates must not teach the checker
    // persistent root facts absent from the kernel before a later SR step.
    if (!terminal) {
      if (std::find(clause.begin(), clause.end(), p) != clause.end()) continue;
      if (std::find(clause.begin(), clause.end(), goal) == clause.end())
        clause.push_back(goal);
    }
    const uint64_t id = ++k.next_clause_id;
    emit_rup_lemma(k, id, clause);
    ++k.stats.region_lemmas;
    if (!terminal) transients.push_back({id, clause});
  }
  if (terminal) {
    k.inconsistent = true; ++k.stats.conflicts; kernel_emit_empty(k);
    return ApplyResult::Unsat;
  }
  const auto committed = unit(goal);
  delete_transients(k, transients);
  return committed;
}
ApplyResult RegionSession::apply(const RegionQuery &q) {
  if (!valid_query(q) || !root_action_guard(owner_, false).clausal_certificate()
        .fact("proof-id-available", owner_.next_clause_id <= uint64_t(INT64_MAX)-2)
        .fact("not-terminated", !kernel_terminating(owner_)).finish())
    return ApplyResult::NoOp;
  return apply_rule_structured(tactix_rules()[q.rule], owner_, "region",
                               [&] { return infer(q); });
}
} // namespace tactix
