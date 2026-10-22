#undef NDEBUG
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "population/population_observer.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/substitute/substitute.hpp"

#include <array>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

using namespace tactix;

namespace {

constexpr size_t MAX_EVENTS = 512;
constexpr size_t MAX_LITERALS = 16;

struct CopiedClause {
  std::array<int32_t, MAX_LITERALS> literals{};
  uint32_t size = 0;

  void assign(PopulationClauseView view) noexcept {
    assert(view.size() <= literals.size());
    size = view.size();
    for (uint32_t i = 0; i < size; ++i)
      literals[i] = view.at(i);
  }

  bool equals(std::initializer_list<int32_t> expected) const noexcept {
    if (expected.size() != size)
      return false;
    size_t i = 0;
    for (const int32_t literal : expected)
      if (literals[i++] != literal)
        return false;
    return true;
  }
};

struct RawRecord {
  RawInputEventKind kind{};
  uint64_t id = 0;
  uint32_t disposition = 0;
  bool normalized_available = false;
  PopulationSourceClassification source_classification =
      PopulationSourceClassification::PendingInputClassification;
  CopiedClause raw;
  CopiedClause normalized;
};

struct CheckerRecord {
  CheckerPopulationEventKind kind{};
  CheckerClauseRole role{};
  CheckerRoute route{};
  CheckerNaming naming{};
  uint64_t line_id = 0;
  uint64_t checker_id = 0;
  uint64_t predecessor_id = 0;
  uint64_t transaction_id = 0;
  bool proof_only = false;
  PopulationSourceClassification source_classification =
      PopulationSourceClassification::Ineligible;
  CopiedClause clause;
};

struct SolverRecord {
  SolverPopulationEventKind kind{};
  SolverClauseKind clause_kind{};
  uint64_t id = 0;
  uint32_t copies = 0;
  bool redundant = false;
  uint64_t transaction_id = 0;
  CopiedClause clause;
};

struct ReplacementRecord {
  SolverReplacementPhase phase{};
  SolverReplacementOutcome outcome{};
  uint64_t predecessor_id = 0;
  uint64_t replacement_id = 0;
  CopiedClause predecessor;
  CopiedClause replacement;
};

// Callback recording is bounded and allocation-free. This is also a concrete
// compile-time check of the observer's noexcept/preallocated contract.
class Recorder final : public PopulationObserver {
public:
  void observe(const RawInputPopulationEvent &event) noexcept override {
    check_kernel();
    assert(raw_size < raw.size());
    RawRecord &record = raw[raw_size++];
    record.kind = event.kind;
    record.id = event.raw_input_id;
    record.disposition = event.disposition;
    record.normalized_available = event.normalized_available;
    record.source_classification = event.source_classification;
    record.raw.assign(event.raw);
    record.normalized.assign(event.normalized);
  }

  void observe(const CheckerPopulationEvent &event) noexcept override {
    check_kernel();
    assert(checker_size < checker.size());
    CheckerRecord &record = checker[checker_size++];
    record.kind = event.kind;
    record.role = event.role;
    record.route = event.route;
    record.naming = event.naming;
    record.line_id = event.line_id;
    record.checker_id = event.checker_id;
    record.predecessor_id = event.predecessor_id;
    record.transaction_id = event.transaction_id;
    record.proof_only = event.proof_only;
    record.source_classification = event.source_classification;
    record.clause.assign(event.clause);
  }

  void observe(const SolverPopulationEvent &event) noexcept override {
    check_kernel();
    assert(solver_size < solver.size());
    SolverRecord &record = solver[solver_size++];
    record.kind = event.kind;
    record.clause_kind = event.clause_kind;
    record.id = event.logical_id;
    record.copies = event.copies;
    record.redundant = event.redundant;
    record.transaction_id = event.transaction_id;
    record.clause.assign(event.clause);
  }

  void observe(const SolverReplacementEvent &event) noexcept override {
    check_kernel();
    assert(replacement_size < replacement.size());
    ReplacementRecord &record = replacement[replacement_size++];
    record.phase = event.phase;
    record.outcome = event.outcome;
    record.predecessor_id = event.predecessor_id;
    record.replacement_id = event.replacement_id;
    record.predecessor.assign(event.predecessor);
    record.replacement.assign(event.replacement);
  }

  const RawRecord &raw_event(RawInputEventKind kind, uint64_t id) const {
    for (size_t i = 0; i < raw_size; ++i)
      if (raw[i].kind == kind && raw[i].id == id)
        return raw[i];
    assert(false && "missing raw population event");
    return raw[0];
  }

  size_t checker_count(CheckerPopulationEventKind kind) const noexcept {
    size_t result = 0;
    for (size_t i = 0; i < checker_size; ++i)
      result += checker[i].kind == kind;
    return result;
  }

  size_t solver_count(SolverPopulationEventKind kind) const noexcept {
    size_t result = 0;
    for (size_t i = 0; i < solver_size; ++i)
      result += solver[i].kind == kind;
    return result;
  }

  void check_kernel() const noexcept {
    if (!invariant_kernel)
      return;
    if (check_full_kernel) {
      kernel_check_invariants(*invariant_kernel);
      return;
    }
    // Sweep's private mixed-occurrence stream can be intentionally ahead of
    // its final dense publication during the connected transaction.  The
    // population seam still must never expose a half-watched binary: compare
    // exact directed multiplicities without imposing the later stream-order
    // boundary.
    for (Lit a = 0; a < 2 * invariant_kernel->nvars; ++a)
      for (const WatchEntry watch : invariant_kernel->watches.entries(a)) {
        if (!watch.binary() || a > watch.other())
          continue;
        uint32_t forward = 0, reverse = 0;
        for (const WatchEntry candidate :
             invariant_kernel->watches.entries(a))
          forward += candidate.binary() && candidate.other() == watch.other();
        for (const WatchEntry candidate :
             invariant_kernel->watches.entries(watch.other()))
          reverse += candidate.binary() && candidate.other() == a;
        assert(forward == reverse);
      }
  }

  std::array<RawRecord, MAX_EVENTS> raw{};
  std::array<CheckerRecord, MAX_EVENTS> checker{};
  std::array<SolverRecord, MAX_EVENTS> solver{};
  std::array<ReplacementRecord, MAX_EVENTS> replacement{};
  size_t raw_size = 0;
  size_t checker_size = 0;
  size_t solver_size = 0;
  size_t replacement_size = 0;
  const Kernel *invariant_kernel = nullptr;
  bool check_full_kernel = true;
};

FILE *dimacs_file(const char *text) {
  FILE *file = std::tmpfile();
  assert(file);
  assert(std::fputs(text, file) >= 0);
  std::rewind(file);
  return file;
}

std::string proof_path(const char *tag) {
  return "/tmp/tactix-population-" + std::string(tag) + "-" +
         std::to_string(static_cast<long>(getpid())) + ".proof";
}

Lit lit(int dimacs) {
  assert(dimacs);
  return internalize(dimacs);
}

void test_no_proof_raw_and_solver_population() {
  Kernel kernel;
  Recorder recorder;
  {
    ScopedPopulationObserverBinding binding(kernel, recorder);
    FILE *file = dimacs_file(
        "p cnf 14 9\n"
        "1 2 3 0\n"       // unchanged large
        "4 4 5 0\n"       // repeated literal -> binary
        "6 -6 0\n"        // tautology
        "7 0\n"           // root unit
        "7 8 0\n"         // root-satisfied
        "-7 9 10 0\n"     // root-shortened -> binary
        "11 12 0\n"       // binary
        "12 11 0\n"       // duplicate binary
        "13 -7 14 0\n");  // another root-shortened binary
    assert(parse_dimacs(file, kernel) == nullptr);
    std::fclose(file);
  }

  assert(recorder.checker_size == 0); // no Proof: never fabricate checker state
  assert(recorder.raw_size == 19);    // clause/outcome pairs plus Finished
  assert(recorder.raw[18].kind == RawInputEventKind::Finished);
  assert(recorder.raw[18].id == 9);

  assert(recorder.raw_event(RawInputEventKind::Clause, 1).raw.equals({1, 2, 3}));
  const RawRecord &one = recorder.raw_event(RawInputEventKind::Outcome, 1);
  assert((one.disposition & (RawAdmitted | RawLarge)) ==
         (RawAdmitted | RawLarge));
  assert(one.normalized_available);
  assert(one.source_classification ==
         PopulationSourceClassification::Eligible);
  assert(one.normalized.equals({1, 2, 3}));

  const RawRecord &two = recorder.raw_event(RawInputEventKind::Outcome, 2);
  assert((two.disposition & (RawRepeatedLiteral | RawAdmitted | RawBinary)) ==
         (RawRepeatedLiteral | RawAdmitted | RawBinary));
  assert(two.normalized.equals({4, 5}));

  const RawRecord &three = recorder.raw_event(RawInputEventKind::Outcome, 3);
  assert((three.disposition & (RawDropped | RawTautology | RawBinary)) ==
         (RawDropped | RawTautology | RawBinary));
  assert(!three.normalized_available);
  assert(three.normalized.size == 0);
  assert(three.source_classification ==
         PopulationSourceClassification::Ineligible);

  const RawRecord &four = recorder.raw_event(RawInputEventKind::Outcome, 4);
  assert((four.disposition & (RawAdmitted | RawUnit)) ==
         (RawAdmitted | RawUnit));
  assert(four.normalized.equals({7}));

  const RawRecord &five = recorder.raw_event(RawInputEventKind::Outcome, 5);
  assert((five.disposition & (RawDropped | RawRootSatisfied)) ==
         (RawDropped | RawRootSatisfied));
  assert(!five.normalized_available);
  assert(five.normalized.size == 0);

  const RawRecord &six = recorder.raw_event(RawInputEventKind::Outcome, 6);
  assert((six.disposition & (RawAdmitted | RawRootShortened | RawBinary)) ==
         (RawAdmitted | RawRootShortened | RawBinary));
  assert(six.normalized.equals({9, 10}));

  const RawRecord &eight = recorder.raw_event(RawInputEventKind::Outcome, 8);
  assert((eight.disposition & (RawDropped | RawDuplicate | RawBinary)) ==
         (RawDropped | RawDuplicate | RawBinary));
  assert(eight.normalized.equals({12, 11}));

  assert(recorder.solver_count(SolverPopulationEventKind::Add) == 6);
  assert(recorder.solver_count(SolverPopulationEventKind::UnitMaterialized) ==
         0);
}

void test_raw_empty_and_long_tautology_classification() {
  {
    Kernel kernel;
    Recorder recorder;
    ScopedPopulationObserverBinding binding(kernel, recorder);
    FILE *file = dimacs_file("p cnf 0 1\n0\n");
    assert(parse_dimacs(file, kernel) == nullptr);
    std::fclose(file);
    const RawRecord &outcome =
        recorder.raw_event(RawInputEventKind::Outcome, 1);
    assert(outcome.normalized_available && outcome.normalized.size == 0);
    assert((outcome.disposition & (RawAdmitted | RawEmpty)) ==
           (RawAdmitted | RawEmpty));
    assert(outcome.source_classification ==
           PopulationSourceClassification::Ineligible);
  }
  {
    Kernel kernel;
    Recorder recorder;
    ScopedPopulationObserverBinding binding(kernel, recorder);
    FILE *file = dimacs_file(
        "p cnf 11 1\n1 2 3 4 5 6 7 8 9 10 11 -1 0\n");
    assert(parse_dimacs(file, kernel) == nullptr);
    std::fclose(file);
    const RawRecord &raw = recorder.raw_event(RawInputEventKind::Clause, 1);
    assert(raw.raw.equals({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, -1}));
    const RawRecord &outcome =
        recorder.raw_event(RawInputEventKind::Outcome, 1);
    assert((outcome.disposition & (RawDropped | RawTautology)) ==
           (RawDropped | RawTautology));
    assert(!outcome.normalized_available && outcome.normalized.size == 0);
    assert(outcome.source_classification ==
           PopulationSourceClassification::Ineligible);
  }
}

void test_duplicate_unit_and_large_input_classification() {
  Kernel kernel;
  Recorder recorder;
  {
    ScopedPopulationObserverBinding binding(kernel, recorder);
    FILE *file = dimacs_file(
        "p cnf 4 4\n"
        "1 0\n"
        "1 0\n"
        "2 3 4 0\n"
        "2 3 4 0\n");
    assert(parse_dimacs(file, kernel) == nullptr);
    std::fclose(file);
  }
  const RawRecord &duplicate_unit =
      recorder.raw_event(RawInputEventKind::Outcome, 2);
  assert((duplicate_unit.disposition &
          (RawDropped | RawRootSatisfied)) ==
         (RawDropped | RawRootSatisfied));
  assert(!duplicate_unit.normalized_available);
  assert(duplicate_unit.source_classification ==
         PopulationSourceClassification::Ineligible);
  const RawRecord &first_large =
      recorder.raw_event(RawInputEventKind::Outcome, 3);
  const RawRecord &duplicate_large =
      recorder.raw_event(RawInputEventKind::Outcome, 4);
  assert(first_large.normalized_available &&
         duplicate_large.normalized_available);
  assert(first_large.normalized.equals({2, 3, 4}));
  assert(duplicate_large.normalized.equals({2, 3, 4}));
  assert(first_large.source_classification ==
         PopulationSourceClassification::Eligible);
  assert(duplicate_large.source_classification ==
         PopulationSourceClassification::Eligible);
  size_t large_adds = 0;
  for (size_t i = 0; i < recorder.solver_size; ++i)
    large_adds += recorder.solver[i].kind == SolverPopulationEventKind::Add &&
                  recorder.solver[i].clause_kind == SolverClauseKind::Large &&
                  recorder.solver[i].clause.equals({2, 3, 4});
  assert(large_adds == 2);
}

void test_proof_route_lifecycle_and_streaming_pbp_unbind() {
  {
    Proof proof;
    Recorder recorder;
    proof.bind_population_observer(&recorder);
    assert(recorder.checker_size == 1);
    assert(recorder.checker[0].kind ==
           CheckerPopulationEventKind::ObserverBound);
    assert(recorder.checker[0].route == CheckerRoute::None);
    assert(!proof.open("/path/that/does/not/exist/tactix.proof",
                       ProofFormat::DsrAscii));
    assert(!proof.open("ignored.pbp", ProofFormat::Pbp));
    assert(recorder.checker_count(CheckerPopulationEventKind::RouteOpened) ==
           0);
    proof.bind_population_observer(nullptr);
    assert(recorder.checker[recorder.checker_size - 1].route ==
           CheckerRoute::None);
  }

  for (const auto format : {ProofFormat::DsrAscii, ProofFormat::DsrBinary,
                            ProofFormat::DratAscii, ProofFormat::DratBinary,
                            ProofFormat::Lrat}) {
    Recorder recorder;
    const std::string path = proof_path("route");
    {
      Proof proof;
      proof.bind_population_observer(&recorder);
      assert(proof.open(path.c_str(), format));
      assert(recorder.checker_count(CheckerPopulationEventKind::RouteOpened) ==
             1);
      const CheckerRecord &opened = recorder.checker[1];
      assert(opened.kind == CheckerPopulationEventKind::RouteOpened);
      const CheckerRoute expected =
          format == ProofFormat::Lrat
              ? CheckerRoute::Lrat
              : (format == ProofFormat::DratAscii ||
                         format == ProofFormat::DratBinary
                     ? CheckerRoute::Drat
                     : CheckerRoute::Dsr);
      assert(opened.route == expected);
    }
    assert(std::remove(path.c_str()) == 0);
  }

  {
    Recorder recorder;
    const std::string path = proof_path("bind-after-open");
    {
      Proof proof;
      assert(proof.open(path.c_str(), ProofFormat::DratAscii));
      proof.bind_population_observer(&recorder);
      assert(recorder.checker[0].kind ==
             CheckerPopulationEventKind::ObserverBound);
      assert(recorder.checker[0].route == CheckerRoute::Drat);
    }
    assert(std::remove(path.c_str()) == 0);
  }

  {
    Recorder recorder;
    const std::string path = proof_path("startup-pbp");
    {
      Proof proof;
      proof.bind_population_observer(&recorder);
      assert(proof.open(path.c_str(), ProofFormat::PbpStreaming));
      proof.finish_input_transaction();
      assert(proof.begin_pb());
      assert(recorder.checker_count(
                 CheckerPopulationEventKind::ClausalRouteDiscarded) == 1);
      bool committed_pbp = false;
      for (size_t i = 0; i < recorder.checker_size; ++i)
        committed_pbp |=
            recorder.checker[i].kind ==
                CheckerPopulationEventKind::RouteCommitted &&
            recorder.checker[i].route == CheckerRoute::Pbp;
      assert(committed_pbp);
    }
    assert(std::remove(path.c_str()) == 0);
  }
}

void test_explicit_route_rebind_settles_the_original_observer() {
  Kernel kernel;
  Recorder first, second;
  const std::string path = proof_path("explicit-rebind");
  {
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
    kernel.proof = &proof;
    kernel_bind_population_observer(kernel, &first);
    const Lit one = lit(1);
    proof.add_derived_unit(1, one, nullptr, 0);

    // Replacing a binding must finish the transaction with the observer that
    // saw its staged record.  The replacement starts from a clean boundary.
    kernel_bind_population_observer(kernel, &second);
    assert(first.checker_count(CheckerPopulationEventKind::StageAdd) == 1);
    assert(first.checker_count(
               CheckerPopulationEventKind::PendingCommitBegin) == 1);
    assert(first.checker_count(
               CheckerPopulationEventKind::PendingCommitEnd) == 1);
    assert(first.checker_count(CheckerPopulationEventKind::ObserverUnbound) ==
           1);
    assert(first.checker_count(
               CheckerPopulationEventKind::ObserverUnboundIncomplete) == 0);
    assert(second.checker_count(CheckerPopulationEventKind::StageAdd) == 0);
    assert(second.checker_count(
               CheckerPopulationEventKind::PendingCommitBegin) == 0);

    const Lit two = lit(2);
    proof.add_derived_unit(2, two, nullptr, 0);
    kernel_bind_population_observer(kernel, nullptr);
    assert(second.checker_count(CheckerPopulationEventKind::StageAdd) == 1);
    assert(second.checker_count(
               CheckerPopulationEventKind::PendingCommitBegin) == 1);
    assert(second.checker_count(
               CheckerPopulationEventKind::PendingCommitEnd) == 1);
    assert(second.checker_count(CheckerPopulationEventKind::ObserverUnbound) ==
           1);
    kernel.proof = nullptr;
  }
  assert(std::remove(path.c_str()) == 0);
}

void test_nested_scoped_rebind_settles_each_observer() {
  Kernel kernel;
  Recorder outer, inner;
  const std::string path = proof_path("nested-rebind");
  {
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
    kernel.proof = &proof;
    {
      ScopedPopulationObserverBinding outer_binding(kernel, outer);
      const Lit one = lit(1);
      proof.add_derived_unit(1, one, nullptr, 0);
      {
        ScopedPopulationObserverBinding inner_binding(kernel, inner);
        assert(outer.checker_count(
                   CheckerPopulationEventKind::PendingCommitEnd) == 1);
        const Lit two = lit(2);
        proof.add_derived_unit(2, two, nullptr, 0);
      }
      assert(inner.checker_count(
                 CheckerPopulationEventKind::PendingCommitEnd) == 1);
      const Lit three = lit(3);
      proof.add_derived_unit(3, three, nullptr, 0);
    }
    kernel.proof = nullptr;
  }
  assert(std::remove(path.c_str()) == 0);

  assert(outer.checker_count(CheckerPopulationEventKind::StageAdd) == 2);
  assert(outer.checker_count(
             CheckerPopulationEventKind::PendingCommitBegin) == 2);
  assert(outer.checker_count(CheckerPopulationEventKind::PendingCommitEnd) ==
         2);
  assert(outer.checker_count(
             CheckerPopulationEventKind::ObserverUnboundIncomplete) == 0);
  assert(inner.checker_count(CheckerPopulationEventKind::StageAdd) == 1);
  assert(inner.checker_count(
             CheckerPopulationEventKind::PendingCommitBegin) == 1);
  assert(inner.checker_count(CheckerPopulationEventKind::PendingCommitEnd) ==
         1);
  assert(inner.checker_count(
             CheckerPopulationEventKind::ObserverUnboundIncomplete) == 0);
}

void test_dsr_checker_transactions_and_exact_clauses() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 6);
  Recorder recorder;
  const std::string path = proof_path("dsr");
  {
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
    kernel.proof = &proof;
    {
      ScopedPopulationObserverBinding binding(kernel, recorder);

    const Lit unit = lit(1);
    const Lit deleted[2] = {lit(2), lit(-3)};
    proof.seed_population(7, deleted, 2);
    proof.arm_population_shadow();
    proof.add_derived_unit(10, unit, nullptr, 0);
    proof.del(11, 7, deleted, 2);
    proof.flush();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    assert(!proof.population_contains(7, deleted, 2));
    assert(proof.population_contains(10, &unit, 1));
#endif

    const Lit ordinary[2] = {lit(4), lit(5)};
    proof.add_empty(12, nullptr, 0);
    proof.add_rup(13, ordinary, 2, nullptr, 0);

    const Lit auxiliary_case[2] = {lit(1), lit(2)};
    assert(proof.add_sr(20, auxiliary_case, 2, nullptr, 0, nullptr, 0, 2,
                        true));

    const DsrLiteralMapping lemma_mapping{lit(1), lit(3)};
    assert(proof.add_sr(21, auxiliary_case, 2, nullptr, 0,
                        &lemma_mapping, 1, 3, false));

    const DsrLiteralMapping repivot_mappings[2] = {
        {lit(1), lit(1)}, {lit(2), lit(-1)}};
    assert(proof.add_sr(22, auxiliary_case, 2, nullptr, 0,
                        repivot_mappings, 2, 3, false));
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    assert(proof.population_contains(20, auxiliary_case, 2));
    assert(proof.population_contains(21, auxiliary_case, 2));
    assert(proof.population_contains(22, auxiliary_case, 2));
#endif
    kernel_assign(kernel, lit(6), 0, lit(4), true);
    kernel_materialize_dsr_root_unit(kernel, lit(6));
    proof.flush();
    }
    kernel.proof = nullptr;
  }
  assert(std::remove(path.c_str()) == 0);

  size_t stage_add = MAX_EVENTS, stage_delete = MAX_EVENTS;
  size_t begin = MAX_EVENTS, deletes_commit = MAX_EVENTS, end = MAX_EVENTS;
  uint64_t split_transaction = 0;
  size_t helper_adds = 0, helper_deletes = 0;
  bool saw_final_unit = false, saw_empty = false, saw_rup_after_empty = false;
  bool saw_aux_negative = false, saw_aux_positive = false;
  bool saw_lemma_add = false, saw_lemma_delete = false, saw_repivot = false;
  for (size_t i = 0; i < recorder.checker_size; ++i) {
    const CheckerRecord &event = recorder.checker[i];
    assert(event.route == CheckerRoute::Dsr);
    assert(event.naming == CheckerNaming::LiteralMultiset);
    if (event.kind == CheckerPopulationEventKind::StageAdd &&
        event.line_id == 10) {
      stage_add = i;
      assert(event.role == CheckerClauseRole::DerivedUnit);
      assert(event.clause.equals({1}));
      assert(event.transaction_id != 0);
      split_transaction = event.transaction_id;
    } else if (event.kind ==
               CheckerPopulationEventKind::StageDeleteRequested) {
      stage_delete = i;
      assert(event.clause.equals({2, -3}));
    } else if (event.kind == CheckerPopulationEventKind::PendingCommitBegin &&
               (begin == MAX_EVENTS ||
                event.transaction_id == split_transaction)) {
      begin = i;
    } else if (event.kind ==
                   CheckerPopulationEventKind::PendingDeletesCommit &&
               event.transaction_id == split_transaction) {
      deletes_commit = i;
    } else if (event.kind == CheckerPopulationEventKind::PendingCommitEnd &&
               event.transaction_id == split_transaction) {
      end = i;
    }
    if (event.role == CheckerClauseRole::DsrSplitHelper) {
      assert(event.proof_only &&
             event.source_classification ==
                 PopulationSourceClassification::Ineligible);
      if (event.kind == CheckerPopulationEventKind::Add) {
        ++helper_adds;
        assert(event.clause.equals({1, 4}) ||
               event.clause.equals({1, -4}));
      } else if (event.kind ==
                 CheckerPopulationEventKind::DeleteRequested) {
        ++helper_deletes;
        assert(event.clause.equals({1, 4}) ||
               event.clause.equals({1, -4}));
      }
    }
    if (event.kind == CheckerPopulationEventKind::Add &&
        event.role == CheckerClauseRole::DerivedUnit &&
        event.clause.equals({1}))
      saw_final_unit = true;
    if (event.kind == CheckerPopulationEventKind::Add &&
        event.role == CheckerClauseRole::Empty && event.clause.size == 0)
      saw_empty = true;
    if (event.kind == CheckerPopulationEventKind::Add &&
        event.role == CheckerClauseRole::Rup && event.line_id == 13 &&
        event.clause.equals({4, 5}))
      saw_rup_after_empty = true;

    // The temporary clauses are the checker clauses only. DSR witness data
    // serialized after C must never appear in these population views.
    if (event.role == CheckerClauseRole::DsrSubstitutionHelper &&
        event.kind == CheckerPopulationEventKind::Add &&
        event.clause.equals({-3, 1, 2}))
      saw_aux_negative = true;
    if (event.role == CheckerClauseRole::DsrSubstitutionHelper &&
        event.kind == CheckerPopulationEventKind::Add &&
        event.clause.equals({3, 1, 2}))
      saw_aux_positive = true;
    if (event.role == CheckerClauseRole::DsrSubstitutionHelper &&
        event.clause.equals({1, 2, 3})) {
      saw_lemma_add |= event.kind == CheckerPopulationEventKind::Add;
      saw_lemma_delete |=
          event.kind == CheckerPopulationEventKind::DeleteRequested;
    }
    if (event.role == CheckerClauseRole::SubstitutionRedundancy &&
        event.line_id == 22 && event.clause.equals({2, 1}))
      saw_repivot = true;
  }
  assert(stage_add < stage_delete && stage_delete < begin &&
         begin < deletes_commit && deletes_commit < end);
  assert(helper_adds == 2 && helper_deletes == 2 && saw_final_unit);
  assert(saw_empty && saw_rup_after_empty); // role is scoped and restored
  assert(saw_aux_negative && saw_aux_positive);
  assert(saw_lemma_add && saw_lemma_delete && saw_repivot);
  bool solver_materialized = false;
  for (size_t i = 0; i < recorder.solver_size; ++i)
    solver_materialized |=
        recorder.solver[i].kind ==
            SolverPopulationEventKind::UnitMaterialized &&
        recorder.solver[i].clause.equals({6});
  assert(solver_materialized);
}

void test_format_specific_derived_units_and_external_clauses() {
  for (const ProofFormat format : {ProofFormat::DratAscii,
                                   ProofFormat::Lrat}) {
    Kernel kernel;
    Recorder recorder;
    const std::string path = proof_path(format == ProofFormat::Lrat
                                            ? "lrat"
                                            : "drat");
    {
      Proof proof;
      assert(proof.open(path.c_str(), format));
      kernel.proof = &proof;
      {
        ScopedPopulationObserverBinding binding(kernel, recorder);
        const Lit unit = lit(1);
        const uint64_t chain[1] = {1};
        proof.add_derived_unit(10, unit, chain, 1);
        const Lit named[2] = {lit(2), lit(3)};
        proof.seed_population(2, named, 2);
        proof.del(12, 2, named, 2);
        if (format == ProofFormat::DratAscii) {
          const int32_t clause[2] = {7, -8};
          assert(proof.add_external_rat(11, clause, 2));
          assert(proof.del_external(12, 11, clause, 2));
        }
        proof.flush();
      }
      kernel.proof = nullptr;
    }
    assert(std::remove(path.c_str()) == 0);

    bool derived = false, external_add = false, external_delete = false;
    bool exact_delete = false;
    for (size_t i = 0; i < recorder.checker_size; ++i) {
      const CheckerRecord &event = recorder.checker[i];
      derived |= event.kind == CheckerPopulationEventKind::Add &&
                 event.role == CheckerClauseRole::DerivedUnit &&
                 event.clause.equals({1});
      external_add |= event.kind == CheckerPopulationEventKind::Add &&
                      event.role == CheckerClauseRole::ExternalRat &&
                      event.proof_only && event.clause.equals({7, -8});
      external_delete |=
                         event.kind ==
                             CheckerPopulationEventKind::DeleteRequested &&
                         event.proof_only && event.clause.equals({7, -8});
      exact_delete |= event.kind == CheckerPopulationEventKind::Delete &&
                      event.checker_id == 2 && event.clause.equals({2, 3});
    }
    assert(derived == (format == ProofFormat::Lrat));
    assert(external_add == (format == ProofFormat::DratAscii));
    assert(external_delete == (format == ProofFormat::DratAscii));
    assert(exact_delete == (format == ProofFormat::Lrat));
  }
}

void test_lrat_input_population_and_rebind() {
  Kernel kernel;
  Recorder recorder;
  const std::string path = proof_path("input-lrat");
  {
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::Lrat));
    kernel.proof = &proof;
    {
      ScopedPopulationObserverBinding binding(kernel, recorder);
      FILE *file = dimacs_file(
          "p cnf 3 2\n"
          "1 0\n"
          "-1 2 2 3 0\n");
      assert(parse_dimacs(file, kernel) == nullptr);
      std::fclose(file);
    }
    kernel.proof = nullptr;
  }
  assert(std::remove(path.c_str()) == 0);

  bool seed_unit = false, seed_raw = false, finished = false;
  bool replacement = false, rebind = false;
  uint64_t replacement_id = 0;
  for (size_t i = 0; i < recorder.checker_size; ++i) {
    const CheckerRecord &event = recorder.checker[i];
    assert(event.route == CheckerRoute::Lrat);
    assert(event.naming == CheckerNaming::Id);
    seed_unit |= event.kind == CheckerPopulationEventKind::InputSeed &&
                 event.checker_id == 1 && event.clause.equals({1});
    seed_raw |= event.kind == CheckerPopulationEventKind::InputSeed &&
                event.checker_id == 2 && event.clause.equals({-1, 2, 2, 3});
    if (event.kind == CheckerPopulationEventKind::Add &&
        event.role == CheckerClauseRole::Rup && event.clause.equals({2, 3})) {
      replacement = true;
      replacement_id = event.checker_id;
    }
    rebind |= event.kind == CheckerPopulationEventKind::IdRebind &&
              event.checker_id == replacement_id &&
              event.clause.equals({2, 3});
    finished |= event.kind == CheckerPopulationEventKind::InputFinished;
  }
  assert(seed_unit && seed_raw && replacement && replacement_id > 2);
  for (size_t i = 0; i < recorder.checker_size; ++i)
    if (recorder.checker[i].kind == CheckerPopulationEventKind::InputSeed)
      assert(recorder.checker[i].source_classification ==
             PopulationSourceClassification::PendingInputClassification);
  // Re-evaluate after replacement_id is known; serialization order puts the
  // rebind later, but the assertion should state only the lineage contract.
  rebind = false;
  for (size_t i = 0; i < recorder.checker_size; ++i)
    rebind |= recorder.checker[i].kind ==
                  CheckerPopulationEventKind::IdRebind &&
              recorder.checker[i].predecessor_id == 2 &&
              recorder.checker[i].checker_id == replacement_id &&
              recorder.checker[i].clause.equals({2, 3});
  assert(rebind && finished);

  const RawRecord &outcome =
      recorder.raw_event(RawInputEventKind::Outcome, 2);
  assert((outcome.disposition &
          (RawRepeatedLiteral | RawRootShortened | RawAdmitted | RawBinary)) ==
         (RawRepeatedLiteral | RawRootShortened | RawAdmitted | RawBinary));
  assert(outcome.normalized.equals({2, 3}));
}

void test_kernel_mutation_endpoints_and_representation_silence() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 8);
  Recorder recorder;
  {
    ScopedPopulationObserverBinding binding(kernel, recorder);
    kernel_add_binary_clause(kernel, lit(1), lit(2), 10);
    kernel_add_binary_clause(kernel, lit(1), lit(2), 11);
    assert(kernel_delete_binary_one_copy(kernel, lit(1), lit(2)));
    assert(kernel_delete_binary(kernel, lit(1), lit(2)));

    kernel_add_binary_clause(kernel, lit(6), lit(7), 12);
    kernel_add_binary_clause(kernel, lit(6), lit(7), 13);
    kernel_add_binary_clause(kernel, lit(6), lit(7), 14);
    assert(kernel_remove_duplicate_binary_copies(kernel, lit(6), lit(7)) == 2);
    kernel_add_binary_clause(kernel, lit(7), lit(8), 15);
    kernel_add_binary_clause(kernel, lit(7), lit(8), 16);
    assert(kernel_delete_binary_all_copies(kernel, lit(7), lit(8)));

    const Lit first[4] = {lit(1), lit(3), lit(4), lit(5)};
    Ref first_ref = kernel_add_large_clause(kernel, first, 4, false, 0, 20);
    const size_t semantic_events = recorder.solver_size;
    kernel_promote_watched_lit(kernel, first_ref, 0, 2);
    kernel_collect_garbage(kernel);
    assert(recorder.solver_size == semantic_events);

    // Find the collected live record by id, then replace it by a binary.
    Clause *collected = nullptr;
    for (Ref ref = 0; ref < kernel.arena.size();) {
      Clause *candidate = clause_at(kernel.arena, ref);
      if (!candidate->garbage() && candidate->id() == 20)
        collected = candidate;
      ref += clause_actual_words(candidate);
    }
    assert(collected);
    const Lit binary[2] = {lit(3), lit(4)};
    kernel.next_clause_id = 21;
    assert(kernel_replace_clause(kernel, collected, 21, binary, 2, false, 0,
                                 nullptr) == ReplaceOutcome::Binary);

    const Lit second[4] = {lit(2), lit(5), lit(6), lit(7)};
    const Ref second_ref =
        kernel_add_large_clause(kernel, second, 4, false, 0, 30);
    const Lit shortened[3] = {lit(2), lit(5), lit(6)};
    kernel.next_clause_id = 31;
    kernel_replace_large_clause_in_place(kernel, second_ref, 31, shortened, 3,
                                         0);
  }

  assert(recorder.solver_count(SolverPopulationEventKind::Add) == 11);
  assert(recorder.solver_count(SolverPopulationEventKind::Delete) == 6);
  assert(recorder.replacement_size == 4);
  assert(recorder.replacement[0].phase == SolverReplacementPhase::Begin);
  assert(recorder.replacement[0].predecessor_id == 20);
  assert(recorder.replacement[0].replacement_id == 21);
  assert(recorder.replacement[0].predecessor.equals({4, 3, 1, 5}));
  assert(recorder.replacement[0].replacement.equals({3, 4}));
  assert(recorder.replacement[1].phase == SolverReplacementPhase::End);
  assert(recorder.replacement[1].outcome == SolverReplacementOutcome::Binary);
  assert(recorder.replacement[2].phase == SolverReplacementPhase::Begin);
  assert(recorder.replacement[2].predecessor_id == 30);
  assert(recorder.replacement[2].replacement_id == 31);
  assert(recorder.replacement[3].phase == SolverReplacementPhase::End);
  assert(recorder.replacement[3].outcome == SolverReplacementOutcome::Large);

  bool one_copy = false, two_duplicate_copies = false, two_all_copies = false;
  for (size_t i = 0; i < recorder.solver_size; ++i)
    if (recorder.solver[i].kind == SolverPopulationEventKind::Delete &&
        recorder.solver[i].clause_kind == SolverClauseKind::Binary) {
      one_copy |= recorder.solver[i].copies == 1 &&
                  recorder.solver[i].clause.equals({1, 2});
      two_duplicate_copies |= recorder.solver[i].copies == 2 &&
                              recorder.solver[i].clause.equals({6, 7});
      two_all_copies |= recorder.solver[i].copies == 2 &&
                        recorder.solver[i].clause.equals({7, 8});
    }
  assert(one_copy && two_duplicate_copies && two_all_copies);
}

void test_metadata_promotion_and_empty_unit_replacements() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 9);
  Recorder recorder;
  {
    ScopedPopulationObserverBinding binding(kernel, recorder);
    const Lit redundant[3] = {lit(1), lit(2), lit(3)};
    const Ref promoted =
        kernel_add_large_clause(kernel, redundant, 3, true, 2, 40);
    const size_t before = recorder.solver_size;
    kernel_promote_large_irredundant(kernel, promoted);
    assert(recorder.solver_size == before + 1);
    const SolverRecord &metadata = recorder.solver[before];
    assert(metadata.kind == SolverPopulationEventKind::MetadataUpdate);
    assert(metadata.id == 40 && !metadata.redundant);
    assert(metadata.clause.equals({1, 2, 3}));

    const Lit empty_source[3] = {lit(4), lit(5), lit(6)};
    Clause *empty_clause = clause_at(
        kernel.arena,
        kernel_add_large_clause(kernel, empty_source, 3, false, 0, 50));
    kernel.next_clause_id = 51;
    assert(kernel_replace_clause(kernel, empty_clause, 51, nullptr, 0, false,
                                 0, nullptr) == ReplaceOutcome::Refuted);

    // The replacement primitive exposes the exact unit image and lineage; the
    // caller remains responsible for installing the root assignment.
    kernel.inconsistent = false;
    const Lit unit_source[3] = {lit(7), lit(8), lit(9)};
    Clause *unit_clause = clause_at(
        kernel.arena,
        kernel_add_large_clause(kernel, unit_source, 3, false, 0, 60));
    const Lit unit = lit(7);
    kernel.next_clause_id = 61;
    assert(kernel_replace_clause(kernel, unit_clause, 61, &unit, 1, false, 0,
                                 nullptr) == ReplaceOutcome::Unit);
  }

  bool empty_end = false, unit_end = false;
  for (size_t i = 0; i < recorder.replacement_size; ++i) {
    const ReplacementRecord &event = recorder.replacement[i];
    empty_end |= event.phase == SolverReplacementPhase::End &&
                 event.outcome == SolverReplacementOutcome::Refuted &&
                 event.predecessor_id == 50 && event.replacement_id == 51 &&
                 event.replacement.size == 0;
    unit_end |= event.phase == SolverReplacementPhase::End &&
                event.outcome == SolverReplacementOutcome::Unit &&
                event.predecessor_id == 60 && event.replacement_id == 61 &&
                event.replacement.equals({7});
  }
  assert(empty_end && unit_end);
}

void test_rebuilt_binary_publication_endpoints() {
  {
    Kernel kernel;
    kernel_reserve_vars(kernel, 2);
    Recorder recorder;
    {
      ScopedPopulationObserverBinding binding(kernel, recorder);
      kernel.watches.push_binary(lit(1), lit(2));
      kernel_add_binary_mirror(kernel, lit(1), lit(2), 41);
    }
    assert(recorder.solver_size == 0);
  }

  for (const BinaryPopulationEffect effect : {
           BinaryPopulationEffect::RepresentationOnly,
           BinaryPopulationEffect::LogicalAdd}) {
    Kernel kernel;
    kernel_reserve_vars(kernel, 2);
    Recorder recorder;
    {
      ScopedPopulationObserverBinding binding(kernel, recorder);
      kernel.watches.push_binary(lit(1), lit(2));
      kernel.watches.push_binary(lit(2), lit(1));
      kernel_publish_binary_clause(kernel, lit(1), lit(2), 42, true,
                                   effect);
    }
    if (effect == BinaryPopulationEffect::RepresentationOnly) {
      assert(recorder.solver_size == 0);
      continue;
    }
    assert(recorder.solver_size == 1);
    assert(recorder.solver[0].kind == SolverPopulationEventKind::Add);
    assert(recorder.solver[0].clause_kind == SolverClauseKind::Binary);
    assert(recorder.solver[0].id == 42);
    assert(recorder.solver[0].copies == 1);
    assert(recorder.solver[0].clause.equals({1, 2}));
  }
}

void test_variable_compaction_is_population_silent() {
  Kernel kernel;
  ScopedHeuristics heuristics(kernel);
  kernel_reserve_vars(kernel, 3);
  const Lit clause[2] = {lit(1), lit(3)};
  kernel_add_binary_clause(kernel, clause[0], clause[1], 7);
  kernel.var_status.assign(kernel.nvars, VarStatus::Active);
  kernel.var_status[1] = VarStatus::Eliminated;
  heuristics_deactivate_var(kernel, 1);
  --kernel.unassigned_active;

  Recorder recorder;
  {
    ScopedPopulationObserverBinding binding(kernel, recorder);
    assert(kernel_compact_variables(kernel));
  }
  assert(recorder.solver_size == 0);
  assert(kernel.nvars == 2);
  assert(kernel_binary_live(kernel, kernel_internalize(kernel, 1),
                            kernel_internalize(kernel, 3)));
}

void test_substitute_binary_population_rewrite() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 3);
  // x1 <-> x2, plus a payload clause that must become (x1 v x3).
  kernel_add_binary_clause(kernel, lit(-1), lit(2), 1);
  kernel_add_binary_clause(kernel, lit(-2), lit(1), 2);
  kernel_add_binary_clause(kernel, lit(2), lit(3), 3);

  Recorder recorder;
  recorder.invariant_kernel = &kernel;
  {
    ScopedPopulationObserverBinding binding(kernel, recorder);
    assert(substitute_apply(kernel) == ApplyResult::Ok);
  }

  bool retired_payload = false, added_image = false;
  size_t begin = MAX_EVENTS, first_stage = MAX_EVENTS, commit = MAX_EVENTS;
  uint64_t transaction = 0;
  for (size_t i = 0; i < recorder.solver_size; ++i) {
    const SolverRecord &event = recorder.solver[i];
    if (event.kind == SolverPopulationEventKind::TransactionBegin) {
      begin = i;
      transaction = event.transaction_id;
    }
    if (event.kind == SolverPopulationEventKind::StageAdd ||
        event.kind == SolverPopulationEventKind::StageDelete)
      first_stage = std::min(first_stage, i);
    if (event.kind == SolverPopulationEventKind::TransactionCommit) {
      commit = i;
      assert(event.transaction_id == transaction);
    }
    retired_payload |= event.kind == SolverPopulationEventKind::StageDelete &&
                       event.clause.equals({2, 3});
    added_image |= event.kind == SolverPopulationEventKind::StageAdd &&
                   event.clause.equals({1, 3});
    if (event.kind == SolverPopulationEventKind::StageAdd ||
        event.kind == SolverPopulationEventKind::StageDelete)
      assert(event.transaction_id == transaction);
  }
  assert(retired_payload && added_image);
  assert(begin < first_stage && first_stage < commit);
  assert(!kernel_binary_live(kernel, lit(2), lit(3)));
  assert(kernel_binary_live(kernel, lit(1), lit(3)));
}

void assert_one_committed_binary_transaction(const Recorder &recorder) {
  size_t begin = MAX_EVENTS, commit = MAX_EVENTS;
  uint64_t transaction = 0;
  for (size_t i = 0; i < recorder.solver_size; ++i) {
    const SolverRecord &event = recorder.solver[i];
    if (event.kind == SolverPopulationEventKind::TransactionBegin) {
      assert(begin == MAX_EVENTS);
      begin = i;
      transaction = event.transaction_id;
    } else if (event.kind == SolverPopulationEventKind::TransactionCommit) {
      assert(commit == MAX_EVENTS);
      commit = i;
      assert(event.transaction_id == transaction);
    } else if (event.kind == SolverPopulationEventKind::StageAdd ||
               event.kind == SolverPopulationEventKind::StageDelete) {
      assert(begin < i && commit == MAX_EVENTS);
      assert(event.transaction_id == transaction);
    }
  }
  assert(begin < commit && transaction != 0);
}

void test_substitute_transaction_preexisting_coalescing_and_duplicates() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 3);
  kernel_add_binary_clause(kernel, lit(-1), lit(2), 1);
  kernel_add_binary_clause(kernel, lit(-2), lit(1), 2);
  kernel_add_binary_clause(kernel, lit(1), lit(3), 3); // pre-existing target
  kernel_add_binary_clause(kernel, lit(2), lit(3), 4);
  kernel_add_binary_clause(kernel, lit(2), lit(3), 5); // duplicate source copy

  Recorder recorder;
  recorder.invariant_kernel = &kernel;
  {
    ScopedPopulationObserverBinding binding(kernel, recorder);
    assert(substitute_apply(kernel) == ApplyResult::Ok);
  }
  assert_one_committed_binary_transaction(recorder);

  size_t source_deletes = 0, target_adds = 0;
  for (size_t i = 0; i < recorder.solver_size; ++i) {
    const SolverRecord &event = recorder.solver[i];
    if (event.kind == SolverPopulationEventKind::StageDelete &&
        event.clause.equals({2, 3}))
      source_deletes += event.copies;
    if (event.kind == SolverPopulationEventKind::StageAdd &&
        event.clause.equals({1, 3}))
      target_adds += event.copies;
  }
  assert(source_deletes == 2 && target_adds == 2);
  assert(!kernel_binary_live(kernel, lit(2), lit(3)));
  assert(kernel_binary_copies(kernel, lit(1), lit(3)) == 3);
}

void test_substitute_lrat_transaction() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 3);
  Recorder recorder;
  recorder.invariant_kernel = &kernel;
  const std::string path = proof_path("substitute-lrat");
  {
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::Lrat));
    kernel.proof = &proof;
    kernel_add_binary_clause(kernel, lit(-1), lit(2), 1);
    kernel_add_binary_clause(kernel, lit(-2), lit(1), 2);
    kernel_add_binary_clause(kernel, lit(2), lit(3), 3);
    {
      ScopedPopulationObserverBinding binding(kernel, recorder);
      assert(substitute_apply(kernel) == ApplyResult::Ok);
    }
    kernel.proof = nullptr;
  }
  assert(std::remove(path.c_str()) == 0);
  assert_one_committed_binary_transaction(recorder);
  bool exact_checker_delete = false;
  for (size_t i = 0; i < recorder.checker_size; ++i)
    exact_checker_delete |=
        recorder.checker[i].kind == CheckerPopulationEventKind::Delete &&
        recorder.checker[i].naming == CheckerNaming::Id;
  assert(exact_checker_delete);
}

void test_substitute_transaction_collapse_and_tautology() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 2);
  kernel_add_binary_clause(kernel, lit(-1), lit(2), 1);
  kernel_add_binary_clause(kernel, lit(-2), lit(1), 2);
  kernel_add_binary_clause(kernel, lit(1), lit(2), 3);  // unit image
  kernel_add_binary_clause(kernel, lit(-1), lit(2), 4); // tautological image

  Recorder recorder;
  recorder.invariant_kernel = &kernel;
  {
    ScopedPopulationObserverBinding binding(kernel, recorder);
    assert(substitute_apply(kernel) == ApplyResult::Ok);
  }
  assert_one_committed_binary_transaction(recorder);
  bool collapsed = false;
  size_t tautology_deletes = 0;
  bool staged_bad_image = false;
  for (size_t i = 0; i < recorder.solver_size; ++i) {
    const SolverRecord &event = recorder.solver[i];
    if (event.kind == SolverPopulationEventKind::StageDelete &&
        event.clause.equals({1, 2}))
      collapsed = true;
    if (event.kind == SolverPopulationEventKind::StageDelete &&
        event.clause.equals({-1, 2}))
      tautology_deletes += event.copies;
    staged_bad_image |= event.kind == SolverPopulationEventKind::StageAdd &&
                        (event.clause.equals({1, 1}) ||
                         event.clause.equals({-1, 1}));
  }
  assert(collapsed && tautology_deletes == 2 && !staged_bad_image);
  assert(kernel.value(lit(1)) > 0);
}

void test_substitute_sweep_session_callbacks_see_mirrored_watches() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 4);
  // Two independently retired endpoints force the session path to rebuild
  // both directed watches of the payload image (x2 v x4) -> (x1 v x3).
  kernel_add_binary_clause(kernel, lit(-1), lit(2), 1);
  kernel_add_binary_clause(kernel, lit(1), lit(-2), 2);
  kernel_add_binary_clause(kernel, lit(-3), lit(4), 3);
  kernel_add_binary_clause(kernel, lit(3), lit(-4), 4);
  kernel_add_binary_clause(kernel, lit(2), lit(4), 5);
  OccIndex occurrences;
  kernel_dense_connect(kernel, occurrences, true);
  SweepSubstitutionSession session;
  session.begin(kernel);

  Recorder recorder;
  recorder.invariant_kernel = &kernel;
  recorder.check_full_kernel = false;
  {
    ScopedPopulationObserverBinding binding(kernel, recorder);
    assert(substitute_apply_sweep_equivalences(
               kernel, {{lit(1), lit(2)}, {lit(3), lit(4)}}, session) ==
           ApplyResult::Ok);
  }
  assert_one_committed_binary_transaction(recorder);
  bool retired_payload = false, added_image = false;
  for (size_t i = 0; i < recorder.solver_size; ++i) {
    const SolverRecord &event = recorder.solver[i];
    retired_payload |=
        event.kind == SolverPopulationEventKind::StageDelete &&
        event.clause.equals({2, 4});
    added_image |= event.kind == SolverPopulationEventKind::StageAdd &&
                   event.clause.equals({1, 3});
  }
  assert(retired_payload && added_image);
  assert(!kernel_binary_live(kernel, lit(2), lit(4)));
  assert(kernel_binary_live(kernel, lit(1), lit(3)));
  kernel_check_invariants(kernel);
  kernel_dense_disconnect(kernel);
}

void test_scope_flushes_before_unbinding() {
  Kernel kernel;
  Recorder recorder;
  const std::string path = proof_path("scope");
  {
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
    kernel.proof = &proof;
    {
      ScopedPopulationObserverBinding binding(kernel, recorder);
      const Lit unit = lit(1);
      proof.add_derived_unit(1, unit, nullptr, 0);
    }
    assert(proof.population_observer() == nullptr);
    kernel.proof = nullptr;
  }
  assert(std::remove(path.c_str()) == 0);
  assert(recorder.checker_count(CheckerPopulationEventKind::StageAdd) == 1);
  assert(recorder.checker_count(CheckerPopulationEventKind::PendingCommitBegin) ==
         1);
  assert(recorder.checker_count(CheckerPopulationEventKind::PendingCommitEnd) ==
         1);
  assert(recorder.checker[recorder.checker_size - 1].kind ==
         CheckerPopulationEventKind::ObserverUnbound);
}

} // namespace

int main() {
  test_no_proof_raw_and_solver_population();
  test_raw_empty_and_long_tautology_classification();
  test_duplicate_unit_and_large_input_classification();
  test_proof_route_lifecycle_and_streaming_pbp_unbind();
  test_explicit_route_rebind_settles_the_original_observer();
  test_nested_scoped_rebind_settles_each_observer();
  test_dsr_checker_transactions_and_exact_clauses();
  test_format_specific_derived_units_and_external_clauses();
  test_lrat_input_population_and_rebind();
  test_kernel_mutation_endpoints_and_representation_silence();
  test_metadata_promotion_and_empty_unit_replacements();
  test_rebuilt_binary_publication_endpoints();
  test_variable_compaction_is_population_silent();
  test_substitute_binary_population_rewrite();
  test_substitute_transaction_preexisting_coalescing_and_duplicates();
  test_substitute_lrat_transaction();
  test_substitute_transaction_collapse_and_tautology();
  test_substitute_sweep_session_callbacks_see_mirrored_watches();
  test_scope_flushes_before_unbinding();
  return 0;
}
