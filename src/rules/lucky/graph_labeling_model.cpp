#include "rules/lucky/graph_labeling_model.hpp"

#include "rules/lucky/graph_labeling_completion.hpp"
#include "rules/lucky/graph_labeling_repair.hpp"
#include "strategy/rule_parameters.hpp"

namespace tactix {

RuleEligibility graph_labeling_model_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("no-conflict", !k.conflict_valid)
      .fact("no-pending", !k.pending.valid)
      .fact("root-level", !k.level)
      .fact("propagated", kernel_propagation_fixpoint(k))
      .fact("sparse-mode", !k.dense)
      .fact("no-probe-transaction", !k.probing.recording)
      .fact("large-watches-connected", k.large_watches_connected);
}

ApplyResult graph_labeling_model_apply(Kernel &k) {
  using namespace graph_labeling;
  using Stage = strategy::GraphLabelingStage;
  ++k.stats.graph_labeling_calls;
  const auto p = strategy::GraphLabelingParameters::from_kernel(k);
  if (!p.valid() || !graph_labeling_model_guard(k)) return ApplyResult::NoOp;
  uint64_t ceiling = 0;
  for (uint64_t allowance : p.work) ceiling += allowance;
  if (kernel_has_work_budget(k)) ceiling = std::min(ceiling, k.work_budget);
  // Reserve publication allowance, not work: it is charged only if performed.
  // A bounded unsuccessful search can still expose its complete best advice.
  const uint64_t reserve = std::min(ceiling, p.work[size_t(Stage::Publication)]);
  uint64_t used = 0;
  bool interrupted = false;
  const auto phase = [&](Stage stage) {
    const uint64_t cap = p.work[size_t(stage)];
    const uint64_t phase_start = used;
    const uint64_t end = stage == Stage::Publication ? ceiling : ceiling - reserve;
    bool exhausted = false;
    return [&, cap, phase_start, end, exhausted](uint64_t amount) mutable {
      if (exhausted || interrupted) return false;
      if (kernel_rule_interrupt(k, k.stats.graph_labeling_interrupted)) {
        interrupted = true;
        return false;
      }
      if (amount > cap - (used - phase_start) || amount > end - used) {
        exhausted = true;
        ++k.stats.graph_labeling_budget;
        return false;
      }
      used += amount;
      k.stats.graph_labeling_work += amount;
      return true;
    };
  };
  Shape shape;
  auto recognition = phase(Stage::Recognition);
  if (!recognize(k, p.maximum_vertices, p.workspace_bytes, recognition, shape))
    return ApplyResult::NoOp;
  ++k.stats.graph_labeling_recognized;
  k.stats.graph_labeling_vertices = shape.vertices;
  k.stats.graph_labeling_separation = shape.separation;
  const uint64_t n = shape.vertices, square = n * n;
  // Conservatively cover retained graph data, depth frames, degree/order
  // arrays, swap states and worst-case vector growth in all search helpers.
  const uint64_t graph_bytes = 16 * square + 64 * n;
  const uint64_t search_bytes = 16 * square + 256 * n;
  if (graph_bytes > p.workspace_bytes || search_bytes > p.workspace_bytes - graph_bytes) {
    ++k.stats.graph_labeling_budget;
    return ApplyResult::NoOp;
  }
  Candidate best;
  uint32_t seeds = 0;
  auto construction = phase(Stage::Construction);
  (void)order_seeds(shape, construction, best, seeds);
  k.stats.graph_labeling_seeds += seeds;
  if (best.labels.empty() || interrupted) return ApplyResult::NoOp;
  if (best.score.first && n <= p.maximum_dfs_vertices && p.work[size_t(Stage::Window)]) {
    auto window = phase(Stage::Window);
    uint64_t nodes = 0;
    (void)window_search(shape, {p.maximum_dfs_nodes, p.maximum_dfs_starts,
        p.early_dfs_choices, p.seed ^ 0x51A7EULL ^ (n << 20) ^ shape.separation},
        window, best, nodes);
    k.stats.graph_labeling_nodes += nodes;
  }
  const uint64_t repair_seed = p.seed ^ 0xC0FFEE1234ULL ^ (n << 8) ^ shape.separation;
  const uint64_t anneal_seed = p.seed ^ 0xA17B00D5ULL ^ (n << 16) ^ shape.separation;
  for (Stage stage : {Stage::Repair, Stage::Anneal, Stage::FinalRepair}) {
    if (interrupted || !best.score.first || !p.work[size_t(stage)]) continue;
    auto charge = phase(stage);
    uint64_t rounds = 0, attempts = 0;
    if (stage == Stage::Anneal)
      (void)anneal(shape, p.anneal_iterations, anneal_seed, {}, charge, best, attempts);
    else
      (void)repair(shape, p.repair_rounds, p.stall_anneal_iterations, repair_seed,
          anneal_seed, {}, {}, charge, best, rounds, attempts);
    k.stats.graph_labeling_repairs += rounds;
    k.stats.graph_labeling_anneal_attempts += attempts;
  }
  if (interrupted) return ApplyResult::NoOp;
  auto publication = phase(Stage::Publication);
  if (uint64_t(k.nvars) > p.workspace_bytes - graph_bytes) {
    ++k.stats.graph_labeling_budget;
    return ApplyResult::NoOp;
  }
  const uint64_t publication_start = used;
  if (!publication(uint64_t(k.nvars) + square) ||
      !normalize_reflection(best, publication)) return ApplyResult::NoOp;
  std::vector<int8_t> partial(k.nvars, 0);
  for (uint32_t vertex = 0; vertex < shape.vertices; ++vertex) {
    for (uint32_t label = 0; label < shape.vertices; ++label) {
      const Lit primary = shape.primary[uint64_t(vertex) * shape.vertices + label];
      const bool positive = (label == best.labels[vertex]) != negated(primary);
      partial[var_of(primary)] = positive ? 1 : -1;
    }
  }
  // Publication preparation above consumes reserved work. Completion gets only
  // the remaining unreserved quota; its phase ceiling must account for that
  // already-performed reserved work rather than subtracting reserve twice.
  const uint64_t publication_preparation = used - publication_start;
  std::vector<uint8_t> model;
  if (!best.score.first && p.work[size_t(Stage::Completion)]) {
    const uint64_t cap = p.work[size_t(Stage::Completion)];
    const uint64_t start = used;
    const uint64_t end = ceiling - reserve + publication_preparation;
    bool exhausted = false;
    auto completion = [&](uint64_t amount) {
      if (exhausted || interrupted) return false;
      if (kernel_rule_interrupt(k, k.stats.graph_labeling_interrupted)) {
        interrupted = true; return false;
      }
      if (amount > cap - (used - start) || amount > end - used) {
        exhausted = true; ++k.stats.graph_labeling_budget; return false;
      }
      used += amount; k.stats.graph_labeling_work += amount; return true;
    };
    const uint64_t retained_bytes = graph_bytes + k.nvars;
    const auto result = complete_model(k, partial, p.workspace_bytes - retained_bytes,
                                       completion, model);
    if (result == CompletionStatus::NoModel) ++k.stats.graph_labeling_models_rejected;
  }
  if (interrupted) return ApplyResult::NoOp;
  // A fresh publication phase deducts the preparation already paid from its
  // allowance. No construction/completion work is charged a second time.
  uint64_t publication_remaining = p.work[size_t(Stage::Publication)] - publication_preparation;
  const auto finish_work = [&](uint64_t amount) {
    if (amount > publication_remaining || amount > ceiling - used) {
      ++k.stats.graph_labeling_budget;
      return false;
    }
    if (kernel_rule_interrupt(k, k.stats.graph_labeling_interrupted)) return false;
    publication_remaining -= amount;
    used += amount; k.stats.graph_labeling_work += amount; return true;
  };
  if (!model.empty()) {
    if (!finish_work(k.nvars)) return ApplyResult::NoOp;
    lucky_publish_model(k, model);
    ++k.stats.graph_labeling_sat;
    return ApplyResult::Sat;
  }
  if (!finish_work(uint64_t(k.nvars) + k.external_to_internal.size()))
    return ApplyResult::NoOp;
  const uint64_t offered = k.stats.local_search_hints_offered;
  kernel_offer_local_search_partial_hint(k, strategy::LocalSearchKind::GraphLabeling,
      partial, best.score.first, true, {used, strategy::LocalSearchEffortUnit::GraphWork});
  return k.stats.local_search_hints_offered != offered ? ApplyResult::Ok : ApplyResult::NoOp;
}

} // namespace tactix
