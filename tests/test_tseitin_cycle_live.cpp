#undef NDEBUG
#include "proof/proof.hpp"
#include "rules/lucky/tseitin_cycle_live.hpp"
#include "rules/lucky/parity_certificate.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <unistd.h>

using namespace tactix;
namespace live = tactix::tseitin_cycle_live;
namespace cycle = tactix::tseitin_cycle;
using tactix::parity_certificate::xor_equation;

namespace {

struct Equation {
  bool rhs;
  std::vector<int32_t> variables;
};

const std::vector<Equation> triangle_equations = {
    {true, {1, 3}}, {false, {1, 2}}, {false, {2, 3}}};

Kernel triangle(bool residual = false) {
  Kernel kernel;
  kernel_reserve_vars(kernel, residual ? 4 : 3);
  for (const Equation &equation : triangle_equations) {
    const auto clauses = xor_equation(equation.variables.data(),
                                      equation.variables.size(), equation.rhs);
    for (const auto &clause : clauses) {
      assert(clause.size == 2);
      kernel_add_binary_clause(kernel, internalize(clause.lits[0]),
                               internalize(clause.lits[1]),
                               ++kernel.next_clause_id);
    }
  }
  if (residual)
    kernel_add_binary_clause(kernel, internalize(1), internalize(4),
                             ++kernel.next_clause_id);
  return kernel;
}

live::Request request() {
  live::Request result;
  result.snapshot = {100, 1000, 10000};
  result.admission = {10000, nullptr, nullptr};
  result.planning = {10000, 16, 1};
  result.transaction = {10000, 3, 2, 2, 100, nullptr, nullptr};
  result.maximum_validation_ticks = 100000;
  return result;
}

std::vector<int32_t> clause_key(std::vector<int32_t> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

std::vector<cycle::CertifiedGroup> groups(
    const live::Snapshot &snapshot,
    const std::vector<Equation> &equations = triangle_equations) {
  std::vector<cycle::CertifiedGroup> result;
  uint64_t identity = 100;
  for (const Equation &equation : equations) {
    cycle::CertifiedGroup group;
    group.identity = identity++;
    group.certified.equation.size = equation.variables.size();
    group.certified.equation.rhs = equation.rhs;
    for (size_t index = 0; index < equation.variables.size(); ++index)
      group.certified.equation.variables[index] = equation.variables[index];
    const auto expected = xor_equation(equation.variables.data(),
                                       equation.variables.size(), equation.rhs);
    for (const auto &clause : expected) {
      const std::vector<int32_t> wanted = clause_key(
          {clause.lits.begin(), clause.lits.begin() + clause.size});
      const auto found = std::find_if(
          snapshot.sources.begin(), snapshot.sources.end(),
          [&](const live::Source &source) {
            return clause_key(source.clause) == wanted;
          });
      assert(found != snapshot.sources.end());
      group.certified.premises.push_back(found->occurrence);
      group.premise_proofs.push_back(found->native_proof_id);
    }
    result.push_back(std::move(group));
  }
  return result;
}

live::Snapshot snapshot(Kernel &);

void compacted_external_mapping() {
  const std::vector<Equation> equations = {
      {true, {2, 4}}, {false, {2, 3}}, {false, {3, 4}}};
  Kernel kernel;
  kernel_reserve_vars(kernel, 4);
  for (const Equation &equation : equations) {
    const auto clauses = xor_equation(equation.variables.data(),
                                      equation.variables.size(), equation.rhs);
    for (const auto &clause : clauses)
      kernel_add_binary_clause(kernel, internalize(clause.lits[0]),
                               internalize(clause.lits[1]),
                               ++kernel.next_clause_id);
  }
  kernel.var_status.assign(kernel.nvars, VarStatus::Active);
  kernel.var_status[0] = VarStatus::Eliminated;
  --kernel.unassigned_active;
  assert(kernel_compact_variables(kernel));
  assert((kernel.internal_to_external == std::vector<Var>{1, 2, 3}));

  const live::Snapshot captured = snapshot(kernel);
  live::Prepared prepared;
  assert(live::prepare(kernel, captured, groups(captured, equations), request(),
                       prepared)
             .status == live::Status::Prepared);
  assert(prepared.transaction().identity.pivot == 4);
  assert(kernel_externalize(kernel, prepared.instance().clause.front()) == -4);
  assert(var_of(prepared.instance().clause.front()) == 2);
  for (const auto &[source, target] : prepared.instance().substitution) {
    assert(target == negate(source));
    assert(kernel_externalize(kernel, source) == 2 ||
           kernel_externalize(kernel, source) == 3);
  }
  assert(live::apply(kernel, prepared).status == live::Status::Applied);
  assert(kernel.value(kernel_internalize(kernel, -4)) > 0);
  kernel_check_invariants(kernel);
}

void large_clause_population() {
  const std::vector<Equation> equations = {
      {true, {1, 2, 3}}, {false, {1, 4, 5}},
      {false, {2, 4, 6}}, {false, {3, 5, 6}}};
  Kernel kernel;
  kernel_reserve_vars(kernel, 6);
  for (const Equation &equation : equations) {
    const auto clauses = xor_equation(equation.variables.data(),
                                      equation.variables.size(), equation.rhs);
    for (const auto &clause : clauses) {
      std::vector<Lit> literals;
      for (uint32_t index = 0; index < clause.size; ++index)
        literals.push_back(internalize(clause.lits[index]));
      kernel_add_large_clause(kernel, literals.data(), literals.size(), false,
                              0, ++kernel.next_clause_id);
    }
  }
  const live::Snapshot captured = snapshot(kernel);
  assert(captured.sources.size() == 16 && captured.literals == 48);
  for (const live::Source &source : captured.sources) {
    assert(source.kind == live::SourceKind::Large);
    assert(source.ref != INVALID_REF && source.clause.size() == 3);
  }
  live::Prepared prepared;
  assert(live::prepare(kernel, captured, groups(captured, equations), request(),
                       prepared)
             .status == live::Status::Prepared);
  assert(prepared.transaction().identity.pivot == 4);
  assert((prepared.transaction().identity.cycle_edges ==
          std::vector<cycle::Edge>{4, 1, 2}));
  assert(live::apply(kernel, prepared).status == live::Status::Applied);
  assert(kernel.value(internalize(-4)) > 0);
  kernel_check_invariants(kernel);
}

live::Snapshot snapshot(Kernel &kernel) {
  live::Snapshot result;
  const live::Result captured = live::capture(kernel, request().snapshot, result);
  assert(captured.status == live::Status::Captured);
  return result;
}

std::string temporary_path() {
  char pattern[] = "/tmp/tseitin-cycle-live-XXXXXX";
  const int descriptor = mkstemp(pattern);
  assert(descriptor >= 0);
  close(descriptor);
  return pattern;
}

std::string read_file(const std::string &path) {
  FILE *file = std::fopen(path.c_str(), "rb");
  assert(file);
  std::string output;
  char bytes[4096];
  while (const size_t count = std::fread(bytes, 1, sizeof bytes, file))
    output.append(bytes, count);
  assert(!std::ferror(file));
  assert(!std::fclose(file));
  return output;
}

void exact_snapshot_prepare_apply() {
  Kernel kernel = triangle();
  const live::Snapshot captured = snapshot(kernel);
  assert(captured.sources.size() == 6 && captured.clauses.size() == 6);
  assert(captured.literals == 12);
  for (size_t index = 0; index < captured.sources.size(); ++index) {
    assert(captured.sources[index].occurrence == index + 1);
    assert(captured.sources[index].native_proof_id == index + 1);
    assert(captured.sources[index].kind == live::SourceKind::Binary);
    assert(captured.clauses[index].occurrence == index + 1);
    assert(captured.clauses[index].proof == index + 1);
  }

  live::Prepared prepared;
  const live::Result admitted =
      live::prepare(kernel, captured, groups(captured), request(), prepared);
  assert(admitted.status == live::Status::Prepared && prepared.ready());
  assert(admitted.admission_reason == cycle::AdmissionReason::None);
  assert(admitted.planning_reason == cycle::DeclineReason::None);
  assert(admitted.transaction_reason == cycle::TransactionReason::None);
  assert(admitted.substitution_status ==
         SubstitutionRedundancyApplicationStatus::Applied);
  assert(prepared.transaction().identity.pivot == 3);
  assert((prepared.transaction().identity.cycle_edges ==
          std::vector<cycle::Edge>{3, 1, 2}));
  assert((prepared.instance().clause == std::vector<Lit>{internalize(-3)}));
  assert((prepared.instance().substitution ==
          std::vector<std::pair<Lit, Lit>>{{internalize(1), internalize(-1)},
                                          {internalize(2), internalize(-2)}}));
  const uint64_t before_id = kernel.next_clause_id;
  const live::Result applied = live::apply(kernel, prepared);
  assert(applied.status == live::Status::Applied);
  assert(applied.result == ApplyResult::Ok);
  assert(applied.substitution_status ==
         SubstitutionRedundancyApplicationStatus::Applied);
  assert(kernel.next_clause_id == before_id + 1);
  assert(kernel.value(internalize(-3)) > 0);
  assert(kernel.unit_ids[var_of(internalize(-3))] == kernel.next_clause_id);
  kernel_check_invariants(kernel);

  // The immutable transaction cannot be replayed after its own unit changes
  // the exact formula/root snapshot.
  assert(live::apply(kernel, prepared).status == live::Status::Inapplicable);
}

void dsr_publication() {
  const std::string path = temporary_path();
  {
    Kernel kernel = triangle();
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
    proof.bind_variable_map(&kernel.internal_to_external);
    proof.bind_external_variable_map(&kernel.external_to_internal);
    kernel.proof = &proof;
    const live::Snapshot captured = snapshot(kernel);
    live::Prepared prepared;
    assert(live::prepare(kernel, captured, groups(captured), request(), prepared)
               .status == live::Status::Prepared);
    assert(live::apply(kernel, prepared).status == live::Status::Applied);
    proof.flush();
    kernel.proof = nullptr;
  }
  const std::string proof = read_file(path);
  assert(!proof.empty());
  assert(proof.find("-3 0") != std::string::npos);
  unlink(path.c_str());
}

void resource_and_interrupt_boundaries() {
  {
    Kernel kernel = triangle();
    live::Snapshot output;
    auto limits = request().snapshot;
    limits.maximum_clauses = 5;
    const live::Result result = live::capture(kernel, limits, output);
    assert(result.status == live::Status::ResourceExhausted);
    assert(result.reason == live::Reason::SnapshotClauseLimit);
    assert(output.sources.empty());
  }
  {
    Kernel kernel = triangle();
    live::Snapshot output;
    auto limits = request().snapshot;
    limits.maximum_literals = 11;
    const live::Result result = live::capture(kernel, limits, output);
    assert(result.status == live::Status::ResourceExhausted);
    assert(result.reason == live::Reason::SnapshotLiteralLimit);
    assert(output.sources.empty());
  }
  {
    Kernel kernel = triangle();
    live::Snapshot output;
    auto limits = request().snapshot;
    limits.maximum_work = 1;
    const live::Result result = live::capture(kernel, limits, output);
    assert(result.status == live::Status::ResourceExhausted);
    assert(result.reason == live::Reason::SnapshotWorkLimit);
  }
  {
    Kernel kernel = triangle();
    const live::Snapshot captured = snapshot(kernel);
    auto limited = request();
    limited.transaction.additions = 2;
    live::Prepared prepared;
    const live::Result result =
        live::prepare(kernel, captured, groups(captured), limited, prepared);
    assert(result.status == live::Status::ResourceExhausted);
    assert(result.transaction_reason == cycle::TransactionReason::AdditionLimit);
    assert(!prepared.ready());
    // Reusing an output object cannot retain an earlier authorized tuple.
    auto ample = request();
    assert(live::prepare(kernel, captured, groups(captured), ample, prepared)
               .status == live::Status::Prepared);
    assert(prepared.ready());
    assert(live::prepare(kernel, captured, groups(captured), limited, prepared)
               .status == live::Status::ResourceExhausted);
    assert(!prepared.ready());
  }
  {
    Kernel kernel = triangle();
    const live::Snapshot captured = snapshot(kernel);
    auto limited = request();
    limited.maximum_validation_ticks = 0;
    live::Prepared prepared;
    const live::Result result =
        live::prepare(kernel, captured, groups(captured), limited, prepared);
    assert(result.status == live::Status::ResourceExhausted);
    assert(result.reason == live::Reason::SemanticPreflight);
    assert(!prepared.ready());
  }
  {
    Kernel kernel = triangle();
    volatile sig_atomic_t stop = 1;
    kernel.stop = &stop;
    live::Snapshot output;
    assert(live::capture(kernel, request().snapshot, output).status ==
           live::Status::Interrupted);
  }
  {
    Kernel kernel = triangle();
    const live::Snapshot captured = snapshot(kernel);
    live::Prepared prepared;
    assert(live::prepare(kernel, captured, groups(captured), request(), prepared)
               .status == live::Status::Prepared);
    const uint64_t before_id = kernel.next_clause_id;
    volatile sig_atomic_t stop = 1;
    kernel.stop = &stop;
    const live::Result result = live::apply(kernel, prepared);
    assert(result.status == live::Status::Interrupted);
    assert(kernel.next_clause_id == before_id);
    assert(!kernel.value(internalize(-3)));
  }
}

void stale_and_invalid_declines_are_atomic() {
  {
    Kernel kernel = triangle();
    live::Snapshot captured = snapshot(kernel);
    const uint64_t before_id = kernel.next_clause_id;
    ++captured.clauses.front().proof;
    live::Prepared prepared;
    const live::Result result =
        live::prepare(kernel, captured, groups(captured), request(), prepared);
    assert(result.status == live::Status::Stale);
    assert(result.reason == live::Reason::SnapshotMismatch);
    assert(kernel.next_clause_id == before_id && !prepared.ready());
  }
  {
    Kernel kernel = triangle();
    const live::Snapshot captured = snapshot(kernel);
    auto selected = groups(captured);
    ++selected.front().premise_proofs.front();
    live::Prepared prepared;
    const live::Result result =
        live::prepare(kernel, captured, selected, request(), prepared);
    assert(result.status == live::Status::Invalid);
    assert(result.admission_reason ==
           cycle::AdmissionReason::PremiseIdentityMismatch);
    assert(!prepared.ready());
  }
  {
    Kernel kernel = triangle(true);
    const live::Snapshot captured = snapshot(kernel);
    live::Prepared prepared;
    const live::Result result =
        live::prepare(kernel, captured, groups(captured), request(), prepared);
    assert(result.status == live::Status::Declined);
    assert(result.admission_reason == cycle::AdmissionReason::ResidualEdgeTouch);
    assert(!prepared.ready());
  }
  {
    Kernel kernel = triangle();
    const live::Snapshot captured = snapshot(kernel);
    live::Prepared prepared;
    assert(live::prepare(kernel, captured, groups(captured), request(), prepared)
               .status == live::Status::Prepared);
    const uint64_t before_id = kernel.next_clause_id;
    ++kernel.next_clause_id;
    const live::Result result = live::apply(kernel, prepared);
    assert(result.status == live::Status::Stale);
    assert(result.reason == live::Reason::SnapshotMismatch);
    assert(kernel.next_clause_id == before_id + 1);
    assert(!kernel.value(internalize(-3)));
  }
  {
    Kernel kernel = triangle();
    kernel_add_binary_clause(kernel, internalize(1), internalize(-2),
                             ++kernel.next_clause_id);
    // A second identical occurrence has no independently addressable live
    // binary proof identity in the kernel map, so exact capture must refuse it.
    kernel_add_binary_clause(kernel, internalize(1), internalize(-2),
                             ++kernel.next_clause_id);
    live::Snapshot output;
    const live::Result result = live::capture(kernel, request().snapshot, output);
    assert(result.status == live::Status::Invalid);
    assert(result.reason == live::Reason::InvalidKernelPopulation);
  }
}

void unsupported_proof_is_precommit() {
  const std::string path = temporary_path();
  Kernel kernel = triangle();
  const live::Snapshot captured = snapshot(kernel);
  Proof proof;
  assert(proof.open(path.c_str(), ProofFormat::DratAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  kernel.proof = &proof;
  const uint64_t before_id = kernel.next_clause_id;
  live::Prepared prepared;
  const live::Result result =
      live::prepare(kernel, captured, groups(captured), request(), prepared);
  assert(result.status == live::Status::UnsupportedProof);
  assert(result.reason == live::Reason::UnsupportedProof);
  assert(kernel.next_clause_id == before_id && !prepared.ready());
  proof.flush();
  kernel.proof = nullptr;
  assert(read_file(path).empty());
  unlink(path.c_str());
}

} // namespace

int main() {
  exact_snapshot_prepare_apply();
  compacted_external_mapping();
  large_clause_population();
  dsr_publication();
  resource_and_interrupt_boundaries();
  stale_and_invalid_declines_are_atomic();
  unsupported_proof_is_precommit();
  std::puts("test_tseitin_cycle_live: passed");
}
