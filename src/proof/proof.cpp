#include "proof/proof.hpp"
#ifdef TACTIX_ENABLE_RULE_JOURNAL
#include "diagnostics/rule_journal.hpp"
#endif

#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <cinttypes>
#include <cstdlib>
#ifdef TACTIX_ENABLE_PROOF_SHADOW
#include <execinfo.h>
#endif

namespace tactix {

CheckerRoute Proof::population_route() const noexcept {
  if (!file_ && !automatic_output_)
    return CheckerRoute::None;
  if (automatic_pending_)
    return CheckerRoute::AutomaticPending;
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
  case ProofFormat::Automatic:
    return CheckerRoute::AutomaticPending;
  }
  return CheckerRoute::None;
}

CheckerNaming Proof::population_naming() const noexcept {
  const CheckerRoute route = population_route();
  if (route == CheckerRoute::Lrat)
    return CheckerNaming::Id;
  if (route == CheckerRoute::Dsr || route == CheckerRoute::Drat ||
      route == CheckerRoute::AutomaticPending)
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
    // commit for stages it never saw.  Automatic is deliberately not settled:
    // merely changing an observer must not choose DSR over PB.
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
  if (automatic_pending_)
    return false;
  flush();
  return true;
}

void Proof::observe_checker_id_rebind(uint64_t predecessor_id,
                                      uint64_t checker_id, const Lit *lits,
                                      uint32_t size) noexcept {
  observe_checker(CheckerPopulationEventKind::IdRebind,
                  CheckerClauseRole::Input, checker_id, checker_id,
                  PopulationClauseView::internal_lits(lits, size,
                                                       variable_map_),
                  false, predecessor_id);
}

void Proof::finish_input_transaction() {
  observe_checker(CheckerPopulationEventKind::InputFinished,
                  CheckerClauseRole::Input, 0, 0, {});
  input_transaction_ = false;
  if (streaming_pbp())
    publish_streaming_pbp_header();
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
  flush();
  bind_population_observer(nullptr);
  if (file_ && std::fclose(file_))
    std::abort();
  if (pbp_output_ && std::fclose(pbp_output_))
    std::abort();
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
  if (file_ || automatic_output_ || pbp_output_ || format == ProofFormat::Pbp)
    return false;
  file_ = fopen(path, "wb");
  format_ = format;
  has_sr_witness_ = false;
  streaming_pbp_finished_ = false;
  pbp_original_constraints_ = 0;
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
  if (file_ && format == ProofFormat::Automatic) {
    automatic_output_ = file_;
    file_ = std::tmpfile();
    if (!file_) {
      std::fclose(automatic_output_);
      automatic_output_ = nullptr;
      return false;
    }
    format_ = ProofFormat::DsrBinary;
    automatic_pending_ = input_transaction_ = true;
  }
  if(file_) {
    stream_identity_=next_proof_stream_identity();
    if(!stream_identity_) {
      std::fclose(file_);file_=nullptr;
      if(automatic_output_){std::fclose(automatic_output_);automatic_output_=nullptr;}
      if(pbp_output_){std::fclose(pbp_output_);pbp_output_=nullptr;}
      automatic_pending_=input_transaction_=false;return false;
    }
  }
  if (file_)
    setvbuf(file_, nullptr, binary() ? _IONBF : _IOFBF, 1 << 20);
  if (file_) {
    observe_checker(CheckerPopulationEventKind::RouteOpened,
                    CheckerClauseRole::None, 0, 0, {});
    if (!automatic_pending_)
      observe_checker(CheckerPopulationEventKind::RouteCommitted,
                      CheckerClauseRole::None, 0, 0, {});
  }
  return file_ != nullptr;
}

bool Proof::commit_dsr() {
  if (!automatic_pending_)
    return streaming_pbp() || !pb();
  // Preserve the ordinary binary serializer's unbuffered stdio contract:
  // binary_buffer_ owns buffering and every fwrite must observe I/O failure.
  // The destination has not yet received any bytes, so setvbuf is still legal.
  if (std::setvbuf(automatic_output_, nullptr, _IONBF, 0))
    std::abort();
  flush_binary();
  if (std::fflush(file_) || std::fseek(file_, 0, SEEK_SET))
    std::abort();
  std::array<unsigned char, 65536> bytes{};
  for (;;) {
    const size_t count = std::fread(bytes.data(), 1, bytes.size(), file_);
    if (count && std::fwrite(bytes.data(), 1, count, automatic_output_) != count)
      std::abort();
    if (count < bytes.size()) {
      if (std::ferror(file_))
        std::abort();
      break;
    }
  }
  std::fclose(file_);
  file_ = automatic_output_;
  automatic_output_ = nullptr;
  automatic_pending_ = input_transaction_ = false;
  observe_checker(CheckerPopulationEventKind::RouteCommitted,
                  CheckerClauseRole::None, 0, 0, {});
  return true;
}

FILE *Proof::begin_pb() {
  if (!startup_pb_available())
    return nullptr;
  // The input DSR transaction was never published. Its facts are justified
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
  std::fclose(file_);
  binary_buffer_size_ = 0;
  file_ = automatic_output_;
  automatic_output_ = nullptr;
  automatic_pending_ = false;
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
  std::array<unsigned char, 65536> bytes{};
  for (;;) {
    const size_t count = std::fread(bytes.data(), 1, bytes.size(), file_);
    if (count && std::fwrite(bytes.data(), 1, count, pbp_output_) != count)
      std::abort();
    if (count < bytes.size()) {
      if (std::ferror(file_))
        std::abort();
      break;
    }
  }
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
            pending_deletion_bytes_.size())
      std::abort();
    pending_deletion_bytes_.clear();
  }
  FILE *destination = spooling_deletions_ ? pending_deletions_ : file_;
  const size_t written =
      fwrite(binary_buffer_.data(), 1, binary_buffer_size_, destination);
  if (written != binary_buffer_size_)
    std::abort();
  binary_buffer_size_ = 0;
}

// Binary DSR/DRAT literal encoding: map external literal e
// to unsigned u = (|e| << 1) | (e < 0), then 7-bit varint chunks,
// high bit = continuation.
void Proof::put_lit(Lit l) {
  if (variable_map_) {
    assert(var_of(l) < variable_map_->size());
    l = make_lit((*variable_map_)[var_of(l)], negated(l));
  }
  put_external_lit(l);
}

void Proof::put_external_lit(Lit l) {
  put_external_dimacs(externalize(l));
}

void Proof::put_external_dimacs(int32_t e) {
  assert(e && e != INT32_MIN);
  if (binary()) {
    const uint32_t magnitude =
        static_cast<uint32_t>(e < 0 ? -int64_t(e) : int64_t(e));
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

void Proof::put_pbp_external_dimacs(int32_t literal) {
  assert(literal && literal != INT32_MIN);
  std::fputs("1 ", file_);
  if (literal < 0)
    std::fputc('~', file_);
  std::fprintf(file_, "x%" PRIu32 " ", static_cast<uint32_t>(
      literal < 0 ? -int64_t(literal) : int64_t(literal)));
}

void Proof::put_pbp_lit(Lit literal) {
  if (variable_map_) {
    assert(var_of(literal) < variable_map_->size());
    literal = make_lit((*variable_map_)[var_of(literal)], negated(literal));
  }
  put_pbp_external_dimacs(externalize(literal));
}

void Proof::put_pbp_clause(const char *rule, const Lit *lits,
                           uint32_t size) {
  std::fprintf(file_, "%s ", rule);
  for (uint32_t i = 0; i < size; ++i)
    put_pbp_lit(lits[i]);
  std::fputs(">= 1", file_);
}

void Proof::put_pbp_external_clause(const char *rule, const int32_t *lits,
                                    uint32_t size) {
  std::fprintf(file_, "%s ", rule);
  for (uint32_t i = 0; i < size; ++i)
    put_pbp_external_dimacs(lits[i]);
  std::fputs(">= 1", file_);
}

// VeriPB's CNF reader drops tautological clauses and canonicalizes repeated
// literals before populating the initial constraint database.  A `del spec`
// must name that canonical constraint, rather than the raw DIMACS spelling
// retained by Tactix for input-proof accounting.
static bool canonical_pbp_deletion(const int32_t *lits, uint32_t size,
                                   std::vector<int32_t> &canonical) {
  canonical.clear();
  canonical.reserve(size);
  for (uint32_t i = 0; i < size; ++i) {
    const int32_t literal = lits[i];
    if (!literal || literal == INT32_MIN)
      return false;
    bool duplicate = false;
    for (const int32_t other : canonical) {
      if (literal == -other)
        return false;
      if (literal == other) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate)
      canonical.push_back(literal);
  }
  return true;
}

void Proof::put_u64(uint64_t v) { fprintf(file_, "%" PRIu64 " ", v); }

void Proof::end_line() {
  if (binary())
    put_byte(0);
  else
    fputs("0\n", file_);
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
    if (format_ == ProofFormat::DsrBinary)
      put_byte(deletion ? 2 : 1);
    else if (deletion)
      fputs("d ", file_);
    put_external_dimacs(unit);
    if (companion)
      put_external_dimacs(companion);
    end_line();
  };
  if (split) {
    // Under either assumption on the shared auxiliary, every earlier pair
    // propagates its unit. Thus both helpers for the next RUP unit see the
    // same preceding units as the original stream. The pair also makes each
    // unit RUP after the deferred deletions, under an assumption of its negation.
    for (const auto &unit : pending_units_) {
      emit(unit.external, false, auxiliary);
      const int32_t positive[2] = {unit.external, auxiliary};
      observe_checker(CheckerPopulationEventKind::Add,
                      CheckerClauseRole::DsrSplitHelper, 0, 0,
                      PopulationClauseView::external_lits(positive, 2), true,
                      0, pending_population_transaction_);
      emit(unit.external, false, -auxiliary);
      const int32_t negative[2] = {unit.external, -auxiliary};
      observe_checker(CheckerPopulationEventKind::Add,
                      CheckerClauseRole::DsrSplitHelper, 0, 0,
                      PopulationClauseView::external_lits(negative, 2), true,
                      0, pending_population_transaction_);
    }
  } else {
    // No deletion needs moving, or the external literal representation has
    // no spare identity; preserve the original direct-unit serialization.
    for (const auto &unit : pending_units_) {
      emit(unit.external, false, 0);
      observe_checker(CheckerPopulationEventKind::Add, unit.role, unit.id,
                      unit.id,
                      PopulationClauseView::external_lits(&unit.external, 1),
                      false, 0, pending_population_transaction_);
    }
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
      std::array<unsigned char, 65536> bytes{};
      for (;;) {
        const size_t count = std::fread(bytes.data(), 1, bytes.size(),
                                        pending_deletions_);
        if (count && std::fwrite(bytes.data(), 1, count, file_) != count)
          std::abort();
        if (count < bytes.size()) {
          if (std::ferror(pending_deletions_))
            std::abort();
          break;
        }
      }
      if (std::fclose(pending_deletions_))
        std::abort();
      pending_deletions_ = nullptr;
    }
    observe_checker(CheckerPopulationEventKind::PendingDeletesCommit,
                    CheckerClauseRole::None, 0, 0, {}, false, 0,
                    pending_population_transaction_);
  }
  if (split) {
    for (const auto &unit : pending_units_) {
      emit(unit.external, false, 0);
      observe_checker(CheckerPopulationEventKind::Add, unit.role, unit.id,
                      unit.id,
                      PopulationClauseView::external_lits(&unit.external, 1),
                      false, 0, pending_population_transaction_);
    }
    for (const auto &unit : pending_units_) {
      emit(unit.external, true, auxiliary);
      const int32_t positive[2] = {unit.external, auxiliary};
      observe_checker(CheckerPopulationEventKind::DeleteRequested,
                      CheckerClauseRole::DsrSplitHelper, 0, 0,
                      PopulationClauseView::external_lits(positive, 2), true,
                      0, pending_population_transaction_);
      emit(unit.external, true, -auxiliary);
      const int32_t negative[2] = {unit.external, -auxiliary};
      observe_checker(CheckerPopulationEventKind::DeleteRequested,
                      CheckerClauseRole::DsrSplitHelper, 0, 0,
                      PopulationClauseView::external_lits(negative, 2), true,
                      0, pending_population_transaction_);
    }
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

void Proof::add_rup(uint64_t id, const Lit *lits, uint32_t size,
                    const uint64_t *chain, uint32_t nchain) {
  const CheckerClauseRole role = population_role_;
  if (!input_transaction_ && !commit_dsr())
    std::abort(); // a terminal PB stream cannot accept a clausal suffix
  // A new unit extends a consecutive batch. A deletion block or a nonunit
  // addition ends it, so later RUP checks still see the earlier units.
  if (!dsr() || size != 1 || pending_deletions_ ||
      !pending_deletion_bytes_.empty())
    flush_pending_dsr_units();
#ifdef TACTIX_ENABLE_RULE_JOURNAL
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
  if (streaming_pbp()) {
    if (streaming_pbp_finished_)
      std::abort();
    put_pbp_clause("rup", lits, size);
    std::fputs(" ;\n", file_);
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    population_add(id, lits, size);
#endif
    observe_checker(CheckerPopulationEventKind::Add, role, id, id,
                    PopulationClauseView::internal_lits(lits, size,
                                                         variable_map_));
    if (!size)
      finish_streaming_pbp_unsat();
    return;
  }
  if (dsr() && size == 1) {
    Lit external = lits[0];
    if (variable_map_) {
      assert(var_of(external) < variable_map_->size());
      external = make_lit((*variable_map_)[var_of(external)], negated(external));
    }
    const int32_t unit = externalize(external);
    pending_units_.push_back({unit, id, role});
    pending_unit_literals_.insert(unit);
    if (id)
      pending_unit_ids_.insert(id);
    pending_max_external_ = static_cast<uint32_t>(
        std::max<int64_t>(pending_max_external_,
            unit < 0 ? -int64_t(unit) : int64_t(unit)));
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    population_add(id, lits, size);
#endif
    if (!pending_population_transaction_)
      pending_population_transaction_ = ++next_population_transaction_;
    observe_checker(CheckerPopulationEventKind::StageAdd, role, id, id,
                    PopulationClauseView::internal_lits(lits, size,
                                                         variable_map_),
                    false, 0, pending_population_transaction_);
    return;
  }
  if (format_ == ProofFormat::Lrat) {
    put_u64(id);
    for (uint32_t i = 0; i < size; i++)
      put_lit(lits[i]);
    fputs("0 ", file_);
    for (uint32_t i = 0; i < nchain; i++)
      put_u64(chain[i]);
    fputs("0\n", file_);
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    population_add(id, lits, size);
#endif
    observe_checker(CheckerPopulationEventKind::Add, role, id, id,
                    PopulationClauseView::internal_lits(lits, size,
                                                         variable_map_));
    return;
  }
  if (format_ == ProofFormat::DsrBinary)
    put_byte(1);
  else if (format_ == ProofFormat::DratBinary)
    put_byte('a');
  for (uint32_t i = 0; i < size; i++)
    put_lit(lits[i]);
  end_line();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  population_add(id, lits, size);
#endif
  observe_checker(CheckerPopulationEventKind::Add, role, id, id,
                  PopulationClauseView::internal_lits(lits, size,
                                                       variable_map_));
}

void Proof::add_rat(uint64_t id, const Lit *lits, uint32_t size,
                    const int64_t *hints, uint32_t nhints) {
  if (size && !premise_groups_.empty()) {
    invalidate_premises_for_sr(id, lits, 1, nullptr, 0, nullptr, 0);
    flush_persistent_external(id);
  }
  if (!input_transaction_ && !commit_dsr())
    std::abort();
  flush_pending_dsr_units();
#ifdef TACTIX_ENABLE_RULE_JOURNAL
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
    Lit pivot = lits[0];
    if (variable_map_) {
      assert(var_of(pivot) < variable_map_->size());
      pivot = make_lit((*variable_map_)[var_of(pivot)], negated(pivot));
    }
    const int32_t external = externalize(pivot);
    std::fprintf(file_, " : x%" PRIu32 " -> %d ;\n",
                 static_cast<uint32_t>(external < 0 ? -int64_t(external)
                                                    : int64_t(external)),
                 external < 0 ? 0 : 1);
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    population_add(id, lits, size);
#endif
    observe_checker(CheckerPopulationEventKind::Add,
                    CheckerClauseRole::Rat, id, id,
                    PopulationClauseView::internal_lits(lits, size,
                                                         variable_map_));
    return;
  }
  if (format_ == ProofFormat::Lrat) {
    put_u64(id);
    for (uint32_t i = 0; i < size; i++)
      put_lit(lits[i]);
    fputs("0 ", file_);
    for (uint32_t i = 0; i < nhints; i++)
      fprintf(file_, "%" PRId64 " ", hints[i]);
    fputs("0\n", file_);
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    population_add(id, lits, size);
#endif
    observe_checker(CheckerPopulationEventKind::Add,
                    CheckerClauseRole::Rat, id, id,
                    PopulationClauseView::internal_lits(lits, size,
                                                         variable_map_));
    return;
  }
  // DSR/DRAT: a witnessless addition with the pivot ordered first. The checker
  // first tries RUP and then infers first-literal RAT. DRAT output is unchanged.
  if (format_ == ProofFormat::DsrBinary)
    put_byte(1);
  else if (format_ == ProofFormat::DratBinary)
    put_byte('a');
  for (uint32_t i = 0; i < size; i++)
    put_lit(lits[i]);
  end_line();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  population_add(id, lits, size);
#endif
  observe_checker(CheckerPopulationEventKind::Add, CheckerClauseRole::Rat,
                  id, id,
                  PopulationClauseView::internal_lits(lits, size,
                                                       variable_map_));
}

bool Proof::add_sr(uint64_t id, const Lit *lits, uint32_t size,
                   const Lit *true_lits, uint32_t ntrue,
                   const DsrLiteralMapping *mappings, uint32_t nmappings,
                   uint32_t external_variables, bool propagates) {
  if (!input_transaction_ && !commit_dsr())
    return false;
#ifndef TACTIX_ENABLE_PROOF_SHADOW
  (void)id;
#endif
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
      if (variable_map_) {
        assert(var_of(literal) < variable_map_->size());
        literal = make_lit((*variable_map_)[var_of(literal)],
                           negated(literal));
      }
      return externalize(literal);
    };
    const Var pivot_var = var_of(lits[0]);
    bool pivot_assigned = false;
    for (uint32_t i = 0; i < ntrue; ++i)
      pivot_assigned |= var_of(true_lits[i]) == pivot_var;
    for (uint32_t i = 0; i < nmappings; ++i)
      pivot_assigned |= var_of(mappings[i].from) == pivot_var;
    const auto put_constant = [&](Lit source, bool value) {
      const int32_t external = external_lit(source);
      std::fprintf(file_, " x%" PRIu32 " -> %d",
                   static_cast<uint32_t>(external < 0 ? -int64_t(external)
                                                      : int64_t(external)),
                   value ? 1 : 0);
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
                   static_cast<uint32_t>(from < 0 ? -int64_t(from)
                                                  : int64_t(from)),
                   to < 0 ? "~" : "",
                   static_cast<uint32_t>(to < 0 ? -int64_t(to)
                                                : int64_t(to)));
    }
    std::fputs(" ;\n", file_);
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    population_add(id, lits, size);
#endif
    observe_checker(CheckerPopulationEventKind::Add,
                    CheckerClauseRole::SubstitutionRedundancy, id, id,
                    PopulationClauseView::internal_lits(lits, size,
                                                         variable_map_));
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
      if (format_ == ProofFormat::DsrBinary)
        put_byte(deletion ? 2 : 1);
      else if (deletion)
        fputs("d ", file_);
      put_external_lit(negative ? negate(auxiliary) : auxiliary);
      for (uint32_t i = 0; i < size; ++i)
        put_lit(lits[i]);
    };
    // The first clause is RAT on a fresh variable. Under the negation of
    // the second clause, the original X7 obligations hold; the first clause's
    // image is exactly the already-validated candidate image C|sigma.
    temporary(false, true);
    end_line();
    PopulationClauseView negative_helper =
        PopulationClauseView::internal_lits(lits, size, variable_map_);
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
    PopulationClauseView positive_helper =
        PopulationClauseView::internal_lits(lits, size, variable_map_);
    positive_helper.prefix = external_auxiliary;
    observe_checker(CheckerPopulationEventKind::Add,
                    CheckerClauseRole::DsrSubstitutionHelper, 0, 0,
                    positive_helper, true);
    if (format_ == ProofFormat::DsrBinary)
      put_byte(1);
    for (uint32_t i = 0; i < size; ++i)
      put_lit(lits[i]);
    end_line();
    observe_checker(CheckerPopulationEventKind::Add,
                    CheckerClauseRole::SubstitutionRedundancy, id, id,
                    PopulationClauseView::internal_lits(lits, size,
                                                         variable_map_));
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
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  population_add(id, lits, size);
#endif
    has_sr_witness_ = true;
    return true;
  }
  const Lit emitted_pivot = lits[plan.pivot];
  PopulationClauseView emitted_clause =
      PopulationClauseView::internal_lits(lits, size, variable_map_);
  emitted_clause.first_body_index = plan.pivot;
  const auto put_addition = [&](bool lemma) {
    if (format_ == ProofFormat::DsrBinary)
      put_byte(1);
    put_lit(emitted_pivot);
    for (uint32_t i = 0; i < size; ++i)
      if (i != plan.pivot)
        put_lit(lits[i]);
    if (lemma)
      put_lit(plan.lemma);
  };
  const auto put_lemma_deletion = [&] {
    if (format_ == ProofFormat::DsrBinary)
      put_byte(2);
    else
      fputs("d ", file_);
    put_lit(emitted_pivot);
    for (uint32_t i = 0; i < size; ++i)
      if (i != plan.pivot)
        put_lit(lits[i]);
    put_lit(plan.lemma);
    end_line();
  };

  if (plan.form == DsrWitnessForm::Lemma) {
    put_addition(true);
    end_line();
    Lit external_lemma = plan.lemma;
    if (variable_map_)
      external_lemma = make_lit((*variable_map_)[var_of(external_lemma)],
                                negated(external_lemma));
    PopulationClauseView lemma = emitted_clause;
    lemma.suffix = externalize(external_lemma);
    observe_checker(CheckerPopulationEventKind::Add,
                    CheckerClauseRole::DsrSubstitutionHelper, 0, 0, lemma,
                    true);
  }
  put_addition(false);
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
    put_lemma_deletion();
    Lit external_lemma = plan.lemma;
    if (variable_map_)
      external_lemma = make_lit((*variable_map_)[var_of(external_lemma)],
                                negated(external_lemma));
    PopulationClauseView lemma = emitted_clause;
    lemma.suffix = externalize(external_lemma);
    observe_checker(CheckerPopulationEventKind::DeleteRequested,
                    CheckerClauseRole::DsrSubstitutionHelper, 0, 0, lemma,
                    true);
  }
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  population_add(id, lits, size);
#endif
  return true;
}

void Proof::add_empty(uint64_t id, const uint64_t *chain, uint32_t nchain) {
  const CheckerClauseRole saved = population_role_;
  population_role_ = CheckerClauseRole::Empty;
  add_rup(id, nullptr, 0, chain, nchain);
  population_role_ = saved;
}

void Proof::add_derived_unit(uint64_t id, Lit unit, const uint64_t *chain,
                             uint32_t nchain) {
  if (!file_)
    return;
  if (format_ == ProofFormat::Lrat || streaming_pbp()) {
    const CheckerClauseRole saved = population_role_;
    population_role_ = CheckerClauseRole::DerivedUnit;
    add_rup(id, &unit, 1, chain, nchain);
    population_role_ = saved;
    return;
  }
  // Native DSR keeps root units explicit so they remain available after a
  // reason clause is deleted and later direct-SR checks see the same root
  // formula as dsr-trim/Trestle. DRAT retains its historical implicit units.
  if (dsr()) {
    const CheckerClauseRole saved = population_role_;
    population_role_ = CheckerClauseRole::DerivedUnit;
    add_rup(id, &unit, 1, nullptr, 0);
    population_role_ = saved;
  }
}

void Proof::del(uint64_t line_id, uint64_t del_id, const Lit *lits,
                uint32_t size) {
  if (!leased_premises_.empty() && size <= premise_maximum_width_) {
    std::vector<int32_t> external; external.reserve(size);
    for (uint32_t i = 0; i < size; ++i) {
      Lit lit = lits[i];
      if (variable_map_) lit = make_lit((*variable_map_)[var_of(lit)], negated(lit));
      external.push_back(externalize(lit));
    }
    if (defer_leased_deletion(external.data(), size)) return;
  }
  if (!input_transaction_ && !commit_dsr())
    std::abort();
  if (!file_)
    return;
  if (streaming_pbp()) {
    // RUP is monotone under retained constraints, but RAT/SR redundancy is
    // not. Delete one exact matching constraint without coupling PBP's own
    // numbering to Tactix clause IDs.
    if (streaming_pbp_finished_)
      return;
    std::vector<int32_t> external;
    external.reserve(size);
    for (uint32_t i = 0; i < size; ++i) {
      Lit literal = lits[i];
      if (variable_map_)
        literal = make_lit((*variable_map_)[var_of(literal)], negated(literal));
      external.push_back(externalize(literal));
    }
    std::vector<int32_t> canonical;
    if (!canonical_pbp_deletion(external.data(), external.size(), canonical))
      return;
    put_pbp_external_clause("del spec", canonical.data(), canonical.size());
    std::fputs(" ;\n", file_);
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    population_del(del_id, lits, size);
#endif
    observe_checker(CheckerPopulationEventKind::DeleteRequested,
                    CheckerClauseRole::None, line_id, del_id,
                    PopulationClauseView::internal_lits(lits, size,
                                                         variable_map_));
    return;
  }
  Lit external_unit = size == 1 ? lits[0] : INVALID_LIT;
  if (size == 1 && variable_map_)
    external_unit = make_lit((*variable_map_)[var_of(external_unit)],
                             negated(external_unit));
  if ((del_id && pending_unit_ids_.count(del_id)) ||
      (size == 1 && pending_unit_literals_.count(externalize(external_unit))))
    flush_pending_dsr_units();
  const bool defer = !pending_units_.empty();
  FILE *main_file = nullptr;
  if (defer) {
    flush_binary();
    if (binary())
      spooling_deletions_ = true;
    else {
      if (!pending_deletions_ && !(pending_deletions_ = std::tmpfile()))
        std::abort();
      main_file = file_;
      file_ = pending_deletions_;
    }
    for (uint32_t i = 0; i < size; ++i) {
      const Var external = variable_map_ ? (*variable_map_)[var_of(lits[i])]
                                          : var_of(lits[i]);
      pending_max_external_ = std::max(pending_max_external_, external + 1);
    }
  }
  if (format_ == ProofFormat::Lrat) {
    put_u64(line_id);
    fputs("d ", file_);
    put_u64(del_id);
    fputs("0\n", file_);
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    population_del(del_id, lits, size);
#endif
    observe_checker(CheckerPopulationEventKind::Delete,
                    CheckerClauseRole::None, line_id, del_id,
                    PopulationClauseView::internal_lits(lits, size,
                                                         variable_map_));
    return;
  }
  if (format_ == ProofFormat::DsrBinary)
    put_byte(2);
  else if (format_ == ProofFormat::DratBinary)
    put_byte('d');
  else
    fputc('d', file_);
  if (format_ == ProofFormat::DsrAscii ||
      format_ == ProofFormat::DratAscii)
    fputc(' ', file_);
  for (uint32_t i = 0; i < size; i++)
    put_lit(lits[i]);
  end_line();
  if (defer) {
    flush_binary();
    if (binary())
      spooling_deletions_ = false;
    else
      file_ = main_file;
  }
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  population_del(del_id, lits, size);
#endif
  const CheckerPopulationEventKind deletion_kind =
      defer ? CheckerPopulationEventKind::StageDeleteRequested
            : CheckerPopulationEventKind::DeleteRequested;
  observe_checker(deletion_kind,
                  CheckerClauseRole::None, line_id, del_id,
                  PopulationClauseView::internal_lits(lits, size,
                                                       variable_map_),
                  false, 0, defer ? pending_population_transaction_ : 0);
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

bool Proof::add_external_rup(uint64_t id, const int32_t *lits,
                             uint32_t size) {
  const CheckerClauseRole role =
      population_role_ == CheckerClauseRole::ExternalRat
          ? CheckerClauseRole::ExternalRat
          : CheckerClauseRole::ExternalRup;
  if (!input_transaction_ && !commit_dsr())
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
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    std::vector<Lit> external;
    external.reserve(size);
    for (uint32_t i = 0; i < size; ++i)
      external.push_back(internalize(lits[i]));
    const auto *saved = variable_map_;
    variable_map_ = nullptr;
    population_add(id, external.data(), size);
    variable_map_ = saved;
#endif
    observe_checker(CheckerPopulationEventKind::Add, role, id, id,
                    PopulationClauseView::external_lits(lits, size), true);
    if (!size)
      finish_streaming_pbp_unsat();
    return true;
  }
  if (format_ == ProofFormat::DsrBinary)
    put_byte(1);
  else if (format_ == ProofFormat::DratBinary)
    put_byte('a');
  for (uint32_t i = 0; i < size; ++i) {
    proof_external_max_ = std::max(proof_external_max_,
                                   static_cast<uint32_t>(
                                       lits[i] < 0 ? -int64_t(lits[i]) : lits[i]));
    put_external_dimacs(lits[i]);
  }
  end_line();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  std::vector<Lit> external;
  external.reserve(size);
  for (uint32_t i = 0; i < size; ++i)
    external.push_back(internalize(lits[i]));
  const auto *saved = variable_map_;
  variable_map_ = nullptr;
  population_add(id, external.data(), size);
  variable_map_ = saved;
#else
  (void)id;
#endif
  observe_checker(CheckerPopulationEventKind::Add, role, id, id,
                  PopulationClauseView::external_lits(lits, size), true);
  return true;
}

bool Proof::add_external_rat(uint64_t id, const int32_t *lits,
                             uint32_t size) {
  if (size && !premise_groups_.empty()) {
    const auto *saved = variable_map_; variable_map_ = nullptr;
    const Lit pivot = internalize(lits[0]);
    const uint64_t before = premise_epoch_;
    invalidate_premises_for_sr(id, &pivot, 1, nullptr, 0, nullptr, 0);
    variable_map_ = saved;
    if (premise_epoch_ != before) flush_persistent_external(id);
  }
  if (!input_transaction_ && !commit_dsr())
    return false;
  if (streaming_pbp()) {
    if (!size || !lits || streaming_pbp_finished_)
      return false;
    put_pbp_external_clause("red", lits, size);
    const int32_t pivot = lits[0];
    std::fprintf(file_, " : x%" PRIu32 " -> %d ;\n",
                 static_cast<uint32_t>(pivot < 0 ? -int64_t(pivot)
                                                 : int64_t(pivot)),
                 pivot < 0 ? 0 : 1);
    observe_checker(CheckerPopulationEventKind::Add,
                    CheckerClauseRole::ExternalRat, id, id,
                    PopulationClauseView::external_lits(lits, size), true);
    return true;
  }
  // DSR/DRAT infer RAT from the first literal after RUP fails, so their byte
  // stream is identical to an external RUP addition.
  const CheckerClauseRole saved = population_role_;
  population_role_ = CheckerClauseRole::ExternalRat;
  const bool result = add_external_rup(id, lits, size);
  population_role_ = saved;
  return result;
}

namespace {
std::vector<int32_t> premise_key(const int32_t *lits, uint32_t size) {
  std::vector<int32_t> result(lits, lits + size);
  std::sort(result.begin(), result.end());
  return result;
}
}

bool Proof::can_defer_premise_group_deletions(
    const std::vector<PremiseGroup> &groups, DeferredDeletionLimits incoming,
    const std::function<bool(uint64_t)> &charge) const {
  if (!charge || lrat() || (pb() && !streaming_pbp()) || !file_) return false;
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

bool Proof::retain_premise_groups(const std::vector<PremiseGroup> &groups,
    uint64_t maximum_groups, uint64_t maximum_literals,
    uint64_t maximum_bytes, const std::function<bool(uint64_t)> &charge,
    std::vector<uint64_t> &handles, DeferredDeletionLimits limits) {
  const bool bounded = limits.maximum_pending_deletions || limits.maximum_pending_literal_work;
  if (bounded && (!limits.maximum_pending_deletions || !limits.maximum_pending_literal_work ||
      premise_pending_deletions_ > limits.maximum_pending_deletions ||
      premise_pending_literal_work_ > limits.maximum_pending_literal_work)) return false;
  if (!charge || lrat() || (pb() && !streaming_pbp()) || !file_ ||
      groups.size() > maximum_groups ||
      premise_groups_.size() > maximum_groups - groups.size() ||
      groups.size() > UINT64_MAX - next_premise_group_)
    return false;
  // Include both old and transactional candidate populations in peak memory.
  uint64_t literals = premise_literals_, bytes = premise_bytes_;
  for (const auto &group : groups) {
    if (group.empty() || !charge(1 + group.size())) return false;
    for (const auto &clause : group) {
      const uint64_t storage = (4 * sizeof(PremiseGroup) + 16 * sizeof(void *) + sizeof(LeasedPremise)) + 2 * uint64_t(clause.size()) * sizeof(int32_t);
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
  // Durable clausal ownership commits an Automatic route only after every
  // resource/identity check has passed. No lease can survive a PB transition.
  if (input_transaction_ || !commit_dsr()) return false;
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
  premise_maintenance_work_ = work > UINT64_MAX - premise_maintenance_work_ ? UINT64_MAX :
                              premise_maintenance_work_ + work;
  const auto found = leased_premises_.find(premise_key(lits, size));
  if (found == leased_premises_.end()) return false;
  const uint64_t cost = 1 + uint64_t(size);
  if (premise_pending_deletions_ >= premise_pending_deletion_cap_ ||
      cost > premise_pending_work_cap_ - std::min(premise_pending_literal_work_, premise_pending_work_cap_)) {
    const __uint128_t scan = __uint128_t(1 + premise_groups_.size()) +
        __uint128_t(1 + size) * (premise_literals_ + premise_groups_.size());
    premise_maintenance_work_ = scan > UINT64_MAX - premise_maintenance_work_ ? UINT64_MAX :
                                premise_maintenance_work_ + uint64_t(scan);
    std::vector<uint64_t> affected;
    for (const auto &[handle, group] : premise_groups_)
      if (std::find(group.begin(), group.end(), found->first) != group.end()) affected.push_back(handle);
    release_premise_groups(affected, 0);
    return false;
  }
  ++premise_pending_deletions_; premise_pending_literal_work_ += cost;
  if (found->second.deferred == UINT64_MAX || premise_deferred_ == UINT64_MAX)
    std::abort();
  ++found->second.deferred; ++premise_deferred_;
  return true;
}

void Proof::release_premise_groups(const std::vector<uint64_t> &handles,
                                  uint64_t line_id) {
  bool changed = false;
  for (uint64_t handle : handles) {
    auto group = premise_groups_.find(handle);
    if (group == premise_groups_.end()) continue;
    const uint64_t maintenance = 1 + group->second.size();
    premise_maintenance_work_ = maintenance > UINT64_MAX - premise_maintenance_work_ ? UINT64_MAX :
                                premise_maintenance_work_ + maintenance;
    PremiseGroup clauses = std::move(group->second);
    premise_groups_.erase(group); changed = true;
    for (const auto &clause : clauses) {
      auto found = leased_premises_.find(clause);
      if (found == leased_premises_.end() || !found->second.users) std::abort();
      premise_literals_ -= clause.size();
      premise_bytes_ -= (4 * sizeof(PremiseGroup) + 16 * sizeof(void *) + sizeof(LeasedPremise)) + 2 * uint64_t(clause.size()) * sizeof(int32_t);
      if (--found->second.users) continue;
      const uint64_t deferred = found->second.deferred;
      const uint64_t scan = uint64_t(clause.size()) *
          (2 * std::bit_width(leased_premises_.size()) + 3);
      const __uint128_t serialization = __uint128_t(deferred) * (1 + clause.size()) + scan;
      premise_maintenance_work_ = serialization > UINT64_MAX - premise_maintenance_work_ ? UINT64_MAX :
                                  premise_maintenance_work_ + uint64_t(serialization);
      premise_pending_deletions_ -= deferred;
      premise_pending_literal_work_ -= deferred * (1 + uint64_t(clause.size()));
      leased_premises_.erase(found);
      for (uint64_t i = 0; i < deferred; ++i)
        if (!del_external(line_id, 0, clause.data(), clause.size())) std::abort();
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
  premise_maintenance_work_ = work > UINT64_MAX - premise_maintenance_work_ ? UINT64_MAX :
                              premise_maintenance_work_ + work;
  std::vector<uint32_t> changed;
  const auto take = [&](Lit lit) {
    changed.push_back(variable_map_ ? (*variable_map_)[var_of(lit)] + 1
                                    : var_of(lit) + 1);
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
                 uint32_t(literal < 0 ? -int64_t(literal) : literal));
    if (touched) retire.push_back(handle);
  }
  release_premise_groups(retire, line_id);
}

uint64_t Proof::persistent_external_bytes() const {
  uint64_t bytes = persistent_external_.capacity() * sizeof(PersistentExternalClause);
  for (const auto &clause : persistent_external_)
    bytes += clause.lits.capacity() * sizeof(int32_t);
  return bytes;
}

bool Proof::retain_external(uint64_t id, const int32_t *lits, uint32_t size) {
  if (!id || !size || !lits ||
      external_tautological(lits, size) || lrat() ||
      (pb() && !streaming_pbp()))
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
  if (lrat() || (pb() && !streaming_pbp()) ||
      persistent_external_epoch_ == UINT64_MAX)
    std::abort();
  std::vector<PersistentExternalClause> clauses =
      std::move(persistent_external_);
  persistent_external_.clear();
  for (auto clause = clauses.rbegin(); clause != clauses.rend(); ++clause)
    if (!del_external(line_id, clause->id, clause->lits.data(),
                      clause->lits.size()))
      std::abort();
  ++persistent_external_epoch_;
}

bool Proof::del_external(uint64_t line_id, uint64_t del_id,
                         const int32_t *lits, uint32_t size) {
  if (defer_leased_deletion(lits, size)) return true;
  if (!input_transaction_ && !commit_dsr())
    return false;
  assert(!size || lits);
  if (!file_)
    return true;
  if (streaming_pbp()) {
    if (streaming_pbp_finished_)
      return true;
    std::vector<int32_t> canonical;
    if (!canonical_pbp_deletion(lits, size, canonical))
      return true;
    put_pbp_external_clause("del spec", canonical.data(), canonical.size());
    std::fputs(" ;\n", file_);
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    std::vector<Lit> external;
    external.reserve(size);
    for (uint32_t i = 0; i < size; ++i)
      external.push_back(internalize(lits[i]));
    const auto *saved = variable_map_;
    variable_map_ = nullptr;
    population_del(del_id, external.data(), size);
    variable_map_ = saved;
#endif
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
  FILE *main_file = nullptr;
  if (defer) {
    flush_binary();
    if (binary())
      spooling_deletions_ = true;
    else {
      if (!pending_deletions_ && !(pending_deletions_ = std::tmpfile()))
        std::abort();
      main_file = file_;
      file_ = pending_deletions_;
    }
  }
  if (format_ == ProofFormat::DsrBinary)
    put_byte(2);
  else if (format_ == ProofFormat::DratBinary)
    put_byte('d');
  else
    fputs("d ", file_);
  for (uint32_t i = 0; i < size; ++i) {
    const uint32_t variable = static_cast<uint32_t>(
        lits[i] < 0 ? -int64_t(lits[i]) : lits[i]);
    proof_external_max_ = std::max(proof_external_max_, variable);
    if (defer)
      pending_max_external_ = std::max(pending_max_external_, variable);
    put_external_dimacs(lits[i]);
  }
  end_line();
  if (defer) {
    flush_binary();
    if (binary())
      spooling_deletions_ = false;
    else
      file_ = main_file;
  }
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  std::vector<Lit> external;
  external.reserve(size);
  for (uint32_t i = 0; i < size; ++i)
    external.push_back(internalize(lits[i]));
  const auto *saved = variable_map_;
  variable_map_ = nullptr;
  population_del(del_id, external.data(), size);
  variable_map_ = saved;
#else
  (void)del_id;
#endif
  observe_checker(defer ? CheckerPopulationEventKind::StageDeleteRequested
                         : CheckerPopulationEventKind::DeleteRequested,
                  CheckerClauseRole::None, line_id, del_id,
                  PopulationClauseView::external_lits(lits, size), true, 0,
                  defer ? pending_population_transaction_ : 0);
  return true;
}

void Proof::flush() {
  if (automatic_pending_)
    commit_dsr();
  flush_pending_dsr_units();
  if (file_) {
    flush_binary();
    // Text serializers use stdio buffering. Both a previous short write and
    // this publication flush must succeed before a caller may report UNSAT.
    if (std::ferror(file_) || std::fflush(file_))
      std::abort();
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
  for (uint32_t i = 0; i < size; i++) {
    Lit l = lits[i];
    if (variable_map_)
      l = make_lit((*variable_map_)[var_of(l)], negated(l));
    external.push_back(externalize(l));
  }
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
  for (uint32_t i = 0; i < size; i++) {
    Lit l = lits[i];
    if (variable_map_)
      l = make_lit((*variable_map_)[var_of(l)], negated(l));
    fprintf(stderr, " %d", externalize(l));
  }
  fputs(" 0\n", stderr);
  void *frames[32];
  const int depth = backtrace(frames, 32);
  backtrace_symbols_fd(frames, depth, fileno(stderr));
  fflush(stderr);
  // Abort on the FIRST one: the emitting rule is what this build exists to
  // name, and every later line is downstream of an already-diverged checker.
  std::abort();
}

#endif

void Proof::seed_population(uint64_t id, const Lit *lits, uint32_t size) {
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  population_add(id, lits, size);
#endif
  if (!file_)
    return;
  // VeriPB's `f` statement names the number of constraints in the original
  // CNF, not the largest internal clause id.  Streaming PBP may derive and
  // spool root units while input parsing is still in progress; those derived
  // ids can therefore make later raw-input ids non-contiguous.  Count each
  // raw seed exactly once instead of inferring the input population from its
  // internal identity namespace.
  if (streaming_pbp() && input_transaction_)
    ++pbp_original_constraints_;
  observe_checker(CheckerPopulationEventKind::InputSeed,
                  CheckerClauseRole::Input, id, id,
                  PopulationClauseView::internal_lits(lits, size,
                                                       variable_map_));
}

} // namespace tactix
