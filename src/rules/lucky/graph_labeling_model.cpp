#include "rules/lucky/graph_labeling_model.hpp"

#include "rules/lucky/graph_labeling_completion.hpp"
#include "rules/lucky/graph_labeling_repair.hpp"
#include "strategy/rule_parameters.hpp"

#include <algorithm>
#include <limits>

namespace tactix {

struct GraphLabelingModelSession::State {
  enum class Stage : uint8_t {
    Construction,
    Window,
    Repair,
    Anneal,
    FinalRepair,
    Completion,
    Done,
  } stage = Stage::Construction;
  const Kernel *owner = nullptr;
  strategy::GraphLabelingParameters parameters;
  graph_labeling::Shape shape;
  graph_labeling::Candidate best;
  graph_labeling::Score published{UINT64_MAX, UINT64_MAX};
  std::vector<uint8_t> primary_negative;
  uint64_t cumulative_work = 0;
};

GraphLabelingModelSession::GraphLabelingModelSession() = default;
GraphLabelingModelSession::~GraphLabelingModelSession() = default;
// Kernel copies start with an empty heuristic cache, never an aliased owner.
GraphLabelingModelSession::GraphLabelingModelSession(const GraphLabelingModelSession &) {}
GraphLabelingModelSession &GraphLabelingModelSession::operator=(const GraphLabelingModelSession &other) {
  if (this != &other) clear();
  return *this;
}
GraphLabelingModelSession::GraphLabelingModelSession(GraphLabelingModelSession &&) noexcept = default;
GraphLabelingModelSession &GraphLabelingModelSession::operator=(GraphLabelingModelSession &&) noexcept = default;
bool GraphLabelingModelSession::authenticated() const { return bool(state_); }
bool GraphLabelingModelSession::complete() const {
  return state_ && state_->stage == State::Stage::Done;
}
void GraphLabelingModelSession::clear() { state_.reset(); }
uint64_t GraphLabelingModelSession::candidate_fingerprint() const {
  if (!state_ || state_->best.labels.empty()) return 0;
  uint64_t result = 0xcbf29ce484222325ULL;
  auto mix = [&](uint64_t value) {
    result ^= value;
    result *= 0x100000001b3ULL;
  };
  mix(state_->best.score.first);
  mix(state_->best.score.second);
  for (uint32_t label : state_->best.labels) mix(label);
  return result;
}

namespace {

struct GraphMeter {
  Kernel &kernel;
  uint64_t ceiling;
  uint64_t used = 0;
  bool exhausted = false;
  bool interrupted = false;

  GraphMeter(Kernel &k, uint64_t allowance) : kernel(k), ceiling(allowance) {
    if (kernel_has_work_budget(k)) ceiling = std::min(ceiling, k.work_budget);
  }
  ~GraphMeter() { kernel.stats.graph_labeling_work += used; }
  bool charge(uint64_t amount) {
    if (amount > ceiling - used) {
      if (!exhausted) ++kernel.stats.graph_labeling_budget;
      exhausted = true;
      return false;
    }
    used += amount;
    return true;
  }
  bool poll() {
    if (!interrupted &&
        kernel_rule_interrupt(kernel, kernel.stats.graph_labeling_interrupted))
      interrupted = true;
    return interrupted;
  }
};

uint64_t saturated_add(uint64_t a, uint64_t b) {
  return b > UINT64_MAX - a ? UINT64_MAX : a + b;
}

std::vector<int8_t> graph_partial_assignment(
    const Kernel &kernel, const GraphLabelingModelSession::State &state,
    const graph_labeling::Candidate &candidate) {
  std::vector<int8_t> partial(kernel.nvars, 0);
  const uint32_t n = state.shape.vertices;
  if (candidate.labels.size() != n || state.primary_negative.size() != uint64_t(n) * n)
    return {};
  for (uint32_t vertex = 0; vertex < n; ++vertex) {
    for (uint32_t label = 0; label < n; ++label) {
      const uint64_t external = uint64_t(vertex) * n + label;
      if (external >= kernel.external_to_internal.size()) continue;
      const Var internal = kernel.external_to_internal[external];
      if (internal == INVALID_VAR || internal >= kernel.nvars ||
          kernel_var_retired(kernel, internal))
        continue;
      const bool positive = (label == candidate.labels[vertex]) !=
                            bool(state.primary_negative[external]);
      partial[internal] = positive ? 1 : -1;
    }
  }
  return partial;
}

} // namespace

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

GraphLabelingAuthenticationResult graph_labeling_model_authenticate(
    Kernel &k, GraphLabelingModelSession &session) {
  using namespace graph_labeling;
  using Stage = strategy::GraphLabelingStage;
  GraphLabelingAuthenticationResult result;
  ++k.stats.graph_labeling_calls;
  const auto p = strategy::GraphLabelingParameters::from_kernel(k);
  if (!p.valid() || !graph_labeling_model_guard(k)) return result;
  if (session.state_ && session.state_->owner != &k) return result;
  // Authentication with a new profile explicitly replaces the trajectory;
  // invalid profiles/guards and foreign owners leave it untouched.
  if (session.state_ && !(session.state_->parameters == p)) session.clear();
  if (session.authenticated()) {
    result.status = GraphLabelingAuthenticationStatus::Authenticated;
    return result;
  }
  GraphMeter meter(k, p.work[size_t(Stage::Recognition)]);
  Shape shape;
  auto charge = [&](uint64_t amount) {
    if ((meter.used & ((uint64_t{1} << 20) - 1)) == 0 && meter.poll())
      return false;
    return meter.charge(amount);
  };
  if (!recognize(k, p.maximum_vertices, p.workspace_bytes, charge, shape)) {
    result.work = meter.used;
    result.status = meter.interrupted
                        ? GraphLabelingAuthenticationStatus::Interrupted
                        : meter.exhausted
                              ? GraphLabelingAuthenticationStatus::Exhausted
                              : GraphLabelingAuthenticationStatus::NoStructure;
    return result;
  }
  const uint64_t n = shape.vertices, square = n * n;
  const uint64_t graph_bytes = 16 * square + 64 * n;
  const uint64_t search_bytes = 16 * square + 256 * n;
  if (graph_bytes > p.workspace_bytes ||
      search_bytes > p.workspace_bytes - graph_bytes) {
    ++k.stats.graph_labeling_budget;
    result.work = meter.used;
    result.status = GraphLabelingAuthenticationStatus::Exhausted;
    return result;
  }
  auto next = std::make_unique<GraphLabelingModelSession::State>();
  next->owner = &k;
  next->parameters = p;
  next->primary_negative.resize(static_cast<size_t>(square));
  for (uint64_t i = 0; i < square; ++i)
    next->primary_negative[i] = negated(shape.primary[i]);
  next->shape = std::move(shape);
  next->cumulative_work = meter.used;
  session.state_ = std::move(next);
  ++k.stats.graph_labeling_recognized;
  k.stats.graph_labeling_vertices = session.state_->shape.vertices;
  k.stats.graph_labeling_separation = session.state_->shape.separation;
  result.work = meter.used;
  result.status = GraphLabelingAuthenticationStatus::Authenticated;
  return result;
}

GraphLabelingSliceResult graph_labeling_model_slice(
    Kernel &k, GraphLabelingModelSession &session) {
  using namespace graph_labeling;
  using ParameterStage = strategy::GraphLabelingStage;
  using SessionStage = GraphLabelingModelSession::State::Stage;
  GraphLabelingSliceResult result;
  ++k.stats.graph_labeling_calls;
  const auto p = strategy::GraphLabelingParameters::from_kernel(k);
  if (!session.state_ || session.state_->owner != &k || !p.valid() ||
      !(session.state_->parameters == p) || !graph_labeling_model_guard(k))
    return result;
  auto &state = *session.state_;
  result.objective_before = state.best.score.first;
  if (state.stage == SessionStage::Done) {
    result.status = GraphLabelingSliceStatus::TrajectoryComplete;
    result.objective_after = state.best.score.first;
    return result;
  }
  const auto parameter_stage = [&] {
    switch (state.stage) {
    case SessionStage::Construction: return ParameterStage::Construction;
    case SessionStage::Window: return ParameterStage::Window;
    case SessionStage::Repair: return ParameterStage::Repair;
    case SessionStage::Anneal: return ParameterStage::Anneal;
    case SessionStage::FinalRepair: return ParameterStage::FinalRepair;
    case SessionStage::Completion: return ParameterStage::Completion;
    case SessionStage::Done: break;
    }
    return ParameterStage::Publication;
  }();
  const uint64_t stage_allowance = p.work[size_t(parameter_stage)];
  const uint64_t publication_allowance = p.work[size_t(ParameterStage::Publication)];
  GraphMeter meter(k, saturated_add(stage_allowance, publication_allowance));
  if (meter.poll()) {
    result.status = GraphLabelingSliceStatus::Interrupted;
    result.objective_after = state.best.score.first;
    return result;
  }
  const SessionStage stage_before = state.stage;
  uint64_t stage_remaining = std::min(stage_allowance, meter.ceiling);
  auto charge = [&](uint64_t amount) {
    if (!stage_remaining || amount > stage_remaining) {
      if (!meter.exhausted) ++k.stats.graph_labeling_budget;
      meter.exhausted = true;
      return false;
    }
    if ((meter.used & ((uint64_t{1} << 20) - 1)) == 0 && meter.poll())
      return false;
    if (!meter.charge(amount)) return false;
    stage_remaining -= amount;
    return true;
  };
  bool attempted_completion = false;
  std::vector<uint8_t> model;
  switch (state.stage) {
  case SessionStage::Construction: {
    Candidate candidate;
    uint32_t seeds = 0;
    (void)order_seeds(state.shape, charge, candidate, seeds);
    k.stats.graph_labeling_seeds += seeds;
    if (!candidate.labels.empty() && candidate.score < state.best.score)
      state.best = std::move(candidate);
    state.stage = SessionStage::Window;
    break;
  }
  case SessionStage::Window: {
    uint64_t nodes = 0;
    if (!state.best.labels.empty() && state.best.score.first &&
        state.shape.vertices <= p.maximum_dfs_vertices)
      (void)window_search(state.shape,
          {p.maximum_dfs_nodes, p.maximum_dfs_starts, p.early_dfs_choices,
           p.seed ^ 0x51A7EULL ^ (uint64_t(state.shape.vertices) << 20) ^
               state.shape.separation},
          charge, state.best, nodes);
    k.stats.graph_labeling_nodes += nodes;
    state.stage = SessionStage::Repair;
    break;
  }
  case SessionStage::Repair:
  case SessionStage::FinalRepair: {
    uint64_t rounds = 0, attempts = 0;
    if (!state.best.labels.empty() && state.best.score.first)
      (void)repair(state.shape, p.repair_rounds, p.stall_anneal_iterations,
          p.seed ^ 0xC0FFEE1234ULL ^ (uint64_t(state.shape.vertices) << 8) ^
              state.shape.separation,
          p.seed ^ 0xA17B00D5ULL ^ (uint64_t(state.shape.vertices) << 16) ^
              state.shape.separation,
          {}, {}, charge, state.best, rounds, attempts);
    k.stats.graph_labeling_repairs += rounds;
    k.stats.graph_labeling_anneal_attempts += attempts;
    state.stage = state.stage == SessionStage::Repair
                      ? SessionStage::Anneal : SessionStage::Completion;
    break;
  }
  case SessionStage::Anneal: {
    uint64_t attempts = 0;
    if (!state.best.labels.empty() && state.best.score.first)
      (void)anneal(state.shape, p.anneal_iterations,
          p.seed ^ 0xA17B00D5ULL ^ (uint64_t(state.shape.vertices) << 16) ^
              state.shape.separation,
          {}, charge, state.best, attempts);
    k.stats.graph_labeling_anneal_attempts += attempts;
    state.stage = SessionStage::FinalRepair;
    break;
  }
  case SessionStage::Completion: {
    attempted_completion = true;
    if (!state.best.labels.empty() && !state.best.score.first) {
      Candidate candidate = state.best;
      if (normalize_reflection(candidate, charge)) {
        const auto partial = graph_partial_assignment(k, state, candidate);
        if (!partial.empty()) {
          const uint64_t n = state.shape.vertices;
          const uint64_t retained = 16 * n * n + 64 * n + k.nvars;
          if (retained <= p.workspace_bytes)
            (void)complete_model(k, partial, p.workspace_bytes - retained,
                                 charge, model);
        }
      }
    }
    state.stage = SessionStage::Done;
    break;
  }
  case SessionStage::Done: break;
  }
  result.objective_after = state.best.score.first;
  if (meter.interrupted) {
    state.stage = stage_before;
    state.cumulative_work = saturated_add(state.cumulative_work, meter.used);
    result.work = meter.used;
    result.status = GraphLabelingSliceStatus::Interrupted;
    return result;
  }
  if (!model.empty()) {
    const uint64_t publish_cost = k.nvars;
    if (publish_cost <= publication_allowance && meter.charge(publish_cost)) {
      lucky_publish_model(k, model);
      ++k.stats.graph_labeling_sat;
      result.result = ApplyResult::Sat;
      result.status = GraphLabelingSliceStatus::Sat;
      state.cumulative_work = saturated_add(state.cumulative_work, meter.used);
      result.work = meter.used;
      return result;
    }
  }
  if (!state.best.labels.empty() && state.best.score < state.published) {
    Candidate candidate = state.best;
    uint64_t publication_remaining = publication_allowance;
    auto publish_charge = [&](uint64_t amount) {
      if (amount > publication_remaining || !meter.charge(amount)) return false;
      publication_remaining -= amount;
      return true;
    };
    if (normalize_reflection(candidate, publish_charge)) {
      const auto partial = graph_partial_assignment(k, state, candidate);
      const uint64_t before = k.stats.local_search_hints_offered;
      if (!partial.empty() && publish_charge(uint64_t(k.nvars)))
        kernel_offer_local_search_partial_hint(
            k, strategy::LocalSearchKind::GraphLabeling, partial,
            candidate.score.first, true,
            {saturated_add(state.cumulative_work, meter.used),
             strategy::LocalSearchEffortUnit::GraphWork});
      if (k.stats.local_search_hints_offered != before) {
        state.published = state.best.score;
        result.hint_published = result.useful_hint = true;
        result.result = ApplyResult::Ok;
      }
    }
  }
  result.status = attempted_completion || state.stage == SessionStage::Done
                      ? GraphLabelingSliceStatus::TrajectoryComplete
                      : GraphLabelingSliceStatus::Pending;
  state.cumulative_work = saturated_add(state.cumulative_work, meter.used);
  result.work = meter.used;
  return result;
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
