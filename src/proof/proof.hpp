// Proof backend (plan §5.2): rules emit typed logical events. Native DSR is
// the canonical stream (witnessless RUP/RAT additions, direct SR additions
// with a substitution witness), streamed, never held in memory (§6).
#pragma once

#include "kernel/types.hpp"
#include "proof/dsr_to_pbp.hpp"
#include "population/population_observer.hpp"
#include "proof/derivation_observer.hpp"
#include "proof/parity_equation.hpp"

#include <array>
#include <compare>
#include <functional>
#include <map>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tactix {

enum class ProofFormat { DsrBinary, DsrAscii, Pbp, PbpStreaming };

// One route for a proof that cannot be written: say why and stop, with no verdict.
[[noreturn]] void proof_error(const char *what);

struct DsrLiteralMapping {
  Lit from = INVALID_LIT;
  Lit to = INVALID_LIT;
};

struct QueryProofClause {
  uint64_t id = 0;
  std::span<const int32_t> literals;
};

struct QueryProofOwnedClause {
  uint64_t id = 0;
  std::vector<int32_t> literals;
};

// Cold native snapshot at a witnessed publication boundary. The provider owns
// solver liveness; Proof separately authenticates every copied source.
class QueryProofSources {
public:
  virtual ~QueryProofSources() = default;
  virtual bool snapshot(std::vector<QueryProofOwnedClause> &,
      std::vector<int32_t> &assumptions, uint32_t &extent) const = 0;
};

struct QueryProofLimits {
  uint32_t variables = 0;
  uint64_t clauses = 0, literals = 0, work = 0;
};
struct QueryProofSnapshot {
  uint64_t generations = 0, work = 0;
};

// The semantic pivot override in an encoding shared by dsr-trim and Trestle,
// classified relative to !C: unmapped pivot Direct, a true image Repivot,
// all-false Rup, one open image Lemma, several a temporary Extension. Every
// form preserves the X7 application (calculus.md §6d).
enum class DsrWitnessForm : uint8_t { Direct, Repivot, Rup, Lemma, Extension };

struct DsrWitnessPlan {
  DsrWitnessForm form = DsrWitnessForm::Direct;
  uint32_t pivot = 0;      // clause index of the emitted pivot
  Lit lemma = INVALID_LIT; // Lemma: open image appended to the clause
};

// Bounds of an authenticated complete XOR encoding; source ownership remains
// the caller's obligation in every certificate format.
struct PbpParityBounds {
  uint64_t lower = 0;
  uint64_t upper = 0;
  std::vector<int32_t> carries;
  // Which numbering wrote lower and upper; a row from another numbering is
  // written again before its next use.
  uint64_t numbering = 0;
};

class PbpParityBoundsCache {
public:
  uint64_t generation() const noexcept { return generation_; }
  uint64_t stream_identity() const noexcept { return stream_identity_; }
  size_t size() const noexcept { return entries_.size(); }
  uint64_t retained_bytes() const noexcept;

private:
  friend class Proof;
  static uint64_t empty_retained_bytes() noexcept;
  uint64_t stream_identity_ = 0;
  uint64_t generation_ = 0;
  std::map<PbpParityEquation, PbpParityBounds> entries_;
  // Cached value of the retained_bytes() scan, excluded from the logical base.
  uint64_t retained_bytes_ = 0;
};

struct PbpParityReasonPlan {
  uint64_t stream_identity = 0;
  uint64_t cache_generation = 0;
  uint64_t proof_constraints = 0;
  uint64_t retained_bytes = 0;
  uint32_t starting_external_extent = 0;
  uint32_t helper_variables = 0;
  std::vector<int32_t> odd_variables;
  bool valid = false;
};

// `lits[0]` is the semantic pivot; `mappings` use positive sources.
DsrWitnessPlan plan_dsr_witness(const Lit *lits, uint32_t size,
                                const Lit *true_lits, uint32_t ntrue,
                                const DsrLiteralMapping *mappings,
                                uint32_t nmappings);

// Automatic proof selection (plan §5): whole-run streaming PBP owns every
// kernel-visible decision (trajectory equals `--pbp`) and a native DSR copy is
// the proof, unless a startup PB refutation, frequent parity reasons, or an
// event the copy cannot follow converts it to PBP.
enum class AutomaticProofReason : uint8_t {
  Pending,              // the DSR copy is still live
  DsrSelected,          // the empty clause kept the DSR copy
  StartupPbRefutation,  // Lucky parity / count-preservation terminal PB
  ParityReason,         // compact GF(2) reason (affine root closure, search)
  CopyDeclined,         // the DSR copy rejected an event PBP accepted
  CopyWriteFailed,      // the DSR stage could not be written
  DestinationUnsupported, // not a regular file: no copy was opened
  StageUnavailable,     // no stage file could be created
  ForcedSwitch,         // TACTIX_AUTO_PROOF_SWITCH_AT (validation only)
};
const char *automatic_proof_reason_name(AutomaticProofReason) noexcept;

struct LazyPbpReport {
  // off, dsr (still DSR), converted, dsr-kept, startup-pb, fallback, or why the
  // run stayed eager (off-*).
  const char *state = "off";
  uint64_t dsr_bytes = 0; // DSR bytes converted at the switch
  uint64_t pbp_bytes = 0; // PBP text the conversion wrote, or that DSR replaced
  uint64_t nanos = 0;     // time spent converting
};

class Proof {
public:
  Proof(); // defined out of line: LazyPbp is complete only in proof.cpp
  ~Proof();

  bool open(const char *path, ProofFormat format);
  // The DSR writer alone, for its own tests. A production kernel never holds
  // one: open() always gives the kernel the streaming-PBP writer.
  bool open_dsr_writer(const char *path, ProofFormat format);
  // Automatic selection: streaming PBP at `path` plus a binary (or ASCII) DSR
  // copy, staged at `stage_path`, a renamed sibling, or a copied anonymous
  // file. False only when `path` cannot be opened. With `dsr_only` the run
  // writes DSR to `path` and never converts (N2: same search and kernel view).
  bool open_automatic(const char *path, bool binary_dsr,
                      const char *stage_path = nullptr,
                      bool dsr_only = false);
  // A run with no proof: the writer the search answers to, with nothing to write.
  bool open_nowhere();
  bool nowhere() const noexcept { return nowhere_; }
  bool automatic() const noexcept { return automatic_ && !dsr_only_; }
  // A startup parity refutation that ends the run: `write_pb` writes it to the
  // PBP stream; DSR-only spells it in clauses. False when already published.
  bool refute_parity(const std::vector<PbpParityEquation> &equations,
                     const std::function<bool(FILE *)> &write_pb);
  // A startup counting refutation that ends the run: `write_pb` writes it to
  // the PBP stream. DSR-only takes `input` (original DIMACS literals) and is
  // pending until a follow-on solve (frontend/dsr_follow_on.hpp). False when
  // already published.
  bool refute_by_counting(const std::function<bool(FILE *)> &write_pb,
                          std::vector<int32_t> &input);
  bool dsr_follow_on_pending() const noexcept { return follow_on_pending_; }
  const std::vector<int32_t> &follow_on_input() const noexcept {
    return follow_on_input_;
  }
  // Open `f`, a fresh proof, as DSR-only on this proof's untouched destination.
  bool open_dsr_follow_on(Proof &f);
  // After `f`'s run reported UNSAT: true when its DSR is complete, which then
  // names this proof's route.
  bool settle_dsr_follow_on(const Proof &f);
  AutomaticProofReason automatic_reason() const noexcept {
    return automatic_reason_;
  }
  // The selection is final once the copy is discarded or kept.
  bool automatic_dsr_live() const noexcept { return dsr_copy_ != nullptr; }
  // Whether the run paid the DSR-to-PBP conversion (see LazyPbpReport).
  void release_lazy_event() noexcept;
  const LazyPbpReport &lazy_pbp_report() const noexcept { return lazy_report_; }
  uint64_t stream_identity() const noexcept { return stream_identity_; }
  // Whole-run PBP stages input normalization privately; the first clausal
  // event after this boundary publishes the streaming-PBP header.
  void finish_input_transaction();
  // Cold session admission: only a freshly opened search-facing writer can
  // receive another exact query's input. No event counter is added to search.
  bool accepts_query_input() const noexcept {
    return stream_identity_ && input_transaction_ &&
           !pbp_original_constraints_ && !pbp_derived_constraints_ &&
           !population_observer_ && pending_units_.empty();
  }
  bool startup_pb_available() const {
    return !input_transaction_ && streaming_pbp() && pbp_output_;
  }
  bool commit_clause_stream();
  FILE *begin_pb();
  bool finish_pb();
  // A complete startup PB refutation owns the stream, which takes no further clause
  // events. No search follows it.
  bool startup_pb_written() const { return format_ == ProofFormat::Pbp; }
  // Delete by specification for the rest of the run (a proof attached after
  // replayed input cannot name clauses by id).
  void disable_pbp_id_deletions() { abandon_pbp_id_deletions(7, 0); }
  // Cold query-entry or witnessed-boundary isolation after input closes.
  // Sources must be licensed in this stream. Native identities/IDs stay fixed.
  static bool can_isolate_query(uint32_t extent);
  void preserve_query_input_refutation();
  bool isolate_query(std::span<const QueryProofClause>,
                     std::span<const int32_t> assumptions, uint32_t extent);
  uint32_t certificate_offset() const { return certificate_offset_; }
  void enable_query_names();
  void enable_query_certificates(const QueryProofSources &, QueryProofLimits,
      std::span<const QueryProofClause> initial = {},
      std::span<const int32_t> assumptions = {}, bool isolate_before_witness = false);
  QueryProofSnapshot query_certificate_state() const;
  uint32_t certificate_logical_extent() const { return external_variable_bound(); }
  // Published proof feature, used only to choose an independent final checker.
  // The runner also scans the finished certificate and rejects disagreement.
  const char *verification_route() const {
    if (follow_on_route_)
      return follow_on_route_;
    if (automatic_selected_dsr_)
      return automatic_dsr_witness_ ? "dsr-trestle" : "dsr-cake_lpr";
    if (dsr_only_ && dsr_copy_)
      return dsr_copy_->has_sr_witness_ ? "dsr-trestle" : "dsr-cake_lpr";
    if (pb()) return "pbp";
    return has_sr_witness_ ? "dsr-trestle" : "dsr-cake_lpr";
  }
  void bind_variable_map(const std::vector<Var> *map) {
    variable_map_ = map;
    if (dsr_copy_)
      dsr_copy_->bind_variable_map(map);
  }
  void bind_external_variable_map(const std::vector<Var> *map) {
    external_variable_map_ = map;
    if (dsr_copy_)
      dsr_copy_->bind_external_variable_map(map);
  }
  // Staged input may contain original names absent from the live kernel.
  void reserve_external_variables(uint32_t extent) {
    proof_external_max_ = std::max(proof_external_max_, extent);
    if (dsr_copy_) dsr_copy_->reserve_external_variables(extent);
  }
  void bind_population_observer(PopulationObserver *) noexcept;
  // Settle only a route whose bytes have a committed destination.
  bool settle_population_observer() noexcept;
  PopulationObserver *population_observer() const noexcept {
    return population_observer_;
  }
  // Independent of checker population/format; the owner outlives this writer.
  void bind_derivation_observer(DerivationObserver *observer) noexcept {
    derivation_observer_ = observer;
  }
  // --- proof-delta vocabulary ---
  // add-RUP. Native DSR stages consecutive RUP units so a following deletion
  // block can be ordered before the batch.
  void add_rup(uint64_t id, const Lit *lits, uint32_t size);
  // add-RAT: `lits` MUST be pivot-first (plan §5.2).
  void add_rat(uint64_t id, const Lit *lits, uint32_t size);
  // Add a substitution-redundant clause: `lits[0]` is the DSR pivot,
  // `true_lits` map to true, each mapping maps source to target.
  // `external_variables` bounds every proof-visible external variable; the
  // general encoding borrows the next one and deletes its clauses before
  // returning. False for terminal PBP or literal exhaustion, before any line.
  // `propagates` (unit at the root) forces the general encoding via RUP
  // (implementation-notes, native DSR checking).
  bool add_sr(uint64_t id, const Lit *lits, uint32_t size,
              const Lit *true_lits, uint32_t ntrue,
              const DsrLiteralMapping *mappings, uint32_t nmappings,
              uint32_t external_variables, bool propagates = false);
  void add_empty(uint64_t id);
  // root-level propagation-derived unit: explicit in DSR and streaming PBP
  void add_derived_unit(uint64_t id, Lit unit);
  // delete: DSR writes the literals; streaming PBP names the constraint
  void del(uint64_t line_id, uint64_t del_id, const Lit *lits,
           uint32_t size);

  // Proof-only extension clauses in DIMACS literals that need not exist in the
  // kernel.
  bool add_external_rup(uint64_t id, const int32_t *lits, uint32_t size);
  bool add_external_rat(uint64_t id, const int32_t *lits, uint32_t size);
  // Compact live GF(2) consequence for streaming PBP: pure, bounded planning;
  // publication rechecks the stream/cache generation. `reason` is one entailed
  // clause (or the empty root conflict) with checker-visible CNF premises.
  bool plan_pbp_parity_reason(
      const std::vector<PbpParityEquation> &, const int32_t *reason,
      uint32_t reason_size, uint32_t starting_external_extent,
      const PbpParityBoundsCache &, PbpParityReasonPlan &) const;
  // Commit a plan the caller validated just before its mutation boundary.
  bool add_prevalidated_pbp_parity_reason(
      uint64_t kernel_clause_id, const std::vector<PbpParityEquation> &,
      const int32_t *reason, uint32_t reason_size,
      PbpParityBoundsCache &, const PbpParityReasonPlan &);
  bool del_external(uint64_t line_id, uint64_t del_id,
                    const int32_t *lits, uint32_t size);
  // Keep an emitted proof-only clause for later RUP steps; add_sr deletes the
  // registry before a witnessed line and advances the epoch.
  bool retain_external(uint64_t id, const int32_t *lits, uint32_t size);
  uint64_t persistent_external_epoch() const {
    return persistent_external_epoch_;
  }
  uint64_t persistent_external_bytes() const;

  // Exact authenticated checker premises (not duplicate additions): native
  // deletion may proceed while the checker copy remains, under one literal-body
  // lease. Registration is transactional and caller-metered; witnessed SR
  // retires groups whose variables it changes.
  struct DeferredDeletionLimits {
    uint64_t maximum_pending_deletions;
    uint64_t maximum_pending_literal_work;
    constexpr DeferredDeletionLimits(uint64_t deletions = 0, uint64_t work = 0)
        : maximum_pending_deletions(deletions), maximum_pending_literal_work(work) {}
  };
  using PremiseGroup = std::vector<std::vector<int32_t>>;
  // Read-only reservation for the physical helper deletions queued after a
  // new lease transaction. Every shared owner's common minimum is respected.
  bool can_defer_premise_group_deletions(const std::vector<PremiseGroup> &,
      DeferredDeletionLimits, const std::function<bool(uint64_t)> &) const;
  bool retain_premise_groups(const std::vector<PremiseGroup> &,
      uint64_t maximum_groups, uint64_t maximum_literals,
      uint64_t maximum_bytes, const std::function<bool(uint64_t)> &charge,
      std::vector<uint64_t> &handles, DeferredDeletionLimits limits = {});
  bool premise_group_active(uint64_t) const;
  bool premise_group_matches(uint64_t, const PremiseGroup &) const;
  uint64_t premise_epoch() const { return premise_epoch_; }
  uint64_t premise_literals() const { return premise_literals_; }
  uint64_t premise_bytes() const { return premise_bytes_; }
  // Cumulative serializer overhead, not outstanding occupancy.
  uint64_t premise_deletions_deferred() const { return premise_deferred_; }
  uint64_t premise_pending_deletions() const { return premise_pending_deletions_; }
  uint64_t premise_pending_literal_work() const { return premise_pending_literal_work_; }
  uint64_t premise_maintenance_work() const { return premise_maintenance_work_; }
  void release_premise_groups(const std::vector<uint64_t> &, uint64_t line_id);

  void flush(); // signal-safe enough: fflush on the buffered stream
  // Complete a query's automatic output selection before publication; no
  // later emissions on this writer.
  void finish_query_output();

  // Streaming PBP: root units from a propagation ending in a root conflict
  // are staged, dropped if the next event is the empty clause, else written
  // first. Solver state is unaffected.
  void begin_deferred_root_units() noexcept;
  void end_deferred_root_units() noexcept;

  // Streaming-PBP deletion accounting for tests (no proof effect).
  struct PbpDeletionCounters {
    uint64_t by_id = 0;        // every `del id` line
    uint64_t by_anonymous = 0; // ... resolved by the id-0 literal registry
    uint64_t by_rebind = 0;    // ... restated-input originals
    uint64_t spec_unit = 0, spec_binary = 0, spec_wide = 0;
    uint64_t anonymous_registered = 0;
    uint64_t rebinds_recorded = 0;
    uint64_t deferred_batches = 0, deferred_units = 0;
    uint64_t dropped_batches = 0, dropped_units = 0;
    uint32_t abandon_reason = 0; // first reason; 0 = never abandoned
    uint32_t abandon_width = 0;
    uint64_t abandon_line = 0; // derived-constraint count at abandonment
  };
  const PbpDeletionCounters &pbp_deletion_counters() const noexcept {
    return pbp_counters_;
  }

  // A tautological (or duplicate-carrying) lemma is uncheckable, so rules
  // skip the inference instead (D-CONG-SKIP); debug builds assert on it in
  // `add_rup`/`add_rat`.
  static bool tautological(const Lit *lits, uint32_t size);

  // Register one exact checker input premise (observers and proof shadow).
  void seed_population(uint64_t id, const Lit *lits, uint32_t size);

#ifdef TACTIX_ENABLE_PROOF_SHADOW
  // Diagnostic N2 oracle: a deletion must be backed by one checker-visible
  // copy, named by its literals. Armed after the complete input is registered.
  void arm_population_shadow() {
    armed_ = true;
    if (dsr_copy_)
      dsr_copy_->arm_population_shadow();
  }
  bool population_shadow_available() const { return true; }
  bool population_contains(uint64_t id, const Lit *lits, uint32_t size) const;
  uint64_t unbacked_deletions() const { return unbacked_deletions_; }
  bool population_shadow() const { return armed_; }
#else
  void arm_population_shadow() {}
  bool population_shadow_available() const { return false; }
  bool population_contains(uint64_t, const Lit *, uint32_t) const {
    return true;
  }
  uint64_t unbacked_deletions() const { return 0; }
  bool population_shadow() const { return false; }
#endif

private:
  struct QueryCertificates;
  std::unique_ptr<QueryCertificates> query_certificates_;
  bool query_names_enabled_ = false;
  bool certificate_raw_ = false, checker_event_named_ = false;
  uint32_t raw_helper_next_ = 0, raw_helper_limit_ = 0;
  uint32_t certificate_offset_ = 0;
  bool query_input_refuted_ = false;
  int32_t certificate_literal(int32_t literal) const;
  std::vector<int32_t> certificate_literals(const int32_t *, uint32_t) const;
  void query_clause(bool add, PopulationClauseView);
  void prepare_query_witness(uint32_t rat_pivot = 0);
  bool isolate_query_impl(std::span<const QueryProofClause>,
      std::span<const int32_t>, uint32_t extent);
  void observe_named_helper(bool deletion, std::span<const int32_t> clause);
  DerivationObserver *derivation_observer_ = nullptr;
  bool has_derivation_consumers() const noexcept {
    return derivation_observer_ || query_certificates_;
  }
  void observe_derivation(DerivationKind, uint64_t, uint64_t,
                          PopulationClauseView) noexcept;
  // Which stream this object writes is the writer's own business: a rule that
  // could ask could steer the search by it (N2).
  bool pb() const {
    return format_ == ProofFormat::Pbp ||
           format_ == ProofFormat::PbpStreaming;
  }
  bool streaming_pbp() const { return format_ == ProofFormat::PbpStreaming; }
  bool dsr() const {
    return format_ == ProofFormat::DsrBinary ||
           format_ == ProofFormat::DsrAscii;
  }
  bool binary() const { return format_ == ProofFormat::DsrBinary; }
  // Event bodies. The public methods run these and then feed the automatic
  // DSR copy; internal calls use the bodies so the copy sees each event once.
  void add_rup_impl(uint64_t id, const Lit *lits, uint32_t size);
  void add_rat_impl(uint64_t id, const Lit *lits, uint32_t size);
  bool add_sr_impl(uint64_t id, const Lit *lits, uint32_t size,
                   const Lit *true_lits, uint32_t ntrue,
                   const DsrLiteralMapping *mappings, uint32_t nmappings,
                   uint32_t external_variables, bool propagates);
  void add_empty_impl(uint64_t id);
  void add_derived_unit_impl(uint64_t id, Lit unit);
  void del_impl(uint64_t line_id, uint64_t del_id, const Lit *lits,
                uint32_t size);
  bool add_external_rup_impl(uint64_t id, const int32_t *lits, uint32_t size);
  bool add_external_rat_impl(uint64_t id, const int32_t *lits, uint32_t size);
  bool del_external_impl(uint64_t line_id, uint64_t del_id,
                         const int32_t *lits, uint32_t size);
  bool retain_external_impl(uint64_t id, const int32_t *lits, uint32_t size);
  bool retain_premise_groups_impl(const std::vector<PremiseGroup> &,
      uint64_t maximum_groups, uint64_t maximum_literals,
      uint64_t maximum_bytes, const std::function<bool(uint64_t)> &charge,
      std::vector<uint64_t> &handles, DeferredDeletionLimits limits);
  void release_premise_groups_impl(const std::vector<uint64_t> &,
                                   uint64_t line_id);
  void seed_population_impl(uint64_t id, const Lit *lits, uint32_t size);
  void finish_input_transaction_impl();
  void flush_impl();
  FILE *begin_pb_impl();
  // A complete startup PB refutation written to the PBP stream (see
  // refute_by_counting).
  bool write_startup_pb(const std::function<bool(FILE *)> &write_pb);
  bool open_stream(FILE *file, ProofFormat format);
  // Explicit DSR on `destination` (owned); see open_automatic.
  bool open_dsr_only(FILE *destination, bool binary);
  void publish_dsr_only_prefix();

  // The DSR follow-on of a startup counting refutation (refute_by_counting).
  bool follow_on_pending_ = false;
  FILE *follow_on_destination_ = nullptr; // owned until open_dsr_follow_on
  bool follow_on_binary_ = true;
  std::vector<int32_t> follow_on_input_;
  const char *follow_on_route_ = nullptr; // the written follow-on's route

  // Automatic selection state (see AutomaticProofReason). The copy is a
  // native-DSR Proof fed every event the PBP stream accepted.
  std::unique_ptr<Proof> dsr_copy_;
  bool automatic_ = false;
  bool automatic_selected_dsr_ = false;
  bool automatic_dsr_witness_ = false;
  AutomaticProofReason automatic_reason_ = AutomaticProofReason::Pending;
  std::string automatic_path_;       // the destination, for the final rename
  std::string automatic_stage_name_; // a named sibling stage, else empty
  // The DSR copy writes to the destination itself, so a killed run leaves it.
  // A DSR-only copy writes there once its prefix is published.
  bool dsr_in_destination_ = false;
  bool dsr_only_ = false; // explicit --dsr: DSR is the proof, whatever fires
  // DSR copy: proof-only parity helper variables; at solver extent `extent`,
  // `helpers` helpers exist in total.
  std::vector<DsrHelperNames> helper_names_;
  uint64_t pbp_numbering_ = 0; // bumped when written rows stop being usable
  std::vector<uint64_t> pbp_parity_constraints_;
  std::unordered_set<uint32_t> pbp_parity_variables_;
  bool nowhere_ = false;
  bool pbp_text_wanted() const noexcept;
  __attribute__((format(printf, 2, 3))) void put_pbp_text(const char *format, ...);
  // DSR copy: bytes spent on parity reasons; --auto-proof converts once they
  // reach parity_bytes_limit_ (Tier D: rare-parity runs spend about 1 MB).
  uint64_t parity_bytes_ = 0;
  uint64_t parity_bytes_limit_ = 10000000;
  uint32_t new_helper();
  // A solver variable's DSR name, above every earlier helper; checker_variable
  // stops the run past the checkers' name range.
  uint64_t checker_name(uint64_t solver_variable) const;
  uint32_t checker_variable(uint32_t solver_variable) const;
  struct ParityWriter;
  friend struct ParityWriter;
  void write_parity_reason(const std::vector<PbpParityEquation> &equations,
                           const int32_t *reason, uint32_t reason_size);
  void put_checker_literal(int32_t name);
  // The destination the automatic selection rewrites (non-owning alias of
  // pbp_output_, file_ or lazy_destination_).
  FILE *automatic_destination_ = nullptr;

  // One proof at a time (plan §5): until a trigger, file_ only counts PBP
  // text and the DSR copy is the proof; a trigger converts the copy to PBP on
  // the destination. Until then every kernel answer is that of --pbp.
  struct LazyPbp;
  std::unique_ptr<LazyPbp> lazy_;     // null when the stream is real
  bool lazy_published_ = false;       // the header's f is recorded, unwritten
  uint64_t lazy_f_ = 0;
  FILE *lazy_destination_ = nullptr;  // owned destination after lazy publish
  LazyPbpReport lazy_report_;
  // The copy is the only proof (held across a failed conversion, when lazy_ is
  // gone): losing its bytes must abort before any UNSAT claim.
  bool dsr_is_the_proof_ = false;
  // In the copy: which additions are witness-free RAT lines (dsr_to_pbp.hpp),
  // kept by a copy a conversion may read.
  std::vector<uint64_t> rat_additions_;
  bool keeps_rat_additions_ = false;
  uint64_t additions_ = 0;
  bool next_addition_rat_ = false;
  // Validation: switch after this many DSR additions (never by default); the
  // maximum means at the empty clause.
  uint64_t forced_switch_at_ = 0;
  // Past this much DSR a conversion is declined where possible (it rereads
  // every byte); TACTIX_AUTO_PROOF_DSR_LIMIT lowers it.
  uint64_t dsr_limit_ = 3000000000;
  // In the copy: lines written, counted as binary DSR in either spelling, so
  // text and binary runs reach the limit at the same event.
  uint64_t dsr_bytes_written_ = 0;
  bool conversion_barred() const noexcept;
  void start_lazy_pbp();
  void hold_lazy_event();
  bool convert_lazy_pbp();
  void discard_lazy_pbp() noexcept;
  // In the copy: a failed write disables it instead of aborting the solver.
  bool copy_role_ = false;
  bool copy_failed_ = false;
  void drop_dsr_copy(AutomaticProofReason) noexcept;
  void discard_dsr_copy() noexcept;
  void select_dsr_copy(bool finished = true);
  // After forwarding: discard a failed copy, keep the copy at the empty clause.
  void settle_dsr_copy();
  // Feeds one event to the DSR copy in `copy_slot` (always dsr_copy_), then
  // settles it.
  template <class Event>
  void forward_to_copy(std::unique_ptr<Proof> &copy_slot, Event &&event) {
    if (copy_slot) {
      event(*copy_slot);
      settle_dsr_copy();
    }
  }
  // Writes an event the copy in `copy_slot` (always dsr_copy_) may decline:
  // holds it, then forwards it (when `forward`) and drops a declining copy.
  template <class Write, class Copy>
  bool write_held_event(std::unique_ptr<Proof> &copy_slot, Write &&write, Copy &&copy,
                        bool forward = true);

  // Consecutive RUP units and their following deletion block form one split
  // transaction: two RUP helpers per unit share a fresh variable; deletion
  // bytes are spooled; helpers retire before a later SR step.
  void flush_pending_dsr_units();
  void flush_persistent_external(uint64_t line_id);
  CheckerRoute population_route() const noexcept;
  CheckerNaming population_naming() const noexcept;
  void observe_checker(CheckerPopulationEventKind, CheckerClauseRole,
                       uint64_t line_id, uint64_t checker_id,
                       PopulationClauseView, bool proof_only = false,
                       uint64_t transaction_id = 0,
                       PopulationSourceClassification =
                           PopulationSourceClassification::Ineligible) noexcept;
  uint32_t external_variable_bound() const;
  struct PendingDsrUnit {
    int32_t external;
    uint64_t id;
    CheckerClauseRole role;
  };
  std::vector<PendingDsrUnit> pending_units_;
  std::unordered_set<int32_t> pending_unit_literals_;
  std::unordered_set<uint64_t> pending_unit_ids_;
  uint32_t pending_max_external_ = 0;
  uint32_t proof_external_max_ = 0;
  struct PersistentExternalClause {
    uint64_t id = 0;
    std::vector<int32_t> lits;
  };
  std::vector<PersistentExternalClause> persistent_external_;
  uint64_t persistent_external_epoch_ = 0;
  struct LeasedPremise {
    uint64_t users = 0, deferred = 0;
  };
  // Retained bytes charged per leased premise clause; release refunds them.
  static constexpr uint64_t premise_storage_bytes(size_t size) {
    return (4 * sizeof(PremiseGroup) + 16 * sizeof(void *) +
            sizeof(LeasedPremise)) + 2 * uint64_t(size) * sizeof(int32_t);
  }
  std::map<std::vector<int32_t>, LeasedPremise> leased_premises_;
  std::map<uint64_t, PremiseGroup> premise_groups_;
  uint64_t next_premise_group_ = 0, premise_epoch_ = 0;
  uint64_t stream_identity_ = 0;
  uint64_t premise_literals_ = 0, premise_bytes_ = 0, premise_deferred_ = 0;
  uint64_t premise_pending_deletions_ = 0, premise_pending_literal_work_ = 0;
  uint64_t premise_pending_deletion_cap_ = UINT64_MAX, premise_pending_work_cap_ = UINT64_MAX;
  uint32_t premise_maximum_width_ = 0;
  uint64_t premise_maintenance_work_ = 0;
  // Occurrences of each external variable in premise_groups_, so a line whose
  // pivot and witness touch no lease need not scan them.
  std::vector<uint32_t> premise_variable_uses_;
  void count_premise_variables(const PremiseGroup &, bool add);
  bool defer_leased_deletion(const int32_t *, uint32_t);
  // Delete parity-reason constraints that mention a variable the witness
  // changes; cached rows are rewritten before their next use.
  void retire_pbp_parity_constraints(const Lit *, uint32_t, const Lit *, uint32_t,
                                     const DsrLiteralMapping *, uint32_t);
  // The DSR writer cannot spell this line: its auxiliary variable would need a
  // name past the ones the checkers read.
  bool sr_needs_missing_auxiliary(const Lit *, uint32_t, const Lit *, uint32_t,
                                  const DsrLiteralMapping *, uint32_t, uint32_t,
                                  bool) const;
  void invalidate_premises_for_sr(uint64_t, const Lit *, uint32_t,
      const Lit *, uint32_t, const DsrLiteralMapping *, uint32_t);
  // Binary deletion blocks are usually only a few bytes. Keep them in a
  // bounded buffer and spill exceptional blocks to a temporary file.
  std::vector<unsigned char> pending_deletion_bytes_;
  FILE *pending_deletions_ = nullptr;
  bool spooling_deletions_ = false;
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  std::string population_key(uint64_t id, const Lit *lits, uint32_t size) const;
  void population_add(uint64_t id, const Lit *lits, uint32_t size);
  void population_del(uint64_t id, const Lit *lits, uint32_t size);
  // The same oracle for a proof-only clause given by external DIMACS literals.
  void population_external(bool add, uint64_t id, const int32_t *lits,
                           uint32_t size);
  std::unordered_map<std::string, uint32_t> population_;
  uint64_t unbacked_deletions_ = 0;
  bool armed_ = false;
#else
  void population_add(uint64_t, const Lit *, uint32_t) {}
  void population_del(uint64_t, const Lit *, uint32_t) {}
  void population_external(bool, uint64_t, const int32_t *, uint32_t) {}
#endif
  // The checker-visible literal for an internal literal under the bound map.
  Lit checker_lit(Lit l) const {
    if (certificate_raw_ || !variable_map_)
      return l;
    assert(var_of(l) < variable_map_->size());
    return make_lit((*variable_map_)[var_of(l)], negated(l));
  }
  // The population view of internal literals under the same map.
  PopulationClauseView internal_view(const Lit *lits,
                                     uint32_t size) const noexcept {
    return PopulationClauseView::internal_lits(lits, size, certificate_raw_ ? nullptr : variable_map_);
  }
  // A staged DSR-unit batch spools the next deletion line; see del().
  FILE *begin_pending_deletion();
  void end_pending_deletion(FILE *main_file);
  template <class External>
  bool put_pbp_deletion(uint64_t del_id, uint32_t size, External external);
  // `external()` yields the constraint's checker literals; it runs only for
  // an anonymous (id-0) constraint that may later need its exact identity.
  template <class External>
  void note_pbp_derived(uint64_t id, uint32_t clause_size, External external);
  void register_pbp_anonymous(const int32_t *lits, uint32_t size);
  bool put_pbp_anonymous_deletion(const int32_t *canonical, uint32_t size);
  bool put_pbp_rebind_deletion();
  bool pbp_current_ordinal(uint64_t &constraint) const;
  bool pbp_absolute_constraint(uint64_t stored, uint64_t &constraint) const;
  void settle_pbp_deferred_units(bool drop);
  void put_byte(unsigned char byte);
  void flush_binary();
  void put_lit(Lit l);
  void put_external_lit(Lit l);
  void put_external_dimacs(int32_t literal);
  // A DSR line: binary formats tag every line, text ones a deletion.
  void put_line_tag(bool deletion);
  void put_clausal_line(bool deletion, const Lit *lits, uint32_t size);
  void put_pbp_clause(const char *rule, const Lit *lits, uint32_t size);
  const int32_t *pbp_checker_literals(const Lit *lits, uint32_t size);
  void put_pbp_external_clause(const char *rule, const int32_t *lits,
                               uint32_t size);
  void track_pbp_original_constraint(uint64_t id, uint32_t clause_size,
                                     bool repeated_literal);
  void track_pbp_derived_constraint(uint64_t id, uint32_t clause_size);
  bool put_pbp_deletion_id(uint64_t id);
  void abandon_pbp_id_deletions(uint32_t reason = 0, uint32_t width = 0);
  void finish_streaming_pbp_unsat();
  void publish_streaming_pbp_header();
  void end_line();
  FILE *file_ = nullptr;
  FILE *pbp_output_ = nullptr;
  bool input_transaction_ = false;
  bool has_sr_witness_ = false;
  bool streaming_pbp_finished_ = false;
  uint64_t pbp_original_constraints_ = 0;
  uint64_t pbp_derived_constraints_ = 0;
  // Constraint IDs match Tactix IDs through the raw-input prefix; after a
  // split, input runs of consecutive ids cost one run per split plus two bits
  // per input. `pbp_constraint_ids_` names derived constraints; the high bit
  // marks an ordinal resolved when input parsing finishes.
  struct PbpInputRun {
    uint64_t first_id, first_constraint, count;
  };
  std::vector<PbpInputRun> pbp_input_runs_;
  std::vector<bool> pbp_input_by_spec_; // by input constraint
  std::vector<bool> pbp_input_deleted_; // by input constraint
  std::unordered_map<uint64_t, uint64_t> pbp_constraint_ids_;
  uint64_t pbp_identity_prefix_ = 0;
  bool pbp_input_ids_diverged_ = false;
  bool pbp_input_constraint(uint64_t id, uint64_t &constraint,
                            bool &exact) const;
  bool pbp_input_deleted(uint64_t constraint) const;
  void mark_pbp_input_deleted(uint64_t constraint);
  bool pbp_id_deletions_enabled_ = true;
  // Anonymous (id-0) derived constraints keyed by sorted checker literals,
  // each a stack of IDs (high bit: ordinal). An ID lives in exactly one of
  // this registry and `pbp_constraint_ids_`.
  struct PbpLiteralKeyHash {
    size_t operator()(const std::vector<int32_t> &key) const noexcept;
  };
  std::unordered_map<std::vector<int32_t>, std::vector<uint64_t>,
                     PbpLiteralKeyHash>
      pbp_anonymous_constraints_;
  std::vector<int32_t> pbp_key_scratch_;
  // Input restatement keeps the input's Tactix id. The most recent raw input
  // and, at most, one replacement awaiting deletion of that raw original.
  uint64_t pbp_last_input_id_ = 0;
  uint64_t pbp_last_input_constraint_ = 0;
  bool pbp_last_input_live_ = false;
  struct PbpPendingRebind {
    uint64_t id = 0, original = 0, replacement = 0;
  };
  PbpPendingRebind pbp_pending_rebind_;
  // Lazily published root units of a conflicting root propagation.
  struct PbpDeferredUnit {
    uint64_t id;
    Lit lit;
  };
  std::vector<PbpDeferredUnit> pbp_deferred_units_;
  bool pbp_deferring_units_ = false;
  bool pbp_replaying_leased_ = false;
  uint64_t pbp_last_deleted_ = 0; // constraint named by the last `del id`
  // Assertion-build audit of exact deletion (members exist in every build for
  // a fixed layout): canonical literals of every constraint by proof ID.
  std::unordered_map<uint64_t, std::vector<int32_t>> pbp_audit_input_;
  std::unordered_map<uint64_t, std::vector<int32_t>> pbp_audit_derived_;
  void pbp_audit_record(bool input, uint64_t index, const int32_t *lits,
                        uint32_t size);
  void pbp_audit_deletion(uint64_t constraint, const int32_t *lits,
                          uint32_t size);
  PbpDeletionCounters pbp_counters_;
  std::vector<int32_t> pbp_external_scratch_;
  std::vector<int32_t> pbp_canonical_scratch_;
  std::vector<uint32_t> pbp_canonical_order_;
  std::vector<uint8_t> pbp_canonical_keep_;
  ProofFormat format_ = ProofFormat::DsrBinary;
  std::array<unsigned char, 1 << 20> binary_buffer_{};
  size_t binary_buffer_size_ = 0;
  const std::vector<Var> *variable_map_ = nullptr;
  const std::vector<Var> *external_variable_map_ = nullptr;
  PopulationObserver *population_observer_ = nullptr; // non-owning
  CheckerClauseRole population_role_ = CheckerClauseRole::Rup;
  uint64_t pending_population_transaction_ = 0;
  uint64_t next_population_transaction_ = 0;
};

} // namespace tactix
