#pragma once

#include "session/query_history.hpp"
#include "session/retained_clauses.hpp"
#include "session/query_projection.hpp"
#include "session/weakening_history.hpp"
#include "session/derivation_history.hpp"
#include "session/result_snapshot.hpp"
#include "session/learned_clause_export.hpp"

#include <memory>
#include <optional>
#include <span>

namespace tactix {
struct Kernel;
class Proof;
struct StructuralActionSessions;
namespace session {

class QueryKernel;

// A kernel-local instance/reason cannot survive restoration or be used through
// another session. A ticket is checked before the owner exposes its kernel.
struct KernelTicket {
  const QueryKernel *owner = nullptr;
  uint64_t generation = 0;
  uint64_t query = 0;
};

enum class ReplayStatus : uint8_t {
  Ok,
  NoActiveQuery,
  WrongHistory,
  Busy,
  ProofUnavailable,
  Exhausted,
  Interrupted
};

enum class AssumptionStatus : uint8_t {
  Ready,
  Decide,
  Propagate,
  Analyze,
  Backtrack,
  Restore,
  BaseUnsat,
  QueryUnsat,
  Busy,
  Stale,
  Invalid
};

struct AssumptionStep {
  AssumptionStatus status = AssumptionStatus::Stale;
  size_t index = 0; // zero-based index in the query's exact assumption list
  int32_t literal = 0; // permanent external literal, never a compact index
};

struct NativeQueryLimits {
  WeakeningLimits weakening;
  RetentionLimits proof;
  uint64_t proof_ticks = 0;
  bool enabled() const {
    return weakening.records && weakening.literals && weakening.live_binary_copies &&
           proof.enabled() && proof_ticks;
  }
};

enum class NativeAdvanceStatus : uint8_t {
  Ok, Replayed, NoActiveQuery, WrongHistory, Busy, ReplayRequired,
  ProofUnavailable, Exhausted, Interrupted, NotRequired
};
struct NativeAdvanceResult {
  NativeAdvanceStatus status = NativeAdvanceStatus::ReplayRequired;
  uint64_t restored = 0, checked = 0, ticks = 0;
  uint64_t appended = 0, created = 0;
  uint64_t derivations = 0;
  uint64_t retracted = 0; // unlicensed old native facts (including root facts)
};

// Shared query admission, restoration and assumption execution, independent of
// the strategy driving the rule registry. Native advance retains Kernel/H;
// replay is the explicit recovery path and imports F into a fresh epoch.
// Assumptions are checker premises but enter search only as nonroot decisions.
// The IPASIR transport and custom strategies share this service.
class QueryKernel {
public:
  QueryKernel();
  explicit QueryKernel(NativeQueryLimits);
  ~QueryKernel();
  QueryKernel(const QueryKernel &) = delete;
  QueryKernel &operator=(const QueryKernel &) = delete;
  QueryKernel(QueryKernel &&) = delete;
  QueryKernel &operator=(QueryKernel &&) = delete;

  // A supplied writer must be freshly opened and unused. Ownership transfers
  // to the epoch. Without one, open the same search-facing writer nowhere.
  // Failed preparation leaves the previous kernel epoch and tickets intact.
  // The history must outlive this owner and its outstanding tickets.
  ReplayStatus replay(const QueryHistory &);
  ReplayStatus replay(const QueryHistory &, std::unique_ptr<Proof> writer);
  // Abandon a failed execution after all borrowed views are destroyed. Keep
  // callbacks and independently retained knowledge; the next entry replays F.
  void discard_epoch();
  // Explicit native query transition: validate a fresh exact-query proof,
  // restart to root, selectively restore, and bind the new assumption cube.
  // Keeps the same Kernel/H and admits the append-only raw suffix and new
  // client coordinates. Captured weakening/repair history and independently certified
  // native facts are supported; other provenance/namespaces request replay.
  NativeAdvanceResult advance(const QueryHistory &);
  NativeAdvanceResult advance(const QueryHistory &, std::unique_ptr<Proof> writer,
      const volatile sig_atomic_t *stop = nullptr);
  // Explicit repair after next_assumption reports Restore, before publishing
  // a terminal result. Reuses the same query ID/F/A and Kernel/H, preparing a
  // fresh exact-query certificate and projection. Success stales old tickets;
  // a rejected preflight preserves the old ticket and repair obligation.
  NativeAdvanceResult repair(const QueryHistory &);
  NativeAdvanceResult repair(const QueryHistory &, std::unique_ptr<Proof> writer,
      const volatile sig_atomic_t *stop = nullptr);
  KernelTicket ticket() const;
  bool owns_query(KernelTicket t,const QueryHistory &h) const {return history_==&h && valid(t);}
  Kernel *kernel(KernelTicket);
  const Kernel *kernel(KernelTicket) const;
  StructuralActionSessions *execution_services(KernelTicket);
  std::span<const int32_t> effective_assumptions(KernelTicket) const;
  ProjectionSnapshot projection_state(KernelTicket) const;
  DerivationSnapshot derivation_state(KernelTicket) const;
  // Explicitly reconstruct and validate the complete original-query model.
  // Licensed retired coordinates receive effective-cube seeds, not root units.
  ModelCheck check_model(KernelTicket);

  // The caller explicitly schedules the returned transition. No propagation,
  // analysis, restart or restoration is hidden inside this inspection.
  AssumptionStep next_assumption(KernelTicket) const;
  // Maximum level of currently true effective assumptions, ignoring free
  // reconstruction coordinates. Pure observation, not a Ready certificate.
  std::optional<uint32_t> assigned_assumption_floor(KernelTicket) const;
  bool decide_assumption(KernelTicket, size_t index);
  // Publish a query refutation only after next_assumption proves QueryUnsat.
  // This does not set Kernel::inconsistent or make assumptions base units.
  bool publish_assumption_refutation(KernelTicket);
  bool flush_proof(KernelTicket);
  // Capture the terminal model/core before consuming A and completing history.
  // No native search or proof event depends on optional core minimization.
  // Status can be observed inside a phase; terminal capture waits for its exit.
  ResultCaptureStatus finish_query(QueryHistory &,QueryResult);
  ResultCaptureStatus finish_query(QueryHistory &,QueryResult,const CoreLimits &,
      const volatile sig_atomic_t *stop = nullptr);
  const QueryResultSnapshot *result_snapshot() const;
  std::shared_ptr<const QueryResultSnapshot> own_result_snapshot() const {
    return result_snapshot()?result_:nullptr;
  }
  int32_t model_literal(int32_t external) const; // signed assignment, same for +/-v
  bool failed_literal(int32_t external) const;
  // Session-owned callback state. A setter preserves the current result view;
  // the frontend resets the stop latch once per valid solve, before admission.
  void set_terminate(void *state,int (*terminate)(void *)) {
    terminate_state_=state;terminate_=terminate;
  }
  void reset_termination() { callback_stop_=0; }
  bool termination_enabled() const { return terminate_!=nullptr; }
  bool termination_requested() const { return callback_stop_!=0; }
  bool poll_terminate(volatile sig_atomic_t *stop=nullptr) const {
    if(!callback_stop_ && terminate_ && terminate_(terminate_state_))callback_stop_=1;
    if(callback_stop_ && stop)*stop=1;
    return callback_stop_;
  }
  void set_learn(void *state, int maximum_length, void (*learn)(void *, int *));
  void set_learn_budget(uint64_t ticks);
  void drain_learned(KernelTicket, const volatile sig_atomic_t *stop = nullptr);
  LearningSnapshot learning_snapshot() const {
    return learning_ ? learning_->snapshot() : LearningSnapshot{};
  }

private:
  struct Epoch;
  std::unique_ptr<Epoch> epoch_;
  NativeQueryLimits native_limits_;
  const QueryHistory *history_ = nullptr;
  uint64_t generation_ = 0;
  uint64_t query_ = 0;
  std::shared_ptr<const QueryResultSnapshot> result_;
  std::unique_ptr<LearnedClauseExport> learning_;
  void *terminate_state_=nullptr;
  int (*terminate_)(void *)=nullptr;
  mutable volatile sig_atomic_t callback_stop_=0;
  bool valid(KernelTicket) const;
  NativeAdvanceResult transition(const QueryHistory &, std::unique_ptr<Proof>,
      const volatile sig_atomic_t *, bool repair);
};

} // namespace session
} // namespace tactix
