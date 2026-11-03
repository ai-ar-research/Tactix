#pragma once
// A checker certificate for a theory action: the checker CNF (the formula plus
// an unsatisfiable gadget) and a proof whose derivation is the action's own
// output followed by the gadget's unit and the empty clause, both RUP.
#ifdef NDEBUG
#error "checker_certificate.hpp asserts have side effects; #undef NDEBUG first"
#endif

#include "test_io.hpp"
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>

// publish(records) runs the action on kernel with a bound proof on
// proof_path; records is a fresh temporary stream closed afterwards.
template <class Publish>
void write_checker_certificate(const char *cnf_path, const std::string &checker_cnf,
                               const char *proof_path, tactix::ProofFormat format,
                               tactix::Kernel &kernel, int32_t unit,
                               Publish publish) {
  write_file(cnf_path, checker_cnf);
  tactix::Proof proof;
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *records = std::tmpfile();
  assert(records);
  publish(records);
  assert(proof.add_external_rup(++kernel.next_clause_id, &unit, 1));
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}
