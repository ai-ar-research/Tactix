// Behavior-preserving observation seam for raw, checker, and solver clause
// populations.  The observer is deliberately not a ledger: views are valid
// only for the duration of a noexcept callback and no ownership is transferred.
#pragma once

#include "kernel/types.hpp"

#include <cstdint>
#include <limits>
#include <vector>

namespace tactix {

struct Kernel;

// A callback-local view that can describe solver literals, proof-only DIMACS
// literals, and the one-prefix/one-suffix helpers emitted by native DSR.  It
// exposes stable external DIMACS identities without allocating a conversion
// buffer.  `internal_to_external` may be null only for the identity map.
struct PopulationClauseView {
  const Lit *internal = nullptr;
  const int32_t *external = nullptr;
  const std::vector<Var> *internal_to_external = nullptr;
  uint32_t body_size = 0;
  // When set, expose this internal body literal first and retain the relative
  // order of every other body literal. Native substitution-redundancy lines can
  // repivot the checker
  // clause before appending witness metadata; the observer must see that exact
  // clause order and none of the witness.
  uint32_t first_body_index = std::numeric_limits<uint32_t>::max();
  int32_t prefix = 0;
  int32_t suffix = 0;

  uint32_t size() const noexcept {
    return body_size + static_cast<uint32_t>(prefix != 0) +
           static_cast<uint32_t>(suffix != 0);
  }

  int32_t at(uint32_t index) const noexcept {
    if (prefix) {
      if (!index)
        return prefix;
      --index;
    }
    if (index < body_size) {
      if (first_body_index < body_size) {
        if (!index)
          index = first_body_index;
        else if (index <= first_body_index)
          --index;
      }
      if (external)
        return external[index];
      Lit literal = internal[index];
      if (internal_to_external) {
        const Var variable = var_of(literal);
        if (variable >= internal_to_external->size())
          return 0;
        literal = make_lit((*internal_to_external)[variable],
                           negated(literal));
      }
      return externalize(literal);
    }
    return suffix;
  }

  static PopulationClauseView internal_lits(
      const Lit *literals, uint32_t count,
      const std::vector<Var> *map = nullptr) noexcept {
    PopulationClauseView result;
    result.internal = literals;
    result.internal_to_external = map;
    result.body_size = count;
    return result;
  }

  static PopulationClauseView external_lits(const int32_t *literals,
                                             uint32_t count) noexcept {
    PopulationClauseView result;
    result.external = literals;
    result.body_size = count;
    return result;
  }
};

enum class RawInputEventKind : uint8_t { Clause, Outcome, Finished };

enum RawInputDisposition : uint32_t {
  RawNone = 0,
  RawAdmitted = 1u << 0,
  RawDropped = 1u << 1,
  RawRepeatedLiteral = 1u << 2,
  RawTautology = 1u << 3,
  RawRootSatisfied = 1u << 4,
  RawRootShortened = 1u << 5,
  RawDuplicate = 1u << 6,
  RawEmpty = 1u << 7,
  RawUnit = 1u << 8,
  RawBinary = 1u << 9,
  RawLarge = 1u << 10,
  RawContradiction = 1u << 11,
  RawAlreadyInconsistent = 1u << 12,
};

enum class PopulationSourceClassification : uint8_t {
  PendingInputClassification,
  Eligible,
  Ineligible,
};

struct RawInputPopulationEvent {
  RawInputEventKind kind = RawInputEventKind::Clause;
  uint64_t raw_input_id = 0;
  uint32_t disposition = RawNone;
  bool normalized_available = false;
  PopulationSourceClassification source_classification =
      PopulationSourceClassification::PendingInputClassification;
  PopulationClauseView raw;
  PopulationClauseView normalized;
};

enum class CheckerRoute : uint8_t {
  None,
  Dsr,
  Drat,
  Lrat,
  Pbp,
};

enum class CheckerNaming : uint8_t { None, LiteralMultiset, Id };

enum class CheckerPopulationEventKind : uint8_t {
  ObserverBound,
  ObserverUnbound,
  ObserverUnboundIncomplete,
  RouteOpened,
  RouteCommitted,
  ClausalRouteDiscarded,
  InputSeed,
  InputFinished,
  Add,
  Delete,
  DeleteRequested,
  StageAdd,
  StageDelete,
  StageDeleteRequested,
  PendingCommitBegin,
  PendingDeletesCommit,
  PendingCommitEnd,
  IdRebind,
};

enum class CheckerClauseRole : uint8_t {
  None,
  Input,
  Rup,
  Rat,
  SubstitutionRedundancy,
  Empty,
  DerivedUnit,
  ExternalRup,
  ExternalRat,
  DsrSplitHelper,
  DsrSubstitutionHelper,
};

struct CheckerPopulationEvent {
  CheckerPopulationEventKind kind = CheckerPopulationEventKind::Add;
  CheckerClauseRole role = CheckerClauseRole::None;
  CheckerRoute route = CheckerRoute::None;
  CheckerNaming naming = CheckerNaming::None;
  uint64_t line_id = 0;
  uint64_t checker_id = 0;
  uint64_t predecessor_id = 0;
  uint64_t transaction_id = 0;
  bool proof_only = false;
  PopulationSourceClassification source_classification =
      PopulationSourceClassification::Ineligible;
  PopulationClauseView clause;
};

enum class SolverPopulationEventKind : uint8_t {
  Add,
  Delete,
  UnitMaterialized,
  MetadataUpdate,
  TransactionBegin,
  StageAdd,
  StageDelete,
  TransactionCommit,
};

enum class SolverClauseKind : uint8_t { Empty, Unit, Binary, Large };

struct SolverPopulationEvent {
  SolverPopulationEventKind kind = SolverPopulationEventKind::Add;
  SolverClauseKind clause_kind = SolverClauseKind::Empty;
  uint64_t logical_id = 0;
  uint32_t copies = 1;
  bool redundant = false;
  uint64_t transaction_id = 0;
  PopulationClauseView clause;
};

enum class SolverReplacementPhase : uint8_t { Begin, End };
enum class SolverReplacementOutcome : uint8_t {
  Pending,
  Refuted,
  Unit,
  Binary,
  Large,
};

struct SolverReplacementEvent {
  SolverReplacementPhase phase = SolverReplacementPhase::Begin;
  SolverReplacementOutcome outcome = SolverReplacementOutcome::Pending;
  uint64_t predecessor_id = 0;
  uint64_t replacement_id = 0;
  PopulationClauseView predecessor;
  PopulationClauseView replacement;
};

// Callbacks must neither throw nor retain a view.  A future bounded ledger can
// preallocate its records before installing this seam; the null path performs
// only one predictable pointer test at each authoritative mutation endpoint.
class PopulationObserver {
public:
  virtual ~PopulationObserver() = default;
  virtual void observe(const RawInputPopulationEvent &) noexcept {}
  virtual void observe(const CheckerPopulationEvent &) noexcept {}
  virtual void observe(const SolverPopulationEvent &) noexcept {}
  virtual void observe(const SolverReplacementEvent &) noexcept {}
};

// Install one non-owning observer on the kernel and its currently attached
// Proof.  The returned pointer is the previous binding.
PopulationObserver *kernel_bind_population_observer(
    Kernel &, PopulationObserver *) noexcept;

// Reconcile a Proof pointer assigned by existing frontend/test code with the
// kernel binding.  This does not emit a second bind event when already synced.
void kernel_sync_population_observer(Kernel &) noexcept;

// Settles pending transactions for an already-selected proof route while the
// observer is still alive, then restores the previous binding. An unpublished
// streaming-PBP prefix reports an incomplete unbind. Construct this after
// both Kernel and Proof when their destruction order does not already
// guarantee that Proof dies before the observer.
class ScopedPopulationObserverBinding {
public:
  ScopedPopulationObserverBinding(Kernel &, PopulationObserver &) noexcept;
  ~ScopedPopulationObserverBinding();
  ScopedPopulationObserverBinding(const ScopedPopulationObserverBinding &) =
      delete;
  ScopedPopulationObserverBinding &
  operator=(const ScopedPopulationObserverBinding &) = delete;

private:
  Kernel *kernel_ = nullptr;
  PopulationObserver *previous_ = nullptr;
};

} // namespace tactix
