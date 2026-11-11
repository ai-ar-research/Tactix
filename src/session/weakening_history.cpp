#include "session/weakening_history.hpp"

#include "kernel/kernel.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <unordered_set>

namespace tactix::session {
namespace {

uint64_t binary_key(PopulationClauseView clause) {
  const Lit a = internalize(clause.at(0)), b = internalize(clause.at(1));
  return kernel_binary_key(a, b);
}

bool valid(PopulationClauseView view) {
  for (uint32_t i = 0; i < view.size(); ++i)
    if (!view.at(i) || view.at(i) == INT32_MIN) return false;
  return true;
}

bool same(PopulationClauseView view, const std::vector<int32_t> &owned) {
  if (view.size() != owned.size()) return false;
  // Large-clause watch rotations preserve the multiset, not literal order.
  std::vector<int32_t> a, b = owned;
  for (uint32_t i = 0; i < view.size(); ++i) a.push_back(view.at(i));
  std::sort(a.begin(), a.end());
  std::sort(b.begin(), b.end());
  return a == b;
}

// Visit exact stable-external rho segments in reconstruction (reverse) order.
template <class Visit>
bool visit_repair(std::span<const Lit> words, Visit visit) {
  size_t end = words.size();
  while (end) {
    const size_t width = words[end-1];
    if (width) {
      if (width >= end) return false;
      const size_t begin = end-width-1;
      if (!visit(words.subspan(begin, width), words.subspan(end-2, 1), false)) return false;
      end = begin;
    } else {
      if (end < 3) return false;
      const size_t nw = words[end-2], nc = words[end-3];
      if (!nw || !nc || nc > end-3 || nw > end-3-nc) return false;
      const size_t begin = end-3-nc-nw;
      if (!visit(words.subspan(begin, nc), words.subspan(begin+nc, nw), true)) return false;
      end = begin;
    }
  }
  return true;
}

bool valid_repair(const Kernel &k, std::span<const Lit> literals) {
  return std::all_of(literals.begin(), literals.end(), [&](Lit lit) {
    return var_of(lit) < k.external_to_internal.size();
  });
}

std::vector<int32_t> external_words(std::span<const Lit> words) {
  std::vector<int32_t> out;
  out.reserve(words.size());
  for (Lit lit : words) out.push_back(externalize(lit));
  return out;
}

} // namespace

WeakeningHistory::WeakeningHistory(const Kernel &k, WeakeningLimits limits)
    : kernel_(k), limits_(limits), intact_(!k.nvars && !k.next_clause_id &&
                                        k.reconstruction.empty()) {}

bool WeakeningHistory::capture_complete() const noexcept {
  return intact_ && pending_.empty() && transactions_.empty() &&
         reconstruction_end_ == kernel_.reconstruction.size() &&
         population_observer_contains(kernel_.population_observer, this) &&
         kernel_.population_observe_solver;
}

RestorationPlan WeakeningHistory::plan_restoration(
    std::span<const int32_t> used_literals) const noexcept {
  RestorationPlan plan;
  if (!capture_complete() || revision_ == UINT64_MAX) return plan;
  for (int32_t literal : used_literals)
    if (!literal || literal == INT32_MIN) {
      plan.status_ = RestorationPlanStatus::InvalidLiteral;
      return plan;
    }
  try {
    std::unordered_set<int32_t> witnesses, tainted;
    for (const auto &record : records_)
      witnesses.insert(record.witness.begin(), record.witness.end());
    const auto use = [&](int32_t literal) {
      if (witnesses.count(-literal)) tainted.insert(literal);
    };
    for (int32_t literal : used_literals) use(literal);
    std::vector<bool> selected(records_.size(), false);
    for (size_t at = 0; at < records_.size(); ++at) {
      const auto &record = records_[at];
      if (!std::any_of(record.witness.begin(), record.witness.end(),
                      [&](int32_t w) { return tainted.count(-w); })) continue;
      selected[at] = true;
      plan.selected_indices_.push_back(at);
      plan.selected_.push_back(record);
      for (int32_t literal : record.clause) use(literal);
      if (record.family == WeakeningFamily::Cce &&
          !visit_repair(record.repair, [&](auto clause, auto, bool) {
            for (Lit literal : clause) use(externalize(literal));
            return true;
          })) return RestorationPlan{};
    }
    // Zip complete producer groups with the existing rho. A CCE group contains
    // all prefixes for one physical victim; a Condition group contains one cube.
    std::vector<std::pair<size_t, size_t>> spans(records_.size());
    size_t end = kernel_.reconstruction.size();
    for (size_t at = records_.size(); at--;) {
      const auto &record = records_[at];
      if (!record.reconstruction_words || record.reconstruction_words > end) return RestorationPlan{};
      const size_t begin = end-record.reconstruction_words;
      if (!record.repair.empty()) {
        if (record.repair.size() != record.reconstruction_words ||
            !std::equal(record.repair.begin(), record.repair.end(), kernel_.reconstruction.begin()+begin))
          return RestorationPlan{};
      } else if (record.witness.size() != 1 || end-begin < 2 ||
                 kernel_.reconstruction[end-1] != end-begin-1 ||
                 kernel_.reconstruction[end-2] != internalize(record.witness[0])) return RestorationPlan{};
      spans[at] = {begin, end};
      end = begin;
    }
    if (end) return RestorationPlan{};
    for (size_t at = 0; at < records_.size(); ++at)
      if (!selected[at]) {
        ++plan.kept_;
        const auto [begin, finish] = spans[at];
        plan.kept_rho_.insert(plan.kept_rho_.end(), kernel_.reconstruction.begin()+begin,
                             kernel_.reconstruction.begin()+finish);
      }
    plan.before_ = kernel_.reconstruction;
    plan.owner_ = this;
    plan.revision_ = revision_;
    plan.status_ = RestorationPlanStatus::Ready;
    return plan;
  } catch (const std::bad_alloc &) { plan.status_ = RestorationPlanStatus::Exhausted; }
    catch (const std::length_error &) { plan.status_ = RestorationPlanStatus::Exhausted; }
  plan.selected_.clear();
  plan.selected_indices_.clear();
  return plan;
}

bool WeakeningHistory::current(const RestorationPlan &plan) const noexcept {
  return plan.status_ == RestorationPlanStatus::Ready && plan.owner_ == this &&
         plan.revision_ == revision_ && capture_complete() &&
         plan.before_ == kernel_.reconstruction;
}

void WeakeningHistory::consume_restoration(Kernel &k, const RestorationPlan &plan) {
  assert(&k == &kernel_ && current(plan));
  std::vector<WeakeningRecord> kept;
  kept.reserve(plan.kept_);
  size_t selected = 0;
  for (size_t at = 0; at < records_.size(); ++at) {
    if (selected < plan.selected_indices_.size() && plan.selected_indices_[selected] == at) {
      literals_ -= records_[at].clause.size() + records_[at].witness.size() + records_[at].repair.size();
      source_copies_ -= records_[at].sources.size();
      ++selected;
    } else kept.push_back(std::move(records_[at]));
  }
  records_ = std::move(kept);
  k.reconstruction = plan.kept_rho_;
  reconstruction_end_ = k.reconstruction.size();
  ++revision_;
  k.model_extended = false;
  k.model_values.clear();
}

void WeakeningHistory::observe_binding(const Kernel &k, bool bound) noexcept {
  if (!bound || &k != &kernel_ || binding_seen_ || k.nvars || k.next_clause_id ||
      !k.reconstruction.empty() || !k.population_observe_solver) intact_ = false;
  if (bound) binding_seen_ = true;
}

void WeakeningHistory::observe(const SolverPopulationEvent &event) noexcept {
  if (!intact_) return;
  try { observe_solver(event); }
  catch (const std::bad_alloc &) { intact_ = false; }
  catch (const std::length_error &) { intact_ = false; }
}

void WeakeningHistory::observe(const WeakeningPopulationEvent &event) noexcept {
  if (!intact_) return;
  try { observe_weakening(event); }
  catch (const std::bad_alloc &) { intact_ = false; }
  catch (const std::length_error &) { intact_ = false; }
}

void WeakeningHistory::observe_solver(const SolverPopulationEvent &event) {
  if (event.kind == SolverPopulationEventKind::TransactionBegin) {
    if (!event.transaction_id || transactions_.count(event.transaction_id) ||
        transactions_.size() >= limits_.records) intact_ = false;
    else transactions_.emplace(event.transaction_id, std::vector<BinaryDelta>{});
    return;
  }
  if (event.kind == SolverPopulationEventKind::StageAdd ||
      event.kind == SolverPopulationEventKind::StageDelete) {
    auto transaction = transactions_.find(event.transaction_id);
    if (transaction == transactions_.end()) { intact_ = false; return; }
    if (event.clause_kind != SolverClauseKind::Binary) return;
    if (event.clause.size() != 2 || !valid(event.clause) || !event.copies ||
        limits_.literals - literals_ - staged_literals_ < 2) { intact_ = false; return; }
    transaction->second.push_back({event.kind, event.logical_id, event.copies,
                                   {event.clause.at(0), event.clause.at(1)}});
    staged_literals_ += 2;
    return;
  }
  if (event.kind == SolverPopulationEventKind::TransactionCommit) {
    auto transaction = transactions_.find(event.transaction_id);
    if (transaction == transactions_.end()) { intact_ = false; return; }
    for (const auto &delta : transaction->second) {
      SolverPopulationEvent committed;
      committed.kind = delta.kind == SolverPopulationEventKind::StageAdd
                           ? SolverPopulationEventKind::Add : SolverPopulationEventKind::Delete;
      committed.clause_kind = SolverClauseKind::Binary;
      committed.logical_id = delta.id;
      committed.copies = delta.copies;
      committed.clause = PopulationClauseView::external_lits(delta.clause.data(), 2);
      observe_solver(committed);
      staged_literals_ -= 2;
      if (!intact_) break;
    }
    transactions_.erase(transaction);
    return;
  }
  if (event.kind != SolverPopulationEventKind::Add &&
      event.kind != SolverPopulationEventKind::Delete) return;
  if (event.clause_kind != SolverClauseKind::Binary) {
    if (event.kind != SolverPopulationEventKind::Delete) return;
    const auto match = large_pending_.find(event.logical_id);
    if (match == large_pending_.end()) return;
    auto &pending = pending_.at(match->second);
    if (pending.deleted || !same(event.clause, pending.record.clause)) intact_ = false;
    else ++pending.deleted;
    return;
  }
  if (event.clause.size() != 2 || !valid(event.clause) || !event.copies) {
    intact_ = false;
    return;
  }
  const uint64_t key = binary_key(event.clause);
  if (event.kind == SolverPopulationEventKind::Add) {
    if (!event.logical_id || event.copies > limits_.live_binary_copies - live_binary_copies_ ||
        event.copies > UINT64_MAX - occurrence_) {
      intact_ = false;
      return;
    }
    auto &population = binaries_[key];
    // No producer may append a new physical copy beneath an open reservation.
    if (population.reserved) { intact_ = false; return; }
    auto &copies = population.copies;
    for (uint32_t i = 0; i < event.copies; ++i)
      copies.push_back({{++occurrence_, event.logical_id}, 0});
    live_binary_copies_ += event.copies;
    return;
  }
  const auto it = binaries_.find(key);
  if (it == binaries_.end() || it->second.copies.size() < event.copies) {
    intact_ = false;
    return;
  }
  auto &copies = it->second.copies;
  for (uint32_t i = 0; i < event.copies; ++i) {
    const auto source = copies.back();
    if (source.reserved) {
      const auto stage = pending_.find(source.reserved);
      if (stage == pending_.end()) { intact_ = false; return; }
      ++stage->second.deleted;
      --it->second.reserved;
    }
    copies.pop_back();
    --live_binary_copies_;
  }
  if (copies.empty()) binaries_.erase(it);
  else if (copies.size() <= copies.capacity() / 4) {
    // Vector growth doubles capacity. Shrink at the opposite, quarter-full
    // watermark and retain twice the live size: hysteresis prevents repeated
    // add/delete pairs from copying the whole population on every operation.
    // Without reclamation, many once-large duplicate populations can retain
    // quadratic storage despite a fixed bound on currently live copies.
    std::vector<BinaryCopy> compact;
    compact.reserve(2 * copies.size());
    compact.insert(compact.end(), copies.begin(), copies.end());
    copies.swap(compact);
  }
}

void WeakeningHistory::release(uint64_t token) noexcept {
  for (auto &[key, population] : binaries_)
    for (auto &copy : population.copies)
      if (copy.reserved == token) {
        copy.reserved = 0;
        --population.reserved;
      }
}

bool WeakeningHistory::reconstruction_matches(const Pending &stage) const {
  if (stage.end > kernel_.reconstruction.size()) return false;
  if (stage.record.family == WeakeningFamily::Condition || stage.record.family == WeakeningFamily::Cce) {
    const auto words = std::span<const Lit>(kernel_.reconstruction).subspan(stage.begin, stage.end-stage.begin);
    std::vector<int32_t> witnesses;
    size_t segments = 0;
    const bool matched = visit_repair(words, [&](auto clause, auto witness, bool cube) {
      if (!valid_repair(kernel_, clause) || !valid_repair(kernel_, witness)) return false;
      ++segments;
      auto body = external_words(clause), original = stage.record.clause;
      std::sort(body.begin(), body.end());
      std::sort(original.begin(), original.end());
      if (stage.record.family == WeakeningFamily::Condition) {
        return cube && body == original && external_words(witness) == stage.record.witness;
      }
      if (cube || !std::includes(body.begin(), body.end(), original.begin(), original.end()) ||
          std::find(clause.begin(), clause.end(), witness[0]) == clause.end()) return false;
      witnesses.push_back(externalize(witness[0]));
      return true;
    });
    if (!matched || !segments) return false;
    if (stage.record.family == WeakeningFamily::Condition) return segments == 1;
    std::reverse(witnesses.begin(), witnesses.end());
    return witnesses == stage.record.witness;
  }
  if (stage.record.witness.size() != 1 || stage.end > kernel_.reconstruction.size() ||
      stage.end <= stage.begin + 1 ||
      kernel_.reconstruction[stage.end - 1] != stage.end - stage.begin - 1 ||
      kernel_.reconstruction[stage.end - 2] != internalize(stage.record.witness[0])) return false;
  std::vector<int32_t> actual, residual;
  for (uint64_t at = stage.begin; at + 1 < stage.end; ++at) {
    const Lit literal = kernel_.reconstruction[at];
    if (var_of(literal) >= kernel_.external_to_internal.size()) return false;
    actual.push_back(externalize(literal));
  }
  residual = stage.record.clause;
  std::sort(actual.begin(), actual.end());
  std::sort(residual.begin(), residual.end());
  if (!std::includes(residual.begin(), residual.end(), actual.begin(), actual.end())) return false;
  const auto fixed_false = [&](int32_t literal) {
    return kernel_external_value(kernel_, internalize(literal)) < 0;
  };
  if (fixed_false(stage.record.witness[0])) return false;
  // BVE can learn a root while generating resolvents after gathering its
  // source clauses. Its valid repair may retain those now-false literals.
  // Every saved literal must belong to the source; only false ones may be
  // omitted, and the witness must remain available to repair that source.
  std::erase_if(actual, fixed_false);
  std::erase_if(residual, fixed_false);
  return actual == residual;
}

void WeakeningHistory::observe_weakening(const WeakeningPopulationEvent &event) {
  if (event.kind == WeakeningEventKind::Reconstruction) {
    if (event.family != WeakeningFamily::Substitute || !event.transaction_id ||
        !pending_.empty() || !transactions_.empty() ||
        event.reconstruction_offset != reconstruction_end_ || !event.reconstruction_words ||
        reconstruction_end_ > kernel_.reconstruction.size() ||
        event.reconstruction_words != kernel_.reconstruction.size()-reconstruction_end_ ||
        source_copies_ >= limits_.records || occurrence_ == UINT64_MAX || revision_ == UINT64_MAX) {
      intact_ = false;
      return;
    }
    WeakeningRecord record;
    record.order = event.transaction_id;
    record.family = event.family;
    record.derived = true;
    record.reconstruction_words = event.reconstruction_words;
    const auto words = std::span<const Lit>(kernel_.reconstruction).subspan(reconstruction_end_);
    size_t segments = 0;
    const bool matched = visit_repair(words, [&](auto clause, auto witness, bool cube) {
      if (cube || ++segments != 1 || !valid_repair(kernel_, clause) || !valid_repair(kernel_, witness)) return false;
      record.clause = external_words(clause);
      record.witness = external_words(witness);
      return true;
    });
    const uint64_t cost = record.clause.size() + record.witness.size() + words.size();
    if (!matched || segments != 1 || cost > limits_.literals-literals_-staged_literals_) {
      intact_ = false;
      return;
    }
    record.repair.assign(words.begin(), words.end());
    record.sources.push_back({++occurrence_, 0});
    records_.push_back(std::move(record));
    ++source_copies_;
    literals_ += cost;
    reconstruction_end_ = kernel_.reconstruction.size();
    ++revision_;
    return;
  }
  if (event.kind != WeakeningEventKind::Prepare) {
    const auto it = pending_.find(event.transaction_id);
    if (it == pending_.end()) { intact_ = false; return; }
    auto &stage = it->second;
    if (!stage.binary)
      large_pending_.erase(stage.record.sources[0].proof_id);
    if (event.kind == WeakeningEventKind::Cancel) {
      if (stage.deleted || reconstruction_end_ != stage.end ||
          kernel_.reconstruction.size() != stage.begin) intact_ = false;
      else reconstruction_end_ = stage.begin;
      const bool grouped = stage.record.family == WeakeningFamily::Condition ||
                           stage.record.family == WeakeningFamily::Cce;
      literals_ -= stage.record.clause.size() + stage.record.witness.size() +
                   (grouped ? stage.record.reconstruction_words : 0);
      source_copies_ -= stage.record.sources.size();
      release(event.transaction_id);
    } else if (stage.deleted != stage.record.sources.size() ||
               !reconstruction_matches(stage)) intact_ = false;
    else {
      // A batch can prepare several records before deleting their sources.
      // Preserve prepare/reconstruction order even if commits arrive reordered.
      if (stage.record.family == WeakeningFamily::Condition || stage.record.family == WeakeningFamily::Cce)
        stage.record.repair.assign(kernel_.reconstruction.begin()+stage.begin, kernel_.reconstruction.begin()+stage.end);
      const auto position = std::lower_bound(records_.begin(), records_.end(),
          stage.record.order, [](const auto &r, uint64_t order) { return r.order < order; });
      records_.insert(position, std::move(stage.record));
      if (revision_ == UINT64_MAX) intact_ = false;
      else ++revision_;
    }
    pending_.erase(it);
    return;
  }
  const bool grouped = event.family == WeakeningFamily::Condition || event.family == WeakeningFamily::Cce;
  const uint64_t repair_words = grouped ? event.reconstruction_words : 0;
  if (repair_words > UINT64_MAX-uint64_t(event.clause.size())-event.witness.size()) {
    intact_ = false;
    return;
  }
  const uint64_t words = uint64_t(event.clause.size()) + event.witness.size() + repair_words;
  if (!event.transaction_id || pending_.count(event.transaction_id) ||
      !event.clause.size() || !event.witness.size() ||
      !valid(event.clause) || !valid(event.witness) ||
      records_.size() + pending_.size() >= limits_.records ||
      words > limits_.literals - literals_ - staged_literals_ || !transactions_.empty() ||
      event.reconstruction_offset != reconstruction_end_ ||
      !event.reconstruction_words || event.reconstruction_words > UINT64_MAX - reconstruction_end_) {
    intact_ = false;
    return;
  }
  Pending stage;
  stage.binary = event.binary;
  stage.begin = reconstruction_end_;
  stage.end = reconstruction_end_ + event.reconstruction_words;
  auto &record = stage.record;
  record.order = event.transaction_id;
  record.family = event.family;
  record.reconstruction_words = event.reconstruction_words;
  if (grouped) record.repair.reserve(repair_words);
  for (uint32_t i = 0; i < event.clause.size(); ++i) record.clause.push_back(event.clause.at(i));
  for (uint32_t i = 0; i < event.witness.size(); ++i) record.witness.push_back(event.witness.at(i));
  if (event.binary) {
    if (event.clause.size() != 2) { intact_ = false; return; }
    auto it = binaries_.find(binary_key(event.clause));
    if (it == binaries_.end()) { intact_ = false; return; }
    auto &population = it->second;
    const size_t available = population.copies.size() - population.reserved;
    const size_t count = event.all_binary_copies ? available : std::min<size_t>(available, 1);
    if (count > limits_.records - source_copies_) { intact_ = false; return; }
    for (size_t i = 0; i < count; ++i) {
      auto &copy = population.copies[population.copies.size() - population.reserved - 1];
      copy.reserved = event.transaction_id;
      record.sources.push_back(copy.source);
      ++population.reserved;
    }
    if (record.sources.empty()) { intact_ = false; return; }
  } else {
    if (!event.source_id || occurrence_ == UINT64_MAX || large_pending_.count(event.source_id) ||
        source_copies_ == limits_.records) {
      intact_ = false;
      return;
    }
    record.sources.push_back({++occurrence_, event.source_id});
    large_pending_.emplace(event.source_id, event.transaction_id);
  }
  const size_t sources = record.sources.size();
  pending_.emplace(event.transaction_id, std::move(stage));
  source_copies_ += sources;
  reconstruction_end_ += event.reconstruction_words;
  literals_ += static_cast<size_t>(words);
}

} // namespace tactix::session
