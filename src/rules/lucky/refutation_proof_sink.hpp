#pragma once

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"

#include <climits>
#include <cstdint>
#include <cstdlib>

namespace tactix {

// Common two-pass DRAT sink for proof-producing Lucky refutations. Clause
// containers may either expose vector-style data()/size() or the bounded
// parity-certificate lits/size fields.
template <class Clause> class RefutationProofSink {
  Kernel &kernel_;
  bool writing_;
  uint64_t maximum_additions_;
  uint64_t &interrupted_counter_;

  static const int32_t *data(const Clause &clause) {
    if constexpr (requires { clause.data(); })
      return clause.data();
    else
      return clause.lits.data();
  }

  static uint32_t size(const Clause &clause) {
    if constexpr (requires { clause.size(); })
      return static_cast<uint32_t>(clause.size());
    else
      return clause.size;
  }

public:
  uint64_t additions = 0;
  uint64_t deletions = 0;
  uint32_t fresh = 0;
  bool stopped = false;

  RefutationProofSink(Kernel &kernel, bool writing,
                      uint32_t original_variables,
                      uint64_t maximum_additions,
                      uint64_t &interrupted_counter)
      : kernel_(kernel), writing_(writing),
        maximum_additions_(maximum_additions),
        interrupted_counter_(interrupted_counter), fresh(original_variables) {}

  bool add(const Clause &clause, bool rat = false) {
    if (additions == maximum_additions_) {
      stopped = true;
      return false;
    }
    if (!writing_ && !(additions & 4095) &&
        kernel_rule_interrupt(kernel_, interrupted_counter_)) {
      stopped = true;
      return false;
    }
    ++additions;
    if (!writing_ || !kernel_.proof)
      return true;
    const uint64_t id = ++kernel_.next_clause_id;
    const bool ok = rat ? kernel_.proof->add_external_rat(
                              id, data(clause), size(clause))
                        : kernel_.proof->add_external_rup(
                              id, data(clause), size(clause));
    stopped |= !ok;
    return ok;
  }

  bool erase(const Clause &clause) {
    ++deletions;
    if (!writing_ || !kernel_.proof)
      return true;
    const bool ok = kernel_.proof->del_external(
        kernel_.next_clause_id, 0, data(clause), size(clause));
    stopped |= !ok;
    return ok;
  }

  bool new_variable(int32_t &variable) {
    if (fresh == static_cast<uint32_t>(INT32_MAX)) {
      stopped = true;
      return false;
    }
    variable = static_cast<int32_t>(++fresh);
    return true;
  }
};

// With a proof attached, derive the certificate once count-only and once into
// the proof; `run(sink, variables)` performs one derivation. Returns false when
// the preflight declines, and true without deriving when there is no proof.
template <class Clause, class Run>
bool refute_with_preflight(Kernel &k, uint64_t maximum_additions,
                           uint64_t &interrupted_counter,
                           uint64_t &additions_counter,
                           uint64_t &deletions_counter, Run run) {
  if (!k.proof)
    return true;
  const auto variables = static_cast<uint32_t>(k.external_to_internal.size());
  RefutationProofSink<Clause> preflight(k, false, variables, maximum_additions,
                                        interrupted_counter);
  if (!run(preflight, variables) || preflight.stopped)
    return false;
  // Emission is terminal and atomic with respect to solver scheduling.  Stop
  // requests are honored throughout recognition and preflight; once bytes are
  // written, finish the already-bounded proof rather than expose a prefix.
  RefutationProofSink<Clause> writer(k, true, variables, maximum_additions,
                                     interrupted_counter);
  if (!run(writer, variables) || writer.stopped ||
      writer.additions != preflight.additions ||
      writer.deletions != preflight.deletions ||
      writer.fresh != preflight.fresh)
    std::abort();
  additions_counter += writer.additions;
  deletions_counter += writer.deletions;
  return true;
}

} // namespace tactix
