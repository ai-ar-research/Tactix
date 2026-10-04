#undef NDEBUG
#include "rules/semantic_extension/disjunction.hpp"
#include "rules/propagate/propagate.hpp"
#include "proof/proof.hpp"
#include <cassert>
#include <cstdio>

using namespace tactix;

static DisjunctionDefinitionRequest request(FILE *records) {
  return {{1, -2, 3}, 3, 100, 4, 1, 1, records};
}

static void projection_and_propagation() {
  // Every old assignment has exactly its expected extension, including when
  // it was already propagated before the new definition was installed.
  for (bool before : {false, true}) for (unsigned mask = 0; mask < 8; ++mask) {
    Kernel k;
    kernel_reserve_vars(k, 3);
    auto assign = [&] {
      for (Var v = 0; v < 3; ++v) {
        const Lit lit = make_lit(v, !(mask & (1u << v)));
        if (!k.value(lit)) kernel_assign_unit(k, lit);
        else assert(k.value(lit) > 0);
      }
      assert(propagate_service(k) == ApplyResult::Ok);
    };
    if (before) assign();
    FILE *records = std::tmpfile(); assert(records);
    const auto result = define_disjunction(k, request(records));
    assert(result.result == ApplyResult::Ok && result.output == 4);
    assert(result.additions == 4 && result.literal_occurrences == 10);
    assert(kernel_num_output_vars(k) == 3 && k.nvars == 4);
    if (!before) assign();
    assert(propagate_service(k) == ApplyResult::Ok);
    const bool expected = (mask & 1) || !(mask & 2) || (mask & 4);
    assert(k.value(kernel_internalize(k, expected ? 4 : -4)) == 1);
    kernel_extend_model(k);
    for (Var v = 0; v < 3; ++v)
      assert(k.value(make_lit(v, !(mask & (1u << v)))) == 1);
    std::fclose(records);
  }
}

static void admission() {
  Kernel k; kernel_reserve_vars(k, 3);
  FILE *records = std::tmpfile(); assert(records);
  auto declined = [&](const DisjunctionDefinitionRequest &r) {
    const auto map = k.external_to_internal;
    const auto id = k.next_clause_id;
    const auto arena = k.arena.size();
    assert(define_disjunction(k, r).result == ApplyResult::NoOp);
    assert(k.external_to_internal == map && k.next_clause_id == id);
    assert(k.arena.size() == arena && std::ftell(records) == 0);
  };
  for (auto inputs : {std::vector<int32_t>{}, {0}, {INT32_MIN}, {4},
                      {1, 1}, {1, -1}, {1, 2, 3, 4}}) {
    auto r = request(records); r.inputs = inputs; declined(r);
  }
  auto r = request(records); r.maximum_additions = 3; declined(r);
  r = request(records); r.maximum_external_extent = 3; declined(r);
  k.var_status.resize(k.nvars, VarStatus::Active);
  k.var_status[0] = VarStatus::Eliminated; declined(request(records));
  k.var_status[0] = VarStatus::Substituted; declined(request(records));
  k.var_status[0] = VarStatus::Active;
  k.binary_only_head = 0; k.propagate_head = 1;
  declined(request(records)); k.propagate_head = 0; k.binary_only_head = UINT32_MAX;
  Var first; assert(kernel_reserve_proof_variables(k, 2, first));
  r = request(records); r.inputs = {4}; declined(r);
  const auto result = define_disjunction(k, request(records));
  assert(result.output == 6 && k.nvars == 4 && kernel_num_output_vars(k) == 3);
  // Chaining reuses an existing live helper, not its retired proof-hole peers.
  r = request(records); r.inputs = {6};
  assert(define_disjunction(k, r).output == 7);
  std::fclose(records);
}

static void continuing_search() {
  for (bool positive : {false, true}) {
    Kernel k; kernel_reserve_vars(k, 3);
    FILE *records = std::tmpfile(); assert(records);
    assert(define_disjunction(k, request(records)).result == ApplyResult::Ok);
    // The helper is a genuine branching literal. A negative decision fixes
    // every disjunct false; a positive decision plus two false disjuncts
    // propagates the remaining one true.
    kernel_assign_decision(k, kernel_internalize(k, positive ? 4 : -4));
    assert(propagate_service(k) == ApplyResult::Ok);
    if (positive) {
      kernel_assign_decision(k, kernel_internalize(k, -1));
      assert(propagate_service(k) == ApplyResult::Ok);
      kernel_assign_decision(k, kernel_internalize(k, 2));
      assert(propagate_service(k) == ApplyResult::Ok);
      assert(k.value(kernel_internalize(k, 3)) > 0);
    } else {
      for (int l : {-1, 2, -3}) assert(k.value(kernel_internalize(k, l)) > 0);
    }
    const auto before = k.next_clause_id;
    assert(define_disjunction(k, request(records)).result == ApplyResult::NoOp);
    assert(k.next_clause_id == before); // root-only publication
    std::fclose(records);
  }
}

static void proof(const char *path, ProofFormat format) {
  Kernel k; kernel_reserve_vars(k, 3);
  // Match the independently checked unit-free micro fixture: original 4/5
  // host the contradictory gadget, while definition inputs remain unfixed.
  kernel_reserve_vars(k, 5); k.next_clause_id = 4;
  Proof out; assert(out.open(path, format));
  out.bind_variable_map(&k.internal_to_external);
  out.bind_external_variable_map(&k.external_to_internal); k.proof = &out;
  FILE *records = std::tmpfile(); assert(records);
  const auto result = define_disjunction(k, request(records));
  assert(result.output == 6 && result.first_clause_id == 5 && k.next_clause_id == 8);
  const Lit unit = internalize(4);
  const uint64_t unit_hints[] = {1, 2};
  out.add_rup(9, &unit, 1, unit_hints, 2);
  const uint64_t empty_hints[] = {9, 3, 4};
  out.add_empty(10, empty_hints, 3);
  out.flush(); k.proof = nullptr; std::fclose(records);
}

int main(int argc, char **argv) {
  projection_and_propagation(); admission(); continuing_search();
  if (argc == 3) {
    proof(argv[1], ProofFormat::Lrat);
    proof(argv[2], ProofFormat::DsrAscii);
  }
  std::puts("test_disjunction_definition: passed");
}
