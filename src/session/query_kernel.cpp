#include "session/query_kernel.hpp"

#include "frontend/parse.hpp"
#include "engine/subsolver.hpp"
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/restart/restart.hpp"
#include "session/native_variables.hpp"
#include "session/restriction_knowledge.hpp"
#include "session/knowledge_proof.hpp"
#include "strategy/structural_action_sessions.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <unordered_set>
#include <utility>

namespace tactix::session {
namespace {

bool query_boundary_busy(const Kernel &k) {
  return k.inprocessing || k.phase_saving_suppressed || k.probing.recording ||
         !k.large_watches_connected || k.dense || k.pending.valid;
}

// Solver-population events are independent of proof formatting. Capture only
// bounded immutable copies; no validation or search runs inside the callback.
struct ClauseCapture final : PopulationObserver {
  RetentionLimits limits;
  uint32_t variables = 0;
  std::vector<std::vector<int32_t>> pending;
  uint64_t literals = 0, dropped = 0;
  explicit ClauseCapture(RetentionLimits bounds) : limits(bounds) {}
  void observe(const SolverPopulationEvent &event) noexcept override {
    if (event.kind != SolverPopulationEventKind::Add) return;
    const uint32_t size = event.clause.size();
    if (size > limits.maximum_clause_literals ||
        pending.size() >= limits.maximum_pending_clauses ||
        size > limits.maximum_pending_literals - literals) {
      ++dropped;
      return;
    }
    for (uint32_t i = 0; i < size; ++i) {
      const int32_t literal = event.clause.at(i);
      if (!literal || literal == INT32_MIN ||
          external_variable_of(literal) > variables) {
        ++dropped;
        return;
      }
    }
    std::vector<int32_t> clause;
    clause.reserve(size);
    for (uint32_t i = 0; i < size; ++i) clause.push_back(event.clause.at(i));
    literals += size;
    pending.push_back(std::move(clause));
  }
};

bool install_retained(Kernel &k, const RetainedClause &record) {
  if (k.inconsistent) return true;
  std::vector<Lit> clause;
  clause.reserve(record.literals.size());
  for (const int32_t external : record.literals) {
    const Lit literal = kernel_internalize(k, external);
    assert(literal != INVALID_LIT);
    if (k.value(literal) > 0) return true;
    if (!k.value(literal)) clause.push_back(literal);
  }
  if (clause.size() == 2 && kernel_binary_live(k, clause[0], clause[1])) return true;
  if (!record.parity_equations.empty()) {
    PbpParityBoundsCache cache;
    if (!emit_query_knowledge(*k.proof,record,k.next_clause_id,k.external_to_internal.size(),
                        cache,[](uint64_t) {return true;})) return false;
  }
  if (clause.empty()) {
    k.proof->add_empty(++k.next_clause_id);
    k.inconsistent = true; // proved from permanent input, without assumptions
    return true;
  }
  const uint64_t id = ++k.next_clause_id;
  if (clause.size() == 1) {
    k.proof->add_derived_unit(id, clause[0]);
    k.unit_ids[var_of(clause[0])] = id;
    kernel_assign_unit(k, clause[0]);
    heuristics_deactivate_var(k, var_of(clause[0]));
  } else {
    k.proof->add_rup(id, clause.data(), clause.size());
    if (clause.size() == 2) kernel_add_binary_clause(k, clause[0], clause[1], id);
    else {
      // With no decision levels at import, width is a conservative initial
      // upper bound on LBD; this service never gives a clause privileged glue.
      kernel_add_large_clause(k, clause.data(), clause.size(), true,
                               static_cast<uint32_t>(clause.size()), id);
      // Every retained literal is unassigned after root normalization. Its
      // fresh watches need no trail replay; the theory rearm service instead
      // handles nonroot insertion and requires an open decision level.
    }
  }
  (void)propagate_service(k);
  return true;
}

// The full external source is already a checker premise, either independently
// imported for restoration or seeded as exact new client input. No assumption
// is asserted here. Preserve raw order while removing repeated/root-false
// literals, and retire the full checker copy when its native body changes.
template <class Activate>
void install_external_source(Kernel &k, uint64_t source_id,
    std::span<const int32_t> source, bool deduplicate_binary, Activate activate) {
  std::unordered_set<int32_t> seen;
  std::vector<Lit> clause;
  bool satisfied = false;
  for (int32_t lit : source) {
    if (seen.count(-lit)) { satisfied = true; break; }
    if (!seen.insert(lit).second) continue;
    const int8_t value = kernel_external_value(k, internalize(lit));
    if (value > 0) { satisfied = true; break; }
    if (!value) clause.push_back(activate(lit));
  }
  if (!satisfied && deduplicate_binary && clause.size() == 2 &&
      kernel_binary_live(k, clause[0], clause[1])) satisfied = true;
  const bool normalized = satisfied || clause.size() != source.size();
  if (!satisfied) {
    const uint64_t id = normalized || clause.empty() ? ++k.next_clause_id : source_id;
    if (clause.empty()) {
      k.proof->add_empty(id);
      k.inconsistent = true;
    } else if (clause.size() == 1) {
      if (normalized) k.proof->add_derived_unit(id, clause[0]);
      k.unit_ids[var_of(clause[0])] = id;
      kernel_assign_unit(k, clause[0]);
      heuristics_deactivate_var(k, var_of(clause[0]));
    } else {
      if (normalized) k.proof->add_rup(id, clause.data(), clause.size());
      if (clause.size() == 2) kernel_add_binary_clause(k, clause[0], clause[1], id);
      else kernel_add_large_clause(k, clause.data(), clause.size(), false, 0, id);
    }
  }
  if (normalized && !k.inconsistent)
    if (!k.proof->del_external(++k.next_clause_id, source_id, source.data(), source.size()))
      std::abort();
}

} // namespace

struct QueryKernel::Epoch final : QueryProofSources {
  // Outlive Proof's observer-unbind and Kernel destruction.
  std::unique_ptr<ClauseCapture> capture;
  std::unique_ptr<QueryProjection> projection;
  std::unique_ptr<WeakeningHistory> weakening;
  std::unique_ptr<DerivationHistory> derivations;
  uint64_t derivation_stream = 0;
  Kernel kernel;
  Heuristics heuristics;
  std::unique_ptr<Proof> proof;
  std::unique_ptr<StructuralActionSessions> services;
  std::vector<uint64_t> assumption_ids;
  bool assumption_refuted = false;
  AssumptionStep failure;
  struct AssumptionCursor {
    uint64_t generation=0, retraction=0, names=0, translations=0;
    size_t assumption=0, trail=0;
    bool only_assumptions=true;
    size_t root_trail=SIZE_MAX;
    bool assigned_cube=false, skipped_free=false;
  } assumption_cursor;
  size_t input_words = 0;
  NativeQueryLimits limits;

  QueryProofLimits certificate_limits() const {
    return {limits.proof.maximum_variables,
        uint64_t(limits.proof.maximum_clauses)+limits.proof.maximum_pending_clauses,
        limits.proof.maximum_literals+limits.proof.maximum_pending_literals,
        limits.proof_ticks};
  }

  bool snapshot(std::vector<QueryProofOwnedClause> &sources,
      std::vector<int32_t> &assumptions, uint32_t &extent) const override {
    const Kernel &k = kernel;
    if (!weakening || !weakening->capture_complete()) return false;
    extent = k.external_to_internal.size();
    if (extent>limits.proof.maximum_variables) return false;
    uint64_t literals = 0;
    const auto append = [&](uint64_t id, std::vector<int32_t> clause) {
      if (sources.size()>=limits.proof.maximum_clauses ||
          clause.size()>limits.proof.maximum_clause_literals ||
          clause.size()>limits.proof.maximum_literals-literals) return false;
      literals += clause.size();
      sources.push_back({id, std::move(clause)}); return true;
    };
    for (const auto [ref, c] : arena_records(k.arena)) {
      (void)ref;
      if (c->garbage()) continue;
      if (c->detached_reason()) return false;
      std::vector<int32_t> clause;
      for (Lit literal : *c) clause.push_back(kernel_externalize(k, literal));
      if (!append(c->id(), std::move(clause))) return false;
    }
    for (const auto &[key, population] : weakening->binaries_)
      for (const auto &copy : population.copies)
        if (!append(copy.source.proof_id, {externalize(static_cast<Lit>(key>>32)),
                                          externalize(static_cast<Lit>(key))})) return false;
    for (Var ev = 0; ev<k.external_to_internal.size(); ++ev) {
      int8_t value = ev<k.fixed_external_values.size() ? k.fixed_external_values[ev] : 0;
      const Var iv = k.external_to_internal[ev];
      if (!value && iv!=INVALID_VAR && k.assigned[iv].level==0)
        value = k.values[make_lit(iv, false)];
      if (value && !append(kernel_external_unit_id(k, ev),
                          {externalize(make_lit(ev, value<0))})) return false;
    }
    if (projection) assumptions.assign(projection->constraints().begin(), projection->constraints().end());
    return true;
  }

  explicit Epoch(std::unique_ptr<Proof> writer)
      : proof(std::move(writer)), services(std::make_unique<StructuralActionSessions>(kernel)) {
    kernel.h = &heuristics;
    kernel.saved_phases = &heuristics.saved;
    kernel.proof = proof.get();
    proof->bind_variable_map(&kernel.internal_to_external);
    proof->bind_external_variable_map(&kernel.external_to_internal);
  }
};

QueryKernel::QueryKernel() = default;
QueryKernel::QueryKernel(RetentionLimits limits) : retention_limits_(limits) {
  if (limits.enabled()) retained_ = std::make_unique<RetainedClauses>(limits);
}
QueryKernel::QueryKernel(NativeQueryLimits limits) : native_limits_(limits) {}
QueryKernel::~QueryKernel() = default;
void QueryKernel::discard_epoch() {
  epoch_.reset();
  result_.reset();
}

ReplayStatus QueryKernel::replay(const QueryHistory &history) {
  return replay(history, {});
}

ReplayStatus QueryKernel::replay(const QueryHistory &history,
                                std::unique_ptr<Proof> writer) {
  const QueryRecord *const query = history.active_query();
  if (!query)
    return ReplayStatus::NoActiveQuery;
  if (history_ && history_ != &history)
    return ReplayStatus::WrongHistory;
  if(poll_terminate())return ReplayStatus::Interrupted;
  if (epoch_) {
    const Kernel &old = epoch_->kernel;
    if (old.inprocessing || old.phase_saving_suppressed || old.probing.recording ||
        !old.large_watches_connected || old.dense || old.arena_ordering || old.pending.valid ||
        (old.affine_matrix_search_session &&
         old.affine_matrix_search_session->has_deferred_reason()))
      return ReplayStatus::Busy;
  }
  if (generation_ == std::numeric_limits<uint64_t>::max())
    return ReplayStatus::Exhausted;
  if (writer && !writer->stream_identity())
    return ReplayStatus::ProofUnavailable;
  if (!writer) {
    writer = std::make_unique<Proof>();
    if (!writer->open_nowhere())
      return ReplayStatus::ProofUnavailable;
  }
  auto next = std::make_unique<Epoch>(std::move(writer));
  Kernel &k = next->kernel;
  if (native_limits_.enabled()) {
    next->limits = native_limits_;
    next->weakening = std::make_unique<WeakeningHistory>(k, native_limits_.weakening);
    kernel_bind_population_observer(k, next->weakening.get());
    next->derivations = std::make_unique<DerivationHistory>(
        native_limits_.proof.maximum_pending_clauses,
        native_limits_.proof.maximum_pending_literals);
    next->derivations->start_query(query->id);
    next->proof->bind_derivation_observer(next->derivations.get());
    next->proof->enable_query_certificates(*next, next->certificate_limits());
  }
  next->input_words = query->input_words;
  declare_input(k, query->query_variables,
                query->clauses + query->assumptions.size());
  std::vector<Lit> clause;
  for (const int32_t literal : history.input().first(query->input_words)) {
    if(poll_terminate())return ReplayStatus::Interrupted;
    if (literal) clause.push_back(internalize(literal));
    else {
      ingest_clause(k, clause);
      clause.clear();
    }
  }
  assert(clause.empty());

  // A query certificate's input is F followed by A as unit clauses. Register
  // those exact checker inputs without asserting them in the live kernel.
  // Input-time derived IDs can split the namespace; seed_population owns that
  // mapping just as it does for later raw clauses in the ordinary importer.
  k.input_activated.resize(k.nvars, 0);
  for (const int32_t external : query->assumptions) {
    if(poll_terminate())return ReplayStatus::Interrupted;
    const Lit literal = kernel_internalize(k, external);
    assert(literal != INVALID_LIT);
    const uint64_t id = ++k.next_clause_id;
    next->proof->seed_population(id, &literal, 1);
    next->assumption_ids.push_back(id);
    const Var v = var_of(literal);
    // An assumption-only variable must remain branchable after finish_input.
    // This is an activation event, not a root assignment or a new base clause.
    if (!k.values[literal] && !k.input_activated[v]) {
      k.input_activated[v] = 1;
      if (k.tumble) k.input_activation_order.push_back(v);
    }
  }
  finish_input(k);
  if (!query->assumptions.empty())
    // Existing startup recognizers own the exact original-input opportunity.
    // Their query-aware integration is pending; this epoch's startup_input
    // contains F only, while the checker input additionally contains A.
    k.close_startup_certificate();
  if (!query->assumptions.empty()) {
    next->projection = std::make_unique<QueryProjection>(history, query->id);
    k.query_projection = next->projection.get();
  }
  if (retained_) {
    for (const auto &record : retained_->clauses()) {
      if(poll_terminate())return ReplayStatus::Interrupted;
      assert(record.source_input_words <= query->input_words);
      if (!install_retained(k, record)) return ReplayStatus::ProofUnavailable;
    }
    next->capture = std::make_unique<ClauseCapture>(retention_limits_);
    next->capture->variables = query->formula_variables;
    kernel_bind_population_observer(k, next->capture.get());
  }
  ++generation_;
  next->derivation_stream = generation_;
  history_ = &history;
  query_ = query->id;
  epoch_ = std::move(next);
  return ReplayStatus::Ok;
}

NativeAdvanceResult QueryKernel::advance(const QueryHistory &history) {
  return advance(history, {});
}

NativeAdvanceResult QueryKernel::advance(const QueryHistory &history,
    std::unique_ptr<Proof> writer, const volatile sig_atomic_t *stop) {
  return transition(history,std::move(writer),stop,false);
}

NativeAdvanceResult QueryKernel::repair(const QueryHistory &history) {
  return repair(history,{});
}

NativeAdvanceResult QueryKernel::repair(const QueryHistory &history,
    std::unique_ptr<Proof> writer, const volatile sig_atomic_t *stop) {
  return transition(history,std::move(writer),stop,true);
}

NativeAdvanceResult QueryKernel::transition(const QueryHistory &history,
    std::unique_ptr<Proof> writer, const volatile sig_atomic_t *stop, bool repair) {
  NativeAdvanceResult out;
  const auto interrupted=[&] {return poll_terminate() || (stop&&*stop);};
  const auto fail = [&](NativeAdvanceStatus status) { out.status = status; return out; };
  const QueryRecord *query = history.active_query();
  if (!query) return fail(NativeAdvanceStatus::NoActiveQuery);
  if (history_ && history_ != &history) return fail(NativeAdvanceStatus::WrongHistory);
  if (interrupted()) return fail(NativeAdvanceStatus::Interrupted);
  if (writer && !writer->accepts_query_input()) return fail(NativeAdvanceStatus::ProofUnavailable);
  if (!epoch_) {
    if (repair) return fail(NativeAdvanceStatus::ReplayRequired);
    switch (replay(history, std::move(writer))) {
    case ReplayStatus::Ok: return fail(NativeAdvanceStatus::Replayed);
    case ReplayStatus::Exhausted: return fail(NativeAdvanceStatus::Exhausted);
    case ReplayStatus::ProofUnavailable: return fail(NativeAdvanceStatus::ProofUnavailable);
    case ReplayStatus::Interrupted: return fail(NativeAdvanceStatus::Interrupted);
    default: return fail(NativeAdvanceStatus::ReplayRequired);
    }
  }
  Epoch &epoch = *epoch_;
  Kernel &k = epoch.kernel;
  if (repair) {
    if (query->id!=query_ || query->input_words!=epoch.input_words || epoch.assumption_refuted)
      return fail(NativeAdvanceStatus::ReplayRequired);
    if (!epoch.projection || (!epoch.projection->snapshot().restore &&
                             k.query_projection==epoch.projection.get()))
      return fail(NativeAdvanceStatus::NotRequired);
  }
  if (k.inprocessing || k.warming || k.phase_saving_suppressed || k.probing.recording ||
      !k.large_watches_connected || k.dense || k.arena_ordering || k.pending.valid ||
      (k.affine_matrix_search_session && k.affine_matrix_search_session->has_deferred_reason()))
    return fail(NativeAdvanceStatus::Busy);
  if (!native_limits_.enabled() || !epoch.weakening ||
      !epoch.weakening->capture_complete() || k.inconsistent ||
      (!repair && query->id <= query_) || query->input_words < epoch.input_words)
    return fail(NativeAdvanceStatus::ReplayRequired);
  if (generation_ == UINT64_MAX) return fail(NativeAdvanceStatus::Exhausted);
  if (query->query_variables > native_limits_.proof.maximum_variables ||
      query->input_words > native_limits_.proof.maximum_input_words)
    return fail(NativeAdvanceStatus::Exhausted);
  // Native proof dependencies through private helpers and affine reason leases
  // need their own composition owner. Never import them as trusted new input.
  if (k.original_external_vars != UINT32_MAX &&
      k.external_to_internal.size() > k.original_external_vars)
    return fail(NativeAdvanceStatus::ReplayRequired);
  for (Lit lit : k.trail)
    if (k.assigned[var_of(lit)].affine) return fail(NativeAdvanceStatus::ReplayRequired);
  const auto admitted_identity = [&](int32_t external) {
    const Var ev = var_of(internalize(external));
    return ev < k.external_to_internal.size() &&
        ((ev < k.fixed_external_values.size() && k.fixed_external_values[ev]) ||
         (ev < k.external_import_rank.size() && k.external_import_rank[ev] != UINT32_MAX));
  };
  struct Fact { uint64_t id; std::vector<int32_t> clause; bool native = true; };
  std::vector<Fact> facts;
  std::vector<Fact> suffix;
  std::vector<Fact> recovery;
  uint64_t recovery_literals = 0;
  std::vector<int32_t> input_clause;
  std::vector<int32_t> used;
  for (int32_t lit : history.input().subspan(epoch.input_words, query->input_words-epoch.input_words)) {
    if (lit) {
      if (input_clause.size() >= native_limits_.proof.maximum_clause_literals)
        return fail(NativeAdvanceStatus::Exhausted);
      input_clause.push_back(lit);
      used.push_back(lit);
    } else {
      if (suffix.size() >= native_limits_.proof.maximum_clauses)
        return fail(NativeAdvanceStatus::Exhausted);
      suffix.push_back({0, std::move(input_clause)});
      input_clause.clear();
    }
  }
  assert(input_clause.empty());
  used.insert(used.end(), query->assumptions.begin(), query->assumptions.end());
  std::vector<Lit> native_roots;
  uint64_t next_id = k.next_clause_id;
  size_t fact_literals = 0;
  const auto append = [&](uint64_t id, std::vector<int32_t> clause, bool native = true) {
    if (!id || clause.empty() || clause.size() > native_limits_.proof.maximum_clause_literals ||
        facts.size() >= native_limits_.proof.maximum_clauses ||
        clause.size() > native_limits_.proof.maximum_literals - fact_literals)
      return false;
    fact_literals += clause.size();
    facts.push_back({id, std::move(clause), native});
    return true;
  };
  for (const auto [ref, c] : arena_records(k.arena)) {
    (void)ref;
    if (c->detached_reason() && c->reason()) return fail(NativeAdvanceStatus::ReplayRequired);
    if (c->garbage()) continue;
    if (!c->id() || c->id() > k.next_clause_id)
      return fail(NativeAdvanceStatus::ReplayRequired);
    std::vector<int32_t> clause;
    for (Lit lit : *c) clause.push_back(kernel_externalize(k, lit));
    if (!append(c->id(), std::move(clause))) return fail(NativeAdvanceStatus::Exhausted);
  }
  for (const auto &[key, population] : epoch.weakening->binaries_) {
    const std::vector<int32_t> clause{
        externalize(static_cast<Lit>(key >> 32)), externalize(static_cast<Lit>(key))};
    for (const auto &copy : population.copies) {
      if (!copy.source.proof_id || copy.source.proof_id > k.next_clause_id)
        return fail(NativeAdvanceStatus::ReplayRequired);
      if (!append(copy.source.proof_id, clause)) return fail(NativeAdvanceStatus::Exhausted);
    }
  }
  // Read only permanent roots. A preceding query's decisions and its SAT model
  // must never become premises or taint seeds for this transition.
  for (Var ev = 0; ev < k.external_to_internal.size(); ++ev) {
    int8_t value = ev < k.fixed_external_values.size() ? k.fixed_external_values[ev] : 0;
    const Var iv = k.external_to_internal[ev];
    if (!value && iv != INVALID_VAR && k.assigned[iv].level == 0)
      value = k.values[make_lit(iv, false)];
    if (!value) continue;
    const int32_t literal = externalize(make_lit(ev, value < 0));
    uint64_t id = kernel_external_unit_id(k, ev);
    if (id > k.next_clause_id) return fail(NativeAdvanceStatus::ReplayRequired);
    if (!id) {
      if (next_id == UINT64_MAX) return fail(NativeAdvanceStatus::Exhausted);
      id = ++next_id; // proof-only name; do not change solver-semantic unit_ids
    }
    if (!append(id, {literal})) return fail(NativeAdvanceStatus::Exhausted);
    used.push_back(literal);
    if (iv != INVALID_VAR) native_roots.push_back(make_lit(iv, value < 0));
  }
  RestorationPlan plan = epoch.weakening->plan_restoration(used);
  if (plan.status() == RestorationPlanStatus::Exhausted) return fail(NativeAdvanceStatus::Exhausted);
  if (plan.status() != RestorationPlanStatus::Ready) return fail(NativeAdvanceStatus::ReplayRequired);
  size_t restoration_copies = 0;
  for (auto &record : plan.selected_) {
    for (int32_t lit : record.clause)
      if (!admitted_identity(lit)) return fail(NativeAdvanceStatus::ReplayRequired);
    restoration_copies += record.sources.size();
    for (auto &source : record.sources) {
      if (record.derived && !source.proof_id) {
        if (next_id == UINT64_MAX) return fail(NativeAdvanceStatus::Exhausted);
        source.proof_id = ++next_id;
      } else if (!source.proof_id || source.proof_id > k.next_clause_id)
        return fail(NativeAdvanceStatus::ReplayRequired);
      if (!append(source.proof_id, record.clause, false)) return fail(NativeAdvanceStatus::Exhausted);
    }
  }
  size_t possible_binaries = restoration_copies + suffix.size();
  if (possible_binaries > native_limits_.weakening.live_binary_copies -
                             epoch.weakening->live_binary_copies())
    return fail(NativeAdvanceStatus::Exhausted);
  // Bound all new input, deletion and normalization IDs before emitting or
  // mutating, including one publication per possible new root and a conflict.
  const uint64_t input_ids = query->clauses + query->assumptions.size();
  const uint64_t root_ids = std::max<uint64_t>(k.external_to_internal.size(), query->query_variables) + 1;
  std::sort(facts.begin(), facts.end(), [](const Fact &a, const Fact &b) {
    return a.id != b.id ? a.id < b.id : a.clause < b.clause;
  });
  // The same live ID must never purport to name two distinct literal sets.
  std::map<uint64_t, std::vector<int32_t>> bodies;
  for (const auto &fact : facts) {
    auto body = fact.clause;
    std::sort(body.begin(), body.end());
    auto [at, inserted] = bodies.emplace(fact.id, body);
    if (!inserted && at->second != body) return fail(NativeAdvanceStatus::ReplayRequired);
  }
  std::vector<RetainedClause> derivations;
  std::vector<RestrictionStep> restricted_program;
  std::vector<size_t> order;
  bool rollback = false;
  uint64_t retracted = 0;
  {
    RetainedClauses oracle(native_limits_.proof);
    // Re-establish recorded intermediate additions from permanent F alone.
    // Input records can be old assumptions; SR/RAT metadata is not authority.
    // Deleting a globally entailed lemma cannot invalidate its later reuse.
    // No record ID conveys authority in the destination stream.
    if (epoch.derivations)
      for (const auto &record : epoch.derivations->records()) {
        if (record.kind == DerivationKind::Input || record.kind == DerivationKind::Delete ||
            record.clause.empty()) continue;
        if (out.ticks >= native_limits_.proof_ticks) return fail(NativeAdvanceStatus::Exhausted);
        const auto result = record.kind==DerivationKind::ParityReason
            ? oracle.admit_parity(history,query->id,record.parity_equations,record.clause,
                                  native_limits_.proof_ticks-out.ticks,stop)
            : oracle.admit(history,query->id,record.clause,native_limits_.proof_ticks-out.ticks,stop);
        out.ticks += std::min(result.ticks, UINT64_MAX-out.ticks);
        switch (result.status) {
        case RetentionStatus::Accepted:
          ++out.derivations;
          break;
        case RetentionStatus::AlreadyPresent:
        case RetentionStatus::NotRup:
        case RetentionStatus::Tautology:
        case RetentionStatus::Invalid: break;
        case RetentionStatus::Interrupted: return fail(NativeAdvanceStatus::Interrupted);
        case RetentionStatus::Exhausted: return fail(NativeAdvanceStatus::Exhausted);
        default: return fail(NativeAdvanceStatus::ReplayRequired);
        }
      }
    derivations.assign(oracle.clauses().begin(),oracle.clauses().end());
    std::vector<bool> accepted(facts.size(), false);
    while (order.size() < facts.size()) {
      const size_t before = order.size();
      for (size_t i = 0; i < facts.size(); ++i) {
        if (accepted[i]) continue;
        if (out.ticks >= native_limits_.proof_ticks) return fail(NativeAdvanceStatus::Exhausted);
        const auto result = oracle.admit(history, query->id, facts[i].clause,
                                        native_limits_.proof_ticks-out.ticks, stop);
        out.ticks += std::min(result.ticks, UINT64_MAX-out.ticks);
        switch (result.status) {
        case RetentionStatus::Accepted:
        case RetentionStatus::AlreadyPresent:
          accepted[i] = true; order.push_back(i); ++out.checked; break;
        case RetentionStatus::NotRup: break; // later admitted facts may license it
        case RetentionStatus::Interrupted: return fail(NativeAdvanceStatus::Interrupted);
        case RetentionStatus::Exhausted: return fail(NativeAdvanceStatus::Exhausted);
        default: return fail(NativeAdvanceStatus::ReplayRequired);
        }
      }
      if (before == order.size()) break;
    }
  }
  if(order.size()!=facts.size()) {
    // A restriction is not a permanent-F consequence. Revalidate its full
    // repair witness and the current cube before licensing its descendants.
    // Keep this scope entirely separate from the permanent retention pool.
    if(!epoch.derivations || !std::any_of(epoch.derivations->records().begin(),epoch.derivations->records().end(),
        [](const auto &record) {return !record.clause.empty() &&
            (record.kind==DerivationKind::Rat || record.kind==DerivationKind::SubstitutionRedundancy);}))
      return fail(NativeAdvanceStatus::ReplayRequired);
    RestrictionKnowledge knowledge(history,*query,native_limits_.proof);
    out.checked=out.derivations=0;order.clear();derivations.clear();
    for(const auto &record:epoch.derivations->records()) {
      if(record.kind==DerivationKind::Input || record.kind==DerivationKind::Delete || record.clause.empty())continue;
      if(out.ticks>=native_limits_.proof_ticks)return fail(NativeAdvanceStatus::Exhausted);
      const auto result=knowledge.admit(record,native_limits_.proof_ticks-out.ticks,stop);
      out.ticks+=std::min(result.ticks,UINT64_MAX-out.ticks);
      switch(result.status) {
      case RetentionStatus::Accepted:++out.derivations;break;
      case RetentionStatus::AlreadyPresent:
      case RetentionStatus::NotRup:
      case RetentionStatus::Tautology:
      case RetentionStatus::Invalid:break;
      case RetentionStatus::Interrupted:return fail(NativeAdvanceStatus::Interrupted);
      case RetentionStatus::Exhausted:return fail(NativeAdvanceStatus::Exhausted);
      default:return fail(NativeAdvanceStatus::ReplayRequired);
      }
    }
    restricted_program.assign(knowledge.steps().begin(),knowledge.steps().end());
    for(const auto &step:restricted_program)derivations.push_back(step.consequence);
    std::vector<bool> accepted(facts.size(),false);
    while(order.size()<facts.size()) {
      const size_t before=order.size();
      for(size_t i=0;i<facts.size();++i) {
        if(accepted[i])continue;
        if(out.ticks>=native_limits_.proof_ticks)return fail(NativeAdvanceStatus::Exhausted);
        const auto result=knowledge.admit(facts[i].clause,native_limits_.proof_ticks-out.ticks,stop);
        out.ticks+=std::min(result.ticks,UINT64_MAX-out.ticks);
        switch(result.status) {
        case RetentionStatus::Accepted:
        case RetentionStatus::AlreadyPresent:accepted[i]=true;order.push_back(i);++out.checked;break;
        case RetentionStatus::NotRup:break;
        case RetentionStatus::Interrupted:return fail(NativeAdvanceStatus::Interrupted);
        case RetentionStatus::Exhausted:return fail(NativeAdvanceStatus::Exhausted);
        default:return fail(NativeAdvanceStatus::ReplayRequired);
        }
      }
      if(before==order.size()) { rollback = true; break; }
    }
  }
  if (rollback) {
    // Keep only independently licensed facts. In particular, retract every
    // unlicensed root, not just the restriction that first implied it.
    std::vector<Fact> licensed;
    std::vector<bool> kept(facts.size(),false);
    for (size_t at : order) kept[at]=true;
    for (size_t at=0; at<facts.size(); ++at) retracted += !kept[at] && facts[at].native;
    for (size_t at : order) licensed.push_back(std::move(facts[at]));
    facts = std::move(licensed);
    order.clear();
    fact_literals = 0;
    std::set<std::vector<int32_t>> represented;
    for (size_t at=0; at<facts.size(); ++at) {
      order.push_back(at);
      fact_literals += facts[at].clause.size();
      auto body=facts[at].clause;
      std::sort(body.begin(),body.end());
      represented.insert(std::move(body));
    }
    // Restoring the original representation discharges all old model repairs,
    // including root-conditioned/flush effects that have no weakening record.
    // Do this only on invalidation; normal queries keep selective restoration.
    std::vector<int32_t> witnesses;
    for (const auto &record : epoch.weakening->records())
      for (int32_t witness : record.witness) witnesses.push_back(-witness);
    plan = epoch.weakening->plan_restoration(witnesses);
    if (plan.status()==RestorationPlanStatus::Exhausted) return fail(NativeAdvanceStatus::Exhausted);
    if (plan.status()!=RestorationPlanStatus::Ready || plan.kept())
      return fail(NativeAdvanceStatus::ReplayRequired);
    std::vector<int32_t> body;
    for (int32_t lit : history.input().first(epoch.input_words)) {
      if (lit) body.push_back(lit);
      else {
        auto key=body;std::sort(key.begin(),key.end());
        if (represented.insert(std::move(key)).second) {
          if (body.size()>native_limits_.proof.maximum_clause_literals ||
              recovery.size()+facts.size()>=native_limits_.proof.maximum_clauses ||
              body.size()>native_limits_.proof.maximum_literals-fact_literals-recovery_literals)
            return fail(NativeAdvanceStatus::Exhausted);
          recovery_literals+=body.size();
          recovery.push_back({0,std::move(body),false});
        }
        body.clear();
      }
    }
    possible_binaries = recovery.size()+suffix.size();
    for (const auto &fact : facts) possible_binaries += !fact.native;
    if (possible_binaries > native_limits_.weakening.live_binary_copies-
                              epoch.weakening->live_binary_copies())
      return fail(NativeAdvanceStatus::Exhausted);
    native_roots.clear(); // the transaction will reassert only licensed roots
  }
  if (interrupted()) return fail(NativeAdvanceStatus::Interrupted);
  __uint128_t knowledge_ids=0;
  for (const auto &record:derivations) {
    ++knowledge_ids;
    for (const auto &row:record.parity_equations) knowledge_ids+=uint64_t(1)<<(row.variables.size()-1);
  }
  if (knowledge_ids>UINT64_MAX-next_id) return fail(NativeAdvanceStatus::Exhausted);
  const uint64_t reserved=next_id+uint64_t(knowledge_ids);
  if (root_ids > UINT64_MAX-reserved ||
      input_ids > (UINT64_MAX-reserved-root_ids)/2 ||
      possible_binaries > (UINT64_MAX-reserved-root_ids-2*input_ids)/2)
    return fail(NativeAdvanceStatus::Exhausted);
  const uint32_t namespace_extent = std::max<uint32_t>(query->query_variables, k.external_to_internal.size());
  if (!Proof::can_isolate_query(namespace_extent)) return fail(NativeAdvanceStatus::Exhausted);
  __uint128_t copy_work = (__uint128_t(facts.size())+suffix.size()+recovery.size()+query->assumptions.size()) *
                                (6*uint64_t(namespace_extent)+64) + 12*__uint128_t(fact_literals + recovery_literals +
                                query->input_words-epoch.input_words+query->assumptions.size());
  __uint128_t prefix_sources=query->clauses,prefix_literals=query->input_words-query->clauses;
  for(const auto &step:restricted_program) {
    if(step.restriction)copy_work+=(prefix_sources+query->assumptions.size())*(6*uint64_t(namespace_extent)+64)+
        12*(prefix_literals+query->assumptions.size());
    ++prefix_sources;prefix_literals+=step.consequence.literals.size();
  }
  if (out.ticks >= native_limits_.proof_ticks || copy_work > native_limits_.proof_ticks-out.ticks)
    return fail(NativeAdvanceStatus::Exhausted);
  out.ticks += uint64_t(copy_work);
  bool proof_refuted = false;
  {
    __uint128_t imported = query->input_words + __uint128_t(fact_literals) +
        3*(query->clauses+facts.size()+__uint128_t(query->assumptions.size()));
    for (const auto &record:derivations) imported+=record.literals.size()+3;
    if (imported >= UINT32_MAX || out.ticks >= native_limits_.proof_ticks)
      return fail(NativeAdvanceStatus::Exhausted);
    SubSolver roots;
    roots.reset(query->query_variables);
    std::vector<SubLit> clause;
    for (int32_t literal : history.input().first(query->input_words)) {
      if (interrupted()) return fail(NativeAdvanceStatus::Interrupted);
      if (literal) clause.push_back(internalize(literal));
      else { roots.add_clause(clause); clause.clear(); }
    }
    for (const auto &fact : facts) {
      clause.clear(); for (int32_t literal : fact.clause) clause.push_back(internalize(literal));
      roots.add_clause(clause);
    }
    for (const auto &record:derivations) {
      clause.clear();for (int32_t literal:record.literals)clause.push_back(internalize(literal));
      roots.add_clause(clause);
    }
    for (int32_t literal : query->assumptions) roots.add_clause({internalize(literal)});
    const auto status = roots.propagate_assumptions_only({}, native_limits_.proof_ticks-out.ticks, stop);
    out.ticks += std::min(roots.ticks(), UINT64_MAX-out.ticks);
    if (status == SubPropagationResult::Interrupted) return fail(NativeAdvanceStatus::Interrupted);
    if (status == SubPropagationResult::Budget) return fail(NativeAdvanceStatus::Exhausted);
    proof_refuted = status == SubPropagationResult::Conflict;
  }
  if (!writer) {
    writer = std::make_unique<Proof>();
    if (!writer->open_nowhere()) return fail(NativeAdvanceStatus::ProofUnavailable);
  }
  // A fresh stream starts from exact raw F+A. Re-establish every live fact as a
  // checked delta in the old namespace. Old numeric IDs carry no
  // authority from the preceding proof. Input IDs are above every native ID.
  writer->bind_variable_map(nullptr);
  writer->bind_external_variable_map(&k.external_to_internal);
  writer->disable_pbp_id_deletions();
  std::vector<QueryProofClause> imported_sources;
  std::vector<Lit> raw;
  size_t word = 0, appended = 0, clause_begin=0, recovered=0;
  for (int32_t lit : history.input().first(query->input_words)) {
    if (lit) raw.push_back(internalize(lit));
    else {
      writer->seed_population(++next_id, raw.data(), raw.size());
      imported_sources.push_back({next_id,history.input().subspan(clause_begin,word-clause_begin)});
      if (word >= epoch.input_words) suffix[appended++].id = next_id;
      else if (recovered<recovery.size() &&
               std::equal(recovery[recovered].clause.begin(),recovery[recovered].clause.end(),
                          history.input().begin()+clause_begin,history.input().begin()+word))
        recovery[recovered++].id=next_id;
      clause_begin=word+1;
      raw.clear();
    }
    ++word;
  }
  assert(recovered==recovery.size());
  std::vector<uint64_t> assumption_ids;
  for (int32_t lit : query->assumptions) {
    const Lit literal = internalize(lit);
    assumption_ids.push_back(++next_id);
    writer->seed_population(next_id, &literal, 1);
  }
  writer->finish_input_transaction();
  PbpParityBoundsCache parity_cache;
  // Clausal programs contain only licensed prefix conclusions and guarded SR
  // helpers. Every remaining helper clause is a weakening of its conclusion,
  // so it imposes no witness obligation beyond the checked prefix itself.
  // Algebraic support has additional proof-only constraints: isolate that path.
  const bool clausal_program=std::all_of(restricted_program.begin(),restricted_program.end(),
      [](const auto &step) {return step.consequence.parity_equations.empty();});
  if(clausal_program)writer->enable_query_names();
  bool proof_exhausted=false;
  const auto charge_proof=[&](uint64_t work) {
    if (interrupted()) return false;
    if (out.ticks>=native_limits_.proof_ticks || work>native_limits_.proof_ticks-out.ticks) {
      proof_exhausted=true;return false;
    }
    out.ticks+=work;return true;
  };
  if(restricted_program.empty()) {
    for (const auto &record:derivations)
      if (!emit_query_knowledge(*writer,record,next_id,query->query_variables,parity_cache,charge_proof))
        return fail(interrupted() ? NativeAdvanceStatus::Interrupted :
            proof_exhausted ? NativeAdvanceStatus::Exhausted : NativeAdvanceStatus::ProofUnavailable);
  } else for(const auto &step:restricted_program) {
    if(interrupted())return fail(NativeAdvanceStatus::Interrupted);
    const auto &record=step.consequence;
    if(step.restriction) {
      // Copy exactly the independently checked prefix. Old proof-only XOR
      // support and ignored deletions then cannot add witness obligations.
      if(!clausal_program && !writer->isolate_query(imported_sources,query->assumptions,namespace_extent))
        return fail(NativeAdvanceStatus::ProofUnavailable);
      std::vector<Lit> body,witness;
      std::vector<DsrLiteralMapping> mappings;
      for(int32_t l:record.literals)body.push_back(internalize(l));
      for(int32_t l:step.witness)witness.push_back(internalize(l));
      for(auto [from,to]:step.mappings)mappings.push_back({internalize(from),internalize(to)});
      // A checked implicit-pivot-only substitution is RAT. Explicit witnesses
      // and mappings still require the general SR encoder.
      if(witness.empty() && mappings.empty())
        writer->add_rat(++next_id,body.data(),body.size());
      else if(!writer->add_sr(++next_id,body.data(),body.size(),witness.data(),witness.size(),
          mappings.data(),mappings.size(),namespace_extent,true))return fail(NativeAdvanceStatus::ProofUnavailable);
    } else if(!emit_query_knowledge(*writer,record,next_id,query->query_variables,parity_cache,charge_proof))
      return fail(interrupted()?NativeAdvanceStatus::Interrupted:
          proof_exhausted?NativeAdvanceStatus::Exhausted:NativeAdvanceStatus::ProofUnavailable);
    imported_sources.push_back({next_id,record.literals});
  }
  for (size_t at:order) {
    const auto &fact=facts[at];
    if (!writer->add_external_rup(fact.id, fact.clause.data(), fact.clause.size()))
      return fail(NativeAdvanceStatus::ProofUnavailable);
  }
  if (proof_refuted) writer->preserve_query_input_refutation();
  // RUP needs no namespace copy. At the first witnessed step, the existing
  // refresh service isolates the then-live licensed sources from raw F+A and
  // old imports. Until then, avoid serializing substitutions for every clause.
  std::vector<QueryProofClause> namespace_sources;
  namespace_sources.reserve(facts.size()+suffix.size()+recovery.size());
  for (const auto &fact : facts) namespace_sources.push_back({fact.id, fact.clause});
  for (const auto &source : suffix) namespace_sources.push_back({source.id, source.clause});
  for (const auto &source : recovery) namespace_sources.push_back({source.id, source.clause});
  writer->enable_query_certificates(epoch, epoch.certificate_limits(), namespace_sources,
                                    query->assumptions, true);

  // All rejecting checks precede the native mutation boundary. The observer
  // remains bound to this kernel; replacing only its proof must not report a
  // lost solver capture interval.
  if (rollback) {
    const QueryPopulationRetirement retirement;
    retract_query_roots(k);
    // These sources belong to neither the replacement native population nor
    // its new certificate. Preserve the previous query's published bytes.
    std::unordered_set<uint64_t> licensed_ids;
    for (const auto &fact : facts) licensed_ids.insert(fact.id);
    for (const auto [ref,c] : arena_records(k.arena)) {
      (void)ref;
      if (!c->garbage() && !licensed_ids.count(c->id())) kernel_discard_query_clause(k,c,retirement);
    }
    std::vector<std::pair<Lit,Lit>> retired_binaries;
    for (const auto &[key,population] : epoch.weakening->binaries_) {
      if (std::all_of(population.copies.begin(),population.copies.end(),
          [&](const auto &copy) {return licensed_ids.count(copy.source.proof_id);})) continue;
      const Lit a=kernel_internalize(k,externalize(static_cast<Lit>(key>>32)));
      const Lit b=kernel_internalize(k,externalize(static_cast<Lit>(key)));
      assert(a!=INVALID_LIT && b!=INVALID_LIT);
      retired_binaries.emplace_back(a,b);
    }
    for (const auto &[a,b] : retired_binaries) {
      const bool removed=kernel_discard_query_binary_copies(k,a,b,retirement);
      assert(removed);(void)removed;
    }
    // Root collectors may have dropped individual large watches. Reconnect
    // every surviving clause now that root truth is no longer permanent.
    kernel_rebuild_large_watches(k);
    out.retracted = retracted;
  }
  epoch.proof->bind_population_observer(nullptr);
  epoch.proof->bind_derivation_observer(nullptr);
  epoch.proof = std::move(writer);
  k.proof = epoch.proof.get();
  k.proof->bind_variable_map(&k.internal_to_external);
  kernel_sync_population_observer(k);
  if (epoch.derivations) {
    epoch.derivations->start_query(query->id);
    k.proof->bind_derivation_observer(epoch.derivations.get());
  }
  k.next_clause_id = next_id;
  k.close_startup_certificate();
  k.query_projection = nullptr;
  epoch.projection.reset();
  k.dsr_root_values.clear();
  k.dsr_root_proof = nullptr;
  k.dsr_root_stream_identity = 0;
  for (Lit lit : native_roots) kernel_note_dsr_root_unit(k, lit);
  if (!rollback) (void)restart_apply_to(k, 0);
  // Structural proof/source caches belong to the old query generation. Drop
  // them after abandoning its trail; Kernel/H and the clausal population stay.
  epoch.services = std::make_unique<StructuralActionSessions>(k);
  const auto activate = [&](int32_t lit) {
    const auto result = admit_external_variable(k, lit);
    assert(result.status == NativeVariableStatus::Existing ||
           result.status == NativeVariableStatus::Reactivated ||
           result.status == NativeVariableStatus::Created ||
           result.status == NativeVariableStatus::Fixed);
    out.created += result.status == NativeVariableStatus::Created;
    return result.literal;
  };
  for (int32_t lit : used) if (!k.inconsistent) (void)activate(lit);
  if (rollback) {
    for (const auto &fact : facts) {
      if (k.inconsistent) break;
      if (fact.native && fact.clause.size()!=1) continue;
      install_external_source(k,fact.id,fact.clause,true,activate);
      if (!fact.native) ++out.restored;
    }
    for (const auto &source : recovery) {
      if (k.inconsistent) break;
      install_external_source(k,source.id,source.clause,true,activate);
      ++out.restored;
    }
    if (!k.inconsistent) (void)propagate_service(k);
  } else for (const auto &record : plan.selected()) {
    if (k.inconsistent) break;
    for (const auto &source : record.sources) {
      install_external_source(k, source.proof_id, record.clause, false, activate);
      ++out.restored;
      if (k.inconsistent) break;
    }
    if (!k.inconsistent) (void)propagate_service(k);
  }
  // A base refutation makes the still-uninstalled records immaterial. Keep
  // their history in that terminal case instead of claiming they were restored.
  if (!k.inconsistent) epoch.weakening->consume_restoration(k, plan);
  for (const auto &source : suffix) {
    if (k.inconsistent) break;
    install_external_source(k, source.id, source.clause, true, activate);
    ++out.appended;
    if (!k.inconsistent) (void)propagate_service(k);
  }
  epoch.input_words = query->input_words;
  k.input_header = {true, query->query_variables, query->clauses+query->assumptions.size()};
  k.model_extended = false;
  k.model_values.clear();
  ++k.strategy_epoch;
  epoch.assumption_ids = std::move(assumption_ids);
  epoch.assumption_refuted = false;
  epoch.failure = {};
  if (!query->assumptions.empty()) {
    epoch.projection = std::make_unique<QueryProjection>(history, query->id);
    k.query_projection = epoch.projection.get();
  }
  ++generation_;
  query_ = query->id;
  return fail(NativeAdvanceStatus::Ok);
}

bool QueryKernel::valid(KernelTicket t) const {
  return t.owner == this && t.generation == generation_ && t.query == query_ &&
         epoch_ && history_ && history_->active_query() &&
         history_->active_query()->id == query_;
}

KernelTicket QueryKernel::ticket() const {
  const KernelTicket t{this, generation_, query_};
  return valid(t) ? t : KernelTicket{};
}

Kernel *QueryKernel::kernel(KernelTicket t) {
  return valid(t) ? &epoch_->kernel : nullptr;
}

const Kernel *QueryKernel::kernel(KernelTicket t) const {
  return valid(t) ? &epoch_->kernel : nullptr;
}
StructuralActionSessions *QueryKernel::execution_services(KernelTicket t) {
  return valid(t)?epoch_->services.get():nullptr;
}

std::span<const uint64_t> QueryKernel::assumption_input_ids(KernelTicket t) const {
  return valid(t) ? std::span<const uint64_t>(epoch_->assumption_ids)
                  : std::span<const uint64_t>{};
}

std::span<const int32_t> QueryKernel::effective_assumptions(KernelTicket t) const {
  if (!valid(t)) return {};
  return epoch_->projection ? epoch_->projection->constraints()
                           : std::span<const int32_t>{};
}

ProjectionSnapshot QueryKernel::projection_state(KernelTicket t) const {
  if (!valid(t)) { ProjectionSnapshot out; out.restore = true; return out; }
  ProjectionSnapshot out = epoch_->projection ? epoch_->projection->snapshot() : ProjectionSnapshot{};
  out.restore |= epoch_->kernel.query_projection != epoch_->projection.get();
  return out;
}

DerivationSnapshot QueryKernel::derivation_state(KernelTicket t) const {
  return valid(t) && epoch_->derivations ? epoch_->derivations->snapshot() : DerivationSnapshot{};
}

ModelCheck QueryKernel::check_model(KernelTicket t) {
  if (!valid(t) || (epoch_->projection &&
      (epoch_->kernel.query_projection != epoch_->projection.get() ||
       epoch_->projection->snapshot().restore))) return {ModelStatus::InvalidQuery};
  Kernel &k = epoch_->kernel;
  if (epoch_->projection) return epoch_->projection->check_model(k);
  k.model_extended = false;
  kernel_extend_model_seeded(k, effective_assumptions(t));
  return history_->check_model(query_, [&](int32_t variable) {
    return k.model_values[internalize(variable)];
  });
}

std::optional<uint32_t> QueryKernel::assigned_assumption_floor(KernelTicket t) const {
  if (!valid(t)) return std::nullopt;
  const Kernel &k=epoch_->kernel;
  if (epoch_->projection &&
      (k.query_projection!=epoch_->projection.get() ||
       epoch_->projection->snapshot().restore ||
       epoch_->projection->snapshot().invalid_model)) return std::nullopt;
  uint32_t floor=0;
  for (const int external : effective_assumptions(t)) {
    const Lit literal=kernel_internalize(k,external);
    if (literal!=INVALID_LIT && k.value(literal)>0)
      floor=std::max(floor,k.var_level(var_of(literal)));
  }
  return floor;
}

AssumptionStep QueryKernel::next_assumption(KernelTicket t) const {
  if (!valid(t)) return {};
  const Kernel &k = epoch_->kernel;
  if (epoch_->projection && epoch_->projection->snapshot().invalid_model)
    return {AssumptionStatus::Invalid};
  // The projection owner licenses compatible reconstruction, translated
  // equivalences and cube-preserving restrictions. An incompatible effect
  // requires restoration; statistics reset cannot erase that obligation.
  if (epoch_->projection &&
      (k.query_projection != epoch_->projection.get() ||
       epoch_->projection->snapshot().restore))
    return {AssumptionStatus::Restore};
  if (epoch_->assumption_refuted)
    return {AssumptionStatus::QueryUnsat};
  if (k.inconsistent) return {AssumptionStatus::BaseUnsat};
  if (epoch_->projection && epoch_->projection->contradictory())
    return {AssumptionStatus::QueryUnsat};
  // Proved terminal states remain observable inside a phase. Restoration and
  // projection validity above still take precedence over such a verdict.
  if (query_boundary_busy(k))
    return {AssumptionStatus::Busy};
  if (k.conflict_valid) return {AssumptionStatus::Analyze};
  if (!kernel_propagation_fixpoint(k)) return {AssumptionStatus::Propagate};
  const auto assumptions = effective_assumptions(t);
  auto &cursor=epoch_->assumption_cursor;
  const uint64_t translations=epoch_->projection ?
      epoch_->projection->snapshot().translated_assumptions : 0;
  // Assignment appends preserve a satisfied cube prefix. Retraction, namespace
  // changes and equivalence translation invalidate it, including same-shape
  // public actions. Reuse existing semantic epochs; never add assignment hooks.
  if (!k.affine_capture_epochs_trusted || cursor.generation!=generation_ ||
      cursor.retraction!=k.affine_trail_retraction_epoch ||
      cursor.names!=k.affine_semantic_namespace_epoch ||
      cursor.translations!=translations || cursor.trail>k.trail.size())
    cursor={generation_,k.affine_trail_retraction_epoch,
            k.affine_semantic_namespace_epoch,translations,0,0,true};
  // Appending assignments cannot falsify an already assigned true cube.
  // Free reconstruction coordinates are excluded: N1 allows deciding them.
  if (cursor.assigned_cube) return {AssumptionStatus::Ready};
  // A root-falsified assumption refutes the cube even when an earlier member
  // is undecided. Scan once per root trail/semantic epoch, not once per member.
  // Above root an arbitrary client branch may falsify A, so keep the ordinary
  // decision-origin check below. Cache only absence, never a refutation.
  if (!k.level && cursor.root_trail!=k.trail.size()) {
    for (size_t i=0;i<assumptions.size();++i)
      if (kernel_external_value(k,internalize(assumptions[i]))<0)
        return {AssumptionStatus::QueryUnsat,i,assumptions[i]};
    cursor.root_trail=k.trail.size();
  }
  // A false assumption is a query refutation only when the current decisions
  // are assumptions themselves. An arbitrary client may have branched first;
  // that calls for backtracking, never an UNSAT answer from a wrong branch.
  const auto only_assumption_decisions = [&] {
    for (;cursor.trail<k.trail.size();++cursor.trail) {
      const Lit literal=k.trail[cursor.trail];
      const auto &assigned = k.assigned[var_of(literal)];
      const int external = kernel_externalize(k, literal);
      const bool decision=assigned.level && !assigned.binary && assigned.reason==DECISION_REASON;
      // N1 permits decisions on retired coordinates. Opposite-sign detection
      // covers every append; positive membership matters only for these two
      // cases, not ordinary non-assumption propagations.
      const bool opposite=epoch_->projection && epoch_->projection->is_assumption(-external);
      if (opposite || decision) {
        const bool requested=epoch_->projection && epoch_->projection->is_assumption(external);
        if (!requested) {
          if (opposite) cursor.assumption=0;
          if (decision) cursor.only_assumptions=false;
        }
      }
    }
    return cursor.only_assumptions;
  };
  const bool assumption_decisions=assumptions.empty() || only_assumption_decisions();
  for (size_t &i=cursor.assumption; i < assumptions.size(); ++i) {
    const int32_t external = assumptions[i];
    const Lit literal = kernel_internalize(k, external);
    const Lit raw = internalize(external);
    const Var ev = var_of(raw);
    const bool fixed = ev < k.fixed_external_values.size() &&
                       k.fixed_external_values[ev];
    if (!fixed && (literal == INVALID_LIT ||
        (kernel_var_retired(k, var_of(literal)) && !k.value(literal))))
    {
      cursor.skipped_free=true;
      continue; // licensed free coordinate; seed it before reconstructing rho
    }
    const int8_t value = kernel_external_value(k, raw);
    if (value > 0) continue;
    if (!assumption_decisions)
      return {AssumptionStatus::Backtrack, i, external};
    return {value < 0 ? AssumptionStatus::QueryUnsat : AssumptionStatus::Decide,
            i, external};
  }
  cursor.assigned_cube=!cursor.skipped_free;
  return {AssumptionStatus::Ready};
}

bool QueryKernel::decide_assumption(KernelTicket t, size_t index) {
  const AssumptionStep next = next_assumption(t);
  if (next.status != AssumptionStatus::Decide || next.index != index)
    return false;
  Kernel &k = epoch_->kernel;
  return decide_apply(k, kernel_internalize(k, next.literal), false) ==
         ApplyResult::Ok;
}

bool QueryKernel::publish_assumption_refutation(KernelTicket t) {
  const auto next=next_assumption(t);
  if (next.status != AssumptionStatus::QueryUnsat)
    return false;
  if (!epoch_->assumption_refuted) {
    epoch_->failure=next;
    epoch_->proof->add_empty(++epoch_->kernel.next_clause_id);
    epoch_->assumption_refuted = true;
  }
  epoch_->proof->flush();
  return true;
}

bool QueryKernel::flush_proof(KernelTicket t) {
  if (!valid(t)) return false;
  epoch_->proof->flush();
  return true;
}

ResultCaptureStatus QueryKernel::finish_query(QueryHistory &history,QueryResult answer) {
  return finish_query(history,answer,{native_limits_.proof,native_limits_.proof_ticks});
}

ResultCaptureStatus QueryKernel::finish_query(QueryHistory &history,QueryResult answer,
    const CoreLimits &limits,const volatile sig_atomic_t *stop) {
  if(&history!=history_)return ResultCaptureStatus::WrongHistory;
  const auto t=ticket();
  if(!valid(t))return ResultCaptureStatus::NoActiveQuery;
  if(answer!=QueryResult::Unknown && answer!=QueryResult::Sat && answer!=QueryResult::Unsat)
    return ResultCaptureStatus::InvalidResult;
  if(answer!=QueryResult::Unknown && stop&&*stop)return ResultCaptureStatus::Interrupted;
  if(answer!=QueryResult::Unknown && query_boundary_busy(epoch_->kernel))
    return ResultCaptureStatus::NotReady;
  const auto *query=history.active_query();
  auto next=std::make_shared<QueryResultSnapshot>();
  next->query=query->id;next->generation=t.generation;next->input_words=query->input_words;next->result=answer;
  if(answer==QueryResult::Sat) {
    if(check_model(t).status!=ModelStatus::Valid)return ResultCaptureStatus::InvalidModel;
    next->model.resize(query->query_variables);
    for(uint32_t v=0;v<query->query_variables;++v)
      next->model[v]=epoch_->kernel.model_values[make_lit(v,false)];
  } else if(answer==QueryResult::Unsat) {
    const auto step=next_assumption(t);
    if(step.status!=AssumptionStatus::BaseUnsat && step.status!=AssumptionStatus::QueryUnsat)
      return ResultCaptureStatus::NotReady;
    const bool base=step.status==AssumptionStatus::BaseUnsat;
    if(!base && !epoch_->assumption_refuted && !publish_assumption_refutation(t))
      return ResultCaptureStatus::NotReady;
    const auto &failure=epoch_->failure;
    next->core=prepare_failed_core(epoch_->kernel,history,*query,effective_assumptions(t),
        failure.literal,failure.index,base,epoch_->derivations?epoch_->derivations->records():std::span<const DerivationRecord>{},limits,stop);
    next->failed=next->core.assumptions;
    std::sort(next->failed.begin(),next->failed.end());
  }
  drain_learned(t, stop);
  // A learning/licensing boundary can poll termination after the caller's
  // final pre-capture poll. Do not consume A as a terminal result in that case.
  if (answer != QueryResult::Unknown && (callback_stop_ || (stop && *stop)))
    return ResultCaptureStatus::Interrupted;
  if(answer==QueryResult::Unknown)epoch_->proof->flush();
  else epoch_->proof->finish_query_output();
  const auto status=history.finish_query(query->id,answer);
  assert(status==InputStatus::Ok);(void)status;
  result_=std::move(next);
  return ResultCaptureStatus::Ok;
}

const QueryResultSnapshot *QueryKernel::result_snapshot() const {
  if(!history_ || !result_)return nullptr;
  const auto *query=history_->result_query();
  return query && query->id==result_->query && query->result==result_->result ? result_.get():nullptr;
}

int32_t QueryKernel::model_literal(int32_t external) const {
  const auto *r=result_snapshot();
  if(!r || r->result!=QueryResult::Sat || !external || external==INT32_MIN)return 0;
  const auto v=external_variable_of(external);
  if(v>r->model.size() || !r->model[v-1])return 0;
  return r->model[v-1]>0?int32_t(v):-int32_t(v);
}

bool QueryKernel::failed_literal(int32_t external) const {
  const auto *r=result_snapshot();
  return r && r->failed_literal(external);
}

std::span<const RetainedClause> QueryKernel::retained_clauses() const {
  return retained_ ? retained_->clauses() : std::span<const RetainedClause>{};
}

void QueryKernel::set_learn(void *state, int maximum_length,
                            void (*learn)(void *, int *)) {
  if (!learning_ && learn) learning_ = std::make_unique<LearnedClauseExport>(native_limits_.proof);
  if (learning_) learning_->set(state, maximum_length, learn);
}
void QueryKernel::set_learn_budget(uint64_t ticks) {
  if (!learning_) learning_ = std::make_unique<LearnedClauseExport>(native_limits_.proof);
  learning_->set_budget(ticks);
}
void QueryKernel::drain_learned(KernelTicket t, const volatile sig_atomic_t *stop) {
  if (learning_ && valid(t) && epoch_->derivations)
    learning_->drain(*history_, query_, epoch_->derivation_stream,
        epoch_->derivations->records(), epoch_->derivations->snapshot().dropped, stop,
        [](void *owner) { return static_cast<QueryKernel *>(owner)->poll_terminate(); }, this);
}

RetentionBatch QueryKernel::retain_pending(KernelTicket t, uint64_t ticks_budget,
                                          const volatile sig_atomic_t *stop) {
  RetentionBatch out;
  if (!valid(t)) { out.stale = true; return out; }
  if (!retained_ || !epoch_->capture) return out;
  ClauseCapture &capture = *epoch_->capture;
  out.capture_dropped = capture.dropped;
  size_t consumed = 0;
  for (const auto &clause : capture.pending) {
    if (out.ticks >= ticks_budget) { out.exhausted = true; break; }
    const RetentionResult result = retained_->admit(
        *history_, query_, clause, ticks_budget - out.ticks, stop);
    out.ticks += std::min(result.ticks, UINT64_MAX - out.ticks);
    switch (result.status) {
    case RetentionStatus::Accepted: ++out.accepted; break;
    case RetentionStatus::AlreadyPresent: ++out.duplicates; break;
    case RetentionStatus::NotRup:
    case RetentionStatus::Tautology:
    case RetentionStatus::Invalid: ++out.rejected; break;
    case RetentionStatus::Interrupted: out.interrupted = true; break;
    case RetentionStatus::Exhausted: out.exhausted = true; break;
    default: out.stale = true; break;
    }
    if (out.interrupted || out.exhausted || out.stale) break;
    capture.literals -= clause.size();
    ++consumed;
  }
  capture.pending.erase(capture.pending.begin(), capture.pending.begin() + consumed);
  out.pending = capture.pending.size();
  return out;
}

} // namespace tactix::session
