// The proof ledger: every answer the search reads, the same in every mode.
// The writer (proof_writer.cpp) exists only when a proof is written; the
// ledger hands it each event it accepts, at the event's own point.
#include "proof/proof.hpp"
#include "proof/proof_writer.hpp"
#include "kernel/types.hpp"
#ifdef TACTIX_ENABLE_RULE_JOURNAL
#include "diagnostics/rule_journal.hpp"
#endif

#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstdlib>
#include <set>
#include <utility>

namespace tactix {

void proof_error(const char *what) {
  std::fflush(stdout);
  std::fprintf(stderr, "tactix: proof error: %s\n", what);
  std::_Exit(1);
}

uint64_t PbpParityBoundsCache::empty_retained_bytes() noexcept {
  return uint64_t{2} * sizeof(uint64_t) +
         sizeof(std::map<PbpParityEquation, PbpParityBounds>);
}

uint64_t PbpParityBoundsCache::retained_bytes() const noexcept {
#ifndef NDEBUG
  uint64_t bytes = entries_.empty() ? 0 : empty_retained_bytes();
  for (const auto &[equation, bounds] : entries_) {
    const uint64_t entry = sizeof(equation) + sizeof(bounds) +
        equation.variables.capacity() * sizeof(int32_t) +
        bounds.carries.capacity() * sizeof(int32_t);
    bytes = entry > UINT64_MAX - bytes ? UINT64_MAX : bytes + entry;
  }
  assert(bytes == retained_bytes_);
#endif
  return retained_bytes_;
}

DsrWitnessPlan plan_dsr_witness(const Lit *lits, uint32_t size,
                                const Lit *true_lits, uint32_t ntrue,
                                const DsrLiteralMapping *mappings,
                                uint32_t nmappings) {
  DsrWitnessPlan plan;
  if (!size)
    return plan;
  const Var pivot_var = var_of(lits[0]);
  const DsrLiteralMapping *pivot_mapping = nullptr;
  for (uint32_t i = 0; i < nmappings && !pivot_mapping; ++i)
    if (var_of(mappings[i].from) == pivot_var)
      pivot_mapping = &mappings[i];
  if (!pivot_mapping)
    return plan;

  enum class Image : uint8_t { True, False, Open };
  const auto in_clause = [&](Lit lit) {
    for (uint32_t i = 0; i < size; ++i)
      if (lits[i] == lit)
        return true;
    return false;
  };
  const auto image_of = [&](uint32_t at, Lit &image) {
    const Lit lit = lits[at];
    for (uint32_t i = 0; i < nmappings; ++i)
      if (var_of(mappings[i].from) == var_of(lit)) {
        image = negated(lit) ? negate(mappings[i].to) : mappings[i].to;
        return in_clause(negate(image)) ? Image::True
               : in_clause(image)       ? Image::False
                                        : Image::Open;
      }
    for (uint32_t i = 0; i < ntrue; ++i)
      if (var_of(true_lits[i]) == var_of(lit))
        return true_lits[i] == lit ? Image::True : Image::False;
    return Image::False; // an identity image of a clause literal
  };

  uint32_t open = 0;
  for (uint32_t at = 0; at < size; ++at) {
    Lit image = INVALID_LIT;
    switch (image_of(at, image)) {
    case Image::True:
      plan.form = DsrWitnessForm::Repivot;
      plan.pivot = at;
      plan.lemma = INVALID_LIT;
      return plan;
    case Image::Open:
      if (!open++) {
        plan.pivot = at;
        plan.lemma = image;
      }
      break;
    case Image::False:
      break;
    }
  }
  plan.form = !open        ? DsrWitnessForm::Rup
              : open == 1 ? DsrWitnessForm::Lemma
                          : DsrWitnessForm::Extension;
  if (plan.form != DsrWitnessForm::Lemma) {
    plan.pivot = 0;
    plan.lemma = INVALID_LIT;
  }
  return plan;
}

namespace {
uint64_t next_proof_stream_identity() noexcept {
  static std::atomic<uint64_t> next{1};
  uint64_t value=next.load(std::memory_order_relaxed);
  while(value) {
    const uint64_t successor=value==UINT64_MAX?0:value+1;
    if(next.compare_exchange_weak(value,successor,std::memory_order_relaxed))return value;
  }
  return 0; // Exhaustion is permanent, never wraps into a reused identity.
}
}

Proof::Proof() = default;

Proof::~Proof() {
  if (writer_)
    writer_->teardown_copy();
  // An unfinished stream keeps every staged root unit (see add_rup).
  if (live_stream() && !finished_)
    settle_staged(false);
  flush();
  bind_population_observer(nullptr);
}

bool Proof::start(std::unique_ptr<ProofWriter> writer, bool streaming) {
  stream_identity_ = 0;
  if (open_ || !(stream_identity_ = next_proof_stream_identity()))
    return false;
  open_ = true;
  streaming_ = input_transaction_ = streaming;
  published_ = !streaming;
  writer_ = std::move(writer);
  if (writer_) {
    writer_->set_population_observer(population_observer_);
    writer_->bind_variable_map(variable_map_);
    writer_->bind_external_variable_map(external_variable_map_);
    writer_->opened();
  }
  return true;
}

bool Proof::open(const char *path, ProofFormat format) {
  stream_identity_ = 0; // Failed opens cannot lend cached-unit authority.
  if (open_ || format == ProofFormat::Pbp)
    return false;
  FILE *const file = fopen(path, "wb");
  // N2: the kernel only ever answers to a streaming ledger.
  if (format != ProofFormat::PbpStreaming)
    return file && open_dsr_only(file, format == ProofFormat::DsrBinary);
  std::unique_ptr<ProofWriter> writer = ProofWriter::pbp(file);
  return writer && start(std::move(writer), true);
}

bool Proof::open_dsr_only(FILE *destination, bool binary) {
  std::unique_ptr<ProofWriter> writer = ProofWriter::pbp(destination);
  if (!writer || !start(std::move(writer), true))
    return false;
  writer_->start_dsr_only(binary);
  return true;
}

bool Proof::open_dsr_writer(const char *path, ProofFormat format) {
  stream_identity_ = 0;
  if (open_ ||
      (format != ProofFormat::DsrBinary && format != ProofFormat::DsrAscii))
    return false;
  std::unique_ptr<ProofWriter> writer =
      ProofWriter::dsr(fopen(path, "wb"), format, false);
  return writer && start(std::move(writer), false);
}

bool Proof::open_nowhere() {
  stream_identity_ = 0;
  return !open_ && start(nullptr, true);
}

bool Proof::open_automatic(const char *path, bool binary_dsr,
                           const char *stage_path, bool dsr_only) {
  if (dsr_only)
    return open(path, binary_dsr ? ProofFormat::DsrBinary : ProofFormat::DsrAscii);
  if (!open(path, ProofFormat::PbpStreaming))
    return false;
  writer_->start_automatic(path, binary_dsr, stage_path);
  return true;
}

// --- Lifecycle ----------------------------------------------------------------

bool Proof::commit() {
  if (!open_ || !streaming_)
    return true;
  if (terminal_)
    return false; // a terminal PB stream cannot accept a clausal suffix
  if (!published_) {
    published_ = true;
    if (writer_)
      writer_->publish();
  }
  return true;
}

bool Proof::commit_clause_stream() {
  const bool committed = commit();
  check_lifecycle();
  return committed;
}

void Proof::check_lifecycle() const {
#ifndef NDEBUG
  if (writer_)
    writer_->check_lifecycle(input_transaction_, published_, terminal_,
                             finished_);
#endif
}

void Proof::make_terminal() noexcept {
  terminal_ = true;
  // Staged root units belong to the discarded clausal stream.
  staged_.clear();
  deferring_ = false;
}

void Proof::finish_input_transaction() {
  input_transaction_ = false;
  if (writer_) {
    writer_->finish_input();
    writer_->forward([](Proof &copy) { copy.finish_input_transaction(); });
  }
  check_lifecycle();
}

void Proof::flush() {
  if (live_stream() && !published_)
    published_ = true;
  if (writer_) {
    writer_->flush();
    writer_->forward([](Proof &copy) { copy.flush(); });
  }
  check_lifecycle();
}

FILE *Proof::begin_pb() {
  if (writer_)
    writer_->check_startup_pb();
  if (!startup_pb_available())
    return nullptr;
  make_terminal();
  FILE *const terminal = writer_ ? writer_->startup_pb_begin() : nullptr;
  check_lifecycle();
  return terminal;
}

bool Proof::finish_pb() { return writer_ && writer_->finish_pb(); }

bool Proof::refute_parity(const std::vector<PbpParityEquation> &equations,
                          const std::function<bool(FILE *)> &write_pb) {
  if (!startup_pb_available())
    return false;
  make_terminal();
  if (writer_)
    writer_->refute_parity(equations, write_pb);
  check_lifecycle();
  return true;
}

bool Proof::refute_by_counting(const std::function<bool(FILE *)> &write_pb,
                               std::vector<int32_t> &input) {
  if (!startup_pb_available())
    return false;
  make_terminal();
  if (writer_)
    writer_->refute_by_counting(write_pb, input);
  check_lifecycle();
  return true;
}

bool Proof::dsr_follow_on_pending() const noexcept {
  return writer_ && writer_->follow_on_pending();
}

const std::vector<int32_t> &Proof::follow_on_input() const noexcept {
  static const std::vector<int32_t> none;
  return writer_ ? writer_->follow_on_input() : none;
}

bool Proof::open_dsr_follow_on(Proof &f) {
  return writer_ && writer_->open_follow_on(f);
}

bool Proof::settle_dsr_follow_on(const Proof &f) {
  return writer_ && writer_->settle_follow_on(f);
}

// --- Bindings and reporting -----------------------------------------------------

void Proof::bind_variable_map(const std::vector<Var> *map) {
  variable_map_ = map;
  if (writer_)
    writer_->bind_variable_map(map);
}

void Proof::bind_external_variable_map(const std::vector<Var> *map) {
  external_variable_map_ = map;
  if (writer_)
    writer_->bind_external_variable_map(map);
}

void Proof::bind_population_observer(PopulationObserver *observer) noexcept {
  if (observer == population_observer_)
    return;
  const auto route = [&] {
    return writer_ ? writer_->population_route() : CheckerRoute::None;
  };
  const auto naming = [&] {
    return writer_ ? writer_->population_naming() : CheckerNaming::None;
  };
  if (population_observer_) {
    // An explicit clausal route can still own a staged DSR-unit transaction.
    // Finish it while the observer that saw the StageAdd/StageDelete events is
    // alive.  Otherwise a direct replacement (or a nested scoped binding)
    // gives the old observer an apparently clean unbind and the new observer a
    // commit for stages it never saw. An unpublished streaming-PBP prefix is
    // deliberately not settled merely by changing observers.
    const bool complete = settle_population_observer();
    CheckerPopulationEvent event;
    event.kind = !complete
                     ? CheckerPopulationEventKind::ObserverUnboundIncomplete
                     : CheckerPopulationEventKind::ObserverUnbound;
    event.route = route();
    event.naming = naming();
    population_observer_->observe(event);
  }
  population_observer_ = observer;
  if (writer_)
    writer_->set_population_observer(observer);
  if (population_observer_) {
    CheckerPopulationEvent event;
    event.kind = CheckerPopulationEventKind::ObserverBound;
    event.route = route();
    event.naming = naming();
    population_observer_->observe(event);
  }
}

bool Proof::settle_population_observer() noexcept {
  if (live_stream() && !published_)
    return false;
  flush();
  return true;
}

bool Proof::automatic() const noexcept {
  return writer_ && writer_->automatic();
}

AutomaticProofReason Proof::automatic_reason() const noexcept {
  return writer_ ? writer_->automatic_reason() : AutomaticProofReason::Pending;
}

bool Proof::automatic_dsr_live() const noexcept {
  return writer_ && writer_->automatic_dsr_live();
}

void Proof::release_lazy_event() noexcept {
  if (writer_)
    writer_->release_lazy_event();
}

const LazyPbpReport &Proof::lazy_pbp_report() const noexcept {
  static const LazyPbpReport off;
  return writer_ ? writer_->lazy_report() : off;
}

const char *Proof::verification_route() const {
  return writer_ ? writer_->verification_route() : nullptr;
}

void Proof::disable_pbp_id_deletions() {
  if (writer_)
    writer_->disable_id_deletions();
}

const Proof::PbpDeletionCounters &Proof::pbp_deletion_counters() const noexcept {
  static const PbpDeletionCounters none;
  return writer_ ? writer_->counters() : none;
}

#ifdef TACTIX_ENABLE_PROOF_SHADOW
void Proof::arm_population_shadow() {
  if (writer_)
    writer_->arm_population_shadow();
}

bool Proof::population_contains(uint64_t id, const Lit *lits,
                                uint32_t size) const {
  return !writer_ || writer_->population_contains(id, lits, size);
}

uint64_t Proof::unbacked_deletions() const {
  return writer_ ? writer_->unbacked_deletions() : 0;
}

bool Proof::population_shadow() const {
  return writer_ && writer_->population_shadow();
}
#endif

// --- Clausal events -------------------------------------------------------------

// Duplicate/complementary-literal scan. Small clauses only reach here, so the
// quadratic form beats any scratch allocation; it runs in debug builds only.
bool Proof::tautological(const Lit *lits, uint32_t size) {
  for (uint32_t i = 0; i < size; i++)
    for (uint32_t j = i + 1; j < size; j++)
      if (lits[i] == lits[j] || lits[i] == negate(lits[j]))
        return true;
  return false;
}

bool Proof::external_tautological(const int32_t *lits, uint32_t size) {
  for (uint32_t i = 0; i < size; ++i) {
    if (!lits[i] || lits[i] == INT32_MIN)
      return true;
    for (uint32_t j = i + 1; j < size; ++j)
      if (lits[i] == lits[j] ||
          static_cast<int64_t>(lits[i]) == -static_cast<int64_t>(lits[j]))
        return true;
  }
  return false;
}

void Proof::settle_staged(bool drop) {
  if (staged_.empty())
    return;
  std::vector<StagedUnit> units = std::move(staged_);
  staged_.clear();
  if (writer_)
    writer_->staged_settled(units.size(), drop);
  if (!drop)
    for (const StagedUnit &unit : units)
      rup_event(unit.id, &unit.lit, 1, CheckerClauseRole::DerivedUnit);
}

void Proof::begin_deferred_root_units() noexcept {
  deferring_ = live_stream() && !finished_;
}

void Proof::end_deferred_root_units() noexcept {
  if (deferring_ && !staged_.empty() && writer_)
    writer_->staged_batch();
  deferring_ = false;
}

void Proof::rup_event(uint64_t id, const Lit *lits, uint32_t size,
                      CheckerClauseRole role) {
  settle_staged(!size && streaming_);
  if (!input_transaction_ && !commit())
    std::abort();
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  if (!copy_role_) // the automatic DSR copy repeats an already noted event
    rule_journal_note_rup(id);
#endif
  assert(!tautological(lits, size)); // see Proof::tautological
  if (!open_)
    return;
  if (streaming_ && finished_)
    std::abort(); // nothing follows the empty clause
  if (writer_)
    writer_->rup(id, lits, size, role);
  if (streaming_ && !size)
    finished_ = true;
}

void Proof::add_rup(uint64_t id, const Lit *lits, uint32_t size) {
  rup_event(id, lits, size, CheckerClauseRole::Rup);
  if (writer_)
    writer_->forward([&](Proof &copy) { copy.add_rup(id, lits, size); });
  check_lifecycle();
}

void Proof::add_empty(uint64_t id) {
  rup_event(id, nullptr, 0, CheckerClauseRole::Empty);
  if (writer_)
    writer_->forward([&](Proof &copy) { copy.add_empty(id); });
  check_lifecycle();
}

void Proof::add_derived_unit(uint64_t id, Lit unit) {
  // Native DSR keeps root units explicit so they remain available after a
  // reason clause is deleted and later direct-SR checks see the same root
  // formula as dsr-trim/Trestle.
  if (open_ && !terminal_) {
    if (deferring_) {
      staged_.push_back({id, unit});
      if (writer_)
        writer_->staged_unit();
    } else {
      rup_event(id, &unit, 1, CheckerClauseRole::DerivedUnit);
    }
  }
  if (writer_)
    writer_->forward([&](Proof &copy) { copy.add_derived_unit(id, unit); });
  check_lifecycle();
}

void Proof::add_rat(uint64_t id, const Lit *lits, uint32_t size) {
  settle_staged(false);
  if (size) {
    invalidate_premises_for_sr(id, lits, 1, nullptr, 0, nullptr, 0);
    flush_persistent_external(id);
    if (writer_)
      writer_->retire_parity(lits, 1, nullptr, 0, nullptr, 0);
  }
  if (!input_transaction_ && !commit())
    std::abort();
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  if (!copy_role_)
    rule_journal_note_rat(id);
#endif
  assert(!tautological(lits, size)); // see Proof::tautological
  if (open_) {
    if (streaming_ && (!size || finished_))
      std::abort();
    if (writer_)
      writer_->rat(id, lits, size);
  }
  if (writer_)
    writer_->forward([&](Proof &copy) { copy.add_rat(id, lits, size); });
  check_lifecycle();
}

bool Proof::add_sr(uint64_t id, const Lit *lits, uint32_t size,
                   const Lit *true_lits, uint32_t ntrue,
                   const DsrLiteralMapping *mappings, uint32_t nmappings,
                   uint32_t external_variables, bool propagates) {
  if (writer_) {
    writer_->before_sr(lits, size, true_lits, ntrue, mappings, nmappings,
                       external_variables, propagates);
    writer_->hold();
  }
  const bool written = sr_event(id, lits, size, true_lits, ntrue, mappings,
                                nmappings, external_variables, propagates);
  if (writer_)
    writer_->forward_held_strict(written, [&](Proof &copy) {
      return copy.add_sr(id, lits, size, true_lits, ntrue, mappings, nmappings,
                         external_variables, propagates);
    });
  check_lifecycle();
  return written;
}

bool Proof::sr_event(uint64_t id, const Lit *lits, uint32_t size,
                     const Lit *true_lits, uint32_t ntrue,
                     const DsrLiteralMapping *mappings, uint32_t nmappings,
                     uint32_t external_variables, bool propagates) {
  settle_staged(false);
  if (!input_transaction_ && !commit())
    return false;
  assert(size > 0);
  assert(lits);
  assert(!tautological(lits, size));
  assert(!ntrue || true_lits);
  assert(!nmappings || mappings);

#ifndef NDEBUG
  const Lit pivot = lits[0];
  for (uint32_t i = 0; i < ntrue; ++i) {
    assert(var_of(true_lits[i]) != var_of(pivot));
    for (uint32_t j = 0; j < i; ++j)
      assert(var_of(true_lits[i]) != var_of(true_lits[j]));
  }
  for (uint32_t i = 0; i < nmappings; ++i) {
    assert(mappings[i].from != INVALID_LIT);
    assert(mappings[i].to != INVALID_LIT);
    assert(mappings[i].from != mappings[i].to ||
           var_of(mappings[i].from) == var_of(pivot));
    for (uint32_t j = 0; j < ntrue; ++j)
      assert(var_of(mappings[i].from) != var_of(true_lits[j]));
    for (uint32_t j = 0; j < i; ++j)
      assert(var_of(mappings[i].from) != var_of(mappings[j].from));
  }
#endif

  if (!open_)
    return true;
  if (streaming_ ? finished_
                 : writer_ && writer_->sr_needs_missing_auxiliary(
                                  lits, size, true_lits, ntrue, mappings,
                                  nmappings, external_variables, propagates))
    return false;
  const DsrWitnessPlan plan =
      plan_dsr_witness(lits, size, true_lits, ntrue, mappings, nmappings);
  // A witness must hold against every clause the checker has, so the leases it
  // changes, all retained support and the parity constraints it touches go first. A
  // witness-free line transforms nothing, and retained constraints only strengthen RUP.
  if (plan.form != DsrWitnessForm::Rup) {
    invalidate_premises_for_sr(id, lits, size, true_lits, ntrue, mappings, nmappings);
    flush_persistent_external(id);
    if (writer_)
      writer_->retire_parity(lits, size, true_lits, ntrue, mappings, nmappings);
  }
  if (writer_)
    writer_->sr(id, lits, size, true_lits, ntrue, mappings, nmappings,
                external_variables, propagates, plan);
  return true;
}

void Proof::del_event(uint64_t line_id, uint64_t del_id, const Lit *lits,
                      uint32_t size) {
  settle_staged(false);
  if (!leased_premises_.empty() && size <= premise_maximum_width_) {
    std::vector<int32_t> external; external.reserve(size);
    for (uint32_t i = 0; i < size; ++i)
      external.push_back(externalize(checker_lit(lits[i])));
    if (defer_leased_deletion(external.data(), size)) return;
  }
  if (!input_transaction_ && !commit())
    std::abort();
  if (writer_)
    writer_->del(line_id, del_id, lits, size);
}

void Proof::del(uint64_t line_id, uint64_t del_id, const Lit *lits,
                uint32_t size) {
  del_event(line_id, del_id, lits, size);
  if (writer_)
    writer_->forward(
        [&](Proof &copy) { copy.del(line_id, del_id, lits, size); });
  check_lifecycle();
}

bool Proof::external_rup_event(uint64_t id, const int32_t *lits, uint32_t size,
                               CheckerClauseRole role) {
  settle_staged(!size && streaming_);
  if (!input_transaction_ && !commit())
    return false;
  assert(!size || lits);
  if (external_tautological(lits, size))
    return false;
  if (!open_)
    return true;
  if (streaming_ && finished_)
    return false;
  if (writer_)
    writer_->external_rup(id, lits, size, role);
  if (streaming_ && !size)
    finished_ = true;
  return true;
}

bool Proof::add_external_rup(uint64_t id, const int32_t *lits,
                             uint32_t size) {
  if (writer_)
    writer_->hold();
  const bool written =
      external_rup_event(id, lits, size, CheckerClauseRole::ExternalRup);
  if (writer_)
    writer_->forward_held(written, [&](Proof &copy) {
      return copy.add_external_rup(id, lits, size);
    });
  check_lifecycle();
  return written;
}

bool Proof::external_rat_event(uint64_t id, const int32_t *lits,
                               uint32_t size) {
  settle_staged(false);
  if (size && !premise_groups_.empty()) {
    const auto *saved = variable_map_; variable_map_ = nullptr;
    const Lit pivot = internalize(lits[0]);
    const uint64_t before = premise_epoch_;
    invalidate_premises_for_sr(id, &pivot, 1, nullptr, 0, nullptr, 0);
    variable_map_ = saved;
    if (premise_epoch_ != before) flush_persistent_external(id);
  }
  if (!input_transaction_ && !commit())
    return false;
  // DSR infers RAT from the first literal after RUP fails, so its byte stream
  // is identical to an external RUP addition.
  if (!streaming_)
    return external_rup_event(id, lits, size, CheckerClauseRole::ExternalRat);
  if (!size || !lits || finished_)
    return false;
  if (writer_)
    writer_->external_rat(id, lits, size);
  return true;
}

bool Proof::add_external_rat(uint64_t id, const int32_t *lits,
                             uint32_t size) {
  const bool skip = writer_ && writer_->before_external_rat(lits, size);
  if (writer_)
    writer_->hold();
  const bool written = external_rat_event(id, lits, size);
  if (writer_)
    writer_->forward_held(written && !skip, [&](Proof &copy) {
      return copy.add_external_rat(id, lits, size);
    });
  check_lifecycle();
  return written;
}

bool Proof::del_external_event(uint64_t line_id, uint64_t del_id,
                               const int32_t *lits, uint32_t size) {
  settle_staged(false);
  if (defer_leased_deletion(lits, size)) return true;
  if (!input_transaction_ && !commit())
    return false;
  assert(!size || lits);
  if (writer_)
    writer_->del_external(line_id, del_id, lits, size, replaying_leased_);
  return true;
}

bool Proof::del_external(uint64_t line_id, uint64_t del_id,
                         const int32_t *lits, uint32_t size) {
  if (writer_)
    writer_->hold();
  const bool written = del_external_event(line_id, del_id, lits, size);
  if (writer_)
    writer_->forward_held(written, [&](Proof &copy) {
      return copy.del_external(line_id, del_id, lits, size);
    });
  check_lifecycle();
  return written;
}

bool Proof::retain_external(uint64_t id, const int32_t *lits, uint32_t size) {
  if (writer_)
    writer_->hold();
  const bool retained = retain_external_event(id, lits, size);
  if (writer_)
    writer_->forward_held(retained, [&](Proof &copy) {
      return copy.retain_external(id, lits, size);
    });
  check_lifecycle();
  return retained;
}

bool Proof::retain_premise_groups(const std::vector<PremiseGroup> &groups,
    uint64_t maximum_groups, uint64_t maximum_literals,
    uint64_t maximum_bytes, const std::function<bool(uint64_t)> &charge,
    std::vector<uint64_t> &handles, DeferredDeletionLimits limits) {
  if (writer_)
    writer_->hold();
  const bool retained = retain_premise_groups_event(
      groups, maximum_groups, maximum_literals, maximum_bytes, charge, handles,
      limits);
  if (writer_)
    writer_->forward_held(retained, [&](Proof &copy) {
      // The kernel's work meter is charged once, by this ledger.
      const std::function<bool(uint64_t)> free_charge = [](uint64_t) {
        return true;
      };
      std::vector<uint64_t> copy_handles;
      return copy.retain_premise_groups(groups, maximum_groups,
                                        maximum_literals, maximum_bytes,
                                        free_charge, copy_handles, limits) &&
             copy_handles == handles;
    });
  check_lifecycle();
  return retained;
}

void Proof::release_premise_groups(const std::vector<uint64_t> &handles,
                                   uint64_t line_id) {
  release_premise_groups_event(handles, line_id);
  if (writer_)
    writer_->forward([&](Proof &copy) {
      copy.release_premise_groups(handles, line_id);
    });
  check_lifecycle();
}

void Proof::seed_population(uint64_t id, const Lit *lits, uint32_t size) {
  if (writer_) {
    writer_->seed(id, lits, size);
    writer_->forward(
        [&](Proof &copy) { copy.seed_population(id, lits, size); });
  }
  check_lifecycle();
}

// --- Parity reasons ---------------------------------------------------------------

bool Proof::plan_pbp_parity_reason(
    const std::vector<PbpParityEquation> &equations, const int32_t *reason,
    uint32_t reason_size, uint32_t starting_external_extent,
    const PbpParityBoundsCache &cache, PbpParityReasonPlan &output) const {
  PbpParityReasonPlan plan;
  if (!live_stream() || input_transaction_ || !published_ || finished_ ||
      equations.empty() ||
      (!reason && reason_size) ||
      starting_external_extent > static_cast<uint32_t>(INT32_MAX))
    return false;

  std::set<int32_t> odd;
  std::set<PbpParityEquation> new_rows;
  bool rhs = false;
  const bool matching_cache = cache.stream_identity_ == stream_identity_;
  uint64_t constraints = 3;
  uint64_t retained = matching_cache ? cache.retained_bytes()
                                     : cache.empty_retained_bytes();
  uint64_t helpers = 0;
  for (const PbpParityEquation &equation : equations) {
    if (equation.variables.size() < 2 || equation.variables.size() > 8 ||
        !std::is_sorted(equation.variables.begin(), equation.variables.end()) ||
        std::adjacent_find(equation.variables.begin(),
                           equation.variables.end()) !=
            equation.variables.end())
      return false;
    for (const int32_t variable : equation.variables) {
      if (variable <= 0 ||
          static_cast<uint32_t>(variable) > starting_external_extent)
        return false;
      const auto found = odd.find(variable);
      if (found == odd.end()) odd.insert(variable);
      else odd.erase(found);
    }
    rhs ^= equation.rhs;
    const bool cached = matching_cache && cache.entries_.contains(equation);
    if (cached || !new_rows.insert(equation).second)
      continue;
    const uint64_t carries = equation.variables.size() / 2;
    const uint64_t bound_constraints =
        (uint64_t{1} << equation.variables.size()) + 1;
    const uint64_t row_constraints = 6 * carries + 2 * bound_constraints;
    if (row_constraints > UINT64_MAX - constraints ||
        carries > UINT64_MAX - helpers)
      return false;
    constraints += row_constraints;
    helpers += carries;
    const uint64_t bytes = 2 * (sizeof(PbpParityEquation) +
        sizeof(PbpParityBounds) +
        equation.variables.size() * sizeof(int32_t) +
        carries * sizeof(int32_t));
    if (bytes > UINT64_MAX - retained)
      return false;
    retained += bytes;
  }
  if (helpers > UINT32_MAX || helpers > INT32_MAX - starting_external_extent)
    return false;

  std::map<int32_t, int32_t> target;
  bool falsifying_parity = false;
  for (uint32_t i = 0; i < reason_size; ++i) {
    const int32_t literal = reason[i];
    if (!literal || literal == INT32_MIN)
      return false;
    const int32_t variable =
        static_cast<int32_t>(external_variable_of(literal));
    if (static_cast<uint32_t>(variable) > starting_external_extent ||
        !target.emplace(variable, literal).second)
      return false;
    if (odd.contains(variable))
      falsifying_parity ^= literal < 0;
  }
  for (const int32_t variable : odd)
    if (!target.contains(variable))
      return false;
  if (falsifying_parity == rhs)
    return false;

  plan.stream_identity = stream_identity_;
  plan.cache_generation = matching_cache ? cache.generation_ : 0;
  plan.proof_constraints = constraints;
  plan.retained_bytes = retained;
  plan.starting_external_extent = starting_external_extent;
  plan.helper_variables = static_cast<uint32_t>(helpers);
  plan.odd_variables.assign(odd.begin(), odd.end());
  plan.valid = true;
  output = plan;
  return true;
}

bool Proof::add_prevalidated_pbp_parity_reason(
    uint64_t kernel_clause_id,
    const std::vector<PbpParityEquation> &equations, const int32_t *reason,
    uint32_t reason_size, PbpParityBoundsCache &cache,
    const PbpParityReasonPlan &plan) {
  settle_staged(false);
  const bool matching_cache = cache.stream_identity_ == stream_identity_;
  if (!plan.valid || plan.stream_identity != stream_identity_ ||
      !live_stream() || input_transaction_ || !published_ || finished_ ||
      equations.empty() || (!reason && reason_size) ||
      (matching_cache ? cache.generation_ : 0) != plan.cache_generation)
    return false;
  // The copy's clausal spelling, or the conversion it triggers, comes first.
  if (writer_)
    writer_->parity_copy(equations, reason, reason_size);
  if (!matching_cache) {
    cache.entries_.clear();
    cache.retained_bytes_ = 0;
    cache.generation_ = 0;
    cache.stream_identity_ = stream_identity_;
  }
  // A new row reserves its carry names; the writer writes its bounds.
  uint32_t fresh = plan.starting_external_extent;
  for (const PbpParityEquation &equation : equations) {
    if (cache.entries_.contains(equation))
      continue;
    PbpParityBounds bounds;
    for (uint32_t threshold = 2;
         threshold <= equation.variables.size(); threshold += 2) {
      if (fresh == static_cast<uint32_t>(INT32_MAX))
        std::abort();
      bounds.carries.push_back(static_cast<int32_t>(++fresh));
    }
    const auto found = cache.entries_.emplace(equation, std::move(bounds)).first;
    uint64_t retained = cache.retained_bytes_;
    if (!retained)
      retained = cache.empty_retained_bytes();
    const uint64_t entry = sizeof(found->first) + sizeof(found->second) +
        found->first.variables.capacity() * sizeof(int32_t) +
        found->second.carries.capacity() * sizeof(int32_t);
    if (entry > UINT64_MAX - retained)
      std::abort();
    cache.retained_bytes_ = retained + entry;
    if (cache.generation_ == UINT64_MAX)
      std::abort();
    ++cache.generation_;
  }
  if (fresh != plan.starting_external_extent + plan.helper_variables ||
      cache.retained_bytes() > plan.retained_bytes)
    std::abort();
  if (writer_)
    writer_->parity_reason(kernel_clause_id, equations, reason, reason_size,
                           cache, plan);
  if (!reason_size)
    finished_ = true;
  if (writer_)
    writer_->settle_copy();
  check_lifecycle();
  return true;
}

// --- Premise leases and retained external clauses ---------------------------------

namespace {
std::vector<int32_t> premise_key(const int32_t *lits, uint32_t size) {
  std::vector<int32_t> result(lits, lits + size);
  std::sort(result.begin(), result.end());
  return result;
}
// Premise maintenance work saturates instead of wrapping.
void add_saturating(uint64_t &total, __uint128_t work) {
  total = work > UINT64_MAX - total ? UINT64_MAX : total + uint64_t(work);
}
}

bool Proof::can_defer_premise_group_deletions(
    const std::vector<PremiseGroup> &groups, DeferredDeletionLimits incoming,
    const std::function<bool(uint64_t)> &charge) const {
  if (!charge || terminal_ || !open_) return false;
  const bool bounded = incoming.maximum_pending_deletions || incoming.maximum_pending_literal_work;
  if (bounded && (!incoming.maximum_pending_deletions || !incoming.maximum_pending_literal_work)) return false;
  const uint64_t count_cap = std::min(premise_pending_deletion_cap_,
      bounded ? incoming.maximum_pending_deletions : UINT64_MAX);
  const uint64_t work_cap = std::min(premise_pending_work_cap_,
      bounded ? incoming.maximum_pending_literal_work : UINT64_MAX);
  uint64_t count = premise_pending_deletions_, work = premise_pending_literal_work_;
  if (count > count_cap || work > work_cap) return false;
  for (const auto &group : groups) {
    if (!charge(1)) return false;
    for (const auto &clause : group) {
      const uint64_t cost = 1 + uint64_t(clause.size());
      if (!charge(cost) || count == count_cap || cost > work_cap - work) return false;
      ++count; work += cost;
    }
  }
  return true;
}

bool Proof::retain_premise_groups_event(const std::vector<PremiseGroup> &groups,
    uint64_t maximum_groups, uint64_t maximum_literals,
    uint64_t maximum_bytes, const std::function<bool(uint64_t)> &charge,
    std::vector<uint64_t> &handles, DeferredDeletionLimits limits) {
  const bool bounded = limits.maximum_pending_deletions || limits.maximum_pending_literal_work;
  if (bounded && (!limits.maximum_pending_deletions || !limits.maximum_pending_literal_work ||
      premise_pending_deletions_ > limits.maximum_pending_deletions ||
      premise_pending_literal_work_ > limits.maximum_pending_literal_work)) return false;
  if (!charge || terminal_ || !open_ ||
      groups.size() > maximum_groups ||
      premise_groups_.size() > maximum_groups - groups.size() ||
      groups.size() > UINT64_MAX - next_premise_group_)
    return false;
  // Include both old and transactional candidate populations in peak memory.
  uint64_t literals = premise_literals_, bytes = premise_bytes_;
  for (const auto &group : groups) {
    if (group.empty() || !charge(1 + group.size())) return false;
    for (const auto &clause : group) {
      const uint64_t storage = premise_storage_bytes(clause.size());
      if (!charge(1 + uint64_t(clause.size()) * clause.size() +
              uint64_t(clause.size()) * (2 * std::bit_width(maximum_literals) + 2)) || clause.empty() ||
          external_tautological(clause.data(), clause.size()) ||
          clause.size() > maximum_literals - std::min(literals, maximum_literals) ||
          storage > maximum_bytes - std::min(bytes, maximum_bytes))
        return false;
      literals += clause.size(); bytes += storage;
    }
  }
  if (bytes > maximum_bytes / 2 || literals > maximum_literals ||
      !charge(1 + premise_literals_ + premise_groups_.size()))
    return false;
  auto premises = leased_premises_;
  auto retained = premise_groups_;
  std::vector<uint64_t> candidate;
  uint64_t next = next_premise_group_;
  for (const auto &group : groups) {
    PremiseGroup canonical;
    for (const auto &clause : group) {
      auto key = premise_key(clause.data(), clause.size());
      if (++premises[key].users == 0) return false;
      canonical.push_back(std::move(key));
    }
    candidate.push_back(++next);
    retained.emplace(next, std::move(canonical));
  }
  // Publish the clausal stream only after every resource and identity check
  // has passed. No lease can survive a terminal PB transition.
  if (input_transaction_ || !commit()) return false;
  leased_premises_.swap(premises); premise_groups_.swap(retained);
  next_premise_group_ = next; premise_literals_ = literals; premise_bytes_ = bytes;
  for (const uint64_t handle : candidate)
    count_premise_variables(premise_groups_.at(handle), true);
  for (const auto &group : groups)
    for (const auto &clause : group)
      premise_maximum_width_ = std::max<uint32_t>(premise_maximum_width_, clause.size());
  if (bounded) {
    premise_pending_deletion_cap_ = std::min(premise_pending_deletion_cap_, limits.maximum_pending_deletions);
    premise_pending_work_cap_ = std::min(premise_pending_work_cap_, limits.maximum_pending_literal_work);
  }
  handles.swap(candidate);
  return true;
}

bool Proof::premise_group_active(uint64_t handle) const {
  return handle && premise_groups_.contains(handle);
}
bool Proof::premise_group_matches(uint64_t handle, const PremiseGroup &group) const {
  const auto found = premise_groups_.find(handle);
  if (found == premise_groups_.end() || found->second.size() != group.size())
    return false;
  for (size_t i = 0; i < group.size(); ++i)
    if (found->second[i] != premise_key(group[i].data(), group[i].size()))
      return false;
  return true;
}

bool Proof::defer_leased_deletion(const int32_t *lits, uint32_t size) {
  if (leased_premises_.empty() || !size || size > premise_maximum_width_) return false;
  const uint64_t work = 1 + uint64_t(size) * size +
                        uint64_t(size) * (2 * std::bit_width(leased_premises_.size()) + 2);
  add_saturating(premise_maintenance_work_, work);
  const auto found = leased_premises_.find(premise_key(lits, size));
  if (found == leased_premises_.end()) return false;
  const uint64_t cost = 1 + uint64_t(size);
  if (premise_pending_deletions_ >= premise_pending_deletion_cap_ ||
      cost > premise_pending_work_cap_ - std::min(premise_pending_literal_work_, premise_pending_work_cap_)) {
    const __uint128_t scan = __uint128_t(1 + premise_groups_.size()) +
        __uint128_t(1 + size) * (premise_literals_ + premise_groups_.size());
    add_saturating(premise_maintenance_work_, scan);
    std::vector<uint64_t> affected;
    for (const auto &[handle, group] : premise_groups_)
      if (std::find(group.begin(), group.end(), found->first) != group.end()) affected.push_back(handle);
    release_premise_groups_event(affected, 0);
    return false;
  }
  ++premise_pending_deletions_; premise_pending_literal_work_ += cost;
  if (found->second.deferred == UINT64_MAX || premise_deferred_ == UINT64_MAX)
    std::abort();
  ++found->second.deferred; ++premise_deferred_;
  return true;
}

void Proof::release_premise_groups_event(const std::vector<uint64_t> &handles,
                                         uint64_t line_id) {
  bool changed = false;
  for (uint64_t handle : handles) {
    auto group = premise_groups_.find(handle);
    if (group == premise_groups_.end()) continue;
    const uint64_t maintenance = 1 + group->second.size();
    add_saturating(premise_maintenance_work_, maintenance);
    PremiseGroup clauses = std::move(group->second);
    premise_groups_.erase(group); changed = true;
    count_premise_variables(clauses, false);
    for (const auto &clause : clauses) {
      auto found = leased_premises_.find(clause);
      if (found == leased_premises_.end() || !found->second.users) std::abort();
      premise_literals_ -= clause.size();
      premise_bytes_ -= premise_storage_bytes(clause.size());
      if (--found->second.users) continue;
      const uint64_t deferred = found->second.deferred;
      const uint64_t scan = uint64_t(clause.size()) *
          (2 * std::bit_width(leased_premises_.size()) + 3);
      const __uint128_t serialization = __uint128_t(deferred) * (1 + clause.size()) + scan;
      add_saturating(premise_maintenance_work_, serialization);
      premise_pending_deletions_ -= deferred;
      premise_pending_literal_work_ -= deferred * (1 + uint64_t(clause.size()));
      leased_premises_.erase(found);
      // A deferred deletion no longer knows which identity it named, so it
      // must not resolve to an anonymous constraint (see put_pbp_deletion).
      replaying_leased_ = true;
      for (uint64_t i = 0; i < deferred; ++i)
        if (!del_external_event(line_id, 0, clause.data(), clause.size())) std::abort();
      replaying_leased_ = false;
    }
  }
  if (changed && ++premise_epoch_ == 0) std::abort();
}

void Proof::count_premise_variables(const PremiseGroup &group, bool add) {
  for (const auto &clause : group)
    for (const int32_t literal : clause) {
      const uint32_t variable = external_variable_of(literal);
      if (variable >= premise_variable_uses_.size())
        premise_variable_uses_.resize(variable + 1, 0);
      uint32_t &uses = premise_variable_uses_[variable];
      if (add ? uses == UINT32_MAX : !uses)
        std::abort();
      add ? ++uses : --uses;
    }
}

void Proof::invalidate_premises_for_sr(uint64_t line_id, const Lit *lits,
    uint32_t size, const Lit *true_lits, uint32_t ntrue,
    const DsrLiteralMapping *mappings, uint32_t nmappings) {
  if (premise_groups_.empty()) return;
  const uint64_t changed_bound = 1 + uint64_t(ntrue) + nmappings;
  std::vector<uint32_t> changed;
  const auto take = [&](Lit lit) {
    changed.push_back(var_of(checker_lit(lit)) + 1);
  };
  // The implicit pivot image is true even without an explicit mapping.
  if (size) take(lits[0]);
  for (uint32_t i = 0; i < ntrue; ++i) take(true_lits[i]);
  for (uint32_t i = 0; i < nmappings; ++i) take(mappings[i].from);
  std::sort(changed.begin(), changed.end());
  add_saturating(premise_maintenance_work_, changed_bound);
  bool leased = false;
  for (const uint32_t variable : changed)
    leased |= variable < premise_variable_uses_.size() &&
              premise_variable_uses_[variable];
#ifndef NDEBUG
  // The counts must agree with the scan they replace.
  bool scanned = false;
  for (const auto &entry : premise_groups_)
    for (const auto &clause : entry.second)
      for (const int32_t literal : clause)
        scanned |= std::binary_search(changed.begin(), changed.end(),
                                      external_variable_of(literal));
  assert(leased == scanned);
#endif
  if (!leased) return; // Most RAT pivots here are fresh helper variables.
  const uint64_t work = 1 + premise_groups_.size() +
      changed_bound * (2 * std::bit_width(changed_bound) + 2) +
      premise_literals_ * (std::bit_width(changed_bound) + 1);
  add_saturating(premise_maintenance_work_, work);
  std::vector<uint64_t> retire;
  for (const auto &[handle, clauses] : premise_groups_) {
    bool touched = false;
    for (const auto &clause : clauses)
      for (int32_t literal : clause)
        touched |= std::binary_search(changed.begin(), changed.end(),
                                      external_variable_of(literal));
    if (touched) retire.push_back(handle);
  }
  release_premise_groups_event(retire, line_id);
}

uint64_t Proof::persistent_external_bytes() const {
  uint64_t bytes = persistent_external_.capacity() * sizeof(PersistentExternalClause);
  for (const auto &clause : persistent_external_)
    bytes += clause.lits.capacity() * sizeof(int32_t);
  return bytes;
}

bool Proof::retain_external_event(uint64_t id, const int32_t *lits,
                                  uint32_t size) {
  if (!id || !size || !lits ||
      external_tautological(lits, size) || terminal_)
    return false;
  if (!open_)
    return true;
  for (const PersistentExternalClause &clause : persistent_external_)
    if (clause.id == id)
      return false;
  PersistentExternalClause clause;
  clause.id = id;
  clause.lits.assign(lits, lits + size);
  persistent_external_.push_back(std::move(clause));
  return true;
}

void Proof::flush_persistent_external(uint64_t line_id) {
  if (persistent_external_.empty())
    return;
  if (terminal_ || persistent_external_epoch_ == UINT64_MAX)
    std::abort();
  std::vector<PersistentExternalClause> clauses =
      std::move(persistent_external_);
  persistent_external_.clear();
  for (auto clause = clauses.rbegin(); clause != clauses.rend(); ++clause)
    if (!del_external_event(line_id, clause->id, clause->lits.data(),
                            clause->lits.size()))
      std::abort();
  ++persistent_external_epoch_;
}

} // namespace tactix
