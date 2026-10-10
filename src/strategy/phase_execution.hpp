#pragma once

#include "strategy/execution_scope.hpp"
#include "strategy/guidance_execution.hpp"
#include "strategy/occurrence_execution.hpp"
#include "strategy/representation_execution.hpp"
#include "strategy/root_execution.hpp"

#include <cassert>
#include <optional>

namespace tactix::execution {

// The inprocessing phase bracket: Kissat's simplifier entry/exit around one
// probe.c / eliminate.c pipeline (dense.c enter/resume sparse mode). Entry
// enters the inprocessing context, remembers the sparse-resume cursor,
// disconnects large watches for Eliminate and installs the Factor selection.
// The occurrence session opens separately (structured startup runs between
// entry and the session). Exit flushes retired redundant clauses outside
// Eliminate, publishes retirements, closes the session, reconnects watches,
// resumes sparse propagation after Eliminate, clears the selection and leaves
// the context. None is a proof rule; the sparse resume is M1 Propagate.
struct InprocessPhaseEntryRequest {
  bool eliminate = false;
  FactorSelection &selection;
  std::optional<uint32_t> bound;
};

// Public support argument (TACTIX_SUPPORT_ENTER_INPROCESS_PHASE): the
// TACTIX_PHASE_ELIMINATE flag in bit 0 and the default Factor selection's
// bound in the high 32 bits.
inline constexpr uint64_t PHASE_ELIMINATE = 1;
constexpr uint64_t inprocess_phase_argument(bool eliminate, uint32_t factor_bound) {
  return (eliminate ? PHASE_ELIMINATE : 0) | (static_cast<uint64_t>(factor_bound) << 32);
}

class InprocessPhase {
public:
  InprocessPhase() = default;
  InprocessPhase(const InprocessPhase &) = delete;
  InprocessPhase &operator=(const InprocessPhase &) = delete;
  // An abandoned bracket restores the scopes it owns, as the native RAII
  // scopes did on an early return.
  ~InprocessPhase() {
    occurrences_.reset();
    if (kernel_) leave_execution_context(*kernel_, ExecutionContext::Inprocessing, context_);
  }

  bool open() const { return kernel_ != nullptr; }
  bool eliminate() const { return eliminate_; }
  bool occurrences_open() const { return occurrences_.has_value(); }
  uint32_t sparse_resume_trail() const { return sparse_resume_trail_; }

  void enter(Kernel &k, const InprocessPhaseEntryRequest &request) {
    assert(!open());
    context_ = enter_execution_context(k, ExecutionContext::Inprocessing);
    kernel_ = &k;
    eliminate_ = request.eliminate;
    // Kissat clears the propagated root trail here; Tactix keeps its
    // immutable prefix and resumes from the equivalent cursor at exit.
    sparse_resume_trail_ = static_cast<uint32_t>(k.trail.size());
    // kissat_enter_dense_mode disconnects large watches for Eliminate only;
    // Probe rules own their own sparse-watch transitions.
    if (request.eliminate) execute_action(k, DisconnectLargeWatchesRequest{});
    execute_action(k, FactorSelectionInstallationRequest{request.selection, request.bound});
  }

  void open_occurrences(Kernel &k, OccIndex &index) {
    assert(open() && !occurrences_);
    occurrences_.emplace(k, OccurrenceSessionRequest{
                                index, OccurrenceAttachment::Build, eliminate_});
  }

  // Exit after a structured-startup SAT terminal: no session was opened, so
  // only the Factor handoff and the context are released.
  void exit_terminal(Kernel &k) {
    assert(open() && !occurrences_);
    execute_action(k, ClearFactorSelectionRequest{});
    close_context(k);
  }

  // `publish_retirements()` delivers retirement events to guidance;
  // `propagate()` is the root Propagate of the sparse resume. Returns Unsat
  // when the phase is or becomes refuted.
  template <class PublishRetirements, class Propagate>
  ApplyResult exit(Kernel &k, bool refuted, PublishRetirements &&publish_retirements,
                   Propagate &&propagate) {
    assert(open() && occurrences_);
    // Eliminate's sparse resume performs the single arena-order collection;
    // other groups flush redundant clauses on retired variables.
    if (!refuted && !eliminate_) execute_action(k, FlushRetiredRedundantRequest{});
    publish_retirements();
    occurrences_->close();
    occurrences_.reset();
    // Only Eliminate reconnects at the pipeline level (eliminate.c); Probe
    // keeps the pose its last sparse propagation left unless a rule left the
    // watches disconnected.
    if (eliminate_ || !k.large_watches_connected)
      execute_action(k, ReconnectLargeWatchesRequest{eliminate_});
    if (eliminate_ && !refuted) {
      // dense.c kissat_resume_sparse_mode uses search propagation.
      const ApplyResult replay = execute_action(
          k, SparseResumeRequest{sparse_resume_trail_, true}, propagate);
      if (replay == ApplyResult::Unsat || k.inconsistent) refuted = true;
      else assert(replay == ApplyResult::Ok);
    }
    execute_action(k, ClearFactorSelectionRequest{});
    close_context(k);
    return refuted ? ApplyResult::Unsat : ApplyResult::Ok;
  }

private:
  void close_context(Kernel &k) {
    assert(kernel_ == &k);
    leave_execution_context(k, ExecutionContext::Inprocessing, context_);
    kernel_ = nullptr;
  }

  Kernel *kernel_ = nullptr;
  ExecutionContextState context_;
  bool eliminate_ = false;
  uint32_t sparse_resume_trail_ = 0;
  std::optional<OccurrenceSession> occurrences_;
};

} // namespace tactix::execution
