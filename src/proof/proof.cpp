#include "proof/proof.hpp"
#include "kernel/types.hpp"
#ifdef TACTIX_ENABLE_RULE_JOURNAL
#include "diagnostics/rule_journal.hpp"
#endif

#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <charconv>
#include <cinttypes>
#include <cstdlib>
#include <limits>
#include <set>
#include <string_view>

#include <stdio_ext.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef TACTIX_ENABLE_PROOF_SHADOW
#include <execinfo.h>
#endif

namespace tactix {

namespace {

class PbpTextWriter {
public:
  explicit PbpTextWriter(FILE *file) : file_(file) {
    if (!file_)
      std::abort();
  }

  void put(std::string_view text) {
    while (!text.empty()) {
      if (size_ == buffer_.size())
        flush();
      const size_t count = std::min(text.size(), buffer_.size() - size_);
      std::copy_n(text.data(), count, buffer_.data() + size_);
      size_ += count;
      text.remove_prefix(count);
    }
  }

  void put_uint(uint64_t value) {
    std::array<char, 20> digits{};
    const auto converted =
        std::to_chars(digits.data(), digits.data() + digits.size(), value);
    if (converted.ec != std::errc{})
      std::abort();
    put(std::string_view(
        digits.data(), static_cast<size_t>(converted.ptr - digits.data())));
  }

  void flush() {
    if (size_ && std::fwrite(buffer_.data(), 1, size_, file_) != size_)
      std::abort();
    size_ = 0;
  }

private:
  FILE *file_;
  std::array<char, 1 << 16> buffer_;
  size_t size_ = 0;
};

void put_pbp_literal(PbpTextWriter &text, int32_t literal) {
  if (!literal || literal == INT32_MIN)
    std::abort();
  text.put("1 ");
  if (literal < 0)
    text.put("~");
  text.put("x");
  text.put_uint(external_variable_of(literal));
  text.put(" ");
}

// Append every remaining byte of `source` to `target`.
void copy_stream(FILE *source, FILE *target) {
  std::array<unsigned char, 65536> bytes{};
  for (;;) {
    const size_t count = std::fread(bytes.data(), 1, bytes.size(), source);
    if (count && std::fwrite(bytes.data(), 1, count, target) != count)
      std::abort();
    if (count < bytes.size()) {
      if (std::ferror(source))
        std::abort();
      break;
    }
  }
}

} // namespace

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

CheckerRoute Proof::population_route() const noexcept {
  if (!file_)
    return CheckerRoute::None;
  switch (format_) {
  case ProofFormat::DsrBinary:
  case ProofFormat::DsrAscii:
    return CheckerRoute::Dsr;
  case ProofFormat::DratBinary:
  case ProofFormat::DratAscii:
    return CheckerRoute::Drat;
  case ProofFormat::Lrat:
    return CheckerRoute::Lrat;
  case ProofFormat::Pbp:
  case ProofFormat::PbpStreaming:
    return CheckerRoute::Pbp;
  }
  return CheckerRoute::None;
}

CheckerNaming Proof::population_naming() const noexcept {
  const CheckerRoute route = population_route();
  if (route == CheckerRoute::Lrat)
    return CheckerNaming::Id;
  if (route == CheckerRoute::Dsr || route == CheckerRoute::Drat)
    return CheckerNaming::LiteralMultiset;
  return CheckerNaming::None;
}

void Proof::observe_checker(CheckerPopulationEventKind kind,
                            CheckerClauseRole role, uint64_t line_id,
                            uint64_t checker_id,
                            PopulationClauseView clause, bool proof_only,
                            uint64_t predecessor_id,
                            uint64_t transaction_id,
                            PopulationSourceClassification
                                source_classification) noexcept {
  if (!population_observer_)
    return;
  CheckerPopulationEvent event;
  event.kind = kind;
  event.role = role;
  event.route = population_route();
  event.naming = population_naming();
  event.line_id = line_id;
  event.checker_id = checker_id;
  event.predecessor_id = predecessor_id;
  event.transaction_id = transaction_id;
  event.proof_only = proof_only;
  if (kind == CheckerPopulationEventKind::InputSeed)
    source_classification =
        PopulationSourceClassification::PendingInputClassification;
  else if (!proof_only && clause.size() &&
           (kind == CheckerPopulationEventKind::Add ||
            kind == CheckerPopulationEventKind::StageAdd))
    source_classification = PopulationSourceClassification::Eligible;
  else
    source_classification = PopulationSourceClassification::Ineligible;
  event.source_classification = source_classification;
  event.clause = clause;
  population_observer_->observe(event);
}

void Proof::bind_population_observer(PopulationObserver *observer) noexcept {
  if (observer == population_observer_)
    return;
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
    event.route = population_route();
    event.naming = population_naming();
    population_observer_->observe(event);
  }
  population_observer_ = observer;
  if (population_observer_) {
    CheckerPopulationEvent event;
    event.kind = CheckerPopulationEventKind::ObserverBound;
    event.route = population_route();
    event.naming = population_naming();
    population_observer_->observe(event);
  }
}

bool Proof::settle_population_observer() noexcept {
  if (streaming_pbp() && pbp_output_)
    return false;
  flush();
  return true;
}

void Proof::observe_checker_id_rebind(uint64_t predecessor_id,
                                      uint64_t checker_id, const Lit *lits,
                                      uint32_t size) noexcept {
  observe_checker(CheckerPopulationEventKind::IdRebind,
                  CheckerClauseRole::Input, checker_id, checker_id,
                  internal_view(lits, size), false, predecessor_id);
}

void Proof::finish_input_transaction_impl() {
  observe_checker(CheckerPopulationEventKind::InputFinished,
                  CheckerClauseRole::Input, 0, 0, {});
  // A restated input whose raw original was never deleted leaves both
  // constraints live under one Tactix id; exact deletion cannot name either.
  if (pbp_pending_rebind_.id)
    abandon_pbp_id_deletions(5, 0);
  pbp_last_input_live_ = false;
  input_transaction_ = false;
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

Proof::~Proof() {
  // A run that never reached the empty clause keeps its PBP prefix.
  if (dsr_copy_)
    drop_dsr_copy(automatic_reason_);
  // An unfinished stream keeps every staged root unit (see add_rup).
  if (file_ && streaming_pbp() && !streaming_pbp_finished_)
    settle_pbp_deferred_units(false);
  flush();
  bind_population_observer(nullptr);
  if (file_ && std::fclose(file_) && !copy_role_)
    std::abort();
  if (pbp_output_ && std::fclose(pbp_output_))
    std::abort();
  if (pending_deletions_)
    std::fclose(pending_deletions_);
  stream_identity_=0;
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
bool Proof::open(const char *path, ProofFormat format) {
  stream_identity_=0; // Failed opens cannot lend cached-unit authority.
  if (file_ || pbp_output_ || format == ProofFormat::Pbp || dsr_copy_)
    return false;
  FILE *const file = fopen(path, "wb");
  return file && open_stream(file, format);
}

bool Proof::open_stream(FILE *file, ProofFormat format) {
  stream_identity_=0;
  if (file_ || pbp_output_ || format == ProofFormat::Pbp || !file) {
    if (file)
      std::fclose(file);
    return false;
  }
  file_ = file;
  format_ = format;
  has_sr_witness_ = false;
  streaming_pbp_finished_ = false;
  pbp_original_constraints_ = 0;
  pbp_derived_constraints_ = 0;
  pbp_constraint_ids_.clear();
  pbp_input_runs_.clear();
  pbp_input_by_spec_.clear();
  pbp_input_deleted_.clear();
  pbp_identity_prefix_ = 0;
  pbp_input_ids_diverged_ = false;
  pbp_id_deletions_enabled_ = true;
  pbp_anonymous_constraints_.clear();
  pbp_last_input_id_ = pbp_last_input_constraint_ = 0;
  pbp_last_input_live_ = false;
  pbp_pending_rebind_ = {};
  pbp_deferred_units_.clear();
  pbp_deferring_units_ = false;
  pbp_counters_ = {};
  pbp_last_deleted_ = 0;
  pbp_audit_input_.clear();
  pbp_audit_derived_.clear();
  if (file_ && format == ProofFormat::PbpStreaming) {
    pbp_output_ = file_;
    file_ = std::tmpfile();
    if (!file_) {
      std::fclose(pbp_output_);
      pbp_output_ = nullptr;
      return false;
    }
    input_transaction_ = true;
  }
  if(file_) {
    stream_identity_=next_proof_stream_identity();
    if(!stream_identity_) {
      std::fclose(file_);file_=nullptr;
      if(pbp_output_){std::fclose(pbp_output_);pbp_output_=nullptr;}
      input_transaction_=false;return false;
    }
  }
  if (file_)
    setvbuf(file_, nullptr, binary() ? _IONBF : _IOFBF, 1 << 20);
  if (file_) {
    observe_checker(CheckerPopulationEventKind::RouteOpened,
                    CheckerClauseRole::None, 0, 0, {});
    observe_checker(CheckerPopulationEventKind::RouteCommitted,
                    CheckerClauseRole::None, 0, 0, {});
  }
  return file_ != nullptr;
}

bool Proof::commit_clause_stream() {
  if (streaming_pbp() && pbp_output_) {
    publish_streaming_pbp_header();
    return true;
  }
  return streaming_pbp() || !pb();
}

FILE *Proof::begin_pb_impl() {
  if (!startup_pb_available())
    return nullptr;
  // The staged input transaction was never published. Its facts are justified
  // afresh from original CNF by the complete, prevalidated PB derivation.
  observe_checker(CheckerPopulationEventKind::ClausalRouteDiscarded,
                  CheckerClauseRole::None, 0, 0, {});
  if (pending_deletions_ && std::fclose(pending_deletions_))
    std::abort();
  pending_deletions_ = nullptr;
  pending_units_.clear();
  pending_unit_literals_.clear();
  pending_unit_ids_.clear();
  pending_max_external_ = 0;
  pending_deletion_bytes_.clear();
  spooling_deletions_ = false;
  // Staged root units belong to the discarded clausal stream.
  pbp_deferred_units_.clear();
  pbp_deferring_units_ = false;
  std::fclose(file_);
  binary_buffer_size_ = 0;
  file_ = pbp_output_;
  pbp_output_ = nullptr;
  format_ = ProofFormat::Pbp;
  observe_checker(CheckerPopulationEventKind::RouteCommitted,
                  CheckerClauseRole::None, 0, 0, {});
  return file_;
}

bool Proof::finish_pb() {
  return pb() && file_ && !std::ferror(file_) && std::fflush(file_) == 0;
}

void Proof::publish_streaming_pbp_header() {
  if (!streaming_pbp() || !pbp_output_)
    return;
  if (std::fflush(file_) || std::fseek(file_, 0, SEEK_SET))
    std::abort();
  if (std::fprintf(pbp_output_,
                   "pseudo-Boolean proof version 3.0\nf %" PRIu64 ";\n",
                   pbp_original_constraints_) < 0)
    std::abort();
  copy_stream(file_, pbp_output_);
  std::fclose(file_);
  file_ = pbp_output_;
  pbp_output_ = nullptr;
}

void Proof::finish_streaming_pbp_unsat() {
  if (!streaming_pbp() || streaming_pbp_finished_)
    return;
  std::fputs("output NONE ;\nconclusion UNSAT : -1 ;\n"
             "end pseudo-Boolean proof ;\n", file_);
  streaming_pbp_finished_ = true;
}

void Proof::put_byte(unsigned char byte) {
  binary_buffer_[binary_buffer_size_++] = byte;
  if (binary_buffer_size_ == binary_buffer_.size())
    flush_binary();
}

void Proof::flush_binary() {
  if (!file_ || !binary_buffer_size_)
    return;
  if (spooling_deletions_ && !pending_deletions_ &&
      pending_deletion_bytes_.size() + binary_buffer_size_ <= 65536) {
    pending_deletion_bytes_.insert(pending_deletion_bytes_.end(),
                                   binary_buffer_.data(),
                                   binary_buffer_.data() + binary_buffer_size_);
    binary_buffer_size_ = 0;
    return;
  }
  if (spooling_deletions_ && !pending_deletions_) {
    pending_deletions_ = std::tmpfile();
    if (!pending_deletions_ ||
        std::fwrite(pending_deletion_bytes_.data(), 1,
                    pending_deletion_bytes_.size(), pending_deletions_) !=
            pending_deletion_bytes_.size()) {
      if (!copy_role_ || !pending_deletions_)
        std::abort();
      copy_failed_ = true;
    }
    pending_deletion_bytes_.clear();
  }
  FILE *destination = spooling_deletions_ ? pending_deletions_ : file_;
  const size_t written =
      fwrite(binary_buffer_.data(), 1, binary_buffer_size_, destination);
  if (written != binary_buffer_size_) {
    if (!copy_role_)
      std::abort();
    copy_failed_ = true; // the owner discards this copy (settle_dsr_copy)
  }
  binary_buffer_size_ = 0;
}

// Binary DSR/DRAT literal encoding: map external literal e
// to unsigned u = (|e| << 1) | (e < 0), then 7-bit varint chunks,
// high bit = continuation.
void Proof::put_lit(Lit l) { put_external_lit(checker_lit(l)); }

void Proof::put_external_lit(Lit l) {
  put_external_dimacs(externalize(l));
}

void Proof::put_external_dimacs(int32_t e) {
  assert(e && e != INT32_MIN);
  if (binary()) {
    const uint32_t magnitude = external_variable_of(e);
    uint32_t u = (magnitude << 1) | (e < 0);
    while (u >= 128) {
      put_byte(static_cast<unsigned char>(128 | (u & 127)));
      u >>= 7;
    }
    put_byte(static_cast<unsigned char>(u));
  } else {
    fprintf(file_, "%d ", e);
  }
}

const int32_t *Proof::pbp_checker_literals(const Lit *lits, uint32_t size) {
  pbp_external_scratch_.clear();
  pbp_external_scratch_.reserve(size);
  for (uint32_t i = 0; i < size; ++i)
    pbp_external_scratch_.push_back(externalize(checker_lit(lits[i])));
  return pbp_external_scratch_.data();
}

void Proof::put_pbp_clause(const char *rule, const Lit *lits,
                           uint32_t size) {
  PbpTextWriter text(file_);
  text.put(rule);
  text.put(" ");
  for (uint32_t i = 0; i < size; ++i)
    put_pbp_literal(text, externalize(checker_lit(lits[i])));
  text.put(">= 1");
  text.flush();
}

void Proof::put_pbp_external_clause(const char *rule, const int32_t *lits,
                                    uint32_t size) {
  PbpTextWriter text(file_);
  text.put(rule);
  text.put(" ");
  for (uint32_t i = 0; i < size; ++i)
    put_pbp_literal(text, lits[i]);
  text.put(">= 1");
  text.flush();
}

namespace {
constexpr uint64_t PBP_DERIVED_ORDINAL = uint64_t{1} << 63;
}

void Proof::abandon_pbp_id_deletions(uint32_t reason, uint32_t width) {
  if (pbp_id_deletions_enabled_ && !pbp_counters_.abandon_reason) {
    pbp_counters_.abandon_reason = reason ? reason : 99;
    pbp_counters_.abandon_width = width;
    pbp_counters_.abandon_line = pbp_derived_constraints_;
  }
  pbp_id_deletions_enabled_ = false;
  // Every tracker returns early once disabled, so the map never refills.  A
  // repeated clear() would still zero the whole bucket array (sized by the
  // input census) on every later by-specification deletion.
  if (!pbp_constraint_ids_.empty())
    pbp_constraint_ids_.clear();
  if (!pbp_anonymous_constraints_.empty())
    pbp_anonymous_constraints_.clear();
  std::vector<PbpInputRun>().swap(pbp_input_runs_);
  std::vector<bool>().swap(pbp_input_by_spec_);
  std::vector<bool>().swap(pbp_input_deleted_);
  pbp_pending_rebind_ = {};
}

// The input constraint a raw input id names, deleted or not, and whether its
// deletion is exact (false for an ordinary binary of the divergent suffix).
bool Proof::pbp_input_constraint(uint64_t id, uint64_t &constraint,
                                 bool &exact) const {
  if (!id)
    return false;
  if (id <= pbp_identity_prefix_) {
    constraint = id;
    exact = true;
    return true;
  }
  const auto after = std::upper_bound(
      pbp_input_runs_.begin(), pbp_input_runs_.end(), id,
      [](uint64_t value, const PbpInputRun &run) {
        return value < run.first_id;
      });
  if (after == pbp_input_runs_.begin())
    return false;
  const PbpInputRun &run = after[-1];
  if (id - run.first_id >= run.count)
    return false;
  constraint = run.first_constraint + (id - run.first_id);
  exact = !(constraint < pbp_input_by_spec_.size() &&
            pbp_input_by_spec_[constraint]);
  return true;
}

bool Proof::pbp_input_deleted(uint64_t constraint) const {
  return constraint < pbp_input_deleted_.size() &&
         pbp_input_deleted_[constraint];
}

void Proof::mark_pbp_input_deleted(uint64_t constraint) {
  if (constraint >= pbp_input_deleted_.size())
    pbp_input_deleted_.resize(
        std::max<uint64_t>(constraint + 1, 2 * pbp_input_deleted_.size()));
  pbp_input_deleted_[constraint] = true;
}

// The ID the next derived constraint receives: absolute after input, or a
// transaction-relative ordinal while the `f` count is still unknown.
bool Proof::pbp_current_ordinal(uint64_t &constraint) const {
  if (input_transaction_) {
    if (pbp_derived_constraints_ >= PBP_DERIVED_ORDINAL)
      return false;
    constraint = PBP_DERIVED_ORDINAL | pbp_derived_constraints_;
    return true;
  }
  if (pbp_original_constraints_ > UINT64_MAX - pbp_derived_constraints_)
    return false;
  constraint = pbp_original_constraints_ + pbp_derived_constraints_;
  return true;
}

// A stored ID as VeriPB numbers it, once that number is known.
bool Proof::pbp_absolute_constraint(uint64_t stored,
                                    uint64_t &constraint) const {
  if (!stored)
    return false;
  if (!(stored & PBP_DERIVED_ORDINAL)) {
    constraint = stored;
    return true;
  }
  if (input_transaction_)
    return false;
  const uint64_t ordinal = stored & ~PBP_DERIVED_ORDINAL;
  if (pbp_original_constraints_ > UINT64_MAX - ordinal)
    return false;
  constraint = pbp_original_constraints_ + ordinal;
  return true;
}

void Proof::track_pbp_original_constraint(uint64_t id, uint32_t clause_size,
                                          bool repeated_literal) {
  if (!id || !pbp_id_deletions_enabled_)
    return;
  const uint64_t constraint = pbp_original_constraints_;
  if (!pbp_input_ids_diverged_ && id == constraint) {
    pbp_identity_prefix_ = id;
    return;
  }
  pbp_input_ids_diverged_ = true;
  // A divergent input suffix needs explicit internal-to-proof identities.
  // Raw input ids only increase, so consecutive ids extend one run.
  if (pbp_input_runs_.empty() ? id <= pbp_identity_prefix_
                              : id < pbp_input_runs_.back().first_id +
                                         pbp_input_runs_.back().count) {
    abandon_pbp_id_deletions(3, clause_size);
    return;
  }
  PbpInputRun *last = pbp_input_runs_.empty() ? nullptr
                                              : &pbp_input_runs_.back();
  if (last && id == last->first_id + last->count &&
      constraint == last->first_constraint + last->count)
    ++last->count;
  else
    pbp_input_runs_.push_back({id, constraint, 1});
  // Ordinary PBP still never references a binary identity: its deletion is
  // by specification, which keeps the binary archive out of the solver's hot
  // state. A binary that repeats its literal is the exception: it
  // canonicalizes to a unit, whose deletion is exact.
  if (clause_size == 2 && !repeated_literal) {
    if (constraint >= pbp_input_by_spec_.size())
      pbp_input_by_spec_.resize(
          std::max<uint64_t>(constraint + 1, 2 * pbp_input_by_spec_.size()));
    pbp_input_by_spec_[constraint] = true;
  }
}

void Proof::track_pbp_derived_constraint(uint64_t id, uint32_t clause_size) {
  // The ordinary PBP route deliberately does not retain binary proof IDs in
  // the solver. Avoid rebuilding the same large archive here: binary
  // deletions use `del spec`, while constraints of every other width continue
  // to use exact IDs. finish_input discards the binary-ID archive on every
  // streaming-PBP route, so no binary identity is ever exact.
  if (clause_size == 2)
    return;
  if (!id || !pbp_id_deletions_enabled_)
    return;
  // Input restatement: the replacement of the raw clause just seeded keeps
  // that clause's id while its raw original is still live, and the original
  // is deleted under the same id next (frontend/parse.cpp restate_input).
  // Record the replacement as pending; that deletion names the original's
  // exact constraint and then rebinds the id to the replacement. The
  // replacement consumes no Tactix id, so the identity prefix is unchanged.
  if (input_transaction_ && pbp_last_input_live_ &&
      id == pbp_last_input_id_) {
    uint64_t replacement = 0;
    if (pbp_pending_rebind_.id || !pbp_current_ordinal(replacement)) {
      abandon_pbp_id_deletions(4, clause_size);
      return;
    }
    pbp_pending_rebind_ = {id, pbp_last_input_constraint_, replacement};
    ++pbp_counters_.rebinds_recorded;
    return;
  }
  // A normalized replacement deliberately reuses its deleted input identity
  // to preserve the proofless trajectory.  A deleted input proves that
  // rebinding is injective; otherwise root-unit publication may be colliding
  // with a still-live input constraint and exact ID deletion is unsafe.
  uint64_t input = 0;
  bool exact = false;
  if (pbp_input_constraint(id, input, exact)) {
    const bool live = !pbp_input_deleted(input);
    if (id <= pbp_identity_prefix_ &&
        (live || pbp_constraint_ids_.count(id))) {
      abandon_pbp_id_deletions(2, clause_size);
      return;
    }
    if (id > pbp_identity_prefix_ && live && exact) {
      abandon_pbp_id_deletions(3, clause_size);
      return;
    }
  }
  uint64_t constraint = 0;
  if (input_transaction_)
    pbp_input_ids_diverged_ = true;
  if (!pbp_current_ordinal(constraint)) {
    abandon_pbp_id_deletions(6, clause_size);
    return;
  }
  const auto [_, inserted] = pbp_constraint_ids_.emplace(id, constraint);
  if (!inserted)
    abandon_pbp_id_deletions(3, clause_size);
}

size_t Proof::PbpLiteralKeyHash::operator()(
    const std::vector<int32_t> &key) const noexcept {
  uint64_t hash = 0x9e3779b97f4a7c15ull ^ key.size();
  for (const int32_t literal : key) {
    hash ^= static_cast<uint32_t>(literal);
    hash *= 0xff51afd7ed558ccdull;
    hash ^= hash >> 32;
  }
  return static_cast<size_t>(hash);
}

namespace {
// The exact registry key: sorted literals. Clauses with a repeated or
// complementary literal have no unique VeriPB constraint and get no key.
bool pbp_literal_key(const int32_t *lits, uint32_t size,
                     std::vector<int32_t> &key) {
  key.assign(lits, lits + size);
  std::sort(key.begin(), key.end());
  for (uint32_t i = 0; i < size; ++i) {
    if (!key[i] || key[i] == INT32_MIN)
      return false;
    if (i && (key[i] == key[i - 1] || key[i] == -key[i - 1]))
      return false;
  }
  // Complements need not be adjacent in signed order.
  for (uint32_t i = 0; i < size; ++i)
    if (key[i] < 0 && std::binary_search(key.begin(), key.end(), -key[i]))
      return false;
  return true;
}
} // namespace

// Remember an anonymous constraint just counted as the newest derived one.
void Proof::register_pbp_anonymous(const int32_t *lits, uint32_t size) {
  if (!size || !lits || !pbp_id_deletions_enabled_ || size == 2)
    return;
  uint64_t constraint = 0;
  if (!pbp_current_ordinal(constraint) ||
      !pbp_literal_key(lits, size, pbp_key_scratch_))
    return; // a later deletion falls back to the specification
  pbp_anonymous_constraints_[pbp_key_scratch_].push_back(constraint);
  ++pbp_counters_.anonymous_registered;
}

// Count one derived streaming-PBP constraint and track its identity.
template <class External>
void Proof::note_pbp_derived(uint64_t id, uint32_t clause_size,
                             External external) {
  if (pbp_derived_constraints_ == UINT64_MAX)
    std::abort();
  ++pbp_derived_constraints_;
#ifndef NDEBUG
  pbp_audit_record(false, pbp_derived_constraints_, external(), clause_size);
#endif
  if (id)
    track_pbp_derived_constraint(id, clause_size);
  else if (clause_size && clause_size != 2 && pbp_id_deletions_enabled_)
    register_pbp_anonymous(external(), clause_size);
}

bool Proof::put_pbp_deletion_id(uint64_t id) {
  if (!id || !pbp_id_deletions_enabled_)
    return false;
  uint64_t constraint = 0;
  const auto found = pbp_constraint_ids_.find(id);
  if (found != pbp_constraint_ids_.end()) {
    if (!pbp_absolute_constraint(found->second, constraint))
      return false;
    pbp_constraint_ids_.erase(found);
  } else {
    // A deleted input stays deleted, so a repeated deletion of its id can
    // never resolve to it again.
    bool exact = false;
    if (!pbp_input_constraint(id, constraint, exact) || !exact ||
        pbp_input_deleted(constraint))
      return false;
    mark_pbp_input_deleted(constraint);
  }
  std::fprintf(file_, "del id %" PRIu64 " ;\n", constraint);
  pbp_last_deleted_ = constraint;
  ++pbp_counters_.by_id;
  return true;
}

// Delete the raw original of a pending restatement by its exact input
// constraint, then let its id name the replacement.
bool Proof::put_pbp_rebind_deletion() {
  const PbpPendingRebind rebind = pbp_pending_rebind_;
  pbp_pending_rebind_ = {};
  pbp_last_input_live_ = false;
  pbp_constraint_ids_.insert_or_assign(rebind.id, rebind.replacement);
  mark_pbp_input_deleted(rebind.original);
  std::fprintf(file_, "del id %" PRIu64 " ;\n", rebind.original);
  pbp_last_deleted_ = rebind.original;
  ++pbp_counters_.by_id;
  ++pbp_counters_.by_rebind;
  return true;
}

// `canonical` is the VeriPB-canonical clause of a deletion whose own id named
// nothing. Delete the newest anonymous constraint with exactly these literals.
bool Proof::put_pbp_anonymous_deletion(const int32_t *canonical,
                                       uint32_t size) {
  if (pbp_anonymous_constraints_.empty() || !pbp_id_deletions_enabled_ ||
      !pbp_literal_key(canonical, size, pbp_key_scratch_))
    return false;
  const auto found = pbp_anonymous_constraints_.find(pbp_key_scratch_);
  if (found == pbp_anonymous_constraints_.end())
    return false;
  uint64_t constraint = 0;
  if (!pbp_absolute_constraint(found->second.back(), constraint))
    return false;
  found->second.pop_back();
  if (found->second.empty())
    pbp_anonymous_constraints_.erase(found);
  std::fprintf(file_, "del id %" PRIu64 " ;\n", constraint);
  pbp_last_deleted_ = constraint;
  ++pbp_counters_.by_id;
  ++pbp_counters_.by_anonymous;
  return true;
}

// VeriPB's CNF reader drops tautological clauses and canonicalizes repeated
// literals before populating the initial constraint database.  A `del spec`
// must name that canonical constraint, rather than the raw DIMACS spelling
// retained by Tactix for input-proof accounting.
//
// Literal positions are grouped by variable, earliest first: a group with both
// signs is a tautology, and only the first position of each group is kept.
// The kept literals stay in input order.  Time is O(k log k) and scratch
// memory O(k) in the clause length k.
static bool canonical_pbp_deletion(const int32_t *lits, uint32_t size,
                                   std::vector<int32_t> &canonical,
                                   std::vector<uint32_t> &order,
                                   std::vector<uint8_t> &keep) {
  canonical.clear();
  order.resize(size);
  keep.assign(size, 0);
  for (uint32_t i = 0; i < size; ++i) {
    if (!lits[i] || lits[i] == INT32_MIN)
      return false;
    order[i] = i;
  }
  const auto variable = [&](uint32_t at) {
    return static_cast<uint32_t>(lits[at] > 0 ? lits[at] : -lits[at]);
  };
  std::sort(order.begin(), order.end(), [&](uint32_t left, uint32_t right) {
    const uint32_t lv = variable(left), rv = variable(right);
    return lv != rv ? lv < rv : left < right;
  });
  for (uint32_t i = 0; i < size; ++i) {
    if (i && variable(order[i]) == variable(order[i - 1])) {
      if (lits[order[i]] != lits[order[i - 1]])
        return false;
      continue;
    }
    keep[order[i]] = 1;
  }
  canonical.reserve(size);
  for (uint32_t i = 0; i < size; ++i)
    if (keep[i])
      canonical.push_back(lits[i]);
  return true;
}

// One streaming-PBP deletion: the exact tracked identity when available,
// otherwise `del spec` of the canonical constraint.  `external()` yields the
// clause's external literals and runs only on that fallback.  False means a
// tautological specification for which nothing was written.
//
// Exact identities are tried in order: a pending input restatement, the
// tracked Tactix id, then an anonymous constraint with exactly the canonical
// literals. Only when all fail does an exact-width deletion write `del spec`
// and permanently abandon ID deletion. A binary whose two literals coincide
// canonicalizes to a unit, so it is an exact-width deletion as well.
template <class External>
bool Proof::put_pbp_deletion(uint64_t del_id, uint32_t size,
                             External external) {
  const int32_t *lits = nullptr;
#ifndef NDEBUG
  lits = external();
  const auto audited = [&] {
    pbp_audit_deletion(pbp_last_deleted_, lits, size);
    return true;
  };
#else
  const auto audited = [] { return true; };
#endif
  if (del_id && del_id == pbp_pending_rebind_.id)
    return put_pbp_rebind_deletion() && audited();
  if (del_id && del_id == pbp_last_input_id_)
    pbp_last_input_live_ = false;
  bool exact_identity = size != 2;
  if (!exact_identity) {
    if (!lits)
      lits = external();
    exact_identity = lits[0] == lits[1];
  }
  if (exact_identity && put_pbp_deletion_id(del_id))
    return audited();
  if (!lits)
    lits = external();
  if (!canonical_pbp_deletion(lits, size, pbp_canonical_scratch_,
                              pbp_canonical_order_, pbp_canonical_keep_))
    return false;
  const uint32_t width = static_cast<uint32_t>(pbp_canonical_scratch_.size());
  if (exact_identity) {
    if (!pbp_replaying_leased_ &&
        put_pbp_anonymous_deletion(pbp_canonical_scratch_.data(), width))
      return audited();
    abandon_pbp_id_deletions(1, width);
  }
  put_pbp_external_clause("del spec", pbp_canonical_scratch_.data(), width);
  std::fputs(" ;\n", file_);
  ++(width == 1 ? pbp_counters_.spec_unit
     : width == 2 ? pbp_counters_.spec_binary
                  : pbp_counters_.spec_wide);
  return true;
}

#ifndef NDEBUG
namespace {
// Sorted VeriPB-canonical literals; {0} marks a tautology.
std::vector<int32_t> pbp_audit_key(const int32_t *lits, uint32_t size) {
  std::vector<int32_t> key(lits, lits + size);
  std::sort(key.begin(), key.end());
  key.erase(std::unique(key.begin(), key.end()), key.end());
  for (const int32_t literal : key)
    if (literal < 0 && std::binary_search(key.begin(), key.end(), -literal))
      return {0};
  return key;
}
} // namespace

void Proof::pbp_audit_record(bool input, uint64_t index, const int32_t *lits,
                             uint32_t size) {
  (input ? pbp_audit_input_ : pbp_audit_derived_)[index] =
      pbp_audit_key(lits, size);
}

void Proof::pbp_audit_deletion(uint64_t constraint, const int32_t *lits,
                               uint32_t size) {
  const bool input = constraint <= pbp_original_constraints_;
  auto &audit = input ? pbp_audit_input_ : pbp_audit_derived_;
  const uint64_t index =
      input ? constraint : constraint - pbp_original_constraints_;
  const auto found = audit.find(index);
  if (found == audit.end()) {
    std::fprintf(stderr,
                 "PBP-AUDIT: del id %" PRIu64 " names no live %s constraint\n",
                 constraint, input ? "input" : "derived");
    std::abort();
  }
  if (found->second != pbp_audit_key(lits, size)) {
    std::fprintf(stderr, "PBP-AUDIT: del id %" PRIu64
                         " literal mismatch (width %u vs %zu)\n",
                 constraint, size, found->second.size());
    std::abort();
  }
  audit.erase(found);
}
#endif

void Proof::begin_deferred_root_units() noexcept {
  pbp_deferring_units_ = streaming_pbp() && file_ && !streaming_pbp_finished_;
}

void Proof::end_deferred_root_units() noexcept {
  if (pbp_deferring_units_ && !pbp_deferred_units_.empty())
    ++pbp_counters_.deferred_batches;
  pbp_deferring_units_ = false;
}

// Write (or, before the empty clause, drop) every staged root unit.
void Proof::settle_pbp_deferred_units(bool drop) {
  if (pbp_deferred_units_.empty())
    return;
  std::vector<PbpDeferredUnit> units = std::move(pbp_deferred_units_);
  pbp_deferred_units_.clear();
  if (drop) {
    ++pbp_counters_.dropped_batches;
    pbp_counters_.dropped_units += units.size();
    return;
  }
  const CheckerClauseRole saved = population_role_;
  population_role_ = CheckerClauseRole::DerivedUnit;
  for (const PbpDeferredUnit &unit : units)
    add_rup_impl(unit.id, &unit.lit, 1, nullptr, 0);
  population_role_ = saved;
}

void Proof::put_u64(uint64_t v) { fprintf(file_, "%" PRIu64 " ", v); }

void Proof::end_line() {
  if (binary())
    put_byte(0);
  else
    fputs("0\n", file_);
}

void Proof::put_line_tag(bool deletion) {
  if (format_ == ProofFormat::DsrBinary)
    put_byte(deletion ? 2 : 1);
  else if (format_ == ProofFormat::DratBinary)
    put_byte(deletion ? 'd' : 'a');
  else if (deletion) {
    fputc('d', file_);
    if (format_ == ProofFormat::DsrAscii || format_ == ProofFormat::DratAscii)
      fputc(' ', file_);
  }
}

void Proof::put_clausal_line(bool deletion, const Lit *lits, uint32_t size) {
  put_line_tag(deletion);
  for (uint32_t i = 0; i < size; i++)
    put_lit(lits[i]);
  end_line();
}

uint32_t Proof::external_variable_bound() const {
  uint32_t bound = std::max(pending_max_external_, proof_external_max_);
  if (external_variable_map_)
    bound = std::max(bound,
                     static_cast<uint32_t>(external_variable_map_->size()));
  else if (variable_map_)
    for (const Var external : *variable_map_)
      bound = std::max(bound, external + 1);
  return bound;
}

void Proof::flush_pending_dsr_units() {
  if (pending_units_.empty())
    return;
  assert(file_ && dsr());
  if (!pending_population_transaction_)
    pending_population_transaction_ = ++next_population_transaction_;
  observe_checker(CheckerPopulationEventKind::PendingCommitBegin,
                  CheckerClauseRole::None, 0, 0, {}, false, 0,
                  pending_population_transaction_);
  const bool split = (pending_deletions_ || !pending_deletion_bytes_.empty()) &&
                     external_variable_bound() < static_cast<uint32_t>(INT32_MAX);
  const int32_t auxiliary = split ?
      static_cast<int32_t>(external_variable_bound() + 1) : 0;
  const auto emit = [&](int32_t unit, bool deletion, int32_t companion) {
    put_line_tag(deletion);
    put_external_dimacs(unit);
    if (companion)
      put_external_dimacs(companion);
    end_line();
  };
  // Both split helpers of one unit: the positive auxiliary, then the negative.
  const auto split_helpers = [&](const PendingDsrUnit &unit, bool deletion) {
    for (const int32_t companion : {auxiliary, -auxiliary}) {
      emit(unit.external, deletion, companion);
      const int32_t helper[2] = {unit.external, companion};
      observe_checker(deletion ? CheckerPopulationEventKind::DeleteRequested
                               : CheckerPopulationEventKind::Add,
                      CheckerClauseRole::DsrSplitHelper, 0, 0,
                      PopulationClauseView::external_lits(helper, 2), true,
                      0, pending_population_transaction_);
    }
  };
  const auto direct_unit = [&](const PendingDsrUnit &unit) {
    emit(unit.external, false, 0);
    observe_checker(CheckerPopulationEventKind::Add, unit.role, unit.id,
                    unit.id,
                    PopulationClauseView::external_lits(&unit.external, 1),
                    false, 0, pending_population_transaction_);
  };
  if (split) {
    // Under either assumption on the shared auxiliary, every earlier pair
    // propagates its unit. Thus both helpers for the next RUP unit see the
    // same preceding units as the original stream. The pair also makes each
    // unit RUP after the deferred deletions, under an assumption of its negation.
    for (const auto &unit : pending_units_)
      split_helpers(unit, false);
  } else {
    // No deletion needs moving, or the external literal representation has
    // no spare identity; preserve the original direct-unit serialization.
    for (const auto &unit : pending_units_)
      direct_unit(unit);
  }
  if (pending_deletions_ || !pending_deletion_bytes_.empty()) {
    // Keep short binary blocks in the existing 1 MiB stream buffer. Only a
    // spilled block needs the main stream flushed before copying its file.
    for (const unsigned char byte : pending_deletion_bytes_)
      put_byte(byte);
    pending_deletion_bytes_.clear();
    if (pending_deletions_) {
      flush_binary();
      if (std::fflush(pending_deletions_) ||
          std::fseek(pending_deletions_, 0, SEEK_SET))
        std::abort();
      copy_stream(pending_deletions_, file_);
      if (std::fclose(pending_deletions_))
        std::abort();
      pending_deletions_ = nullptr;
    }
    observe_checker(CheckerPopulationEventKind::PendingDeletesCommit,
                    CheckerClauseRole::None, 0, 0, {}, false, 0,
                    pending_population_transaction_);
  }
  if (split) {
    for (const auto &unit : pending_units_)
      direct_unit(unit);
    for (const auto &unit : pending_units_)
      split_helpers(unit, true);
  }
  observe_checker(CheckerPopulationEventKind::PendingCommitEnd,
                  CheckerClauseRole::None, 0, 0, {}, false, 0,
                  pending_population_transaction_);
  pending_units_.clear();
  pending_unit_literals_.clear();
  pending_unit_ids_.clear();
  pending_max_external_ = 0;
  pending_population_transaction_ = 0;
}

// Duplicate/complementary-literal scan. Small clauses only reach here, so the
// quadratic form beats any scratch allocation; it runs in debug builds only.
bool Proof::tautological(const Lit *lits, uint32_t size) {
  for (uint32_t i = 0; i < size; i++)
    for (uint32_t j = i + 1; j < size; j++)
      if (lits[i] == lits[j] || lits[i] == negate(lits[j]))
        return true;
  return false;
}

void Proof::add_rup_impl(uint64_t id, const Lit *lits, uint32_t size,
                         const uint64_t *chain, uint32_t nchain) {
  if (!pbp_deferred_units_.empty())
    settle_pbp_deferred_units(!size && streaming_pbp());
  const CheckerClauseRole role = population_role_;
  if (!input_transaction_ && !commit_clause_stream())
    std::abort(); // a terminal PB stream cannot accept a clausal suffix
  // A new unit extends a consecutive batch. A deletion block or a nonunit
  // addition ends it, so later RUP checks still see the earlier units.
  if (!dsr() || size != 1 || pending_deletions_ ||
      !pending_deletion_bytes_.empty())
    flush_pending_dsr_units();
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  if (!copy_role_) // the automatic DSR copy repeats an already noted event
    rule_journal_note_rup(id, chain, nchain);
#endif
  assert(!tautological(lits, size)); // see Proof::tautological
  // Under LRAT a non-empty clause with NO antecedents is uncheckable: the
  // checker has nothing to propagate and rejects the line (`<id> <lits> 0 0`).
  // Rules must skip the inference instead of emitting one; a trivially-true
  // lemma whose replay produces an empty chain is the way this happens.
  assert(!(format_ == ProofFormat::Lrat && size > 0 && nchain == 0));
#ifndef NDEBUG
  // 0 is LRAT's line terminator, never an antecedent, so a zero hint TRUNCATES
  // the line instead of corrupting it — the checker sees a shorter chain and
  // rejects a lemma that the emitter believed it had justified. Caught here
  // because the hand-built chains (Congruence's RUP replay, Substitute's
  // rewrite, Sweep's general path) never pass through `kernel_lrat_chain`,
  // whose own assert covers the reason-walk chains.
  if (format_ == ProofFormat::Lrat)
    for (uint32_t i = 0; i < nchain; i++)
      assert(chain[i] != 0 && "LRAT RUP chain contains the line terminator");
#endif
  if (!file_)
    return;
  // A native-DSR unit is staged into the pending batch instead of written.
  CheckerPopulationEventKind kind = CheckerPopulationEventKind::Add;
  uint64_t transaction = 0;
  if (streaming_pbp()) {
    if (streaming_pbp_finished_)
      std::abort();
    put_pbp_clause("rup", lits, size);
    std::fputs(" ;\n", file_);
    note_pbp_derived(id, size,
                     [&] { return pbp_checker_literals(lits, size); });
  } else if (dsr() && size == 1) {
    const int32_t unit = externalize(checker_lit(lits[0]));
    pending_units_.push_back({unit, id, role});
    pending_unit_literals_.insert(unit);
    if (id)
      pending_unit_ids_.insert(id);
    pending_max_external_ =
        std::max(pending_max_external_, external_variable_of(unit));
    if (!pending_population_transaction_)
      pending_population_transaction_ = ++next_population_transaction_;
    kind = CheckerPopulationEventKind::StageAdd;
    transaction = pending_population_transaction_;
  } else if (format_ == ProofFormat::Lrat) {
    put_u64(id);
    for (uint32_t i = 0; i < size; i++)
      put_lit(lits[i]);
    fputs("0 ", file_);
    for (uint32_t i = 0; i < nchain; i++)
      put_u64(chain[i]);
    fputs("0\n", file_);
  } else {
    put_clausal_line(false, lits, size);
  }
  population_add(id, lits, size);
  observe_checker(kind, role, id, id, internal_view(lits, size), false, 0,
                  transaction);
  if (streaming_pbp() && !size)
    finish_streaming_pbp_unsat();
}

void Proof::add_rat_impl(uint64_t id, const Lit *lits, uint32_t size,
                         const int64_t *hints, uint32_t nhints) {
  if (!pbp_deferred_units_.empty())
    settle_pbp_deferred_units(false);
  if (size && !premise_groups_.empty()) {
    invalidate_premises_for_sr(id, lits, 1, nullptr, 0, nullptr, 0);
    flush_persistent_external(id);
  }
  if (!input_transaction_ && !commit_clause_stream())
    std::abort();
  flush_pending_dsr_units();
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  if (!copy_role_)
    rule_journal_note_rat(id, hints, nhints);
#endif
  assert(!tautological(lits, size)); // see Proof::tautological
#ifndef NDEBUG
  if (format_ == ProofFormat::Lrat) // same terminator hazard as add_rup
    for (uint32_t i = 0; i < nhints; i++)
      assert(hints[i] != 0 && "LRAT RAT hints contain the line terminator");
#endif
  if (!file_)
    return;
  if (streaming_pbp()) {
    if (!size || streaming_pbp_finished_)
      std::abort();
    put_pbp_clause("red", lits, size);
    const int32_t external = externalize(checker_lit(lits[0]));
    std::fprintf(file_, " : x%" PRIu32 " -> %d ;\n",
                 external_variable_of(external), external < 0 ? 0 : 1);
    note_pbp_derived(id, size,
                     [&] { return pbp_checker_literals(lits, size); });
  } else if (format_ == ProofFormat::Lrat) {
    put_u64(id);
    for (uint32_t i = 0; i < size; i++)
      put_lit(lits[i]);
    fputs("0 ", file_);
    for (uint32_t i = 0; i < nhints; i++)
      fprintf(file_, "%" PRId64 " ", hints[i]);
    fputs("0\n", file_);
  } else {
    // DSR/DRAT: a witnessless addition with the pivot ordered first. The
    // checker first tries RUP and then infers first-literal RAT. DRAT output
    // is unchanged.
    put_clausal_line(false, lits, size);
  }
  population_add(id, lits, size);
  observe_checker(CheckerPopulationEventKind::Add, CheckerClauseRole::Rat,
                  id, id, internal_view(lits, size));
}

bool Proof::add_sr_impl(uint64_t id, const Lit *lits, uint32_t size,
                        const Lit *true_lits, uint32_t ntrue,
                        const DsrLiteralMapping *mappings,
                        uint32_t nmappings, uint32_t external_variables,
                        bool propagates) {
  if (!pbp_deferred_units_.empty())
    settle_pbp_deferred_units(false);
  if (!input_transaction_ && !commit_clause_stream())
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

  if (!file_)
    return true;
  if (streaming_pbp()) {
    if (streaming_pbp_finished_)
      return false;
    put_pbp_clause("red", lits, size);
    std::fputs(" :", file_);
    const auto external_lit = [&](Lit literal) {
      return externalize(checker_lit(literal));
    };
    const Var pivot_var = var_of(lits[0]);
    bool pivot_assigned = false;
    for (uint32_t i = 0; i < ntrue; ++i)
      pivot_assigned |= var_of(true_lits[i]) == pivot_var;
    for (uint32_t i = 0; i < nmappings; ++i)
      pivot_assigned |= var_of(mappings[i].from) == pivot_var;
    const auto put_constant = [&](Lit source, bool value) {
      std::fprintf(file_, " x%" PRIu32 " -> %d",
                   external_variable_of(external_lit(source)), value ? 1 : 0);
    };
    if (!pivot_assigned)
      put_constant(lits[0], !negated(lits[0]));
    for (uint32_t i = 0; i < ntrue; ++i)
      put_constant(true_lits[i], !negated(true_lits[i]));
    for (uint32_t i = 0; i < nmappings; ++i) {
      const int32_t from = external_lit(mappings[i].from);
      int32_t to = external_lit(mappings[i].to);
      if (from < 0)
        to = -to;
      std::fprintf(file_, " x%" PRIu32 " -> %sx%" PRIu32,
                   external_variable_of(from), to < 0 ? "~" : "",
                   external_variable_of(to));
    }
    std::fputs(" ;\n", file_);
    note_pbp_derived(id, size,
                     [&] { return pbp_checker_literals(lits, size); });
    population_add(id, lits, size);
    observe_checker(CheckerPopulationEventKind::Add,
                    CheckerClauseRole::SubstitutionRedundancy, id, id,
                    internal_view(lits, size));
    has_sr_witness_ = true;
    return true;
  }
  if (!dsr())
    return false;

  const DsrWitnessPlan plan =
      plan_dsr_witness(lits, size, true_lits, ntrue, mappings, nmappings);
  // A pivot without an explicit mapping has the default image true; the
  // general encoding moves the pivot to the auxiliary, so it states that image.
  const bool default_pivot = plan.form == DsrWitnessForm::Direct;
  const bool needs_auxiliary = plan.form == DsrWitnessForm::Extension ||
                               (propagates && plan.form != DsrWitnessForm::Rup);
  if (needs_auxiliary &&
      external_variables >= static_cast<uint32_t>(INT32_MAX))
    return false;
  // The authenticated Rup plan emits no witness. Its metadata does not
  // transform checker clauses, and retained constraints only strengthen RUP.
  if (plan.form != DsrWitnessForm::Rup)
    invalidate_premises_for_sr(id, lits, size, true_lits, ntrue, mappings, nmappings);
  flush_persistent_external(id);
  flush_pending_dsr_units();
  if (needs_auxiliary) {
    const Lit auxiliary = make_lit(external_variables, false);
    const int32_t external_auxiliary = externalize(auxiliary);
    const auto temporary = [&](bool deletion, bool negative) {
      put_line_tag(deletion);
      put_external_lit(negative ? negate(auxiliary) : auxiliary);
      for (uint32_t i = 0; i < size; ++i)
        put_lit(lits[i]);
    };
    // The first clause is RAT on a fresh variable. Under the negation of
    // the second clause, the original X7 obligations hold; the first clause's
    // image is exactly the already-validated candidate image C|sigma.
    temporary(false, true);
    end_line();
    PopulationClauseView negative_helper = internal_view(lits, size);
    negative_helper.prefix = -external_auxiliary;
    observe_checker(CheckerPopulationEventKind::Add,
                    CheckerClauseRole::DsrSubstitutionHelper, 0, 0,
                    negative_helper, true);
    temporary(false, false);
    put_external_lit(auxiliary);
    if (default_pivot)
      put_lit(lits[0]);
    for (uint32_t i = 0; i < ntrue; ++i)
      put_lit(true_lits[i]);
    if (nmappings) {
      put_external_lit(auxiliary);
      for (uint32_t i = 0; i < nmappings; ++i) {
        put_lit(mappings[i].from);
        put_lit(mappings[i].to);
      }
    }
    end_line();
    PopulationClauseView positive_helper = internal_view(lits, size);
    positive_helper.prefix = external_auxiliary;
    observe_checker(CheckerPopulationEventKind::Add,
                    CheckerClauseRole::DsrSubstitutionHelper, 0, 0,
                    positive_helper, true);
    put_clausal_line(false, lits, size);
    observe_checker(CheckerPopulationEventKind::Add,
                    CheckerClauseRole::SubstitutionRedundancy, id, id,
                    internal_view(lits, size));
    // These are nonunit clauses, so the checkers honour both deletions.
    // No live clause retains the auxiliary identity after this transaction.
    temporary(true, false);
    end_line();
    observe_checker(CheckerPopulationEventKind::DeleteRequested,
                    CheckerClauseRole::DsrSubstitutionHelper, 0, 0,
                    positive_helper, true);
    temporary(true, true);
    end_line();
    observe_checker(CheckerPopulationEventKind::DeleteRequested,
                    CheckerClauseRole::DsrSubstitutionHelper, 0, 0,
                    negative_helper, true);
    population_add(id, lits, size);
    has_sr_witness_ = true;
    return true;
  }
  const Lit emitted_pivot = lits[plan.pivot];
  PopulationClauseView emitted_clause = internal_view(lits, size);
  emitted_clause.first_body_index = plan.pivot;
  // The pivot-first clause, optionally extended by the plan's lemma literal.
  const auto put_emitted = [&](bool deletion, bool lemma) {
    put_line_tag(deletion);
    put_lit(emitted_pivot);
    for (uint32_t i = 0; i < size; ++i)
      if (i != plan.pivot)
        put_lit(lits[i]);
    if (lemma)
      put_lit(plan.lemma);
  };

  if (plan.form == DsrWitnessForm::Lemma) {
    put_emitted(false, true);
    end_line();
    PopulationClauseView lemma = emitted_clause;
    lemma.suffix = externalize(checker_lit(plan.lemma));
    observe_checker(CheckerPopulationEventKind::Add,
                    CheckerClauseRole::DsrSubstitutionHelper, 0, 0, lemma,
                    true);
  }
  put_emitted(false, false);
  if (plan.form != DsrWitnessForm::Rup) {
    const Var dropped = plan.form == DsrWitnessForm::Direct
                            ? INVALID_VAR
                            : var_of(emitted_pivot);
    uint32_t kept_mappings = 0;
    for (uint32_t i = 0; i < nmappings; ++i)
      kept_mappings += var_of(mappings[i].from) != dropped;
    put_lit(emitted_pivot); // begin witness; the pivot is mapped true
    for (uint32_t i = 0; i < ntrue; ++i)
      if (var_of(true_lits[i]) != dropped)
        put_lit(true_lits[i]);
    if (kept_mappings) {
      put_lit(emitted_pivot); // separate true literals from literal mappings
      for (uint32_t i = 0; i < nmappings; ++i)
        if (var_of(mappings[i].from) != dropped) {
          put_lit(mappings[i].from);
          put_lit(mappings[i].to);
        }
    }
  }
  end_line();
  observe_checker(CheckerPopulationEventKind::Add,
                  plan.form == DsrWitnessForm::Rup ? CheckerClauseRole::Rup :
                                                   CheckerClauseRole::SubstitutionRedundancy,
                  id, id, emitted_clause);
  if (plan.form != DsrWitnessForm::Rup)
    has_sr_witness_ = true;
  if (plan.form == DsrWitnessForm::Lemma) {
    put_emitted(true, true);
    end_line();
    PopulationClauseView lemma = emitted_clause;
    lemma.suffix = externalize(checker_lit(plan.lemma));
    observe_checker(CheckerPopulationEventKind::DeleteRequested,
                    CheckerClauseRole::DsrSubstitutionHelper, 0, 0, lemma,
                    true);
  }
  population_add(id, lits, size);
  return true;
}

void Proof::add_empty_impl(uint64_t id, const uint64_t *chain,
                           uint32_t nchain) {
  const CheckerClauseRole saved = population_role_;
  population_role_ = CheckerClauseRole::Empty;
  add_rup_impl(id, nullptr, 0, chain, nchain);
  population_role_ = saved;
}

void Proof::add_derived_unit_impl(uint64_t id, Lit unit,
                                  const uint64_t *chain, uint32_t nchain) {
  if (!file_)
    return;
  // LRAT and streaming PBP cite the unit's chain. Native DSR keeps root units
  // explicit so they remain available after a reason clause is deleted and
  // later direct-SR checks see the same root formula as dsr-trim/Trestle.
  // DRAT retains its historical implicit units.
  const bool chained = format_ == ProofFormat::Lrat || streaming_pbp();
  if (!chained && !dsr())
    return;
  if (pbp_deferring_units_ && streaming_pbp()) {
    pbp_deferred_units_.push_back({id, unit});
    ++pbp_counters_.deferred_units;
    return;
  }
  const CheckerClauseRole saved = population_role_;
  population_role_ = CheckerClauseRole::DerivedUnit;
  add_rup_impl(id, &unit, 1, chained ? chain : nullptr, chained ? nchain : 0);
  population_role_ = saved;
}

// While a staged DSR-unit batch is open, the next deletion line is spooled
// (binary formats) or written to the pending temporary file (text formats);
// end_pending_deletion restores the main stream. Returns that main stream.
FILE *Proof::begin_pending_deletion() {
  flush_binary();
  if (binary()) {
    spooling_deletions_ = true;
    return nullptr;
  }
  if (!pending_deletions_ && !(pending_deletions_ = std::tmpfile()))
    std::abort();
  FILE *const main_file = file_;
  file_ = pending_deletions_;
  return main_file;
}

void Proof::end_pending_deletion(FILE *main_file) {
  flush_binary();
  if (binary())
    spooling_deletions_ = false;
  else
    file_ = main_file;
}

void Proof::del_impl(uint64_t line_id, uint64_t del_id, const Lit *lits,
                     uint32_t size) {
  if (!pbp_deferred_units_.empty())
    settle_pbp_deferred_units(false);
  if (!leased_premises_.empty() && size <= premise_maximum_width_) {
    std::vector<int32_t> external; external.reserve(size);
    for (uint32_t i = 0; i < size; ++i)
      external.push_back(externalize(checker_lit(lits[i])));
    if (defer_leased_deletion(external.data(), size)) return;
  }
  if (!input_transaction_ && !commit_clause_stream())
    std::abort();
  if (!file_)
    return;
  if (streaming_pbp()) {
    // RUP is monotone under retained constraints, but RAT/SR redundancy is
    // not. Prefer the exact tracked PBP identity. Ordinary binary clauses do
    // not retain their IDs and are always deleted by specification; this
    // cannot drift a later exact identity because constraints of another
    // width are syntactically distinct. Any other untracked deletion still
    // permanently abandons ID deletion so duplicate selection cannot drift.
    if (streaming_pbp_finished_ ||
        !put_pbp_deletion(del_id, size,
                          [&] { return pbp_checker_literals(lits, size); }))
      return;
    // Both PBP deletion forms report the same checker-population request.
    population_del(del_id, lits, size);
    observe_checker(CheckerPopulationEventKind::DeleteRequested,
                    CheckerClauseRole::None, line_id, del_id,
                    internal_view(lits, size));
    return;
  }
  const Lit external_unit = size == 1 ? checker_lit(lits[0]) : INVALID_LIT;
  if ((del_id && pending_unit_ids_.count(del_id)) ||
      (size == 1 && pending_unit_literals_.count(externalize(external_unit))))
    flush_pending_dsr_units();
  const bool defer = !pending_units_.empty();
  FILE *const main_file = defer ? begin_pending_deletion() : nullptr;
  if (defer)
    for (uint32_t i = 0; i < size; ++i) {
      const Var external = var_of(checker_lit(lits[i]));
      pending_max_external_ = std::max(pending_max_external_, external + 1);
    }
  if (format_ == ProofFormat::Lrat) {
    put_u64(line_id);
    fputs("d ", file_);
    put_u64(del_id);
    fputs("0\n", file_);
    population_del(del_id, lits, size);
    observe_checker(CheckerPopulationEventKind::Delete,
                    CheckerClauseRole::None, line_id, del_id,
                    internal_view(lits, size));
    return;
  }
  put_clausal_line(true, lits, size);
  if (defer)
    end_pending_deletion(main_file);
  population_del(del_id, lits, size);
  const CheckerPopulationEventKind deletion_kind =
      defer ? CheckerPopulationEventKind::StageDeleteRequested
            : CheckerPopulationEventKind::DeleteRequested;
  observe_checker(deletion_kind,
                  CheckerClauseRole::None, line_id, del_id,
                  internal_view(lits, size), false, 0,
                  defer ? pending_population_transaction_ : 0);
}

namespace {

bool external_tautological(const int32_t *lits, uint32_t size) {
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

} // namespace

bool Proof::add_external_rup_impl(uint64_t id, const int32_t *lits,
                                  uint32_t size) {
  if (!pbp_deferred_units_.empty())
    settle_pbp_deferred_units(!size && streaming_pbp());
  const CheckerClauseRole role =
      population_role_ == CheckerClauseRole::ExternalRat
          ? CheckerClauseRole::ExternalRat
          : CheckerClauseRole::ExternalRup;
  if (!input_transaction_ && !commit_clause_stream())
    return false;
  if (lrat())
    return false;
  assert(!size || lits);
  if (external_tautological(lits, size))
    return false;
  if (!file_)
    return true;
  flush_pending_dsr_units();
  if (streaming_pbp()) {
    if (streaming_pbp_finished_)
      return false;
    put_pbp_external_clause("rup", lits, size);
    std::fputs(" ;\n", file_);
    note_pbp_derived(id, size, [lits] { return lits; });
  } else {
    put_line_tag(false);
    for (uint32_t i = 0; i < size; ++i) {
      proof_external_max_ =
          std::max(proof_external_max_, external_variable_of(lits[i]));
      put_external_dimacs(lits[i]);
    }
    end_line();
  }
  population_external(true, id, lits, size);
  observe_checker(CheckerPopulationEventKind::Add, role, id, id,
                  PopulationClauseView::external_lits(lits, size), true);
  if (streaming_pbp() && !size)
    finish_streaming_pbp_unsat();
  return true;
}

bool Proof::add_external_rat_impl(uint64_t id, const int32_t *lits,
                                  uint32_t size) {
  if (!pbp_deferred_units_.empty())
    settle_pbp_deferred_units(false);
  if (size && !premise_groups_.empty()) {
    const auto *saved = variable_map_; variable_map_ = nullptr;
    const Lit pivot = internalize(lits[0]);
    const uint64_t before = premise_epoch_;
    invalidate_premises_for_sr(id, &pivot, 1, nullptr, 0, nullptr, 0);
    variable_map_ = saved;
    if (premise_epoch_ != before) flush_persistent_external(id);
  }
  if (!input_transaction_ && !commit_clause_stream())
    return false;
  if (streaming_pbp()) {
    if (!size || !lits || streaming_pbp_finished_)
      return false;
    put_pbp_external_clause("red", lits, size);
    const int32_t pivot = lits[0];
    std::fprintf(file_, " : x%" PRIu32 " -> %d ;\n",
                 external_variable_of(pivot), pivot < 0 ? 0 : 1);
    note_pbp_derived(id, size, [lits] { return lits; });
    observe_checker(CheckerPopulationEventKind::Add,
                    CheckerClauseRole::ExternalRat, id, id,
                    PopulationClauseView::external_lits(lits, size), true);
    return true;
  }
  // DSR/DRAT infer RAT from the first literal after RUP fails, so their byte
  // stream is identical to an external RUP addition.
  const CheckerClauseRole saved = population_role_;
  population_role_ = CheckerClauseRole::ExternalRat;
  const bool result = add_external_rup_impl(id, lits, size);
  population_role_ = saved;
  return result;
}

bool Proof::plan_pbp_parity_reason(
    const std::vector<PbpParityEquation> &equations, const int32_t *reason,
    uint32_t reason_size, uint32_t starting_external_extent,
    const PbpParityBoundsCache &cache, PbpParityReasonPlan &output) const {
  PbpParityReasonPlan plan;
  if (!streaming_pbp() || !file_ || input_transaction_ || pbp_output_ ||
      streaming_pbp_finished_ || equations.empty() ||
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
  if (constraints > UINT64_MAX - pbp_derived_constraints_ ||
      pbp_original_constraints_ >
          UINT64_MAX - (pbp_derived_constraints_ + constraints))
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
  if (!pbp_deferred_units_.empty())
    settle_pbp_deferred_units(false);
  const bool matching_cache = cache.stream_identity_ == stream_identity_;
  if (!plan.valid || plan.stream_identity != stream_identity_ ||
      !streaming_pbp() || !file_ || input_transaction_ || pbp_output_ ||
      streaming_pbp_finished_ || equations.empty() ||
      (!reason && reason_size) ||
      (matching_cache ? cache.generation_ : 0) != plan.cache_generation ||
      plan.proof_constraints > UINT64_MAX - pbp_derived_constraints_ ||
      pbp_original_constraints_ >
          UINT64_MAX - (pbp_derived_constraints_ + plan.proof_constraints))
    return false;
  // Cutting-planes lines have no clausal spelling: keep whole-run PBP.
  if (dsr_copy_)
    drop_dsr_copy(AutomaticProofReason::ParityReason);

  if (cache.stream_identity_ != stream_identity_) {
    cache.entries_.clear();
    cache.retained_bytes_ = 0;
    cache.generation_ = 0;
    cache.stream_identity_ = stream_identity_;
  }
  const uint64_t derived_before = pbp_derived_constraints_;
  uint32_t fresh = plan.starting_external_extent;
  PbpTextWriter text(file_);
  const auto constraint = [&]() {
    if (pbp_derived_constraints_ == UINT64_MAX ||
        pbp_original_constraints_ >
            UINT64_MAX - (pbp_derived_constraints_ + 1))
      std::abort();
    ++pbp_derived_constraints_;
    return pbp_original_constraints_ + pbp_derived_constraints_;
  };
  const auto term = [&](uint32_t coefficient, int32_t literal) {
    if (!coefficient || !literal || literal == INT32_MIN)
      std::abort();
    text.put_uint(coefficient);
    text.put(" ");
    if (literal < 0)
      text.put("~");
    text.put("x");
    text.put_uint(external_variable_of(literal));
    text.put(" ");
  };

  const auto emit_bound = [&](const PbpParityEquation &equation,
                              const std::vector<int32_t> &carries,
                              bool lower) {
    text.put("pbc ");
    for (const int32_t variable : equation.variables)
      term(1, lower ? variable : -variable);
    for (const int32_t carry : carries)
      term(2, lower ? -carry : carry);
    const uint32_t rhs = lower
        ? uint32_t(equation.rhs) + 2 * carries.size()
        : equation.variables.size() - uint32_t(equation.rhs);
    text.put(">= ");
    text.put_uint(rhs);
    text.put(" : subproof\n");
    (void)constraint();
    const auto block = [&](auto &&self, uint32_t depth, uint32_t mask) -> void {
      if (depth + 1 < equation.variables.size()) {
        self(self, depth + 1, mask);
        self(self, depth + 1, mask | (uint32_t{1} << depth));
      }
      text.put("rup ");
      for (uint32_t i = 0; i < depth; ++i)
        term(1, mask & (uint32_t{1} << i) ? -equation.variables[i]
                                          : equation.variables[i]);
      text.put(">= 1 ;\n");
      (void)constraint();
    };
    block(block, 0, 0);
    text.put("qed pbc : -1 ;\n");
    return constraint();
  };

  std::vector<uint64_t> lower, upper;
  lower.reserve(equations.size());
  upper.reserve(equations.size());
  for (const PbpParityEquation &equation : equations) {
    auto found = cache.entries_.find(equation);
    if (found == cache.entries_.end()) {
      PbpParityBounds bounds;
      for (uint32_t threshold = 2;
           threshold <= equation.variables.size(); threshold += 2) {
        if (fresh == static_cast<uint32_t>(INT32_MAX))
          std::abort();
        const int32_t carry = static_cast<int32_t>(++fresh);
        bounds.carries.push_back(carry);
        text.put("red ");
        for (const int32_t variable : equation.variables) term(1, variable);
        term(threshold, -carry);
        text.put(">= ");
        text.put_uint(threshold);
        text.put(" : x");
        text.put_uint(static_cast<uint32_t>(carry));
        text.put(" -> 0 ;\n");
        const uint64_t first = constraint();

        const uint32_t complement =
            equation.variables.size() - threshold + 1;
        text.put("red ");
        for (const int32_t variable : equation.variables) term(1, -variable);
        term(complement, carry);
        text.put(">= ");
        text.put_uint(complement);
        text.put(" : x");
        text.put_uint(static_cast<uint32_t>(carry));
        text.put(" -> 1 : subproof\n");
        const uint64_t negated_second = constraint();
        text.put("proofgoal #1\nqed ;\n");
        (void)constraint();
        text.put("proofgoal ");
        text.put_uint(first);
        text.put("\n");
        const uint64_t negated_first = constraint();
        text.put("pol ");
        text.put_uint(negated_second);
        text.put(" ");
        text.put_uint(negated_first);
        text.put(" + ;\nqed : -1 ;\nqed ;\n");
        (void)constraint();
        (void)constraint();
      }
      bounds.lower = emit_bound(equation, bounds.carries, true);
      bounds.upper = emit_bound(equation, bounds.carries, false);
      found = cache.entries_.emplace(equation, std::move(bounds)).first;
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
    lower.push_back(found->second.lower);
    upper.push_back(found->second.upper);
  }

  const auto odd = [&](int32_t variable) {
    return std::binary_search(plan.odd_variables.begin(),
                              plan.odd_variables.end(), variable);
  };
  // Add each reason literal that `select(literal, variable)` names.
  const auto add_reason_literals = [&](auto select) {
    for (uint32_t i = 0; i < reason_size; ++i) {
      const int32_t literal = reason[i];
      const int32_t variable =
          static_cast<int32_t>(external_variable_of(literal));
      if (!select(literal, variable))
        continue;
      text.put(" ");
      if (literal < 0)
        text.put("~");
      text.put("x");
      text.put_uint(static_cast<uint32_t>(variable));
      text.put(" +");
    }
  };
  const auto sum_and_round = [&](const std::vector<uint64_t> &ids,
                                 bool lower_side) {
    text.put("pol ");
    text.put_uint(ids.front());
    for (size_t i = 1; i < ids.size(); ++i) {
      text.put(" ");
      text.put_uint(ids[i]);
      text.put(" +");
    }
    add_reason_literals([&](int32_t literal, int32_t variable) {
      return odd(variable) &&
             ((lower_side && literal < 0) || (!lower_side && literal > 0));
    });
    text.put(" 2 d ;\n");
    return constraint();
  };
  const uint64_t lo = sum_and_round(lower, true);
  const uint64_t hi = sum_and_round(upper, false);
  text.put("pol ");
  text.put_uint(lo);
  text.put(" ");
  text.put_uint(hi);
  text.put(" +");
  add_reason_literals(
      [&](int32_t, int32_t variable) { return !odd(variable); });
  text.put(" ;\n");
  (void)constraint();
  text.flush();

  if (fresh != plan.starting_external_extent + plan.helper_variables ||
      pbp_derived_constraints_ - derived_before != plan.proof_constraints ||
      cache.retained_bytes() > plan.retained_bytes || std::ferror(file_))
    std::abort();
#ifndef NDEBUG
  pbp_audit_record(false, pbp_derived_constraints_, reason, reason_size);
#endif
  if (kernel_clause_id)
    track_pbp_derived_constraint(kernel_clause_id, reason_size);
  else if (reason_size != 2)
    register_pbp_anonymous(reason, reason_size);
  proof_external_max_ = std::max(proof_external_max_, fresh);
  population_external(true, kernel_clause_id, reason, reason_size);
  observe_checker(CheckerPopulationEventKind::Add,
                  CheckerClauseRole::ExternalRup, kernel_clause_id,
                  kernel_clause_id,
                  PopulationClauseView::external_lits(reason, reason_size),
                  true);
  if (!reason_size)
    finish_streaming_pbp_unsat();
  return true;
}

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
  if (!charge || lrat() || terminal_pbp() || !file_) return false;
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

bool Proof::retain_premise_groups_impl(const std::vector<PremiseGroup> &groups,
    uint64_t maximum_groups, uint64_t maximum_literals,
    uint64_t maximum_bytes, const std::function<bool(uint64_t)> &charge,
    std::vector<uint64_t> &handles, DeferredDeletionLimits limits) {
  const bool bounded = limits.maximum_pending_deletions || limits.maximum_pending_literal_work;
  if (bounded && (!limits.maximum_pending_deletions || !limits.maximum_pending_literal_work ||
      premise_pending_deletions_ > limits.maximum_pending_deletions ||
      premise_pending_literal_work_ > limits.maximum_pending_literal_work)) return false;
  if (!charge || lrat() || terminal_pbp() || !file_ ||
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
  if (input_transaction_ || !commit_clause_stream()) return false;
  leased_premises_.swap(premises); premise_groups_.swap(retained);
  next_premise_group_ = next; premise_literals_ = literals; premise_bytes_ = bytes;
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
    release_premise_groups_impl(affected, 0);
    return false;
  }
  ++premise_pending_deletions_; premise_pending_literal_work_ += cost;
  if (found->second.deferred == UINT64_MAX || premise_deferred_ == UINT64_MAX)
    std::abort();
  ++found->second.deferred; ++premise_deferred_;
  return true;
}

void Proof::release_premise_groups_impl(const std::vector<uint64_t> &handles,
                                       uint64_t line_id) {
  bool changed = false;
  for (uint64_t handle : handles) {
    auto group = premise_groups_.find(handle);
    if (group == premise_groups_.end()) continue;
    const uint64_t maintenance = 1 + group->second.size();
    add_saturating(premise_maintenance_work_, maintenance);
    PremiseGroup clauses = std::move(group->second);
    premise_groups_.erase(group); changed = true;
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
      pbp_replaying_leased_ = true;
      for (uint64_t i = 0; i < deferred; ++i)
        if (!del_external_impl(line_id, 0, clause.data(), clause.size())) std::abort();
      pbp_replaying_leased_ = false;
    }
  }
  if (changed && ++premise_epoch_ == 0) std::abort();
}

void Proof::invalidate_premises_for_sr(uint64_t line_id, const Lit *lits,
    uint32_t size, const Lit *true_lits, uint32_t ntrue,
    const DsrLiteralMapping *mappings, uint32_t nmappings) {
  if (premise_groups_.empty()) return;
  const uint64_t changed_bound = 1 + uint64_t(ntrue) + nmappings;
  const uint64_t work = 1 + premise_groups_.size() +
      changed_bound * (2 * std::bit_width(changed_bound) + 2) +
      premise_literals_ * (std::bit_width(changed_bound) + 1);
  add_saturating(premise_maintenance_work_, work);
  std::vector<uint32_t> changed;
  const auto take = [&](Lit lit) {
    changed.push_back(var_of(checker_lit(lit)) + 1);
  };
  // The implicit pivot image is true even without an explicit mapping.
  if (size) take(lits[0]);
  for (uint32_t i = 0; i < ntrue; ++i) take(true_lits[i]);
  for (uint32_t i = 0; i < nmappings; ++i) take(mappings[i].from);
  std::sort(changed.begin(), changed.end());
  std::vector<uint64_t> retire;
  for (const auto &[handle, clauses] : premise_groups_) {
    bool touched = false;
    for (const auto &clause : clauses)
      for (int32_t literal : clause)
        touched |= std::binary_search(changed.begin(), changed.end(),
                                      external_variable_of(literal));
    if (touched) retire.push_back(handle);
  }
  release_premise_groups_impl(retire, line_id);
}

uint64_t Proof::persistent_external_bytes() const {
  uint64_t bytes = persistent_external_.capacity() * sizeof(PersistentExternalClause);
  for (const auto &clause : persistent_external_)
    bytes += clause.lits.capacity() * sizeof(int32_t);
  return bytes;
}

bool Proof::retain_external_impl(uint64_t id, const int32_t *lits,
                                 uint32_t size) {
  if (!id || !size || !lits ||
      external_tautological(lits, size) || lrat() || terminal_pbp())
    return false;
  if (!file_)
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
  if (lrat() || terminal_pbp() ||
      persistent_external_epoch_ == UINT64_MAX)
    std::abort();
  std::vector<PersistentExternalClause> clauses =
      std::move(persistent_external_);
  persistent_external_.clear();
  for (auto clause = clauses.rbegin(); clause != clauses.rend(); ++clause)
    if (!del_external_impl(line_id, clause->id, clause->lits.data(),
                      clause->lits.size()))
      std::abort();
  ++persistent_external_epoch_;
}

bool Proof::del_external_impl(uint64_t line_id, uint64_t del_id,
                              const int32_t *lits, uint32_t size) {
  if (!pbp_deferred_units_.empty())
    settle_pbp_deferred_units(false);
  if (defer_leased_deletion(lits, size)) return true;
  if (!input_transaction_ && !commit_clause_stream())
    return false;
  assert(!size || lits);
  if (!file_)
    return true;
  if (streaming_pbp()) {
    if (streaming_pbp_finished_ ||
        !put_pbp_deletion(del_id, size, [lits] { return lits; }))
      return true;
    // Both PBP deletion forms report the same checker-population request.
    population_external(false, del_id, lits, size);
    observe_checker(CheckerPopulationEventKind::DeleteRequested,
                    CheckerClauseRole::None, line_id, del_id,
                    PopulationClauseView::external_lits(lits, size), true);
    return true;
  }
  if (lrat() || pb())
    return false;
  if ((del_id && pending_unit_ids_.count(del_id)) ||
      (size == 1 && pending_unit_literals_.count(lits[0])))
    flush_pending_dsr_units();
  const bool defer = !pending_units_.empty();
  FILE *const main_file = defer ? begin_pending_deletion() : nullptr;
  put_line_tag(true);
  for (uint32_t i = 0; i < size; ++i) {
    const uint32_t variable = external_variable_of(lits[i]);
    proof_external_max_ = std::max(proof_external_max_, variable);
    if (defer)
      pending_max_external_ = std::max(pending_max_external_, variable);
    put_external_dimacs(lits[i]);
  }
  end_line();
  if (defer)
    end_pending_deletion(main_file);
  population_external(false, del_id, lits, size);
  observe_checker(defer ? CheckerPopulationEventKind::StageDeleteRequested
                         : CheckerPopulationEventKind::DeleteRequested,
                  CheckerClauseRole::None, line_id, del_id,
                  PopulationClauseView::external_lits(lits, size), true, 0,
                  defer ? pending_population_transaction_ : 0);
  return true;
}

void Proof::flush_impl() {
  if (streaming_pbp() && pbp_output_)
    publish_streaming_pbp_header();
  flush_pending_dsr_units();
  if (file_) {
    flush_binary();
    // Text serializers use stdio buffering. Both a previous short write and
    // this publication flush must succeed before a caller may report UNSAT.
    if (std::ferror(file_) || std::fflush(file_)) {
      if (!copy_role_)
        std::abort();
      copy_failed_ = true;
    }
  }
}

#ifdef TACTIX_ENABLE_PROOF_SHADOW
// Key a clause exactly the way the target format's checker matches a deletion:
// by the sorted external literal multiset under DRAT, by the clause id under
// native LRAT. Keying DRAT by id would miss the whole defect class, because the
// hazard is precisely that two different ids can carry the same literals.
std::string Proof::population_key(uint64_t id, const Lit *lits,
                                  uint32_t size) const {
  std::string key;
  if (format_ == ProofFormat::Lrat) {
    key.assign(reinterpret_cast<const char *>(&id), sizeof id);
    return key;
  }
  std::vector<int> external;
  external.reserve(size);
  for (uint32_t i = 0; i < size; i++)
    external.push_back(externalize(checker_lit(lits[i])));
  std::sort(external.begin(), external.end());
  key.assign(reinterpret_cast<const char *>(external.data()),
             external.size() * sizeof(int));
  return key;
}

void Proof::population_add(uint64_t id, const Lit *lits, uint32_t size) {
  population_[population_key(id, lits, size)]++;
}

bool Proof::population_contains(uint64_t id, const Lit *lits,
                                uint32_t size) const {
  const auto found = population_.find(population_key(id, lits, size));
  return found != population_.end() && found->second;
}

void Proof::population_del(uint64_t id, const Lit *lits, uint32_t size) {
  const auto found = population_.find(population_key(id, lits, size));
  if (found != population_.end() && found->second) {
    if (!--found->second)
      population_.erase(found);
    return;
  }
  if (!armed_)
    return; // unknown pre-seeding deltas remain outside the strict oracle
  unbacked_deletions_++;
  fprintf(stderr, "PROOF SHADOW: unbacked deletion #%" PRIu64 " of [%" PRIu64
                  "]",
          unbacked_deletions_, id);
  for (uint32_t i = 0; i < size; i++)
    fprintf(stderr, " %d", externalize(checker_lit(lits[i])));
  fputs(" 0\n", stderr);
  void *frames[32];
  const int depth = backtrace(frames, 32);
  backtrace_symbols_fd(frames, depth, fileno(stderr));
  fflush(stderr);
  // Abort on the FIRST one: the emitting rule is what this build exists to
  // name, and every later line is downstream of an already-diverged checker.
  std::abort();
}

// External DIMACS literals already name checker variables: key them through
// an identity map.
void Proof::population_external(bool add, uint64_t id, const int32_t *lits,
                                uint32_t size) {
  std::vector<Lit> external;
  external.reserve(size);
  for (uint32_t i = 0; i < size; ++i)
    external.push_back(internalize(lits[i]));
  const auto *saved = variable_map_;
  variable_map_ = nullptr;
  if (add)
    population_add(id, external.data(), size);
  else
    population_del(id, external.data(), size);
  variable_map_ = saved;
}

#endif

void Proof::seed_population_impl(uint64_t id, const Lit *lits, uint32_t size) {
  population_add(id, lits, size);
  if (!file_)
    return;
  // VeriPB's `f` statement names the number of constraints in the original
  // CNF, not the largest internal clause id.  Streaming PBP may derive and
  // spool root units while input parsing is still in progress; those derived
  // ids can therefore make later raw-input ids non-contiguous.  Count each
  // raw seed exactly once instead of inferring the input population from its
  // internal identity namespace.
  if (streaming_pbp() && input_transaction_) {
    ++pbp_original_constraints_;
    track_pbp_original_constraint(id, size, size == 2 && lits[0] == lits[1]);
#ifndef NDEBUG
    pbp_audit_record(true, pbp_original_constraints_,
                     pbp_checker_literals(lits, size), size);
#endif
    pbp_last_input_id_ = id;
    pbp_last_input_constraint_ = pbp_original_constraints_;
    pbp_last_input_live_ = id != 0;
  }
  observe_checker(CheckerPopulationEventKind::InputSeed,
                  CheckerClauseRole::Input, id, id, internal_view(lits, size));
}

} // namespace tactix

// --- automatic selection (plan §5) -----------------------------------------
// The public event methods run the PBP body, then feed the same event to the
// native-DSR copy. Kernel queries see only the PBP stream, so the copy never
// steers the solver. An event the copy cannot follow ends it before writing.

namespace tactix {

const char *automatic_proof_reason_name(AutomaticProofReason reason) noexcept {
  switch (reason) {
  case AutomaticProofReason::Pending: return "pending";
  case AutomaticProofReason::DsrSelected: return "dsr-selected";
  case AutomaticProofReason::StartupPbRefutation: return "startup-pb-refutation";
  case AutomaticProofReason::ParityReason: return "gf2-parity-reason";
  case AutomaticProofReason::SrWithAffineSupport: return "sr-with-affine-support";
  case AutomaticProofReason::CopyDeclined: return "dsr-copy-declined";
  case AutomaticProofReason::CopyWriteFailed: return "dsr-copy-write-failed";
  case AutomaticProofReason::DestinationUnsupported:
    return "destination-unsupported";
  case AutomaticProofReason::StageUnavailable: return "stage-unavailable";
  }
  return "unknown";
}

namespace {

// `<dir>/.<name>.dsr-XXXXXX` next to `path`, so the final rename stays on
// the destination's filesystem.
std::string sibling_stage_template(const char *path) {
  const std::string full(path);
  const size_t slash = full.rfind('/');
  const std::string directory =
      slash == std::string::npos ? std::string() : full.substr(0, slash + 1);
  const std::string name =
      slash == std::string::npos ? full : full.substr(slash + 1);
  return directory + "." + name + ".dsr-XXXXXX";
}

} // namespace

bool Proof::open_automatic(const char *path, bool binary_dsr,
                           const char *stage_path) {
  if (!open(path, ProofFormat::PbpStreaming))
    return false;
  automatic_ = true;
  automatic_selected_dsr_ = false;
  automatic_dsr_witness_ = false;
  automatic_reason_ = AutomaticProofReason::Pending;
  automatic_path_ = path;
  automatic_stage_name_.clear();
  // The empty clause rewrites the destination in place, so it must be a
  // regular file (a proc-fd path to an anonymous file qualifies).
  FILE *const destination = pbp_output_ ? pbp_output_ : file_;
  struct stat destination_stat;
  if (fstat(fileno(destination), &destination_stat) ||
      !S_ISREG(destination_stat.st_mode)) {
    automatic_reason_ = AutomaticProofReason::DestinationUnsupported;
    return true;
  }
  FILE *stage = nullptr;
  struct stat link_stat;
  const bool renameable = !lstat(path, &link_stat) &&
                          S_ISREG(link_stat.st_mode) &&
                          link_stat.st_nlink == 1 &&
                          link_stat.st_dev == destination_stat.st_dev &&
                          link_stat.st_ino == destination_stat.st_ino;
  if (stage_path) {
    stage = std::fopen(stage_path, "w+b");
  } else if (renameable) {
    std::string name = sibling_stage_template(path);
    const int descriptor = mkstemp(name.data());
    if (descriptor >= 0) {
      struct stat stage_stat;
      if (!fstat(descriptor, &stage_stat) &&
          stage_stat.st_dev == destination_stat.st_dev &&
          !fchmod(descriptor, destination_stat.st_mode & 07777) &&
          (stage = fdopen(descriptor, "w+b"))) {
        automatic_stage_name_ = name;
      } else {
        close(descriptor);
        unlink(name.c_str());
      }
    }
  }
  if (!stage && !stage_path)
    stage = std::tmpfile();
  if (!stage) {
    automatic_reason_ = AutomaticProofReason::StageUnavailable;
    return true;
  }
  auto copy = std::make_unique<Proof>();
  copy->copy_role_ = true;
  if (!copy->open_stream(stage, binary_dsr ? ProofFormat::DsrBinary
                                           : ProofFormat::DsrAscii)) {
    if (!automatic_stage_name_.empty())
      unlink(automatic_stage_name_.c_str());
    automatic_stage_name_.clear();
    automatic_reason_ = AutomaticProofReason::StageUnavailable;
    return true;
  }
  copy->bind_variable_map(variable_map_);
  copy->bind_external_variable_map(external_variable_map_);
  dsr_copy_ = std::move(copy);
  return true;
}

void Proof::drop_dsr_copy(AutomaticProofReason reason) noexcept {
  if (!dsr_copy_)
    return;
  // Discard the copy's staged and buffered bytes instead of writing them,
  // and release the stage's space at once (a caller-owned stage stays).
  Proof &copy = *dsr_copy_;
  copy.pending_units_.clear();
  copy.pending_unit_literals_.clear();
  copy.pending_unit_ids_.clear();
  copy.pending_deletion_bytes_.clear();
  copy.spooling_deletions_ = false;
  copy.binary_buffer_size_ = 0;
  copy.copy_failed_ = true;
  if (copy.file_) {
    __fpurge(copy.file_);
    (void)ftruncate(fileno(copy.file_), 0);
    std::fclose(copy.file_);
    copy.file_ = nullptr;
  }
  dsr_copy_.reset();
  if (!automatic_stage_name_.empty())
    unlink(automatic_stage_name_.c_str());
  automatic_stage_name_.clear();
  automatic_reason_ = reason;
}

void Proof::settle_dsr_copy() {
  if (!dsr_copy_)
    return;
  if (dsr_copy_->copy_failed_) {
    drop_dsr_copy(AutomaticProofReason::CopyWriteFailed);
    return;
  }
  // The empty clause completed both streams: keep the native DSR copy.
  if (streaming_pbp_finished_)
    select_dsr_copy();
}

// Replace the destination's PBP bytes with the complete DSR copy: rename a
// sibling stage over the destination, else rewrite the destination in place.
// The discarded PBP bytes are dropped unwritten, so only the DSR bytes can
// fail to land; a failed placement aborts before any UNSAT verdict.
void Proof::select_dsr_copy() {
  Proof &copy = *dsr_copy_;
  copy.flush_impl();
  if (copy.copy_failed_) {
    drop_dsr_copy(AutomaticProofReason::CopyWriteFailed);
    return;
  }
  FILE *destination = pbp_output_ ? pbp_output_ : file_;
  FILE *stage = copy.file_;
  copy.file_ = nullptr;
  // The PBP stream accepts nothing further. A still-private PBP prefix and
  // any buffered PBP bytes are discarded.
  if (pbp_output_ && file_ && file_ != pbp_output_) {
    __fpurge(file_);
    std::fclose(file_);
  }
  file_ = nullptr;
  pbp_output_ = nullptr;
  binary_buffer_size_ = 0;
  __fpurge(destination);
  bool placed = false;
  if (!automatic_stage_name_.empty()) {
    if (std::fclose(stage))
      std::abort();
    stage = nullptr;
    std::fclose(destination);
    destination = nullptr;
    placed = !std::rename(automatic_stage_name_.c_str(),
                          automatic_path_.c_str());
    if (!placed &&
        (!(stage = std::fopen(automatic_stage_name_.c_str(), "rb")) ||
         !(destination = std::fopen(automatic_path_.c_str(), "wb"))))
      std::abort();
  }
  if (!placed) {
    if (ftruncate(fileno(destination), 0) ||
        std::fseek(destination, 0, SEEK_SET) || std::fseek(stage, 0, SEEK_SET))
      std::abort();
    copy_stream(stage, destination);
    if (std::fflush(destination) || std::ferror(destination))
      std::abort();
    // The copied stage's bytes are no longer needed (a caller-owned stage
    // stays, emptied).
    (void)ftruncate(fileno(stage), 0);
    if (std::fclose(stage))
      std::abort();
    if (!automatic_stage_name_.empty())
      unlink(automatic_stage_name_.c_str());
  }
  automatic_stage_name_.clear();
  if (destination && std::fclose(destination))
    std::abort();
  automatic_selected_dsr_ = true;
  automatic_dsr_witness_ = copy.has_sr_witness_;
  automatic_reason_ = AutomaticProofReason::DsrSelected;
  dsr_copy_.reset();
}

void Proof::finish_input_transaction() {
  finish_input_transaction_impl();
  if (dsr_copy_) {
    dsr_copy_->finish_input_transaction();
    settle_dsr_copy();
  }
}

FILE *Proof::begin_pb() {
  FILE *const terminal = begin_pb_impl();
  // A complete native PB refutation has no clausal spelling.
  if (terminal)
    drop_dsr_copy(AutomaticProofReason::StartupPbRefutation);
  return terminal;
}

void Proof::add_rup(uint64_t id, const Lit *lits, uint32_t size,
                    const uint64_t *chain, uint32_t nchain) {
  add_rup_impl(id, lits, size, chain, nchain);
  if (dsr_copy_) {
    dsr_copy_->add_rup(id, lits, size, chain, nchain);
    settle_dsr_copy();
  }
}

void Proof::add_rat(uint64_t id, const Lit *lits, uint32_t size,
                    const int64_t *hints, uint32_t nhints) {
  add_rat_impl(id, lits, size, hints, nhints);
  if (dsr_copy_) {
    dsr_copy_->add_rat(id, lits, size, hints, nhints);
    settle_dsr_copy();
  }
}

bool Proof::add_sr(uint64_t id, const Lit *lits, uint32_t size,
                   const Lit *true_lits, uint32_t ntrue,
                   const DsrLiteralMapping *mappings, uint32_t nmappings,
                   uint32_t external_variables, bool propagates) {
  const bool written = add_sr_impl(id, lits, size, true_lits, ntrue, mappings,
                                   nmappings, external_variables, propagates);
  if (!written || !dsr_copy_)
    return written;
  // Native DSR retires leased affine premises and retained proof-only
  // support before a witnessed line; PBP keeps both, and later clausal
  // affine steps may cite them. The copy cannot follow that state.
  if (!dsr_copy_->premise_groups_.empty() ||
      !dsr_copy_->persistent_external_.empty()) {
    drop_dsr_copy(AutomaticProofReason::SrWithAffineSupport);
    return written;
  }
  if (!dsr_copy_->add_sr(id, lits, size, true_lits, ntrue, mappings, nmappings,
                         external_variables, propagates))
    drop_dsr_copy(AutomaticProofReason::CopyDeclined);
  else
    settle_dsr_copy();
  return written;
}

void Proof::add_empty(uint64_t id, const uint64_t *chain, uint32_t nchain) {
  add_empty_impl(id, chain, nchain);
  if (dsr_copy_) {
    dsr_copy_->add_empty(id, chain, nchain);
    settle_dsr_copy();
  }
}

void Proof::add_derived_unit(uint64_t id, Lit unit, const uint64_t *chain,
                             uint32_t nchain) {
  add_derived_unit_impl(id, unit, chain, nchain);
  if (dsr_copy_) {
    dsr_copy_->add_derived_unit(id, unit, chain, nchain);
    settle_dsr_copy();
  }
}

void Proof::del(uint64_t line_id, uint64_t del_id, const Lit *lits,
                uint32_t size) {
  del_impl(line_id, del_id, lits, size);
  if (dsr_copy_) {
    dsr_copy_->del(line_id, del_id, lits, size);
    settle_dsr_copy();
  }
}

bool Proof::add_external_rup(uint64_t id, const int32_t *lits,
                             uint32_t size) {
  const bool written = add_external_rup_impl(id, lits, size);
  if (written && dsr_copy_) {
    if (!dsr_copy_->add_external_rup(id, lits, size))
      drop_dsr_copy(AutomaticProofReason::CopyDeclined);
    else
      settle_dsr_copy();
  }
  return written;
}

bool Proof::add_external_rat(uint64_t id, const int32_t *lits,
                             uint32_t size) {
  const bool written = add_external_rat_impl(id, lits, size);
  if (written && dsr_copy_) {
    if (!dsr_copy_->add_external_rat(id, lits, size))
      drop_dsr_copy(AutomaticProofReason::CopyDeclined);
    else
      settle_dsr_copy();
  }
  return written;
}

bool Proof::del_external(uint64_t line_id, uint64_t del_id,
                         const int32_t *lits, uint32_t size) {
  const bool written = del_external_impl(line_id, del_id, lits, size);
  if (written && dsr_copy_) {
    if (!dsr_copy_->del_external(line_id, del_id, lits, size))
      drop_dsr_copy(AutomaticProofReason::CopyDeclined);
    else
      settle_dsr_copy();
  }
  return written;
}

bool Proof::retain_external(uint64_t id, const int32_t *lits, uint32_t size) {
  const bool retained = retain_external_impl(id, lits, size);
  if (retained && dsr_copy_) {
    if (!dsr_copy_->retain_external(id, lits, size))
      drop_dsr_copy(AutomaticProofReason::CopyDeclined);
    else
      settle_dsr_copy();
  }
  return retained;
}

bool Proof::retain_premise_groups(const std::vector<PremiseGroup> &groups,
    uint64_t maximum_groups, uint64_t maximum_literals,
    uint64_t maximum_bytes, const std::function<bool(uint64_t)> &charge,
    std::vector<uint64_t> &handles, DeferredDeletionLimits limits) {
  const bool retained = retain_premise_groups_impl(
      groups, maximum_groups, maximum_literals, maximum_bytes, charge, handles,
      limits);
  if (retained && dsr_copy_) {
    // The kernel's work meter is charged once, by the PBP stream.
    const std::function<bool(uint64_t)> free_charge = [](uint64_t) {
      return true;
    };
    std::vector<uint64_t> copy_handles;
    if (!dsr_copy_->retain_premise_groups(groups, maximum_groups,
                                          maximum_literals, maximum_bytes,
                                          free_charge, copy_handles, limits) ||
        copy_handles != handles)
      drop_dsr_copy(AutomaticProofReason::CopyDeclined);
    else
      settle_dsr_copy();
  }
  return retained;
}

void Proof::release_premise_groups(const std::vector<uint64_t> &handles,
                                  uint64_t line_id) {
  release_premise_groups_impl(handles, line_id);
  if (dsr_copy_) {
    dsr_copy_->release_premise_groups(handles, line_id);
    settle_dsr_copy();
  }
}

void Proof::seed_population(uint64_t id, const Lit *lits, uint32_t size) {
  seed_population_impl(id, lits, size);
  if (dsr_copy_) {
    dsr_copy_->seed_population(id, lits, size);
    settle_dsr_copy();
  }
}

void Proof::flush() {
  flush_impl();
  if (dsr_copy_) {
    dsr_copy_->flush();
    settle_dsr_copy();
  }
}

} // namespace tactix
