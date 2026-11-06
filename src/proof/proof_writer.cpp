// The proof writer: every byte of every proof format. The ledger (proof.cpp)
// decides each event; the bodies here only spell it.
#include "proof/proof_writer.hpp"
#include "kernel/types.hpp"
#include "proof/dsr_to_pbp.hpp"
#include "rules/lucky/parity_certificate.hpp"

#include <algorithm>
#include <cstdarg>
#include <cassert>
#include <charconv>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <utility>

#if defined(__GLIBC__) || defined(__linux__)
#include <stdio_ext.h>
#endif
#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>
#ifdef TACTIX_ENABLE_PROOF_SHADOW
#include <execinfo.h>
#endif

namespace tactix {

namespace {
// The largest variable name a DSR line carries. The pinned drat-trim and
// dsr-trim decode a binary literal, (name << 1) | sign, into a signed int, and
// dsr-trim indexes a literal of either spelling by twice its name.
constexpr uint32_t MAX_CHECKER_NAME = (uint32_t{1} << 30) - 1;
[[noreturn]] void checker_name_error() {
  proof_error("the DSR proof needs a variable name above 2^30-1, the largest "
              "drat-trim and dsr-trim read");
}
} // namespace

namespace {

class PbpTextWriter {
public:
  PbpTextWriter(FILE *file, bool wanted) : file_(file), wanted_(wanted) {
    if (!file_)
      std::abort();
  }

  void put(std::string_view text) {
    if (!wanted_)
      return;
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
    if (!wanted_)
      return;
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
  bool wanted_;
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

// Append every remaining byte of `source` to `target`; false on a failed
// read or write.
// The streaming-PBP header; publication and lazy materialization share it so
// both spell the same bytes.
int write_pbp_header(FILE *file, uint64_t constraints) {
  return std::fprintf(file, "pseudo-Boolean proof version 3.0\nf %" PRIu64 ";\n",
                      constraints);
}

constexpr const char *kPbpConclusion =
    "output NONE ;\nconclusion UNSAT : -1 ;\nend pseudo-Boolean proof ;\n";

bool copy_stream(FILE *source, FILE *target) {
  std::array<unsigned char, 65536> bytes{};
  for (;;) {
    const size_t count = std::fread(bytes.data(), 1, bytes.size(), source);
    if (count && std::fwrite(bytes.data(), 1, count, target) != count)
      return false;
    if (count < bytes.size())
      return !std::ferror(source);
  }
}

} // namespace

namespace {
// What the writer's in-process stream does with its bytes.
struct LazySinkTarget {
  // An event the DSR copy may decline after the writer accepted it: its text
  // is kept until the copy answers, since a switch must write it after the
  // converted prefix (see ProofWriter::hold_lazy_event).
  bool holding = false;
  std::string held;
  size_t accept(const char *data, size_t size) {
    if (holding)
      held.append(data, size);
    return size;
  }
};
} // namespace

// Lazy PBP holder (see ProofWriter::lazy_); the destructor is defined with the
// automatic-selection code.
struct ProofWriter::LazyPbp : LazySinkTarget {
  FILE *sink = nullptr;  // the writer's file_: stores only a held event
  uint64_t derived_at_hold = 0;
  int stage_fd = -1;     // reads the copy's stage for the conversion
  ~LazyPbp();
};

CheckerRoute ProofWriter::population_route() const noexcept {
  if (!file_)
    return CheckerRoute::None;
  switch (format_) {
  case ProofFormat::DsrBinary:
  case ProofFormat::DsrAscii:
    return CheckerRoute::Dsr;
  case ProofFormat::Pbp:
  case ProofFormat::PbpStreaming:
    return CheckerRoute::Pbp;
  }
  return CheckerRoute::None;
}

CheckerNaming ProofWriter::population_naming() const noexcept {
  return population_route() == CheckerRoute::Dsr
             ? CheckerNaming::LiteralMultiset
             : CheckerNaming::None;
}

void ProofWriter::observe_checker(CheckerPopulationEventKind kind,
                            CheckerClauseRole role, uint64_t line_id,
                            uint64_t checker_id,
                            PopulationClauseView clause, bool proof_only,
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

ProofWriter::ProofWriter() = default;

std::unique_ptr<ProofWriter> ProofWriter::pbp(FILE *destination) {
  if (!destination)
    return nullptr;
  FILE *const transaction = std::tmpfile();
  if (!transaction) {
    std::fclose(destination);
    return nullptr;
  }
  std::unique_ptr<ProofWriter> writer(new ProofWriter);
  writer->format_ = ProofFormat::PbpStreaming;
  writer->pbp_output_ = destination;
  writer->file_ = transaction;
  writer->input_transaction_ = true;
  setvbuf(transaction, nullptr, _IOFBF, 1 << 20);
  return writer;
}

std::unique_ptr<ProofWriter> ProofWriter::dsr(FILE *file, ProofFormat format,
                                              bool copy_role) {
  if (!file)
    return nullptr;
  std::unique_ptr<ProofWriter> writer(new ProofWriter);
  writer->format_ = format;
  writer->file_ = file;
  writer->copy_role_ = copy_role;
  setvbuf(file, nullptr, writer->binary() ? _IONBF : _IOFBF, 1 << 20);
  return writer;
}

ProofWriter::~ProofWriter() {
  if (file_ && std::fclose(file_) && !copy_role_)
    std::abort();
  if (pbp_output_ && std::fclose(pbp_output_))
    std::abort();
  if (lazy_destination_ && std::fclose(lazy_destination_))
    std::abort();
  if (follow_on_destination_) // no follow-on ran: nothing was written
    std::fclose(follow_on_destination_);
  if (pending_deletions_)
    std::fclose(pending_deletions_);
}

void ProofWriter::opened() {
  observe_checker(CheckerPopulationEventKind::RouteOpened,
                  CheckerClauseRole::None, 0, 0, {});
  observe_checker(CheckerPopulationEventKind::RouteCommitted,
                  CheckerClauseRole::None, 0, 0, {});
}

// A run that never reached the empty clause never converts to PBP: it leaves
// the DSR copy's prefix (an unfinished run has no certificate). If the prefix
// cannot be placed, a held destination is closed empty here, so no later flush
// tries to publish a header for a stream that no longer exists. An unfinished
// run never aborts over a proof no verdict rests on.
void ProofWriter::teardown_copy() {
  const bool held = lazy_ != nullptr;
  if (copy_)
    select_copy(false);
  if (!held)
    return;
  if (pbp_output_)
    (void)std::fclose(pbp_output_);
  if (lazy_destination_)
    (void)std::fclose(lazy_destination_);
  pbp_output_ = lazy_destination_ = nullptr;
}

void ProofWriter::check_lifecycle([[maybe_unused]] bool input,
                                  [[maybe_unused]] bool published,
                                  [[maybe_unused]] bool terminal,
                                  [[maybe_unused]] bool finished) const {
  if (dsr())
    return;
  assert(input == input_transaction_);
  assert(terminal == (format_ == ProofFormat::Pbp || refuted_in_clauses_));
  // A selected copy leaves this stream without a file.
  assert(terminal || !file_ || published == !pbp_output_);
  assert(terminal || !file_ || finished == streaming_pbp_finished_);
}

void ProofWriter::finish_input() {
  observe_checker(CheckerPopulationEventKind::InputFinished,
                  CheckerClauseRole::Input, 0, 0, {});
  // A restated input whose raw original was never deleted leaves both
  // constraints live under one Tactix id; exact deletion cannot name either.
  if (pbp_pending_rebind_.id)
    abandon_pbp_id_deletions(5, 0);
  pbp_last_input_live_ = false;
  input_transaction_ = false;
}

// Never converted, so nothing replaces or rereads the destination: any will
// do, a pipe included. The prefix moves once, unchanged, and the copy then
// writes there directly. A failure is a failed write of the only proof, which
// settle_copy and select_copy stop on before any verdict.
void ProofWriter::publish_dsr_only_prefix() {
  ProofWriter &copy = *copy_->writer_;
  copy.flush_binary();
  FILE *const destination = lazy_destination_ ? lazy_destination_ : pbp_output_;
  const int descriptor = copy.copy_failed_ ? -1 : dup(fileno(destination));
  FILE *const direct = descriptor >= 0 ? fdopen(descriptor, "wb") : nullptr;
  if (!direct) {
    if (descriptor >= 0)
      close(descriptor);
    copy.copy_failed_ = true;
    return;
  }
  if (setvbuf(direct, nullptr, copy.binary() ? _IONBF : _IOFBF, 1 << 20) ||
      std::fflush(copy.file_) || std::ferror(copy.file_) ||
      std::fseek(copy.file_, 0, SEEK_SET) || !copy_stream(copy.file_, direct) ||
      std::fflush(direct))
    copy.copy_failed_ = true;
  std::fclose(copy.file_);
  copy.file_ = direct;
  dsr_in_destination_ = true;
}

FILE *ProofWriter::switch_to_pb() {
  // The staged input transaction was never published. Its facts are justified
  // afresh from original CNF by the complete, prevalidated PB derivation.
  observe_checker(CheckerPopulationEventKind::ClausalRouteDiscarded,
                  CheckerClauseRole::None, 0, 0, {});
  if (lazy_) {
    // The private prefix was never written: the refutation starts from the CNF.
    discard_lazy_pbp();
    dsr_is_the_proof_ = false;
    lazy_report_.state = "startup-pb";
  } else {
    std::fclose(file_);
  }
  binary_buffer_size_ = 0;
  file_ = pbp_output_;
  pbp_output_ = nullptr;
  format_ = ProofFormat::Pbp;
  observe_checker(CheckerPopulationEventKind::RouteCommitted,
                  CheckerClauseRole::None, 0, 0, {});
  return file_;
}

bool ProofWriter::pbp_text_wanted() const noexcept {
  return !lazy_ || file_ != lazy_->sink || lazy_->holding;
}

void ProofWriter::put_pbp_text(const char *format, ...) {
  if (!pbp_text_wanted())
    return;
  va_list arguments;
  va_start(arguments, format);
  std::vfprintf(file_, format, arguments);
  va_end(arguments);
}

bool ProofWriter::finish_pb() {
  return pb() && file_ && !std::ferror(file_) && std::fflush(file_) == 0;
}

void ProofWriter::publish() {
  if (!streaming_pbp() || !pbp_output_ || !file_)
    return;
  if (lazy_) {
    // The header is written at the switch, with the f fixed now.
    lazy_f_ = pbp_original_constraints_;
    lazy_published_ = true;
    lazy_destination_ = pbp_output_;
    pbp_output_ = nullptr;
    if (dsr_only_)
      publish_dsr_only_prefix();
    return;
  }
  if (std::fflush(file_) || std::fseek(file_, 0, SEEK_SET))
    std::abort();
  if (write_pbp_header(pbp_output_, pbp_original_constraints_) < 0)
    std::abort();
  if (!copy_stream(file_, pbp_output_))
    std::abort();
  std::fclose(file_);
  file_ = pbp_output_;
  pbp_output_ = nullptr;
}

void ProofWriter::finish_streaming_pbp_unsat() {
  if (!streaming_pbp() || streaming_pbp_finished_)
    return;
  std::fputs(kPbpConclusion, file_);
  streaming_pbp_finished_ = true;
}

void ProofWriter::put_byte(unsigned char byte) {
  binary_buffer_[binary_buffer_size_++] = byte;
  if (binary_buffer_size_ == binary_buffer_.size())
    flush_binary();
}

void ProofWriter::flush_binary() {
  if (copy_failed_) { // a failed automatic copy writes nothing further
    binary_buffer_size_ = 0;
    pending_deletion_bytes_.clear();
    return;
  }
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
      if (!copy_role_)
        std::abort();
      copy_failed_ = true;
      binary_buffer_size_ = 0;
      pending_deletion_bytes_.clear();
      return;
    }
    pending_deletion_bytes_.clear();
  }
  FILE *destination = spooling_deletions_ ? pending_deletions_ : file_;
  const size_t written =
      fwrite(binary_buffer_.data(), 1, binary_buffer_size_, destination);
  if (written != binary_buffer_size_) {
    if (!copy_role_)
      std::abort();
    copy_failed_ = true; // the owner discards this copy (settle_copy)
  }
  binary_buffer_size_ = 0;
}

// Binary DSR literal encoding: map external literal e
// to unsigned u = (|e| << 1) | (e < 0), then 7-bit varint chunks,
// high bit = continuation.
void ProofWriter::put_lit(Lit l) { put_external_lit(checker_lit(l)); }

void ProofWriter::put_external_lit(Lit l) {
  put_external_dimacs(externalize(l));
}

void ProofWriter::put_external_dimacs(int32_t e) {
  assert(e && e != INT32_MIN);
  const int32_t name = static_cast<int32_t>(checker_variable(external_variable_of(e)));
  put_checker_literal(e < 0 ? -name : name);
}

void ProofWriter::put_checker_literal(int32_t e) {
  uint32_t u = (external_variable_of(e) << 1) | (e < 0);
  uint64_t bytes = 1;
  if (binary()) {
    for (; u >= 128; u >>= 7, ++bytes)
      put_byte(static_cast<unsigned char>(128 | (u & 127)));
    put_byte(static_cast<unsigned char>(u));
  } else {
    for (; u >= 128; u >>= 7)
      ++bytes;
    fprintf(file_, "%d ", e);
  }
  if (copy_role_)
    dsr_bytes_written_ += bytes;
}

const int32_t *ProofWriter::pbp_checker_literals(const Lit *lits, uint32_t size) {
  pbp_external_scratch_.clear();
  pbp_external_scratch_.reserve(size);
  for (uint32_t i = 0; i < size; ++i)
    pbp_external_scratch_.push_back(externalize(checker_lit(lits[i])));
  return pbp_external_scratch_.data();
}

void ProofWriter::put_pbp_clause(const char *rule, const Lit *lits,
                           uint32_t size) {
  if (!pbp_text_wanted())
    return;
  PbpTextWriter text(file_, true);
  text.put(rule);
  text.put(" ");
  for (uint32_t i = 0; i < size; ++i)
    put_pbp_literal(text, externalize(checker_lit(lits[i])));
  text.put(">= 1");
  text.flush();
}

void ProofWriter::put_pbp_external_clause(const char *rule, const int32_t *lits,
                                    uint32_t size) {
  if (!pbp_text_wanted())
    return;
  PbpTextWriter text(file_, true);
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

void ProofWriter::abandon_pbp_id_deletions(uint32_t reason, uint32_t width) {
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
bool ProofWriter::pbp_input_constraint(uint64_t id, uint64_t &constraint,
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

bool ProofWriter::pbp_input_deleted(uint64_t constraint) const {
  return constraint < pbp_input_deleted_.size() &&
         pbp_input_deleted_[constraint];
}

void ProofWriter::mark_pbp_input_deleted(uint64_t constraint) {
  if (constraint >= pbp_input_deleted_.size())
    pbp_input_deleted_.resize(
        std::max<uint64_t>(constraint + 1, 2 * pbp_input_deleted_.size()));
  pbp_input_deleted_[constraint] = true;
}

// The ID the next derived constraint receives: absolute after input, or a
// transaction-relative ordinal while the `f` count is still unknown.
bool ProofWriter::pbp_current_ordinal(uint64_t &constraint) const {
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
bool ProofWriter::pbp_absolute_constraint(uint64_t stored,
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

void ProofWriter::track_pbp_original_constraint(uint64_t id, uint32_t clause_size,
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

void ProofWriter::track_pbp_derived_constraint(uint64_t id, uint32_t clause_size) {
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

size_t ProofWriter::PbpLiteralKeyHash::operator()(
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
void ProofWriter::register_pbp_anonymous(const int32_t *lits, uint32_t size) {
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
void ProofWriter::note_pbp_derived(uint64_t id, uint32_t clause_size,
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

bool ProofWriter::put_pbp_deletion_id(uint64_t id) {
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
  put_pbp_text("del id %" PRIu64 " ;\n", constraint);
  pbp_last_deleted_ = constraint;
  ++pbp_counters_.by_id;
  return true;
}

// Delete the raw original of a pending restatement by its exact input
// constraint, then let its id name the replacement.
bool ProofWriter::put_pbp_rebind_deletion() {
  const PbpPendingRebind rebind = pbp_pending_rebind_;
  pbp_pending_rebind_ = {};
  pbp_last_input_live_ = false;
  pbp_constraint_ids_.insert_or_assign(rebind.id, rebind.replacement);
  mark_pbp_input_deleted(rebind.original);
  put_pbp_text("del id %" PRIu64 " ;\n", rebind.original);
  pbp_last_deleted_ = rebind.original;
  ++pbp_counters_.by_id;
  ++pbp_counters_.by_rebind;
  return true;
}

// `canonical` is the VeriPB-canonical clause of a deletion whose own id named
// nothing. Delete the newest anonymous constraint with exactly these literals.
bool ProofWriter::put_pbp_anonymous_deletion(const int32_t *canonical,
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
  put_pbp_text("del id %" PRIu64 " ;\n", constraint);
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
bool ProofWriter::put_pbp_deletion(uint64_t del_id, uint32_t size,
                                   External external, bool replaying) {
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
    if (!replaying &&
        put_pbp_anonymous_deletion(pbp_canonical_scratch_.data(), width))
      return audited();
    abandon_pbp_id_deletions(1, width);
  }
  put_pbp_external_clause("del spec", pbp_canonical_scratch_.data(), width);
  put_pbp_text(" ;\n");
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

void ProofWriter::pbp_audit_record(bool input, uint64_t index, const int32_t *lits,
                             uint32_t size) {
  (input ? pbp_audit_input_ : pbp_audit_derived_)[index] =
      pbp_audit_key(lits, size);
}

void ProofWriter::pbp_audit_deletion(uint64_t constraint, const int32_t *lits,
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

void ProofWriter::end_line() {
  if (binary())
    put_byte(0);
  else
    fputs("0\n", file_);
  if (copy_role_)
    ++dsr_bytes_written_;
}

void ProofWriter::put_line_tag(bool deletion) {
  // In the copy: the ordinals of witness-free RAT additions, which the DSR
  // text does not distinguish from RUP.
  if (copy_role_ && !deletion) {
    if (next_addition_rat_) {
      if (keeps_rat_additions_)
        rat_additions_.push_back(additions_);
      next_addition_rat_ = false;
    }
    ++additions_;
  }
  if (copy_role_)
    ++dsr_bytes_written_;
  if (format_ == ProofFormat::DsrBinary)
    put_byte(deletion ? 2 : 1);
  else if (deletion) {
    fputc('d', file_);
    if (format_ == ProofFormat::DsrAscii)
      fputc(' ', file_);
  }
}

void ProofWriter::put_clausal_line(bool deletion, const Lit *lits, uint32_t size) {
  put_line_tag(deletion);
  for (uint32_t i = 0; i < size; i++)
    put_lit(lits[i]);
  end_line();
}

uint32_t ProofWriter::external_variable_bound() const {
  uint32_t bound = std::max(pending_max_external_, proof_external_max_);
  if (external_variable_map_)
    bound = std::max(bound,
                     static_cast<uint32_t>(external_variable_map_->size()));
  else if (variable_map_)
    for (const Var external : *variable_map_)
      bound = std::max(bound, external + 1);
  return bound;
}

void ProofWriter::flush_pending_dsr_units() {
  if (copy_failed_) { // the owner discards this copy; drop its staged batch
    pending_units_.clear();
    pending_unit_literals_.clear();
    pending_unit_ids_.clear();
    pending_deletion_bytes_.clear();
    spooling_deletions_ = false;
    if (pending_deletions_)
      std::fclose(pending_deletions_);
    pending_deletions_ = nullptr;
    return;
  }
  if (pending_units_.empty())
    return;
  assert(file_ && dsr());
  if (!pending_population_transaction_)
    pending_population_transaction_ = ++next_population_transaction_;
  observe_checker(CheckerPopulationEventKind::PendingCommitBegin,
                  CheckerClauseRole::None, 0, 0, {}, false,
                  pending_population_transaction_);
  const bool split = (pending_deletions_ || !pending_deletion_bytes_.empty()) &&
                     checker_name(uint64_t{external_variable_bound()} + 1) <=
                         MAX_CHECKER_NAME;
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
                      PopulationClauseView::external_lits(helper, 2), true, pending_population_transaction_);
    }
  };
  const auto direct_unit = [&](const PendingDsrUnit &unit) {
    emit(unit.external, false, 0);
    observe_checker(CheckerPopulationEventKind::Add, unit.role, unit.id,
                    unit.id,
                    PopulationClauseView::external_lits(&unit.external, 1),
                    false, pending_population_transaction_);
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
      const bool copied = !std::fflush(pending_deletions_) &&
                          !std::fseek(pending_deletions_, 0, SEEK_SET) &&
                          copy_stream(pending_deletions_, file_);
      const bool closed = !std::fclose(pending_deletions_);
      pending_deletions_ = nullptr;
      if (!copied || !closed) {
        if (!copy_role_)
          std::abort();
        copy_failed_ = true; // the owner discards this copy (settle_copy)
      }
    }
    observe_checker(CheckerPopulationEventKind::PendingDeletesCommit,
                    CheckerClauseRole::None, 0, 0, {}, false,
                    pending_population_transaction_);
  }
  if (split) {
    for (const auto &unit : pending_units_)
      direct_unit(unit);
    for (const auto &unit : pending_units_)
      split_helpers(unit, true);
  }
  observe_checker(CheckerPopulationEventKind::PendingCommitEnd,
                  CheckerClauseRole::None, 0, 0, {}, false,
                  pending_population_transaction_);
  // Erase exactly this batch's keys: clear() zeroes the whole bucket array,
  // which keeps the size of the largest batch ever staged (input units), and
  // made every later small flush pay for it.
  for (const PendingDsrUnit &unit : pending_units_) {
    pending_unit_literals_.erase(unit.external);
    if (unit.id)
      pending_unit_ids_.erase(unit.id);
  }
  assert(pending_unit_literals_.empty() && pending_unit_ids_.empty());
  pending_units_.clear();
  pending_max_external_ = 0;
  pending_population_transaction_ = 0;
}

void ProofWriter::rup(uint64_t id, const Lit *lits, uint32_t size,
                      CheckerClauseRole role) {
  // A new unit extends a consecutive batch. A deletion block or a nonunit
  // addition ends it, so later RUP checks still see the earlier units.
  if (!dsr() || size != 1 || pending_deletions_ ||
      !pending_deletion_bytes_.empty())
    flush_pending_dsr_units();
  if (!file_)
    return;
  // A native-DSR unit is staged into the pending batch instead of written.
  CheckerPopulationEventKind kind = CheckerPopulationEventKind::Add;
  uint64_t transaction = 0;
  if (streaming_pbp()) {
    if (streaming_pbp_finished_)
      std::abort();
    put_pbp_clause("rup", lits, size);
    put_pbp_text(" ;\n");
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
  } else {
    put_clausal_line(false, lits, size);
  }
  population_add(id, lits, size);
  observe_checker(kind, role, id, id, internal_view(lits, size), false,
                  transaction);
  if (streaming_pbp() && !size)
    finish_streaming_pbp_unsat();
}

void ProofWriter::rat(uint64_t id, const Lit *lits, uint32_t size) {
  flush_pending_dsr_units();
  if (!file_)
    return;
  if (streaming_pbp()) {
    if (!size || streaming_pbp_finished_)
      std::abort();
    put_pbp_clause("red", lits, size);
    const int32_t external = externalize(checker_lit(lits[0]));
    put_pbp_text(" : x%" PRIu32 " -> %d ;\n",
                 external_variable_of(external), external < 0 ? 0 : 1);
    note_pbp_derived(id, size,
                     [&] { return pbp_checker_literals(lits, size); });
  } else {
    // DSR: a witnessless addition with the pivot ordered first. The checker
    // first tries RUP and then infers first-literal RAT.
    next_addition_rat_ = copy_role_;
    put_clausal_line(false, lits, size);
  }
  population_add(id, lits, size);
  observe_checker(CheckerPopulationEventKind::Add, CheckerClauseRole::Rat,
                  id, id, internal_view(lits, size));
}

void ProofWriter::sr(uint64_t id, const Lit *lits, uint32_t size,
                     const Lit *true_lits, uint32_t ntrue,
                     const DsrLiteralMapping *mappings, uint32_t nmappings,
                     uint32_t external_variables, bool propagates,
                     const DsrWitnessPlan &plan) {
  if (!file_)
    return;
  if (streaming_pbp()) {
    put_pbp_clause("red", lits, size);
    put_pbp_text(" :");
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
      put_pbp_text(" x%" PRIu32 " -> %d",
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
      put_pbp_text(" x%" PRIu32 " -> %sx%" PRIu32,
                   external_variable_of(from), to < 0 ? "~" : "",
                   external_variable_of(to));
    }
    put_pbp_text(" ;\n");
    note_pbp_derived(id, size,
                     [&] { return pbp_checker_literals(lits, size); });
    population_add(id, lits, size);
    observe_checker(CheckerPopulationEventKind::Add,
                    CheckerClauseRole::SubstitutionRedundancy, id, id,
                    internal_view(lits, size));
    has_sr_witness_ = true;
    return;
  }

  // A pivot without an explicit mapping has the default image true; the
  // general encoding moves the pivot to the auxiliary, so it states that image.
  const bool default_pivot = plan.form == DsrWitnessForm::Direct;
  const bool needs_auxiliary = plan.form == DsrWitnessForm::Extension ||
                               (propagates && plan.form != DsrWitnessForm::Rup);
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
    next_addition_rat_ = copy_role_;
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
    return;
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
  if (plan.form != DsrWitnessForm::Rup) {
    has_sr_witness_ = true;
  }
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
}

// While a staged DSR-unit batch is open, the next deletion line is spooled
// (binary formats) or written to the pending temporary file (text formats);
// end_pending_deletion restores the main stream. Returns that main stream.
FILE *ProofWriter::begin_pending_deletion() {
  flush_binary();
  if (binary()) {
    spooling_deletions_ = true;
    return nullptr;
  }
  if (!pending_deletions_ && !(pending_deletions_ = std::tmpfile())) {
    if (!copy_role_)
      std::abort();
    copy_failed_ = true; // keep the main stream; the copy is discarded
    return file_;
  }
  FILE *const main_file = file_;
  file_ = pending_deletions_;
  return main_file;
}

void ProofWriter::end_pending_deletion(FILE *main_file) {
  flush_binary();
  if (binary())
    spooling_deletions_ = false;
  else
    file_ = main_file;
}

void ProofWriter::del(uint64_t line_id, uint64_t del_id, const Lit *lits,
                      uint32_t size) {
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
                          [&] { return pbp_checker_literals(lits, size); },
                          false))
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
  put_clausal_line(true, lits, size);
  if (defer)
    end_pending_deletion(main_file);
  population_del(del_id, lits, size);
  const CheckerPopulationEventKind deletion_kind =
      defer ? CheckerPopulationEventKind::StageDeleteRequested
            : CheckerPopulationEventKind::DeleteRequested;
  observe_checker(deletion_kind,
                  CheckerClauseRole::None, line_id, del_id,
                  internal_view(lits, size), false,
                  defer ? pending_population_transaction_ : 0);
}

void ProofWriter::external_rup(uint64_t id, const int32_t *lits, uint32_t size,
                               CheckerClauseRole role) {
  if (!file_)
    return;
  flush_pending_dsr_units();
  if (streaming_pbp()) {
    if (streaming_pbp_finished_)
      return;
    put_pbp_external_clause("rup", lits, size);
    put_pbp_text(" ;\n");
    note_pbp_derived(id, size, [lits] { return lits; });
  } else {
    next_addition_rat_ = copy_role_ && role == CheckerClauseRole::ExternalRat;
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
}

void ProofWriter::external_rat(uint64_t id, const int32_t *lits,
                               uint32_t size) {
  put_pbp_external_clause("red", lits, size);
  const int32_t pivot = lits[0];
  put_pbp_text(" : x%" PRIu32 " -> %d ;\n",
               external_variable_of(pivot), pivot < 0 ? 0 : 1);
  note_pbp_derived(id, size, [lits] { return lits; });
  population_external(true, id, lits, size);
  observe_checker(CheckerPopulationEventKind::Add,
                  CheckerClauseRole::ExternalRat, id, id,
                  PopulationClauseView::external_lits(lits, size), true);
}

uint32_t ProofWriter::new_helper() {
  const uint32_t extent = external_variable_bound();
  const uint32_t before = helper_names_.empty() ? 0 : helper_names_.back().helpers;
  if (uint64_t{extent} + before + 1 > MAX_CHECKER_NAME)
    checker_name_error();
  if (helper_names_.empty() || helper_names_.back().extent != extent)
    helper_names_.push_back({extent, before + 1});
  else
    ++helper_names_.back().helpers;
  return extent + before + 1;
}

uint64_t ProofWriter::checker_name(uint64_t solver_variable) const {
  if (helper_names_.empty() || solver_variable <= helper_names_.front().extent)
    return solver_variable;
  const auto after = std::lower_bound(
      helper_names_.begin(), helper_names_.end(), solver_variable,
      [](const DsrHelperNames &entry, uint64_t variable) {
        return entry.extent < variable;
      });
  return solver_variable + std::prev(after)->helpers;
}

uint32_t ProofWriter::checker_variable(uint32_t solver_variable) const {
  const uint64_t name = checker_name(solver_variable);
  if (name > MAX_CHECKER_NAME)
    checker_name_error();
  return static_cast<uint32_t>(name);
}

// Writes one parity certificate to the DSR copy. Dense variable i+1 is
// variables[i]; dense variables beyond them are this certificate's helpers.
struct ProofWriter::ParityWriter {
  ProofWriter &proof;
  const std::vector<int32_t> &variables;
  std::vector<uint32_t> helpers;

  int32_t checker_literal(int32_t dense) const {
    const uint32_t at = external_variable_of(dense);
    const uint32_t name = at <= variables.size()
        ? proof.checker_variable(static_cast<uint32_t>(variables[at - 1]))
        : helpers[at - variables.size() - 1];
    return dense < 0 ? -static_cast<int32_t>(name) : static_cast<int32_t>(name);
  }
  void line(const int32_t *lits, uint32_t size, bool deletion, bool rat) {
    if (!deletion)
      proof.next_addition_rat_ = proof.copy_role_ && rat;
    proof.put_line_tag(deletion);
    for (uint32_t i = 0; i < size; ++i)
      proof.put_checker_literal(checker_literal(lits[i]));
    proof.end_line();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    // The shadow names solver variables before renaming, as every other event
    // does. A helper keeps its checker name: it lies above every solver name
    // that exists while the certificate's helper clauses are live.
    std::vector<int32_t> named(size);
    for (uint32_t i = 0; i < size; ++i) {
      const uint32_t at = external_variable_of(lits[i]);
      const int32_t name = static_cast<int32_t>(
          at <= variables.size() ? static_cast<uint32_t>(variables[at - 1])
                                 : helpers[at - variables.size() - 1]);
      named[i] = lits[i] < 0 ? -name : name;
    }
    proof.population_external(!deletion, 0, named.data(), size);
#endif
  }
  bool add(const parity_certificate::SmallClause &clause, bool rat) {
    line(clause.lits.data(), clause.size, false, rat);
    return true;
  }
  bool add(const std::vector<int32_t> &clause, bool rat) {
    line(clause.data(), static_cast<uint32_t>(clause.size()), false, rat);
    return true;
  }
  bool erase(const parity_certificate::SmallClause &clause) {
    line(clause.lits.data(), clause.size, true, false);
    return true;
  }
  bool new_variable(int32_t &variable) {
    helpers.push_back(proof.new_helper());
    variable = static_cast<int32_t>(variables.size() + helpers.size());
    return true;
  }
};

void ProofWriter::write_parity_reason(const std::vector<PbpParityEquation> &equations,
                                const int32_t *reason, uint32_t reason_size) {
  flush_pending_dsr_units();
  std::vector<int32_t> variables;
  for (const PbpParityEquation &equation : equations)
    variables.insert(variables.end(), equation.variables.begin(),
                     equation.variables.end());
  for (uint32_t i = 0; i < reason_size; ++i)
    variables.push_back(static_cast<int32_t>(external_variable_of(reason[i])));
  std::sort(variables.begin(), variables.end());
  variables.erase(std::unique(variables.begin(), variables.end()), variables.end());
  if (!variables.empty())
    proof_external_max_ = std::max(proof_external_max_,
                                   static_cast<uint32_t>(variables.back()));
  const auto dense = [&](int32_t variable) {
    return static_cast<int32_t>(
        std::lower_bound(variables.begin(), variables.end(), variable) -
        variables.begin() + 1);
  };
  std::vector<parity_certificate::Equation> dense_equations;
  dense_equations.reserve(equations.size());
  for (const PbpParityEquation &equation : equations) {
    parity_certificate::Equation row;
    if (equation.variables.size() > row.variables.size())
      proof_error("a parity equation is wider than its certificate");
    for (const int32_t variable : equation.variables)
      row.variables[row.size++] = dense(variable);
    row.rhs = equation.rhs;
    dense_equations.push_back(row);
  }
  ParityWriter writer{*this, variables, {}};
  parity_certificate::Certificate<ParityWriter> certificate(
      writer, static_cast<uint32_t>(variables.size()), UINT32_MAX);
  if (!certificate.accumulate(dense_equations))
    proof_error("a parity reason has no clausal certificate");
  if (!reason_size) {
    if (!certificate.refuted())
      proof_error("a parity refutation has no clausal certificate");
    return;
  }
  std::vector<int32_t> dense_reason;
  dense_reason.reserve(reason_size);
  for (uint32_t i = 0; i < reason_size; ++i) {
    const int32_t variable = dense(static_cast<int32_t>(external_variable_of(reason[i])));
    dense_reason.push_back(reason[i] < 0 ? -variable : variable);
  }
  if (!certificate.export_reason(dense_reason) || !certificate.retire_helpers())
    proof_error("a parity reason has no clausal certificate");
}

void ProofWriter::check_startup_pb() {
  // Every startup refutation spells or defers itself on a DSR-only proof.
  if (dsr_only_)
    proof_error("a DSR proof has no PB stream");
}

FILE *ProofWriter::startup_pb_begin() {
  FILE *const terminal = switch_to_pb();
  // A complete native PB refutation has no clausal spelling.
  drop_copy(AutomaticProofReason::StartupPbRefutation);
  return terminal;
}

void ProofWriter::startup_pb(const std::function<bool(FILE *)> &write_pb) {
  // A published but incomplete certificate never claims UNSAT.
  if (!write_pb(startup_pb_begin()) || !finish_pb())
    std::abort();
}

void ProofWriter::refute_parity(const std::vector<PbpParityEquation> &equations,
                                const std::function<bool(FILE *)> &write_pb) {
  if (!dsr_only_)
    return startup_pb(write_pb);
  // Published first, so the certificate streams to the destination.
  publish();
  copy_->writer_->write_parity_reason(equations, nullptr, 0);
  finish_streaming_pbp_unsat();
  refuted_in_clauses_ = true;
  settle_copy();
}

void ProofWriter::refute_by_counting(
    const std::function<bool(FILE *)> &write_pb, std::vector<int32_t> &input) {
  if (!dsr_only_)
    return startup_pb(write_pb);
  // The kernel gets the terminal PB stream every other mode gives it, and no PB
  // text is written, as with no proof. Nothing has reached the destination
  // (publish_dsr_only_prefix), so the follow-on takes it untouched.
  switch_to_pb();
  assert(copy_ && !dsr_in_destination_);
  follow_on_binary_ = copy_->writer_->binary();
  discard_copy();
  automatic_reason_ = AutomaticProofReason::StartupPbRefutation;
  automatic_destination_ = nullptr;
  follow_on_destination_ = std::exchange(file_, nullptr);
  follow_on_input_.swap(input);
  follow_on_pending_ = true;
}

bool ProofWriter::open_follow_on(Proof &follow_on) {
  return follow_on_pending_ && follow_on_destination_ &&
         follow_on.open_dsr_only(std::exchange(follow_on_destination_, nullptr),
                                 follow_on_binary_);
}

bool ProofWriter::settle_follow_on(const Proof &follow_on) {
  const ProofWriter *const writer = follow_on.writer_.get();
  if (!follow_on_pending_ || !writer || !writer->automatic_selected_dsr_)
    return false;
  follow_on_route_ = writer->verification_route();
  follow_on_pending_ = false;
  std::vector<int32_t>().swap(follow_on_input_);
  return true;
}

void ProofWriter::parity_copy(const std::vector<PbpParityEquation> &equations,
                              const int32_t *reason, uint32_t reason_size) {
  if (!copy_)
    return;
  // The copy spells in clauses what this writer states in a few constraints.
  ProofWriter &copy = *copy_->writer_;
  if (dsr_only_ || conversion_barred() ||
      copy.parity_bytes_ < parity_bytes_limit_) {
    const uint64_t before = copy.dsr_bytes_written_;
    copy.write_parity_reason(equations, reason, reason_size);
    copy.parity_bytes_ += copy.dsr_bytes_written_ - before;
  } else {
    drop_copy(AutomaticProofReason::ParityReason);
  }
}

void ProofWriter::parity_reason(uint64_t kernel_clause_id,
                                const std::vector<PbpParityEquation> &equations,
                                const int32_t *reason, uint32_t reason_size,
                                PbpParityBoundsCache &cache,
                                const PbpParityReasonPlan &plan) {
  if (plan.proof_constraints > UINT64_MAX - pbp_derived_constraints_ ||
      pbp_original_constraints_ >
          UINT64_MAX - (pbp_derived_constraints_ + plan.proof_constraints))
    std::abort();
  const uint64_t derived_before = pbp_derived_constraints_;
  PbpTextWriter text(file_, pbp_text_wanted());
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

  // Only a proof that reads these constraints has to delete them later.
  const bool track_parity = pbp_text_wanted();
  const auto track_parity_constraint = [&](uint64_t id) {
    if (track_parity)
      pbp_parity_constraints_.push_back(id);
  };
  const auto track_parity_variable = [&](auto variable) {
    if (track_parity)
      pbp_parity_variables_.insert(static_cast<uint32_t>(variable));
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

  // Carry definitions and both bounds of one row. Its carry names were reserved
  // when the row was first seen, so a rewrite reuses them.
  const auto write_bounds = [&](const PbpParityEquation &equation,
                                PbpParityBounds &bounds) {
    uint32_t threshold = 2;
    for (const int32_t carry : bounds.carries) {
      text.put("red ");
      for (const int32_t variable : equation.variables) term(1, variable);
      term(threshold, -carry);
      text.put(">= ");
      text.put_uint(threshold);
      text.put(" : x");
      text.put_uint(static_cast<uint32_t>(carry));
      text.put(" -> 0 ;\n");
      const uint64_t first = constraint();
      track_parity_constraint(first);

      const uint32_t complement = equation.variables.size() - threshold + 1;
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
      track_parity_constraint(constraint());
      track_parity_variable(carry);
      threshold += 2;
    }
    bounds.lower = emit_bound(equation, bounds.carries, true);
    bounds.upper = emit_bound(equation, bounds.carries, false);
    bounds.numbering = pbp_numbering_;
    track_parity_constraint(bounds.lower);
    track_parity_constraint(bounds.upper);
    for (const int32_t variable : equation.variables)
      track_parity_variable(variable);
  };

  std::vector<uint64_t> lower, upper;
  lower.reserve(equations.size());
  upper.reserve(equations.size());
  uint64_t rewritten = 0; // constraints the plan does not count
  // The ledger entered every new row. A row is written when it never was (a
  // written lower bound names a constraint, so it is at least 1) or when
  // another numbering wrote it; a duplicate row is then current.
  for (const PbpParityEquation &equation : equations) {
    PbpParityBounds &bounds = cache.entries_.find(equation)->second;
    if (!bounds.lower) {
      write_bounds(equation, bounds);
    } else if (bounds.numbering != pbp_numbering_) {
      const uint64_t before = pbp_derived_constraints_;
      write_bounds(equation, bounds);
      rewritten += pbp_derived_constraints_ - before;
    }
    lower.push_back(bounds.lower);
    upper.push_back(bounds.upper);
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
  track_parity_constraint(lo);
  track_parity_constraint(hi);
  for (uint32_t i = 0; i < reason_size; ++i)
    track_parity_variable(external_variable_of(reason[i]));
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

  if (pbp_derived_constraints_ - derived_before !=
          plan.proof_constraints + rewritten ||
      std::ferror(file_))
    std::abort();
#ifndef NDEBUG
  pbp_audit_record(false, pbp_derived_constraints_, reason, reason_size);
#endif
  if (kernel_clause_id)
    track_pbp_derived_constraint(kernel_clause_id, reason_size);
  else if (reason_size != 2)
    register_pbp_anonymous(reason, reason_size);
  proof_external_max_ =
      std::max(proof_external_max_,
               plan.starting_external_extent + plan.helper_variables);
  population_external(true, kernel_clause_id, reason, reason_size);
  observe_checker(CheckerPopulationEventKind::Add,
                  CheckerClauseRole::ExternalRup, kernel_clause_id,
                  kernel_clause_id,
                  PopulationClauseView::external_lits(reason, reason_size),
                  true);
  if (!reason_size)
    finish_streaming_pbp_unsat();
}

bool ProofWriter::sr_needs_missing_auxiliary(const Lit *lits, uint32_t size,
                                       const Lit *true_lits, uint32_t ntrue,
                                       const DsrLiteralMapping *mappings,
                                       uint32_t nmappings, uint32_t external_variables,
                                       bool propagates) const {
  const DsrWitnessForm form =
      plan_dsr_witness(lits, size, true_lits, ntrue, mappings, nmappings).form;
  return (form == DsrWitnessForm::Extension ||
          (propagates && form != DsrWitnessForm::Rup)) &&
         checker_name(uint64_t{external_variables} + 1) > MAX_CHECKER_NAME;
}

void ProofWriter::retire_parity(const Lit *lits, uint32_t size,
                                const Lit *true_lits, uint32_t ntrue,
                                const DsrLiteralMapping *mappings,
                                uint32_t nmappings) {
  if (pbp_parity_constraints_.empty())
    return;
  const auto touched = [&](Lit lit) {
    return pbp_parity_variables_.count(var_of(checker_lit(lit)) + 1) != 0;
  };
  bool touches = size && touched(lits[0]);
  for (uint32_t i = 0; i < ntrue && !touches; ++i)
    touches = touched(true_lits[i]);
  for (uint32_t i = 0; i < nmappings && !touches; ++i)
    touches = touched(mappings[i].from);
  if (!touches)
    return;
  for (auto id = pbp_parity_constraints_.rbegin(); id != pbp_parity_constraints_.rend(); ++id)
    put_pbp_text("del id %" PRIu64 " ;\n", *id);
  pbp_parity_constraints_.clear();
  pbp_parity_variables_.clear();
  ++pbp_numbering_;
}

void ProofWriter::del_external(uint64_t line_id, uint64_t del_id,
                               const int32_t *lits, uint32_t size,
                               bool replaying) {
  if (!file_)
    return;
  if (streaming_pbp()) {
    if (streaming_pbp_finished_ ||
        !put_pbp_deletion(del_id, size, [lits] { return lits; }, replaying))
      return;
    // Both PBP deletion forms report the same checker-population request.
    population_external(false, del_id, lits, size);
    observe_checker(CheckerPopulationEventKind::DeleteRequested,
                    CheckerClauseRole::None, line_id, del_id,
                    PopulationClauseView::external_lits(lits, size), true);
    return;
  }
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
                  PopulationClauseView::external_lits(lits, size), true,
                  defer ? pending_population_transaction_ : 0);
}

void ProofWriter::flush() {
  if (streaming_pbp() && pbp_output_)
    publish();
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
// Key a clause exactly the way the checker matches a deletion: by the sorted
// external literal multiset. Keying by id would miss the whole defect class,
// because the hazard is precisely that two different ids can carry the same
// literals.
std::string ProofWriter::population_key(uint64_t, const Lit *lits,
                                  uint32_t size) const {
  std::string key;
  std::vector<int> external;
  external.reserve(size);
  for (uint32_t i = 0; i < size; i++)
    external.push_back(externalize(checker_lit(lits[i])));
  std::sort(external.begin(), external.end());
  key.assign(reinterpret_cast<const char *>(external.data()),
             external.size() * sizeof(int));
  return key;
}

void ProofWriter::population_add(uint64_t id, const Lit *lits, uint32_t size) {
  population_[population_key(id, lits, size)]++;
}

bool ProofWriter::population_contains(uint64_t id, const Lit *lits,
                                uint32_t size) const {
  const auto found = population_.find(population_key(id, lits, size));
  return found != population_.end() && found->second;
}

void ProofWriter::population_del(uint64_t id, const Lit *lits, uint32_t size) {
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
void ProofWriter::population_external(bool add, uint64_t id, const int32_t *lits,
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

void ProofWriter::seed(uint64_t id, const Lit *lits, uint32_t size) {
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
// --- automatic selection (plan §5) -----------------------------------------
// Each public front runs the ledger with this writer's bodies, then feeds the
// same event to the native-DSR copy. The kernel's answers come from the ledger,
// so the copy never steers the solver. An event the copy cannot follow ends it
// before writing.

const char *automatic_proof_reason_name(AutomaticProofReason reason) noexcept {
  switch (reason) {
  case AutomaticProofReason::Pending: return "pending";
  case AutomaticProofReason::DsrSelected: return "dsr-selected";
  case AutomaticProofReason::StartupPbRefutation: return "startup-pb-refutation";
  case AutomaticProofReason::ParityReason: return "gf2-parity-reason";
  case AutomaticProofReason::CopyDeclined: return "dsr-copy-declined";
  case AutomaticProofReason::CopyWriteFailed: return "dsr-copy-write-failed";
  case AutomaticProofReason::DestinationUnsupported:
    return "destination-unsupported";
  case AutomaticProofReason::StageUnavailable: return "stage-unavailable";
  case AutomaticProofReason::ForcedSwitch: return "forced-switch";
  }
  return "unknown";
}

namespace {

// Drop a stream's buffered, unwritten bytes (they are being discarded), so
// closing it cannot fail on them. Without a purge primitive the bytes are
// written and any error is ignored.
void discard_buffered(FILE *file) {
#if defined(__GLIBC__) || defined(__linux__)
  __fpurge(file);
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
  fpurge(file);
#else
  (void)file;
#endif
}

// A stdio stream that stores only an event held for the DSR copy's answer.
#if defined(__GLIBC__)
ssize_t pbp_sink_write(void *state, const char *data, size_t size) {
  return static_cast<ssize_t>(
      static_cast<LazySinkTarget *>(state)->accept(data, size));
}
int pbp_sink_close(void *) { return 0; }
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
int pbp_sink_write(void *state, const char *data, int size) {
  return size < 0 ? -1
                  : static_cast<int>(static_cast<LazySinkTarget *>(state)->accept(
                        data, static_cast<size_t>(size)));
}
int pbp_sink_close(void *) { return 0; }
#endif

FILE *open_pbp_sink(LazySinkTarget *state) {
#if defined(__GLIBC__)
  cookie_io_functions_t io{};
  io.write = pbp_sink_write;
  io.close = pbp_sink_close;
  return fopencookie(state, "w", io);
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
  return funopen(state, nullptr, pbp_sink_write, nullptr, pbp_sink_close);
#else
  (void)state;
  return nullptr; // no custom streams: automatic mode stays eager
#endif
}

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

// A new file next to `path`, on the filesystem with `device` and with `mode`,
// so a rename can place it over `path` and leave an ordinary proof file.
FILE *open_sibling(const std::string &path, dev_t device, mode_t mode,
                   std::string &name) {
  name = sibling_stage_template(path.c_str());
  const int descriptor = mkstemp(name.data());
  if (descriptor < 0) {
    name.clear();
    return nullptr;
  }
  struct stat sibling;
  FILE *const file =
      !fstat(descriptor, &sibling) && sibling.st_dev == device &&
              !fchmod(descriptor, mode & 07777)
          ? fdopen(descriptor, "w+b")
          : nullptr;
  if (!file) {
    close(descriptor);
    unlink(name.c_str());
    name.clear();
  }
  return file;
}

} // namespace

ProofWriter::LazyPbp::~LazyPbp() {
  if (sink) { // buffered bytes are dropped unwritten
    discard_buffered(sink);
    std::fclose(sink);
  }
  if (stage_fd >= 0)
    close(stage_fd);
}

namespace {
// A descriptor that reads the stage open on `fd`: a fresh read-only open file
// description where /proc allows it (it reads the bytes written so far even if
// the copy's own descriptor fails), else a duplicate of a readable `fd`.
int open_stage_reader(int fd) {
#ifdef __linux__
  char link[64];
  std::snprintf(link, sizeof link, "/proc/self/fd/%d", fd);
  const int reader = ::open(link, O_RDONLY | O_CLOEXEC);
  if (reader >= 0)
    return reader;
#endif
  const int flags = fcntl(fd, F_GETFL);
  if (flags < 0 || (flags & O_ACCMODE) == O_WRONLY)
    return -1;
  return dup(fd);
}
} // namespace

// Replace the empty private input-transaction file of a freshly opened
// automatic stream with a counting sink. Without one (no custom streams, no
// readable stage) the run stays eager and writes both streams.
void ProofWriter::start_lazy_pbp() {
  if (!streaming_pbp() || !file_ || !pbp_output_ || !input_transaction_ ||
      ftello(file_) != 0 || !copy_ || !copy_->writer_->file_)
    return;
  auto lazy = std::make_unique<LazyPbp>();
  if (!dsr_only_)
    lazy->stage_fd = open_stage_reader(fileno(copy_->writer_->file_));
  if (lazy->stage_fd < 0 && !dsr_only_) {
    lazy_report_.state = "off-no-reader";
    return;
  }
  lazy->sink = open_pbp_sink(lazy.get());
  if (!lazy->sink) {
    lazy_report_.state = "off-no-stream";
    return;
  }
  // The solver's text output stays buffered as under --pbp.
  setvbuf(lazy->sink, nullptr, _IOFBF, 1 << 20);
  std::fclose(file_);
  file_ = lazy->sink;
  lazy_ = std::move(lazy);
  lazy_published_ = false;
  dsr_is_the_proof_ = true;
  lazy_report_.state = "dsr";
  // Validation runs convert at a chosen point, so the conversion and the join
  // with the PBP suffix are checked on runs no trigger would switch.
  if (const char *at = std::getenv("TACTIX_AUTO_PROOF_SWITCH_AT"))
    forced_switch_at_ = !std::strcmp(at, "end")
                            ? UINT64_MAX
                            : std::strtoull(at, nullptr, 10);
  if (const char *limit = std::getenv("TACTIX_AUTO_PROOF_DSR_LIMIT"))
    dsr_limit_ = std::strtoull(limit, nullptr, 10);
  if (const char *limit = std::getenv("TACTIX_AUTO_PROOF_PARITY_BYTES"))
    parity_bytes_limit_ = std::strtoull(limit, nullptr, 10);
}

bool ProofWriter::conversion_barred() const noexcept {
  return lazy_ && copy_ && copy_->writer_->dsr_bytes_written_ >= dsr_limit_;
}

// Keep the text of the event about to be written, until the copy has answered.
void ProofWriter::hold_lazy_event() {
  if (!lazy_ || file_ != lazy_->sink)
    return;
  if (std::fflush(file_))
    std::abort();
  lazy_->holding = true;
  lazy_->held.clear();
  lazy_->derived_at_hold = pbp_derived_constraints_;
}

void ProofWriter::release_lazy_event() noexcept {
  if (lazy_) {
    lazy_->holding = false;
    lazy_->held.clear();
  }
}

// The switch (plan §5): convert the complete DSR proof to PBP and continue on it.
// DSR written to the destination is replaced by a rename once the PBP prefix is
// complete, so the path names one or the other, whatever kills the run. The
// numbering restarts from the converted lines, so no line cites a prefix constraint.
bool ProofWriter::convert_lazy_pbp() {
  LazyPbp &lazy = *lazy_;
  ProofWriter &copy = *copy_->writer_;
  const auto started = std::chrono::steady_clock::now();
  copy.flush();
  // A DSR-only copy has no stage reader: nothing converts.
  if (copy.copy_failed_ || std::fflush(file_) || lazy.stage_fd < 0)
    return false;
  // Replaces a destination that holds the DSR. Before publication the PBP
  // prefix stays private, as on any route, and this file waits for the header.
  FILE *replacement = nullptr;
  std::string replacement_name;
  if (dsr_in_destination_) {
    struct stat dsr_stat;
    if (fstat(fileno(copy.file_), &dsr_stat))
      return false;
    replacement = open_sibling(automatic_path_, dsr_stat.st_dev,
                               dsr_stat.st_mode, replacement_name);
    if (!replacement)
      return false;
  }
  const auto discard_replacement = [&] {
    if (replacement) {
      discard_buffered(replacement);
      std::fclose(replacement);
      unlink(replacement_name.c_str());
    }
  };
  FILE *target = !lazy_published_ ? std::tmpfile()
                 : replacement    ? replacement
                                  : lazy_destination_;
  if (!target) {
    discard_replacement();
    return false;
  }
  if (!lazy_published_ || replacement)
    setvbuf(target, nullptr, _IOFBF, 1 << 20);
  DsrToPbpResult converted;
  bool ok = !lazy_published_ || write_pbp_header(target, lazy_f_) >= 0;
  if (ok) {
    converted = convert_dsr_to_pbp(lazy.stage_fd, copy.binary(),
                                   copy.rat_additions_, copy.helper_names_, target);
    ok = converted.ok;
  }
  // Held text uses the writer's numbering, which ends here: an id deletion in
  // it would name the wrong constraint.
  if (ok && lazy.held.find("del id ") != std::string::npos)
    ok = false;
  if (ok && !lazy.held.empty())
    ok = std::fwrite(lazy.held.data(), 1, lazy.held.size(), target) ==
         lazy.held.size();
  // A held event wrote its own conclusion; otherwise the stage ends with the
  // empty clause and the conclusion follows it.
  if (ok && streaming_pbp_finished_ && !lazy.holding)
    ok = std::fputs(kPbpConclusion, target) >= 0;
  ok = ok && !std::fflush(target) && !std::ferror(target);
  // The DSR leaves the path only here, when the PBP that replaces it is whole.
  if (ok && replacement)
    ok = !std::rename(replacement_name.c_str(), automatic_path_.c_str());
  if (!ok) {
    if (!lazy_published_) {
      std::fclose(target);
      discard_replacement();
    } else if (replacement) {
      discard_replacement();
    } else {
      // The destination is free again for the complete DSR copy.
      discard_buffered(target);
      std::clearerr(target);
      if (ftruncate(fileno(target), 0) || std::fseek(target, 0, SEEK_SET))
        std::clearerr(target); // the placement reports its own failure
    }
    return false;
  }
  const uint64_t in_flight = lazy.holding ? pbp_derived_constraints_ - lazy.derived_at_hold : 0;
  // Deletion by id cannot survive: the writer's identities name its own
  // numbering, and `del spec` may select any copy of a duplicate.
  abandon_pbp_id_deletions(6, 0);
  pbp_constraint_ids_.clear();
  pbp_anonymous_constraints_.clear();
  pbp_pending_rebind_ = {};
  pbp_last_input_live_ = false;
  pbp_derived_constraints_ = converted.derived + in_flight;
#ifndef NDEBUG
  pbp_audit_input_.clear();
  pbp_audit_derived_.clear();
#endif
  ++pbp_numbering_;
  pbp_parity_constraints_.clear();
  pbp_parity_variables_.clear();
  lazy_report_.dsr_bytes = converted.dsr_bytes;
  lazy_report_.pbp_bytes = converted.pbp_bytes;
  file_ = target; // the stream continues on the real file
  if (replacement) {
    // The old handle names the unlinked DSR file; nothing was written through it.
    FILE *&held = lazy_published_ ? lazy_destination_ : pbp_output_;
    std::fclose(held);
    held = nullptr;
    if (!lazy_published_)
      pbp_output_ = replacement;
    dsr_in_destination_ = false;
  } else if (lazy_published_) {
    lazy_destination_ = nullptr; // now owned through file_
  }
  lazy_.reset();
  dsr_is_the_proof_ = false;
  automatic_destination_ = file_;
  lazy_report_.state = "converted";
  lazy_report_.nanos = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - started)
          .count());
  return true;
}

// Stop counting PBP text (DSR selected, startup PB refutation, unfinished
// run). The destination stays as the caller leaves it.
void ProofWriter::discard_lazy_pbp() noexcept {
  if (!lazy_)
    return;
  if (file_ == lazy_->sink)
    file_ = nullptr;
  lazy_.reset();
}

// A native-DSR Proof on `stage` (owned) in the copy role: its ledger is
// published from the start and its writer never aborts the solver.
std::unique_ptr<Proof> ProofWriter::open_copy(FILE *stage, bool binary) {
  std::unique_ptr<ProofWriter> writer = dsr(
      stage, binary ? ProofFormat::DsrBinary : ProofFormat::DsrAscii, true);
  auto copy = std::make_unique<Proof>();
  copy->copy_role_ = true;
  if (!writer || !copy->start(std::move(writer), false))
    return nullptr;
  copy->bind_variable_map(variable_map_);
  copy->bind_external_variable_map(external_variable_map_);
  return copy;
}

// The DSR writer is the copy, so the kernel's view is that of every other run.
// Like whole-run PBP it keeps the input transaction private: the copy writes to
// a temporary file until the first clausal event (publish_dsr_only_prefix).
void ProofWriter::start_dsr_only(bool binary) {
  automatic_ = true;
  dsr_only_ = true;
  automatic_reason_ = AutomaticProofReason::Pending;
  automatic_destination_ = pbp_output_;
  copy_ = open_copy(std::tmpfile(), binary);
  if (!copy_)
    proof_error("cannot write the DSR proof");
  start_lazy_pbp();
  if (!lazy_)
    proof_error("no counting stream for the DSR proof");
}

void ProofWriter::start_automatic(const char *path, bool binary_dsr,
                                  const char *stage_path) {
  automatic_ = true;
  automatic_selected_dsr_ = false;
  automatic_dsr_witness_ = false;
  automatic_reason_ = AutomaticProofReason::Pending;
  // Absolute, so a later change of directory cannot redirect the placement.
  std::error_code absolute_error;
  automatic_path_ = std::filesystem::absolute(path, absolute_error).string();
  if (absolute_error)
    automatic_path_ = path;
  automatic_stage_name_.clear();
  // The empty clause rewrites the destination in place, so it must be a
  // regular file (a proc-fd path to an anonymous file qualifies).
  FILE *const destination = pbp_output_ ? pbp_output_ : file_;
  automatic_destination_ = destination;
  struct stat destination_stat;
  if (fstat(fileno(destination), &destination_stat) ||
      !S_ISREG(destination_stat.st_mode)) {
    automatic_reason_ = AutomaticProofReason::DestinationUnsupported;
    return;
  }
  struct stat link_stat;
  const bool renameable = !lstat(path, &link_stat) &&
                          S_ISREG(link_stat.st_mode) &&
                          link_stat.st_nlink == 1 &&
                          link_stat.st_dev == destination_stat.st_dev &&
                          link_stat.st_ino == destination_stat.st_ino;
  const auto attach_copy = [&](FILE *stage) {
    copy_ = open_copy(stage, binary_dsr);
    if (!copy_)
      return false;
    copy_->writer_->keeps_rat_additions_ = true;
    start_lazy_pbp();
    return true;
  };
  // The DSR goes straight to the destination, so a killed run leaves its
  // prefix. That needs a counting PBP stream, since nothing else may write
  // there, and a place for the file that replaces it at a switch.
  if (!stage_path && renameable) {
    std::string probe_name;
    FILE *const probe = open_sibling(automatic_path_, destination_stat.st_dev,
                                     destination_stat.st_mode, probe_name);
    if (probe) {
      std::fclose(probe);
      unlink(probe_name.c_str());
      const int descriptor =
          ::open(automatic_path_.c_str(), O_RDWR | O_CLOEXEC);
      struct stat direct_stat;
      FILE *const direct =
          descriptor >= 0 && !fstat(descriptor, &direct_stat) &&
                  direct_stat.st_dev == destination_stat.st_dev &&
                  direct_stat.st_ino == destination_stat.st_ino
              ? fdopen(descriptor, "r+b")
              : nullptr;
      if (!direct && descriptor >= 0)
        close(descriptor);
      if (direct && attach_copy(direct)) {
        if (lazy_) {
          dsr_in_destination_ = true;
          return;
        }
        copy_.reset(); // nothing was written: fall back to a stage
      }
    }
  }
  FILE *stage = nullptr;
  if (stage_path) {
    // A caller stage is copied from at the empty clause: it must be a
    // regular file other than the destination, or the copy could not be
    // read back.
    stage = std::fopen(stage_path, "w+b");
    struct stat stage_stat;
    if (stage && (fstat(fileno(stage), &stage_stat) ||
                  !S_ISREG(stage_stat.st_mode) ||
                  (stage_stat.st_dev == destination_stat.st_dev &&
                   stage_stat.st_ino == destination_stat.st_ino))) {
      std::fclose(stage);
      stage = nullptr;
    }
  } else if (renameable) {
    stage = open_sibling(automatic_path_, destination_stat.st_dev,
                         destination_stat.st_mode, automatic_stage_name_);
  }
  if (!stage && !stage_path)
    stage = std::tmpfile();
  if (!stage) {
    automatic_reason_ = AutomaticProofReason::StageUnavailable;
    return;
  }
  if (!attach_copy(stage)) {
    if (!automatic_stage_name_.empty())
      unlink(automatic_stage_name_.c_str());
    automatic_stage_name_.clear();
    automatic_reason_ = AutomaticProofReason::StageUnavailable;
  }
}

void ProofWriter::drop_copy(AutomaticProofReason reason) noexcept {
  if (!copy_)
    return;
  if (dsr_only_)
    proof_error(automatic_proof_reason_name(reason));
  // PBP becomes the proof: pay the conversion. Without a converted prefix no
  // UNSAT claim may follow: fail closed.
  if (lazy_ && !convert_lazy_pbp())
    std::abort();
  discard_copy();
  automatic_reason_ = reason;
}

// Discard the copy's staged and buffered bytes instead of writing them,
// and release the stage's space at once (a caller-owned stage stays).
void ProofWriter::discard_copy() noexcept {
  ProofWriter &copy = *copy_->writer_;
  copy.pending_units_.clear();
  copy.pending_unit_literals_.clear();
  copy.pending_unit_ids_.clear();
  copy.pending_deletion_bytes_.clear();
  copy.spooling_deletions_ = false;
  copy.binary_buffer_size_ = 0;
  copy.copy_failed_ = true;
  if (copy.file_) {
    discard_buffered(copy.file_);
    (void)ftruncate(fileno(copy.file_), 0);
    std::fclose(copy.file_);
    copy.file_ = nullptr;
  }
  copy_.reset();
  dsr_in_destination_ = false;
  if (!automatic_stage_name_.empty())
    unlink(automatic_stage_name_.c_str());
  automatic_stage_name_.clear();
}

void ProofWriter::settle_copy() {
  if (!copy_)
    return;
  // The copy holds the event: a switch from here must not write it again.
  release_lazy_event();
  // The only proof could not be written: there is nothing left to convert.
  if (copy_->writer_->copy_failed_) {
    if (dsr_is_the_proof_)
      std::abort();
    drop_copy(AutomaticProofReason::CopyWriteFailed);
    return;
  }
  if (lazy_ && forced_switch_at_ && !streaming_pbp_finished_ &&
      forced_switch_at_ != UINT64_MAX &&
      copy_->writer_->additions_ >= forced_switch_at_) {
    drop_copy(AutomaticProofReason::ForcedSwitch);
    return;
  }
  if (!streaming_pbp_finished_)
    return;
  // Validation: a switch forced at the empty clause. If it fails, the complete
  // DSR copy remains a valid certificate and is kept instead.
  if (lazy_ && forced_switch_at_ == UINT64_MAX) {
    if (convert_lazy_pbp()) {
      drop_copy(AutomaticProofReason::ForcedSwitch);
      return;
    }
    discard_lazy_pbp();
    lazy_report_.state = "fallback";
  }
  select_copy();
}

// Make the DSR copy the proof: it is already in the destination, else rename a
// sibling stage over the destination, else rewrite the destination in place. Until the
// destination changes, any copy-side failure keeps the complete PBP stream.
// Afterwards the discarded PBP bytes are dropped unwritten, so only the DSR
// bytes can fail to land; a failed placement aborts before any UNSAT verdict.
// An unfinished run (`finished` false: no empty clause, so no verdict rests on
// the proof, and the verdict may be SAT) installs the copy's prefix the same
// way, but a failure only loses that prefix: it never aborts and never
// converts to PBP.
void ProofWriter::select_copy(bool finished) {
  if (dsr_only_ && !dsr_in_destination_)
    publish_dsr_only_prefix();
  ProofWriter &copy = *copy_->writer_;
  copy.flush();
  if (copy.copy_failed_) {
    if (!finished)
      discard_lazy_pbp();
    else if (dsr_is_the_proof_)
      std::abort(); // the only proof could not be written
    // Whatever reached the destination stays: dropping would truncate it.
    if (dsr_in_destination_ && copy.file_) {
      discard_buffered(copy.file_);
      std::fclose(copy.file_);
      copy.file_ = nullptr;
    }
    drop_copy(AutomaticProofReason::CopyWriteFailed);
    return;
  }
  FILE *stage = copy.file_;
  copy.file_ = nullptr;
  bool placed = false;
  if (dsr_in_destination_) {
    placed = true;
    const bool closed = !std::fclose(stage);
    stage = nullptr;
    if (!closed && finished)
      std::abort();
  } else if (!automatic_stage_name_.empty()) {
    // The rename is atomic: the path names either the PBP or the DSR file.
    const bool closed = !std::fclose(stage);
    stage = nullptr;
    placed = closed && !std::rename(automatic_stage_name_.c_str(),
                                    automatic_path_.c_str());
    if (!placed && (!closed ||
                    !(stage = std::fopen(automatic_stage_name_.c_str(), "rb")))) {
      unlink(automatic_stage_name_.c_str());
      automatic_stage_name_.clear();
      copy_.reset();
      automatic_reason_ = AutomaticProofReason::CopyWriteFailed;
      if (!finished) {
        discard_lazy_pbp();
        return;
      }
      // The stage is gone: only an eager PBP stream can still be the proof.
      if (dsr_is_the_proof_)
        std::abort();
      return; // the PBP destination is untouched and complete
    }
  }
  FILE *destination = automatic_destination_;
  // The PBP stream accepts nothing further. A still-private PBP prefix and any
  // buffered PBP bytes are discarded unwritten.
  if (lazy_) {
    const bool barred = conversion_barred();
    discard_lazy_pbp();
    lazy_report_.state = barred ? "dsr-limit" : "dsr-kept";
    if (barred)
      lazy_report_.dsr_bytes = copy.dsr_bytes_written_;
  }
  if (file_ && file_ != destination) {
    discard_buffered(file_);
    std::fclose(file_);
  }
  file_ = nullptr;
  pbp_output_ = nullptr;
  automatic_destination_ = nullptr;
  lazy_destination_ = nullptr; // closed below with the destination
  binary_buffer_size_ = 0;
  discard_buffered(destination);
  if (placed) {
    std::fclose(destination); // a second handle, or the renamed-over PBP file
    destination = nullptr;
  } else {
    if ((ftruncate(fileno(destination), 0) ||
         std::fseek(destination, 0, SEEK_SET) ||
         std::fseek(stage, 0, SEEK_SET) || !copy_stream(stage, destination) ||
         std::fflush(destination) || std::ferror(destination)) &&
        finished)
      std::abort();
    // The copied stage's bytes are no longer needed (a caller-owned stage
    // stays, emptied).
    (void)ftruncate(fileno(stage), 0);
    if (std::fclose(stage) && finished)
      std::abort();
    if (!automatic_stage_name_.empty())
      unlink(automatic_stage_name_.c_str());
  }
  automatic_stage_name_.clear();
  if (destination && std::fclose(destination) && finished)
    std::abort();
  automatic_selected_dsr_ = true;
  automatic_dsr_witness_ = copy.has_sr_witness_;
  automatic_reason_ = AutomaticProofReason::DsrSelected;
  copy_.reset();
}

void ProofWriter::before_sr(const Lit *lits, uint32_t size,
                            const Lit *true_lits, uint32_t ntrue,
                            const DsrLiteralMapping *mappings,
                            uint32_t nmappings, uint32_t external_variables,
                            bool propagates) {
  // Before either stream retires anything for the line.
  if (!copy_ || !copy_->writer_->sr_needs_missing_auxiliary(
                    lits, size, true_lits, ntrue, mappings, nmappings,
                    external_variables, propagates))
    return;
  if (dsr_only_)
    checker_name_error();
  drop_copy(AutomaticProofReason::CopyDeclined);
}

bool ProofWriter::before_external_rat(const int32_t *lits, uint32_t size) {
  // DSR rejects a tautology only after retiring leased premises, so its text
  // and the writer's would overlap: convert before either stream runs.
  const bool tautology =
      copy_ && size && lits && Proof::external_tautological(lits, size);
  if (tautology && !dsr_only_)
    drop_copy(AutomaticProofReason::CopyDeclined);
  return tautology;
}

} // namespace tactix
