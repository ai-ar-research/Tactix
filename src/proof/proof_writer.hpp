// The proof writer (private to src/proof). It exists only when a proof is
// written and spells the events the ledger (Proof, proof.hpp) has accepted.
// Every answer the search reads comes from the ledger, and the event bodies
// return void, so a writer cannot steer the search (N2).
#pragma once

#include "proof/proof.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tactix {

class ProofWriter {
public:
  // Streaming PBP to `destination` (owned) behind a private input-transaction
  // file; null, with `destination` closed, when either cannot be used.
  static std::unique_ptr<ProofWriter> pbp(FILE *destination);
  // Native DSR to `file` (owned); null when `file` is null. In the copy role a
  // failed write disables the writer instead of aborting the solver.
  static std::unique_ptr<ProofWriter> dsr(FILE *file, ProofFormat format,
                                          bool copy_role);
  // Closes every handle; a failed close of a proof a verdict may rest on aborts.
  ~ProofWriter();

  // --- Opening and teardown ---
  void opened(); // the ledger drew its stream identity: RouteOpened, RouteCommitted
  // Automatic selection on `path` (see Proof::open_automatic).
  void start_automatic(const char *path, bool binary, const char *stage);
  // Explicit DSR on this writer's destination: the copy is the proof, kept
  // private until the stream is published (publish_dsr_only_prefix).
  void start_dsr_only(bool binary);
  // An unfinished run installs the copy's prefix; a held destination closes.
  void teardown_copy();

  // --- Event bodies: the ledger has decided, these only spell ---
  void publish(); // the stream's header and private prefix reach the destination
  void rup(uint64_t id, const Lit *lits, uint32_t size, CheckerClauseRole role);
  void rat(uint64_t id, const Lit *lits, uint32_t size);
  // Delete the parity constraints a witness changes (see pbp_parity_constraints_).
  void retire_parity(const Lit *lits, uint32_t size, const Lit *true_lits,
                     uint32_t ntrue, const DsrLiteralMapping *mappings,
                     uint32_t nmappings);
  void sr(uint64_t id, const Lit *lits, uint32_t size, const Lit *true_lits,
          uint32_t ntrue, const DsrLiteralMapping *mappings, uint32_t nmappings,
          uint32_t external_variables, bool propagates,
          const DsrWitnessPlan &plan);
  void del(uint64_t line_id, uint64_t del_id, const Lit *lits, uint32_t size);
  // Role ExternalRat: the DSR spelling of an external RAT line.
  void external_rup(uint64_t id, const int32_t *lits, uint32_t size,
                    CheckerClauseRole role);
  void external_rat(uint64_t id, const int32_t *lits, uint32_t size); // streaming PBP
  // `replaying`: a lease release replays a deferred deletion, which no longer
  // knows which identity it named.
  void del_external(uint64_t line_id, uint64_t del_id, const int32_t *lits,
                    uint32_t size, bool replaying);
  void seed(uint64_t id, const Lit *lits, uint32_t size);
  void finish_input();
  void flush();
  // The copy's clausal spelling of a parity reason, or the conversion it
  // triggers; runs before the ledger touches the cache.
  void parity_copy(const std::vector<PbpParityEquation> &equations,
                   const int32_t *reason, uint32_t reason_size);
  // Rows never written (lower == 0) or written under another numbering are
  // written, once each, in order; then the sums and the reason.
  void parity_reason(uint64_t kernel_clause_id,
                     const std::vector<PbpParityEquation> &equations,
                     const int32_t *reason, uint32_t reason_size,
                     PbpParityBoundsCache &cache,
                     const PbpParityReasonPlan &plan);

  // --- Startup PB refutations, after the ledger made the stream terminal ---
  void check_startup_pb(); // a DSR-only proof has no PB stream: proof_error
  FILE *startup_pb_begin(); // the terminal PB stream; the copy converts away
  void startup_pb(const std::function<bool(FILE *)> &write_pb); // aborts unless complete
  bool finish_pb();
  // DSR-only: spelled in clauses (refuted_in_clauses_); otherwise startup_pb.
  void refute_parity(const std::vector<PbpParityEquation> &equations,
                     const std::function<bool(FILE *)> &write_pb);
  // DSR-only: takes the untouched destination and `input` for the follow-on;
  // otherwise startup_pb.
  void refute_by_counting(const std::function<bool(FILE *)> &write_pb,
                          std::vector<int32_t> &input);
  void disable_id_deletions() { abandon_pbp_id_deletions(7, 0); }

  // --- Staged root units (Proof::begin_deferred_root_units): counters only ---
  void staged_unit() noexcept { ++pbp_counters_.deferred_units; }
  void staged_batch() noexcept { ++pbp_counters_.deferred_batches; }
  void staged_settled(uint64_t units, bool dropped) noexcept {
    if (dropped) {
      ++pbp_counters_.dropped_batches;
      pbp_counters_.dropped_units += units;
    }
  }

  // --- Bindings ---
  void bind_variable_map(const std::vector<Var> *map) {
    variable_map_ = map;
    if (copy_)
      copy_->bind_variable_map(map);
  }
  void bind_external_variable_map(const std::vector<Var> *map) {
    external_variable_map_ = map;
    if (copy_)
      copy_->bind_external_variable_map(map);
  }
  void set_population_observer(PopulationObserver *observer) noexcept {
    population_observer_ = observer;
  }
  CheckerRoute population_route() const noexcept;
  CheckerNaming population_naming() const noexcept;
  // The DSR writer cannot spell this line: its auxiliary variable would need a
  // name past the ones the checkers read. Read only by a DSR writer's own
  // ledger and by before_sr.
  bool sr_needs_missing_auxiliary(const Lit *lits, uint32_t size,
                                  const Lit *true_lits, uint32_t ntrue,
                                  const DsrLiteralMapping *mappings,
                                  uint32_t nmappings,
                                  uint32_t external_variables,
                                  bool propagates) const;

  // --- The automatic DSR copy: a complete Proof fed each public event ---
  // Each front: hold, ledger body (with this writer's bodies), forward, release.
  void hold() noexcept {
    if (copy_)
      hold_lazy_event();
  }
  // A line the copy cannot spell converts first; --dsr stops there.
  void before_sr(const Lit *lits, uint32_t size, const Lit *true_lits,
                 uint32_t ntrue, const DsrLiteralMapping *mappings,
                 uint32_t nmappings, uint32_t external_variables,
                 bool propagates);
  // True: the copy skips this event (a tautology, which converts first
  // unless the run is DSR-only).
  bool before_external_rat(const int32_t *lits, uint32_t size);
  template <class Event> void forward(Event &&event) {
    if (!copy_)
      return;
    event(*copy_);
    settle_copy();
  }
  // A copy that declines an accepted event converts; then the hold ends.
  template <class Event> void forward_held(bool accepted, Event &&event) {
    if (accepted && copy_) {
      if (!event(*copy_))
        drop_copy(AutomaticProofReason::CopyDeclined);
      else
        settle_copy();
    }
    release_lazy_event();
  }
  // The copy must accept: the pre-check left it every line it can spell.
  template <class Event> void forward_held_strict(bool accepted, Event &&event) {
    if (accepted && copy_) {
      if (!event(*copy_))
        std::abort();
      settle_copy();
    }
    release_lazy_event();
  }
  // After forwarding: discard a failed copy, keep the copy at the empty clause.
  void settle_copy();

  // --- The DSR follow-on of a startup counting refutation ---
  bool follow_on_pending() const noexcept { return follow_on_pending_; }
  const std::vector<int32_t> &follow_on_input() const noexcept {
    return follow_on_input_;
  }
  bool open_follow_on(Proof &follow_on);
  bool settle_follow_on(const Proof &follow_on);

  // --- Output-only reporting ---
  const char *verification_route() const {
    if (follow_on_route_)
      return follow_on_route_;
    if (automatic_selected_dsr_)
      return automatic_dsr_witness_ ? "dsr-trestle" : "dsr-cake_lpr";
    if (dsr_only_ && copy_)
      return copy_->writer_->has_sr_witness_ ? "dsr-trestle" : "dsr-cake_lpr";
    if (pb())
      return "pbp";
    return has_sr_witness_ ? "dsr-trestle" : "dsr-cake_lpr";
  }
  bool automatic() const noexcept { return automatic_ && !dsr_only_; }
  bool automatic_dsr_live() const noexcept { return copy_ != nullptr; }
  AutomaticProofReason automatic_reason() const noexcept {
    return automatic_reason_;
  }
  const LazyPbpReport &lazy_report() const noexcept { return lazy_report_; }
  void release_lazy_event() noexcept;
  const Proof::PbpDeletionCounters &counters() const noexcept {
    return pbp_counters_;
  }
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  void arm_population_shadow() {
    armed_ = true;
    if (copy_)
      copy_->arm_population_shadow();
  }
  bool population_contains(uint64_t id, const Lit *lits, uint32_t size) const;
  uint64_t unbacked_deletions() const { return unbacked_deletions_; }
  bool population_shadow() const { return armed_; }
#endif
  // Debug builds, after every public front: this writer's lifecycle is the
  // ledger's (proof.hpp).
  void check_lifecycle(bool input, bool published, bool terminal,
                       bool finished) const;

private:
  ProofWriter();

  bool pb() const noexcept {
    return format_ == ProofFormat::Pbp || format_ == ProofFormat::PbpStreaming;
  }
  bool streaming_pbp() const noexcept {
    return format_ == ProofFormat::PbpStreaming;
  }
  bool dsr() const noexcept {
    return format_ == ProofFormat::DsrBinary || format_ == ProofFormat::DsrAscii;
  }
  bool binary() const noexcept { return format_ == ProofFormat::DsrBinary; }

  // --- Route and handles ---
  ProofFormat format_ = ProofFormat::DsrBinary;
  FILE *file_ = nullptr;
  // The destination while the stream is private (input transaction).
  FILE *pbp_output_ = nullptr;
  bool input_transaction_ = false; // input constraints are still being numbered
  bool streaming_pbp_finished_ = false;
  bool has_sr_witness_ = false;
  bool copy_role_ = false;
  bool copy_failed_ = false;
  // A DSR-only startup parity refutation, spelled in clauses on the published
  // stream while the ledger is terminal.
  bool refuted_in_clauses_ = false;
  // The terminal PB stream replaces the private clausal prefix.
  FILE *switch_to_pb();
  void finish_streaming_pbp_unsat();
  void observe_checker(CheckerPopulationEventKind, CheckerClauseRole,
                       uint64_t line_id, uint64_t checker_id,
                       PopulationClauseView, bool proof_only = false,
                       uint64_t transaction_id = 0,
                       PopulationSourceClassification =
                           PopulationSourceClassification::Ineligible) noexcept;
  const std::vector<Var> *variable_map_ = nullptr;
  const std::vector<Var> *external_variable_map_ = nullptr;
  PopulationObserver *population_observer_ = nullptr; // non-owning
  uint64_t pending_population_transaction_ = 0;
  uint64_t next_population_transaction_ = 0;

  // --- The DSR follow-on of a startup counting refutation ---
  bool follow_on_pending_ = false;
  FILE *follow_on_destination_ = nullptr; // owned until open_follow_on
  bool follow_on_binary_ = true;
  std::vector<int32_t> follow_on_input_;
  const char *follow_on_route_ = nullptr; // the written follow-on's route

  // --- Automatic selection (see AutomaticProofReason) ---
  // The copy is a native-DSR Proof fed every event the PBP stream accepted.
  std::unique_ptr<Proof> copy_;
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
  // The proof file the automatic selection finally rewrites (non-owning: an
  // alias of pbp_output_ or, after publication, file_ or lazy_destination_).
  // Every fstat, truncation and copy-in of the destination goes through it.
  FILE *automatic_destination_ = nullptr;
  std::unique_ptr<Proof> open_copy(FILE *stage, bool binary);
  // Never converted, so any destination will do; the prefix moves once.
  void publish_dsr_only_prefix();
  void drop_copy(AutomaticProofReason) noexcept;
  void discard_copy() noexcept;
  void select_copy(bool finished = true);

  // One proof at a time (plan §5). In automatic mode, until a trigger, file_ is
  // an in-process stream that only counts PBP text, and the DSR copy is the
  // proof. A trigger converts the copy to PBP, which takes over the destination,
  // and the writer continues on the real file.
  struct LazyPbp;
  std::unique_ptr<LazyPbp> lazy_;     // null when the stream is real
  bool lazy_published_ = false;       // the header's f is recorded, unwritten
  uint64_t lazy_f_ = 0;
  FILE *lazy_destination_ = nullptr;  // owned destination after lazy publish
  LazyPbpReport lazy_report_;
  // The copy is the only proof (held across a failed conversion, when lazy_ is
  // gone): losing its bytes must abort before any UNSAT claim.
  bool dsr_is_the_proof_ = false;
  // Validation: switch after this many DSR additions (never by default); the
  // maximum means at the empty clause.
  uint64_t forced_switch_at_ = 0;
  // A conversion rereads every DSR byte, so past this much DSR the run stops
  // converting where it has a choice: an event DSR can spell or a PB step it
  // can decline stays on DSR. An event with no clausal spelling still converts.
  // Validation runs lower it with TACTIX_AUTO_PROOF_DSR_LIMIT.
  uint64_t dsr_limit_ = 3000000000;
  bool conversion_barred() const noexcept;
  void start_lazy_pbp();
  void hold_lazy_event();
  bool convert_lazy_pbp();
  void discard_lazy_pbp() noexcept;

  // --- In the copy: DSR naming and what a conversion reads ---
  // Proof-only variables that spell parity reasons. The solver never sees
  // them, so a variable it creates later takes a higher checker name. Each
  // entry: at solver extent `extent`, `helpers` helpers exist in total.
  std::vector<DsrHelperNames> helper_names_;
  // Which additions are witness-free RAT lines (dsr_to_pbp.hpp), kept by a
  // copy a conversion may read.
  std::vector<uint64_t> rat_additions_;
  bool keeps_rat_additions_ = false;
  uint64_t additions_ = 0;
  bool next_addition_rat_ = false;
  // Lines written, counted as binary DSR in either spelling, so text and
  // binary runs reach the limit at the same event.
  uint64_t dsr_bytes_written_ = 0;
  // Bytes spent spelling parity reasons. --auto-proof converts once they reach
  // parity_bytes_limit_: measured on Tier D, a run with rare parity reasons
  // spends at most about 1 MB on them in total, and a parity-heavy run passes
  // this within its first 64 reasons, while the converted prefix is small.
  uint64_t parity_bytes_ = 0;
  uint64_t parity_bytes_limit_ = 10000000;
  uint32_t new_helper();
  // A solver variable's DSR name: every helper named while the solver had fewer
  // variables sits below it. checker_variable stops the run past the names
  // the checkers read.
  uint64_t checker_name(uint64_t solver_variable) const;
  uint32_t checker_variable(uint32_t solver_variable) const;
  struct ParityWriter;
  friend struct ParityWriter;
  void write_parity_reason(const std::vector<PbpParityEquation> &equations,
                           const int32_t *reason, uint32_t reason_size);
  void put_checker_literal(int32_t name);

  // --- Parity constraints of the PBP stream ---
  uint64_t pbp_numbering_ = 0; // bumped when written rows stop being usable
  std::vector<uint64_t> pbp_parity_constraints_;
  std::unordered_set<uint32_t> pbp_parity_variables_;

  // --- Text ---
  // An automatic run spares formatting text it only counts, except the event
  // it holds for the copy's answer.
  bool pbp_text_wanted() const noexcept;
  __attribute__((format(printf, 2, 3))) void put_pbp_text(const char *format, ...);

  // --- Native DSR: staged units and spooled deletions ---
  // Consecutive RUP units and their following deletion block form one split
  // transaction. Two RUP helpers per unit use a shared fresh variable; each
  // prior pair supplies its unit under either assumption on that variable.
  // Deletion bytes are spooled; helpers are retired before a later SR step.
  void flush_pending_dsr_units();
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
  // Binary deletion blocks are usually only a few bytes. Keep them in a
  // bounded buffer and spill exceptional blocks to a temporary file.
  std::vector<unsigned char> pending_deletion_bytes_;
  FILE *pending_deletions_ = nullptr;
  bool spooling_deletions_ = false;
  // A staged DSR-unit batch spools the next deletion line; see del().
  FILE *begin_pending_deletion();
  void end_pending_deletion(FILE *main_file);

#ifdef TACTIX_ENABLE_PROOF_SHADOW
  // Diagnostic N2 oracle (Proof::arm_population_shadow).
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
    if (!variable_map_)
      return l;
    assert(var_of(l) < variable_map_->size());
    return make_lit((*variable_map_)[var_of(l)], negated(l));
  }
  // The population view of internal literals under the same map.
  PopulationClauseView internal_view(const Lit *lits,
                                     uint32_t size) const noexcept {
    return PopulationClauseView::internal_lits(lits, size, variable_map_);
  }

  // --- Streaming-PBP numbering and deletion ---
  template <class External>
  bool put_pbp_deletion(uint64_t del_id, uint32_t size, External external,
                        bool replaying);
  // `external()` yields the constraint's checker literals; it runs only for
  // an anonymous (id-0) constraint that may later need its exact identity.
  template <class External>
  void note_pbp_derived(uint64_t id, uint32_t clause_size, External external);
  void register_pbp_anonymous(const int32_t *lits, uint32_t size);
  bool put_pbp_anonymous_deletion(const int32_t *canonical, uint32_t size);
  bool put_pbp_rebind_deletion();
  bool pbp_current_ordinal(uint64_t &constraint) const;
  bool pbp_absolute_constraint(uint64_t stored, uint64_t &constraint) const;
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
  void end_line();
  uint64_t pbp_original_constraints_ = 0;
  uint64_t pbp_derived_constraints_ = 0;
  // Constraint IDs match Tactix IDs through the ordinary raw-input prefix.
  // After an input-time derivation splits the namespaces, later raw inputs
  // keep strictly increasing Tactix ids: runs of consecutive ids name their
  // input constraints, so the input side costs one run per split plus two
  // bits per input (deleted by specification; already deleted), never one
  // map entry per clause. `pbp_constraint_ids_` names derived constraints
  // only; the high bit marks a derived ordinal whose final absolute ID
  // becomes known when input parsing finishes.
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
  // Anonymous (id-0) derived constraints of an exact-identity width, keyed by
  // their sorted checker literals and compared exactly. Each key holds a stack
  // of proof constraint IDs (high bit: transaction-relative ordinal). Every ID
  // lives in exactly one of this registry and `pbp_constraint_ids_`, so a
  // registry deletion names an anonymous constraint whose literals equal the
  // requested clause and that no other deletion can still name.
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
  uint64_t pbp_last_deleted_ = 0; // constraint named by the last `del id`
  // Assertion-build audit of exact deletion (proof_writer.cpp compiled
  // without NDEBUG; the members exist in every build so the layout never
  // depends on a translation unit's NDEBUG): the sorted canonical literals of
  // every derived constraint and input, by proof ID. Each `del id` must name
  // a live constraint whose literals equal the request's.
  std::unordered_map<uint64_t, std::vector<int32_t>> pbp_audit_input_;
  std::unordered_map<uint64_t, std::vector<int32_t>> pbp_audit_derived_;
  void pbp_audit_record(bool input, uint64_t index, const int32_t *lits,
                        uint32_t size);
  void pbp_audit_deletion(uint64_t constraint, const int32_t *lits,
                          uint32_t size);
  Proof::PbpDeletionCounters pbp_counters_;
  std::vector<int32_t> pbp_external_scratch_;
  std::vector<int32_t> pbp_canonical_scratch_;
  std::vector<uint32_t> pbp_canonical_order_;
  std::vector<uint8_t> pbp_canonical_keep_;
  std::array<unsigned char, 1 << 20> binary_buffer_{};
  size_t binary_buffer_size_ = 0;
};

} // namespace tactix
